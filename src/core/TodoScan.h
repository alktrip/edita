// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// El panel TODO (fase 96), sin interfaz: los comentarios con TODO, FIXME, HACK, XXX, BUG o
// NOTE de un texto o de un archivo. La palabra va en mayúsculas y entera (no TODOS ni
// TODO_LIST) y dentro de lo que parece un comentario (//, #, /*, --, ;, <!--…): así no cuenta
// en el código ni en el texto normal.

#pragma once

#include <QString>
#include <QStringList>

#include <vector>

namespace edita::todo {

struct Item
{
    QString path;
    int line = 0;   // desde 0
    int column = 0; // donde empieza la palabra, en caracteres
    QString kind;   // TODO, FIXME…
    QString text;   // lo que sigue, sin «:» ni el cierre del comentario

    bool operator==(const Item&) const = default;
};

QStringList keywords(); // TODO FIXME HACK XXX BUG NOTE

std::vector<Item> scanText(const QString& text, const QString& path = QString());

// Un archivo del disco: los binarios (un byte 0 al principio) y los de más de maxBytes, nada
inline constexpr qint64 kMaxFileBytes = 2 * 1024 * 1024;
std::vector<Item> scanFile(const QString& path, qint64 maxBytes = kMaxFileBytes);

} // namespace edita::todo
