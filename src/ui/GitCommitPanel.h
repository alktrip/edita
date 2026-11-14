// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ver › Git (fase 85; no existe en N++): los archivos cambiados del repositorio, lo que está
// en el índice («stage») y el mensaje para confirmar (commit)

#pragma once

#include <QList>
#include <QString>
#include <QWidget>

class QCheckBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace edita {

class GitCommitPanel : public QWidget
{
    Q_OBJECT

public:
    struct Entry
    {
        QString path; // relativa a la raíz del repositorio
        QChar index;  // X de «git status --porcelain»: ' ' sin cambios en el índice, '?' sin seguimiento
        QChar worktree; // Y
    };
    // `git status --porcelain=v1 -z`
    static QList<Entry> parseStatus(const QByteArray& output);

    // El historial (fase 98)
    struct Commit
    {
        QString hash;
        QString shortHash;
        QString author;
        QString date; // AAAA-MM-DD
        QString subject;
        QStringList parents;
    };
    struct FileChange
    {
        QChar status;    // A, M, D, R, C, T
        QString path;
        QString oldPath; // renombrados y copiados
    };
    // `git log --format=%H%x1f%h%x1f%an%x1f%ad%x1f%s%x1f%P%x1e --date=short`
    static QList<Commit> parseLog(const QByteArray& output);
    // `git diff --name-status -z -M`
    static QList<FileChange> parseNameStatus(const QByteArray& output);

    explicit GitCommitPanel(QWidget* parent = nullptr);

    // `top` vacío: el documento no está en un repositorio
    void setRepository(const QString& top, const QString& branch, const QList<Entry>& entries);
    const QString& repository() const { return _top; }
    QTreeWidget* tree() const { return _tree; }
    QPlainTextEdit* message() const { return _message; }
    QLabel* header() const { return _header; }
    void clearMessage();
    void setHistory(const QList<Commit>& commits, const QString& note = QString());
    void setCommitFiles(const QString& hash, const QList<FileChange>& files);
    bool historyCurrentFileOnly() const;
    bool isHistoryVisible() const;

signals:
    void stageRequested(const QStringList& paths);
    void unstageRequested(const QStringList& paths);
    void commitRequested(const QString& message, bool amend);
    void refreshRequested();
    void openRequested(const QString& path);
    // El historial: hay que (volver a) leerlo; los archivos de un commit; su diff (paths vacío:
    // el commit entero)
    void historyRequested();
    void commitSelected(const QString& hash, const QStringList& parents);
    void diffRequested(const QString& hash, const QStringList& paths);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QStringList selectedPaths(bool staged) const;
    void updateButtons();

    QString _top;
    QLabel* _header = nullptr;
    QTreeWidget* _tree = nullptr;
    QTreeWidgetItem* _stagedGroup = nullptr;
    QTreeWidgetItem* _changesGroup = nullptr;
    QPushButton* _stage = nullptr;
    QPushButton* _unstage = nullptr;
    QPushButton* _stageAll = nullptr;
    QPlainTextEdit* _message = nullptr;
    QCheckBox* _amend = nullptr;
    QPushButton* _commit = nullptr;
    QTabWidget* _tabs = nullptr;
    QCheckBox* _historyCurrentFile = nullptr;
    QTreeWidget* _history = nullptr;
    QTreeWidget* _historyFiles = nullptr;
    QLabel* _historyNote = nullptr;
};

} // namespace edita
