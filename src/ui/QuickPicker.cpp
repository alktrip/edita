// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/QuickPicker.h"

#include "core/FuzzyMatch.h"

#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QScreen>
#include <QVBoxLayout>

#include <algorithm>

namespace edita {

namespace {

constexpr int kIndexRole = Qt::UserRole + 1;

} // namespace

QuickPicker::QuickPicker(QWidget* parent)
    : QFrame(parent, Qt::Popup)
{
    setObjectName(QStringLiteral("quickPicker"));
    setFrameShape(QFrame::StyledPanel);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    _input = new QLineEdit(this);
    _input->setObjectName(QStringLiteral("quickPickerInput"));
    _input->setClearButtonEnabled(true);
    layout->addWidget(_input);
    _list = new QListWidget(this);
    _list->setObjectName(QStringLiteral("quickPickerList"));
    _list->setFrameShape(QFrame::NoFrame);
    _list->setFocusPolicy(Qt::NoFocus);
    _list->setUniformItemSizes(true);
    layout->addWidget(_list);
    connect(_input, &QLineEdit::textChanged, this, [this] { filter(); });
    connect(_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) { choose(_list->row(item)); });
    _input->installEventFilter(this);
}

void QuickPicker::popup(QWidget* anchor, const QString& placeholder, std::vector<Item> items)
{
    _items = std::move(items);
    _input->setPlaceholderText(placeholder);
    _input->clear();
    filter();
    // Encima del control, sin salirse de la pantalla
    const int rowHeight = _list->sizeHintForRow(0) > 0 ? _list->sizeHintForRow(0) : fontMetrics().height() + 6;
    const int rows = std::min<int>(static_cast<int>(_items.size()), 14);
    const int width = std::max(300, anchor ? anchor->width() + 120 : 300);
    const int height = _input->sizeHint().height() + rowHeight * std::max(rows, 3) + 16;
    resize(width, height);
    QPoint pos = anchor ? anchor->mapToGlobal(QPoint(anchor->width() - width, -height - 2)) : QCursor::pos();
    if (const QScreen* screen = anchor ? anchor->screen() : QApplication::primaryScreen()) {
        const QRect area = screen->availableGeometry();
        pos.setX(std::clamp(pos.x(), area.left(), std::max(area.left(), area.right() - width)));
        if (pos.y() < area.top())
            pos.setY(anchor ? anchor->mapToGlobal(QPoint(0, anchor->height() + 2)).y() : area.top());
    }
    move(pos);
    show();
    _input->setFocus();
}

void QuickPicker::filter()
{
    const QString query = _input->text().trimmed();
    struct Row
    {
        int index;
        int score;
    };
    std::vector<Row> rows;
    for (int i = 0; i < static_cast<int>(_items.size()); ++i) {
        if (query.isEmpty()) {
            rows.push_back({i, -i});
            continue;
        }
        const Item& item = _items[static_cast<size_t>(i)];
        FuzzyMatch match = fuzzyMatch(query, item.text);
        if (!match.matched())
            match = fuzzyMatch(query, item.detail + QLatin1Char(' ') + item.text);
        if (match.matched())
            rows.push_back({i, match.score});
    }
    if (!query.isEmpty())
        std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.score > b.score; });
    _list->clear();
    int current = 0;
    for (const Row& row : rows) {
        const Item& item = _items[static_cast<size_t>(row.index)];
        QString text = item.text;
        if (!item.detail.isEmpty())
            text += QStringLiteral("    ") + item.detail;
        auto* entry = new QListWidgetItem(QStringLiteral("%1  %2").arg(item.checked ? QStringLiteral("✓") : QStringLiteral("  "), text), _list);
        entry->setData(kIndexRole, row.index);
        if (item.checked && query.isEmpty())
            current = _list->count() - 1;
    }
    if (_list->count() > 0) {
        _list->setCurrentRow(current);
        _list->scrollToItem(_list->currentItem(), QAbstractItemView::PositionAtCenter);
    }
}

void QuickPicker::choose(int row)
{
    QListWidgetItem* item = _list->item(row);
    if (!item)
        return;
    const int index = item->data(kIndexRole).toInt();
    if (index < 0 || index >= static_cast<int>(_items.size()))
        return;
    const auto run = _items[static_cast<size_t>(index)].run;
    hide();
    if (run)
        run();
}

QStringList QuickPicker::visibleTexts() const
{
    QStringList texts;
    for (int i = 0; i < _list->count(); ++i)
        texts << _items[static_cast<size_t>(_list->item(i)->data(kIndexRole).toInt())].text;
    return texts;
}

bool QuickPicker::activate(const QString& text)
{
    for (int i = 0; i < _list->count(); ++i) {
        if (_items[static_cast<size_t>(_list->item(i)->data(kIndexRole).toInt())].text == text) {
            choose(i);
            return true;
        }
    }
    return false;
}

bool QuickPicker::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != _input || event->type() != QEvent::KeyPress)
        return false;
    auto* key = static_cast<QKeyEvent*>(event);
    const int rows = _list->count();
    switch (key->key()) {
    case Qt::Key_Escape:
        hide();
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        choose(_list->currentRow());
        return true;
    case Qt::Key_Up:
        if (rows)
            _list->setCurrentRow(_list->currentRow() <= 0 ? rows - 1 : _list->currentRow() - 1);
        return true;
    case Qt::Key_Down:
        if (rows)
            _list->setCurrentRow(_list->currentRow() >= rows - 1 ? 0 : _list->currentRow() + 1);
        return true;
    case Qt::Key_PageUp:
        if (rows)
            _list->setCurrentRow(std::max(0, _list->currentRow() - 10));
        return true;
    case Qt::Key_PageDown:
        if (rows)
            _list->setCurrentRow(std::min(rows - 1, _list->currentRow() + 10));
        return true;
    default:
        return false;
    }
}

} // namespace edita
