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

#include "core/Preferences.h"

#include <QDateTime>
#include <QString>

namespace edita {

// Carpeta de las copias de los documentos sin guardar ("snapshot"):
// <configuración>/backup, como %APPDATA%\Notepad++\backup en N++
QString snapshotBackupDir();

// Nombre de la copia de un documento sin guardar: "<nombre>@AAAA-MM-DD_HHMMSS"
// (FileManager::backupCurrentBuffer). Si ya existe se añade un sufijo.
QString snapshotBackupPath(const QString& dir, const QString& documentName, const QDateTime& when);

// Dónde copiar la versión anterior de `filePath` antes de guardar (Notepad_plus::doSave):
// "archivo.bak" junto al archivo o, en modo detallado, "nppBackup/archivo.AAAA-MM-DD_HHMMSS.bak".
// Con carpeta personalizada se usa esa en lugar de la del archivo. Vacío si no hay copia.
QString saveBackupPath(const QString& filePath, const Preferences& prefs, const QDateTime& when);

// Fecha en el FILETIME de Windows partido en dos enteros de 32 bits, como
// originalFileLastModifTimestamp/…High de session.xml
struct FileTimeParts
{
    int low = 0;
    int high = 0;
};
FileTimeParts toFileTime(const QDateTime& time);
QDateTime fromFileTime(FileTimeParts parts); // inválida si ambos son 0

} // namespace edita
