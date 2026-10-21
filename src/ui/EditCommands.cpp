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

#include "ui/EditCommands.h"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <tuple>

namespace edita {

namespace {

using View = ScintillaEditBase;

constexpr uptr_t U(sptr_t value)
{
    return static_cast<uptr_t>(value);
}

std::string textRange(View& v, sptr_t start, sptr_t end)
{
    if (end <= start)
        return {};
    std::string text(static_cast<std::size_t>(end - start) + 1, '\0');
    Sci_TextRangeFull range{{start, end}, text.data()};
    v.send(SCI_GETTEXTRANGEFULL, 0, reinterpret_cast<sptr_t>(&range));
    text.resize(static_cast<std::size_t>(end - start));
    return text;
}

// ScintillaEditView::getEOLString
std::string eolString(View& v)
{
    switch (v.send(SCI_GETEOLMODE)) {
    case SC_EOL_CRLF: return "\r\n";
    case SC_EOL_CR: return "\r";
    default: return "\n";
    }
}

void replaceRange(View& v, sptr_t start, sptr_t end, std::string_view text)
{
    v.send(SCI_SETTARGETRANGE, U(start), end);
    v.send(SCI_REPLACETARGET, text.size(), reinterpret_cast<sptr_t>(text.data()));
}

void insertText(View& v, sptr_t pos, const std::string& text)
{
    v.sends(SCI_INSERTTEXT, U(pos), text.c_str());
}

sptr_t lineStart(View& v, sptr_t line)
{
    return v.send(SCI_POSITIONFROMLINE, U(line));
}

sptr_t lineEnd(View& v, sptr_t line)
{
    return v.send(SCI_GETLINEENDPOSITION, U(line));
}

sptr_t lastLine(View& v)
{
    return v.send(SCI_GETLINECOUNT) - 1;
}

bool isRectangular(View& v)
{
    const sptr_t mode = v.send(SCI_GETSELECTIONMODE);
    return mode == SC_SEL_RECTANGLE || mode == SC_SEL_THIN;
}

// Las líneas [first, last] sin fin de línea
std::vector<std::string> readLines(View& v, sptr_t first, sptr_t last)
{
    std::vector<std::string> lines;
    lines.reserve(static_cast<std::size_t>(last - first + 1));
    for (sptr_t line = first; line <= last; ++line)
        lines.push_back(textRange(v, lineStart(v, line), lineEnd(v, line)));
    return lines;
}

// Sustituye las líneas [first, last] (sin el último fin de línea) por `lines`
// unidas con el fin de línea del documento. false si no cambia nada.
bool writeLines(View& v, sptr_t first, sptr_t last, const std::vector<std::string>& lines)
{
    const std::string eol = eolString(v);
    std::string joined;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i > 0)
            joined += eol;
        joined += lines[i];
    }
    sptr_t start = lineStart(v, first);
    sptr_t end = lineEnd(v, last);
    // Sin líneas (todas quitadas) desaparece también un fin de línea: el que las
    // separaba de la siguiente o, al final del documento, el de la anterior
    if (lines.empty() && last < lastLine(v))
        end = lineStart(v, last + 1);
    else if (lines.empty() && first > 0)
        start = lineEnd(v, first - 1);
    if (textRange(v, start, end) == joined)
        return false;
    replaceRange(v, start, end, joined);
    return true;
}

// _strnicmp(text, prefix, n): las n primeras letras iguales sin distinguir mayúsculas (ASCII)
bool startsWithNoCase(std::string_view text, std::string_view prefix, std::size_t n)
{
    if (text.size() < n || prefix.size() < n)
        return false;
    for (std::size_t i = 0; i < n; ++i) {
        if (std::tolower(static_cast<unsigned char>(text[i])) != std::tolower(static_cast<unsigned char>(prefix[i])))
            return false;
    }
    return true;
}

// ScintillaEditView::searchInTarget: hacia atrás si from > to
sptr_t searchInTarget(View& v, const std::string& text, sptr_t from, sptr_t to)
{
    v.send(SCI_SETTARGETRANGE, U(from), to);
    return v.send(SCI_SEARCHINTARGET, text.size(), reinterpret_cast<sptr_t>(text.data()));
}

bool isFoldExpanded(View& v, sptr_t line)
{
    return v.send(SCI_GETFOLDEXPANDED, U(line)) != 0;
}

// ScintillaEditView::fold
void foldLine(View& v, sptr_t line, bool expand)
{
    if (v.send(SCI_GETENDSTYLED) < v.send(SCI_GETTEXTLENGTH))
        v.send(SCI_COLOURISE, 0, -1);
    sptr_t header = line;
    if (!(v.send(SCI_GETFOLDLEVEL, U(line)) & SC_FOLDLEVELHEADERFLAG)) {
        header = v.send(SCI_GETFOLDPARENT, U(line));
        if (header == -1)
            return;
    }
    if (isFoldExpanded(v, header) != expand)
        v.send(SCI_TOGGLEFOLD, U(header));
}

} // namespace

std::pair<sptr_t, sptr_t> selectionLinesRange(View& v)
{
    const sptr_t start = v.send(SCI_GETSELECTIONSTART);
    const sptr_t end = v.send(SCI_GETSELECTIONEND);
    const sptr_t line1 = v.send(SCI_LINEFROMPOSITION, U(start));
    sptr_t line2 = v.send(SCI_LINEFROMPOSITION, U(end));
    if (line1 != line2 && lineStart(v, line2) == end)
        --line2;
    return {line1, line2};
}

// --- Líneas ----------------------------------------------------------------------------------

