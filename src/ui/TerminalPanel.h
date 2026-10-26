// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// El panel de la terminal integrada: una pestaña por terminal, con «+» para abrir otra.
// Cada una empieza en la carpeta que se le pide (la del documento, la de trabajo…);
// al salir de la shell se cierra su pestaña.

#pragma once

#include "ui/Terminal.h"

#include <QWidget>

class QTabWidget;

namespace edita {

class TerminalPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalPanel(QWidget* parent = nullptr);

    // La shell: vacío = $SHELL (o /bin/sh)
    void setShell(const QString& shell) { _shell = shell; }
    static QString resolveShell(const QString& preference);

    TerminalView* newTerminal(const QString& directory);
    TerminalView* currentTerminal() const;
    int count() const;
    TerminalView* terminalAt(int index) const;
    void closeTerminal(TerminalView* view);

    void setTerminalFont(const QFont& font);
    void setColors(const QColor& foreground, const QColor& background);

signals:
    void newTerminalRequested();
    void lastTerminalClosed();

private:
    void updateTabTitle(TerminalView* view);

    QTabWidget* _tabs = nullptr;
    QString _shell;
    QFont _font;
    bool _hasFont = false;
    TerminalView::Colors _colors;
    bool _hasColors = false;
};

} // namespace edita
