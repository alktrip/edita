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

#include <string>
#include <string_view>

namespace edita {

// SearchType de N++ (FindReplaceDlg.h)
enum class SearchMode { Normal, Extended, Regex };

// FindOption de N++: lo que el usuario eligió en el diálogo. Textos en UTF-8.
struct SearchOptions
{
    std::string pattern;
    std::string replacement;
    SearchMode mode = SearchMode::Normal;
    bool matchCase = false;
    bool wholeWord = false;
    bool wrapAround = true;
    bool searchUp = false;           // _whichDirection == DIR_UP
    bool dotMatchesNewline = false;  // solo en modo regex
    bool inSelection = false;        // reemplazar todo / contar dentro de la selección
};

// Searching::buildSearchFlags: flags SCFIND_* para Scintilla
int searchFlags(const SearchOptions& options);

// Searching::convertExtendedToString: \n \r \t \0 \\ y \bNNNNNNNN \oNNN \dNNN
// \xNN \uNNNN (binario, octal, decimal, hexadecimal, Unicode). N++ lo hace sobre
// wchar_t y después pasa a UTF-8; aquí se produce directamente el carácter en
// UTF-8 (\xE9 es «é», no el byte 0xE9). Una secuencia desconocida se deja tal cual.
std::string convertExtended(std::string_view text);

// Texto que realmente se busca o se pone: extendido convertido; normal y regex sin cambios
std::string effectivePattern(const SearchOptions& options);
std::string effectiveReplacement(const SearchOptions& options);

} // namespace edita
