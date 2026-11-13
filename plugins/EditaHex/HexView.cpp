// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "HexView.h"

#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>
#include <QScrollBar>

namespace edita::hex {

namespace {

constexpr int kMargin = 6;
const char* const kMime = "application/octet-stream";

bool isPrintable(unsigned char c)
{
    return c >= 0x20 && c < 0x7F;
}

} // namespace

HexView::HexView(QWidget* parent)
    : QAbstractScrollArea(parent)
{
    setObjectName(QStringLiteral("hexView"));
    setFocusPolicy(Qt::StrongFocus);
    viewport()->setCursor(Qt::IBeamCursor);
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    updateMetrics();
}

void HexView::setBuffer(HexBuffer* buffer)
{
    _buffer = buffer;
    _position = _anchor = 0;
    _lowNibble = false;
    refresh();
    emit cursorChanged();
}

void HexView::setBytesPerRow(int bytes)
{
    _bytesPerRow = std::clamp(bytes, 4, 64);
    refresh();
    ensureVisible();
}

void HexView::setOverwrite(bool overwrite)
{
    if (_overwrite == overwrite)
        return;
    _overwrite = overwrite;
    viewport()->update();
    emit overwriteChanged(overwrite);
}

void HexView::refresh()
{
    const qsizetype size = _buffer ? _buffer->size() : 0;
    _position = std::clamp<qsizetype>(_position, 0, size);
    _anchor = std::clamp<qsizetype>(_anchor, 0, size);
    updateMetrics();
    updateScrollBars();
    viewport()->update();
}

void HexView::updateMetrics()
{
    const QFontMetrics fm(font());
    _charWidth = std::max(1, fm.horizontalAdvance(QLatin1Char('0')));
    _lineHeight = fm.height() + 2;
    _ascent = fm.ascent() + 1;
}

qsizetype HexView::rows() const
{
    const qsizetype size = _buffer ? _buffer->size() : 0;
    return size / _bytesPerRow + 1; // la última fila tiene sitio para escribir al final
}

int HexView::offsetDigits() const
{
    int digits = 8;
    for (qsizetype size = _buffer ? _buffer->size() : 0; (size >> (4 * digits)) != 0; ++digits) {
    }
    return digits;
}

int HexView::hexX(int column) const
{
    // «HH » por byte, con un espacio más cada 8
    return kMargin + (offsetDigits() + 2) * _charWidth + column * 3 * _charWidth + (column / 8) * _charWidth;
}

int HexView::asciiX(int column) const
{
    return hexX(_bytesPerRow) + _charWidth + column * _charWidth;
}

void HexView::updateScrollBars()
{
    const int visible = std::max(1, viewport()->height() / _lineHeight);
    const qsizetype total = rows();
    verticalScrollBar()->setRange(0, static_cast<int>(std::max<qsizetype>(0, std::min<qsizetype>(total - visible, INT_MAX))));
    verticalScrollBar()->setPageStep(visible);
    verticalScrollBar()->setSingleStep(1);
    const int width = asciiX(_bytesPerRow) + kMargin;
    horizontalScrollBar()->setRange(0, std::max(0, width - viewport()->width()));
    horizontalScrollBar()->setPageStep(viewport()->width());
}

void HexView::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollBars();
}

void HexView::ensureVisible()
{
    const qsizetype row = _position / _bytesPerRow;
    const int first = verticalScrollBar()->value();
    const int visible = std::max(1, viewport()->height() / _lineHeight);
    if (row < first)
        verticalScrollBar()->setValue(static_cast<int>(row));
    else if (row >= first + visible)
        verticalScrollBar()->setValue(static_cast<int>(row - visible + 1));
}

std::pair<qsizetype, qsizetype> HexView::selection() const
{
    return {std::min(_anchor, _position), std::max(_anchor, _position)};
}

QByteArray HexView::selectedBytes() const
{
    const auto [from, to] = selection();
    return _buffer ? _buffer->data().mid(from, to - from) : QByteArray();
}

