// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Búsqueda difusa para la paleta de órdenes y «Ir a archivo»: cada palabra del texto
// buscado tiene que aparecer, letra a letra y en orden, en el texto (sin importar
// mayúsculas ni acentos); las palabras pueden ir en cualquier orden («lin ord»
// encuentra «Ordenar líneas»). Puntúa más los inicios de palabra y las letras seguidas.

#pragma once

#include <QList>
#include <QString>

namespace edita {

struct FuzzyMatch
{
    int score = -1;         // -1: no coincide
    QList<int> positions;   // posiciones del texto que coinciden, ordenadas
    bool matched() const { return score >= 0; }
};

FuzzyMatch fuzzyMatch(const QString& pattern, const QString& text);

// La letra sin acento y en minúscula («Á» → «a», «ñ» → «n»), como compara fuzzyMatch
QChar foldForSearch(QChar c);

} // namespace edita
