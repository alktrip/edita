// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Colores por columna en CSV (fase 99; Notepad++ no tiene nada para CSV): cada columna de un
// color, ocho que se repiten. Como RainbowBrackets: un indicador INDIC_TEXTFORE con el color
// en su valor, solo en las líneas visibles (un CSV de millones de filas no se vuelve lento).

#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>

#include <cstdint>

namespace edita {

class EditorView;

class CsvColumns : public QObject
{
    Q_OBJECT

public:
    static constexpr int kIndicator = 6; // N++ no usa los de 0 a 7 (los de los plugins empiezan en 9)

    explicit CsvColumns(EditorView* view);

    // Activo para el documento de la vista (un .csv o .tsv, con los colores encendidos)
    void setActive(bool active, QChar delimiter);
    bool isActive() const { return _active; }
    QChar delimiter() const { return _delimiter; }

    void schedule();
    void updateNow();

    // Para las pruebas: el color (0xBBGGRR) del texto en esa posición, o -1
    intptr_t colorAt(intptr_t pos) const;

private:
    void clear();

    QPointer<EditorView> _view;
    bool _active = false;
    QChar _delimiter = QLatin1Char(',');
    QTimer _timer;
};

} // namespace edita
