// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fragmentos de código (no existen en Notepad++) con la sintaxis de VS Code y de LSP:
// marcadores ($1, ${1:texto}, ${1|uno,dos|}), el final ($0), variables ($TM_FILENAME,
// ${CURRENT_YEAR}, ${VAR:por defecto}) y escapes (\$, \}, \\). Un marcador repetido es un
// espejo: se edita a la vez en todos sus sitios. Los archivos son los de VS Code: un JSON
// con «prefix», «body» (texto o lista de líneas) y «description», con comentarios.

#pragma once

#include <QString>

#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace edita::snippet {

struct Tabstop
{
    int index = 0;                         // 1, 2…; 0 es el final
    std::vector<std::pair<int, int>> ranges; // inicio y fin en el texto (unidades de QString)
};

struct Parsed
{
    QString text;
    std::vector<Tabstop> tabstops; // 1, 2… y el 0 al final (siempre está: si falta, al final del texto)
};

// El valor de una variable (TM_FILENAME…); nullopt si no se conoce
using Variables = std::function<std::optional<QString>(const QString& name)>;

Parsed parse(const QString& body, const Variables& variables = {});

struct Definition
{
    QString name;
    QString prefix;
    QString body;
    QString description;
};

// Un archivo de fragmentos de VS Code (JSON con comentarios // y /* */). Vacío si no se
// entiende (y el motivo en `error`)
std::vector<Definition> loadFile(const QString& path, QString* error = nullptr);
QByteArray stripJsonComments(const QByteArray& json);

} // namespace edita::snippet
