// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/ScrollBarMarks.h"

#include "syntax/SyntaxStyler.h"
#include "ui/EditCommands.h"
#include "ui/EditorView.h"
#include "ui/GitChanges.h"

#include <QEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStyle>
#include <QStyleOptionSlider>

#include <algorithm>

namespace edita {

namespace {

constexpr int kMaxMarks = 4000; // más allá, se dejan de añadir (una por píxel basta)

QColor fromScintilla(sptr_t color)
{
    return QColor(static_cast<int>(color & 0xFF), static_cast<int>((color >> 8) & 0xFF), static_cast<int>((color >> 16) & 0xFF));
}

} // namespace

ScrollBarMarks::ScrollBarMarks(EditorView* view, GitChanges* git)
    : QWidget(view->verticalScrollBar())
    , _view(view)
    , _git(git)
{
    setObjectName(QStringLiteral("scrollBarMarks"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    QScrollBar* bar = view->verticalScrollBar();
    setGeometry(bar->rect());
    bar->installEventFilter(this);
    _timer.setSingleShot(true);
    _timer.setInterval(120);
    connect(&_timer, &QTimer::timeout, this, &ScrollBarMarks::updateNow);
    connect(view, &ScintillaEditBase::updateUi, this, [this](Scintilla::Update updated) {
        const int flags = static_cast<int>(updated);
        // El cursor, enseguida; lo demás (el resaltado inteligente cambia con la selección), en breve
        if (flags & SC_UPDATE_SELECTION) {
            const int line = static_cast<int>(_view->send(SCI_LINEFROMPOSITION, static_cast<uptr_t>(_view->send(SCI_GETCURRENTPOS))));
            if (line != _caretLine) {
                _caretLine = line;
                update();
            }
        }
        if (flags & (SC_UPDATE_CONTENT | SC_UPDATE_SELECTION))
            scheduleUpdate();
    });
    connect(view, &ScintillaEditBase::modified, this, [this] { scheduleUpdate(); });
    if (git)
        connect(git, &GitChanges::changed, this, &ScrollBarMarks::scheduleUpdate);
    show();
    scheduleUpdate();
}

void ScrollBarMarks::setEnabled(bool enabled)
{
    _enabled = enabled;
    setVisible(enabled);
    if (enabled)
        scheduleUpdate();
}

void ScrollBarMarks::scheduleUpdate()
{
    // Como mucho una vez cada 120 ms, sin volver a empezar la espera con cada aviso: los
    // indicadores de otros (paréntesis, colores) avisan a menudo y la espera no acababa
    if (_enabled && !_timer.isActive())
        _timer.start();
}

void ScrollBarMarks::updateNow()
{
    _timer.stop();
    _marks.clear();
    if (!_view || !_enabled)
        return;
    const auto add = [this](int line, int lines, const QColor& color, int lane) {
        if (static_cast<int>(_marks.size()) < kMaxMarks)
            _marks.push_back({line, std::max(1, lines), color, lane});
    };
    // Git: los colores de sus marcadores
    if (_git && _git->isEnabled()) {
        for (const LineChange& c : _git->changes())
            add(c.line, c.kind == LineChangeKind::Deleted ? 1 : c.count, _git->color(c.kind), 0);
    }
    // Lo marcado («Marcar todo») y el resaltado inteligente, con el color de su estilo
    const sptr_t length = _view->send(SCI_GETLENGTH);
    for (const int indicator : {kIndicatorFound, kIndicatorSmart, kIndicatorIncremental}) {
        const QColor color = fromScintilla(_view->send(SCI_INDICGETFORE, static_cast<uptr_t>(indicator)));
        int lastLine = -1;
        // Los tramos con y sin el indicador se recorren con SCI_INDICATOREND
        for (sptr_t pos = 0; pos < length && static_cast<int>(_marks.size()) < kMaxMarks;) {
            const sptr_t end = _view->send(SCI_INDICATOREND, static_cast<uptr_t>(indicator), pos);
            if (_view->send(SCI_INDICATORVALUEAT, static_cast<uptr_t>(indicator), pos)) {
                const int line = static_cast<int>(_view->send(SCI_LINEFROMPOSITION, static_cast<uptr_t>(pos)));
                if (line != lastLine)
                    add(line, 1, color, 1);
                lastLine = line;
            }
            if (end <= pos)
                break;
            pos = end;
        }
    }
    // Marcadores
    const QColor bookmark(0x2E, 0x8B, 0xE6); // el del marcador (SyntaxStyler)
    for (sptr_t line = _view->send(SCI_MARKERNEXT, 0, 1 << kBookmarkMarker); line >= 0 && static_cast<int>(_marks.size()) < kMaxMarks;
         line = _view->send(SCI_MARKERNEXT, static_cast<uptr_t>(line + 1), 1 << kBookmarkMarker))
        add(static_cast<int>(line), 1, bookmark, 2);
    _caretLine = static_cast<int>(_view->send(SCI_LINEFROMPOSITION, static_cast<uptr_t>(_view->send(SCI_GETCURRENTPOS))));
    update();
}

// El hueco por el que se mueve el deslizador (sin las flechas)
QRect ScrollBarMarks::track() const
{
    auto* bar = static_cast<QScrollBar*>(parentWidget());
    QStyleOptionSlider option;
    option.initFrom(bar);
    option.orientation = Qt::Vertical;
    option.minimum = bar->minimum();
    option.maximum = bar->maximum();
    option.sliderPosition = bar->sliderPosition();
    option.sliderValue = bar->value();
    option.pageStep = bar->pageStep();
    option.singleStep = bar->singleStep();
    option.subControls = QStyle::SC_All;
    return bar->style()->subControlRect(QStyle::CC_ScrollBar, &option, QStyle::SC_ScrollBarGroove, bar);
}

void ScrollBarMarks::paintEvent(QPaintEvent*)
{
    if (!_view || !_enabled)
        return;
    const QRect groove = track().adjusted(0, 2, 0, -2);
    if (groove.height() <= 0)
        return;
    // Las líneas visibles (con plegado y ajuste de línea): como las cuenta la barra
    const sptr_t lineCount = _view->send(SCI_GETLINECOUNT);
    const auto total = static_cast<double>(std::max<sptr_t>(1, _view->send(SCI_VISIBLEFROMDOCLINE, static_cast<uptr_t>(lineCount))));
    const auto y = [&](int line) {
        const sptr_t visible = _view->send(SCI_VISIBLEFROMDOCLINE, static_cast<uptr_t>(std::max(0, line)));
        return groove.top() + static_cast<int>(groove.height() * (static_cast<double>(visible) / total));
    };
    QPainter painter(this);
    const int width = groove.width();
    const int laneWidth = std::max(2, width / 3);
    const int lanes[3][2] = {{groove.left() + 1, laneWidth - 1}, {groove.left() + laneWidth, laneWidth}, {groove.right() - laneWidth + 1, laneWidth - 1}};
    for (const Mark& mark : _marks) {
        const int top = y(mark.line);
        const int bottom = std::max(top + 2, y(mark.line + mark.lines));
        painter.fillRect(QRect(lanes[mark.lane][0], top, lanes[mark.lane][1], bottom - top), mark.color);
    }
    // El cursor: una raya a lo ancho
    if (_caretLine >= 0) {
        QColor caret = palette().color(QPalette::Text);
        caret.setAlphaF(0.55f);
        painter.fillRect(QRect(groove.left(), y(_caretLine), width, 2), caret);
    }
}

bool ScrollBarMarks::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parentWidget() && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        setGeometry(parentWidget()->rect());
        raise();
    }
    return false;
}

} // namespace edita
