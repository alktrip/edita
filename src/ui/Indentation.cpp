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


#include "ui/Indentation.h"

#include "ui/EditorView.h"

#include <SciLexer.h>

#include <cstring>

namespace edita {

namespace {

bool isNewLine(const EditorView& view, int ch)
{
    const sptr_t eol = view.sci(SCI_GETEOLMODE);
    return ((eol == SC_EOL_CRLF || eol == SC_EOL_LF) && ch == '\n') || (eol == SC_EOL_CR && ch == '\r');
}

sptr_t lineLength(const EditorView& view, sptr_t line)
{
    return view.sci(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(line)) - view.sci(SCI_POSITIONFROMLINE, static_cast<uptr_t>(line));
}

sptr_t lineIndent(const EditorView& view, sptr_t line)
{
    return view.sci(SCI_GETLINEINDENTATION, static_cast<uptr_t>(line));
}

// La línea anterior que no está vacía
sptr_t previousNonEmptyLine(const EditorView& view, sptr_t line)
{
    while (line >= 0 && lineLength(view, line) == 0)
        --line;
    return line;
}

// ¿La expresión regular ocupa desde algún punto hasta el final de la línea?
bool lineEndsWithMatch(const EditorView& view, sptr_t line, const char* expr)
{
    const sptr_t start = view.sci(SCI_POSITIONFROMLINE, static_cast<uptr_t>(line));
    const sptr_t end = view.sci(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(line));
    view.sci(SCI_SETSEARCHFLAGS, SCFIND_REGEXP | SCFIND_POSIX);
    view.sci(SCI_SETTARGETRANGE, static_cast<uptr_t>(start), end);
    const sptr_t found = view.sends(SCI_SEARCHINTARGET, std::strlen(expr), expr);
    return found >= 0 && view.sci(SCI_GETTARGETEND) == end;
}

// Notepad_plus::isConditionExprLine: "if (…)", "for (…)", "while (…)" o "else" sin llave
bool isConditionExprLine(const EditorView& view, sptr_t line)
{
    if (line < 0 || line > view.sci(SCI_GETLINECOUNT))
        return false;
    return lineEndsWithMatch(view, line, "((else[ \t]+)?if|for|while)[ \t]*[(].*[)][ \t]*|else[ \t]*");
}

// Notepad_plus::findMachedBracePos hacia atrás
sptr_t findMatchedBraceBackward(const EditorView& view, sptr_t start, char target, char matched)
{
    int balance = 0;
    for (sptr_t i = start; i >= 0; --i) {
        const auto c = static_cast<char>(view.sci(SCI_GETCHARAT, static_cast<uptr_t>(i)));
        if (c == target) {
            if (balance == 0)
                return i;
            --balance;
        } else if (c == matched) {
            ++balance;
        }
    }
    return -1;
}

bool isCLike(LangType lang)
{
    switch (lang) {
    case LangType::C: case LangType::Cpp: case LangType::Java: case LangType::CSharp: case LangType::ObjC:
    case LangType::Php: case LangType::JsEmbedded: case LangType::JavaScript: case LangType::Jsp: case LangType::Css:
    case LangType::Perl: case LangType::Rust: case LangType::PowerShell: case LangType::Json: case LangType::Json5:
    case LangType::TypeScript: case LangType::Go: case LangType::Swift:
        return true;
    default:
        return false;
    }
}

void basicIndent(EditorView& view, sptr_t curLine)
{
    const sptr_t prevLine = previousNonEmptyLine(view, curLine - 1);
    const sptr_t indent = prevLine >= 0 ? lineIndent(view, prevLine) : 0;
    if (indent > 0)
        setLineIndent(view, curLine, indent);
}

} // namespace

void setLineIndent(EditorView& view, sptr_t line, sptr_t indent)
{
    if (indent < 0)
        indent = 0;
    if (view.sci(SCI_GETSELECTIONS) != 1) {
        view.sci(SCI_SETLINEINDENTATION, static_cast<uptr_t>(line), indent);
        return;
    }
    const sptr_t anchor = view.sci(SCI_GETANCHOR);
    const sptr_t caret = view.sci(SCI_GETCURRENTPOS);
    const sptr_t before = view.sci(SCI_GETLINEINDENTPOSITION, static_cast<uptr_t>(line));
    view.sci(SCI_SETLINEINDENTATION, static_cast<uptr_t>(line), indent);
    const sptr_t after = view.sci(SCI_GETLINEINDENTPOSITION, static_cast<uptr_t>(line));
    const sptr_t diff = after - before;
    auto adjust = [&](sptr_t pos) {
        if (diff > 0)
            return pos >= before ? pos + diff : pos;
        if (diff < 0 && pos >= after)
            return pos >= before ? pos + diff : after;
        return pos;
    };
    view.sci(SCI_SETSEL, static_cast<uptr_t>(adjust(anchor)), adjust(caret));
}

void maintainIndentation(EditorView& view, LangType lang, AutoIndent mode, int ch)
{
    if (mode == AutoIndent::None)
        return;
    const sptr_t eol = view.sci(SCI_GETEOLMODE);
    const sptr_t curLine = view.sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(view.sci(SCI_GETCURRENTPOS)));
    const sptr_t prevLine = curLine - 1;
    const sptr_t tabWidth = view.sci(SCI_GETTABWIDTH);

