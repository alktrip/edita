// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Problemas (no existe en Notepad++): los diagnósticos de los servidores de lenguaje,
// por archivo, con su gravedad, mensaje, origen y posición. Elegir uno lleva a él.

#pragma once

#include "ui/LspManager.h"

#include <QWidget>

class QTreeWidget;

namespace edita {

class ProblemsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ProblemsPanel(QWidget* parent = nullptr);

    void setDiagnostics(const QMap<QString, std::vector<LspManager::Diagnostic>>& diagnostics);
    QTreeWidget* tree() const { return _tree; }
    static QIcon severityIcon(int severity);

signals:
    // Posición de LSP (línea y carácter desde 0)
    void activated(const QString& path, int line, int character);

private:
    QTreeWidget* _tree = nullptr;
};

} // namespace edita
