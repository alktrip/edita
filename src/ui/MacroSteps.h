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

#include "core/Macro.h"

#include <ScintillaEditBase.h>

namespace edita {

// recordedMacroStep(iMessage, wParam, lParam): los mensajes con texto guardan
// una copia del texto (SCI_REPLACESEL al escribir…)
MacroStep recordedStep(int message, uptr_t wParam, sptr_t lParam);

// recordedMacroStep::isMacroable: los mensajes de Scintilla que se pueden reproducir
bool isMacroable(const MacroStep& step);

// Envía el paso de Scintilla a la vista (recordedMacroStep::PlayBack); las
// órdenes de menú las ejecuta la ventana
void playScintillaStep(ScintillaEditBase& view, const MacroStep& step);

} // namespace edita
