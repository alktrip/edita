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

#include "ui/EditorView.h"

#include "syntax/SyntaxStyler.h"
#include "ui/EditCommands.h"

#include <ILexer.h>
#include <Lexilla.h>

#include <QFontDatabase>

#include <algorithm>
#include <string>

namespace edita {

EditorView::EditorView(QWidget* parent)
    : ScintillaEditBase(parent)
{
    applyBaseConfiguration();
}

void EditorView::applyBaseConfiguration()
{
    // Notepad++ trabaja siempre en UTF-8 dentro de Scintilla y convierte al
    // cargar y guardar (Buffer.cpp, copyFileContentToScintilla / saveBuffer).
    sci(SCI_SETCODEPAGE, SC_CP_UTF8);

    // Igual que ScintillaEditView::init()
    sci(SCI_SETIDLESTYLING, SC_IDLESTYLING_ALL);
    sci(SCI_SETSCROLLWIDTHTRACKING, 1);
    sci(SCI_SETSCROLLWIDTH, 1);
    sci(SCI_SETFOLDFLAGS, SC_FOLDFLAG_LINEAFTER_CONTRACTED);
    sci(SCI_SETMARGINMASKN, kMarginFolder, static_cast<sptr_t>(SC_MASK_FOLDERS));
    sci(SCI_SETMARGINMASKN, kMarginSymbol, 1 << kBookmarkMarker);
    sci(SCI_SETMARGINMASKN, kMarginChangeHistory,
        (1 << SC_MARKNUM_HISTORY_REVERTED_TO_ORIGIN) | (1 << SC_MARKNUM_HISTORY_SAVED) | (1 << SC_MARKNUM_HISTORY_MODIFIED)
            | (1 << SC_MARKNUM_HISTORY_REVERTED_TO_MODIFIED));
    sci(SCI_SETMARGINSENSITIVEN, kMarginFolder, 1);
    sci(SCI_SETMARGINSENSITIVEN, kMarginSymbol, 1);
    sci(SCI_MARKERSETALPHA, kBookmarkMarker, 70);
    // Plegar y desplegar con clic en el margen, y mostrar lo oculto al editar
    sci(SCI_SETAUTOMATICFOLD, SC_AUTOMATICFOLD_SHOW | SC_AUTOMATICFOLD_CLICK | SC_AUTOMATICFOLD_CHANGE);

    // Fuente monoespaciada del sistema hasta que la Fase 3 aplique stylers.xml
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    const QByteArray family = mono.family().toUtf8();
    sends(SCI_STYLESETFONT, STYLE_DEFAULT, family.constData());
    sci(SCI_STYLESETSIZE, STYLE_DEFAULT, mono.pointSize() > 0 ? mono.pointSize() : 10);
    sci(SCI_STYLECLEARALL);

    sci(SCI_SETMARGINTYPEN, kMarginLineNumber, SC_MARGIN_NUMBER);
    updateLineNumberWidth();
    connect(this, &ScintillaEditBase::linesAdded, this, [this] { updateLineNumberWidth(); });
    connect(this, &ScintillaEditBase::updateUi, this, [this](Scintilla::Update updated) {
        if (static_cast<int>(updated) & (SC_UPDATE_SELECTION | SC_UPDATE_CONTENT))
            matchBraces();
        updateLineNumberWidth();
    });
    applyPreferences(Preferences{});
}

void EditorView::matchBraces()
{
    auto isBrace = [](sptr_t c) { return c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}'; };
    const sptr_t caret = sci(SCI_GETCURRENTPOS);
    sptr_t brace = -1;
    if (caret > 0 && isBrace(sci(SCI_GETCHARAT, static_cast<uptr_t>(caret - 1))))
        brace = caret - 1;
    else if (isBrace(sci(SCI_GETCHARAT, static_cast<uptr_t>(caret))))
        brace = caret;

    if (brace < 0) {
        sci(SCI_BRACEHIGHLIGHT, static_cast<uptr_t>(-1), -1);
        sci(SCI_SETHIGHLIGHTGUIDE, 0);
        return;
    }
    const sptr_t opposite = sci(SCI_BRACEMATCH, static_cast<uptr_t>(brace), 0);
    if (opposite < 0) {
        sci(SCI_BRACEBADLIGHT, static_cast<uptr_t>(brace));
        sci(SCI_SETHIGHLIGHTGUIDE, 0);
        return;
    }
    sci(SCI_BRACEHIGHLIGHT, static_cast<uptr_t>(brace), opposite);
    if (sci(SCI_GETINDENTATIONGUIDES) != SC_IV_NONE) {
        const sptr_t column = std::min(sci(SCI_GETCOLUMN, static_cast<uptr_t>(brace)),
                                       sci(SCI_GETCOLUMN, static_cast<uptr_t>(opposite)));
        sci(SCI_SETHIGHLIGHTGUIDE, static_cast<uptr_t>(column));
    }
}

void EditorView::updateLineNumberWidth(bool force)
{
    if (!_lineNumberMargin) {
        if (_lineNumberDigits != 0 || force)
            sci(SCI_SETMARGINWIDTHN, kMarginLineNumber, 0);
        _lineNumberDigits = 0;
        return;
    }
    auto digitsOf = [](sptr_t n) {
        int digits = 1;
        for (; n >= 10; n /= 10)
            ++digits;
        return digits;
    };
    int digits = 0;
    if (_lineNumberDynamicWidth) {
        const sptr_t lastVisible = sci(SCI_GETFIRSTVISIBLELINE) + sci(SCI_LINESONSCREEN) + 1;
        digits = std::max(digitsOf(sci(SCI_DOCLINEFROMVISIBLE, static_cast<uptr_t>(lastVisible))), 3);
    } else {
        digits = std::max(digitsOf(sci(SCI_GETLINECOUNT)), 4);
    }
    if (digits == _lineNumberDigits && !force)
        return;
    _lineNumberDigits = digits;
    const sptr_t pixelWidth = 8 + digits * sends(SCI_TEXTWIDTH, STYLE_LINENUMBER, "8");
    sci(SCI_SETMARGINWIDTHN, kMarginLineNumber, pixelWidth);
}

void EditorView::applyPreferences(const Preferences& p)
{
    // Ajuste de línea (setWrapMode y wrap) y su símbolo
    sci(SCI_SETWRAPMODE, p.wrap ? SC_WRAP_WORD : SC_WRAP_NONE);
    static constexpr int kWrapIndent[] = {SC_WRAPINDENT_FIXED, SC_WRAPINDENT_SAME, SC_WRAPINDENT_INDENT};
    sci(SCI_SETWRAPINDENTMODE, static_cast<uptr_t>(kWrapIndent[static_cast<int>(p.wrapIndent)]));
    sci(SCI_SETWRAPVISUALFLAGS, p.wrapSymbolShow ? SC_WRAPVISUALFLAG_END : SC_WRAPVISUALFLAG_NONE);

    // Símbolos de espacios y fin de línea
    sci(SCI_SETVIEWWS, p.whiteSpaceShow ? SCWS_VISIBLEALWAYS : SCWS_INVISIBLE);
    sci(SCI_SETVIEWEOL, p.eolShow ? 1 : 0);

    // Cursor ("Caret"): ancho 0 = bloque
    if (p.caretWidth == 0) {
        sci(SCI_SETCARETSTYLE, CARETSTYLE_BLOCK);
    } else {
        sci(SCI_SETCARETSTYLE, CARETSTYLE_LINE);
        sci(SCI_SETCARETWIDTH, static_cast<uptr_t>(p.caretWidth));
    }
    sci(SCI_SETCARETPERIOD, static_cast<uptr_t>(p.caretBlinkRate));

    // Multiselección: escribir en todas y pegar en cada una
    sci(SCI_SETMULTIPLESELECTION, p.multiSelection ? 1 : 0);
    sci(SCI_SETADDITIONALSELECTIONTYPING, p.multiSelection ? 1 : 0);
    sci(SCI_SETMULTIPASTE, SC_MULTIPASTE_EACH);
    sci(SCI_SETVIRTUALSPACEOPTIONS,
        SCVS_RECTANGULARSELECTION | (p.virtualSpace ? SCVS_USERACCESSIBLE | SCVS_NOWRAPLINESTART : 0));
    sci(SCI_SETENDATLASTLINE, p.scrollBeyondLastLine ? 0 : 1);
    sci(SCI_SETFONTQUALITY, p.smoothFont ? SC_EFF_QUALITY_LCD_OPTIMIZED : SC_EFF_QUALITY_DEFAULT);

    _lineNumberMargin = p.lineNumberMargin;
    _lineNumberDynamicWidth = p.lineNumberDynamicWidth;
    updateLineNumberWidth(true);
}

bool EditorView::setLexerByName(std::string_view lexerName)
{
    const std::string name(lexerName);
    Scintilla::ILexer5* lexer = CreateLexer(name.c_str());
    if (!lexer)
        return false;
    // Scintilla toma posesión del lexer (lo libera con Release())
    sci(SCI_SETILEXER, 0, reinterpret_cast<sptr_t>(lexer));
    return true;
}

std::string EditorView::text() const
{
    const auto length = static_cast<std::size_t>(sci(SCI_GETLENGTH));
    std::string result(length + 1, '\0');
    sci(SCI_GETTEXT, length + 1, reinterpret_cast<sptr_t>(result.data()));
    result.resize(length);
    return result;
}

void EditorView::setText(std::string_view utf8)
{
    const std::string copy(utf8); // SCI_SETTEXT necesita terminador nulo
    sends(SCI_SETTEXT, 0, copy.c_str());
}

Position EditorView::currentPosition() const
{
    // Se guarda la línea del documento que está arriba, no la de pantalla,
    // para que siga visible aunque cambie el ajuste de línea
    const sptr_t displayedLine = sci(SCI_GETFIRSTVISIBLELINE);
    Position pos;
    pos.firstVisibleLine = sci(SCI_DOCLINEFROMVISIBLE, static_cast<uptr_t>(displayedLine));
    pos.startPos = sci(SCI_GETANCHOR);
    pos.endPos = sci(SCI_GETCURRENTPOS);
    pos.xOffset = sci(SCI_GETXOFFSET);
    pos.selMode = sci(SCI_GETSELECTIONMODE);
    pos.scrollWidth = sci(SCI_GETSCROLLWIDTH);
    return pos;
}

void EditorView::restorePosition(const Position& pos)
{
    sci(SCI_SETSELECTIONMODE, static_cast<uptr_t>(pos.selMode));
    sci(SCI_SETANCHOR, static_cast<uptr_t>(pos.startPos));
    sci(SCI_SETCURRENTPOS, static_cast<uptr_t>(pos.endPos));
    sci(SCI_CANCEL); // fija el modo de selección sin dejarlo "pegado"
    if (!isWrap()) {
        sci(SCI_SETSCROLLWIDTH, static_cast<uptr_t>(pos.scrollWidth));
        sci(SCI_SETXOFFSET, static_cast<uptr_t>(pos.xOffset));
    }
    sci(SCI_CHOOSECARETX);
    const sptr_t lineToShow = sci(SCI_VISIBLEFROMDOCLINE, static_cast<uptr_t>(pos.firstVisibleLine));
    sci(SCI_SETFIRSTVISIBLELINE, static_cast<uptr_t>(lineToShow));
}

} // namespace edita
