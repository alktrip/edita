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

namespace edita {

class EditorView;

struct SmartHighlightOptions
{
    bool enabled = true;
    bool matchCase = false;
    bool wholeWord = true;
    bool otherView = false; // también en la otra vista si se ve
};

// SmartHighlighter::highlightView: al seleccionar una palabra (o cualquier texto
// de una línea si no se exige palabra completa), marca sus apariciones en las
// líneas visibles con el indicador "Smart Highlighting" (29)
void smartHighlight(EditorView& view, EditorView* otherView, const SmartHighlightOptions& options);

} // namespace edita