sptr_t sortLines(View& v, const SortRequest& request)
{
    sptr_t fromLine = 0;
    sptr_t toLine = 0;
    std::vector<std::string> keys;
    bool hasLineSelection = false;

    if (v.send(SCI_GETSELECTIONS) > 1) {
        if (!v.send(SCI_SELECTIONISRECTANGLE) && v.send(SCI_GETSELECTIONMODE) != SC_SEL_THIN)
            return 0; // varias selecciones normales: N++ no hace nada
        const sptr_t anchorLine = v.send(SCI_LINEFROMPOSITION, U(v.send(SCI_GETRECTANGULARSELECTIONANCHOR)));
        const sptr_t caretLine = v.send(SCI_LINEFROMPOSITION, U(v.send(SCI_GETRECTANGULARSELECTIONCARET)));
        fromLine = std::min(anchorLine, caretLine);
        toLine = std::max(anchorLine, caretLine);
        // Clave de cada línea: el texto de las columnas; una selección fina (sin
        // ancho) toma desde esa columna hasta el final de la línea, como N++
        keys.assign(static_cast<std::size_t>(toLine - fromLine + 1), std::string());
        for (sptr_t n = 0; n < v.send(SCI_GETSELECTIONS); ++n) {
            const sptr_t s = v.send(SCI_GETSELECTIONNSTART, U(n));
            const sptr_t e = v.send(SCI_GETSELECTIONNEND, U(n));
            const sptr_t line = v.send(SCI_LINEFROMPOSITION, U(s));
            if (line < fromLine || line > toLine)
                continue;
            keys[static_cast<std::size_t>(line - fromLine)] = textRange(v, s, e > s ? e : lineEnd(v, line));
        }
    } else if (v.send(SCI_GETSELECTIONSTART) != v.send(SCI_GETSELECTIONEND)) {
        hasLineSelection = true;
        std::tie(fromLine, toLine) = selectionLinesRange(v);
        if (fromLine == toLine)
            return 0; // una sola línea no se ordena
    } else {
        toLine = lastLine(v);
    }
    if (fromLine >= toLine)
        return 0;

    std::vector<std::string> lines = readLines(v, fromLine, toLine);
    const std::ptrdiff_t failed = edita::sortLines(lines, keys, request);
    if (failed >= 0)
        return fromLine + failed + 1;

    v.send(SCI_BEGINUNDOACTION);
    writeLines(v, fromLine, toLine, lines);
    v.send(SCI_ENDUNDOACTION);
    if (hasLineSelection)
        v.send(SCI_SETSEL, U(lineStart(v, fromLine)), lineEnd(v, toLine));
    return 0;
}

void removeDuplicateLines(View& v, bool onlyConsecutive)
{
    sptr_t fromLine = 0;
    sptr_t toLine = lastLine(v);
    const bool hasSelection = v.send(SCI_GETSELECTIONSTART) != v.send(SCI_GETSELECTIONEND);
    if (hasSelection) {
        std::tie(fromLine, toLine) = selectionLinesRange(v);
        if (fromLine == toLine)
            return;
    }
    if (fromLine >= toLine)
        return;
    std::vector<std::string> lines = readLines(v, fromLine, toLine);
    const bool changed = onlyConsecutive ? removeConsecutiveDuplicateLines(lines) : removeAnyDuplicateLines(lines);
    if (!changed)
        return;
    v.send(SCI_BEGINUNDOACTION);
    writeLines(v, fromLine, toLine, lines);
    v.send(SCI_ENDUNDOACTION);
}

void removeEmptyLines(View& v, bool alsoBlank)
{
    sptr_t fromLine = 0;
    sptr_t toLine = lastLine(v);
    if (v.send(SCI_GETSELECTIONSTART) != v.send(SCI_GETSELECTIONEND))
        std::tie(fromLine, toLine) = selectionLinesRange(v);
    std::vector<std::string> lines = readLines(v, fromLine, toLine);
    if (!edita::removeEmptyLines(lines, alsoBlank))
        return;
    v.send(SCI_BEGINUNDOACTION);
    writeLines(v, fromLine, toLine, lines);
    v.send(SCI_ENDUNDOACTION);
}

void insertBlankLine(View& v, bool above)
{
    // ScintillaEditView::insertNewLineAboveCurrentLine / insertNewLineBelowCurrentLine
    const std::string eol = eolString(v);
    const sptr_t current = v.send(SCI_LINEFROMPOSITION, U(v.send(SCI_GETCURRENTPOS)));
    if (above) {
        insertText(v, current == 0 ? 0 : lineStart(v, current) - static_cast<sptr_t>(eol.size()), eol);
        v.send(SCI_SETEMPTYSELECTION, U(lineStart(v, current)));
    } else {
        if (current == lastLine(v))
            v.send(SCI_APPENDTEXT, eol.size(), reinterpret_cast<sptr_t>(eol.data()));
        else
            insertText(v, lineEnd(v, current) + static_cast<sptr_t>(eol.size()), eol);
        v.send(SCI_SETEMPTYSELECTION, U(lineStart(v, current + 1)));
    }
}

void splitLines(View& v)
{
    if (v.send(SCI_GETSELECTIONS) != 1)
        return;
    const auto [first, last] = selectionLinesRange(v);
    v.send(SCI_SETSELECTION, U(lineEnd(v, last)), lineStart(v, first));
    v.send(SCI_TARGETFROMSELECTION);
    const sptr_t edgeMode = v.send(SCI_GETEDGEMODE);
    if (edgeMode == EDGE_NONE) {
        v.send(SCI_LINESSPLIT, 0);
        return;
    }
    const sptr_t textWidth = v.sends(SCI_TEXTWIDTH, STYLE_DEFAULT, "P");
    sptr_t edgeColumn = v.send(SCI_GETEDGECOLUMN);
    if (edgeMode == EDGE_MULTILINE) {
        // La última línea de margen que haya definido el usuario
        for (uptr_t which = 0;; ++which) {
            const sptr_t column = v.send(SCI_GETMULTIEDGECOLUMN, which);
            if (column < 0)
                break;
            edgeColumn = column;
        }
    }
    v.send(SCI_LINESSPLIT, U(textWidth * (edgeColumn + 1)));
}

