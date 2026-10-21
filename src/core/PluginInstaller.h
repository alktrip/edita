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

// Lo que hace gup.exe (el actualizador de N++) por el administrador de plugins:
// -unzipTo, -clean y -moveFolder. En Linux un .so cargado se puede sustituir o
// borrar, así que no hace falta salir de Edita: los cambios valen al reiniciar.
namespace plugin_installer {

// SHA-256 en hexadecimal (el "id" de un paquete en nppPluginList.json)
QString sha256Hex(const QByteArray& data);

// ¿Se pueden instalar paquetes .zip? (Edita compilado con libzip)
bool zipSupported();

// Descomprime en `dir` (que debe existir). Rechaza rutas absolutas o con "..".
bool extractZip(const QByteArray& zip, const QString& dir, QString* error = nullptr);

// -unzipTo [-clean]: comprueba el SHA-256 (si `expectedId` no está vacío), deja el
// contenido en <pluginsRoot>/<folderName>/ y comprueba que esté <folderName>.so
// (también si el .zip lo trae dentro de una carpeta con ese nombre). Con `clean`
// (actualizar) se borra antes la carpeta; sin él, una carpeta que ya existe es un error.
bool installPackage(const QByteArray& zip, const QString& folderName, const QString& expectedId,
                    const QString& pluginsRoot, bool clean, QString* error = nullptr);

// Crea un .zip con esos archivos (nombre dentro del .zip, archivo en disco)
bool createZip(const QString& zipPath, const std::vector<std::pair<QString, QString>>& files, QString* error = nullptr);

// -clean: borra <pluginsRoot>/<folderName>
bool removePlugin(const QString& pluginsRoot, const QString& folderName, QString* error = nullptr);

// -moveFolder: de <fromRoot>/<folderName> a <toRoot>/<folderName> (desactivar: a
// plugins/disabled; activar: de vuelta)
bool movePlugin(const QString& fromRoot, const QString& toRoot, const QString& folderName, QString* error = nullptr);

} // namespace plugin_installer
} // namespace edita
