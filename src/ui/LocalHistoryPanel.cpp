// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/LocalHistoryPanel.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace edita {

LocalHistoryPanel::LocalHistoryPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("localHistoryPanel"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    _label = new QLabel(this);
    _label->setObjectName(QStringLiteral("localHistoryLabel"));
    _label->setWordWrap(true);
    layout->addWidget(_label);
    _tree = new QTreeWidget(this);
    _tree->setObjectName(QStringLiteral("localHistoryTree"));
    _tree->setColumnCount(2);
    _tree->setHeaderLabels({tr("Saved"), tr("Size")});
    _tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    _tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    _tree->header()->setStretchLastSection(false);
    _tree->setRootIsDecorated(false);
    _tree->setUniformRowHeights(true);
    layout->addWidget(_tree, 1);

    auto* buttons = new QHBoxLayout;
    auto button = [this, buttons](const QString& text, const char* name, const QString& tip) {
        auto* b = new QPushButton(text, this);
        b->setObjectName(QString::fromLatin1(name));
        b->setToolTip(tip);
        buttons->addWidget(b);
        return b;
    };
    _compare = button(tr("&Compare"), "localHistoryCompare", tr("Compare this version with the document"));
    _restore = button(tr("&Restore"), "localHistoryRestore",
                      tr("Replace the text of the document with this version (it can be undone)"));
    _open = button(tr("&Open"), "localHistoryOpen", tr("Open this version in a new tab"));
    _delete = button(tr("&Delete"), "localHistoryDelete", tr("Delete this version from the history"));
    buttons->addStretch(1);
    layout->addLayout(buttons);

    auto emitFor = [this](void (LocalHistoryPanel::*signal)(const LocalHistory::Version&)) {
        return [this, signal] {
            LocalHistory::Version v;
            if (selected(v))
                emit(this->*signal)(v);
        };
    };
    connect(_compare, &QPushButton::clicked, this, emitFor(&LocalHistoryPanel::compareRequested));
    connect(_restore, &QPushButton::clicked, this, emitFor(&LocalHistoryPanel::restoreRequested));
    connect(_open, &QPushButton::clicked, this, emitFor(&LocalHistoryPanel::openRequested));
    connect(_delete, &QPushButton::clicked, this, emitFor(&LocalHistoryPanel::deleteRequested));
    connect(_tree, &QTreeWidget::itemActivated, this, emitFor(&LocalHistoryPanel::compareRequested));
    connect(_tree, &QTreeWidget::currentItemChanged, this, &LocalHistoryPanel::updateButtons);
    setFile({}, {}, true);
}

QString LocalHistoryPanel::describe(const QDateTime& when, const QDateTime& now)
{
    const QDateTime local = when.toLocalTime();
    const QString time = QLocale().toString(local.time(), QStringLiteral("HH:mm:ss"));
    const qint64 days = local.date().daysTo(now.toLocalTime().date());
    if (days == 0)
        return tr("Today %1").arg(time);
    if (days == 1)
        return tr("Yesterday %1").arg(time);
    return QLocale().toString(local.date(), QLocale::ShortFormat) + QLatin1Char(' ') + time;
}

void LocalHistoryPanel::setFile(const QString& path, const QList<LocalHistory::Version>& versions, bool enabled)
{
    const bool samePath = path == _path;
    QString currentId;
    if (QTreeWidgetItem* item = _tree->currentItem(); item && samePath)
        currentId = item->data(0, Qt::UserRole).toString();
    _path = path;
    _versions = versions;
    _tree->clear();
    if (!enabled)
        _label->setText(tr("The local history is turned off (Settings › Preferences › Backup)."));
    else if (path.isEmpty())
        _label->setText(tr("The document has not been saved to a file yet."));
    else if (versions.isEmpty())
        _label->setText(tr("%1: no saved versions yet. Each save keeps one.").arg(QFileInfo(path).fileName()));
    else
        _label->setText(tr("%1: %n saved version(s)", nullptr, static_cast<int>(versions.size())).arg(QFileInfo(path).fileName()));
    const QDateTime now = QDateTime::currentDateTime();
    QTreeWidgetItem* current = nullptr;
    for (const LocalHistory::Version& v : versions) {
        auto* item = new QTreeWidgetItem(_tree, {describe(v.when, now), QLocale().formattedDataSize(v.size)});
        item->setData(0, Qt::UserRole, v.id);
        item->setToolTip(0, QLocale().toString(v.when.toLocalTime(), QLocale::LongFormat));
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        if (v.id == currentId)
            current = item;
    }
    if (current)
        _tree->setCurrentItem(current);
    updateButtons();
}

bool LocalHistoryPanel::selected(LocalHistory::Version& version) const
{
    const QTreeWidgetItem* item = _tree->currentItem();
    if (!item)
        return false;
    const QString id = item->data(0, Qt::UserRole).toString();
    for (const LocalHistory::Version& v : _versions) {
        if (v.id == id) {
            version = v;
            return true;
        }
    }
    return false;
}

void LocalHistoryPanel::updateButtons()
{
    const bool any = _tree->currentItem() != nullptr;
    for (QPushButton* b : {_compare, _restore, _open, _delete})
        b->setEnabled(any);
}

} // namespace edita
