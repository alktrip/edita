// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/FindBar.h"

#include "search/SearchEngine.h"
#include "search/SearchTarget.h"
#include "syntax/SyntaxStyler.h"
#include "ui/EditorView.h"
#include "ui/Icons.h"

#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace edita {

namespace {

constexpr int kMaxHighlights = 10000; // más coincidencias: se cuentan pero no se resaltan todas

QToolButton* toolButton(QWidget* parent, const QString& text, const QString& tip, bool checkable = false)
{
    auto* button = new QToolButton(parent);
    button->setText(text);
    button->setToolTip(tip);
    button->setCheckable(checkable);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

} // namespace

FindBar::FindBar(EditorView* view, QWidget* parent)
    : QFrame(parent)
    , _view(view)
{
    setObjectName(QStringLiteral("findBar"));
    setFrameShape(QFrame::StyledPanel);
    setAutoFillBackground(true);
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(16);
    shadow->setOffset(0, 3);
    shadow->setColor(QColor(0, 0, 0, 80));
    setGraphicsEffect(shadow);

    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(4, 4, 4, 4);
    grid->setHorizontalSpacing(2);
    grid->setVerticalSpacing(3);
    _expand = toolButton(this, QStringLiteral("›"), tr("Toggle Replace"));
    _expand->setObjectName(QStringLiteral("findBarExpand"));
    grid->addWidget(_expand, 0, 0, Qt::AlignTop);

    auto* findRow = new QHBoxLayout;
    findRow->setSpacing(2);
    _find = new QLineEdit(this);
    _find->setObjectName(QStringLiteral("findBarFind"));
    _find->setPlaceholderText(tr("Find"));
    _find->setMinimumWidth(220);
    findRow->addWidget(_find, 1);
    _matchCase = toolButton(this, QStringLiteral("Aa"), tr("Match case (Alt+C)"), true);
    _matchCase->setObjectName(QStringLiteral("findBarMatchCase"));
    _wholeWord = toolButton(this, QStringLiteral("ab|"), tr("Match whole word only (Alt+W)"), true);
    _wholeWord->setObjectName(QStringLiteral("findBarWholeWord"));
    _regex = toolButton(this, QStringLiteral(".*"), tr("Use regular expression (Alt+R)"), true);
    _regex->setObjectName(QStringLiteral("findBarRegex"));
    findRow->addWidget(_matchCase);
    findRow->addWidget(_wholeWord);
    findRow->addWidget(_regex);
    _count = new QLabel(this);
    _count->setObjectName(QStringLiteral("findBarCount"));
    _count->setMinimumWidth(fontMetrics().horizontalAdvance(QStringLiteral("9999 of 9999")) + 8);
    _count->setAlignment(Qt::AlignCenter);
    findRow->addWidget(_count);
    auto* previous = toolButton(this, QStringLiteral("↑"), tr("Previous match (Shift+Enter)"));
    auto* next = toolButton(this, QStringLiteral("↓"), tr("Next match (Enter)"));
    auto* more = toolButton(this, QStringLiteral("⋯"), tr("More options (the Find dialog)"));
    auto* closeButton = toolButton(this, QStringLiteral("✕"), tr("Close (Esc)"));
    previous->setObjectName(QStringLiteral("findBarPrevious"));
    next->setObjectName(QStringLiteral("findBarNext"));
    more->setObjectName(QStringLiteral("findBarMore"));
    closeButton->setObjectName(QStringLiteral("findBarClose"));
    findRow->addWidget(previous);
    findRow->addWidget(next);
    findRow->addWidget(more);
    findRow->addWidget(closeButton);
    grid->addLayout(findRow, 0, 1);

    _replaceRow = new QWidget(this);
    auto* replaceRow = new QHBoxLayout(_replaceRow);
    replaceRow->setContentsMargins(0, 0, 0, 0);
    replaceRow->setSpacing(2);
    _replace = new QLineEdit(_replaceRow);
    _replace->setObjectName(QStringLiteral("findBarReplace"));
    _replace->setPlaceholderText(tr("Replace"));
    replaceRow->addWidget(_replace, 1);
    auto* replaceOneButton = toolButton(_replaceRow, tr("Replace"), tr("Replace this match (Enter)"));
    auto* replaceAllButton = toolButton(_replaceRow, tr("All"), tr("Replace all (Ctrl+Alt+Enter)"));
    replaceOneButton->setObjectName(QStringLiteral("findBarReplaceOne"));
    replaceAllButton->setObjectName(QStringLiteral("findBarReplaceAll"));
    replaceRow->addWidget(replaceOneButton);
    replaceRow->addWidget(replaceAllButton);
    replaceRow->addStretch(0);
    grid->addWidget(_replaceRow, 1, 1);
    _replaceRow->hide();

    connect(_expand, &QToolButton::clicked, this, [this] {
        _replaceRow->setVisible(_replaceRow->isHidden());
        _expand->setText(_replaceRow->isHidden() ? QStringLiteral("›") : QStringLiteral("⌄"));
        adjustSize();
        reposition();
    });
    connect(_find, &QLineEdit::textChanged, this, [this] { _timer.start(); });
    for (QToolButton* toggle : {_matchCase, _wholeWord, _regex})
        connect(toggle, &QToolButton::toggled, this, [this] { highlightAll(true); });
    connect(previous, &QToolButton::clicked, this, [this] { findNext(true); });
    connect(next, &QToolButton::clicked, this, [this] { findNext(false); });
    connect(more, &QToolButton::clicked, this, [this] { emit moreOptionsRequested(!_replaceRow->isHidden(), _find->text()); });
    connect(closeButton, &QToolButton::clicked, this, &FindBar::close);
    connect(replaceOneButton, &QToolButton::clicked, this, &FindBar::replaceOne);
    connect(replaceAllButton, &QToolButton::clicked, this, [this] { replaceAll(); });

    // Mientras se escribe, con una pausa breve; al cambiar el texto del documento, igual
    _timer.setSingleShot(true);
    _timer.setInterval(80);
    connect(&_timer, &QTimer::timeout, this, [this] { highlightAll(true); });
    connect(view, &ScintillaEditBase::modified, this, [this](Scintilla::ModificationFlags type) {
        if (isOpen() && (static_cast<int>(type) & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)))
            QTimer::singleShot(0, this, [this] { highlightAll(false); });
    });

