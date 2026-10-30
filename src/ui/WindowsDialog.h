// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ventana › Ventanas… (WindowsDlg de N++): los documentos de la vista actual en una lista
// con nombre, ruta, tipo y tamaño, que se ordena pulsando las columnas; con varios elegidos
// se pueden activar, guardar o cerrar, y «Ordenar las pestañas» las deja en el orden de la
// lista.

#pragma once

#include "doc/Buffer.h"

#include <QDialog>

#include <vector>

class QTreeWidget;

namespace edita {

class WindowsDialog : public QDialog
{
    Q_OBJECT

public:
    struct Row
    {
        BufferId id = kInvalidBuffer;
        QString name;
        QString path;
        QString type;
        qint64 size = 0;
        bool modified = false;
    };

    explicit WindowsDialog(QWidget* parent = nullptr);

    void setRows(const std::vector<Row>& rows, BufferId current);
    // En el orden en que se ven (el de la columna elegida)
    std::vector<BufferId> listedOrder() const;
    std::vector<BufferId> selected() const;
    QTreeWidget* list() const { return _list; }

signals:
    void activateRequested(edita::BufferId id);
    void saveRequested(const std::vector<edita::BufferId>& ids);
    void closeRequested(const std::vector<edita::BufferId>& ids);
    void sortTabsRequested(const std::vector<edita::BufferId>& order);

private:
    QTreeWidget* _list = nullptr;
};

} // namespace edita