void joinLines(View& v)
{
    const auto [first, last] = selectionLinesRange(v);
    if (first == last)
        return;
    v.send(SCI_SETSELECTION, U(lineEnd(v, last)), lineStart(v, first));
    v.send(SCI_TARGETFROMSELECTION);
    v.send(SCI_LINESJOIN);
}

// --- Espacios ------------------------------------------------------------------------------

void trimLines(View& v, bool leading, bool trailing)
{
    if (isRectangular(v))
        return; // N++ no admite la selección rectangular
    const sptr_t anchor = v.send(SCI_GETANCHOR);
    const sptr_t caret = v.send(SCI_GETCURRENTPOS);
    const bool entireDoc = anchor == caret;
    sptr_t fromLine = 0;
    sptr_t toLine = lastLine(v);
    if (!entireDoc)
        std::tie(fromLine, toLine) = selectionLinesRange(v);

    std::vector<std::string> lines = readLines(v, fromLine, toLine);
    for (std::string& line : lines) {
        if (trailing) {
            const std::size_t end = line.find_last_not_of(" \t");
            line.erase(end == std::string::npos ? 0 : end + 1);
        }
        if (leading)
            line.erase(0, std::min(line.find_first_not_of(" \t"), line.size()));
    }
    v.send(SCI_BEGINUNDOACTION);
    const bool changed = writeLines(v, fromLine, toLine, lines);
    v.send(SCI_ENDUNDOACTION);
    if (changed && !entireDoc)
        v.send(SCI_SETSEL, U(lineStart(v, fromLine)), lineEnd(v, toLine));
}

void eolToSpace(View& v)
{
    if (isRectangular(v))
        return;
    const bool entireDoc = v.send(SCI_GETANCHOR) == v.send(SCI_GETCURRENTPOS);
    v.send(entireDoc ? SCI_TARGETWHOLEDOCUMENT : SCI_TARGETFROMSELECTION);
    v.send(SCI_LINESJOIN);
}

void trimAll(View& v)
{
    v.send(SCI_BEGINUNDOACTION);
    const bool entireDoc = v.send(SCI_GETANCHOR) == v.send(SCI_GETCURRENTPOS);
    trimLines(v, true, true);
    if (entireDoc || v.send(SCI_GETANCHOR) != v.send(SCI_GETCURRENTPOS))
        eolToSpace(v);
    v.send(SCI_ENDUNDOACTION);
}

void convertTabs(View& v, TabConversion conversion)
{
    if (isRectangular(v))
        return;
    const int tabWidth = static_cast<int>(std::max<sptr_t>(1, v.send(SCI_GETTABWIDTH)));
    const sptr_t caret = v.send(SCI_GETCURRENTPOS);
    const sptr_t anchor = v.send(SCI_GETANCHOR);
    const sptr_t caretLine = v.send(SCI_LINEFROMPOSITION, U(caret));
    const bool entireDoc = anchor == caret;
    sptr_t fromLine = 0;
    sptr_t toLine = lastLine(v);
    if (!entireDoc)
        std::tie(fromLine, toLine) = selectionLinesRange(v);

    std::ptrdiff_t caretOffset = caret - lineStart(v, caretLine);
    std::vector<sptr_t> folded;
    bool changed = false;
    v.send(SCI_BEGINUNDOACTION);
    for (sptr_t line = fromLine; line <= toLine; ++line) {
        const sptr_t start = lineStart(v, line);
        const sptr_t end = lineEnd(v, line);
        const std::string source = textRange(v, start, end);
        const std::string converted = edita::convertTabs(source, tabWidth, conversion, line == caretLine ? &caretOffset : nullptr);
        if ((v.send(SCI_GETFOLDLEVEL, U(line)) & SC_FOLDLEVELHEADERFLAG) && !isFoldExpanded(v, line))
            folded.push_back(line);
        if (converted != source) {
            replaceRange(v, start, end, converted);
            changed = true;
        }
    }
    v.send(SCI_ENDUNDOACTION);

    if (!changed)
        return;
    if (entireDoc)
        v.send(SCI_GOTOPOS, U(lineStart(v, caretLine) + caretOffset));
    else
        v.send(SCI_SETSEL, U(lineStart(v, fromLine)), lineEnd(v, toLine));
    for (const sptr_t line : folded)
        foldLine(v, line, false);
}

// --- Convertir --------------------------------------------------------------------------

namespace {

// ScintillaEditView::caseConvertRange: devuelve cuánto cambió la longitud (en bytes)
sptr_t convertRange(View& v, sptr_t start, sptr_t end, TextCase textCase, unsigned seed)
{
    if (end <= start)
        return 0;
    const std::string original = textRange(v, start, end);
    const std::string converted = changeCase(QString::fromStdString(original), textCase, seed).toStdString();
    if (converted != original)
        replaceRange(v, start, end, converted);
    return static_cast<sptr_t>(converted.size()) - static_cast<sptr_t>(original.size());
}

} // namespace