void HexView::setPosition(qsizetype position, bool extendSelection)
{
    const qsizetype size = _buffer ? _buffer->size() : 0;
    _position = std::clamp<qsizetype>(position, 0, size);
    if (!extendSelection)
        _anchor = _position;
    _lowNibble = false;
    if (_buffer)
        _buffer->breakMerge();
    ensureVisible();
    viewport()->update();
    emit cursorChanged();
}

void HexView::select(qsizetype from, qsizetype length)
{
    setPosition(from);
    setPosition(from + length, true);
}

void HexView::setArea(Area area)
{
    _area = area;
    _lowNibble = false;
    viewport()->update();
    emit cursorChanged();
}

void HexView::changed(qsizetype newPosition)
{
    _position = _anchor = std::clamp<qsizetype>(newPosition, 0, _buffer->size());
    updateScrollBars();
    ensureVisible();
    viewport()->update();
    emit contentChanged();
    emit cursorChanged();
}

void HexView::undo()
{
    if (_buffer && _buffer->canUndo())
        changed(_buffer->undo());
}

void HexView::redo()
{
    if (_buffer && _buffer->canRedo())
        changed(_buffer->redo());
}

void HexView::deleteSelection()
{
    const auto [from, to] = selection();
    if (!_buffer || from == to)
        return;
    _buffer->breakMerge();
    _buffer->remove(from, to - from);
    changed(from);
}

void HexView::copy()
{
    const QByteArray bytes = selectedBytes();
    if (bytes.isEmpty())
        return;
    auto* mime = new QMimeData;
    // En la columna de bytes, el texto es «4F 6B»; en la de ASCII, los propios caracteres
    mime->setText(_area == Area::Hex ? toHex(bytes) : QString::fromLatin1(bytes));
    mime->setData(QString::fromLatin1(kMime), bytes);
    QApplication::clipboard()->setMimeData(mime);
}

void HexView::paste()
{
    if (!_buffer)
        return;
    const QMimeData* mime = QApplication::clipboard()->mimeData();
    if (!mime)
        return;
    QByteArray bytes;
    if (mime->hasFormat(QString::fromLatin1(kMime)))
        bytes = mime->data(QString::fromLatin1(kMime));
    else if (const auto hex = _area == Area::Hex ? parseHex(mime->text()) : std::nullopt)
        bytes = *hex;
    else
        bytes = mime->text().toUtf8();
    if (bytes.isEmpty())
        return;
    const auto [from, to] = selection();
    _buffer->breakMerge();
    if (from != to)
        _buffer->remove(from, to - from);
    if (_overwrite && from == to)
        _buffer->replace(from, bytes);
    else
        _buffer->insert(from, bytes);
    _buffer->breakMerge();
    changed(from + bytes.size());
}

void HexView::typeHex(int value)
{
    const auto [from, to] = selection();
    if (from != to) {
        deleteSelection();
    }
    if (!_overwrite && !_lowNibble) {
        _buffer->insert(_position, QByteArray(1, static_cast<char>(value << 4)));
        _lowNibble = true;
    } else {
        _buffer->setNibble(_position, !_lowNibble, value);
        if (_lowNibble) {
            _lowNibble = false;
            ++_position;
        } else {
            _lowNibble = true;
        }
    }
    _anchor = _position;
    const bool low = _lowNibble;
    updateScrollBars();
    ensureVisible();
    viewport()->update();
    _lowNibble = low;
    emit contentChanged();
    emit cursorChanged();
}

void HexView::typeAscii(char byte)
{
    const auto [from, to] = selection();
    if (from != to)
        deleteSelection();
    if (_overwrite)
        _buffer->replace(_position, QByteArray(1, byte));
    else
        _buffer->insert(_position, QByteArray(1, byte));
    changed(_position + 1);
}

