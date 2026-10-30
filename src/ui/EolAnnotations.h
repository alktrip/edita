// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// El texto en gris al final de las líneas (anotaciones de fin de línea de Scintilla), que
// comparten varios: las pistas del servidor de lenguaje y Git blame en la línea del cursor.
// Cada uno pone lo suyo y aquí se junta, en un estilo propio (gris y cursiva).

#pragma once

#include <QMap>
#include <QObject>
#include <QPointer>

class ScintillaEditBase;

namespace edita {

class EolAnnotations : public QObject
{
    Q_OBJECT

public:
    enum Owner { Hints = 0, Blame = 1 };

    // El de esa vista (se crea la primera vez)
    static EolAnnotations* of(ScintillaEditBase* view);

    void set(Owner owner, int line, const QString& text);
    void clear(Owner owner);
    // Otro documento en la vista: se olvida todo (y se quita lo que quedara en él)
    void reset();
    QString textAt(int line) const;

private:
    explicit EolAnnotations(ScintillaEditBase* view);
    void apply(int line);
    void configureStyle();

    QPointer<ScintillaEditBase> _view;
    QMap<int, QMap<int, QString>> _lines; // línea → dueño → texto
};

} // namespace edita
