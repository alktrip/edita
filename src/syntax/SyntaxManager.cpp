// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
//
// Edita is derived from Notepad++ (Copyright (C) Don HO and Notepad++ team).
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "syntax/SyntaxManager.h"

#include "core/Paths.h"
#include "syntax/LanguageDetection.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>

namespace edita {

namespace {

const QString kThemeConfig = QStringLiteral("stylerTheme");

QString samePath(const QString& path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

// <GUIConfig name="globalOverride" fg="no" bg="no" font="no" fontSize="no" bold="no" italic="no" underline="no" />
const QString kGlobalOverrideConfig = QStringLiteral("globalOverride");
constexpr std::pair<const char*, bool GlobalOverride::*> kOverrideFlags[] = {
    {"fg", &GlobalOverride::foreground}, {"bg", &GlobalOverride::background}, {"font", &GlobalOverride::font},
    {"fontSize", &GlobalOverride::fontSize}, {"bold", &GlobalOverride::bold}, {"italic", &GlobalOverride::italic},
    {"underline", &GlobalOverride::underline},
};

GlobalOverride readGlobalOverride(const ConfigXml& config)
{
    GlobalOverride go;
    for (const auto& [name, flag] : kOverrideFlags)
        go.*flag = config.guiConfig(kGlobalOverrideConfig, QString::fromLatin1(name)) == QLatin1String("yes");
    return go;
}

void writeGlobalOverride(ConfigXml& config, const GlobalOverride& go)
{
    for (const auto& [name, flag] : kOverrideFlags)
        config.setGuiConfig(kGlobalOverrideConfig, QString::fromLatin1(name), go.*flag ? QStringLiteral("yes") : QStringLiteral("no"));
}

} // namespace

SyntaxManager::SyntaxManager(const QString& configDir, QObject* parent)
    : QObject(parent)
    , _configDir(configDir)
{
}

QString SyntaxManager::langsPath() const { return QDir(_configDir).filePath(QStringLiteral("langs.xml")); }
QString SyntaxManager::stylersPath() const { return QDir(_configDir).filePath(QStringLiteral("stylers.xml")); }
QString SyntaxManager::configPath() const { return QDir(_configDir).filePath(QStringLiteral("config.xml")); }

QString SyntaxManager::activeStylePath() const
{
    return _themePath.isEmpty() ? stylersPath() : _themePath;
}

QString SyntaxManager::ensureUserFile(const QString& userName, const QString& modelName, QStringList& warnings) const
{
    const QString userPath = QDir(_configDir).filePath(userName);
    if (QFileInfo::exists(userPath))
        return userPath;
    const QString model = paths::findDataFile(modelName);
    if (model.isEmpty()) {
        warnings << tr("%1 was not found in the application data").arg(modelName);
        return {};
    }
    // Primer arranque: copia del modelo, como N++ (que lo copia a %APPDATA%)
    if (!QFile::copy(model, userPath)) {
        warnings << tr("Cannot create %1; using %2").arg(userPath, model);
        return model;
    }
    QFile::setPermissions(userPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup
                                        | QFileDevice::ReadOther);
    return userPath;
}

QStringList SyntaxManager::initialize()
{
    QDir().mkpath(_configDir);
    QStringList warnings;
    if (!_config.load(configPath()))
        warnings << tr("config.xml is damaged; using the default values");
    _themePath = _config.guiConfig(kThemeConfig, QStringLiteral("path"));
    if (!_themePath.isEmpty() && !QFileInfo::exists(_themePath)) {
        warnings << tr("The theme %1 no longer exists; using the default one").arg(_themePath);
        _themePath.clear();
    }
    warnings << loadAll();
    return warnings;
}

QStringList SyntaxManager::reload()
{
    QStringList warnings = loadAll();
    emit stylesChanged();
    return warnings;
}

QStringList SyntaxManager::loadAll()
{
    QStringList warnings;
    QString error;

    const QString langs = ensureUserFile(QStringLiteral("langs.xml"), QStringLiteral("langs.model.xml"), warnings);
    if (langs.isEmpty() || !_languages.load(langs, &error)) {
        if (!langs.isEmpty())
            warnings << tr("Cannot read %1 (%2); using langs.model.xml").arg(langs, error);
        const QString model = paths::findDataFile(QStringLiteral("langs.model.xml"));
        if (model.isEmpty() || !_languages.load(model, &error))
            warnings << tr("langs.model.xml cannot be read either: there will be no keywords");
    }

    const QString stylers = ensureUserFile(QStringLiteral("stylers.xml"), QStringLiteral("stylers.model.xml"), warnings);
    const QString styles = _themePath.isEmpty() ? stylers : _themePath;
    if (styles.isEmpty() || !_styles.load(styles, &error)) {
        if (!styles.isEmpty())
            warnings << tr("Cannot read %1 (%2); using stylers.model.xml").arg(styles, error);
        const QString model = paths::findDataFile(QStringLiteral("stylers.model.xml"));
        if (model.isEmpty() || !_styles.load(model, &error))
            warnings << tr("stylers.model.xml cannot be read either: there will be no colors");
    }
    _styles.globalOverride() = readGlobalOverride(_config);
    warnings << _userLanguages.load(_configDir);
    return warnings;
}

std::vector<SyntaxManager::Theme> SyntaxManager::themes() const
{
    std::vector<Theme> result;
    QStringList dirs;
    for (const QString& data : paths::dataDirs())
        dirs << QDir(data).filePath(QStringLiteral("themes"));
    dirs << QDir(_configDir).filePath(QStringLiteral("themes"));
    for (const QString& dir : dirs) {
        const QFileInfoList files = QDir(dir).entryInfoList({QStringLiteral("*.xml")}, QDir::Files, QDir::Name);
        for (const QFileInfo& file : files) {
            const QString name = file.completeBaseName();
            // Un tema del usuario con el mismo nombre sustituye al de la aplicación
            const auto it = std::find_if(result.begin(), result.end(), [&](const Theme& t) { return t.name == name; });
            if (it != result.end())
                it->path = file.absoluteFilePath();
            else
                result.push_back({name, file.absoluteFilePath()});
        }
    }
    std::sort(result.begin(), result.end(),
              [](const Theme& a, const Theme& b) { return a.name.compare(b.name, Qt::CaseInsensitive) < 0; });
    result.insert(result.begin(), Theme{tr("Default"), QString()});
    return result;
}

bool SyntaxManager::setTheme(const QString& path, QString* error)
{
    StyleSheet candidate;
    candidate.globalOverride() = _styles.globalOverride();
    const QString file = path.isEmpty() ? stylersPath() : path;
    if (!candidate.load(file, error))
        return false;
    _styles = std::move(candidate);
    _themePath = path;
    _config.setGuiConfig(kThemeConfig, QStringLiteral("path"), path);
    _config.save();
    emit stylesChanged();
    return true;
}

void SyntaxManager::previewStyles(const StyleSheet& styles)
{
    _styles = styles;
    emit stylesChanged();
}

bool SyntaxManager::saveStyles(const StyleSheet& styles, QString* error)
{
    QString target = activeStylePath();
    const QString configDir = QDir(_configDir).absolutePath() + QLatin1Char('/');
    if (!QFileInfo(target).absoluteFilePath().startsWith(configDir)) {
        // Un tema de la aplicación: se edita una copia en ~/.config/edita/themes
        const QString copy = QDir(_configDir).filePath(QStringLiteral("themes/") + QFileInfo(target).fileName());
        QDir().mkpath(QFileInfo(copy).absolutePath());
        if (!QFileInfo::exists(copy) && !QFile::copy(target, copy)) {
            if (error)
                *error = tr("Cannot create %1").arg(copy);
            return false;
        }
        QFile::setPermissions(copy, QFile::permissions(copy) | QFileDevice::WriteOwner);
        target = copy;
        _themePath = copy;
        _config.setGuiConfig(kThemeConfig, QStringLiteral("path"), copy);
    }
    if (!styles.save(target, error))
        return false;
    _styles = styles;
    writeGlobalOverride(_config, _styles.globalOverride());
    _config.save();
    emit stylesChanged();
    return true;
}

void SyntaxManager::apply(ScintillaEditBase& view, LangType lang, const QString& userLanguage) const
{
    const UserLanguage* udl = lang == LangType::User ? _userLanguages.find(userLanguage) : nullptr;
    SyntaxStyler(_languages, _styles, _options).apply(view, lang, udl);
}

QString SyntaxManager::detectUserLanguage(const QString& filePath) const
{
    const UserLanguage* udl = _userLanguages.forFile(filePath, _darkMode);
    return udl ? udl->name : QString();
}

void SyntaxManager::setOptions(const StylerOptions& options)
{
    if (options == _options)
        return;
    _options = options;
    emit stylesChanged();
}

LangType SyntaxManager::detect(const QString& filePath, std::string_view textStart) const
{
    return detectLanguage(filePath, textStart, _languages, _styles);
}

bool SyntaxManager::isActiveConfigFile(const QString& path) const
{
    const QString p = samePath(path);
    return p == samePath(langsPath()) || p == samePath(activeStylePath());
}

} // namespace edita
