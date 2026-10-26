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

namespace edita {

// Rutas de la aplicación según XDG. Equivale a NppParameters::_userPath
// (%APPDATA%\Notepad++) y _nppPath (carpeta del ejecutable con los *.model.xml).
namespace paths {

// Directorio de configuración del usuario: $XDG_CONFIG_HOME/<app> (~/.config/edita; en macOS,
// sin XDG_CONFIG_HOME, ~/Library/Preferences/edita).
// Ahí irán langs.xml, stylers.xml, config.xml y session.xml.
QString configDir();

// Crea configDir() si no existe. Devuelve false si no se pudo.
bool ensureConfigDir();

// Los QSettings sin ruta propia (el diálogo de archivos de Qt guarda en «QtProject»
// sus accesos directos, el historial y la última carpeta) van a configDir() y no a
// ~/.config/QtProject.conf, que comparten todas las aplicaciones de Qt. Las de Qt 5
// leen ese archivo como Latin-1 y lo guardan con escapes que Qt 6 lee de otra forma:
// cada vez que se alternan, las rutas con acentos («Música») se estropean un poco más.
void useOwnQtSettings();

// Directorios donde buscar los datos de solo lectura (langs.model.xml,
// stylers.model.xml, themes/), por orden de prioridad:
// $EDITA_DATA_DIR, junto al ejecutable (../share/<app>; en macOS ../Resources/<app>), $XDG_DATA_DIRS/<app>
// (/usr/share/edita) y, en builds sin instalar, editor/data del árbol de fuentes.
QStringList dataDirs();

// Primer archivo existente con esa ruta relativa dentro de dataDirs(); vacío si no hay.
QString findDataFile(const QString& relativePath);

} // namespace paths
} // namespace edita
