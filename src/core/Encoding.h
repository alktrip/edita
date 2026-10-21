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

#include <span>
#include <string>
#include <string_view>

namespace edita {

// Codepage de Windows para UTF-8 (SC_CP_UTF8 en Scintilla)
inline constexpr int kCodepageUtf8 = 65001;

// Mismos valores que UniMode en Notepad++ (MISC/Common/NppConstants.h), que se
// guardan en session.xml y en config.xml.
enum class UniMode : int
{
    Ansi = 0,          // uni8Bit: bytes de 8 bits, se interpretan con un codepage
    Utf8Bom = 1,       // uniUTF8
    Utf16BeBom = 2,    // uni16BE
    Utf16LeBom = 3,    // uni16LE
    Utf8 = 4,          // uniUTF8_NoBOM
    Ascii7 = 5,        // uni7Bit: solo 0-127
    Utf16BeNoBom = 6,  // uni16BE_NoBOM
    Utf16LeNoBom = 7,  // uni16LE_NoBOM
};

// Cómo está codificado un archivo en disco. Es el par Buffer::_encoding /
// Buffer::_unicodeMode de Notepad++:
//   codepage == -1 → familia Unicode, decide unicodeMode;
//   codepage != -1 → codepage de Windows (1252, 932…); unicodeMode no se usa.
// Dentro del editor el texto siempre está en UTF-8.
struct TextEncoding
{
    int codepage = -1;
    UniMode unicodeMode = UniMode::Utf8;

    static constexpr TextEncoding utf8() { return {}; }
    static constexpr TextEncoding unicode(UniMode mode) { return {-1, mode}; }
    static constexpr TextEncoding fromCodepage(int cp) { return {cp, UniMode::Utf8}; }

    bool isUnicode() const { return codepage == -1; }
    bool hasBom() const;
    bool isUtf16() const;

    friend bool operator==(const TextEncoding& a, const TextEncoding& b)
    {
        // unicodeMode no importa cuando hay codepage
        return a.codepage == b.codepage && (a.codepage != -1 || a.unicodeMode == b.unicodeMode);
    }
};

// Codepages que admite Notepad++, en el orden de su menú Codificación
// (EncodingMapper.cpp, tabla encodings[], sin las entradas vacías).
std::span<const int> knownCodepages();

// Port de EncodingMapper::getEncodingFromString: nombre de charset (como lo
// devuelve uchardet, o como aparece en una cabecera XML/HTML) → codepage.
// Usa la misma lista de alias que N++, sin distinguir mayúsculas.
// Devuelve kCodepageUtf8 para "utf-8"/"utf8" y -1 si no lo conoce.
int codepageFromCharsetName(std::string_view name);

// Nombre de iconv para un codepage; nullptr si glibc no lo implementa (CP720)
// o si el codepage no está en la tabla.
const char* iconvNameForCodepage(int codepage);

// Nombre corto para la barra de estado ("UTF-8", "UTF-8-BOM",
// "UTF-16 LE BOM", "Windows-1252"…), como los que muestra Notepad++.
std::string encodingDisplayName(const TextEncoding& encoding);

} // namespace edita