void convertCase(View& v, TextCase textCase, unsigned seed)
{
    const sptr_t selections = v.send(SCI_GETSELECTIONS);
    if (selections > 1) {
        // Varias selecciones o modo columna: se convierten en orden y se vuelven a seleccionar
        struct Range
        {
            sptr_t anchor;
            sptr_t caret;
        };
        std::vector<Range> ranges;
        for (sptr_t n = 0; n < selections; ++n)
            ranges.push_back({v.send(SCI_GETSELECTIONNANCHOR, U(n)), v.send(SCI_GETSELECTIONNCARET, U(n))});
        const bool rectangular = isRectangular(v);
        const sptr_t rectAnchor = v.send(SCI_GETRECTANGULARSELECTIONANCHOR);
        const sptr_t rectCaret = v.send(SCI_GETRECTANGULARSELECTIONCARET);
        const sptr_t rectAnchorVs = v.send(SCI_GETRECTANGULARSELECTIONANCHORVIRTUALSPACE);
        const sptr_t rectCaretVs = v.send(SCI_GETRECTANGULARSELECTIONCARETVIRTUALSPACE);
        std::vector<std::size_t> order(ranges.size());
        for (std::size_t i = 0; i < order.size(); ++i)
            order[i] = i;
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            return std::min(ranges[a].anchor, ranges[a].caret) < std::min(ranges[b].anchor, ranges[b].caret);
        });

        v.send(SCI_BEGINUNDOACTION);
        sptr_t delta = 0;
        for (const std::size_t i : order) {
            Range& r = ranges[i];
            r.anchor += delta;
            r.caret += delta;
            const sptr_t start = std::min(r.anchor, r.caret);
            const sptr_t end = std::max(r.anchor, r.caret);
            const sptr_t change = convertRange(v, start, end, textCase, seed);
            (r.anchor > r.caret ? r.anchor : r.caret) += change;
            delta += change;
        }
        v.send(SCI_ENDUNDOACTION);

        if (rectangular && delta == 0) {
            v.send(SCI_SETRECTANGULARSELECTIONANCHOR, U(rectAnchor));
            v.send(SCI_SETRECTANGULARSELECTIONCARET, U(rectCaret));
            v.send(SCI_SETRECTANGULARSELECTIONANCHORVIRTUALSPACE, U(rectAnchorVs));
            v.send(SCI_SETRECTANGULARSELECTIONCARETVIRTUALSPACE, U(rectCaretVs));
        } else {
            v.send(SCI_SETSELECTION, U(ranges[0].caret), ranges[0].anchor);
            for (std::size_t i = 1; i < ranges.size(); ++i)
                v.send(SCI_ADDSELECTION, U(ranges[i].caret), ranges[i].anchor);
        }
        return;
    }

    const sptr_t start = v.send(SCI_GETSELECTIONSTART);
    const sptr_t end = v.send(SCI_GETSELECTIONEND);
    if (start < end) {
        const sptr_t change = convertRange(v, start, end, textCase, seed);
        v.send(SCI_SETSEL, U(start), end + change);
    }
}

// --- Comentarios --------------------------------------------------------------------------

bool lineComment(View& v, const CommentSymbols& symbols, CommentMode mode)
{
    // Notepad_plus::doBlockComment
    if (v.send(SCI_GETREADONLY))
        return false;

    // Modo normal: símbolo de línea. Modo avanzado: sin él, los de bloque en cada línea.
    bool advanced = false;
    if (symbols.line.empty()) {
        if (symbols.start.empty() || symbols.end.empty())
            return false;
        if (mode == CommentMode::Uncomment)
            return undoStreamComment(v, symbols, false);
        advanced = true;
    }

    std::string comment;
    std::string advStart;
    std::string advEnd;
    if (!advanced) {
        comment = symbols.line;
        if (!symbols.noSpaceAfterLine)
            comment += ' ';
    } else {
        advStart = symbols.start + ' ';
        advEnd = ' ' + symbols.end;
    }
    const auto commentLength = static_cast<sptr_t>(comment.size());
    const auto advStartLength = static_cast<sptr_t>(advStart.size());
    const auto advEndLength = static_cast<sptr_t>(advEnd.size());

    sptr_t selectionStart = v.send(SCI_GETSELECTIONSTART);
    sptr_t selectionEnd = v.send(SCI_GETSELECTIONEND);
    const sptr_t caretPosition = v.send(SCI_GETCURRENTPOS);
    // ¿Está el cursor al principio del bloque seleccionado?
    const bool moveCaret = caretPosition < selectionEnd;
    const sptr_t selStartLine = v.send(SCI_LINEFROMPOSITION, U(selectionStart));
    sptr_t selEndLine = v.send(SCI_LINEFROMPOSITION, U(selectionEnd));
    // El salto de línea es parte de la última línea seleccionada
    if (selEndLine > selStartLine && selectionEnd == lineStart(v, selEndLine))
        --selEndLine;
    int uncommented = 0;

    v.send(SCI_BEGINUNDOACTION);
    for (sptr_t i = selStartLine; i <= selEndLine; ++i) {
        const sptr_t start = lineStart(v, i);
        sptr_t indent = v.send(SCI_GETLINEINDENTPOSITION, U(i));
        const sptr_t end = lineEnd(v, i);
        // Las líneas vacías no se comentan, salvo en BaanC
        if (indent == end && !symbols.commentEmptyLines)
            continue;
        if (symbols.avoidIndent)
            indent = start;
        const std::string text = textRange(v, indent, end);

        if (mode != CommentMode::Comment) {
            if (!advanced) {
                // Sin distinguir mayúsculas: "REM" y "rem" en los .bat
                const std::size_t compared = symbols.noSpaceAfterLine ? comment.size() : comment.size() - 1;
                if (startsWithNoCase(text, comment, compared)) {
                    const sptr_t len = (text.size() >= comment.size() && text[comment.size() - 1] == ' ')
                        ? commentLength
                        : static_cast<sptr_t>(compared);
                    v.send(SCI_DELETERANGE, U(indent), len);
                    if (i == selStartLine) {
                        if (selectionStart > indent + len)
                            selectionStart -= len;
                        else if (selectionStart > indent)
                            selectionStart = indent;
                    }
                    if (i == selEndLine) {
                        if (selectionEnd > indent + len)
                            selectionEnd -= len;
                        else if (selectionEnd > indent) {
                            selectionEnd = indent;
                            if (indent == start && i != selStartLine)
                                ++selectionEnd; // no quedarse en el salto de línea
                        }
                    } else {
                        selectionEnd -= len;
                    }
                    ++uncommented;
                    continue;
                }
            } else {
                const auto textLength = static_cast<sptr_t>(text.size());
                const bool endMatches = textLength >= advEndLength - 1
                    && startsWithNoCase(std::string_view(text).substr(static_cast<std::size_t>(textLength - advEndLength + 1)),
                                        std::string_view(advEnd).substr(1), advEnd.size() - 1);
                if (startsWithNoCase(text, advStart, advStart.size() - 1) && endMatches
                    && textLength >= advStartLength - 1 + advEndLength - 1) {
                    const sptr_t startLen = (textLength >= advStartLength && text[advStart.size() - 1] == ' ') ? advStartLength : advStartLength - 1;
                    const sptr_t endLen = (textLength >= advEndLength && text[static_cast<std::size_t>(textLength - advEndLength)] == ' ')
                        ? advEndLength
                        : advEndLength - 1;
                    v.send(SCI_DELETERANGE, U(indent), startLen);
                    v.send(SCI_DELETERANGE, U(end - startLen - endLen), endLen);
                    if (i == selStartLine) {
                        if (selectionStart > end - endLen)
                            selectionStart = end - startLen - endLen;
                        else if (selectionStart > indent + startLen)
                            selectionStart -= startLen;
                        else if (selectionStart > indent)
                            selectionStart = indent;
                    }
                    if (i == selEndLine) {
                        if (selectionEnd > end)
                            selectionEnd -= startLen + endLen;
                        else if (selectionEnd > end - endLen)
                            selectionEnd = end - startLen - endLen;
                        else if (selectionEnd > indent + startLen)
                            selectionEnd -= startLen;
                        else if (selectionEnd > indent) {
                            selectionEnd = indent;
                            if (indent == start && i != selStartLine)
                                ++selectionEnd;
                        }
                    } else {
                        selectionEnd -= startLen + endLen;
                    }
                    ++uncommented;
                    continue;
                }
            }
        }

        if (mode != CommentMode::Uncomment) {
            if (!advanced) {
                insertText(v, indent, comment);
                if (i == selStartLine && selectionStart >= indent)
                    selectionStart += commentLength;
                if (i == selEndLine) {
                    if (selectionEnd >= indent)
                        selectionEnd += commentLength;
                } else {
                    selectionEnd += commentLength;
                }
            } else {
                insertText(v, indent, advStart);
                insertText(v, end + advStartLength, advEnd);
                if (i == selStartLine && selectionStart >= indent)
                    selectionStart += advStartLength;
                if (i == selEndLine) {
                    if (selectionEnd > end)
                        selectionEnd += advStartLength + advEndLength;
                    else if (selectionEnd >= indent)
                        selectionEnd += advStartLength;
                } else {
                    selectionEnd += advStartLength + advEndLength;
                }
            }
        }
    }

    if (moveCaret) {
        // El cursor vuelve al principio del bloque
        v.send(SCI_GOTOPOS, U(selectionEnd));
        v.send(SCI_SETCURRENTPOS, U(selectionStart));
    } else {
        v.send(SCI_SETSEL, U(selectionStart), selectionEnd);
    }
    v.send(SCI_ENDUNDOACTION);

    // Si no había comentarios de línea que quitar, se prueba con los de bloque
    if (mode == CommentMode::Uncomment && uncommented == 0)
        return undoStreamComment(v, symbols, false);
    return true;
}

