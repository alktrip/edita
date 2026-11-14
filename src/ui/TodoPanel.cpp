// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TodoPanel.h"

#include "ui/Icons.h"

#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <map>

namespace edita {

namespace {

enum Role { PathRole = Qt::UserRole, LineRole, ColumnRole };

} // namespace

TodoPanel::TodoPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("todoPanel"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    auto* bar = new QHBoxLayout;
    _scope = new QComboBox(this);
    _scope->setObjectName(QStringLiteral("todoScope"));
    _scope->addItem(tr("Current document"));
    _scope->addItem(tr("Workspace folders"));
    _scope->setToolTip(tr("The folders of Folder as Workspace or, without them, the folder of the document"));
    bar->addWidget(_scope);
    _refresh = new QToolButton(this);
    _refresh->setObjectName(QStringLiteral("todoRefresh"));
    _refresh->setIcon(icons::icon(QStringLiteral("reload")));
    _refresh->setToolTip(tr("Search again"));
    _refresh->setAutoRaise(true);
    bar->addWidget(_refresh);
    _status = new QLabel(this);
    _status->setObjectName(QStringLiteral("todoStatus"));
    bar->addWidget(_status, 1);
    layout->addLayout(bar);

    _tree = new QTreeWidget(this);
    _tree->setObjectName(QStringLiteral("todoTree"));
    _tree->setHeaderLabels({tr("Comment"), tr("Text"), tr("Line")});
    _tree->setUniformRowHeights(true);
    _tree->header()->setStretchLastSection(false);
    _tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    _tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    _tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    layout->addWidget(_tree, 1);

    connect(_scope, &QComboBox::currentIndexChanged, this, [this] { emit scopeChanged(scope()); });
    connect(_refresh, &QToolButton::clicked, this, &TodoPanel::refreshRequested);
    connect(_tree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item) {
        const QString path = item->data(0, PathRole).toString();
        if (item->parent())
            emit activated(path, item->data(0, LineRole).toInt(), item->data(0, ColumnRole).toInt());
    });
}

TodoPanel::Scope TodoPanel::scope() const
{
    return _scope->currentIndex() == 1 ? Scope::Workspace : Scope::Document;
}

void TodoPanel::setScope(Scope scope)
{
    _scope->setCurrentIndex(scope == Scope::Workspace ? 1 : 0);
}

void TodoPanel::setBusy(bool busy)
{
    _refresh->setEnabled(!busy);
    if (busy)
        _status->setText(tr("Searching…"));
}

void TodoPanel::setItems(const std::vector<todo::Item>& items, const QString& root)
{
    _tree->clear();
    // Por archivo, en orden; dentro, por línea
    std::map<QString, std::vector<const todo::Item*>> byFile;
    for (const todo::Item& item : items)
        byFile[item.path].push_back(&item);
    const QDir base(root);
    for (const auto& [path, list] : byFile) {
        QString shown = root.isEmpty() ? QFileInfo(path).fileName() : base.relativeFilePath(path);
        if (shown.isEmpty())
            shown = tr("(new document)");
        auto* file = new QTreeWidgetItem(_tree, {shown});
        file->setToolTip(0, QDir::toNativeSeparators(path));
        file->setFirstColumnSpanned(true);
        for (const todo::Item* item : list) {
            auto* child = new QTreeWidgetItem(file, {item->kind, item->text, QString::number(item->line + 1)});
            child->setData(0, PathRole, path);
            child->setData(0, LineRole, item->line);
            child->setData(0, ColumnRole, item->column);
            child->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
            child->setToolTip(1, item->text);
        }
        file->setExpanded(true);
    }
    _refresh->setEnabled(true);
    _status->setText(items.empty() ? tr("No TODO comments") : tr("%n comment(s)", nullptr, static_cast<int>(items.size())));
}

} // namespace edita
