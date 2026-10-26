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

#include "syntax/SyntaxStyler.h"

#include "syntax/DesktopFonts.h"
#include "syntax/LanguageRegistry.h"
#include "syntax/StyleSheet.h"
#include "syntax/UserLanguages.h"

#include <ScintillaEditBase.h>
#include <ILexer.h>
#include <Lexilla.h>
#include <SciLexer.h>

#include <QFont>
#include <QFontDatabase>

#include <algorithm>
#include <cstdio>
#include <map>
#include <initializer_list>
#include <utility>

namespace edita {

namespace {

using Props = std::initializer_list<std::pair<const char*, const char*>>;

// Listas de palabras clave que se envían (LIST_0…LIST_8 en N++)
constexpr int kList0 = 1 << 0;
constexpr int kList1 = 1 << 1;
constexpr int kList2 = 1 << 2;
constexpr int kList3 = 1 << 3;
constexpr int kList4 = 1 << 4;
constexpr int kList5 = 1 << 5;
constexpr int kList6 = 1 << 6;
constexpr int kList7 = 1 << 7;
constexpr int kList8 = 1 << 8;
constexpr int kListNone = 0;
constexpr int kLists0to2 = kList0 | kList1 | kList2;
constexpr int kLists0to3 = kLists0to2 | kList3;
constexpr int kLists0to4 = kLists0to3 | kList4;
constexpr int kLists0to5 = kLists0to4 | kList5;
constexpr int kLists0to6 = kLists0to5 | kList6;
constexpr int kLists0to7 = kLists0to6 | kList7;

// Colores por defecto de performGlobalStyles (colors.h de N++), en 0x00BBGGRR
constexpr int rgb(int r, int g, int b) { return r | (g << 8) | (b << 16); }
constexpr int kBlack = rgb(0, 0, 0);
constexpr int kWhite = rgb(0xFF, 0xFF, 0xFF);
constexpr int kGrey = rgb(128, 128, 128);
constexpr int kLiteGrey = rgb(0xE8, 0xE8, 0xE8);
constexpr int kVeryLiteGrey = rgb(0xF2, 0xF4, 0xFF);
constexpr int kDarkGrey = rgb(64, 64, 64);
constexpr int kRed = rgb(0xFF, 0, 0);

void setProperty(ScintillaEditBase& view, const char* key, const char* value)
{
    view.send(SCI_SETPROPERTY, reinterpret_cast<uptr_t>(key), reinterpret_cast<sptr_t>(value));
}

void setProperties(ScintillaEditBase& view, Props props)
{
    for (const auto& [key, value] : props)
        setProperty(view, key, value);
}

void setElementColour(ScintillaEditBase& view, int element, int colour)
{
    view.send(SCI_SETELEMENTCOLOUR, static_cast<uptr_t>(element), static_cast<sptr_t>(static_cast<unsigned int>(colour) | 0xFF000000u));
}

constexpr const char* kFoldProps[][2] = {{"fold", "1"}, {"fold.compact", "0"}, {"fold.comment", "1"}};

void setStandardFoldProps(ScintillaEditBase& view)
{
    for (const auto& p : kFoldProps)
        setProperty(view, p[0], p[1]);
}

// Configuración de los lenguajes que N++ prepara con una llamada a setLexer()
// más, a veces, algunas propiedades (ScintillaEditView.h: setCssLexer, setLuaLexer…)
struct GenericLexer
{
    LangType lang;
    int lists;
    int baseStyleId = STYLE_NOT_USED_IN_TABLE;
    int numSubStyles = 8;
    Props props = {};
    std::initializer_list<int> eolFilled = {};
    const char* wordChars = nullptr;
    std::initializer_list<int> visibleWhenControlChars = {};

