// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Comparar dos documentos lado a lado (como el plugin Compare de Notepad++): uno en cada
// vista, alineados con líneas en blanco (anotaciones de Scintilla), con el fondo verde en
// lo añadido, rojo en lo quitado y amarillo en lo cambiado, y dentro de cada línea
// cambiada, los caracteres distintos. Las dos vistas se desplazan juntas y se vuelve a
// comparar al editar cualquiera de los dos.

#pragma once

#include "core/LineDiff.h"

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <vector>

class ScintillaEditBase;

namespace edita {

class CompareSession : public QObject
{
    Q_OBJECT

public:
    // Marcadores (fondo de línea) e indicador (caracteres distintos). N++ deja los
    // marcadores 1-14 a los plugins y usa 18-24; Edita usa 0, 15, 16, 17 y 20
    static constexpr int kMarkerAdded = 21;
    static constexpr int kMarkerRemoved = 22;
    static constexpr int kMarkerChanged = 23;
    static constexpr int kIndicatorChars = 0;

    struct Options
    {
        bool ignoreSpaces = false;
        bool ignoreCase = false;
    };
    struct Stats
    {
        int added = 0;   // líneas
        int removed = 0;
        int changed = 0; // líneas del documento nuevo
        int blocks = 0;
        bool identical() const { return blocks == 0; }
    };

    // `oldView` muestra la referencia y `newView` el documento que se compara con ella
    CompareSession(ScintillaEditBase* oldView, ScintillaEditBase* newView, QObject* parent = nullptr);
    ~CompareSession() override;

    void setOptions(const Options& options);
    const Options& options() const { return _options; }
    // Compara ya (si no, se hace un momento después de cada cambio)
    void recompute();
    const std::vector<LineChange>& changes() const { return _changes; }
    Stats stats() const;
    ScintillaEditBase* oldView() const { return _old; }
    ScintillaEditBase* newView() const { return _new; }

    // La diferencia siguiente o anterior al cursor de `view` (una de las dos): pone el cursor
    // en ella en las dos vistas. Devuelve su número (desde 1) o 0 si no hay diferencias
    int goToDifference(ScintillaEditBase* view, bool forward);

    // Quita todo lo que pone la comparación (también de un documento que ya no se muestra)
    static void clearDecorations(ScintillaEditBase& view);
    static QColor color(int marker, bool dark);

signals:
    void updated();

private:
    void configure(ScintillaEditBase& view);
    void syncScroll(ScintillaEditBase* from);

    QPointer<ScintillaEditBase> _old;
    QPointer<ScintillaEditBase> _new;
    Options _options;
    std::vector<LineChange> _changes;
    QTimer _timer;
    bool _syncing = false;
};

} // namespace edita
