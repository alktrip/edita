// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Bloques de cambios de Git (fase 85; no existe en N++): descartar un bloque en el documento
// o pasarlo al índice («stage») sin tocar los demás

#pragma once

#include "core/LineDiff.h"

#include <string>
#include <string_view>
#include <vector>

namespace edita::githunks {

// Las líneas con su fin de línea (la última puede no tenerlo)
std::vector<std::string_view> splitKeepingEol(std::string_view text);

// `oldText` con el bloque `change` de `newText` (las líneas oldLine…oldLine+oldCount de
// oldText pasan a ser las line…line+count de newText). Si el bloque termina sin fin de línea
// y detrás siguen líneas, se le pone el de oldText
std::string applyHunk(std::string_view oldText, std::string_view newText, const LineChange& change);

// El mismo bloque visto desde el otro lado (para deshacerlo: applyHunk(nuevo, viejo, inverted))
LineChange inverted(const LineChange& change);

// El bloque que toca la línea `line` (desde 0) del texto nuevo; nullptr si ninguno. Un borrado
// se toca desde la línea de después o la de antes
const LineChange* hunkAt(const std::vector<LineChange>& changes, int line);

// Para no reescribir todo el documento: lo que cambia entre a y b, como el rango [start, endA)
// de a que hay que sustituir por [start, endB) de b
struct Span
{
    std::size_t start = 0;
    std::size_t endA = 0;
    std::size_t endB = 0;
};
Span changedSpan(std::string_view a, std::string_view b);

} // namespace edita::githunks
