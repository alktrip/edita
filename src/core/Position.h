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

#include <cstdint>

namespace edita {

// Posición de una vista dentro de un documento: cursor, selección y scroll.
// Mismos campos que Position en N++ (Parameters.h), que se guardan en session.xml.
struct Position
{
    std::intptr_t firstVisibleLine = 0; // línea del documento (no de pantalla)
    std::intptr_t startPos = 0;         // ancla de la selección
    std::intptr_t endPos = 0;           // cursor
    std::intptr_t xOffset = 0;
    std::intptr_t selMode = 0;          // SC_SEL_STREAM, SC_SEL_RECTANGLE…
    std::intptr_t scrollWidth = 1;

    friend bool operator==(const Position&, const Position&) = default;
};

} // namespace edita