    static constexpr int STYLE_NOT_USED_IN_TABLE = -1;
};

constexpr const char* kWordCharsUnderscoreHash = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_#";
constexpr const char* kWordCharsPercentDash = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789%-";

const GenericLexer* genericLexer(LangType lang)
{
    using L = LangType;
    static const GenericLexer table[] = {
        {L::Css, kList0 | kList1 | kList4 | kList6},
        {L::Lua, kLists0to7, SCE_LUA_IDENTIFIER, 4},
        {L::Makefile, kListNone},
        {L::Props, kListNone, -1, 8, {}, {SCE_PROPS_SECTION}},
        {L::Ini, kListNone, -1, 8, {}, {SCE_PROPS_SECTION}},
        {L::Sql, kList0 | kList1 | kList4},
        {L::MsSql, kLists0to5},
        {L::Vb, kList0},
        {L::Pascal, kList0, -1, 8, {{"fold.preprocessor", "1"}}},
        {L::Perl, kList0},
        {L::Python, kList0 | kList1, SCE_P_IDENTIFIER, 8,
         {{"fold.quotes.python", "1"}, {"lexer.python.decorator.attributes", "1"}, {"lexer.python.identifier.attributes", "1"}}},
        {L::GdScript, kList0 | kList1, SCE_GD_IDENTIFIER, 8,
         {{"lexer.gdscript.keywords2.no.sub.identifiers", "1"}, {"lexer.gdscript.whinge.level", "1"}}},
        {L::Batch, kList0},
        {L::Tex, kListNone},
        {L::Nsis, kLists0to3},
        {L::Fortran, kLists0to2},
        {L::Fortran77, kLists0to2},
        {L::Lisp, kList0 | kList1},
        {L::Scheme, kList0 | kList1},
        {L::Asm, kLists0to7, -1, 8,
         {{"fold.asm.syntax.based", "1"}, {"fold.asm.comment.multiline", "1"}, {"fold.asm.comment.explicit", "1"}}},
        {L::Diff, kListNone},
        {L::PostScript, kLists0to3},
        {L::Ruby, kList0, -1, 8, {}, {SCE_RB_POD}},
        {L::Smalltalk, kList0},
        {L::Vhdl, kLists0to6},
        {L::Kix, kLists0to2},
        {L::AutoIt, kLists0to6, -1, 8, {{"fold.preprocessor", "1"}}},
        {L::Caml, kLists0to2},
        {L::Ada, kList0},
        {L::Verilog, kList0 | kList1, -1, 8, {{"fold.preprocessor", "1"}}},
        {L::Matlab, kList0},
        {L::Haskell, kList0},
        {L::Inno, kLists0to5},
        {L::CMake, kLists0to2},
        {L::Yaml, kList0},
        {L::Cobol, kLists0to2},
        {L::Gui4Cli, kLists0to4},
        {L::D, kLists0to6},
        {L::PowerShell, kLists0to5},
        {L::R, kLists0to2},
        {L::CoffeeScript, kLists0to3},
        {L::BaanC, kLists0to7 | kList8, -1, 8,
         {{"lexer.baan.styling.within.preprocessor", "1"}, {"fold.preprocessor", "1"}, {"fold.baan.syntax.based", "1"},
          {"fold.baan.keywords.based", "1"}, {"fold.baan.sections", "1"}, {"fold.baan.inner.level", "1"}},
         {SCE_BAAN_STRINGEOL}, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_$:"},
        {L::Srec, kListNone},
        {L::IHex, kListNone},
        {L::TeHex, kListNone},
        {L::Asn1, kLists0to3},
        {L::Avs, kLists0to5, -1, 8, {}, {}, kWordCharsUnderscoreHash},
        {L::BlitzBasic, kLists0to3},
        {L::PureBasic, kLists0to3},
        {L::FreeBasic, kLists0to3},
        {L::Csound, kLists0to2, -1, 8, {}, {SCE_CSOUND_STRINGEOL}},
        {L::Erlang, kLists0to5},
        {L::EScript, kLists0to2},
        {L::Forth, kLists0to5, -1, 8, {}, {}, kWordCharsPercentDash},
        {L::Latex, kListNone},
        {L::Mmixal, kLists0to2},
        {L::Nim, kList0},
        {L::Nncrontab, kLists0to2, -1, 8, {}, {}, kWordCharsPercentDash},
        {L::OScript, kLists0to5, -1, 8, {}, {}, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_$"},
        {L::Rebol, kLists0to6, -1, 8, {}, {}, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789?!.'+-*&|=_~"},
        {L::Registry, kListNone},
        {L::Rust, kLists0to6, -1, 8, {}, {}, kWordCharsUnderscoreHash},
        {L::Spice, kLists0to2},
        {L::Txt2tags, kListNone},
        {L::VisualProlog, kLists0to3},
        {L::Hollywood, kLists0to3},
        {L::Raku, kLists0to6, -1, 8, {{"fold.raku.comment.multiline", "1"}, {"fold.raku.comment.pod", "1"}}},
        {L::Toml, kList0},
        {L::Sas, kLists0to3},
        {L::StructuredText, kLists0to5, -1, 8, {{"fold.preprocessor", "1"}}},
        {L::ErrorList, kListNone, -1, 8, {{"lexer.errorlist.value.separate", "0"}, {"lexer.errorlist.escape.sequences", "1"}},
         {}, nullptr, {SCE_ERR_ESCSEQ, SCE_ERR_ESCSEQ_UNKNOWN}},
        {L::EscSeq, kListNone, -1, 8, {{"lexer.escseq.colour.text", "1"}}, {}, nullptr,
         {SCE_ESCSEQ_IDENTIFIER, SCE_ESCSEQ_UNKNOWN}},
        {L::SearchResult, kListNone, -1, 8, {}, {SCE_SEARCHRESULT_FILE_HEADER}},
    };
    for (const GenericLexer& g : table) {
        if (g.lang == lang)
            return &g;
    }
    return nullptr;
}

// isPythonStyleIndentation: guías de sangría que miran hacia delante
bool isPythonStyleIndentation(LangType lang)
{
    using L = LangType;
    switch (lang) {
    case L::Python: case L::CoffeeScript: case L::Haskell: case L::C: case L::Cpp: case L::ObjC:
    case L::CSharp: case L::Java: case L::Php: case L::JsEmbedded: case L::JavaScript: case L::Makefile:
    case L::Asn1: case L::GdScript:
        return true;
    default:
        return false;
    }
}

} // namespace

SyntaxStyler::SyntaxStyler(const LanguageRegistry& languages, const StyleSheet& styles, const StylerOptions& options)
    : _languages(languages)
    , _styles(styles)
    , _options(options)
{
}

bool SyntaxStyler::needsFoldMargin(LangType lang)
{
    using L = LangType;
    switch (lang) {
    case L::Ascii: case L::Batch: case L::Text: case L::Makefile: case L::Haskell: case L::Smalltalk:
    case L::Kix: case L::Ada:
        return false;
    default:
        return true;
    }
}

std::string SyntaxStyler::resolveFontName(std::string_view requested, bool replaceWindowsFonts)
{
    const QString family = QString::fromUtf8(requested.data(), static_cast<qsizetype>(requested.size()));
    static const QStringList kWindowsFonts{QStringLiteral("Courier New"), QStringLiteral("Consolas"),
                                           QStringLiteral("Lucida Console")};
    const bool windowsFont = kWindowsFonts.contains(family, Qt::CaseInsensitive);
    // "Monospace" (los temas de Edita): la monoespaciada del sistema, con su nombre real
    if (family.compare(QLatin1String("Monospace"), Qt::CaseInsensitive) == 0)
        return desktopfonts::fixedFont().family().toStdString();
    if (QFontDatabase::hasFamily(family) && !(replaceWindowsFonts && windowsFont))
        return std::string(requested);
    return desktopfonts::fixedFont().family().toStdString();
}

// --- Estilos ----------------------------------------------------------------------

void SyntaxStyler::setSpecialStyle(ScintillaEditBase& view, const Style& style) const
{
    const auto id = static_cast<uptr_t>(style.id);
    if (style.colorStyle & kColorStyleForeground)
        view.send(SCI_STYLESETFORE, id, style.fgColor & 0xFFFFFF);
    if (style.colorStyle & kColorStyleBackground)
        view.send(SCI_STYLESETBACK, id, style.bgColor & 0xFFFFFF);
    if (!style.fontName.empty()) {
        const std::string font = resolveFontName(style.fontName, _options.systemMonospaceFont);
        view.sends(SCI_STYLESETFONT, id, font.c_str());
    }
    if (style.fontStyle != kStyleNotUsed) {
        view.send(SCI_STYLESETBOLD, id, (style.fontStyle & kFontStyleBold) ? 1 : 0);
        view.send(SCI_STYLESETITALIC, id, (style.fontStyle & kFontStyleItalic) ? 1 : 0);
        view.send(SCI_STYLESETUNDERLINE, id, (style.fontStyle & kFontStyleUnderline) ? 1 : 0);
    }
    if (style.fontSize > 0)
        view.send(SCI_STYLESETSIZE, id, style.fontSize);
}

void SyntaxStyler::setStyle(ScintillaEditBase& view, Style style) const
{
    // "Global override" (ScintillaEditView::setStyle)
    const GlobalOverride& go = _styles.globalOverride();
    const Style* over = go.isEnabled() ? _styles.globalByName("Global override") : nullptr;
    if (over) {
        auto overrideColor = [&](bool enabled, int bit, int overColor, int& color) {
            if (!enabled)
                return;
            if (over->colorStyle & bit) {
                style.colorStyle |= bit;
                color = overColor;
            } else if (style.id == STYLE_DEFAULT) {
                style.colorStyle |= bit; // global transparente: se usa el color del estilo por defecto
            } else {
                style.colorStyle &= ~bit;
            }
        };
        overrideColor(go.foreground, kColorStyleForeground, over->fgColor, style.fgColor);
        overrideColor(go.background, kColorStyleBackground, over->bgColor, style.bgColor);
        if (go.font && !over->fontName.empty())
            style.fontName = over->fontName;
        if (go.fontSize && over->fontSize > 0)
            style.fontSize = over->fontSize;
        if (over->fontStyle != kStyleNotUsed) {
            auto overrideBit = [&](bool enabled, int bit) {
                if (!enabled)
                    return;
                if (style.fontStyle == kStyleNotUsed)
                    style.fontStyle = 0;
                style.fontStyle = (over->fontStyle & bit) ? (style.fontStyle | bit) : (style.fontStyle & ~bit);
            };
            overrideBit(go.bold, kFontStyleBold);
            overrideBit(go.italic, kFontStyleItalic);
            overrideBit(go.underline, kFontStyleUnderline);
        }
    }
    setSpecialStyle(view, style);
}

void SyntaxStyler::makeStyle(ScintillaEditBase& view, LangType lang, KeywordArray* keywords) const
{
    const LexerStyles* lexer = _styles.lexer(languageInfo(lang).name);
    if (!lexer)
        return;
    for (const Style& style : lexer->styles) {
        setStyle(view, style);
        if (keywords && style.keywordClass >= 0 && style.keywordClass < kKeywordListCount && !style.keywords.empty())
            (*keywords)[static_cast<std::size_t>(style.keywordClass)] = style.keywords.c_str();
    }
}

std::string SyntaxStyler::buildKeywordList(const char* userWords, LangType lang, int index) const
{
    // concatToBuildKeywordList: palabras del usuario (stylers.xml) + las de langs.xml
    std::string list = userWords ? userWords : "";
    list += ' ';
    list += _languages.keywords(lang, index);
    return list;
}

void SyntaxStyler::setKeywords(ScintillaEditBase& view, LangType lang, const char* userWords, int index) const
{
    const std::string list = buildKeywordList(userWords, lang, index);
    view.sends(SCI_SETKEYWORDS, static_cast<uptr_t>(index), list.c_str());
}

void SyntaxStyler::setKeywordsAt(ScintillaEditBase& view, int sciIndex, LangType lang, const KeywordArray& kw, int index) const
{
    const std::string list = buildKeywordList(kw[static_cast<std::size_t>(index)], lang, index);
    view.sends(SCI_SETKEYWORDS, static_cast<uptr_t>(sciIndex), list.c_str());
}

void SyntaxStyler::populateSubStyleKeywords(ScintillaEditBase& view, LangType lang, int baseStyleId, int count,
                                            int firstIndex, const KeywordArray& kw) const
{
    const sptr_t first = view.send(SCI_ALLOCATESUBSTYLES, static_cast<uptr_t>(baseStyleId), count) & 0xFF;
    for (int i = 0; i < count; ++i) {
        const int index = firstIndex + i;
        const std::string list = buildKeywordList(kw[static_cast<std::size_t>(index)], lang, index);
        view.sends(SCI_SETIDENTIFIERS, static_cast<uptr_t>(first + i), list.c_str());
    }
}

bool SyntaxStyler::setLexerFromLangId(ScintillaEditBase& view, LangType lang) const
{
    if (lang == LangType::External)
        return false;
    // Scintilla toma posesión del lexer; CreateLexer("null") da el lexer vacío
    view.send(SCI_SETILEXER, 0, reinterpret_cast<sptr_t>(CreateLexer(languageInfo(lang).lexerId)));
    return true;
}

// --- Lexers -------------------------------------------------------------------------

void SyntaxStyler::setGenericLexer(ScintillaEditBase& view, LangType lang) const
{
    const GenericLexer* g = genericLexer(lang);
    if (!g)
        return;
    if (lang == LangType::Tex) { // setTeXLexer: las cuatro listas vacías
        for (uptr_t i = 0; i < 4; ++i)
            view.sends(SCI_SETKEYWORDS, i, "");
    }
    // ScintillaEditView::setLexer
    setLexerFromLangId(view, lang);
    KeywordArray kw{};
    makeStyle(view, lang, &kw);
    for (int i = 0; i <= 8; ++i) {
        if (g->lists & (1 << i))
            setKeywords(view, lang, kw[static_cast<std::size_t>(i)], i);
    }
    if (g->baseStyleId != GenericLexer::STYLE_NOT_USED_IN_TABLE)
        populateSubStyleKeywords(view, lang, g->baseStyleId, g->numSubStyles, KeywordIndex::Substyle1, kw);
    setStandardFoldProps(view);
    if (lang == LangType::Sql)
        setProperty(view, "sql.backslash.escapes", _options.sqlBackslashEscapes ? "1" : "0");
    setProperties(view, g->props);
    for (const int style : g->eolFilled)
        view.send(SCI_STYLESETEOLFILLED, static_cast<uptr_t>(style), 1);
    if (g->wordChars)
        view.sends(SCI_SETWORDCHARS, 0, g->wordChars);
    for (const int style : g->visibleWhenControlChars)
        view.send(SCI_STYLESETVISIBLE, static_cast<uptr_t>(style), _options.showControlChars ? 1 : 0);
}

void SyntaxStyler::setCppLexer(ScintillaEditBase& view, LangType lang) const
{
    setLexerFromLangId(view, LangType::Cpp);
    if (lang == LangType::Go)
        setProperty(view, "lexer.cpp.backquoted.strings", "1");
    if (lang != LangType::Rc) {
        const std::string doxygen(_languages.keywords(LangType::Cpp, KeywordIndex::Type2));
        if (!doxygen.empty())
            view.sends(SCI_SETKEYWORDS, 2, doxygen.c_str());
    }
    KeywordArray kw{};
    makeStyle(view, lang, &kw);
    setKeywordsAt(view, 0, lang, kw, KeywordIndex::Instr);
    setKeywordsAt(view, 1, lang, kw, KeywordIndex::Type);
    setKeywordsAt(view, 3, lang, kw, KeywordIndex::Instr2);
    populateSubStyleKeywords(view, lang, SCE_C_IDENTIFIER, 8, KeywordIndex::Substyle1, kw);
    setStandardFoldProps(view);
    setProperties(view, {{"fold.cpp.comment.explicit", "0"}, {"fold.preprocessor", "1"},
                         {"lexer.cpp.track.preprocessor", "0"}});
}

void SyntaxStyler::setJsLexer(ScintillaEditBase& view) const
{
    const std::string doxygen(_languages.keywords(LangType::Cpp, KeywordIndex::Type2));
    setLexerFromLangId(view, LangType::JavaScript);
    KeywordArray kw{};
    makeStyle(view, LangType::JavaScript, &kw);
    if (!doxygen.empty())
        view.sends(SCI_SETKEYWORDS, 2, doxygen.c_str());

    if (_styles.lexer(languageInfo(LangType::JavaScript).name)) {
        // Estilos modernos de "javascript.js"
        setKeywordsAt(view, 0, LangType::JavaScript, kw, KeywordIndex::Instr);
        setKeywordsAt(view, 1, LangType::JavaScript, kw, KeywordIndex::Type);
        setKeywordsAt(view, 3, LangType::JavaScript, kw, KeywordIndex::Instr2);
        populateSubStyleKeywords(view, LangType::JavaScript, SCE_C_IDENTIFIER, 8, KeywordIndex::Substyle1, kw);
    } else if (const LexerStyles* old = _styles.lexer(languageInfo(LangType::JsEmbedded).name)) {
        // Compatibilidad con stylers.xml antiguos: estilos de JS embebido traducidos a cpp
        for (Style style : old->styles) {
            switch (style.id) {
            case SCE_HJ_DEFAULT: style.id = SCE_C_DEFAULT; break;
            case SCE_HJ_WORD: style.id = SCE_C_IDENTIFIER; break;
            case SCE_HJ_SYMBOLS: style.id = SCE_C_OPERATOR; break;
            case SCE_HJ_COMMENT: style.id = SCE_C_COMMENT; break;
            case SCE_HJ_COMMENTLINE: style.id = SCE_C_COMMENTLINE; break;
            case SCE_HJ_COMMENTDOC: style.id = SCE_C_COMMENTDOC; break;
            case SCE_HJ_NUMBER: style.id = SCE_C_NUMBER; break;
            case SCE_HJ_KEYWORD: style.id = SCE_C_WORD; break;
            case SCE_HJ_DOUBLESTRING: style.id = SCE_C_STRING; break;
            case SCE_HJ_SINGLESTRING: style.id = SCE_C_CHARACTER; break;
            case SCE_HJ_REGEX: style.id = SCE_C_REGEX; break;
            default: break;
            }
            setStyle(view, style);
        }
        for (const int s : {SCE_C_DEFAULT, SCE_C_COMMENTLINE, SCE_C_COMMENT, SCE_C_COMMENTDOC})
            view.send(SCI_STYLESETEOLFILLED, static_cast<uptr_t>(s), 1);
        KeywordArray kwOld{};
        makeStyle(view, LangType::JsEmbedded, &kwOld);
        setKeywordsAt(view, 0, LangType::JsEmbedded, kwOld, KeywordIndex::Instr);
    }
    setStandardFoldProps(view);
    setProperties(view, {{"fold.cpp.comment.explicit", "0"}, {"fold.preprocessor", "1"},
                         {"lexer.cpp.track.preprocessor", "0"}, {"lexer.cpp.backquoted.strings", "2"}});
}

void SyntaxStyler::setTypeScriptLexer(ScintillaEditBase& view) const
{
    const std::string doxygen(_languages.keywords(LangType::Cpp, KeywordIndex::Type2));
    setLexerFromLangId(view, LangType::TypeScript);
    if (!doxygen.empty())
        view.sends(SCI_SETKEYWORDS, 2, doxygen.c_str());
    KeywordArray kw{};
    makeStyle(view, LangType::TypeScript, &kw);
    setKeywordsAt(view, 0, LangType::TypeScript, kw, KeywordIndex::Instr);
    setKeywordsAt(view, 1, LangType::TypeScript, kw, KeywordIndex::Type);
    populateSubStyleKeywords(view, LangType::TypeScript, SCE_C_IDENTIFIER, 8, KeywordIndex::Substyle1, kw);
    setStandardFoldProps(view);
    setProperties(view, {{"fold.cpp.comment.explicit", "0"}, {"fold.preprocessor", "1"},
                         {"lexer.cpp.track.preprocessor", "0"}, {"lexer.cpp.backquoted.strings", "1"}});
}

void SyntaxStyler::setObjCLexer(ScintillaEditBase& view, LangType lang) const
{
    setLexerFromLangId(view, LangType::ObjC);
    KeywordArray kw{};
    makeStyle(view, lang, &kw);
    setKeywordsAt(view, 0, lang, kw, KeywordIndex::Instr);
    setKeywordsAt(view, 1, lang, kw, KeywordIndex::Type);
    const std::string doxygen(_languages.keywords(LangType::Cpp, KeywordIndex::Type2));
    view.sends(SCI_SETKEYWORDS, 2, doxygen.c_str());
    setKeywordsAt(view, 3, lang, kw, KeywordIndex::Instr2);
    setKeywordsAt(view, 4, lang, kw, KeywordIndex::Type2);
    setStandardFoldProps(view);
    setProperties(view, {{"fold.cpp.comment.explicit", "0"}, {"fold.preprocessor", "1"}});
}

void SyntaxStyler::setTclLexer(ScintillaEditBase& view) const
{
    setLexerFromLangId(view, LangType::Tcl);
    KeywordArray kw{};
    makeStyle(view, LangType::Tcl, &kw);
    // Orden de las listas de LexTCL: TCL, iTCL, TK, TK cmd, expand, user1-4
    const int order[] = {KeywordIndex::Instr, KeywordIndex::Type, KeywordIndex::Instr2, KeywordIndex::Type2,
                         KeywordIndex::Type3, KeywordIndex::Type4, KeywordIndex::Type5, KeywordIndex::Type6,
                         KeywordIndex::Type7};
    for (int i = 0; i < 9; ++i)
        setKeywordsAt(view, i, LangType::Tcl, kw, order[i]);
}

void SyntaxStyler::setHtmlLexer(ScintillaEditBase& view) const
{
    KeywordArray kw{};
    makeStyle(view, LangType::Html, &kw);
    setKeywordsAt(view, 0, LangType::Html, kw, KeywordIndex::Instr);  // etiquetas
    setKeywordsAt(view, 5, LangType::Html, kw, KeywordIndex::Instr2); // DOCTYPE
    populateSubStyleKeywords(view, LangType::Html, SCE_H_TAG, 4, KeywordIndex::Substyle1, kw);
    populateSubStyleKeywords(view, LangType::Html, SCE_H_ATTRIBUTE, 4, KeywordIndex::Substyle5, kw);
}

void SyntaxStyler::setEmbeddedJsLexer(ScintillaEditBase& view) const
{
    KeywordArray kw{};
    makeStyle(view, LangType::JsEmbedded, &kw);
    setKeywordsAt(view, 1, LangType::JsEmbedded, kw, KeywordIndex::Instr);
    populateSubStyleKeywords(view, LangType::JsEmbedded, SCE_HJ_WORD, 8, KeywordIndex::Substyle1, kw);
    for (const int s : {SCE_HJ_DEFAULT, SCE_HJ_COMMENT, SCE_HJ_COMMENTDOC, SCE_HJ_TEMPLATELITERAL, SCE_HJA_TEMPLATELITERAL})
        view.send(SCI_STYLESETEOLFILLED, static_cast<uptr_t>(s), 1);
}

void SyntaxStyler::setEmbeddedPhpLexer(ScintillaEditBase& view) const
{
    KeywordArray kw{};
    makeStyle(view, LangType::Php, &kw);
    setKeywordsAt(view, 4, LangType::Php, kw, KeywordIndex::Instr);
    populateSubStyleKeywords(view, LangType::Php, SCE_HPHP_WORD, 8, KeywordIndex::Substyle1, kw);
    for (const int s : {SCE_HPHP_DEFAULT, SCE_HPHP_COMMENT})
        view.send(SCI_STYLESETEOLFILLED, static_cast<uptr_t>(s), 1);
}

void SyntaxStyler::setEmbeddedAspLexer(ScintillaEditBase& view) const
{
    KeywordArray kw{};
    makeStyle(view, LangType::Asp, &kw);
    setProperty(view, "asp.default.language", "2");
    // N++ toma aquí las palabras de Visual Basic
    const std::string list = buildKeywordList(kw[KeywordIndex::Instr], LangType::Vb, KeywordIndex::Instr);
    view.sends(SCI_SETKEYWORDS, 2, list.c_str());
    populateSubStyleKeywords(view, LangType::Asp, SCE_HB_WORD, 8, KeywordIndex::Substyle1, kw);
    view.send(SCI_STYLESETEOLFILLED, SCE_HBA_DEFAULT, 1);
}

void SyntaxStyler::setXmlLexer(ScintillaEditBase& view, LangType lang) const
{
    if (lang == LangType::Xml) {
        setLexerFromLangId(view, LangType::Xml);
        KeywordArray kw{};
        makeStyle(view, lang, &kw);
        setKeywordsAt(view, 5, LangType::Xml, kw, KeywordIndex::Instr); // DOCTYPE
        // En XML los subestilos son solo de atributos: los 8 para ellos
        populateSubStyleKeywords(view, lang, SCE_H_ATTRIBUTE, 8, KeywordIndex::Substyle1, kw);
        setProperty(view, "lexer.xml.allow.scripts", "0");
    } else {
        setLexerFromLangId(view, LangType::Html);
        setHtmlLexer(view);
        setEmbeddedJsLexer(view);
        setEmbeddedPhpLexer(view);
        setEmbeddedAspLexer(view);
    }
    setProperties(view, {{"fold", "1"}, {"fold.compact", "0"}, {"fold.html", "1"}, {"fold.hypertext.comment", "1"}});
}

void SyntaxStyler::setJsonLexer(ScintillaEditBase& view, bool json5) const
{
    setLexerFromLangId(view, json5 ? LangType::Json5 : LangType::Json);
    KeywordArray kw{};
    makeStyle(view, LangType::Json, &kw);
    setKeywordsAt(view, 0, LangType::Json, kw, KeywordIndex::Instr);
    setKeywordsAt(view, 1, LangType::Json, kw, KeywordIndex::Instr2);
    setProperties(view, {{"fold", "1"}, {"fold.compact", "0"}, {"lexer.json.escape.sequence", "1"}});
    if (json5)
        setProperty(view, "lexer.json.allow.comments", "1");
}

void SyntaxStyler::setBashLexer(ScintillaEditBase& view) const
{
    setLexerFromLangId(view, LangType::Bash);
    KeywordArray kw{};
    makeStyle(view, LangType::Bash, &kw);
    setKeywords(view, LangType::Bash, kw[KeywordIndex::Instr], KeywordIndex::Instr);
    populateSubStyleKeywords(view, LangType::Bash, SCE_SH_IDENTIFIER, 4, KeywordIndex::Substyle1, kw);
    populateSubStyleKeywords(view, LangType::Bash, SCE_SH_SCALAR, 4, KeywordIndex::Substyle5, kw);
    setStandardFoldProps(view);
}

void SyntaxStyler::setNfoStyle(ScintillaEditBase& view) const
{
    // L_ASCII ("MS-DOS Style"): arte ASCII con una fuente monoespaciada concreta.
    // N++ usa Lucida Console; aquí cae a la monoespaciada del sistema si no está.
    Style nfo;
    nfo.id = STYLE_DEFAULT;
    nfo.fontName = "Lucida Console";
    nfo.fontSize = 10;
    if (const LexerStyles* lexer = _styles.lexer("nfo")) {
        if (const Style* def = lexer->findByName("DEFAULT")) {
            nfo.fgColor = def->fgColor;
            nfo.bgColor = def->bgColor;
            if (!def->fontName.empty()) {
                nfo.fontName = def->fontName;
                if (def->fontSize != kStyleNotUsed)
                    nfo.fontSize = def->fontSize;
                if (def->fontStyle != kStyleNotUsed)
                    nfo.fontStyle = def->fontStyle;
            }
            nfo.colorStyle = def->colorStyle;
        }
    }
    setSpecialStyle(view, nfo);
    view.send(SCI_STYLECLEARALL);
}

void SyntaxStyler::setTabSettings(ScintillaEditBase& view, LangType lang) const
{
    // Lang::setTabInfo: bit 0x80 = reemplazar por espacios, 0x7F = tamaño
    const LanguageDefinition* def = _languages.find(lang);
    const int info = def ? def->tabSettings : -1;
    const int size = info == -1 ? -1 : (info & 0x7F);
    if (size > 0) {
        view.send(SCI_SETTABWIDTH, static_cast<uptr_t>(size));
        view.send(SCI_SETUSETABS, (info & 0x80) ? 0 : 1);
        view.send(SCI_SETBACKSPACEUNINDENTS, def->backspaceUnindent ? 1 : 0);
    } else {
        view.send(SCI_SETTABWIDTH, static_cast<uptr_t>(_options.tabSize > 0 ? _options.tabSize : 4));
        view.send(SCI_SETUSETABS, _options.replaceTabsBySpaces ? 0 : 1);
        view.send(SCI_SETBACKSPACEUNINDENTS, _options.backspaceUnindent ? 1 : 0);
    }
}

// --- defineDocType -------------------------------------------------------------------

void SyntaxStyler::apply(ScintillaEditBase& view, LangType lang, const UserLanguage* userLanguage) const
{
    if (const Style* def = _styles.globalById(STYLE_DEFAULT)) {
        Style forced = *def;
        forced.colorStyle = kColorStyleAll; // el estilo por defecto nunca es transparente
        setStyle(view, forced);
    }
    view.send(SCI_STYLECLEARALL);

    const bool foldMargin = needsFoldMargin(lang) && _options.folderStyle != FolderStyle::None;
    view.send(SCI_SETMARGINWIDTHN, kMarginFolder, foldMargin ? 14 : 0);
    view.send(SCI_SETMARGINWIDTHN, kMarginSymbol, _options.bookmarkMargin ? 16 : 0);

    // Historial de cambios (ScintillaEditView::activateBuffer): es del documento y
    // solo empieza si aún no hay nada que deshacer, así que se aplica al mostrarlo
    const int history = static_cast<int>(_options.changeHistory);
    int historyFlags = SC_CHANGE_HISTORY_DISABLED;
    if (history != 0) {
        historyFlags = SC_CHANGE_HISTORY_ENABLED;
        if (history & 1)
            historyFlags |= SC_CHANGE_HISTORY_MARKERS;
        if (history & 2)
            historyFlags |= SC_CHANGE_HISTORY_INDICATORS;
    }
    view.send(SCI_SETCHANGEHISTORY, static_cast<uptr_t>(historyFlags));
    view.send(SCI_SETMARGINWIDTHN, kMarginChangeHistory, (history & 1) ? 9 : 0);

    using L = LangType;
    switch (lang) {
    case L::C: case L::Cpp: case L::Java: case L::Rc: case L::CSharp: case L::Flash: case L::Swift: case L::Go:
        setCppLexer(view, lang);
        break;
    case L::JsEmbedded: case L::JavaScript:
        setJsLexer(view);
        break;
    case L::Tcl:
        setTclLexer(view);
        break;
    case L::ObjC:
        setObjCLexer(view, lang);
        break;
    case L::Php: case L::Asp: case L::Jsp: case L::Html: case L::Xml:
        setXmlLexer(view, lang);
        break;
    case L::Json:
        setJsonLexer(view, false);
        break;
    case L::Json5:
        setJsonLexer(view, true);
        break;
    case L::TypeScript:
        setTypeScriptLexer(view);
        break;
    case L::Bash:
        setBashLexer(view);
        break;
    case L::Ascii:
        setLexerFromLangId(view, L::Text);
        setNfoStyle(view);
        break;
    case L::User:
        if (userLanguage)
            setUserLexer(view, *userLanguage);
        else
            setLexerFromLangId(view, L::Text);
        break;
    case L::Text: case L::External:
        // Los lexers externos (DLL de N++) no existen aquí: texto normal
        setLexerFromLangId(view, L::Text);
        break;
    default:
        if (genericLexer(lang))
            setGenericLexer(view, lang);
        else
            setLexerFromLangId(view, L::Text);
        break;
    }

    // "Todos los estilos globales deben ir aquí" (defineDocType)
    for (const int id : {STYLE_INDENTGUIDE, STYLE_BRACELIGHT, STYLE_BRACEBAD}) {
        if (const Style* s = _styles.globalById(id))
            setStyle(view, *s);
    }
    if (const Style* s = _styles.globalById(STYLE_LINENUMBER))
        setSpecialStyle(view, *s);

    setTabSettings(view, lang);
    if (_options.indentGuides)
        view.send(SCI_SETINDENTATIONGUIDES, isPythonStyleIndentation(lang) ? SC_IV_LOOKFORWARD : SC_IV_LOOKBOTH);
    else
        view.send(SCI_SETINDENTATIONGUIDES, SC_IV_NONE);

    applyGlobalStyles(view);
}

// --- Lenguajes del usuario (ScintillaEditView::setUserLexer) ----------------------------

void SyntaxStyler::setUserLexer(ScintillaEditBase& view, const UserLanguage& lang) const
{
    Scintilla::ILexer5* lexer = CreateLexer("user");
    if (!lexer)
        return;
    view.send(SCI_SETILEXER, 0, reinterpret_cast<sptr_t>(lexer));

    setProperty(view, "fold", "1");
    setProperty(view, "userDefine.isCaseIgnored", lang.caseIgnored ? "1" : "0");
    setProperty(view, "userDefine.allowFoldOfComments", lang.allowFoldOfComments ? "1" : "0");
    setProperty(view, "userDefine.foldCompact", lang.foldCompact ? "1" : "0");
    for (int i = 0; i < 8; ++i) {
        const std::string name = "userDefine.prefixKeywords" + std::to_string(i + 1);
        setProperty(view, name.c_str(), lang.isPrefix[static_cast<std::size_t>(i)] ? "1" : "0");
    }

    // Unas listas van en propiedades (setLexerMapper); las demás, como palabras
    // clave en orden, con las palabras entre comillas unidas por \v o \b
    static const std::map<int, const char*> kPropertyLists = {
        {UserKeywordList::Comments, "userDefine.comments"},
        {UserKeywordList::Delimiters, "userDefine.delimiters"},
        {UserKeywordList::Operators1, "userDefine.operators1"},
        {UserKeywordList::NumberPrefix1, "userDefine.numberPrefix1"},
        {UserKeywordList::NumberPrefix2, "userDefine.numberPrefix2"},
        {UserKeywordList::NumberExtras1, "userDefine.numberExtras1"},
        {UserKeywordList::NumberExtras2, "userDefine.numberExtras2"},
        {UserKeywordList::NumberSuffix1, "userDefine.numberSuffix1"},
        {UserKeywordList::NumberSuffix2, "userDefine.numberSuffix2"},
        {UserKeywordList::NumberRange, "userDefine.numberRange"},
        {UserKeywordList::FoldersInCode1Open, "userDefine.foldersInCode1Open"},
        {UserKeywordList::FoldersInCode1Middle, "userDefine.foldersInCode1Middle"},
        {UserKeywordList::FoldersInCode1Close, "userDefine.foldersInCode1Close"},
    };
    uptr_t keywordIndex = 0;
    for (int i = 0; i < UserKeywordList::Total; ++i) {
        const std::string& words = lang.keywordLists[static_cast<std::size_t>(i)];
        if (const auto it = kPropertyLists.find(i); it != kPropertyLists.end()) {
            setProperty(view, it->second, words.c_str());
            continue;
        }
        std::string out;
        bool inDouble = false;
        bool inSingle = false;
        bool nonWhiteSpace = false;
        const std::size_t len = words.size();
        auto at = [&](std::size_t j) { return j < len ? words[j] : '\0'; };
        for (std::size_t j = 0; j < len; ++j) {
            const char c = words[j];
            if (!inSingle && c == '"') {
                inDouble = !inDouble;
                continue;
            }
            if (!inDouble && c == '\'') {
                inSingle = !inSingle;
                continue;
            }
            if (c == '\\' && (at(j + 1) == '"' || at(j + 1) == '\'' || at(j + 1) == '\\')) {
                out += words[++j];
                continue;
            }
            if (inDouble || inSingle) {
                if (static_cast<unsigned char>(c) > ' ') {
                    out += c;
                    nonWhiteSpace = true;
                } else if (nonWhiteSpace && j > 0 && words[j - 1] != '"' && at(j + 1) != '"'
                           && static_cast<unsigned char>(at(j + 1)) > ' ') {
                    out += inDouble ? '\v' : '\b';
                }
            } else {
                out += c;
            }
        }
        view.send(SCI_SETKEYWORDS, keywordIndex++, reinterpret_cast<sptr_t>(out.c_str()));
    }

    setProperty(view, "userDefine.forcePureLC", std::to_string(lang.forcePureLC).c_str());
    setProperty(view, "userDefine.decimalSeparator", std::to_string(lang.decimalSeparator).c_str());
    // LexUser guarda en caché las listas por definición y el anidamiento por documento
    setProperty(view, "userDefine.udlName", std::to_string(lang.lexerId).c_str());
    const auto document = static_cast<unsigned long long>(view.send(SCI_GETDOCPOINTER));
    setProperty(view, "userDefine.currentBufferID", std::to_string((document >> 4) & 0x7FFFFFFF).c_str());

    for (const Style& style : lang.styles) {
        if (style.id == kStyleNotUsed)
            continue;
        char nesting[32];
        std::snprintf(nesting, sizeof nesting, "userDefine.nesting.%02d", style.id);
        setProperty(view, nesting, std::to_string(style.nesting).c_str());
        setStyle(view, style);
    }
}

// --- performGlobalStyles --------------------------------------------------------------

void SyntaxStyler::applyGlobalStyles(ScintillaEditBase& view) const
{
    auto byName = [this](const char* name) { return _styles.globalByName(name); };

    // ScintillaEditView::setCRLineHiliteMode: fondo o marco de la línea del cursor
    if (_options.currentLine != CurrentLineMode::None) {
        if (const Style* s = byName("Current line background colour"))
            setElementColour(view, SC_ELEMENT_CARET_LINE_BACK, s->bgColor);
        view.send(SCI_SETCARETLINEVISIBLEALWAYS, 1);
    } else {
        view.send(SCI_RESETELEMENTCOLOUR, SC_ELEMENT_CARET_LINE_BACK);
    }
    view.send(SCI_SETCARETLINEFRAME,
              _options.currentLine == CurrentLineMode::Frame ? static_cast<uptr_t>(std::clamp(_options.currentLineFrameWidth, 1, 6)) : 0);

    int selBack = kGrey;
    int selFore = kBlack;
    if (const Style* s = byName("Selected text colour")) {
        selBack = s->bgColor;
        selFore = s->fgColor;
    }
    (void)selFore; // N++ solo lo usa con la opción "color de texto seleccionado único"
    setElementColour(view, SC_ELEMENT_SELECTION_BACK, selBack);
    setElementColour(view, SC_ELEMENT_SELECTION_INACTIVE_BACK, selBack);

    const Style* multiSel = byName("Multi-selected text color");
    setElementColour(view, SC_ELEMENT_SELECTION_ADDITIONAL_BACK, multiSel ? multiSel->bgColor : kLiteGrey);

    const Style* caret = byName("Caret colour");
    setElementColour(view, SC_ELEMENT_CARET, caret ? caret->fgColor : kBlack);
    const Style* multiCaret = byName("Multi-edit carets color");
    setElementColour(view, SC_ELEMENT_CARET_ADDITIONAL, multiCaret ? multiCaret->fgColor : kDarkGrey);

    // Línea de margen (Notepad_plus::setEdge…): una columna como línea o fondo, varias como líneas
    const Style* edge = byName("Edge colour");
    const int edgeColour = (edge ? edge->fgColor : kLiteGrey) & 0xFFFFFF;
    view.send(SCI_SETEDGECOLOUR, static_cast<uptr_t>(edgeColour));
    view.send(SCI_MULTIEDGECLEARALL);
    if (_options.edgeColumns.isEmpty()) {
        view.send(SCI_SETEDGEMODE, EDGE_NONE);
    } else if (_options.edgeColumns.size() == 1) {
        view.send(SCI_SETEDGEMODE, _options.edgeBackground ? EDGE_BACKGROUND : EDGE_LINE);
        view.send(SCI_SETEDGECOLUMN, static_cast<uptr_t>(_options.edgeColumns.front()));
    } else {
        view.send(SCI_SETEDGEMODE, EDGE_MULTILINE);
        for (const int column : _options.edgeColumns)
            view.send(SCI_MULTIEDGEADDLINE, static_cast<uptr_t>(column), edgeColour);
    }

    // Indicadores de búsqueda: redondeados, translúcidos y bajo el texto (ScintillaEditView::init)
    struct IndicatorDefault
    {
        int id;
        int colour;
    };
    static constexpr IndicatorDefault kIndicators[] = {
        {kIndicatorFound, kRed},
        {kIndicatorSmart, rgb(0, 0xFF, 0)},
        {kIndicatorIncremental, rgb(0, 0, 0xFF)},
        {kIndicatorTagMatch, rgb(0x80, 0x00, 0xFF)},
        {kIndicatorTagAttribute, rgb(0xFF, 0xFF, 0x00)},
        {kIndicatorTokenStyles[0], rgb(0x00, 0xFF, 0xFF)},
        {kIndicatorTokenStyles[1], rgb(0xFF, 0x80, 0x00)},
        {kIndicatorTokenStyles[2], rgb(0xFF, 0xFF, 0x00)},
        {kIndicatorTokenStyles[3], rgb(0x80, 0x00, 0xFF)},
        {kIndicatorTokenStyles[4], rgb(0x00, 0x80, 0x00)},
    };
    for (const auto& [id, colour] : kIndicators) {
        const auto indic = static_cast<uptr_t>(id);
        const Style* s = _styles.globalById(id);
        view.send(SCI_INDICSETSTYLE, indic, INDIC_ROUNDBOX);
        view.send(SCI_INDICSETFORE, indic, (s ? s->bgColor : colour) & 0xFFFFFF);
        view.send(SCI_INDICSETALPHA, indic, 100);
        view.send(SCI_INDICSETUNDER, indic, 1);
    }
    // Diagnósticos (LSP): ondulado rojo, naranja o azul, como en VS Code; las pistas, puntos
    const bool darkBack = [&] {
        const auto back = static_cast<int>(view.send(SCI_STYLEGETBACK, STYLE_DEFAULT));
        return ((back & 0xFF) * 299 + ((back >> 8) & 0xFF) * 587 + ((back >> 16) & 0xFF) * 114) / 1000 < 128;
    }();
    const std::pair<int, int> kDiagnostics[] = {
        {kIndicatorDiagError, darkBack ? rgb(0xF1, 0x4C, 0x4C) : rgb(0xE5, 0x14, 0x00)},
        {kIndicatorDiagWarning, darkBack ? rgb(0xCC, 0xA7, 0x00) : rgb(0xBF, 0x88, 0x03)},
        {kIndicatorDiagInfo, darkBack ? rgb(0x37, 0x94, 0xFF) : rgb(0x1A, 0x85, 0xFF)},
        {kIndicatorDiagHint, darkBack ? rgb(0x80, 0x80, 0x80) : rgb(0x6C, 0x6C, 0x6C)},
    };
    for (const auto& [id, colour] : kDiagnostics) {
        const auto indic = static_cast<uptr_t>(id);
        view.send(SCI_INDICSETSTYLE, indic, id == kIndicatorDiagHint ? INDIC_DOTS : INDIC_SQUIGGLEPIXMAP);
        view.send(SCI_INDICSETFORE, indic, colour);
        view.send(SCI_INDICSETUNDER, indic, 0);
        view.send(SCI_INDICSETSTROKEWIDTH, indic, 150);
    }

    int foldMargin = kGrey;
    int foldMarginHi = kWhite;
    if (const Style* s = byName("Fold margin")) {
        foldMarginHi = s->fgColor;
        foldMargin = s->bgColor;
    }
    view.send(SCI_SETFOLDMARGINCOLOUR, 1, foldMargin & 0xFFFFFF);
    view.send(SCI_SETFOLDMARGINHICOLOUR, 1, foldMarginHi & 0xFFFFFF);

    // Margen de símbolos (marcadores): fondo de "Bookmark margin" o de "Line number margin"
    int bookmarkBack = kVeryLiteGrey;
    if (const Style* s = byName("Bookmark margin"))
        bookmarkBack = s->bgColor;
    else if (const Style* ln = byName("Line number margin"))
        bookmarkBack = ln->bgColor;
    view.send(SCI_SETMARGINTYPEN, kMarginSymbol, SC_MARGIN_COLOUR);
    view.send(SCI_SETMARGINBACKN, kMarginSymbol, bookmarkBack & 0xFFFFFF);
    // Marcador de línea: N++ usa un icono azul; aquí la forma "bookmark" de Scintilla
    view.send(SCI_MARKERDEFINE, kBookmarkMarkerId, SC_MARK_BOOKMARK);
    view.send(SCI_MARKERSETFORE, kBookmarkMarkerId, rgb(0x00, 0x40, 0x80));
    view.send(SCI_MARKERSETBACK, kBookmarkMarkerId, rgb(0x2E, 0x8B, 0xE6));

    // Margen e indicadores del historial de cambios, con los colores de N++
    int historyBack = kVeryLiteGrey;
    if (const Style* s = byName("Change History margin"))
        historyBack = s->bgColor;
    else if (const Style* ln = byName("Line number margin"))
        historyBack = ln->bgColor;
    view.send(SCI_SETMARGINTYPEN, kMarginChangeHistory, SC_MARGIN_COLOUR);
    view.send(SCI_SETMARGINBACKN, kMarginChangeHistory, historyBack & 0xFFFFFF);
    struct HistoryColour
    {
        const char* name;
        int marker;
        int insertion;
        int deletion;
        int fallback;
    };
    static constexpr HistoryColour kHistory[] = {
        {"Change History modified", SC_MARKNUM_HISTORY_MODIFIED, INDICATOR_HISTORY_MODIFIED_INSERTION,
         INDICATOR_HISTORY_MODIFIED_DELETION, rgb(0xFF, 0x80, 0x00)},
        {"Change History revert modified", SC_MARKNUM_HISTORY_REVERTED_TO_MODIFIED,
         INDICATOR_HISTORY_REVERTED_TO_MODIFIED_INSERTION, INDICATOR_HISTORY_REVERTED_TO_MODIFIED_DELETION,
         rgb(0xA0, 0xC0, 0x00)},
        {"Change History revert origin", SC_MARKNUM_HISTORY_REVERTED_TO_ORIGIN,
         INDICATOR_HISTORY_REVERTED_TO_ORIGIN_INSERTION, INDICATOR_HISTORY_REVERTED_TO_ORIGIN_DELETION,
         rgb(0x40, 0xA0, 0xBF)},
        {"Change History saved", SC_MARKNUM_HISTORY_SAVED, INDICATOR_HISTORY_SAVED_INSERTION,
         INDICATOR_HISTORY_SAVED_DELETION, rgb(0x00, 0xA0, 0x00)},
    };
    for (const HistoryColour& h : kHistory) {
        int fore = h.fallback;
        int back = h.fallback;
        if (const Style* s = byName(h.name)) {
            fore = s->fgColor;
            back = s->bgColor;
        }
        view.send(SCI_MARKERSETFORE, static_cast<uptr_t>(h.marker), fore & 0xFFFFFF);
        view.send(SCI_MARKERSETBACK, static_cast<uptr_t>(h.marker), back & 0xFFFFFF);
        view.send(SCI_INDICSETFORE, static_cast<uptr_t>(h.insertion), fore & 0xFFFFFF);
        view.send(SCI_INDICSETFORE, static_cast<uptr_t>(h.deletion), fore & 0xFFFFFF);
    }

    // Marcadores de plegado, estilo "box" (el de N++ por defecto)
    int foldFore = kWhite;
    int foldBack = kGrey;
    int foldActive = kRed;
    if (const Style* s = byName("Fold")) {
        foldFore = s->bgColor;
        foldBack = s->fgColor;
    }
    if (const Style* s = byName("Fold active"))
        foldActive = s->fgColor;
    // ScintillaEditView::_markersArray: una fila por estilo (simple, flecha, círculo, caja)
    static constexpr int kFolderMarkers[] = {SC_MARKNUM_FOLDEROPEN, SC_MARKNUM_FOLDER, SC_MARKNUM_FOLDERSUB,
                                             SC_MARKNUM_FOLDERTAIL, SC_MARKNUM_FOLDEREND, SC_MARKNUM_FOLDEROPENMID,
                                             SC_MARKNUM_FOLDERMIDTAIL};
    static constexpr int kFolderSymbols[4][7] = {
        {SC_MARK_MINUS, SC_MARK_PLUS, SC_MARK_EMPTY, SC_MARK_EMPTY, SC_MARK_EMPTY, SC_MARK_EMPTY, SC_MARK_EMPTY},
        {SC_MARK_ARROWDOWN, SC_MARK_ARROW, SC_MARK_EMPTY, SC_MARK_EMPTY, SC_MARK_EMPTY, SC_MARK_EMPTY, SC_MARK_EMPTY},
        {SC_MARK_CIRCLEMINUS, SC_MARK_CIRCLEPLUS, SC_MARK_VLINE, SC_MARK_LCORNERCURVE, SC_MARK_CIRCLEPLUSCONNECTED,
         SC_MARK_CIRCLEMINUSCONNECTED, SC_MARK_TCORNERCURVE},
        {SC_MARK_BOXMINUS, SC_MARK_BOXPLUS, SC_MARK_VLINE, SC_MARK_LCORNER, SC_MARK_BOXPLUSCONNECTED,
         SC_MARK_BOXMINUSCONNECTED, SC_MARK_TCORNER},
    };
    const int styleRow = _options.folderStyle == FolderStyle::None ? 3 : static_cast<int>(_options.folderStyle) - 1;
    for (std::size_t i = 0; i < 7; ++i) {
        const auto m = static_cast<uptr_t>(kFolderMarkers[i]);
        view.send(SCI_MARKERDEFINE, m, kFolderSymbols[styleRow][i]);
        view.send(SCI_MARKERSETFORE, m, foldFore & 0xFFFFFF);
        view.send(SCI_MARKERSETBACK, m, foldBack & 0xFFFFFF);
        view.send(SCI_MARKERSETBACKSELECTED, m, foldActive & 0xFFFFFF);
    }
    view.send(SCI_MARKERENABLEHIGHLIGHT, 1);

    const Style* ws = byName("White space symbol");
    view.send(SCI_SETWHITESPACEFORE, 1, (ws ? ws->fgColor : kBlack) & 0xFFFFFF);
}

} // namespace edita
