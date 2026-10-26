// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Paréntesis por colores (no existe en Notepad++): cada nivel de ( [ { con su color
// (tres que se repiten), y en rojo el que cierra sin pareja o con la que no le toca.
// Un indicador INDIC_TEXTFORE con el color en su valor (SC_INDICFLAG_VALUEFORE) pinta el
// texto de cada paréntesis. Solo en lo visible; la profundidad se cuenta desde el
// principio del documento, sin los paréntesis de comentarios y cadenas.

#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>

#include <bitset>
#include <cstdint>

namespace edita {

class EditorView;

class RainbowBrackets : public QObject
{
    Q_OBJECT

public:
    static constexpr int kIndicator = 5; // N++ no usa los de 0 a 7 (los de los plugins empiezan en 9)

    explicit RainbowBrackets(EditorView* view);

    void setEnabled(bool enabled);
    bool isEnabled() const { return _enabled; }
    // Los estilos del lexer cuyos paréntesis no cuentan (comentarios, cadenas…)
    void setIgnoredStyles(const std::bitset<256>& styles);

    void schedule();
    void updateNow();

    // Para las pruebas: el color (0xBBGGRR) del paréntesis en esa posición, o -1
    intptr_t colorAt(intptr_t pos) const;

private:
    void clear();

    QPointer<EditorView> _view;
    bool _enabled = true;
    std::bitset<256> _ignored;
    QTimer _timer;
};

} // namespace edita
