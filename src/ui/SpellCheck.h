// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// El corrector ortográfico en el editor: las palabras mal escritas de las líneas visibles,
// subrayadas con una onda roja (como DSpellCheck en N++)

#pragma once

#include <ScintillaEditBase.h>

#include <QString>

#include <array>

namespace edita {

class EditorView;
class SpellChecker;
struct LexerStyles;

// Uno de los indicadores de los plugins de N++ (9-20): Edita les deja del 9 al 19
inline constexpr int kIndicatorSpelling = 20;

// Qué estilos se revisan: todos (texto normal) o los de comentario y de cadena, según el lexer
// (SCI_TAGSOFSTYLE: «comment», «string») o los nombres de stylers.xml (COMMENT, STRING…). Sin
// lexer, todo
std::array<bool, 256> spellCheckedStyles(EditorView& view, const LexerStyles* styles, bool onlyCommentsAndStrings);

// Marca (o borra) las palabras mal escritas de las líneas visibles
void highlightMisspellings(EditorView& view, const SpellChecker& checker, const std::array<bool, 256>& styles, bool enabled);

// La palabra subrayada en esa posición (o justo antes); vacía si no hay. `start` y `end`, sus
// límites en el documento
QString misspelledWordAt(EditorView& view, sptr_t position, sptr_t* start = nullptr, sptr_t* end = nullptr);

} // namespace edita