void HexView::keyPressEvent(QKeyEvent* event)
{
    if (!_buffer) {
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }
    const bool shift = event->modifiers() & Qt::ShiftModifier;
    const qsizetype rowBytes = _bytesPerRow;
    const int page = std::max(1, viewport()->height() / _lineHeight) * _bytesPerRow;
    if (event->matches(QKeySequence::Undo)) {
        undo();
    } else if (event->matches(QKeySequence::Redo)
               || (event->key() == Qt::Key_Y && event->modifiers() == Qt::ControlModifier)) {
        redo();
    } else if (event->matches(QKeySequence::Copy)) {
        copy();
    } else if (event->matches(QKeySequence::Paste)) {
        paste();
    } else if (event->matches(QKeySequence::Cut)) {
        copy();
        deleteSelection();
    } else if (event->matches(QKeySequence::SelectAll)) {
        select(0, _buffer->size());
    } else if (event->key() == Qt::Key_Insert && event->modifiers() == Qt::NoModifier) {
        setOverwrite(!_overwrite);
    } else if (event->key() == Qt::Key_Left) {
        if (_area == Area::Hex && _lowNibble && !shift)
            _lowNibble = false, viewport()->update(), emit cursorChanged();
        else
            setPosition(_position - 1, shift);
    } else if (event->key() == Qt::Key_Right) {
        setPosition(_position + 1, shift);
    } else if (event->key() == Qt::Key_Up) {
        setPosition(_position - rowBytes, shift);
    } else if (event->key() == Qt::Key_Down) {
        setPosition(_position + rowBytes, shift);
    } else if (event->key() == Qt::Key_PageUp) {
        setPosition(_position - page, shift);
    } else if (event->key() == Qt::Key_PageDown) {
        setPosition(_position + page, shift);
    } else if (event->key() == Qt::Key_Home) {
        setPosition(event->modifiers() & Qt::ControlModifier ? 0 : _position - _position % rowBytes, shift);
    } else if (event->key() == Qt::Key_End) {
        setPosition(event->modifiers() & Qt::ControlModifier ? _buffer->size()
                                                             : std::min(_buffer->size(), _position - _position % rowBytes + rowBytes - 1),
                    shift);
    } else if (event->key() == Qt::Key_Backspace) {
        const auto [from, to] = selection();
        if (from != to) {
            deleteSelection();
        } else if (_position > 0) {
            _buffer->remove(_position - 1, 1);
            changed(_position - 1);
        }
    } else if (event->key() == Qt::Key_Delete) {
        const auto [from, to] = selection();
        if (from != to) {
            deleteSelection();
        } else if (_position < _buffer->size()) {
            _buffer->remove(_position, 1);
            changed(_position);
        }
    } else if (!event->text().isEmpty() && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
        const QChar c = event->text().front();
        if (_area == Area::Hex) {
            const int digit = QString(c).toInt(nullptr, 16);
            const bool isHex = c.isDigit() || (c.toLower() >= QLatin1Char('a') && c.toLower() <= QLatin1Char('f'));
            if (isHex)
                typeHex(digit);
        } else if (c.unicode() >= 0x20 && c.unicode() < 0x7F) {
            typeAscii(static_cast<char>(c.unicode()));
        }
    } else {
        QAbstractScrollArea::keyPressEvent(event);
    }
}

bool HexView::focusNextPrevChild(bool)
{
    // Tab cambia entre la columna de bytes y la de ASCII
    setArea(_area == Area::Hex ? Area::Ascii : Area::Hex);
    return true;
}

qsizetype HexView::positionAt(const QPoint& point, Area* area, bool* low) const
{
    const int x = point.x() + horizontalScrollBar()->value();
    const qsizetype row = verticalScrollBar()->value() + point.y() / _lineHeight;
    int column = 0;
    if (x >= asciiX(0) - _charWidth / 2) {
        *area = Area::Ascii;
        column = std::clamp((x - asciiX(0)) / _charWidth, 0, _bytesPerRow - 1);
        *low = false;
    } else {
        *area = Area::Hex;
        column = 0;
        while (column < _bytesPerRow - 1 && x >= hexX(column + 1) - _charWidth / 2)
            ++column;
        *low = x >= hexX(column) + _charWidth;
    }
    const qsizetype size = _buffer ? _buffer->size() : 0;
    return std::clamp<qsizetype>(row * _bytesPerRow + column, 0, size);
}

void HexView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !_buffer)
        return;
    Area area = Area::Hex;
    bool low = false;
    const qsizetype at = positionAt(event->position().toPoint(), &area, &low);
    _area = area;
    setPosition(at, event->modifiers() & Qt::ShiftModifier);
    _lowNibble = area == Area::Hex && low;
}

