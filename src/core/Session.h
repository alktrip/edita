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

#include "core/Position.h"

#include <QDateTime>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <vector>

namespace edita {

// Un <File> de session.xml (sessionFileInfo en N++, Parameters.h)
struct SessionFile
{
    QString path;
    QString languageName;   // texto del menú Lenguaje de N++ ("C++", "None (Normal Text)")
    int encoding = -1;      // codepage forzado; -1 = detectar
    bool userReadOnly = false;
    Position position;
    std::vector<std::size_t> foldedLines; // <Fold line="…"/> (se leen y se conservan)
    std::vector<std::size_t> bookmarks;   // <Mark line="…"/> (se leen y se conservan)
    // Modo "snapshot": copia con los cambios sin guardar y fecha del archivo
    // original cuando se hizo (para avisar si cambió mientras Edita estaba cerrado).
    // En documentos sin título `path` es el nombre de la pestaña ("nuevo 1").
    QString backupFilePath;
    QDateTime originalLastModified;
};

// <Session> de session.xml (struct Session en N++)
struct Session
{
    int activeView = 0;        // 0 = principal, 1 = secundaria
    int activeMainIndex = 0;
    int activeSubIndex = 0;
    std::vector<SessionFile> mainView;
    std::vector<SessionFile> subView;
    // Carpetas del Explorador de carpetas (Folder as Workspace): <FileBrowser
    // latestSelectedItem="…"><root foldername="…"><expanded path="…"/></root></FileBrowser>
    struct FolderRoot
    {
        QString folder;
        QStringList expanded;
        bool operator==(const FolderRoot&) const = default;
    };
    bool hasFileBrowser = false; // N++ solo escribe <FileBrowser> si el panel se usó
    std::vector<FolderRoot> folderRoots;
    QString folderSelected;

    bool isEmpty() const { return mainView.empty() && subView.empty(); }
};

// Sufijo de la copia de seguridad que N++ hace antes de sobrescribir la sesión
inline constexpr const char* kSessionBackupSuffix = ".inCaseOfCorruption.bak";

// NppParameters::getSessionFromXmlTree. Acepta sesiones escritas por Notepad++:
// atributos que Edita no usa se ignoran y las barras invertidas de las rutas se
// convierten en "/". false si el archivo no existe o no es una sesión válida.
bool readSessionFile(const QString& path, Session& session, QString* error = nullptr);

// Como readSessionFile pero, si la sesión está dañada, prueba con la copia
// "<path>.inCaseOfCorruption.bak". `usedBackup` indica si hizo falta.
bool readSessionWithBackup(const QString& path, Session& session, bool* usedBackup = nullptr, QString* error = nullptr);

// NppParameters::writeSession: copia la sesión anterior a .inCaseOfCorruption.bak,
// escribe la nueva con el mismo esquema que N++ y comprueba que se puede volver
// a leer; si no, restaura la copia.
bool writeSessionFile(const QString& path, const Session& session, QString* error = nullptr);

} // namespace edita