bool streamComment(View& v, const CommentSymbols& symbols)
{
    // Notepad_plus::doStreamComment
    if (v.send(SCI_GETREADONLY))
        return false;
    if (symbols.start.empty() || symbols.end.empty()) {
        // Sin comentario de bloque: el de línea
        return !symbols.line.empty() && lineComment(v, symbols, CommentMode::Comment);
    }
    const std::string startComment = symbols.start + ' ';
    const std::string endComment = ' ' + symbols.end;
    const auto startLength = static_cast<sptr_t>(startComment.size());

    sptr_t selectionStart = v.send(SCI_GETSELECTIONSTART);
    sptr_t selectionEnd = v.send(SCI_GETSELECTIONEND);
    const bool moveCaret = v.send(SCI_GETCURRENTPOS) < selectionEnd;
    if (selectionEnd <= selectionStart) {
        // Sin selección: la línea, sin su sangría
        const sptr_t line = v.send(SCI_LINEFROMPOSITION, U(selectionStart));
        selectionStart = v.send(SCI_GETLINEINDENTPOSITION, U(line));
        selectionEnd = lineEnd(v, line);
    }
    v.send(SCI_BEGINUNDOACTION);
    insertText(v, selectionStart, startComment);
    selectionEnd += startLength;
    selectionStart += startLength;
    insertText(v, selectionEnd, endComment);
    if (moveCaret) {
        v.send(SCI_GOTOPOS, U(selectionEnd));
        v.send(SCI_SETCURRENTPOS, U(selectionStart));
    } else {
        v.send(SCI_SETSEL, U(selectionStart), selectionEnd);
    }
    v.send(SCI_ENDUNDOACTION);
    return true;
}

