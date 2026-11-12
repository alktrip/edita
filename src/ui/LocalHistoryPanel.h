// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Archivo › Historial local (fase 83; no existe en N++): las versiones guardadas del
// documento actual, para compararlas, abrirlas, volver a una o borrarlas

#pragma once

#include "core/LocalHistory.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QTreeWidget;

namespace edita {

class LocalHistoryPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LocalHistoryPanel(QWidget* parent = nullptr);

    // `path` vacío: el documento no tiene archivo. `enabled`: si el historial está encendido
    void setFile(const QString& path, const QList<LocalHistory::Version>& versions, bool enabled);
    const QString& file() const { return _path; }
    QTreeWidget* tree() const { return _tree; }

    // «Hoy 14:32:05», «Ayer 09:10:00» o la fecha corta del idioma
    static QString describe(const QDateTime& when, const QDateTime& now);

signals:
    void compareRequested(const LocalHistory::Version& version);
    void restoreRequested(const LocalHistory::Version& version);
    void openRequested(const LocalHistory::Version& version);
    void deleteRequested(const LocalHistory::Version& version);

private:
    bool selected(LocalHistory::Version& version) const;
    void updateButtons();

    QString _path;
    QList<LocalHistory::Version> _versions;
    QLabel* _label = nullptr;
    QTreeWidget* _tree = nullptr;
    QPushButton* _compare = nullptr;
    QPushButton* _restore = nullptr;
    QPushButton* _open = nullptr;
    QPushButton* _delete = nullptr;
};

} // namespace edita
