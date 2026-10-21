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

#include "core/Encoding.h"
#include "core/EncodingDetection.h"
#include "core/Eol.h"

#include <QString>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace edita {

// Tamaño de bloque de lectura de Notepad++ (Buffer.cpp: blockSize). La detección
// de encoding se hace sobre el primer bloque, igual que en N++.
inline constexpr std::size_t kLoadBlockSize = 128 * 1024 + 4;

enum class FileError
{
    None,
    NotFound,
    IsDirectory,
    PermissionDenied,
    ReadFailed,
    WriteFailed,
    UnsupportedEncoding,
    Aborted, // el receptor de datos pidió parar
};

struct LoadOptions
{
    // Codepage forzado por el usuario ("Codificación > Juegos de caracteres").
    // -1 = detectar. Un BOM tiene prioridad, como en N++.
    int forcedCodepage = -1;
    DetectionOptions detection;
    // Fin de línea si el archivo no tiene ninguno (NewDocDefaultSettings::_format).
    // N++ usa el del sistema (CRLF en Windows); aquí, LF.
    EolType defaultEol = EolType::Unix;
    std::size_t blockSize = kLoadBlockSize;
};

struct LoadResult
{
    FileError error = FileError::None;
    QString errorString;
    TextEncoding encoding;
    EolType eol = EolType::Unix;
    bool eolDetected = false;
    std::int64_t fileSize = 0;
    std::size_t invalidSequences = 0;

    bool ok() const { return error == FileError::None; }
};

// Recibe el texto UTF-8 por bloques; devuelve false para cancelar
using Utf8Sink = std::function<bool(std::string_view)>;

// Lee un archivo, detecta su encoding y su fin de línea, y entrega el texto
// convertido a UTF-8 bloque a bloque (sin cargar el archivo entero dos veces).
LoadResult loadTextFile(const QString& path, const LoadOptions& options, const Utf8Sink& sink);

// Atajo: todo el texto en un string
LoadResult loadTextFile(const QString& path, const LoadOptions& options, std::string& utf8Out);

struct SaveResult
{
    FileError error = FileError::None;
    QString errorString;
    std::size_t unmappableChars = 0; // caracteres sustituidos por '?'

    bool ok() const { return error == FileError::None; }
};

// Convierte `utf8` a `encoding` y lo escribe en `path`.
//
// Diferencia con N++: N++ sobrescribe el archivo directamente. Aquí se escribe
// en un temporal y se renombra (QSaveFile), así un fallo (disco lleno, corte de
// luz) nunca deja el archivo a medias. Se conservan los permisos y, si `path`
// es un enlace simbólico, se escribe en su destino. Si el directorio no admite
// escritura pero el archivo sí, se escribe directamente.
SaveResult saveTextFile(const QString& path, std::string_view utf8, const TextEncoding& encoding);

} // namespace edita
