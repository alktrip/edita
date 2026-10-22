// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Marcas en la barra de desplazamiento vertical (no existen en Notepad++): dónde están
// en todo el documento los cambios de Git, los marcadores, lo marcado con «Marcar
// todo», el resaltado inteligente y el cursor. Una capa transparente encima de la
// barra, que no recibe el ratón: la barra funciona igual.

#pragma once

#include <QColor>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <vector>

namespace edita {

class EditorView;
class GitChanges;

class ScrollBarMarks : public QWidget
{
    Q_OBJECT

public:
    struct Mark
    {
        int line = 0;     // línea del documento
        int lines = 1;    // cuántas
        QColor color;
        int lane = 0;     // 0 izquierda (Git), 1 centro (búsquedas), 2 derecha (marcadores)
    };

    ScrollBarMarks(EditorView* view, GitChanges* git);

    void setEnabled(bool enabled);
    // Vuelve a leer las marcas (en breve: se agrupan los avisos)
    void scheduleUpdate();

    // Para las pruebas
    const std::vector<Mark>& marks() const { return _marks; }
    int caretLine() const { return _caretLine; }
    void updateNow();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QRect track() const;

    QPointer<EditorView> _view;
    QPointer<GitChanges> _git;
    bool _enabled = true;
    std::vector<Mark> _marks;
    int _caretLine = -1;
    QTimer _timer;
};

} // namespace edita
