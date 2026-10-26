// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Encabezado fijo («sticky scroll», no existe en Notepad++): al bajar dentro de una
// función, clase o bloque largo, su primera línea queda fija arriba del editor. Son
// los encabezados de los bloques plegables (SCI_GETFOLDPARENT) que contienen a la
// primera línea visible, dibujados con los estilos del editor (colores, fuentes) y su
// número de línea. Un clic lleva a esa línea; la rueda sigue desplazando el editor.

#pragma once

#include <QPointer>
#include <QWidget>

#include <vector>

namespace edita {

class EditorView;

class StickyScroll : public QWidget
{
    Q_OBJECT

public:
    static constexpr int kMaxLines = 5;

    explicit StickyScroll(EditorView* view);

    void setEnabled(bool enabled);
    bool isFeatureEnabled() const { return _enabled; }
    void refresh();

    // Para las pruebas: las líneas (del documento) que se ven fijas
    const std::vector<int>& lines() const { return _lines; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    std::vector<int> headersFor(int line) const;
    int lineHeight() const;

    QPointer<EditorView> _view;
    bool _enabled = true;
    std::vector<int> _lines;
    int _hover = -1;
};

} // namespace edita
