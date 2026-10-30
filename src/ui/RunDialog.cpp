// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/RunDialog.h"

#include "core/UserCommands.h"

#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace edita {

RunDialog::RunDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("runDialog"));
    setWindowTitle(tr("Run…"));
    _combo = new QComboBox(this);
    _combo->setObjectName(QStringLiteral("runCommand"));
    _combo->setEditable(true);
    _combo->setInsertPolicy(QComboBox::NoInsert);
    _combo->setMinimumWidth(460);
    _combo->lineEdit()->setPlaceholderText(QStringLiteral("make -C \"$(CURRENT_DIRECTORY)\""));

    auto* browse = new QToolButton(this);
    browse->setObjectName(QStringLiteral("runBrowse"));
    browse->setText(QStringLiteral("…"));
    browse->setToolTip(tr("Choose the program"));
    connect(browse, &QToolButton::clicked, this, [this] {
        const QString program = QFileDialog::getOpenFileName(this, tr("Program"));
        if (program.isEmpty())
            return;
        _combo->setEditText(program.contains(QLatin1Char(' ')) ? QLatin1Char('"') + program + QLatin1Char('"') : program);
    });
    // IDC_BUTTON_VARIABLES: un menú con las variables, que se insertan en el cursor
    auto* variables = new QToolButton(this);
    variables->setObjectName(QStringLiteral("runVariables"));
    variables->setText(QStringLiteral("$( )"));
    variables->setToolTip(tr("Insert a variable"));
    variables->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu(variables);
    for (const QString& name : runVariableNames()) {
        const QString text = QStringLiteral("$(%1)").arg(name);
        menu->addAction(text, this, [this, text] { _combo->lineEdit()->insert(text); });
    }
    variables->setMenu(menu);

    auto* row = new QHBoxLayout;
    row->addWidget(_combo, 1);
    row->addWidget(browse);
    row->addWidget(variables);

    auto* run = new QPushButton(tr("Run"), this);
    run->setObjectName(QStringLiteral("runButton"));
    run->setDefault(true);
    auto* save = new QPushButton(tr("Save…"), this);
    save->setObjectName(QStringLiteral("runSave"));
    auto* cancel = new QPushButton(tr("Cancel"), this);
    connect(run, &QPushButton::clicked, this, [this] {
        if (!command().isEmpty())
            emit runRequested(command());
    });
    connect(save, &QPushButton::clicked, this, [this] {
        if (!command().isEmpty())
            emit saveRequested(command());
    });
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(run);
    buttons->addWidget(save);
    buttons->addWidget(cancel);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("The program to run:"), this));
    layout->addLayout(row);
    layout->addLayout(buttons);
}

QString RunDialog::command() const
{
    return _combo->currentText().trimmed();
}

void RunDialog::setCommand(const QString& command)
{
    _combo->setEditText(command);
}

void RunDialog::setHistory(const QStringList& history)
{
    const QString current = _combo->currentText();
    _combo->clear();
    _combo->addItems(history);
    _combo->setEditText(current.isEmpty() && !history.isEmpty() ? history.front() : current);
}

} // namespace edita
