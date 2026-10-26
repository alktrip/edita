// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/StickyScroll.h"

#include "syntax/SyntaxStyler.h"
#include "ui/EditorView.h"

#include <QHash>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

namespace edita {

namespace {

QColor fromScintilla(sptr_t color)
{
    return QColor(static_cast<int>(color & 0xFF), static_cast<int>((color >> 8) & 0xFF), static_cast<int>((color >> 16) & 0xFF));
}

} // namespace

StickyScroll::StickyScroll(EditorView* view)
    : QWidget(view->viewport())
    , _view(view)
{
    setObjectName(QStringLiteral("stickyScroll"));
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    hide();
    view->viewport()->installEventFilter(this);
    connect(view, &ScintillaEditBase::updateUi, this, [this] { refresh(); });
}

void StickyScroll::setEnabled(bool enabled)
{
    _enabled = enabled;
    refresh();
}

int StickyScroll::lineHeight() const
{
    return std::max(1, static_cast<int>(_view->send(SCI_TEXTHEIGHT, 0)));
}

// Los encabezados de los bloques que contienen a `line` y empiezan antes, de fuera hacia
// dentro. Con la llave en su propia línea («{»), el bloque empieza ahí: se muestra la línea
// de antes, que es la que tiene el nombre (la firma de la función, la clase…)
std::vector<int> StickyScroll::headersFor(int line) const
{
    std::vector<int> headers;
    for (sptr_t parent = _view->send(SCI_GETFOLDPARENT, static_cast<uptr_t>(line)); parent >= 0;
         parent = _view->send(SCI_GETFOLDPARENT, static_cast<uptr_t>(parent))) {
        if (parent < line) {
            sptr_t shown = parent;
            const sptr_t start = _view->send(SCI_GETLINEINDENTPOSITION, static_cast<uptr_t>(parent));
            const sptr_t end = _view->send(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(parent));
            if (parent > 0 && end - start == 1 && _view->send(SCI_GETCHARAT, static_cast<uptr_t>(start)) == '{')
                shown = parent - 1;
            headers.push_back(static_cast<int>(shown));
        }
        if (headers.size() > 50)
            break;
    }
    std::reverse(headers.begin(), headers.end());
    return headers;
}

void StickyScroll::refresh()
{
    if (!_view)
        return;
    std::vector<int> lines;
    if (_enabled && !_view->isWrap()) {
        const sptr_t firstVisible = _view->send(SCI_GETFIRSTVISIBLELINE);
        const int top = static_cast<int>(_view->send(SCI_DOCLINEFROMVISIBLE, static_cast<uptr_t>(firstVisible)));
        // Lo fijo tapa las primeras líneas: los bloques de la primera que queda a la vista
        int covered = 0;
        for (int round = 0; round < 3; ++round) {
            const int visibleTop = static_cast<int>(_view->send(SCI_DOCLINEFROMVISIBLE, static_cast<uptr_t>(firstVisible + covered)));
            lines = headersFor(std::max(top, visibleTop));
            if (static_cast<int>(lines.size()) > kMaxLines)
                lines.erase(lines.begin(), lines.end() - kMaxLines); // los más de dentro
            if (static_cast<int>(lines.size()) == covered)
                break;
            covered = static_cast<int>(lines.size());
        }
    }
    if (lines == _lines && isVisible() == !lines.empty()) {
        update();
        return;
    }
    _lines = std::move(lines);
    if (_lines.empty()) {
        hide();
        return;
    }
    setGeometry(0, 0, _view->viewport()->width(), lineHeight() * static_cast<int>(_lines.size()) + 2);
    show();
    raise();
    update();
}

void StickyScroll::paintEvent(QPaintEvent*)
{
    if (!_view || _lines.empty())
        return;
    QPainter painter(this);
    const int height = lineHeight();
    const QColor back = fromScintilla(_view->send(SCI_STYLEGETBACK, STYLE_DEFAULT));
    const QColor marginBack = fromScintilla(_view->send(SCI_STYLEGETBACK, STYLE_LINENUMBER));
    const QColor marginFore = fromScintilla(_view->send(SCI_STYLEGETFORE, STYLE_LINENUMBER));
    const bool dark = back.lightness() < 128;
    const QColor hover = dark ? back.lighter(135) : back.darker(106);
    int marginsWidth = 0;
    for (int m = 0; m < 5; ++m)
        marginsWidth += static_cast<int>(_view->send(SCI_GETMARGINWIDTHN, static_cast<uptr_t>(m)));
    const int lineNumberWidth = static_cast<int>(_view->send(SCI_GETMARGINWIDTHN, 0));
    const int textX = marginsWidth + static_cast<int>(_view->send(SCI_GETMARGINLEFT)) - static_cast<int>(_view->send(SCI_GETXOFFSET));
    const int zoom = static_cast<int>(_view->send(SCI_GETZOOM));
    const int tabWidth = std::max(1, static_cast<int>(_view->send(SCI_GETTABWIDTH)));

    // Las fuentes y colores de cada estilo, como los tiene el editor
    QHash<int, QFont> fonts;
    auto fontFor = [&](int style) {
        auto it = fonts.find(style);
        if (it != fonts.end())
            return *it;
        QByteArray family(static_cast<qsizetype>(_view->send(SCI_STYLEGETFONT, static_cast<uptr_t>(style))) + 1, '\0');
        _view->send(SCI_STYLEGETFONT, static_cast<uptr_t>(style), reinterpret_cast<sptr_t>(family.data()));
        QFont font(QString::fromUtf8(family.constData()));
        const double size = static_cast<double>(_view->send(SCI_STYLEGETSIZEFRACTIONAL, static_cast<uptr_t>(style))) / SC_FONT_SIZE_MULTIPLIER;
        font.setPointSizeF(std::max(4.0, size + zoom));
        font.setWeight(static_cast<QFont::Weight>(std::clamp(static_cast<int>(_view->send(SCI_STYLEGETWEIGHT, static_cast<uptr_t>(style))), 1, 1000)));
        font.setItalic(_view->send(SCI_STYLEGETITALIC, static_cast<uptr_t>(style)) != 0);
        return *fonts.insert(style, font);
    };
    const double spaceWidth = QFontMetricsF(fontFor(STYLE_DEFAULT)).horizontalAdvance(QLatin1Char(' '));

    for (int row = 0; row < static_cast<int>(_lines.size()); ++row) {
        const int line = _lines[static_cast<size_t>(row)];
        const QRect rect(0, row * height, width(), height);
        painter.fillRect(rect, row == _hover ? hover : back);
        // El margen, con el número de línea
        painter.fillRect(QRect(0, rect.top(), marginsWidth, height), marginBack);
        if (lineNumberWidth > 0) {
            painter.setFont(fontFor(STYLE_LINENUMBER));
            painter.setPen(marginFore);
            painter.drawText(QRect(0, rect.top(), lineNumberWidth - 4, height), Qt::AlignRight | Qt::AlignVCenter, QString::number(line + 1));
        }
        // El texto, por tramos del mismo estilo; los tabuladores, hasta la siguiente columna
        painter.save();
        painter.setClipRect(QRect(marginsWidth, rect.top(), width() - marginsWidth, height));
        const sptr_t start = _view->send(SCI_POSITIONFROMLINE, static_cast<uptr_t>(line));
        const sptr_t end = std::min(_view->send(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(line)), start + 2000);
        const auto* text = reinterpret_cast<const char*>(_view->send(SCI_GETRANGEPOINTER, static_cast<uptr_t>(start), end - start));
        double x = textX;
        int column = 0;
        sptr_t pos = start;
        while (pos < end && text) {
            const int style = static_cast<int>(_view->send(SCI_GETSTYLEINDEXAT, static_cast<uptr_t>(pos)));
            sptr_t runEnd = pos;
            while (runEnd < end && text[runEnd - start] != '\t'
                   && static_cast<int>(_view->send(SCI_GETSTYLEINDEXAT, static_cast<uptr_t>(runEnd))) == style)
                ++runEnd;
            if (runEnd == pos) { // un tabulador
                const int next = (column / tabWidth + 1) * tabWidth;
                x += (next - column) * spaceWidth;
                column = next;
                ++pos;
                continue;
            }
            const QString run = QString::fromUtf8(text + (pos - start), static_cast<qsizetype>(runEnd - pos));
            const QFont font = fontFor(style);
            painter.setFont(font);
            painter.setPen(fromScintilla(_view->send(SCI_STYLEGETFORE, static_cast<uptr_t>(style))));
            const QFontMetricsF metrics(font);
            painter.drawText(QPointF(x, rect.top() + (height + metrics.ascent() - metrics.descent()) / 2.0), run);
            x += metrics.horizontalAdvance(run);
            column += static_cast<int>(run.size());
            pos = runEnd;
            if (x > width())
                break;
        }
        painter.restore();
    }
    // Una raya debajo, para separarlo del texto que pasa por detrás
    QColor shadow = dark ? QColor(0, 0, 0, 140) : QColor(0, 0, 0, 40);
    painter.fillRect(QRect(0, height * static_cast<int>(_lines.size()), width(), 2), shadow);
}

void StickyScroll::mouseMoveEvent(QMouseEvent* event)
{
    const int row = event->position().toPoint().y() / lineHeight();
    if (row != _hover) {
        _hover = row < static_cast<int>(_lines.size()) ? row : -1;
        update();
    }
}

void StickyScroll::leaveEvent(QEvent*)
{
    _hover = -1;
    update();
}

// Un clic lleva a esa línea: arriba del todo y con el cursor en ella
void StickyScroll::mousePressEvent(QMouseEvent* event)
{
    const int row = event->position().toPoint().y() / lineHeight();
    if (row < 0 || row >= static_cast<int>(_lines.size()) || !_view)
        return;
    const int line = _lines[static_cast<size_t>(row)];
    _view->send(SCI_GOTOLINE, static_cast<uptr_t>(line));
    _view->send(SCI_SETFIRSTVISIBLELINE, static_cast<uptr_t>(_view->send(SCI_VISIBLEFROMDOCLINE, static_cast<uptr_t>(line))));
    _view->setFocus();
    event->accept();
}

void StickyScroll::wheelEvent(QWheelEvent* event)
{
    event->ignore(); // al editor (el padre)
}

bool StickyScroll::eventFilter(QObject* watched, QEvent* event)
{
    if (_view && watched == _view->viewport() && event->type() == QEvent::Resize && isVisible())
        resize(_view->viewport()->width(), height());
    return false;
}

} // namespace edita
