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

#pragma once

#include <QByteArray>
#include <QString>

#include <utility>
#include <vector>

namespace edita {

// La clase Version de N++ (MISC/Common/Common.h): hasta cuatro números
struct PluginVersion
{
    unsigned long major = 0;
    unsigned long minor = 0;
    unsigned long patch = 0;
    unsigned long build = 0;

    // Version(const wstring&): un formato no válido deja 0.0.0.0
    static PluginVersion parse(const QString& text);
    // "1", "1.2", "1.2.3" o "1.2.3.4" (vacío si es 0.0.0.0)
    QString toString() const;
    bool empty() const { return major == 0 && minor == 0 && patch == 0 && build == 0; }
    int compare(const PluginVersion& other) const;
    // Version::isCompatibleTo: dentro de [from, to]; los extremos vacíos no limitan
    bool isCompatibleTo(const PluginVersion& from, const PluginVersion& to) const;

    bool operator<(const PluginVersion& o) const { return compare(o) < 0; }
    bool operator>(const PluginVersion& o) const { return compare(o) > 0; }
    bool operator==(const PluginVersion& o) const { return compare(o) == 0; }
};

// getIntervalVersions: "6.9" (exacta), "[4.2,6.6.6]", "[8.3,]" o "[,8.2.1]"
std::pair<PluginVersion, PluginVersion> parseVersionInterval(const QString& text);

// PluginUpdateInfo de pluginsAdmin.h
struct PluginInfo
{
    QString folderName;  // "folder-name": carpeta y nombre del .so
    QString displayName; // "display-name"
    PluginVersion version;
    std::pair<PluginVersion, PluginVersion> compatibleVersions; // "npp-compatible-versions" (de Edita)
    QString id;          // SHA-256 del paquete
    QString repository;  // URL del .zip (http, https, file o ruta relativa al catálogo); vacío = viene con Edita
    QString description;
    QString author;
    QString homepage;

    QString fullFilePath; // el .so, si está instalado
    bool isSystem = false; // instalado con Edita (no en la carpeta del usuario)
    QString problem;       // por qué es incompatible
    QString recipePath;    // si se compila en el equipo desde su código (recipe.json)

    // PluginUpdateInfo::describe
    QString describe() const;
};

// nppPluginList.json: {"version": …, "npp-plugins": [ … ]}
struct PluginCatalog
{
    QString version;
    QString sourcePath; // de dónde se leyó (para resolver los repository relativos)
    std::vector<PluginInfo> plugins;

    // loadFromJson: los plugins a los que les falta un campo obligatorio se ignoran
    static bool parse(const QByteArray& json, PluginCatalog& catalog, QString* error = nullptr);
    const PluginInfo* find(const QString& folderName) const;
};

// Un plugin que está en disco
struct InstalledPlugin
{
    QString folderName;
    QString path;      // el .so
    PluginVersion version;
    bool isSystem = false;
};

// Las pestañas del administrador (LIST_TYPE)
struct PluginLists
{
    std::vector<PluginInfo> available;
    std::vector<PluginInfo> updates;
    std::vector<PluginInfo> installed;
    std::vector<PluginInfo> incompatible;
    std::vector<PluginInfo> disabled;
};

// PluginsAdminDlg::updateList: disponibles (compatibles y no instalados),
// actualizaciones (instalados con versión menor que la del catálogo), instalados,
// incompatibles y desactivados. Cada lista, por nombre.
PluginLists buildPluginLists(const PluginCatalog& catalog, const std::vector<InstalledPlugin>& loaded,
                             const std::vector<InstalledPlugin>& disabled, const std::vector<PluginInfo>& incompatible,
                             const PluginVersion& appVersion);

} // namespace edita
