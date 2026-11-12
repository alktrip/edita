// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ejecuta una tarea del proyecto (fase 84) con la shell, línea a línea, y reconoce los errores
// de la salida (core/Tasks)

#pragma once

#include "core/Tasks.h"

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>

#include <memory>

namespace edita {

class TaskRunner : public QObject
{
    Q_OBJECT

public:
    explicit TaskRunner(QObject* parent = nullptr);
    ~TaskRunner() override;

    // Si ya hay una en marcha, la detiene primero
    void start(const tasks::Task& task);
    // Termina la tarea y todo lo que lanzó (su grupo de procesos)
    void stop();
    bool isRunning() const;
    const tasks::Task& task() const { return _task; }

signals:
    void started(const edita::tasks::Task& task);
    void line(const QString& text);
    void problem(const edita::tasks::Problem& problem);
    // `exitCode` -1: no se pudo ejecutar o se detuvo
    void finished(int exitCode, qint64 milliseconds);

private:
    void readOutput();
    void emitLine(QString text);
    void finish(int exitCode);

    std::unique_ptr<QProcess> _process;
    std::unique_ptr<tasks::ProblemParser> _parser;
    tasks::Task _task;
    QByteArray _pending;
    QElapsedTimer _clock;
    bool _stopped = false;
};

} // namespace edita
