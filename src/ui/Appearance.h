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

#include "core/Preferences.h"

#include <QColor>
#include <QPalette>
#include <QString>

namespace edita::appearance {

// ¿Modo oscuro? Como el DarkMode de N++: con "seguir al sistema", lo que diga el
// escritorio (`systemScheme`); si no, lo elegido (enable)
bool isDark(const Preferences& prefs, Qt::ColorScheme systemScheme);
// El esquema del escritorio (Qt::ColorScheme::Unknown si no lo dice)
Qt::ColorScheme systemScheme();

// Paletas propias, claras u oscuras, con el verde del logo como color de acento
QPalette palette(bool dark);
QColor accent(bool dark);
// El color del punto de "sin guardar" en las pestañas
QColor unsavedColor(bool dark);
// La hoja de estilos de la interfaz (menús, barra de herramientas, pestañas,
// barra de estado, botones, barras de desplazamiento) para esa paleta
QString styleSheet(bool dark);

// Aplica estilo Fusion, paleta y hoja de estilos a toda la aplicación
void apply(bool dark);
// El modo aplicado por última vez
bool isDarkApplied();

// Tamaño en píxeles de los iconos de la barra de herramientas
int toolbarIconSize(ToolbarIcons icons);

} // namespace edita::appearance