    _find->installEventFilter(this);
    _replace->installEventFilter(this);
    parent->installEventFilter(this);
    hide();
}

SearchOptions FindBar::options() const
{
    SearchOptions o;
    o.pattern = _find->text().toStdString();
    o.replacement = _replace->text().toStdString();
    o.mode = _regex->isChecked() ? SearchMode::Regex : SearchMode::Normal;
    o.matchCase = _matchCase->isChecked();
    o.wholeWord = _wholeWord->isChecked();
    o.wrapAround = true;
    return o;
}

void FindBar::setToggles(bool matchCase, bool wholeWord, bool regex)
{
    _matchCase->setChecked(matchCase);
    _wholeWord->setChecked(wholeWord);
    _regex->setChecked(regex);
}

void FindBar::open(bool replace, const QString& text)
{
    if (!_view)
        return;
    _anchor = _view->send(SCI_GETSELECTIONSTART);
    _replaceRow->setVisible(replace);
    _expand->setText(replace ? QStringLiteral("⌄") : QStringLiteral("›"));
    adjustSize();
    reposition();
    show();
    raise();
    if (!text.isEmpty() && text != _find->text()) {
        const QSignalBlocker block(_find);
        _find->setText(text);
    }
    _find->setFocus();
    _find->selectAll();
    highlightAll(true);
}

void FindBar::close()
{
    clearHighlights();
    hide();
    if (_view)
        _view->setFocus();
    emit closed();
}

void FindBar::reposition()
{
    // Arriba a la derecha del editor, sin tapar la barra de desplazamiento
    QWidget* area = _view ? static_cast<QWidget*>(_view) : parentWidget();
    const QPoint topRight = area->mapTo(parentWidget(), QPoint(area->width(), 0));
    const int scrollBar = _view && _view->verticalScrollBar()->isVisible() ? _view->verticalScrollBar()->width() : 0;
    const int w = std::min(sizeHint().width(), std::max(200, area->width() - 40));
    resize(w, sizeHint().height());
    move(topRight.x() - w - scrollBar - 12, topRight.y() + 2);
}

void FindBar::clearHighlights()
{
    if (!_view)
        return;
    _view->send(SCI_SETINDICATORCURRENT, kIndicatorIncremental);
    _view->send(SCI_INDICATORCLEARRANGE, 0, _view->send(SCI_GETLENGTH));
}

// Todas las coincidencias, resaltadas; si moveToNearest, la primera desde donde estaba el
// cursor al abrir queda seleccionada (como en la búsqueda incremental)
void FindBar::highlightAll(bool moveToNearest)
{
    _timer.stop();
    if (!_view || isHidden())
        return;
    clearHighlights();
    _total = 0;
    _invalid = false;
    const SearchOptions o = options();
    if (o.pattern.empty()) {
        updateCount();
        return;
    }
    ViewSearchTarget target(*_view);
    int highlighted = 0;
    const RangeResult result = processRange(target, o, RangeOperation::CountAll, 0, _view->send(SCI_GETLENGTH),
                                            [&](SciPos start, SciPos end) {
                                                if (highlighted++ < kMaxHighlights && end > start) {
                                                    _view->send(SCI_SETINDICATORCURRENT, kIndicatorIncremental);
                                                    _view->send(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(start), end - start);
                                                }
                                            });
    _invalid = result.invalidRegex;
    _total = result.count;
    if (moveToNearest && _total > 0) {
        const FindResult found = edita::findNext(target, o, _anchor, _anchor, FindNextType::FindNext, true);
        if (found.found()) {
            _view->send(SCI_SETSEL, static_cast<uptr_t>(found.start), found.end);
            _view->send(SCI_SCROLLRANGE, static_cast<uptr_t>(found.end), found.start);
        }
    }
    updateCount();
    emit searched(o);
}

