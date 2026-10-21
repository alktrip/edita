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

namespace edita {

// Configura la faceta ctype de la std::locale global de C++ con una locale UTF-8.
//
// Por qué: la regex de Notepad++ (boostregex/) usa basic_regex<wchar_t> con los
// traits por defecto. En Windows son w32_regex_traits, que clasifican Unicode
// con la API del sistema. En Linux son cpp_regex_traits, que consultan la
// std::locale global, y esta es "C" salvo que se cambie. Sin esto, \w, \b,
// [[:alpha:]] y "palabra completa" no reconocen letras como ñ, é o я.
//
// Solo se cambia ctype: la parte numérica sigue siendo "C" para que el formato
// de números en XML y en streams no dependa del idioma del usuario.
//
// Hay que llamarla antes de compilar cualquier regex. Devuelve el nombre de la
// locale usada para ctype ("" si no se encontró ninguna UTF-8).
std::string initGlobalLocale();

} // namespace edita
