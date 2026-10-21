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

#include <QString>
#include <QStringList>

#include <memory>

namespace pugi {
class xml_document;
}

namespace edita {

// <FindHistory> de config.xml (NppParameters::feedFindHistoryParameters)
struct FindHistory
{
    QStringList paths;
    QStringList filters;
    QStringList finds;
    QStringList replaces;
    bool matchWord = false;
    bool matchCase = false;
    bool wrap = true;
    bool directionDown = true;
    bool recursive = true;
    bool hidden = false;
    bool dotMatchesNewline = false;
    bool folderFollowsDoc = false;
    int searchMode = 0; // 0 normal, 1 extendido, 2 regex (FindNormal…FindRegex)
};

// <History nbMaxFile="10" inSubMenu="no" customLength="-1"><File filename="…"/></History>
// (NppParameters::feedFileListParameters): archivos recientes, el más reciente primero
struct RecentFilesConfig
{
    int max = 10;          // 0-30 (NB_MAX_LRF_FILE)
    bool inSubMenu = false;
    int customLength = -1; // -1 = ruta completa
    QStringList files;
};

// config.xml con el esquema de Notepad++:
//   <NotepadPlus><GUIConfigs><GUIConfig name="stylerTheme" path="…"/>…</GUIConfigs></NotepadPlus>
// Por ahora solo se usa para lo que necesitan las fases implementadas; las
// demás entradas que haya en el archivo se conservan al guardar.
class ConfigXml
{
public:
    ConfigXml();
    ~ConfigXml();
    ConfigXml(const ConfigXml&) = delete;
    ConfigXml& operator=(const ConfigXml&) = delete;

    // Lee el archivo; si no existe o está corrupto empieza vacío (devuelve false si corrupto)
    bool load(const QString& path);
    bool save() const;
    const QString& path() const { return _path; }

    // Atributo `attribute` del <GUIConfig name="name">; `fallback` si no existe
    QString guiConfig(const QString& name, const QString& attribute, const QString& fallback = {}) const;
    void setGuiConfig(const QString& name, const QString& attribute, const QString& value);
    // Texto del <GUIConfig name="name">texto</GUIConfig> (p. ej. "Auto-detection")
    QString guiConfigText(const QString& name, const QString& fallback = {}) const;
    void setGuiConfigText(const QString& name, const QString& value);

    RecentFilesConfig recentFiles() const;
    void setRecentFiles(const RecentFilesConfig& recent);

    FindHistory findHistory() const;
    void setFindHistory(const FindHistory& history);

private:
    QString _path;
    std::unique_ptr<pugi::xml_document> _doc;
};

} // namespace edita
