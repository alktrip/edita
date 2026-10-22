// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Diferencias por líneas (algoritmo de Myers, «An O(ND) Difference Algorithm»), para
// las marcas de Git en el margen: qué líneas del texto actual se añadieron o cambiaron
// respecto a la versión de referencia y dónde se borraron líneas.

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace edita {

enum class LineChangeKind { Added, Modified, Deleted };

struct LineChange
{
    LineChangeKind kind = LineChangeKind::Added;
    int line = 0;     // primera línea del texto actual (desde 0); en Deleted, la línea tras el borrado
    int count = 0;    // líneas del texto actual (0 en Deleted)
    int oldLine = 0;  // primera línea de la referencia
    int oldCount = 0; // líneas de la referencia que se quitaron o cambiaron

    bool operator==(const LineChange&) const = default;
};

// Las líneas de un texto, sin el fin de línea (\n, \r\n o \r); un texto que termina en
// fin de línea no tiene una línea vacía de más
std::vector<std::string_view> splitLines(std::string_view text);

// Si hay más de maxEdits líneas distintas, lo que queda entre lo común del principio y
// del final se da como un solo cambio, y *complete queda en false
std::vector<LineChange> diffLines(const std::vector<std::string_view>& oldLines, const std::vector<std::string_view>& newLines,
                                  int maxEdits = 2000, bool* complete = nullptr);

} // namespace edita