void HexView::mouseMoveEvent(QMouseEvent* event)
{
    if (!(event->buttons() & Qt::LeftButton) || !_buffer)
        return;
    Area area = Area::Hex;
    bool low = false;
    setPosition(positionAt(event->position().toPoint(), &area, &low), true);
}

void HexView::paintEvent(QPaintEvent*)
{
    QPainter p(viewport());
    p.setFont(font());
    const QPalette pal = palette();
    p.fillRect(viewport()->rect(), pal.base());
    if (!_buffer)
        return;
    p.translate(-horizontalScrollBar()->value(), 0);
    const QByteArray& bytes = _buffer->data();
    const qsizetype first = verticalScrollBar()->value();
    const int visible = viewport()->height() / _lineHeight + 1;
    const auto [selFrom, selTo] = selection();
    const QColor offsetColor = pal.color(QPalette::PlaceholderText);
    const QColor selectionColor = pal.color(QPalette::Highlight);
    const QColor selectedText = pal.color(QPalette::HighlightedText);
    const QColor text = pal.color(QPalette::Text);
    QColor otherColumn = selectionColor;
    otherColumn.setAlpha(70);
    const int digits = offsetDigits();
    for (int r = 0; r < visible; ++r) {
        const qsizetype row = first + r;
        if (row >= rows())
            break;
        const int y = r * _lineHeight;
        const qsizetype rowStart = row * _bytesPerRow;
        p.setPen(offsetColor);
        p.drawText(kMargin, y + _ascent, QStringLiteral("%1").arg(rowStart, digits, 16, QLatin1Char('0')).toUpper());
        for (int c = 0; c < _bytesPerRow; ++c) {
            const qsizetype at = rowStart + c;
            if (at >= bytes.size())
                break;
            const auto byte = static_cast<unsigned char>(bytes[at]);
            const bool selected = at >= selFrom && at < selTo;
            const QRect hexRect(hexX(c) - _charWidth / 4, y, 2 * _charWidth + _charWidth / 2, _lineHeight);
            const QRect asciiRect(asciiX(c), y, _charWidth, _lineHeight);
            if (selected) {
                p.fillRect(hexRect, _area == Area::Hex ? selectionColor : otherColumn);
                p.fillRect(asciiRect, _area == Area::Ascii ? selectionColor : otherColumn);
            }
            p.setPen(selected && _area == Area::Hex ? selectedText : (byte == 0 ? offsetColor : text));
            p.drawText(hexX(c), y + _ascent, QStringLiteral("%1").arg(byte, 2, 16, QLatin1Char('0')).toUpper());
            p.setPen(selected && _area == Area::Ascii ? selectedText : (isPrintable(byte) ? text : offsetColor));
            p.drawText(asciiX(c), y + _ascent, isPrintable(byte) ? QString(QLatin1Char(static_cast<char>(byte))) : QStringLiteral("·"));
        }
        // El cursor: una barra (insertar) o un recuadro (sobrescribir) en la columna activa, y
        // un recuadro tenue en la otra
        if (_position >= rowStart && _position < rowStart + _bytesPerRow) {
            const int c = static_cast<int>(_position - rowStart);
            const int hx = hexX(c) + (_lowNibble ? _charWidth : 0);
            const QRect hexCursor(hx, y, _charWidth, _lineHeight - 1);
            const QRect asciiCursor(asciiX(c), y, _charWidth, _lineHeight - 1);
            const QRect active = _area == Area::Hex ? hexCursor : asciiCursor;
            const QRect other = _area == Area::Hex ? asciiCursor : hexCursor;
            p.setPen(QPen(text, 1));
            p.setBrush(Qt::NoBrush);
            if (hasFocus()) {
                if (_overwrite)
                    p.drawRect(active.adjusted(0, 0, -1, -1));
                else
                    p.fillRect(QRect(active.left(), active.top(), 2, active.height()), text);
            }
            p.setPen(QPen(offsetColor, 1, Qt::DotLine));
            p.drawRect(other.adjusted(0, 0, -1, -1));
        }
    }
}

} // namespace edita::hex
