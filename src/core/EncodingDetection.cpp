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

#include "core/EncodingDetection.h"

#include <uchardet.h>

#include <cstdint>
#include <strings.h>

namespace edita {

namespace {

enum class Utf8Class { Utf8, Ascii7, Bits8 };

std::uint8_t byteAt(std::string_view data, std::size_t i)
{
    return static_cast<std::uint8_t>(data[i]);
}

// Port de Utf8_16_Read::utf8_7bits_8bits (Utf8_16.cpp)
Utf8Class classifyUtf8(std::string_view data, bool isWholeFile)
{
    bool ascii7Only = true;
    std::size_t i = 0;
    const std::size_t n = data.size();
    while (i < n) {
        const std::uint8_t c = byteAt(data, i);
        if (c == 0)
            return Utf8Class::Bits8; // N++: un NUL significa "no es UTF-8"
        if ((c & 0x80) == 0) {
            ++i;
            continue;
        }
        ascii7Only = false;
        std::size_t length = 0;
        if ((c & 0xE0) == 0xC0)
            length = 2;
        else if ((c & 0xF0) == 0xE0)
            length = 3;
        else if ((c & 0xF8) == 0xF0)
            length = 4;
        else
            return Utf8Class::Bits8; // 10xxxxxx o 11111xxx como primer byte

        if (n - i < length)
            return isWholeFile ? Utf8Class::Bits8 : Utf8Class::Utf8;
        for (std::size_t k = 1; k < length; ++k) {
            if ((byteAt(data, i + k) & 0xC0) != 0x80)
                return Utf8Class::Bits8;
        }
        i += length;
    }
    return ascii7Only ? Utf8Class::Ascii7 : Utf8Class::Utf8;
}

// Aproximación de IsTextUnicode(IS_TEXT_UNICODE_STATISTICS) de Win32 para el
// caso que comprueba N++: texto UTF-16 LE sin BOM que empieza por un carácter
// ASCII. La mayoría de los bytes altos deben ser 0 y no debe haber caracteres NUL.
bool looksLikeUtf16Le(std::string_view data)
{
    std::size_t pairs = 0;
    std::size_t zeroHigh = 0;
    for (std::size_t i = 0; i + 1 < data.size(); i += 2) {
        const std::uint8_t lo = byteAt(data, i);
        const std::uint8_t hi = byteAt(data, i + 1);
        if (lo == 0 && hi == 0)
            return false;
        ++pairs;
        if (hi == 0)
            ++zeroHigh;
    }
    return pairs > 0 && zeroHigh * 10 >= pairs * 6;
}

// RAII para el handle opaco de uchardet
class Uchardet
{
public:
    Uchardet() : _handle(uchardet_new()) {}
    ~Uchardet() { uchardet_delete(_handle); }
    Uchardet(const Uchardet&) = delete;
    Uchardet& operator=(const Uchardet&) = delete;
    uchardet_t get() const { return _handle; }

private:
    uchardet_t _handle;
};

} // namespace

UniMode unicodeModeFromBom(std::string_view data)
{
    const std::size_t n = data.size();
    if (n > 1 && byteAt(data, 0) == 0xFE && byteAt(data, 1) == 0xFF)
        return UniMode::Utf16BeBom;
    if (n > 1 && byteAt(data, 0) == 0xFF && byteAt(data, 1) == 0xFE)
        return UniMode::Utf16LeBom;
    if (n > 2 && byteAt(data, 0) == 0xEF && byteAt(data, 1) == 0xBB && byteAt(data, 2) == 0xBF)
        return UniMode::Utf8Bom;
    return UniMode::Ansi;
}

std::size_t bomLength(UniMode mode)
{
    switch (mode) {
    case UniMode::Utf8Bom: return 3;
    case UniMode::Utf16BeBom:
    case UniMode::Utf16LeBom: return 2;
    default: return 0;
    }
}

UniMode detectUnicodeMode(std::string_view data, bool isWholeFile)
{
    const UniMode bom = unicodeModeFromBom(data);
    if (bom != UniMode::Ansi)
        return bom;

    // N++ comprueba len % 2 == 0 sobre el bloque; con un bloque parcial la
    // paridad no dice nada, así que solo se exige cuando es el archivo entero.
    const std::size_t n = data.size();
    if (n > 1 && (!isWholeFile || n % 2 == 0) && byteAt(data, 0) != 0 && byteAt(data, 1) == 0
        && looksLikeUtf16Le(data))
        return UniMode::Utf16LeNoBom;

    switch (classifyUtf8(data, isWholeFile)) {
    case Utf8Class::Utf8: return UniMode::Utf8;
    case Utf8Class::Ascii7: return UniMode::Ascii7;
    case Utf8Class::Bits8: break;
    }
    return UniMode::Ansi;
}

int detectCodepageWithUchardet(std::string_view data)
{
    Uchardet ud;
    uchardet_handle_data(ud.get(), data.data(), data.size());
    uchardet_data_end(ud.get());
    const char* charset = uchardet_get_charset(ud.get());
    if (!charset || strcasecmp(charset, "TIS-620") == 0)
        return -1;
    return codepageFromCharsetName(charset);
}

TextEncoding detectEncoding(std::string_view firstBlock, bool isWholeFile, int forcedCodepage,
                            const DetectionOptions& options)
{
    const UniMode bom = unicodeModeFromBom(firstBlock);
    if (bom != UniMode::Ansi)
        return TextEncoding::unicode(bom);

    int codepage = forcedCodepage;
    if (codepage == -1 && options.autoDetect)
        codepage = detectCodepageWithUchardet(firstBlock);

    if (codepage != -1) {
        // resolveLoadedEncoding: UTF-8 detectado (p. ej. por una cabecera XML) → UTF-8 sin BOM
        return codepage == kCodepageUtf8 ? TextEncoding::utf8() : TextEncoding::fromCodepage(codepage);
    }

    switch (detectUnicodeMode(firstBlock, isWholeFile)) {
    case UniMode::Utf16LeNoBom: return TextEncoding::unicode(UniMode::Utf16LeNoBom);
    case UniMode::Ansi: return TextEncoding::fromCodepage(options.fallbackCodepage);
    default: break; // UTF-8 o ASCII de 7 bits: con UTF-8 como codepage del sistema, UTF-8 sin BOM
    }
    return TextEncoding::utf8();
}

} // namespace edita
