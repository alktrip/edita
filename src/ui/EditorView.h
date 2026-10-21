// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
//
// Edita is derived from Notepad++ (Copyright (C) Don HO and Notepad++ team).
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "core/Preferences.h"
#include "doc/Buffer.h"

#include <ScintillaEditBase.h>

#include <string>
#include <string_view>

namespace edita {

// Widget de edición: un ScintillaEditBase con la configuración base que
// Notepad++ aplica en ScintillaEditView::init(). Equivale a ScintillaEditView;
// el cambio de documento al cambiar de pestaña lo gestiona EditorPane.
class EditorView : public ScintillaEditBase
{
    Q_OBJECT

public:
    explicit EditorView(QWidget* parent = nullptr);

    // Atajo tipado para mensajes SCI_* con parámetros enteros.
    sptr_t sci(unsigned int message, uptr_t wParam = 0, sptr_t lParam = 0) const
    {
        return send(message, wParam, lParam);
    }

    // Asigna un lexer de Lexilla por nombre ("cpp", "python"…). Devuelve false
    // si Lexilla no conoce ese nombre (como setLexerFromLangID en N++).
    bool setLexerByName(std::string_view lexerName);

    // Texto completo del documento en UTF-8.
    std::string text() const;
    void setText(std::string_view utf8);

    // ScintillaEditView::saveCurrentPos / restoreCurrentPosPreStep
    Position currentPosition() const;
    void restorePosition(const Position& position);

    // ScintillaEditView::updateLineNumberWidth: con ancho dinámico, según los
    // dígitos de la última línea visible (mínimo 3); si no, de la última línea
    // del documento (mínimo 4). Se llama al pintar; EditorPane lo fuerza al
    // cambiar de documento.
    void updateLineNumberWidth(bool force = false);

    // Opciones de las preferencias que son de la vista: ajuste de línea, símbolos,
    // cursor, multiselección, espacio virtual, márgenes… (ScintillaViewParams)
    void applyPreferences(const Preferences& prefs);

    // Notepad_plus::braceMatch: resalta la llave pareja de la que está junto al
    // cursor, o la marca en rojo si no tiene pareja
    void matchBraces();

    bool isWrap() const { return sci(SCI_GETWRAPMODE) != SC_WRAP_NONE; }
    void setWrap(bool wrap) { sci(SCI_SETWRAPMODE, wrap ? SC_WRAP_WORD : SC_WRAP_NONE); }

private:
    void applyBaseConfiguration();

    int _lineNumberDigits = 0;
    bool _lineNumberMargin = true;
    bool _lineNumberDynamicWidth = true;
};

} // namespace edita
