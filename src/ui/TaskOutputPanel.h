// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ejecutar › Salida de la tarea (fase 84): lo que escribe la tarea, con las líneas de error
// marcadas; doble clic en una lleva al archivo y la línea

#pragma once

#include "core/Tasks.h"

#include <QList>
#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QToolButton;

namespace edita {

class TaskOutputPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TaskOutputPanel(QWidget* parent = nullptr);

    void begin(const tasks::Task& task);
    void appendLine(const QString& text);
    // La última línea añadida es este problema
    void markProblem(const tasks::Problem& problem);
    void end(int exitCode, qint64 milliseconds, int errors, int warnings);
    void setRunning(bool running);

    QPlainTextEdit* output() const { return _output; }
    QLabel* status() const { return _status; }
    // La línea `block` (desde 0) es un problema: lo abre como el doble clic
    bool activateLine(int block);

signals:
    void activated(const edita::tasks::Problem& problem);
    void stopRequested();
    void rerunRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QLabel* _status = nullptr;
    QPlainTextEdit* _output = nullptr;
    QToolButton* _stop = nullptr;
    QToolButton* _rerun = nullptr;
    QList<tasks::Problem> _problems; // la línea guarda su índice (userState), que no cambia al recortar
};

} // namespace edita
