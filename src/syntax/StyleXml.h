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

// Uso interno del módulo de sintaxis: un <WordsStyle>/<WidgetStyle> de los XML de N++

#include "syntax/StyleSheet.h"

#include <pugixml.hpp>

namespace edita {

// StyleArray::addStyler: styleID, name, colores (RRGGBB → BGR), colorStyle,
// fuente, fontStyle, fontSize, keywordClass, palabras y nesting
Style parseStyle(const pugi::xml_node& node);

} // namespace edita
