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

#include <string_view>

namespace edita {

struct DetectionOptions
{
    // NppGUI::_detectEncoding (activo por defecto en N++)
    bool autoDetect = true;
    // Codepage para archivos de 8 bits que no son UTF-8 ni se pudieron detectar.
    // En Windows es el codepage ANSI del sistema (NppParameters::defaultCodepage);
    // en Linux no existe, así que se configura (Windows-1252 por defecto).
    int fallbackCodepage = 1252;
};

// Utf8_16_Read::determineEncodingFromBOM. UniMode::Ansi si no hay BOM.
UniMode unicodeModeFromBom(std::string_view data);

// Tamaño en bytes del BOM de ese modo (0, 2 o 3)
std::size_t bomLength(UniMode mode);

// Utf8_16_Read::determineEncoding: BOM, UTF-16 LE sin BOM, UTF-8 válido,
// ASCII de 7 bits o, si nada encaja, 8 bits.
// `isWholeFile` = false indica que `data` es solo el principio del archivo: un
// carácter UTF-8 cortado al final no cuenta como inválido (N++ sí lo cuenta y
// puede tomar por ANSI un UTF-8 cuyo primer bloque acabe a mitad de carácter).
UniMode detectUnicodeMode(std::string_view data, bool isWholeFile = true);

// FileManager::detectCodepage: uchardet (la copia de N++) + tabla de alias.
// -1 si no detecta nada útil. TIS-620 se descarta, como en N++, porque uchardet
// confunde a menudo UTF-8 con TIS-620.
int detectCodepageWithUchardet(std::string_view data);

// Decide la codificación de un archivo a partir de su primer bloque, siguiendo
// FileManager::copyFileContentToScintilla + resolveLoadedEncoding en el modo
// "UTF-8 como codepage del sistema" de N++, que es el equivalente en Linux:
//   1. Con BOM: Unicode según el BOM (se ignora `forcedCodepage`).
//   2. Si no: `forcedCodepage` o, si es -1 y está activa, la detección de uchardet.
//      UTF-8 detectado → UTF-8 sin BOM.
//   3. Si no hay codepage: UTF-16 LE sin BOM, UTF-8 o ASCII → UTF-8 sin BOM;
//      8 bits → `fallbackCodepage`.
TextEncoding detectEncoding(std::string_view firstBlock, bool isWholeFile, int forcedCodepage,
                            const DetectionOptions& options);

} // namespace edita
