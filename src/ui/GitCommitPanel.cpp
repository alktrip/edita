// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/GitCommitPanel.h"

#include "ui/FileIcons.h"

#include <QCheckBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace edita {

namespace {

constexpr int kRolePath = Qt::UserRole;
constexpr int kRoleStaged = Qt::UserRole + 1;

QString describe(QChar code)
{
    switch (code.unicode()) {
    case 'M':
        return GitCommitPanel::tr("modified");
    case 'A':
        return GitCommitPanel::tr("added");
    case 'D':
        return GitCommitPanel::tr("deleted");
    case 'R':
        return GitCommitPanel::tr("renamed");
    case 'C':
        return GitCommitPanel::tr("copied");
    case 'U':
        return GitCommitPanel::tr("conflict");
    case '?':
        return GitCommitPanel::tr("untracked");
    default:
        return {};
    }
}

} // namespace

QList<GitCommitPanel::Entry> GitCommitPanel::parseStatus(const QByteArray& output)
{
    QList<Entry> entries;
    const QList<QByteArray> parts = output.split('\0');
    for (qsizetype i = 0; i < parts.size(); ++i) {
        const QByteArray& part = parts[i];
        if (part.size() < 4)
            continue;
        Entry e{QString::fromUtf8(part.mid(3)), QLatin1Char(part[0]), QLatin1Char(part[1])};
        if (e.index == QLatin1Char('!'))
            continue;
        entries << e;
        // Renombrado o copiado: la ruta de antes va en la parte siguiente
        if (e.index == QLatin1Char('R') || e.index == QLatin1Char('C'))
            ++i;
    }
    return entries;
}

GitCommitPanel::GitCommitPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("gitCommitPanel"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    _header = new QLabel(this);
    _header->setObjectName(QStringLiteral("gitCommitHeader"));
    _header->setWordWrap(true);
    layout->addWidget(_header);

    _message = new QPlainTextEdit(this);
    _message->setObjectName(QStringLiteral("gitCommitMessage"));
    _message->setPlaceholderText(tr("Commit message (Ctrl+Enter to commit)"));
    _message->setMaximumHeight(_message->fontMetrics().lineSpacing() * 5 + 12);
    _message->setTabChangesFocus(true);
    _message->installEventFilter(this);
    layout->addWidget(_message);
    auto* commitRow = new QHBoxLayout;
    _amend = new QCheckBox(tr("Amend the last commit"), this);
    _amend->setObjectName(QStringLiteral("gitCommitAmend"));
    commitRow->addWidget(_amend);
    commitRow->addStretch(1);
    _commit = new QPushButton(tr("&Commit"), this);
    _commit->setObjectName(QStringLiteral("gitCommit"));
    commitRow->addWidget(_commit);
    layout->addLayout(commitRow);

    _tree = new QTreeWidget(this);
    _tree->setObjectName(QStringLiteral("gitCommitTree"));
    _tree->setHeaderHidden(true);
    _tree->setColumnCount(2);
    _tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    _tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    _tree->header()->setStretchLastSection(false);
    _tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    _tree->setUniformRowHeights(true);
    layout->addWidget(_tree, 1);

    auto* buttons = new QHBoxLayout;
    auto button = [this, buttons](const QString& text, const char* name) {
        auto* b = new QPushButton(text, this);
        b->setObjectName(QString::fromLatin1(name));
        buttons->addWidget(b);
        return b;
    };
    _stage = button(tr("&Stage"), "gitStage");
    _unstage = button(tr("&Unstage"), "gitUnstage");
    _stageAll = button(tr("Stage &All"), "gitStageAll");
    buttons->addStretch(1);
    QPushButton* refresh = button(tr("&Refresh"), "gitRefresh");
    layout->addLayout(buttons);

    connect(_stage, &QPushButton::clicked, this, [this] { emit stageRequested(selectedPaths(false)); });
    connect(_unstage, &QPushButton::clicked, this, [this] { emit unstageRequested(selectedPaths(true)); });
    connect(_stageAll, &QPushButton::clicked, this, [this] {
        QStringList all;
        for (int i = 0; i < _changesGroup->childCount(); ++i)
            all << _changesGroup->child(i)->data(0, kRolePath).toString();
        emit stageRequested(all);
    });
    connect(refresh, &QPushButton::clicked, this, &GitCommitPanel::refreshRequested);
    connect(_commit, &QPushButton::clicked, this, [this] { emit commitRequested(_message->toPlainText(), _amend->isChecked()); });
    connect(_amend, &QCheckBox::toggled, this, &GitCommitPanel::updateButtons);
    connect(_message, &QPlainTextEdit::textChanged, this, &GitCommitPanel::updateButtons);
    connect(_tree, &QTreeWidget::itemSelectionChanged, this, &GitCommitPanel::updateButtons);
    connect(_tree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item) {
        if (item && item->parent())
            emit openRequested(item->data(0, kRolePath).toString());
    });

    _stagedGroup = new QTreeWidgetItem(_tree);
    _changesGroup = new QTreeWidgetItem(_tree);
    for (QTreeWidgetItem* group : {_stagedGroup, _changesGroup}) {
        QFont bold = group->font(0);
        bold.setBold(true);
        group->setFont(0, bold);
        group->setFlags(Qt::ItemIsEnabled);
        group->setExpanded(true);
    }
    setRepository({}, {}, {});
}

