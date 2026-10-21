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

#include "core/PluginCatalog.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include <algorithm>

namespace edita {

PluginVersion PluginVersion::parse(const QString& text)
{
    PluginVersion v;
    const QStringList parts = text.trimmed().split(QLatin1Char('.'));
    if (text.trimmed().isEmpty() || parts.size() > 4)
        return v;
    unsigned long* fields[] = {&v.major, &v.minor, &v.patch, &v.build};
    for (qsizetype i = 0; i < parts.size(); ++i) {
        bool ok = false;
        const QString& part = parts[i];
        const unsigned long n = part.toULong(&ok);
        if (!ok || part.isEmpty() || !std::all_of(part.begin(), part.end(), [](QChar c) { return c.isDigit(); }))
            return PluginVersion{}; // como N++: formato no válido → 0.0.0.0
        *fields[i] = n;
    }
    return v;
}

QString PluginVersion::toString() const
{
    if (empty())
        return {};
    if (build == 0 && patch == 0 && minor == 0)
        return QString::number(major);
    if (build == 0 && patch == 0)
        return QStringLiteral("%1.%2").arg(major).arg(minor);
    if (build == 0)
        return QStringLiteral("%1.%2.%3").arg(major).arg(minor).arg(patch);
    return QStringLiteral("%1.%2.%3.%4").arg(major).arg(minor).arg(patch).arg(build);
}

int PluginVersion::compare(const PluginVersion& o) const
{
    const unsigned long a[] = {major, minor, patch, build};
    const unsigned long b[] = {o.major, o.minor, o.patch, o.build};
    for (int i = 0; i < 4; ++i) {
        if (a[i] != b[i])
            return a[i] > b[i] ? 1 : -1;
    }
    return 0;
}

bool PluginVersion::isCompatibleTo(const PluginVersion& from, const PluginVersion& to) const
{
    if (empty())
        return false;
    if (from.empty() && to.empty())
        return true;
    if (compare(from) >= 0 && to.compare(*this) >= 0)
        return true;
    return compare(from) >= 0 && to.empty();
}

std::pair<PluginVersion, PluginVersion> parseVersionInterval(const QString& text)
{
    std::pair<PluginVersion, PluginVersion> result;
    if (text.isEmpty())
        return result;
    if (text.startsWith(QLatin1Char('[')) && text.endsWith(QLatin1Char(']'))) {
        const QStringList bounds = text.mid(1, text.size() - 2).split(QLatin1Char(','));
        if (bounds.size() == 2) {
            result.first = PluginVersion::parse(bounds[0]);
            result.second = PluginVersion::parse(bounds[1]);
        }
    } else if (!text.startsWith(QLatin1Char('[')) && !text.endsWith(QLatin1Char(']'))) {
        result.first = result.second = PluginVersion::parse(text); // versión exacta
    }
    return result;
}

QString PluginInfo::describe() const
{
    // Como N++: descripción, autor y página
    QString desc;
    if (!description.isEmpty())
        desc += description + QLatin1Char('\n');
    if (!author.isEmpty())
        desc += QCoreApplication::translate("PluginCatalog", "Author: ") + author + QLatin1Char('\n');
    if (!homepage.isEmpty())
        desc += QCoreApplication::translate("PluginCatalog", "Homepage: ") + homepage + QLatin1Char('\n');
    if (!recipePath.isEmpty())
        desc += QCoreApplication::translate("PluginCatalog", "Built on your computer from its source code (recipe: %1).\n").arg(recipePath);
    if (!problem.isEmpty())
        desc += QCoreApplication::translate("PluginCatalog", "Incompatible: ") + problem + QLatin1Char('\n');
    return desc;
}

bool PluginCatalog::parse(const QByteArray& json, PluginCatalog& catalog, QString* error)
{
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (!doc.isObject()) {
        if (error)
            *error = parseError.errorString();
        return false;
    }
    const QJsonObject root = doc.object();
    if (!root.value(QStringLiteral("version")).isString() || !root.value(QStringLiteral("npp-plugins")).isArray()) {
        if (error)
            *error = QStringLiteral("faltan \"version\" o \"npp-plugins\"");
        return false;
    }
    catalog.version = root.value(QStringLiteral("version")).toString();
    catalog.plugins.clear();
    for (const QJsonValue& value : root.value(QStringLiteral("npp-plugins")).toArray()) {
        const QJsonObject o = value.toObject();
        // Campos obligatorios: si falta alguno, ese plugin no se lista (como N++)
        static const char* const kRequired[] = {"folder-name", "display-name", "author", "description",
                                                "id", "version", "repository", "homepage"};
        if (!std::all_of(std::begin(kRequired), std::end(kRequired),
                         [&o](const char* key) { return o.value(QLatin1String(key)).isString(); }))
            continue;
        PluginInfo info;
        info.folderName = o.value(QStringLiteral("folder-name")).toString();
        info.displayName = o.value(QStringLiteral("display-name")).toString();
        info.author = o.value(QStringLiteral("author")).toString();
        info.description = o.value(QStringLiteral("description")).toString();
        info.id = o.value(QStringLiteral("id")).toString();
        info.version = PluginVersion::parse(o.value(QStringLiteral("version")).toString());
        info.repository = o.value(QStringLiteral("repository")).toString();
        info.homepage = o.value(QStringLiteral("homepage")).toString();
        info.compatibleVersions = parseVersionInterval(o.value(QStringLiteral("npp-compatible-versions")).toString());
        if (info.folderName.isEmpty())
            continue;
        catalog.plugins.push_back(std::move(info));
    }
    return true;
}

const PluginInfo* PluginCatalog::find(const QString& folderName) const
{
    for (const PluginInfo& info : plugins) {
        if (info.folderName.compare(folderName, Qt::CaseInsensitive) == 0)
            return &info;
    }
    return nullptr;
}

PluginLists buildPluginLists(const PluginCatalog& catalog, const std::vector<InstalledPlugin>& loaded,
                             const std::vector<InstalledPlugin>& disabled, const std::vector<PluginInfo>& incompatible,
                             const PluginVersion& appVersion)
{
    PluginLists lists;
    QStringList hidden; // del catálogo, pero ya están en disco

    // loadFromPluginInfos: los cargados, con los datos del catálogo si están en él
    auto fromDisk = [&](const InstalledPlugin& plugin, std::vector<PluginInfo>& target, const QString& fallback) {
        PluginInfo info;
        if (const PluginInfo* known = catalog.find(plugin.folderName)) {
            info = *known;
            hidden << plugin.folderName.toLower();
            if (plugin.version < known->version)
                lists.updates.push_back(*known);
        } else {
            info.folderName = plugin.folderName;
            info.displayName = plugin.folderName;
            info.description = fallback;
        }
        info.version = plugin.version;
        info.fullFilePath = plugin.path;
        info.isSystem = plugin.isSystem;
        target.push_back(info);
    };
    for (const InstalledPlugin& plugin : loaded)
        fromDisk(plugin, lists.installed, QString());
    for (const InstalledPlugin& plugin : disabled)
        fromDisk(plugin, lists.disabled, QCoreApplication::translate("PluginCatalog", "Information not available (it is not in the plugin list)."));

    // Incompatibles: si el catálogo tiene una versión más nueva, pasa a actualizaciones
    for (const PluginInfo& info : incompatible) {
        lists.incompatible.push_back(info);
        if (const PluginInfo* known = catalog.find(info.folderName); known && known->version > info.version
            && !info.fullFilePath.isEmpty()) {
            hidden << info.folderName.toLower();
            lists.updates.push_back(*known);
        }
    }

    // initAvailablePluginsViewFromList: solo los compatibles con esta versión de Edita
    for (const PluginInfo& info : catalog.plugins) {
        if (hidden.contains(info.folderName.toLower()))
            continue;
        if (appVersion.isCompatibleTo(info.compatibleVersions.first, info.compatibleVersions.second))
            lists.available.push_back(info);
    }

    auto byName = [](const PluginInfo& a, const PluginInfo& b) {
        return a.displayName.compare(b.displayName, Qt::CaseInsensitive) < 0;
    };
    for (auto* list : {&lists.available, &lists.updates, &lists.installed, &lists.incompatible, &lists.disabled})
        std::stable_sort(list->begin(), list->end(), byName);
    return lists;
}

} // namespace edita
