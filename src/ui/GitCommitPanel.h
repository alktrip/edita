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

    explicit GitCommitPanel(QWidget* parent = nullptr);

    // `top` vacío: el documento no está en un repositorio
    void setRepository(const QString& top, const QString& branch, const QList<Entry>& entries);
    const QString& repository() const { return _top; }
    QTreeWidget* tree() const { return _tree; }
    QPlainTextEdit* message() const { return _message; }
    QLabel* header() const { return _header; }
    void clearMessage();

signals:
    void stageRequested(const QStringList& paths);
    void unstageRequested(const QStringList& paths);
    void commitRequested(const QString& message, bool amend);
    void refreshRequested();
    void openRequested(const QString& path);

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
};

} // namespace edita
