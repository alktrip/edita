// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Iconos por tipo de archivo (no existen en Notepad++, que usa disquetes según el
// estado): una hoja con la etiqueta del tipo («C++», «PY», «MD»…) en su color, dibujada
// aquí (sin archivos de imagen) y con los colores del tema. En las pestañas llevan
// además el estado: un punto si hay cambios sin guardar, un candado si es de solo lectura.

#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

namespace edita::fileicons {

enum class State { Saved, Dirty, ReadOnly };

struct FileType
{
    QString label;  // «C++», «PY»… (vacío: hoja sin etiqueta)
    QColor color;
};

// El tipo por el nombre del archivo (extensión o nombres como Makefile, CMakeLists.txt)
FileType typeOf(const QString& fileName);

QIcon forFile(const QString& fileName, State state = State::Saved);

// Preferencia: iconos por tipo (true, de serie) o los de estado de siempre
void setEnabled(bool enabled);
bool enabled();

// Al cambiar entre claro y oscuro
void clearCache();

} // namespace edita::fileicons
