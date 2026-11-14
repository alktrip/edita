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
#include <QSplitter>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace edita {

namespace {

constexpr int kRolePath = Qt::UserRole;
constexpr int kRoleParents = Qt::UserRole + 1;
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

    // Dos pestañas (fase 98): los cambios (preparar y confirmar) y el historial
    _tabs = new QTabWidget(this);
    _tabs->setObjectName(QStringLiteral("gitTabs"));
    layout->addWidget(_tabs, 1);
    auto* changesPage = new QWidget(_tabs);
    auto* changesLayout = new QVBoxLayout(changesPage);
    changesLayout->setContentsMargins(0, 4, 0, 0);

    _message = new QPlainTextEdit(this);
    _message->setObjectName(QStringLiteral("gitCommitMessage"));
    _message->setPlaceholderText(tr("Commit message (Ctrl+Enter to commit)"));
    _message->setMaximumHeight(_message->fontMetrics().lineSpacing() * 5 + 12);
    _message->setTabChangesFocus(true);
    _message->installEventFilter(this);
    changesLayout->addWidget(_message);
    auto* commitRow = new QHBoxLayout;
    _amend = new QCheckBox(tr("Amend the last commit"), this);
    _amend->setObjectName(QStringLiteral("gitCommitAmend"));
    commitRow->addWidget(_amend);
    commitRow->addStretch(1);
    _commit = new QPushButton(tr("&Commit"), this);
    _commit->setObjectName(QStringLiteral("gitCommit"));
    commitRow->addWidget(_commit);
    changesLayout->addLayout(commitRow);

    _tree = new QTreeWidget(this);
    _tree->setObjectName(QStringLiteral("gitCommitTree"));
    _tree->setHeaderHidden(true);
    _tree->setColumnCount(2);
    _tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    _tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    _tree->header()->setStretchLastSection(false);
    _tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    _tree->setUniformRowHeights(true);
    changesLayout->addWidget(_tree, 1);

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
    changesLayout->addLayout(buttons);


    _tabs->addTab(changesPage, tr("Changes"));
    // El historial: los commits (del repositorio o del archivo actual) y los archivos de cada uno
    auto* historyPage = new QWidget(_tabs);
    auto* historyLayout = new QVBoxLayout(historyPage);
    historyLayout->setContentsMargins(0, 4, 0, 0);
    auto* historyBar = new QHBoxLayout;
    _historyCurrentFile = new QCheckBox(tr("Only the current file"), historyPage);
    _historyCurrentFile->setObjectName(QStringLiteral("gitHistoryCurrentFile"));
    historyBar->addWidget(_historyCurrentFile);
    historyBar->addStretch(1);
    auto* historyRefresh = new QPushButton(tr("Re&fresh"), historyPage);
    historyRefresh->setObjectName(QStringLiteral("gitHistoryRefresh"));
    historyBar->addWidget(historyRefresh);
    historyLayout->addLayout(historyBar);
    auto* split = new QSplitter(Qt::Vertical, historyPage);
    _history = new QTreeWidget(split);
    _history->setObjectName(QStringLiteral("gitHistoryTree"));
    _history->setRootIsDecorated(false);
    _history->setUniformRowHeights(true);
    _history->setHeaderLabels({tr("Commit"), tr("Author"), tr("Date"), tr("Hash")});
    _history->header()->setStretchLastSection(false);
    _history->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c < 4; ++c)
        _history->header()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    _historyFiles = new QTreeWidget(split);
    _historyFiles->setObjectName(QStringLiteral("gitHistoryFiles"));
    _historyFiles->setRootIsDecorated(false);
    _historyFiles->setHeaderHidden(true);
    _historyFiles->setColumnCount(2);
    _historyFiles->header()->setStretchLastSection(true);
    _historyFiles->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    historyLayout->addWidget(split, 1);
    _historyNote = new QLabel(historyPage);
    _historyNote->setObjectName(QStringLiteral("gitHistoryNote"));
    _historyNote->setWordWrap(true);
    historyLayout->addWidget(_historyNote);
    _tabs->addTab(historyPage, tr("History"));

    connect(_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (index == 1)
            emit historyRequested();
    });
    connect(_historyCurrentFile, &QCheckBox::toggled, this, &GitCommitPanel::historyRequested);
    connect(historyRefresh, &QPushButton::clicked, this, &GitCommitPanel::historyRequested);
    connect(_history, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) {
        _historyFiles->clear();
        if (item)
            emit commitSelected(item->data(0, kRolePath).toString(), item->data(0, kRoleParents).toStringList());
    });
    connect(_history, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item) {
        if (item)
            emit diffRequested(item->data(0, kRolePath).toString(), {});
    });
    connect(_historyFiles, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item) {
        QTreeWidgetItem* commit = _history->currentItem();
        if (item && commit)
            emit diffRequested(commit->data(0, kRolePath).toString(), item->data(0, kRolePath).toStringList());
    });
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


