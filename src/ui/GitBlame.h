// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Git blame (no existe en Notepad++): quién cambió cada línea y cuándo. En la línea del
// cursor, al final y en gris («Ana López, hace 3 días • mensaje»); y, a petición, un margen
// con el commit, el autor y la fecha de cada bloque, con el color según la antigüedad y los
// detalles del commit al pasar el ratón. Se le pasa a git el texto del editor
// (--contents -), así que las líneas cuadran aunque haya cambios sin guardar.

#pragma once

#include "core/Encoding.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <vector>

class QProcess;

namespace edita {

class EditorView;

class GitBlame : public QObject
{
    Q_OBJECT

public:
    static constexpr unsigned kMargin = 5; // los de N++ son 0-3; el 4, los cambios de Git

    struct Commit
    {
        QString hash;
        QString author;
        QString mail;
        QDateTime time;
        QString summary;
        bool uncommitted = false;
    };

    explicit GitBlame(EditorView* view);
    ~GitBlame() override;

    void setInlineEnabled(bool enabled);
    void setMarginVisible(bool visible);
    bool isMarginVisible() const { return _marginVisible; }
    // El documento que muestra la vista (vacío: sin archivo, sin blame)
    void setDocument(const QString& path, const TextEncoding& encoding);
    void reload();

    bool hasData() const { return !_lines.empty(); }
    const Commit* commitAt(int line) const;
    // El texto que va al final de la línea del cursor
    QString inlineText(int line) const;
    QString details(const Commit& commit) const;

    // git blame --porcelain: el hash de cada línea y los datos de cada commit
    static std::vector<QString> parsePorcelain(const QByteArray& output, QHash<QString, Commit>& commits);
    static QString relativeTime(const QDateTime& when, const QDateTime& now);

signals:
    void updated();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void run();
    void apply();
    void updateInline();
    void updateMargin();
    void configureStyles();
    int marginLeft() const;

    QPointer<EditorView> _view;
    QString _path;
    TextEncoding _encoding;
    bool _inline = true;
    bool _marginVisible = false;
    std::vector<QString> _lines; // el hash de cada línea
    QHash<QString, Commit> _commits;
    QPointer<QProcess> _process;
    QTimer _timer;
    int _inlineLine = -1;
    quint64 _request = 0;
};

} // namespace edita
