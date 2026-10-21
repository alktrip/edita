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

#include "search/SearchEngine.h"

#include <BoostRegexSearch.h>
#include <Scintilla.h>

#include <algorithm>

namespace edita {

namespace {

bool isRegex(const SearchOptions& options)
{
    return options.mode == SearchMode::Regex;
}

// En regex la plantilla va tal cual ($1, \1); en extendido se convierten las secuencias
std::string replacementText(const SearchOptions& options)
{
    return isRegex(options) ? options.replacement : effectiveReplacement(options);
}

bool isUtf8Start(char c)
{
    return (static_cast<unsigned char>(c) & 0xC0) != 0x80;
}

} // namespace

FindResult findNext(SearchTarget& target, const SearchOptions& options, SciPos selStart, SciPos selEnd,
                    FindNextType type, bool firstIncremental)
{
    FindResult result;
    const std::string pattern = effectivePattern(options);
    if (pattern.empty())
        return result;

    const SciPos docLength = target.length();
    // La zona de búsqueda es relativa a la selección: se busca "fuera" de ella
    SciPos start = selEnd;
    SciPos end = docLength;
    if (options.searchUp) {
        start = selEnd - 1;
        end = 0;
    }
    if (firstIncremental) {
        start = selStart;
        end = docLength;
        if (options.searchUp) {
            start = selEnd;
            end = 0;
        }
    }
    start = std::clamp<SciPos>(start, 0, docLength);

    auto flags = static_cast<unsigned int>(searchFlags(options));
    switch (type) {
    case FindNextType::FindNext:
        flags |= SCFIND_REGEXP_EMPTYMATCH_ALL | SCFIND_REGEXP_SKIPCRLFASONE;
        break;
    case FindNextType::ReplaceNext:
        flags |= SCFIND_REGEXP_EMPTYMATCH_NOTAFTERMATCH | SCFIND_REGEXP_SKIPCRLFASONE;
        break;
    case FindNextType::FindNextForReplace:
        flags |= SCFIND_REGEXP_EMPTYMATCH_ALL | SCFIND_REGEXP_EMPTYMATCH_ALLOWATSTART | SCFIND_REGEXP_SKIPCRLFASONE;
        break;
    }
    // Nunca una coincidencia vacía en medio de un CRLF
    if (start > 0 && target.charAt(start - 1) == '\r' && target.charAt(start) == '\n')
        flags = (flags & ~static_cast<unsigned int>(SCFIND_REGEXP_EMPTYMATCH_MASK)) | SCFIND_REGEXP_EMPTYMATCH_NONE;

    SciPos matchEnd = -1;
    SciPos pos = target.find(start, end, pattern, flags, matchEnd);
    FindStatus status = FindStatus::Found;
    if (pos == kNotFound && options.wrapAround) {
        // Con "ciclo" se usa el resto del documento
        if (!options.searchUp) {
            start = 0;
            end = docLength;
            status = FindStatus::EndReached;
        } else {
            start = docLength;
            end = 0;
            status = FindStatus::TopReached;
        }
        pos = target.find(start, end, pattern, flags, matchEnd);
    }
    if (pos == kInvalidRegex || pos == kRegexFailure) {
        result.status = FindStatus::InvalidRegex;
        result.regexError = SearchTarget::lastRegexError();
        return result;
    }
    if (pos < 0)
        return result; // NotFound
    result.status = status;
    result.start = pos;
    result.end = matchEnd;
    return result;
}

ReplaceOutcome replaceNext(SearchTarget& target, const SearchOptions& options, SciPos selStart, SciPos selEnd)
{
    ReplaceOutcome outcome;
    outcome.next = findNext(target, options, selStart, selEnd, FindNextType::FindNextForReplace, true);
    if (!outcome.next.found()) {
        outcome.selStart = selStart;
        outcome.selEnd = selEnd;
        return outcome;
    }
    if (outcome.next.start != selStart || outcome.next.end != selEnd) {
        // La selección no era una coincidencia: solo se selecciona la siguiente
        outcome.selStart = outcome.next.start;
        outcome.selEnd = outcome.next.end;
        return outcome;
    }

    const SciPos replacedLength = target.replace(selStart, selEnd, replacementText(options), isRegex(options));
    outcome.replaced = true;
    const SciPos after = selStart + replacedLength;
    outcome.next = findNext(target, options, after, after, FindNextType::ReplaceNext, true);
    if (outcome.next.found()) {
        outcome.selStart = outcome.next.start;
        outcome.selEnd = outcome.next.end;
    } else {
        outcome.selStart = after;
        outcome.selEnd = after;
    }
    return outcome;
}

std::pair<SciPos, SciPos> rangeForOperation(RangeOperation op, const SearchOptions& options, SciPos selStart,
                                            SciPos selEnd, SciPos docLength, bool isEntire)
{
    SciPos start = options.searchUp ? 0 : selStart;
    SciPos end = options.searchUp ? selEnd : docLength;
    if (op == RangeOperation::CountAll && options.inSelection) {
        start = selStart;
        end = selEnd;
    } else if (options.wrapAround || isEntire) {
        start = 0;
        end = docLength;
    }
    if (options.inSelection && (op == RangeOperation::ReplaceAll || op == RangeOperation::FindAll) && !isEntire) {
        start = selStart;
        end = selEnd;
    }
    return {start, end};
}

RangeResult processRange(SearchTarget& target, const SearchOptions& options, RangeOperation op, SciPos start,
                         SciPos end, const std::function<void(SciPos, SciPos)>& onMatch)
{
    RangeResult result;
    const std::string pattern = effectivePattern(options);
    if (start == end || pattern.empty())
        return result;
    const std::string replacement = op == RangeOperation::ReplaceAll ? replacementText(options) : std::string();

    const auto flags = static_cast<unsigned int>(searchFlags(options)) | SCFIND_REGEXP_SKIPCRLFASONE
        | SCFIND_REGEXP_EMPTYMATCH_NOTAFTERMATCH;
    for (;;) {
        SciPos matchEnd = -1;
        const SciPos pos = target.find(start, end, pattern, flags, matchEnd);
        if (pos == kNotFound)
            break;
        if (pos < 0) {
            result.invalidRegex = true;
            result.regexError = SearchTarget::lastRegexError();
            return result;
        }
        if (matchEnd > end)
            break;

        const SciPos foundLength = matchEnd - pos;
        SciPos delta = 0;
        if (onMatch)
            onMatch(pos, matchEnd);
        if (op == RangeOperation::ReplaceAll)
            delta = target.replace(pos, matchEnd, replacement, isRegex(options)) - foundLength;
        ++result.count;

        if (pos + foundLength == end)
            break;
        start = pos + foundLength + delta; // buscar a partir del resultado
        end += delta;                      // el reemplazo mueve el final del rango
    }
    return result;
}

std::vector<LineHit> findAllLines(SearchTarget& target, const SearchOptions& options, SciPos start, SciPos end,
                                  RangeResult& result)
{
    std::vector<LineHit> lines;
    constexpr std::size_t kCut = kResultLineMaxLength - 6; // deja sitio para " ..."
    result = processRange(target, options, RangeOperation::FindAll, start, end, [&](SciPos ms, SciPos me) {
        const SciPos line = target.lineFromPosition(ms);
        const SciPos lineStart = target.lineStart(line);
        if (lines.empty() || lines.back().line != line) {
            LineHit hit;
            hit.line = line;
            hit.text = target.text(lineStart, target.lineEnd(line));
            if (hit.text.size() > kCut) {
                std::size_t cut = kCut;
                while (cut > 0 && !isUtf8Start(hit.text[cut]))
                    --cut;
                hit.text.resize(cut);
                hit.text += " ...";
            }
            lines.push_back(std::move(hit));
        }
        LineHit& hit = lines.back();
        const auto textLength = static_cast<SciPos>(hit.text.size());
        const SciPos segStart = std::min(ms - lineStart, textLength);
        const SciPos segEnd = std::min(std::max(me - lineStart, segStart), textLength);
        hit.segments.emplace_back(static_cast<int>(segStart), static_cast<int>(segEnd));
        hit.ranges.emplace_back(ms, me);
    });
    return lines;
}

} // namespace edita
