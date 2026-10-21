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

#include "search/SearchOptions.h"

#include <BoostRegexSearch.h>
#include <Scintilla.h>

namespace edita {

namespace {

// Searching::readBase: `size` dígitos exactos en la base dada
bool readBase(std::string_view digits, int base, int& value)
{
    value = 0;
    for (const char raw : digits) {
        int d = -1;
        if (raw >= '0' && raw <= '9')
            d = raw - '0';
        else if (raw >= 'a' && raw <= 'z')
            d = raw - 'a' + 10;
        else if (raw >= 'A' && raw <= 'Z')
            d = raw - 'A' + 10;
        if (d < 0 || d >= base)
            return false;
        value = value * base + d;
    }
    return true;
}

void appendUtf8(std::string& out, char32_t cp)
{
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

} // namespace

int searchFlags(const SearchOptions& options)
{
    int flags = (options.wholeWord ? SCFIND_WHOLEWORD : 0) | (options.matchCase ? SCFIND_MATCHCASE : 0);
    if (options.mode == SearchMode::Regex) {
        flags |= SCFIND_REGEXP | SCFIND_POSIX;
        if (options.dotMatchesNewline)
            flags |= SCFIND_REGEXP_DOTMATCHESNL;
    }
    return flags;
}

std::string convertExtended(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c != '\\' || i + 1 >= text.size()) {
            out.push_back(c);
            continue;
        }
        const char next = text[++i];
        switch (next) {
        case 'r': out.push_back('\r'); continue;
        case 'n': out.push_back('\n'); continue;
        case '0': out.push_back('\0'); continue;
        case 't': out.push_back('\t'); continue;
        case '\\': out.push_back('\\'); continue;
        default: break;
        }
        int size = 0;
        int base = 0;
        switch (next) {
        case 'b': size = 8; base = 2; break;   // \b11111111
        case 'o': size = 3; base = 8; break;   // \o377
        case 'd': size = 3; base = 10; break;  // \d255
        case 'x': size = 2; base = 16; break;  // \xFF
        case 'u': size = 4; base = 16; break;  // 췍
        default: break;
        }
        int value = 0;
        if (size > 0 && text.size() - (i + 1) >= static_cast<std::size_t>(size)
            && readBase(text.substr(i + 1, static_cast<std::size_t>(size)), base, value)) {
            appendUtf8(out, static_cast<char32_t>(value));
            i += static_cast<std::size_t>(size);
            continue;
        }
        // Secuencia desconocida (o incompleta): se deja como texto
        out.push_back('\\');
        out.push_back(next);
    }
    return out;
}

std::string effectivePattern(const SearchOptions& options)
{
    return options.mode == SearchMode::Extended ? convertExtended(options.pattern) : options.pattern;
}

std::string effectiveReplacement(const SearchOptions& options)
{
    return options.mode == SearchMode::Extended ? convertExtended(options.replacement) : options.replacement;
}

} // namespace edita
