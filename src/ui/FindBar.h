// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Buscar en línea (no existe en Notepad++, que abre el diálogo Buscar): una barra
// flotante arriba a la derecha del editor que resalta todas las coincidencias mientras
// se escribe, va a la más cercana y dice «3 de 17». Mayúsculas, palabra completa y
// expresión regular (Alt+C, Alt+W, Alt+R); Intro siguiente, Mayús+Intro anterior, Esc
// cierra. Con Reemplazar, una o todas. «⋯» abre el diálogo completo de N++ con el mismo
// texto. Busca con el mismo motor que el diálogo (search/SearchEngine).

#pragma once

#include "search/SearchOptions.h"

#include <QFrame>
#include <QPointer>
#include <QTimer>

#include <cstdint>

class QLabel;
class QLineEdit;
class QToolButton;

namespace edita {

class EditorView;

class FindBar : public QFrame
{
    Q_OBJECT

public:
    explicit FindBar(EditorView* view, QWidget* parent);

    // Abre la barra (con la fila de reemplazar o sin ella) y escribe `text` si no está vacío
    void open(bool replace, const QString& text);
    void close();
    bool isOpen() const { return !isHidden(); }

    void findNext(bool backward);
    void replaceOne();
    int replaceAll();
    SearchOptions options() const;

    // Vuelve a contar y resaltar (el texto cambió o se cambió de documento)
    void refresh();

    // Para las pruebas
    QLineEdit* findField() const { return _find; }
    QLineEdit* replaceField() const { return _replace; }
    QString countText() const;
    void setToggles(bool matchCase, bool wholeWord, bool regex);

signals:
    // Lo último que se buscó (para F3 / Mayús+F3)
    void searched(const edita::SearchOptions& options);
    void moreOptionsRequested(bool replace, const QString& text);
    void closed();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void reposition();
    void highlightAll(bool moveToNearest);
    void clearHighlights();
    void updateCount();

    QPointer<EditorView> _view;
    QLineEdit* _find = nullptr;
    QLineEdit* _replace = nullptr;
    QWidget* _replaceRow = nullptr;
    QToolButton* _expand = nullptr;
    QToolButton* _matchCase = nullptr;
    QToolButton* _wholeWord = nullptr;
    QToolButton* _regex = nullptr;
    QLabel* _count = nullptr;
    QTimer _timer;
    intptr_t _anchor = 0; // dónde estaba el cursor al abrir (la búsqueda mientras se escribe empieza ahí)
    int _total = 0;
    bool _invalid = false;
};

} // namespace edita