void FindBar::updateCount()
{
    if (!_view)
        return;
    QString text;
    bool problem = false;
    if (_invalid) {
        text = tr("Invalid regex");
        problem = true;
    } else if (_find->text().isEmpty()) {
        text.clear();
    } else if (_total == 0) {
        text = tr("No results");
        problem = true;
    } else {
        // Cuál es la seleccionada: las que empiezan antes que ella, más una
        const sptr_t selStart = _view->send(SCI_GETSELECTIONSTART);
        const sptr_t selEnd = _view->send(SCI_GETSELECTIONEND);
        int index = 0;
        ViewSearchTarget target(*_view);
        bool onMatch = false;
        processRange(target, options(), RangeOperation::CountAll, 0, selEnd, [&](SciPos start, SciPos end) {
            if (start < selStart)
                ++index;
            else if (start == selStart && end == selEnd)
                onMatch = true;
        });
        text = onMatch ? tr("%1 of %2").arg(index + 1).arg(_total) : tr("%n result(s)", nullptr, _total);
    }
    _count->setText(text);
    QPalette pal = _find->palette();
    pal.setColor(QPalette::Base, problem ? QColor(0xCF, 0x22, 0x2E, 40) : palette().color(QPalette::Base));
    _find->setPalette(pal);
}

QString FindBar::countText() const
{
    return _count->text();
}

void FindBar::findNext(bool backward)
{
    if (!_view || _find->text().isEmpty())
        return;
    // Intro justo después de escribir: primero la búsqueda pendiente (si no, se seguiría
    // buscando desde lo que había antes de escribir)
    if (_timer.isActive()) {
        highlightAll(true);
        return;
    }
    SearchOptions o = options();
    o.searchUp = backward;
    ViewSearchTarget target(*_view);
    const FindResult found = edita::findNext(target, o, _view->send(SCI_GETSELECTIONSTART), _view->send(SCI_GETSELECTIONEND));
    if (found.found()) {
        _view->send(SCI_SETSEL, static_cast<uptr_t>(found.start), found.end);
        _view->send(SCI_SCROLLRANGE, static_cast<uptr_t>(found.end), found.start);
    }
    updateCount();
    emit searched(o);
}

void FindBar::replaceOne()
{
    if (!_view || _find->text().isEmpty() || _view->send(SCI_GETREADONLY))
        return;
    if (_timer.isActive())
        highlightAll(true); // lo recién escrito, seleccionado: Intro ya lo reemplaza
    ViewSearchTarget target(*_view);
    const ReplaceOutcome outcome = replaceNext(target, options(), _view->send(SCI_GETSELECTIONSTART), _view->send(SCI_GETSELECTIONEND));
    if (outcome.selStart >= 0) {
        _view->send(SCI_SETSEL, static_cast<uptr_t>(outcome.selStart), outcome.selEnd);
        _view->send(SCI_SCROLLRANGE, static_cast<uptr_t>(outcome.selEnd), outcome.selStart);
    }
    highlightAll(false);
}

int FindBar::replaceAll()
{
    if (!_view || _find->text().isEmpty() || _view->send(SCI_GETREADONLY))
        return 0;
    ViewSearchTarget target(*_view);
    _view->send(SCI_BEGINUNDOACTION);
    const RangeResult result = processRange(target, options(), RangeOperation::ReplaceAll, 0, _view->send(SCI_GETLENGTH));
    _view->send(SCI_ENDUNDOACTION);
    highlightAll(false);
    _count->setText(tr("%n replaced", nullptr, result.count));
    return result.count;
}

void FindBar::refresh()
{
    if (isOpen())
        highlightAll(false);
}

bool FindBar::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parentWidget()) {
        if (event->type() == QEvent::Resize && isOpen())
            reposition();
        return false;
    }
    if (event->type() != QEvent::KeyPress)
        return false;
    auto* key = static_cast<QKeyEvent*>(event);
    const Qt::KeyboardModifiers mods = key->modifiers();
    if (key->key() == Qt::Key_Escape) {
        close();
        return true;
    }
    // Alt+C, Alt+W, Alt+R: mayúsculas, palabra completa, expresión regular
    if (mods == Qt::AltModifier) {
        QToolButton* toggle = key->key() == Qt::Key_C ? _matchCase : key->key() == Qt::Key_W ? _wholeWord : key->key() == Qt::Key_R ? _regex : nullptr;
        if (toggle) {
            toggle->toggle();
            return true;
        }
    }
    if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
        if (watched == _replace) {
            if (mods & Qt::ControlModifier && mods & Qt::AltModifier)
                replaceAll();
            else
                replaceOne();
        } else {
            findNext(mods & Qt::ShiftModifier);
        }
        return true;
    }
    return false;
}

} // namespace edita
