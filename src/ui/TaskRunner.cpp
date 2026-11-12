// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/TaskRunner.h"

#include <QProcessEnvironment>

#include <csignal>
#include <unistd.h>

namespace edita {

TaskRunner::TaskRunner(QObject* parent)
    : QObject(parent)
{
}

TaskRunner::~TaskRunner()
{
    if (_process) {
        _process->disconnect(this);
        stop();
    }
}

bool TaskRunner::isRunning() const
{
    return _process && _process->state() != QProcess::NotRunning;
}

void TaskRunner::start(const tasks::Task& task)
{
    if (isRunning()) {
        _process->disconnect(this);
        stop();
    }
    _task = task;
    _pending.clear();
    _stopped = false;
    _parser = std::make_unique<tasks::ProblemParser>(task.cwd);
    _process = std::make_unique<QProcess>();
    _process->setProcessChannelMode(QProcess::MergedChannels);
    _process->setWorkingDirectory(task.cwd);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("TERM"), QStringLiteral("dumb")); // sin barras de progreso ni colores
    env.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));
    _process->setProcessEnvironment(env);
    // Un grupo de procesos propio: Detener termina también lo que lance (make → gcc)
    _process->setChildProcessModifier([] { ::setsid(); });
    connect(_process.get(), &QProcess::readyRead, this, &TaskRunner::readOutput);
    connect(_process.get(), &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        readOutput();
        finish(_stopped || status != QProcess::NormalExit ? -1 : code);
    });
    connect(_process.get(), &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            emitLine(tr("Could not run the shell (/bin/sh)."));
            finish(-1);
        }
    });
    _clock.start();
    emit started(_task);
    _process->start(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), task.command});
}

void TaskRunner::stop()
{
    if (!isRunning())
        return;
    _stopped = true;
    const qint64 pid = _process->processId();
    if (pid > 0)
        ::kill(-static_cast<pid_t>(pid), SIGTERM);
    if (!_process->waitForFinished(2000)) {
        if (pid > 0)
            ::kill(-static_cast<pid_t>(pid), SIGKILL);
        _process->waitForFinished(1000);
    }
}

void TaskRunner::readOutput()
{
    if (!_process)
        return;
    _pending += _process->readAll();
    qsizetype newline;
    while ((newline = _pending.indexOf('\n')) >= 0) {
        QByteArray bytes = _pending.left(newline);
        _pending.remove(0, newline + 1);
        if (bytes.endsWith('\r'))
            bytes.chop(1);
        // Lo que se reescribe con \r (progreso): solo lo último
        if (const qsizetype cr = bytes.lastIndexOf('\r'); cr >= 0)
            bytes = bytes.mid(cr + 1);
        emitLine(QString::fromUtf8(bytes));
    }
}

void TaskRunner::emitLine(QString text)
{
    text = tasks::stripAnsi(text);
    emit line(text);
    if (_parser) {
        if (const auto p = _parser->feed(text))
            emit problem(*p);
    }
}

void TaskRunner::finish(int exitCode)
{
    if (!_pending.isEmpty()) {
        emitLine(QString::fromUtf8(_pending));
        _pending.clear();
    }
    const qint64 ms = _clock.isValid() ? _clock.elapsed() : 0;
    emit finished(exitCode, ms);
}

} // namespace edita
