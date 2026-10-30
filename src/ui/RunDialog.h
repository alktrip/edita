// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ejecutar › Ejecutar… (RunDlg de N++): la orden que se ejecuta, con su historial, un botón
// para elegir el programa, otro con las variables ($(FULL_CURRENT_PATH)…), y Ejecutar,
// Guardar… (con un nombre y un atajo, en el menú) y Cancelar.

#pragma once

#include <QDialog>

class QComboBox;

namespace edita {

class RunDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RunDialog(QWidget* parent = nullptr);

    QString command() const;
    void setCommand(const QString& command);
    // Las órdenes que ya se ejecutaron, la más reciente primero
    void setHistory(const QStringList& history);
    QComboBox* combo() const { return _combo; }

signals:
    void runRequested(const QString& command);
    void saveRequested(const QString& command);

private:
    QComboBox* _combo = nullptr;
};

} // namespace edita
