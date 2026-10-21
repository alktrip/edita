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

#include "ui/Translations.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

#include <algorithm>
#include <memory>

namespace edita::i18n {

namespace {

const QString kResourceDir = QStringLiteral(":/edita/i18n");

// Cada traducción dice cómo se llama su idioma: la de "English" ("Español")
const char* const kLanguageNameContext = "i18n";
const char* const kLanguageName = QT_TRANSLATE_NOOP("i18n", "English");

// Las traducciones instaladas, para poder cambiarlas
std::vector<std::unique_ptr<QTranslator>>& installed()
{
    static std::vector<std::unique_ptr<QTranslator>> translators;
    return translators;
}

QString& current()
{
    static QString code = QStringLiteral("en");
    return code;
}

bool& wasInstalled()
{
    static bool installedOnce = false;
    return installedOnce;
}

bool hasTranslation(const QString& code)
{
    return code == QLatin1String("en") || QFileInfo::exists(kResourceDir + QStringLiteral("/edita_%1.qm").arg(code));
}

// "es-MX" / "es_MX" → "es"
QString baseCode(const QString& name)
{
    return name.section(QLatin1Char('-'), 0, 0).section(QLatin1Char('_'), 0, 0).toLower();
}

} // namespace

std::vector<Language> availableLanguages()
{
    std::vector<Language> languages{{QStringLiteral("en"), QString::fromLatin1(kLanguageName)}};
    for (const QString& file : QDir(kResourceDir).entryList({QStringLiteral("edita_*.qm")}, QDir::Files)) {
        const QString code = file.mid(6, file.size() - 9); // edita_<código>.qm
        if (code == QLatin1String("en"))
            continue; // solo los plurales del inglés
        QTranslator translator;
        QString name;
        if (translator.load(file, kResourceDir))
            name = translator.translate(kLanguageNameContext, kLanguageName);
        if (name.isEmpty())
            name = QLocale(code).nativeLanguageName();
        languages.push_back({code, name.isEmpty() ? code : name});
    }
    std::sort(languages.begin(), languages.end(),
              [](const Language& a, const Language& b) { return a.nativeName.localeAwareCompare(b.nativeName) < 0; });
    return languages;
}

QString resolveLanguage(const QString& preference, const QStringList& uiLanguages)
{
    if (!preference.isEmpty() && hasTranslation(baseCode(preference)))
        return baseCode(preference);
    for (const QString& name : uiLanguages) {
        if (hasTranslation(baseCode(name)))
            return baseCode(name);
    }
    return QStringLiteral("en");
}

void install(const QString& code)
{
    for (const auto& translator : installed())
        QCoreApplication::removeTranslator(translator.get());
    installed().clear();

    auto load = [](const QString& file, const QString& dir) {
        auto translator = std::make_unique<QTranslator>();
        if (translator->load(file, dir) && QCoreApplication::installTranslator(translator.get()))
            installed().push_back(std::move(translator));
    };
    // Las de Edita (edita_en.qm: los plurales del inglés) y las de Qt para ese idioma
    load(QStringLiteral("edita_%1").arg(code), kResourceDir);
    if (code != QLatin1String("en"))
        load(QStringLiteral("qtbase_%1").arg(code), QLibraryInfo::path(QLibraryInfo::TranslationsPath));
    current() = code;
    wasInstalled() = true;
    QLocale::setDefault(QLocale(code == QLatin1String("en") ? QLocale::English : QLocale(code).language(),
                                QLocale::system().territory()));
}

QString currentLanguage()
{
    return current();
}

void ensureInstalled()
{
    if (!wasInstalled())
        install(QStringLiteral("en"));
}

void installFromPreference(const QString& preference)
{
    install(resolveLanguage(preference, QLocale::system().uiLanguages()));
}

} // namespace edita::i18n
