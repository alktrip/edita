// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ver › TODO (fase 96): los comentarios con TODO, FIXME, HACK, XXX, BUG o NOTE del documento
// actual o de las carpetas de trabajo, por archivo. Doble clic o Intro van a la línea. El panel
// solo muestra: busca la ventana (MainWindow), que sabe qué documentos y carpetas hay.

#pragma once

#include "core/TodoScan.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QToolButton;
class QTreeWidget;

namespace edita {

class TodoPanel : public QWidget
{
    Q_OBJECT

public:
    enum class Scope { Document, Workspace };

    explicit TodoPanel(QWidget* parent = nullptr);

    Scope scope() const;
    void setScope(Scope scope);
    // Los elementos, con las rutas relativas a root (vacío: solo el nombre del archivo)
    void setItems(const std::vector<todo::Item>& items, const QString& root);
    void setBusy(bool busy);
    QTreeWidget* tree() const { return _tree; }

signals:
    void scopeChanged(edita::TodoPanel::Scope scope);
    void refreshRequested();
    void activated(const QString& path, int line, int column);

private:
    QComboBox* _scope = nullptr;
    QToolButton* _refresh = nullptr;
    QLabel* _status = nullptr;
    QTreeWidget* _tree = nullptr;
};

} // namespace edita
