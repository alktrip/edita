// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// El estado de Git de los archivos de las carpetas de trabajo (no existe en Notepad++):
// modificados, añadidos, sin seguimiento, en conflicto e ignorados, para colorearlos en
// el explorador de carpetas. Las carpetas toman el estado más importante de lo que tienen
// dentro. Se lee con «git status --porcelain» en segundo plano.

#pragma once

#include <QColor>
#include <QHash>
#include <QObject>
#include <QSet>

#include <utility>
#include <vector>

class QProcess;

namespace edita {

class GitStatus : public QObject
{
    Q_OBJECT

public:
    // En orden de importancia (una carpeta muestra el mayor de lo que contiene)
    enum class State { None, Ignored, Untracked, Added, Renamed, Deleted, Modified, Conflict };

    explicit GitStatus(QObject* parent = nullptr);
    ~GitStatus() override;

    // Vuelve a leer el estado de los repositorios de esas carpetas (avisa con updated)
    void refresh(const QStringList& roots);
    State stateOf(const QString& path) const;
    // La rama del repositorio de una carpeta de trabajo (vacía si no es un repositorio)
    QString branchOf(const QString& root) const { return _branches.value(root); }
    bool isBusy() const { return _pending > 0; }

    static QString badge(State state);
    static QString description(State state);
    static QColor color(State state, bool dark);
    // «XY ruta\0…» de git status --porcelain=v1 -z, con rutas absolutas (para las pruebas)
    static std::vector<std::pair<QString, State>> parsePorcelain(const QByteArray& output, const QString& top);

signals:
    void updated();

private:
    void finishRoot(quint64 generation, const QString& root, const QString& top, const QString& branch, const QByteArray& status);

    QHash<QString, State> _files;
    QHash<QString, State> _dirs;
    QSet<QString> _ignoredDirs;
    QHash<QString, QString> _branches;
    // Lo que se va juntando mientras responden los repositorios
    QHash<QString, State> _nextFiles;
    QHash<QString, State> _nextDirs;
    QSet<QString> _nextIgnored;
    QHash<QString, QString> _nextBranches;
    quint64 _generation = 0;
    int _pending = 0;
};

} // namespace edita
