// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cambios respecto al último commit de Git, en un margen del editor (no existe en
// Notepad++, cuyo historial de cambios cuenta desde que se abre el archivo): verde las
// líneas añadidas, azul las modificadas y un triángulo rojo donde se borraron líneas.
// La versión de HEAD se pide a git (`git cat-file blob HEAD:./archivo`) en segundo
// plano y se guarda por archivo; las diferencias (core/LineDiff) se calculan con el
// texto en memoria, así que cambian mientras se escribe. Al pasar el ratón por una
// marca, un aviso muestra las líneas originales.

#pragma once

#include "core/Encoding.h"
#include "core/LineDiff.h"

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <memory>
#include <string>
#include <vector>

class QProcess;

namespace edita {

class EditorView;

class GitChanges : public QObject
{
    Q_OBJECT

public:
    // Margen y marcadores (los de N++ para plugins son 1–15, los suyos 18–20)
    static constexpr int kMargin = 4;
    static constexpr int kMarkerAdded = 16;
    static constexpr int kMarkerModified = 17;
    static constexpr int kMarkerDeleted = 0;

    explicit GitChanges(EditorView* view);
    ~GitChanges() override;

    void setEnabled(bool enabled);
    bool isEnabled() const { return _enabled; }

    // El documento que muestra la vista (vacío: sin archivo, sin marcas)
    void setDocument(const QString& path, const TextEncoding& encoding);
    // Volver a pedir la versión de HEAD (al guardar, al volver a la ventana: puede haber
    // un commit nuevo)
    void reload();
    // Colores de las marcas según el tema (claro u oscuro)
    void applyColors();

    const std::vector<LineChange>& changes() const { return _changes; }
    QColor color(LineChangeKind kind) const { return _colors[static_cast<int>(kind)]; }
    // ¿El archivo está en un repositorio y en HEAD? (si no, no hay marcas)
    bool hasBase() const { return _base != nullptr; }
    // Las líneas de HEAD que quitó o cambió un cambio (para el aviso)
    QStringList originalLines(const LineChange& change, int maxLines = 20) const;
    // El cambio que toca esa línea del documento
    const LineChange* changeAt(int line) const;

    // Olvida las versiones guardadas (todas las vistas vuelven a pedirlas)
    static void clearCache();

signals:
    void changed();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void fetch();
    void recompute();
    void applyMarkers();
    void clearMarkers();
    int marginLeft() const;

    QPointer<EditorView> _view;
    bool _enabled = true;
    QString _path;
    TextEncoding _encoding;
    std::shared_ptr<const std::string> _base; // HEAD en UTF-8; nullptr: no hay
    std::vector<LineChange> _changes;
    QColor _colors[3];
    QTimer _debounce;
    QPointer<QProcess> _process;
    quint64 _request = 0;
};

} // namespace edita
