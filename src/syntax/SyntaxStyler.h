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

#include "core/LangType.h"

#include <array>
#include <string>
#include <string_view>

class ScintillaEditBase;

namespace edita {

class LanguageRegistry;
struct UserLanguage;
class StyleSheet;
struct Style;

// Indicadores de Scintilla que usa N++ (SCE_UNIVERSAL_FOUND_STYLE…, en ScintillaEditView.h);
// su color es el bgColor del estilo global con ese styleID en stylers.xml
inline constexpr int kIndicatorFound = 31;       // "Find Mark Style" (marcar todo)
inline constexpr int kIndicatorSmart = 29;       // "Smart Highlighting"
inline constexpr int kIndicatorIncremental = 28; // "Incremental highlight all"
inline constexpr int kIndicatorTagMatch = 27;    // "Tags match highlighting"
inline constexpr int kIndicatorTagAttribute = 26; // "Tags attribute"
// "Mark Style 1"…"Mark Style 5" (SCE_UNIVERSAL_FOUND_STYLE_EXT1…EXT5): estilos de token
inline constexpr int kIndicatorTokenStyles[] = {25, 24, 23, 22, 21};

// Márgenes en el orden de N++ (ScintillaEditView::_SC_MARGE_*)
inline constexpr unsigned kMarginLineNumber = 0;
inline constexpr unsigned kMarginSymbol = 1;        // marcadores
inline constexpr unsigned kMarginChangeHistory = 2; // historial de cambios
inline constexpr unsigned kMarginFolder = 3;        // plegado
inline constexpr unsigned kBookmarkMarkerId = 20;   // MARK_BOOKMARK

// Historial de cambios (isChangeHistoryEnabled): 0 no, 1 margen, 2 indicadores, 3 los dos
enum class ChangeHistoryMode : int
{
    Disabled = 0,
    Margin = 1,
    Indicator = 2,
    MarginAndIndicator = 3,
};

// Opciones de vista que afectan a los estilos (ScintillaViewParams en N++),
// con sus valores por defecto
struct StylerOptions
{
    bool indentGuides = true;          // _indentGuideLineShow
    CurrentLineMode currentLine = CurrentLineMode::Background; // _currentLineHiliteMode
    int currentLineFrameWidth = 1;
    FolderStyle folderStyle = FolderStyle::Box;
    QList<int> edgeColumns;            // _edgeMultiColumnPos
    bool edgeBackground = false;       // _isEdgeBgMode
    bool showControlChars = true;      // _ccUniEolShow
    bool sqlBackslashEscapes = true;   // _backSlashIsEscapeCharacterForSql
    int tabSize = 4;                   // NppGUI::_tabSize
    bool replaceTabsBySpaces = false;  // NppGUI::_tabReplacedBySpace
    bool backspaceUnindent = false;    // NppGUI::_backspaceUnindent
    bool bookmarkMargin = true;        // _bookMarkMarginShow
    ChangeHistoryMode changeHistory = ChangeHistoryMode::Margin; // _isChangeHistoryEnabled4NextSession
    bool systemMonospaceFont = true;   // las fuentes de Windows de los temas → la del sistema

    bool operator==(const StylerOptions&) const = default;
};

// Aplica a una vista de Scintilla el lexer, las palabras clave y los estilos de
// un lenguaje según langs.xml y stylers.xml. Port de:
//   ScintillaEditView::defineDocType + setXxxLexer (ScintillaEditView.cpp/.h)
//   ScintillaEditView::performGlobalStyles (colores de línea actual, selección,
//   cursor, márgenes y plegado)
// En Scintilla el lexer y sus palabras clave son del documento y los estilos de
// la vista, así que se aplica cada vez que una vista muestra un documento.
class SyntaxStyler
{
public:
    SyntaxStyler(const LanguageRegistry& languages, const StyleSheet& styles, const StylerOptions& options = {});

    // defineDocType + performGlobalStyles. Con LangType::User, `userLanguage` es
    // la definición del lenguaje del usuario (nullptr = texto normal)
    void apply(ScintillaEditBase& view, LangType lang, const UserLanguage* userLanguage = nullptr) const;

    // Solo performGlobalStyles
    void applyGlobalStyles(ScintillaEditBase& view) const;

    // isNeededFolderMargin
    static bool needsFoldMargin(LangType lang);

    // Fuente que se usará en lugar de `requested` si no está instalada
    // (N++ cae a "Courier New"; aquí, a la monoespaciada del sistema). Con
    // `replaceWindowsFonts`, también si es una de las de Windows que piden los temas
    // de N++ (Courier New, Consolas, Lucida Console)
    static std::string resolveFontName(std::string_view requested, bool replaceWindowsFonts = false);

private:
    using KeywordArray = std::array<const char*, 20>;

    void setStyle(ScintillaEditBase& view, Style style) const;   // con "Global override"
    void setSpecialStyle(ScintillaEditBase& view, const Style& style) const;
    void makeStyle(ScintillaEditBase& view, LangType lang, KeywordArray* keywords) const;
    std::string buildKeywordList(const char* userWords, LangType lang, int index) const;
    void setKeywords(ScintillaEditBase& view, LangType lang, const char* userWords, int index) const;
    void setKeywordsAt(ScintillaEditBase& view, int sciIndex, LangType lang, const KeywordArray& kw, int index) const;
    void populateSubStyleKeywords(ScintillaEditBase& view, LangType lang, int baseStyleId, int count, int firstIndex,
                                  const KeywordArray& kw) const;
    bool setLexerFromLangId(ScintillaEditBase& view, LangType lang) const;
    void setGenericLexer(ScintillaEditBase& view, LangType lang) const;
    void setCppLexer(ScintillaEditBase& view, LangType lang) const;
    void setJsLexer(ScintillaEditBase& view) const;
    void setTypeScriptLexer(ScintillaEditBase& view) const;
    void setObjCLexer(ScintillaEditBase& view, LangType lang) const;
    void setTclLexer(ScintillaEditBase& view) const;
    void setXmlLexer(ScintillaEditBase& view, LangType lang) const;
    void setHtmlLexer(ScintillaEditBase& view) const;
    void setEmbeddedJsLexer(ScintillaEditBase& view) const;
    void setEmbeddedPhpLexer(ScintillaEditBase& view) const;
    void setEmbeddedAspLexer(ScintillaEditBase& view) const;
    void setJsonLexer(ScintillaEditBase& view, bool json5) const;
    void setUserLexer(ScintillaEditBase& view, const UserLanguage& language) const;
    void setBashLexer(ScintillaEditBase& view) const;
    void setNfoStyle(ScintillaEditBase& view) const;
    void setTabSettings(ScintillaEditBase& view, LangType lang) const;

    const LanguageRegistry& _languages;
    const StyleSheet& _styles;
    StylerOptions _options;
};

} // namespace edita
