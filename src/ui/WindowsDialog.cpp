// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/WindowsDialog.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLocale>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace edita {

namespace {

constexpr int kIdRole = Qt::UserRole;
constexpr int kSortRole = Qt::UserRole + 1;

// Ordena el tamaño como número y lo demás sin distinguir mayúsculas
class Item : public QTreeWidgetItem
{
public:
    using QTreeWidgetItem::QTreeWidgetItem;
    bool operator<(const QTreeWidgetItem& other) const override
    {
        const int column = treeWidget() ? treeWidget()->sortColumn() : 0;
        const QVariant a = data(column, kSortRole);
        const QVariant b = other.data(column, kSortRole);
        if (a.typeId() == QMetaType::LongLong)
            return a.toLongLong() < b.toLongLong();
        return QString::compare(a.toString(), b.toString(), Qt::CaseInsensitive) < 0;
    }
};

} // namespace

WindowsDialog::WindowsDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("windowsDialog"));
    setWindowTitle(tr("Windows"));
    _list = new QTreeWidget(this);
    _list->setObjectName(QStringLiteral("windowsList"));
    _list->setHeaderLabels({tr("Name"), tr("Path"), tr("Type"), tr("Size")});
    _list->setRootIsDecorated(false);
    _list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    _list->setSortingEnabled(true);
    _list->sortByColumn(-1, Qt::AscendingOrder); // el orden de las pestañas hasta que se pulse una columna
    _list->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    _list->header()->setStretchLastSection(false);

    auto button = [this](const QString& text, const char* name) {
        auto* b = new QPushButton(text, this);
        b->setObjectName(QString::fromLatin1(name));
        b->setAutoDefault(false);
        return b;
    };
    QPushButton* activate = button(tr("&Activate"), "windowsActivate");
    activate->setDefault(true);
    QPushButton* save = button(tr("&Save"), "windowsSave");
    QPushButton* close = button(tr("&Close window(s)"), "windowsClose");
    QPushButton* sort = button(tr("Sort &tabs"), "windowsSort");
    QPushButton* ok = button(tr("&OK"), "windowsOk");
    connect(activate, &QPushButton::clicked, this, [this] {
        const auto ids = selected();
        if (ids.size() == 1) {
            emit activateRequested(ids.front());
            accept();
        }
    });
    connect(_list, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item) {
        emit activateRequested(item->data(0, kIdRole).value<BufferId>());
        accept();
    });
    connect(save, &QPushButton::clicked, this, [this] { emit saveRequested(selected()); });
    connect(close, &QPushButton::clicked, this, [this] { emit closeRequested(selected()); });
    connect(sort, &QPushButton::clicked, this, [this] { emit sortTabsRequested(listedOrder()); });
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    auto updateButtons = [this, activate, save, close] {
        const std::size_t n = selected().size();
        activate->setEnabled(n == 1);
        save->setEnabled(n > 0);
        close->setEnabled(n > 0);
    };
    connect(_list, &QTreeWidget::itemSelectionChanged, this, updateButtons);

    auto* buttons = new QVBoxLayout;
    for (QPushButton* b : {activate, save, close, sort})
        buttons->addWidget(b);
    buttons->addStretch();
    buttons->addWidget(ok);
    auto* layout = new QHBoxLayout(this);
    layout->addWidget(_list, 1);
    layout->addLayout(buttons);
    resize(760, 420);
    updateButtons();
}

void WindowsDialog::setRows(const std::vector<Row>& rows, BufferId current)
{
    const QSignalBlocker blocker(_list);
    const int column = _list->sortColumn();
    const Qt::SortOrder order = _list->header()->sortIndicatorOrder();
    _list->setSortingEnabled(false);
    _list->clear();
    const QLocale locale;
    for (const Row& row : rows) {
        auto* item = new Item(_list);
        item->setText(0, row.modified ? row.name + QLatin1Char('*') : row.name);
        item->setText(1, row.path);
        item->setText(2, row.type);
        item->setText(3, locale.formattedDataSize(row.size, 1, QLocale::DataSizeTraditionalFormat));
        item->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
        item->setData(0, kIdRole, QVariant::fromValue(row.id));
        item->setData(0, kSortRole, row.name);
        item->setData(1, kSortRole, row.path);
        item->setData(2, kSortRole, row.type);
        item->setData(3, kSortRole, static_cast<qlonglong>(row.size));
        if (row.id == current)
            item->setSelected(true);
    }
    _list->setSortingEnabled(true);
    _list->sortByColumn(column, order);
    for (int i = 0; i < 4; ++i)
        if (i != 1)
            _list->resizeColumnToContents(i);
    emit _list->itemSelectionChanged();
}

std::vector<BufferId> WindowsDialog::listedOrder() const
{
    std::vector<BufferId> ids;
    for (int i = 0; i < _list->topLevelItemCount(); ++i)
        ids.push_back(_list->topLevelItem(i)->data(0, kIdRole).value<BufferId>());
    return ids;
}

std::vector<BufferId> WindowsDialog::selected() const
{
    std::vector<BufferId> ids;
    for (int i = 0; i < _list->topLevelItemCount(); ++i) {
        if (_list->topLevelItem(i)->isSelected())
            ids.push_back(_list->topLevelItem(i)->data(0, kIdRole).value<BufferId>());
    }
    return ids;
}

} // namespace edita
