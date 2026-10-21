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

#include "core/LangType.h"
#include "core/Preferences.h"

#include <ScintillaEditBase.h>

namespace edita {

class EditorView;

// Notepad_plus::maintainIndentation: sangría automática al escribir `ch`
// (Intro, "{" o "}"). En modo C-like solo para los lenguajes con llaves y Python,
// como N++; los demás usan el modo básico.
void maintainIndentation(EditorView& view, LangType lang, AutoIndent mode, int ch);

// ScintillaEditView::setLineIndent: cambia la sangría de una línea moviendo la
// selección con el texto
void setLineIndent(EditorView& view, sptr_t line, sptr_t indent);

} // namespace edita