void GitCommitPanel::setRepository(const QString& top, const QString& branch, const QList<Entry>& entries)
{
    _top = top;
    qDeleteAll(_stagedGroup->takeChildren());
    qDeleteAll(_changesGroup->takeChildren());
    if (top.isEmpty()) {
        _header->setText(tr("The current document is not in a Git repository."));
    } else {
        _header->setText(branch.isEmpty() ? QDir::toNativeSeparators(top)
                                          : tr("%1 — branch %2").arg(QDir::toNativeSeparators(top), branch));
    }
    for (const Entry& e : entries) {
        auto add = [&](QTreeWidgetItem* group, QChar code, bool staged) {
            auto* item = new QTreeWidgetItem(group);
            const QFileInfo info(e.path);
            item->setText(0, info.path() == QLatin1String(".") ? info.fileName()
                                                                : QStringLiteral("%1  %2").arg(info.fileName(), info.path()));
            item->setText(1, describe(code));
            item->setIcon(0, fileicons::forFile(e.path));
            item->setToolTip(0, e.path);
            item->setData(0, kRolePath, e.path);
            item->setData(0, kRoleStaged, staged);
        };
        if (e.index != QLatin1Char(' ') && e.index != QLatin1Char('?'))
            add(_stagedGroup, e.index, true);
        if (e.worktree != QLatin1Char(' '))
            add(_changesGroup, e.worktree, false);
    }
    _stagedGroup->setText(0, tr("Staged (%1)").arg(_stagedGroup->childCount()));
    _changesGroup->setText(0, tr("Changes (%1)").arg(_changesGroup->childCount()));
    _stagedGroup->setExpanded(true);
    _changesGroup->setExpanded(true);
    updateButtons();
}

void GitCommitPanel::clearMessage()
{
    _message->clear();
    _amend->setChecked(false);
}

QStringList GitCommitPanel::selectedPaths(bool staged) const
{
    QStringList paths;
    for (const QTreeWidgetItem* item : _tree->selectedItems()) {
        if (item->parent() && item->data(0, kRoleStaged).toBool() == staged)
            paths << item->data(0, kRolePath).toString();
    }
    return paths;
}

void GitCommitPanel::updateButtons()
{
    const bool repo = !_top.isEmpty();
    _stage->setEnabled(repo && !selectedPaths(false).isEmpty());
    _unstage->setEnabled(repo && !selectedPaths(true).isEmpty());
    _stageAll->setEnabled(repo && _changesGroup->childCount() > 0);
    _commit->setEnabled(repo && !_message->toPlainText().trimmed().isEmpty()
                        && (_stagedGroup->childCount() > 0 || _amend->isChecked()));
}

bool GitCommitPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _message && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && (key->modifiers() & Qt::ControlModifier)) {
            if (_commit->isEnabled())
                _commit->click();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace edita
