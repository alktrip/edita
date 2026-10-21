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

#include "core/Locale.h"

#include <locale>
#include <optional>
#include <stdexcept>

namespace edita {

namespace {

// Una locale sirve si clasifica como letra un carácter que no es ASCII
bool classifiesUnicode(const std::locale& loc)
{
    return std::isalpha(L'ñ', loc); // ñ
}

std::optional<std::locale> tryLocale(const char* name)
{
    try {
        std::locale loc(name);
        if (classifiesUnicode(loc))
            return loc;
    } catch (const std::runtime_error&) {
        // Locale no instalada
    }
    return std::nullopt;
}

} // namespace

std::string initGlobalLocale()
{
    // Primero la del usuario (LANG/LC_*); si es C/POSIX o no existe, C.UTF-8,
    // que glibc incluye desde la 2.35 con clasificación Unicode completa.
    for (const char* candidate : {"", "C.UTF-8", "en_US.UTF-8"}) {
        if (auto loc = tryLocale(candidate)) {
            std::locale::global(std::locale(std::locale::classic(), *loc, std::locale::ctype));
            return loc->name();
        }
    }
    return {};
}

} // namespace edita
