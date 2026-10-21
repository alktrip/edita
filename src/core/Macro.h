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

#include <QString>

#include <cstdint>
#include <string>
#include <vector>

namespace edita {

// Un paso de una macro (recordedMacroStep de N++, shortcut.h). Los valores de
// `type` son los de N++, que se guardan en shortcuts.xml.
struct MacroStep
{
    enum Type : int
    {
        UseLParameter = 0, // mensaje de Scintilla con parámetro numérico
        UseSParameter = 1, // mensaje de Scintilla con texto (SCI_REPLACESEL…)
        MenuCommand = 2,   // orden de menú (IDM_* de N++ en wParam)
        SavedSearch = 3,   // paso del diálogo Buscar (mtSavedSnR)
    };

    Type type = MenuCommand;
    int message = 0;
    std::uintptr_t wParam = 0;
    std::intptr_t lParam = 0;
    std::string sParam;

    bool operator==(const MacroStep&) const = default;
};

using Macro = std::vector<MacroStep>;

// <Macro name="…" Ctrl="no" Alt="yes" Shift="yes" Key="83"> de shortcuts.xml.
// `key` es el código de tecla virtual de Windows (VK_*), como en N++.
struct SavedMacro
{
    QString name;
    bool ctrl = false;
    bool alt = false;
    bool shift = false;
    int key = 0; // 0 = sin atajo
    Macro steps;

    bool operator==(const SavedMacro&) const = default;
};

// NppParameters::feedMacros/getActions, con la misma normalización de los
// SCI_REPLACESEL de un salto de línea (pasan a SCI_NEWLINE). false si no se
// puede leer; un archivo que no existe da una lista vacía y true.
bool readMacros(const QString& path, std::vector<SavedMacro>& macros);
// Reescribe <Macros> conservando el resto de shortcuts.xml (o lo crea)
bool writeMacros(const QString& path, const std::vector<SavedMacro>& macros);

// La macro que trae N++ en shortcuts.xml: "Trim Trailing Space and Save" (Alt+Mayús+S)
SavedMacro defaultTrimAndSaveMacro();

// Tecla de Qt (Qt::Key) ↔ código virtual de Windows; 0 si no tiene equivalente
int virtualKeyFromQtKey(int qtKey);
int qtKeyFromVirtualKey(int vk);

} // namespace edita