bool undoStreamComment(View& v, const CommentSymbols& symbols, bool tryLineComment)
{
    // Notepad_plus::undoStreamComment: quita los comentarios de bloque alrededor o
    // dentro de la selección
    if (v.send(SCI_GETREADONLY))
        return false;
    if (symbols.start.empty() || symbols.end.empty())
        return !symbols.line.empty() && tryLineComment && lineComment(v, symbols, CommentMode::Uncomment);

    const std::string& startComment = symbols.start;
    const std::string& endComment = symbols.end;
    bool result = false;

    v.send(SCI_BEGINUNDOACTION);
    // Mientras quede algún comentario de bloque en la selección
    while (true) {
        const sptr_t selectionStart = v.send(SCI_GETSELECTIONSTART);
        const sptr_t selectionEnd = v.send(SCI_GETSELECTIONEND);
        const bool moveCaret = v.send(SCI_GETCURRENTPOS) < selectionEnd;
        const sptr_t docLength = v.send(SCI_GETLENGTH);

        v.send(SCI_SETSEARCHFLAGS, SCFIND_WORDSTART);
        struct Around
        {
            sptr_t startBefore, endBefore, startAfter, endAfter;
        };
        auto around = [&](sptr_t pos) {
            return Around{searchInTarget(v, startComment, pos, 0), searchInTarget(v, endComment, pos, 0),
                          searchInTarget(v, startComment, pos, docLength), searchInTarget(v, endComment, pos, docLength)};
        };
        // ¿Hay un comentario que rodea la posición?
        auto encloses = [](const Around& a) {
            return a.startBefore != -1 && a.endAfter != -1 && (a.endBefore == -1 || a.startBefore >= a.endBefore)
                && (a.startAfter == -1 || a.endAfter <= a.startAfter);
        };

        sptr_t posStartComment = -1;
        sptr_t posEndComment = -1;
        const Around atStart = around(selectionStart);
        if (encloses(atStart)) {
            posStartComment = atStart.startBefore;
            posEndComment = atStart.endAfter;
        } else {
            const Around atEnd = around(selectionEnd);
            if (encloses(atEnd)) {
                posStartComment = atEnd.startBefore;
                posEndComment = atEnd.endAfter;
            } else if (atStart.startAfter != -1 && atStart.startAfter < selectionEnd && atEnd.endBefore != -1
                       && atEnd.endBefore > selectionStart) {
                // Un comentario dentro de la selección: el primero
                posStartComment = atStart.startAfter;
                posEndComment = atStart.endAfter;
            } else {
                v.send(SCI_ENDUNDOACTION);
                return result;
            }
        }

        result = true;
        auto startLength = static_cast<sptr_t>(startComment.size());
        auto endLength = static_cast<sptr_t>(endComment.size());
        // Primero el final (así no se mueve el principio), con el espacio que lo precede
        if (posEndComment > 0 && textRange(v, posEndComment - 1, posEndComment) == " ") {
            ++endLength;
            --posEndComment;
        }
        v.send(SCI_DELETERANGE, U(posEndComment), endLength);
        if (textRange(v, posStartComment + startLength, posStartComment + startLength + 1) == " ")
            ++startLength;
        v.send(SCI_DELETERANGE, U(posStartComment), startLength);

        sptr_t startMove = 0;
        if (selectionStart > posStartComment)
            startMove = selectionStart >= posStartComment + startLength ? -startLength : -(selectionStart - posStartComment);
        sptr_t endMove = 0;
        if (selectionEnd >= posEndComment + endLength)
            endMove = -(startLength + endLength);
        else if (selectionEnd <= posEndComment)
            endMove = -startLength;
        else
            endMove = -(startLength + (selectionEnd - posEndComment));

        if (moveCaret) {
            v.send(SCI_GOTOPOS, U(selectionEnd + endMove));
            v.send(SCI_SETCURRENTPOS, U(selectionStart + startMove));
        } else {
            v.send(SCI_SETSEL, U(selectionStart + startMove), selectionEnd + endMove);
        }
    }
}

// --- Marcadores ------------------------------------------------------------------------------

bool hasBookmark(View& v, sptr_t line)
{
    return (v.send(SCI_MARKERGET, U(line)) & (1 << kBookmarkMarker)) != 0;
}

void toggleBookmark(View& v, sptr_t line)
{
    if (line < 0)
        line = v.send(SCI_LINEFROMPOSITION, U(v.send(SCI_GETCURRENTPOS)));
    if (hasBookmark(v, line)) {
        while (hasBookmark(v, line))
            v.send(SCI_MARKERDELETE, U(line), kBookmarkMarker);
    } else {
        v.send(SCI_MARKERADD, U(line), kBookmarkMarker);
    }
}

void goToBookmark(View& v, bool forward)
{
    const sptr_t current = v.send(SCI_LINEFROMPOSITION, U(v.send(SCI_GETCURRENTPOS)));
    const unsigned message = forward ? SCI_MARKERNEXT : SCI_MARKERPREVIOUS;
    sptr_t next = v.send(message, U(forward ? current + 1 : current - 1), 1 << kBookmarkMarker);
    if (next < 0)
        next = v.send(message, U(forward ? 0 : v.send(SCI_GETLINECOUNT)), 1 << kBookmarkMarker);
    if (next < 0)
        return;
    v.send(SCI_ENSUREVISIBLEENFORCEPOLICY, U(next));
    v.send(SCI_GOTOLINE, U(next));
    v.send(SCI_CHOOSECARETX);
}

void clearBookmarks(View& v)
{
    v.send(SCI_MARKERDELETEALL, kBookmarkMarker);
}

std::vector<sptr_t> bookmarkedLines(View& v)
{
    std::vector<sptr_t> lines;
    for (sptr_t line = v.send(SCI_MARKERNEXT, 0, 1 << kBookmarkMarker); line >= 0;
         line = v.send(SCI_MARKERNEXT, U(line + 1), 1 << kBookmarkMarker))
        lines.push_back(line);
    return lines;
}

void setBookmarks(View& v, const std::vector<sptr_t>& lines)
{
    const sptr_t count = v.send(SCI_GETLINECOUNT);
    for (const sptr_t line : lines) {
        if (line >= 0 && line < count && !hasBookmark(v, line))
            v.send(SCI_MARKERADD, U(line), kBookmarkMarker);
    }
}

std::string markedLinesText(View& v)
{
    std::string text;
    for (const sptr_t line : bookmarkedLines(v)) {
        const sptr_t start = lineStart(v, line);
        text += textRange(v, start, start + v.send(SCI_LINELENGTH, U(line)));
    }
    return text;
}

void deleteMarkedLines(View& v, bool marked)
{
    v.send(SCI_BEGINUNDOACTION);
    for (sptr_t line = lastLine(v); line >= 0; --line) {
        if (hasBookmark(v, line) != marked)
            continue;
        // Notepad_plus::deleteMarkedline: la línea con su fin de línea
        const sptr_t start = lineStart(v, line);
        const sptr_t length = v.send(SCI_LINELENGTH, U(line));
        while (hasBookmark(v, line))
            v.send(SCI_MARKERDELETE, U(line), kBookmarkMarker);
        replaceRange(v, start, start + length, "");
    }
    v.send(SCI_ENDUNDOACTION);
}

