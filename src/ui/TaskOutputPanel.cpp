// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/TaskOutputPanel.h"

#include <QEvent>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <QToolButton>
#include <QVBoxLayout>

namespace edita {

TaskOutputPanel::TaskOutputPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("taskOutputPanel"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    auto* top = new QHBoxLayout;
    _status = new QLabel(tr("Run › Run Task… lists the tasks of the project."), this);
    _status->setObjectName(QStringLiteral("taskStatus"));
    top->addWidget(_status, 1);
    auto button = [this, top](const QString& text, const char* name) {
        auto* b = new QToolButton(this);
        b->setText(text);
        b->setObjectName(QString::fromLatin1(name));
        b->setAutoRaise(true);
        top->addWidget(b);
        return b;
    };
    _stop = button(tr("Stop"), "taskStop");
    _rerun = button(tr("Run Again"), "taskRerun");
    QToolButton* clear = button(tr("Clear"), "taskClear");
    layout->addLayout(top);
    _output = new QPlainTextEdit(this);
    _output->setObjectName(QStringLiteral("taskOutput"));
    _output->setReadOnly(true);
    _output->setLineWrapMode(QPlainTextEdit::NoWrap);
    _output->setMaximumBlockCount(50000);
    _output->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    _output->viewport()->installEventFilter(this);
    layout->addWidget(_output, 1);
    connect(_stop, &QToolButton::clicked, this, &TaskOutputPanel::stopRequested);
    connect(_rerun, &QToolButton::clicked, this, &TaskOutputPanel::rerunRequested);
    connect(clear, &QToolButton::clicked, this, [this] {
        _output->clear();
        _problems.clear();
    });
    setRunning(false);
    _rerun->setEnabled(false);
}

void TaskOutputPanel::begin(const tasks::Task& task)
{
    _output->clear();
    _problems.clear();
    _output->appendPlainText(QStringLiteral("$ %1    (%2)").arg(task.command, task.cwd));
    _status->setText(tr("Running: %1").arg(task.label));
    setRunning(true);
    _rerun->setEnabled(true);
}

void TaskOutputPanel::appendLine(const QString& text)
{
    _output->appendPlainText(text);
}

void TaskOutputPanel::markProblem(const tasks::Problem& problem)
{
    QTextBlock block = _output->document()->lastBlock();
    block.setUserState(static_cast<int>(_problems.size()));
    _problems << problem;
    QTextCursor cursor(block);
    cursor.select(QTextCursor::BlockUnderCursor);
    QTextCharFormat format;
    format.setForeground(problem.severity == 1 ? QColor(0xD0, 0x2B, 0x2B) : problem.severity == 2 ? QColor(0xB0, 0x7A, 0x00)
                                                                                                  : palette().color(QPalette::Link));
    format.setFontUnderline(true);
    format.setToolTip(tr("Double click to go to %1:%2").arg(problem.file).arg(problem.line));
    cursor.mergeCharFormat(format);
}

void TaskOutputPanel::end(int exitCode, qint64 milliseconds, int errors, int warnings)
{
    setRunning(false);
    const QString time = QLocale().toString(static_cast<double>(milliseconds) / 1000.0, 'f', 1);
    QString text;
    if (exitCode < 0)
        text = tr("Stopped after %1 s").arg(time);
    else if (exitCode == 0)
        text = tr("Finished in %1 s").arg(time);
    else
        text = tr("Failed with exit code %1 after %2 s").arg(exitCode).arg(time);
    if (errors > 0 || warnings > 0)
        text += QStringLiteral(" · ") + tr("%n error(s)", nullptr, errors) + QStringLiteral(", ") + tr("%n warning(s)", nullptr, warnings);
    _status->setText(text);
    _output->appendPlainText(QString());
    _output->appendPlainText(QStringLiteral("— ") + text);
}

void TaskOutputPanel::setRunning(bool running)
{
    _stop->setEnabled(running);
}

bool TaskOutputPanel::activateLine(int block)
{
    const int index = _output->document()->findBlockByNumber(block).userState();
    if (index < 0 || index >= _problems.size())
        return false;
    emit activated(_problems[index]);
    return true;
}

bool TaskOutputPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _output->viewport() && event->type() == QEvent::MouseButtonDblClick) {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        if (activateLine(_output->cursorForPosition(mouse->position().toPoint()).blockNumber()))
            return true;
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace edita
