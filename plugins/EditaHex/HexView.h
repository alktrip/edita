// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Editor hexadecimal (fase 89): la vista. Tres columnas (dirección, bytes en hexadecimal y
// los mismos como ASCII), con el cursor en la de bytes o en la de ASCII, selección,
// sobrescribir o insertar, deshacer, copiar y pegar. Dibuja solo las filas visibles: sirve
// para archivos grandes.

#pragma once

#include "HexBuffer.h"

#include <QAbstractScrollArea>

namespace edita::hex {

class HexView : public QAbstractScrollArea
{
    Q_OBJECT

public:
    enum class Area { Hex, Ascii };

    explicit HexView(QWidget* parent = nullptr);

    void setBuffer(HexBuffer* buffer); // no es suyo
    HexBuffer* buffer() const { return _buffer; }
    void setBytesPerRow(int bytes);
    int bytesPerRow() const { return _bytesPerRow; }
    void setOverwrite(bool overwrite);
    bool overwrite() const { return _overwrite; }

    qsizetype position() const { return _position; }
    Area area() const { return _area; }
    bool lowNibble() const { return _lowNibble; }
    // [inicio, fin) de la selección; vacía si no hay
    std::pair<qsizetype, qsizetype> selection() const;
    QByteArray selectedBytes() const;

    void setPosition(qsizetype position, bool extendSelection = false);
    void select(qsizetype from, qsizetype length);
    void setArea(Area area);

    void undo();
    void redo();
    void copy();
    void paste();
    void deleteSelection();
    // Después de cambiar el búfer desde fuera
    void refresh();

signals:
    void cursorChanged();
    void contentChanged();
    void overwriteChanged(bool overwrite);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool focusNextPrevChild(bool next) override;

private:
    void updateMetrics();
    void updateScrollBars();
    void ensureVisible();
    qsizetype rows() const;
    int offsetDigits() const;
    int hexX(int column) const;   // x del byte `column` de la fila en la columna de bytes
    int asciiX(int column) const;
    // La posición bajo el punto, y en qué columna está
    qsizetype positionAt(const QPoint& point, Area* area, bool* low) const;
    void typeHex(int value);
    void typeAscii(char byte);
    void changed(qsizetype newPosition);

    HexBuffer* _buffer = nullptr;
    int _bytesPerRow = 16;
    bool _overwrite = true;
    qsizetype _position = 0;
    qsizetype _anchor = 0;
    bool _lowNibble = false;
    Area _area = Area::Hex;
    int _charWidth = 8;
    int _lineHeight = 16;
    int _ascent = 12;
};

} // namespace edita::hex