void replaceMarkedLines(View& v, const std::string& text)
{
    v.send(SCI_BEGINUNDOACTION);
    for (sptr_t line = lastLine(v); line >= 0; --line) {
        if (hasBookmark(v, line))
            replaceRange(v, lineStart(v, line), lineEnd(v, line), text);
    }
    v.send(SCI_ENDUNDOACTION);
}

void inverseBookmarks(View& v)
{
    for (sptr_t line = 0; line <= lastLine(v); ++line)
        toggleBookmark(v, line);
}

// --- Historial de cambios ---------------------------------------------------------------------

void goToChange(View& v, bool forward)
{
    constexpr int mask = (1 << SC_MARKNUM_HISTORY_REVERTED_TO_ORIGIN) | (1 << SC_MARKNUM_HISTORY_SAVED)
        | (1 << SC_MARKNUM_HISTORY_MODIFIED) | (1 << SC_MARKNUM_HISTORY_REVERTED_TO_MODIFIED);
    sptr_t line = -1;
    const sptr_t currentLine = v.send(SCI_LINEFROMPOSITION, U(v.send(SCI_GETCURRENTPOS)));
    sptr_t blockIndicator = currentLine;
    const sptr_t lineCount = v.send(SCI_GETLINECOUNT);

    if (forward) {
        // Desde la línea actual: si es parte de un bloque cambiado, se salta el bloque
        for (sptr_t i = currentLine; i < lineCount; ++i) {
            if (v.send(SCI_MARKERGET, U(i)) & mask) {
                if (i != blockIndicator) {
                    line = i;
                    break;
                }
                ++blockIndicator;
            }
        }
        if (line == -1) { // dar la vuelta
            for (sptr_t i = 0; i < currentLine + 1; ++i) {
                if (v.send(SCI_MARKERGET, U(i)) & mask) {
                    line = i;
                    break;
                }
            }
        }
    } else {
        while (true) {
            line = v.send(SCI_MARKERPREVIOUS, U(blockIndicator), mask);
            if (line == -1 || line != blockIndicator)
                break;
            --blockIndicator;
        }
        if (line == -1)
            line = v.send(SCI_MARKERPREVIOUS, U(lineCount - 1), mask);
    }
    if (line != -1) {
        v.send(SCI_ENSUREVISIBLEENFORCEPOLICY, U(line));
        v.send(SCI_GOTOLINE, U(line));
    }
}

void clearChangeHistory(View& v)
{
    const sptr_t pos = v.send(SCI_GETCURRENTPOS);
    const sptr_t flags = v.send(SCI_GETCHANGEHISTORY);
    v.send(SCI_EMPTYUNDOBUFFER);
    v.send(SCI_SETCHANGEHISTORY, SC_CHANGE_HISTORY_DISABLED);
    v.send(SCI_SETCHANGEHISTORY, U(flags));
    v.send(SCI_GOTOPOS, U(pos));
}

// --- Estilo de token ---------------------------------------------------------------------------

int markAllOccurrences(View& v, const std::string& text, int indicator, bool matchCase, bool wholeWord)
{
    if (text.empty())
        return 0;
    const sptr_t targetStart = v.send(SCI_GETTARGETSTART);
    const sptr_t targetEnd = v.send(SCI_GETTARGETEND);
    const sptr_t searchFlags = v.send(SCI_GETSEARCHFLAGS);
    v.send(SCI_SETSEARCHFLAGS, (matchCase ? SCFIND_MATCHCASE : 0) | (wholeWord ? SCFIND_WHOLEWORD : 0));
    v.send(SCI_SETINDICATORCURRENT, U(indicator));
    const sptr_t length = v.send(SCI_GETLENGTH);
    int count = 0;
    sptr_t pos = 0;
    while (pos < length) {
        const sptr_t found = searchInTarget(v, text, pos, length);
        if (found < 0)
            break;
        const sptr_t end = v.send(SCI_GETTARGETEND);
        v.send(SCI_INDICATORFILLRANGE, U(found), end - found);
        ++count;
        pos = end > found ? end : found + 1;
    }
    v.send(SCI_SETTARGETRANGE, U(targetStart), targetEnd);
    v.send(SCI_SETSEARCHFLAGS, U(searchFlags));
    return count;
}

void markSelection(View& v, int indicator)
{
    sptr_t start = v.send(SCI_GETSELECTIONSTART);
    sptr_t end = v.send(SCI_GETSELECTIONEND);
    if (start == end) {
        const sptr_t caret = v.send(SCI_GETCURRENTPOS);
        start = v.send(SCI_WORDSTARTPOSITION, U(caret), 1);
        end = v.send(SCI_WORDENDPOSITION, U(caret), 1);
    }
    if (end > start) {
        v.send(SCI_SETINDICATORCURRENT, U(indicator));
        v.send(SCI_INDICATORFILLRANGE, U(start), end - start);
    }
}

void clearIndicator(View& v, int indicator)
{
    v.send(SCI_SETINDICATORCURRENT, U(indicator));
    v.send(SCI_INDICATORCLEARRANGE, 0, v.send(SCI_GETLENGTH));
}