// --- Historial (fase 98) -------------------------------------------------------------------

QList<GitCommitPanel::Commit> GitCommitPanel::parseLog(const QByteArray& output)
{
    // git log --format=%H%x1f%h%x1f%an%x1f%ad%x1f%s%x1f%P%x1e: el asunto puede tener un 0x1f,
    // así que los campos se cuentan desde los dos extremos
    QList<Commit> commits;
    for (const QByteArray& record : output.split('\x1e')) {
        const QByteArray trimmed = record.trimmed();
        if (trimmed.isEmpty())
            continue;
        const QList<QByteArray> f = trimmed.split('\x1f');
        if (f.size() < 6)
            continue;
        Commit c;
        c.hash = QString::fromUtf8(f[0]);
        c.shortHash = QString::fromUtf8(f[1]);
        c.author = QString::fromUtf8(f[2]);
        c.date = QString::fromUtf8(f[3]);
        QList<QByteArray> subject = f.mid(4, f.size() - 5);
        c.subject = QString::fromUtf8(subject.join('\x1f'));
        c.parents = QString::fromUtf8(f.last()).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        commits << c;
    }
    return commits;
}

QList<GitCommitPanel::FileChange> GitCommitPanel::parseNameStatus(const QByteArray& output)
{
    // --name-status -z: «estado\0ruta\0»; los renombrados y copiados, «R100\0vieja\0nueva\0»
    QList<FileChange> changes;
    const QList<QByteArray> parts = output.split('\0');
    for (qsizetype i = 0; i + 1 < parts.size();) {
        const QByteArray status = parts[i];
        if (status.isEmpty())
            break;
        FileChange change;
        change.status = QLatin1Char(status.front());
        if ((change.status == QLatin1Char('R') || change.status == QLatin1Char('C')) && i + 2 < parts.size()) {
            change.oldPath = QString::fromUtf8(parts[i + 1]);
            change.path = QString::fromUtf8(parts[i + 2]);
            i += 3;
        } else {
            change.path = QString::fromUtf8(parts[i + 1]);
            i += 2;
        }
        changes << change;
    }
    return changes;
}

bool GitCommitPanel::historyCurrentFileOnly() const
{
    return _historyCurrentFile->isChecked();
}

bool GitCommitPanel::isHistoryVisible() const
{
    return _tabs->currentIndex() == 1;
}

void GitCommitPanel::setHistory(const QList<Commit>& commits, const QString& note)
{
    const QString selected = _history->currentItem() ? _history->currentItem()->data(0, kRolePath).toString() : QString();
    _history->clear();
    _historyFiles->clear();
    for (const Commit& c : commits) {
        auto* item = new QTreeWidgetItem(_history, {c.subject, c.author, c.date, c.shortHash});
        item->setData(0, kRolePath, c.hash);
        item->setData(0, kRoleParents, c.parents);
        item->setToolTip(0, c.subject);
        item->setToolTip(3, c.hash);
        if (c.hash == selected)
            _history->setCurrentItem(item);
    }
    _historyNote->setText(note);
    _historyNote->setVisible(!note.isEmpty());
}

void GitCommitPanel::setCommitFiles(const QString& hash, const QList<FileChange>& files)
{
    // Puede llegar tarde: solo si el commit sigue elegido
    if (!_history->currentItem() || _history->currentItem()->data(0, kRolePath).toString() != hash)
        return;
    _historyFiles->clear();
    for (const FileChange& f : files) {
        const QString shown = f.oldPath.isEmpty() ? f.path : f.oldPath + QStringLiteral(" → ") + f.path;
        auto* item = new QTreeWidgetItem(_historyFiles, {QString(f.status), shown});
        QStringList paths{f.path};
        if (!f.oldPath.isEmpty())
            paths.prepend(f.oldPath);
        item->setData(0, kRolePath, paths);
        item->setIcon(1, fileicons::enabled() ? fileicons::forFile(f.path) : QIcon());
    }
}

} // namespace edita
