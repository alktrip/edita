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

#include <QColor>
#include <QIcon>
#include <QString>

class QAction;
class QWidget;

namespace edita::icons {

// Icono de línea de resources/icons/ui/<name>.svg, pintado con el color del texto
// de la paleta (y su versión desactivada). Vacío si no existe.
QIcon icon(const QString& name);
// El mismo icono en un color concreto
QIcon icon(const QString& name, const QColor& color);

// Pone el icono a la acción y lo recuerda, para volver a pintarlo con otra paleta
void setIcon(QAction* action, const QString& name);
// El nombre del icono de una acción ("" si no tiene)
QString iconName(const QAction* action);

// Vuelve a pintar los iconos de las acciones de `root` (y sus hijos) con la
// paleta actual: tras cambiar entre claro y oscuro
void refresh(QWidget* root);

} // namespace edita::icons