bool goToIndicator(View& v, int indicator, bool forward)
{
    const auto ind = U(indicator);
    const sptr_t docLength = v.send(SCI_GETLENGTH);
    const sptr_t position = v.send(SCI_GETCURRENTPOS);
    bool inIndicator = v.send(SCI_INDICATORVALUEAT, ind, position) != 0;
    sptr_t posStart = v.send(SCI_INDICATORSTART, ind, position);
    sptr_t posEnd = v.send(SCI_INDICATOREND, ind, position);
    if (posStart == 0 && posEnd == docLength - 1)
        return false;

    sptr_t newPos = 0;
    if (forward) {
        // Notepad_plus::goToNextIndicator
        if (posEnd >= docLength) {
            inIndicator = v.send(SCI_INDICATORVALUEAT, ind, 0) != 0;
            posEnd = v.send(SCI_INDICATOREND, ind, 0);
        }
        if (inIndicator) { // salir del indicador en el que está el cursor
            posEnd = v.send(SCI_INDICATOREND, ind, posEnd);
            if (posEnd >= docLength)
                posEnd = v.send(SCI_INDICATOREND, ind, 0);
        }
        newPos = posEnd;
    } else {
        // Notepad_plus::goToPreviousIndicator
        if (posStart <= 0) {
            inIndicator = v.send(SCI_INDICATORVALUEAT, ind, docLength - 1) != 0;
            posStart = v.send(SCI_INDICATORSTART, ind, docLength - 1);
        }
        if (inIndicator) {
            posStart = v.send(SCI_INDICATORSTART, ind, posStart - 1);
            if (posStart <= 0)
                posStart = v.send(SCI_INDICATORSTART, ind, docLength - 1);
        }
        newPos = posStart - 1;
    }
    posStart = v.send(SCI_INDICATORSTART, ind, newPos);
    posEnd = v.send(SCI_INDICATOREND, ind, newPos);
    if (!v.send(SCI_INDICATORVALUEAT, ind, posStart))
        return false;
    v.send(SCI_ENSUREVISIBLE, U(v.send(SCI_LINEFROMPOSITION, U(posEnd))));
    if (forward)
        v.send(SCI_SETSEL, U(posStart), posEnd);
    else
        v.send(SCI_SETSEL, U(posEnd), posStart);
    v.send(SCI_SCROLLCARET);
    return true;
}

std::string selectedTextOrWord(View& v)
{
    sptr_t start = v.send(SCI_GETSELECTIONSTART);
    sptr_t end = v.send(SCI_GETSELECTIONEND);
    if (start == end) {
        const sptr_t caret = v.send(SCI_GETCURRENTPOS);
        start = v.send(SCI_WORDSTARTPOSITION, U(caret), 1);
        end = v.send(SCI_WORDENDPOSITION, U(caret), 1);
    }
    return textRange(v, start, end);
}

// --- Llaves ------------------------------------------------------------------------------------

void goToMatchingBrace(View& v, bool select)
{
    // Notepad_plus::findMatchingBracePos: la llave antes del cursor o la de después
    auto isBrace = [](sptr_t c) { return c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}'; };
    const sptr_t caret = v.send(SCI_GETCURRENTPOS);
    sptr_t brace = -1;
    if (caret > 0 && isBrace(v.send(SCI_GETCHARAT, U(caret - 1))))
        brace = caret - 1;
    else if (isBrace(v.send(SCI_GETCHARAT, U(caret))))
        brace = caret;
    if (brace < 0)
        return;
    const sptr_t opposite = v.send(SCI_BRACEMATCH, U(brace), 0);
    if (opposite < 0)
        return;
    if (select)
        v.send(SCI_SETSEL, U(std::min(brace, opposite)), std::max(brace, opposite) + 1);
    else
        v.send(SCI_GOTOPOS, U(opposite));
    v.send(SCI_CHOOSECARETX);
}

// --- Plegado -------------------------------------------------------------------------------

void foldAll(View& v, bool expand)
{
    v.send(SCI_FOLDALL, U((expand ? SC_FOLDACTION_EXPAND : SC_FOLDACTION_CONTRACT) | SC_FOLDACTION_CONTRACT_EVERY_LEVEL));
    if (expand)
        v.send(SCI_SCROLLCARET);
}

void foldLevel(View& v, int level, bool expand)
{
    if (v.send(SCI_GETENDSTYLED) < v.send(SCI_GETTEXTLENGTH))
        v.send(SCI_COLOURISE, 0, -1);
    const sptr_t lineCount = v.send(SCI_GETLINECOUNT);

    // ScintillaEditView::isFoldIndentationBased: los niveles los da la sangría
    std::string lexer(64, '\0');
    lexer.resize(static_cast<std::size_t>(v.send(SCI_GETLEXERLANGUAGE, 0, reinterpret_cast<sptr_t>(lexer.data()))));
    static constexpr std::string_view kIndentationLexers[] = {"python", "coffeescript", "haskell", "nimrod", "vb", "yaml"};
    const bool indentationBased =
        std::find(std::begin(kIndentationLexers), std::end(kIndentationLexers), lexer) != std::end(kIndentationLexers);

    if (indentationBased) {
        // foldIndentationBasedLevel: el nivel es la profundidad entre las cabeceras
        std::vector<sptr_t> stack;
        const auto wanted = static_cast<std::size_t>(level + 1);
        for (sptr_t line = 0; line < lineCount; ++line) {
            sptr_t lvl = v.send(SCI_GETFOLDLEVEL, U(line));
            if (!(lvl & SC_FOLDLEVELHEADERFLAG))
                continue;
            lvl &= SC_FOLDLEVELNUMBERMASK;
            while (!stack.empty() && lvl <= stack.back())
                stack.pop_back();
            stack.push_back(lvl);
            if (stack.size() == wanted) {
                if (isFoldExpanded(v, line) != expand)
                    foldLine(v, line, expand);
                line = v.send(SCI_GETLASTCHILD, U(line), -1); // los hijos ya están dentro
            }
        }
    } else {
        for (sptr_t line = 0; line < lineCount; ++line) {
            const sptr_t lvl = v.send(SCI_GETFOLDLEVEL, U(line));
            if (!(lvl & SC_FOLDLEVELHEADERFLAG))
                continue;
            if (level == ((lvl - SC_FOLDLEVELBASE) & SC_FOLDLEVELNUMBERMASK) && isFoldExpanded(v, line) != expand)
                foldLine(v, line, expand);
        }
    }
    if (lineCount > 0)
        v.send(SCI_SCROLLCARET);
}

void foldCurrent(View& v, bool expand)
{
    foldLine(v, v.send(SCI_LINEFROMPOSITION, U(v.send(SCI_GETCURRENTPOS))), expand);
}

} // namespace edita
