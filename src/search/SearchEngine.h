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

#include "search/SearchOptions.h"
#include "search/SearchTarget.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace edita {

// FindStatus de N++
enum class FindStatus
{
    Found,
    EndReached,  // encontrado tras dar la vuelta desde el final
    TopReached,  // encontrado tras dar la vuelta desde el principio (hacia arriba)
    NotFound,
    InvalidRegex,
};

struct FindResult
{
    FindStatus status = FindStatus::NotFound;
    SciPos start = -1;
    SciPos end = -1;
    std::string regexError;

    bool found() const
    {
        return status == FindStatus::Found || status == FindStatus::EndReached || status == FindStatus::TopReached;
    }
};

// FindNextType de N++: cambia cómo se tratan las coincidencias vacías de una regex
enum class FindNextType { FindNext, ReplaceNext, FindNextForReplace };

// processFindNext: busca desde la selección actual hacia abajo (o arriba) y da
// la vuelta si `wrapAround`. `firstIncremental` empieza en el inicio de la
// selección en lugar de en su final (lo usa "Reemplazar").
FindResult findNext(SearchTarget& target, const SearchOptions& options, SciPos selStart, SciPos selEnd,
                    FindNextType type = FindNextType::FindNext, bool firstIncremental = false);

struct ReplaceOutcome
{
    bool replaced = false;
    // Selección que debe quedar: la siguiente coincidencia o, si no la hay, el
    // final de lo reemplazado
    SciPos selStart = -1;
    SciPos selEnd = -1;
    FindResult next;
};

// processReplace: si la selección es exactamente una coincidencia la reemplaza y
// busca la siguiente; si no, solo busca (y selecciona) la siguiente.
ReplaceOutcome replaceNext(SearchTarget& target, const SearchOptions& options, SciPos selStart, SciPos selEnd);

// ProcessOperation de N++ que se usan en el MVP
enum class RangeOperation { FindAll, CountAll, ReplaceAll, MarkAll };

// processAll: rango del documento que recorre cada operación según las opciones
// (todo con "ciclo", desde/hasta el cursor sin él, o la selección)
std::pair<SciPos, SciPos> rangeForOperation(RangeOperation op, const SearchOptions& options, SciPos selStart,
                                            SciPos selEnd, SciPos docLength, bool isEntire);

struct RangeResult
{
    int count = 0;
    bool invalidRegex = false;
    std::string regexError;
};

// processRange: recorre [start, end) y, para cada coincidencia, llama a `onMatch`
// (antes de reemplazar) y/o reemplaza.
RangeResult processRange(SearchTarget& target, const SearchOptions& options, RangeOperation op, SciPos start,
                         SciPos end, const std::function<void(SciPos matchStart, SciPos matchEnd)>& onMatch = {});

// Una línea de resultados de "buscar todo" (lo que Finder::foundLine escribe)
struct LineHit
{
    SciPos line = 0;                                // 0 = primera línea
    std::string text;                               // contenido de la línea (truncado si es larguísima)
    std::vector<std::pair<int, int>> segments;      // coincidencias dentro de `text` (bytes)
    std::vector<std::pair<SciPos, SciPos>> ranges;  // las mismas, en posiciones del documento
};

// Longitud máxima de una línea en el panel de resultados (SC_SEARCHRESULT_LINEBUFFERMAXLENGTH)
inline constexpr std::size_t kResultLineMaxLength = 2048;

// "Buscar todo": como processRange(FindAll) pero devuelve las líneas, con varias
// coincidencias de la misma línea en una sola entrada (opción por defecto de N++
// _finderShowOnlyOneEntryPerFoundLine)
std::vector<LineHit> findAllLines(SearchTarget& target, const SearchOptions& options, SciPos start, SciPos end,
                                  RangeResult& result);

} // namespace edita