    // Intro al principio de una línea vacía: no se toca
    if (isNewLine(view, ch) && prevLine >= 0 && lineLength(view, prevLine) == 0)
        return;

    if (mode == AutoIndent::Basic) {
        if (isNewLine(view, ch))
            basicIndent(view, curLine);
        return;
    }

    if (isCLike(lang)) {
        if (isNewLine(view, ch)) {
            const sptr_t prev = previousNonEmptyLine(view, prevLine);
            const sptr_t indent = prev >= 0 ? lineIndent(view, prev) : 0;
            const sptr_t curPos = view.sci(SCI_GETCURRENTPOS);
            const auto prevChar = static_cast<char>(view.sci(SCI_GETCHARAT, static_cast<uptr_t>(curPos - (eol == SC_EOL_CRLF ? 3 : 2))));
            const auto nextChar = static_cast<char>(view.sci(SCI_GETCHARAT, static_cast<uptr_t>(curPos)));
            if (prevChar == '{') {
                if (nextChar == '}') {
                    // "{|}" + Intro: la llave de cierre baja a su propia línea
                    const char* eolChars = eol == SC_EOL_CRLF ? "\r\n" : (eol == SC_EOL_LF ? "\n" : "\r");
                    view.sends(SCI_INSERTTEXT, static_cast<uptr_t>(curPos), eolChars);
                    setLineIndent(view, curLine + 1, indent);
                }
                setLineIndent(view, curLine, indent + tabWidth);
            } else if (nextChar == '{') {
                setLineIndent(view, curLine, indent);
            } else if (lang == LangType::Perl || lang == LangType::Rust || lang == LangType::PowerShell
                       || lang == LangType::Json || lang == LangType::Json5) {
                // Sin estructuras de una línea sin llaves
                setLineIndent(view, curLine, indent);
            } else if (isConditionExprLine(view, prevLine)) {
                setLineIndent(view, curLine, indent + tabWidth);
            } else if (indent > 0) {
                if (prevLine > 0 && isConditionExprLine(view, prevLine - 1))
                    setLineIndent(view, curLine, indent - tabWidth);
                else
                    setLineIndent(view, curLine, indent);
            }
        } else if (ch == '{') {
            // Solo si no hay nada delante en la línea: alineada con la anterior
            const sptr_t lineStart = view.sci(SCI_POSITIONFROMLINE, static_cast<uptr_t>(curLine));
            const sptr_t curPos = view.sci(SCI_GETCURRENTPOS);
            for (sptr_t i = curPos - 2; i > 0 && i >= lineStart; --i) {
                const auto c = static_cast<char>(view.sci(SCI_GETCHARAT, static_cast<uptr_t>(i)));
                if (c != ' ' && c != '\t')
                    return;
            }
            const sptr_t prev = previousNonEmptyLine(view, prevLine);
            sptr_t indent = 0;
            if (prev >= 0) {
                indent = lineIndent(view, prev);
                // La anterior ya abre una llave: un nivel más
                if (lineEndsWithMatch(view, prev, "[ \t]*\\{.*"))
                    indent += tabWidth;
            }
            setLineIndent(view, curLine, indent);
        } else if (ch == '}') {
            // Alinear "}" con la línea de su "{"
            sptr_t start = view.sci(SCI_GETCURRENTPOS);
            if (start != 0)
                start -= 1;
            const sptr_t found = findMatchedBraceBackward(view, start - 1, '{', '}');
            if (found == -1)
                return;
            const sptr_t braceLine = view.sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(found));
            if (braceLine != curLine)
                setLineIndent(view, curLine, lineIndent(view, braceLine));
        }
        return;
    }

    if (lang == LangType::Python) {
        if (!isNewLine(view, ch))
            return;
        const sptr_t prev = previousNonEmptyLine(view, prevLine);
        if (prev < 0)
            return;
        const sptr_t indent = lineIndent(view, prev);
        // Dos puntos al final (o antes de un comentario) que sean un operador, no texto
        const sptr_t start = view.sci(SCI_POSITIONFROMLINE, static_cast<uptr_t>(prev));
        const sptr_t end = view.sci(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(prev));
        view.sci(SCI_SETSEARCHFLAGS, SCFIND_REGEXP | SCFIND_POSIX);
        view.sci(SCI_SETTARGETRANGE, static_cast<uptr_t>(start), end);
        const char colonExpr[] = ":[ \t]*(#|$)";
        const sptr_t colon = view.sends(SCI_SEARCHINTARGET, std::strlen(colonExpr), colonExpr);
        // Puede que el lexer aún no haya coloreado la línea
        view.sci(SCI_COLOURISE, static_cast<uptr_t>(start), end);
        if (colon >= 0 && view.sci(SCI_GETSTYLEINDEXAT, static_cast<uptr_t>(colon)) == SCE_P_OPERATOR)
            setLineIndent(view, curLine, indent + tabWidth);
        else if (indent > 0)
            setLineIndent(view, curLine, indent);
        return;
    }

    if (isNewLine(view, ch))
        basicIndent(view, curLine);
}

} // namespace edita
