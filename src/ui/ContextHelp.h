// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// F1 contextual (fase 95): en un diálogo o un panel, F1 abre la sección del manual que lo
// explica. Cada uno se asocia a la orden de menú que lo abre, y el manual sabe dónde cita cada
// orden ({{actionX}}), en cualquier idioma (Manual::placeOf).

#pragma once

#include <QString>

#include <utility>
#include <vector>

class QWidget;

namespace edita::context_help {

// La clase (sin el espacio de nombres) o el nombre del objeto de un diálogo o panel, y la orden
// del menú cuya sección del manual lo explica
const std::vector<std::pair<const char*, const char*>>& table();

// La orden para el widget con el foco: la del primero de sus antecesores que está en la tabla
// (vacía: ninguno, F1 abre el manual)
QString commandFor(const QWidget* widget);

} // namespace edita::context_help
