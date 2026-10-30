// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fragmentos de código en el editor (no existen en Notepad++). SnippetLibrary junta los
// que trae Edita (data/snippets/<lenguaje>.json y global.json) con los del usuario
// (~/.config/edita/snippets/, que mandan si se llaman igual). SnippetController, uno por
// vista, los expande al pulsar Tab tras el prefijo y recorre sus marcadores con Tab y
// Mayús+Tab (los espejos, con varias selecciones a la vez); Esc o salir del fragmento
// terminan. El autocompletado de los servidores de lenguaje también lo usa.

#pragma once

#include "core/Snippet.h"

#include <ScintillaEditBase.h>

#include <QMap>
#include <QObject>
#include <QPointer>

#include <vector>

namespace edita {

class EditorView;

class SnippetLibrary : public QObject
{
    Q_OBJECT

public:
    explicit SnippetLibrary(QObject* parent = nullptr);

    void reload();
    // Los de ese lenguaje y los globales (los del usuario, en lugar de los de Edita)
    std::vector<snippet::Definition> forLanguage(const QString& language) const;
    // La clave de un documento: «cpp», «python», «javascript», «markdown»…
    static QString languageKey(int langType, const QString& userLanguage);
    static QString userDirectory();
    // El archivo del usuario para ese lenguaje (con una plantilla si no existe)
    QString ensureUserFile(const QString& language) const;

signals:
    void changed();

private:
    QMap<QString, std::vector<snippet::Definition>> _builtin;
    QMap<QString, std::vector<snippet::Definition>> _user;
};

class SnippetController : public QObject
{
    Q_OBJECT

public:
    SnippetController(EditorView* view, const SnippetLibrary& library);

    void setTabExpansion(bool enabled) { _tabExpansion = enabled; }
    // El documento que muestra la vista (para sus fragmentos y las variables)
    void setContext(const QString& language, const QString& filePath);
    const QString& language() const { return _language; }

    // El prefijo antes del cursor: si es el de un fragmento, lo expande
    bool expandAtCaret();
    // Pone el fragmento en lugar de [start, end) y empieza a recorrer sus marcadores
    void insert(const QString& body, sptr_t start, sptr_t end, const QString& selectedText = {});

    bool isActive() const { return !_stops.empty(); }
    bool next();
    bool previous();
    void stop();
    int currentIndex() const; // el número del marcador actual (0 = final)

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Stop
    {
        int index = 0;
        std::vector<std::pair<sptr_t, sptr_t>> ranges;
    };

    void select(std::size_t stop);
    void track(Scintilla::ModificationFlags type, sptr_t position, sptr_t length);
    void highlight();
    QString indentUnit() const;

    QPointer<EditorView> _view;
    const SnippetLibrary& _library;
    QString _language;
    QString _filePath;
    bool _tabExpansion = true;
    std::vector<Stop> _stops;
    std::size_t _current = 0;
    bool _changing = false;
    bool _savedTyping = false;
};

} // namespace edita
