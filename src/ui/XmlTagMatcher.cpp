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

#include "ui/XmlTagMatcher.h"

#include "syntax/SyntaxStyler.h"

#include <SciLexer.h>

#include <string>
#include <utility>
#include <vector>

namespace edita {

namespace {

// Port de xmlMatchedTagsHighlighter.cpp (reescrito en N++ por Dave Brotherstone
// para no usar expresiones regulares, que hacia atrás son lentas)
class Matcher
{
public:
    Matcher(ScintillaEditBase& view, bool caseSensitive)
        : _view(view)
        , _flags(caseSensitive ? SCFIND_MATCHCASE : 0)
    {
    }

    struct TagsPos
    {
        sptr_t tagOpenStart = 0;
        sptr_t tagNameEnd = 0;
        sptr_t tagOpenEnd = 0;
        sptr_t tagCloseStart = 0;
        sptr_t tagCloseEnd = 0;
    };

    struct Found
    {
        sptr_t start = 0;
        sptr_t end = 0;
        bool success = false;
    };

    bool matchedTagsPos(TagsPos& tags);
    std::vector<std::pair<sptr_t, sptr_t>> attributesPos(sptr_t start, sptr_t end);
    Found findText(const char* text, sptr_t start, sptr_t end);

private:
    sptr_t send(unsigned message, sptr_t w = 0, sptr_t l = 0) { return _view.send(message, static_cast<uptr_t>(w), l); }
    sptr_t charAt(sptr_t pos) { return send(SCI_GETCHARAT, pos); }
    sptr_t styleAt(sptr_t pos) { return send(SCI_GETSTYLEAT, pos); }
    static bool isWhitespace(sptr_t ch) { return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'; }
    static bool isInString(sptr_t style) { return style == SCE_H_DOUBLESTRING || style == SCE_H_SINGLESTRING || style == SCE_H_COMMENT; }

    Found findOpenTag(const std::string& tagName, sptr_t start, sptr_t end);
    Found findCloseTag(const std::string& tagName, sptr_t start, sptr_t end);
    sptr_t findCloseAngle(sptr_t startPosition, sptr_t endPosition);
    std::string tagNameAt(sptr_t position, sptr_t docLength);

    ScintillaEditBase& _view;
    int _flags;
};

Matcher::Found Matcher::findText(const char* text, sptr_t start, sptr_t end)
{
    Sci_TextToFindFull search{};
    search.lpstrText = text;
    search.chrg.cpMin = start;
    search.chrg.cpMax = end;
    Found result;
    if (send(SCI_FINDTEXTFULL, _flags, reinterpret_cast<sptr_t>(&search)) != -1) {
        result.success = true;
        result.start = search.chrgText.cpMin;
        result.end = search.chrgText.cpMax;
    }
    return result;
}

// El nombre de la etiqueta: hasta un espacio, "/", ">" o comillas (comprobar las
// comillas es incorrecto, pero así funciona mejor con XML mal formado)
std::string Matcher::tagNameAt(sptr_t position, sptr_t docLength)
{
    std::string tagName;
    sptr_t nextChar = charAt(position);
    while (position < docLength && !isWhitespace(nextChar) && nextChar != '/' && nextChar != '>' && nextChar != '"'
           && nextChar != '\'') {
        tagName.push_back(static_cast<char>(nextChar));
        ++position;
        nextChar = charAt(position);
    }
    return tagName;
}

bool Matcher::matchedTagsPos(TagsPos& tags)
{
    bool tagFound = false;
    const sptr_t caret = send(SCI_GETCURRENTPOS);
    sptr_t searchStartPoint = caret;
    sptr_t style = 0;
    Found openFound;

    // El "<" anterior al cursor que no esté dentro de un atributo
    do {
        openFound = findText("<", searchStartPoint, 0);
        style = styleAt(openFound.start);
        searchStartPoint = openFound.start - 1;
    } while (openFound.success && isInString(style) && searchStartPoint > 0);

    if (!openFound.success || style == SCE_H_CDATA)
        return false;

    // Y que no haya un ">" entre él y el cursor
    Found closeFound;
    searchStartPoint = openFound.start;
    do {
        closeFound = findText(">", searchStartPoint, caret);
        style = styleAt(closeFound.start);
        searchStartPoint = closeFound.end;
    } while (closeFound.success && isInString(style) && searchStartPoint <= caret);
    if (closeFound.success)
        return false;

    // El cursor está en una etiqueta
    const sptr_t docLength = send(SCI_GETLENGTH);
    if (charAt(openFound.start + 1) == '/') {
        // --- En la etiqueta de cierre: se busca hacia atrás la de apertura -------
        tags.tagCloseStart = openFound.start;
        const Found endCloseTag = findText(">", caret, docLength);
        if (endCloseTag.success)
            tags.tagCloseEnd = endCloseTag.end;
        const std::string tagName = tagNameAt(openFound.start + 2, docLength);
        if (tagName.empty())
            return false;

        // Se busca "<NOMBRE"; si entre ella y el cierre hay otros cierres del mismo
        // nombre, hacen falta tantas aperturas más
        sptr_t currentEndPoint = tags.tagCloseStart;
        sptr_t openTagsRemaining = 1;
        Found nextOpenTag;
        do {
            nextOpenTag = findOpenTag(tagName, currentEndPoint, 0);
            if (nextOpenTag.success) {
                --openTagsRemaining;
                Found betweenCloseTag;
                sptr_t currentStartPosition = nextOpenTag.end;
                sptr_t closeTagsFound = 0;
                const bool forwardSearch = currentStartPosition < currentEndPoint;
                do {
                    betweenCloseTag = findCloseTag(tagName, currentStartPosition, currentEndPoint);
                    if (betweenCloseTag.success) {
                        ++closeTagsFound;
                        currentStartPosition = forwardSearch ? betweenCloseTag.end : betweenCloseTag.start - 1;
                    }
                } while (betweenCloseTag.success);

                if (closeTagsFound == 0 && openTagsRemaining == 0) {
                    tags.tagOpenStart = nextOpenTag.start;
                    tags.tagOpenEnd = nextOpenTag.end + 1;
                    tags.tagNameEnd = nextOpenTag.start + static_cast<sptr_t>(tagName.size()) + 1; // + el "<"
                    tagFound = true;
                } else {
                    openTagsRemaining += closeTagsFound;
                    currentEndPoint = nextOpenTag.start;
                }
            }
        } while (!tagFound && openTagsRemaining > 0 && nextOpenTag.success);
        return tagFound;
    }

    // --- En la etiqueta de apertura ------------------------------------------------
    const sptr_t position = openFound.start + 1;
    tags.tagOpenStart = openFound.start;
    const std::string tagName = tagNameAt(position, docLength);
    if (tagName.empty())
        return false;
    tags.tagNameEnd = openFound.start + static_cast<sptr_t>(tagName.size()) + 1;
    const sptr_t closeAngle = findCloseAngle(position + static_cast<sptr_t>(tagName.size()), docLength);
    if (closeAngle == -1)
        return false;
    tags.tagOpenEnd = closeAngle + 1;
    if (charAt(closeAngle - 1) == '/') {
        // Se cierra a sí misma: no hay etiqueta de cierre
        tags.tagCloseEnd = -1;
        tags.tagCloseStart = -1;
        return true;
    }

    // Se busca "</NOMBRE"; si antes hay otras aperturas del mismo nombre, hacen
    // falta tantos cierres más
    sptr_t currentStartPosition = tags.tagOpenEnd;
    sptr_t closeTagsRemaining = 1;
    Found nextCloseTag;
    do {
        nextCloseTag = findCloseTag(tagName, currentStartPosition, docLength);
        if (nextCloseTag.success) {
            --closeTagsRemaining;
            Found betweenOpenTag;
            const sptr_t currentEndPosition = nextCloseTag.start;
            sptr_t openTagsFound = 0;
            do {
                betweenOpenTag = findOpenTag(tagName, currentStartPosition, currentEndPosition);
                if (betweenOpenTag.success) {
                    ++openTagsFound;
                    currentStartPosition = betweenOpenTag.end;
                }
            } while (betweenOpenTag.success);

            if (openTagsFound == 0 && closeTagsRemaining == 0) {
                tags.tagCloseStart = nextCloseTag.start;
                tags.tagCloseEnd = nextCloseTag.end + 1;
                tagFound = true;
            } else {
                closeTagsRemaining += openTagsFound;
                currentStartPosition = nextCloseTag.end;
            }
        }
    } while (!tagFound && closeTagsRemaining > 0 && nextCloseTag.success);
    return tagFound;
}

Matcher::Found Matcher::findOpenTag(const std::string& tagName, sptr_t start, sptr_t end)
{
    const std::string search = "<" + tagName;
    Found openTagFound;
    Found result;
    sptr_t searchStart = start;
    const bool forwardSearch = start < end;
    do {
        result = findText(search.c_str(), searchStart, end);
        if (result.success) {
            const sptr_t nextChar = charAt(result.end);
            const sptr_t style = styleAt(result.start);
            if (style != SCE_H_CDATA && !isInString(style)) {
                // "<NOMBRE>" o "<NOMBRE atributos…>" (que no se cierre a sí misma)
                if (nextChar == '>') {
                    openTagFound.end = result.end;
                    openTagFound.success = true;
                } else if (isWhitespace(nextChar)) {
                    const sptr_t closeAngle = findCloseAngle(result.end, forwardSearch ? end : start);
                    if (closeAngle != -1 && charAt(closeAngle - 1) != '/') {
                        openTagFound.end = closeAngle;
                        openTagFound.success = true;
                    }
                }
            }
        }
        searchStart = forwardSearch ? result.end + 1 : result.start - 1;
        // Se sigue si estaba en CDATA o era otro nombre más largo ("<NOMBRE2")
    } while (result.success && !openTagFound.success);
    openTagFound.start = result.start;
    return openTagFound;
}

sptr_t Matcher::findCloseAngle(sptr_t startPosition, sptr_t endPosition)
{
    // El siguiente ">" que no esté dentro de un atributo (<a b="x>y"> es XML válido)
    if (startPosition > endPosition)
        std::swap(startPosition, endPosition);
    Found closeAngle;
    do {
        closeAngle = findText(">", startPosition, endPosition);
        if (closeAngle.success) {
            if (!isInString(styleAt(closeAngle.start)))
                return closeAngle.start;
            startPosition = closeAngle.end;
        }
    } while (closeAngle.success);
    return -1;
}

Matcher::Found Matcher::findCloseTag(const std::string& tagName, sptr_t start, sptr_t end)
{
    const std::string search = "</" + tagName;
    Found closeTagFound;
    Found result;
    sptr_t searchStart = start;
    const bool forwardSearch = start < end;
    bool validCloseTag = false;
    do {
        validCloseTag = false;
        result = findText(search.c_str(), searchStart, end);
        if (result.success) {
            sptr_t nextChar = charAt(result.end);
            const sptr_t style = styleAt(result.start);
            searchStart = forwardSearch ? result.end + 1 : result.start - 1;
            if (style != SCE_H_CDATA && !isInString(style)) {
                if (nextChar == '>') {
                    validCloseTag = true;
                    closeTagFound = {result.start, result.end, true};
                } else if (isWhitespace(nextChar)) {
                    // Espacios hasta el ">"; cualquier otra cosa no vale
                    sptr_t whitespacePoint = result.end;
                    do {
                        ++whitespacePoint;
                        nextChar = charAt(whitespacePoint);
                    } while (isWhitespace(nextChar));
                    if (nextChar == '>') {
                        validCloseTag = true;
                        closeTagFound = {result.start, whitespacePoint, true};
                    }
                }
            }
        }
    } while (result.success && !validCloseTag);
    return closeTagFound;
}

std::vector<std::pair<sptr_t, sptr_t>> Matcher::attributesPos(sptr_t start, sptr_t end)
{
    std::vector<std::pair<sptr_t, sptr_t>> attributes;
    const sptr_t bufLen = end - start + 1;
    if (bufLen <= 0)
        return attributes;
    std::string buf(static_cast<std::size_t>(bufLen) + 1, '\0');
    Sci_TextRangeFull range{{start, end}, buf.data()};
    send(SCI_GETTEXTRANGEFULL, 0, reinterpret_cast<sptr_t>(&range));

    enum { attr_invalid, attr_key, attr_pre_assign, attr_assign, attr_string, attr_single_quot_string, attr_value, attr_valid } state
        = attr_invalid;
    sptr_t startPos = -1;
    sptr_t oneMoreChar = 1;
    sptr_t i = 0;
    for (; i < bufLen; ++i) {
        switch (buf[static_cast<std::size_t>(i)]) {
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            if (state == attr_key) {
                state = attr_pre_assign;
            } else if (state == attr_value) {
                state = attr_valid;
                oneMoreChar = 0;
            }
            break;
        case '=':
            if (state == attr_key || state == attr_pre_assign)
                state = attr_assign;
            else if (state == attr_assign || state == attr_value)
                state = attr_invalid;
            break;
        case '"':
            if (state == attr_string) {
                state = attr_valid;
                oneMoreChar = 1;
            } else if (state == attr_key || state == attr_pre_assign || state == attr_value) {
                state = attr_invalid;
            } else if (state == attr_assign) {
                state = attr_string;
            }
            break;
        case '\'':
            if (state == attr_single_quot_string) {
                state = attr_valid;
                oneMoreChar = 1;
            } else if (state == attr_key || state == attr_pre_assign || state == attr_value) {
                state = attr_invalid;
            } else if (state == attr_assign) {
                state = attr_single_quot_string;
            }
            break;
        default:
            if (state == attr_invalid) {
                state = attr_key;
                startPos = i;
            } else if (state == attr_pre_assign) {
                state = attr_invalid;
            } else if (state == attr_assign) {
                state = attr_value;
            }
        }
        if (state == attr_valid) {
            attributes.emplace_back(start + startPos, start + i + oneMoreChar);
            state = attr_invalid;
        }
    }
    if (state == attr_value)
        attributes.emplace_back(start + startPos, start + i - 1);
    return attributes;
}

void clear(ScintillaEditBase& view, int indicator)
{
    view.send(SCI_SETINDICATORCURRENT, static_cast<uptr_t>(indicator));
    view.send(SCI_INDICATORCLEARRANGE, 0, view.send(SCI_GETLENGTH));
}

} // namespace

void matchXmlTags(ScintillaEditBase& view, LangType lang, const XmlTagMatchOptions& options)
{
    clear(view, kIndicatorTagMatch);
    clear(view, kIndicatorTagAttribute);
    if (lang != LangType::Xml && lang != LangType::Html && lang != LangType::Php && lang != LangType::Asp && lang != LangType::Jsp)
        return;

    Matcher matcher(view, options.caseSensitive);
    // Dentro de un bloque de código (no de marcado) no se buscan etiquetas
    if (lang == LangType::Php || lang == LangType::Asp || lang == LangType::Jsp) {
        const char* codeBegin = lang == LangType::Php ? "<?" : "<%";
        const char* codeEnd = lang == LangType::Php ? "?>" : "%>";
        const sptr_t caret = 1 + view.send(SCI_GETCURRENTPOS); // +1: el cursor entre "<" y "?"
        const auto startFound = matcher.findText(codeBegin, caret, 0);
        const auto endFound = matcher.findText(codeEnd, caret, 0);
        if (startFound.success && (!endFound.success || endFound.start <= startFound.end))
            return;
    }

    // Se restauran el destino y las opciones de búsqueda para no interferir con Buscar
    const sptr_t targetStart = view.send(SCI_GETTARGETSTART);
    const sptr_t targetEnd = view.send(SCI_GETTARGETEND);
    const sptr_t searchFlags = view.send(SCI_GETSEARCHFLAGS);

    Matcher::TagsPos tags;
    if (matcher.matchedTagsPos(tags)) {
        view.send(SCI_SETINDICATORCURRENT, kIndicatorTagMatch);
        sptr_t openTagTailLength = 2; // "/>"
        if (tags.tagCloseStart != -1 && tags.tagCloseEnd != -1) {
            view.send(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(tags.tagCloseStart), tags.tagCloseEnd - tags.tagCloseStart);
            openTagTailLength = 1; // ">"
        }
        view.send(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(tags.tagOpenStart), tags.tagNameEnd - tags.tagOpenStart);
        view.send(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(tags.tagOpenEnd - openTagTailLength), openTagTailLength);

        if (options.highlightAttributes) {
            view.send(SCI_SETINDICATORCURRENT, kIndicatorTagAttribute);
            for (const auto& [start, end] : matcher.attributesPos(tags.tagNameEnd, tags.tagOpenEnd - openTagTailLength))
                view.send(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(start), end - start);
        }

        if (options.indentGuides && tags.tagCloseStart != -1) {
            const sptr_t lineOpen = view.send(SCI_LINEFROMPOSITION, static_cast<uptr_t>(tags.tagOpenStart));
            const sptr_t lineClose = view.send(SCI_LINEFROMPOSITION, static_cast<uptr_t>(tags.tagCloseStart));
            if (lineOpen != lineClose) {
                const sptr_t columnOpen = view.send(SCI_GETCOLUMN, static_cast<uptr_t>(tags.tagOpenStart));
                const sptr_t columnClose = view.send(SCI_GETCOLUMN, static_cast<uptr_t>(tags.tagCloseStart));
                view.send(SCI_BRACEHIGHLIGHT, static_cast<uptr_t>(tags.tagOpenStart), tags.tagCloseEnd - 1);
                view.send(SCI_SETHIGHLIGHTGUIDE, static_cast<uptr_t>(columnOpen < columnClose ? columnOpen : columnClose));
            }
        }
    }

    view.send(SCI_SETTARGETRANGE, static_cast<uptr_t>(targetStart), targetEnd);
    view.send(SCI_SETSEARCHFLAGS, static_cast<uptr_t>(searchFlags));
}

} // namespace edita
