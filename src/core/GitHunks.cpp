// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/GitHunks.h"

#include <algorithm>

namespace edita::githunks {

std::vector<std::string_view> splitKeepingEol(std::string_view text)
{
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n' || text[i] == '\r') {
            if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
                ++i;
            lines.push_back(text.substr(start, i + 1 - start));
            start = i + 1;
        }
    }
    if (start < text.size())
        lines.push_back(text.substr(start));
    return lines;
}

namespace {

bool endsWithEol(std::string_view line)
{
    return !line.empty() && (line.back() == '\n' || line.back() == '\r');
}

// El fin de línea de un texto (el de su primera línea); \n si no tiene
std::string_view eolOf(const std::vector<std::string_view>& lines)
{
    for (const std::string_view l : lines) {
        if (l.ends_with("\r\n"))
            return "\r\n";
        if (endsWithEol(l))
            return l.substr(l.size() - 1);
    }
    return "\n";
}

} // namespace

std::string applyHunk(std::string_view oldText, std::string_view newText, const LineChange& change)
{
    const std::vector<std::string_view> oldLines = splitKeepingEol(oldText);
    const std::vector<std::string_view> newLines = splitKeepingEol(newText);
    const auto clampOld = [&](int i) { return static_cast<std::size_t>(std::clamp(i, 0, static_cast<int>(oldLines.size()))); };
    const auto clampNew = [&](int i) { return static_cast<std::size_t>(std::clamp(i, 0, static_cast<int>(newLines.size()))); };
    const std::size_t oldBegin = clampOld(change.oldLine);
    const std::size_t oldEnd = clampOld(change.oldLine + change.oldCount);
    const std::size_t newBegin = clampNew(change.line);
    const std::size_t newEnd = clampNew(change.line + change.count);

    std::string result;
    result.reserve(oldText.size() + 64);
    for (std::size_t i = 0; i < oldBegin; ++i)
        result += oldLines[i];
    // Lo de antes del bloque termina sin fin de línea (era la última línea) y el bloque añade
    // líneas detrás
    if (oldBegin > 0 && !endsWithEol(oldLines[oldBegin - 1]) && newBegin < newEnd)
        result += eolOf(oldLines);
    for (std::size_t i = newBegin; i < newEnd; ++i)
        result += newLines[i];
    if (oldEnd < oldLines.size() && !result.empty() && !endsWithEol(result))
        result += eolOf(oldLines);
    for (std::size_t i = oldEnd; i < oldLines.size(); ++i)
        result += oldLines[i];
    return result;
}

LineChange inverted(const LineChange& change)
{
    LineChange c = change;
    c.line = change.oldLine;
    c.count = change.oldCount;
    c.oldLine = change.line;
    c.oldCount = change.count;
    if (change.kind == LineChangeKind::Added)
        c.kind = LineChangeKind::Deleted;
    else if (change.kind == LineChangeKind::Deleted)
        c.kind = LineChangeKind::Added;
    return c;
}

const LineChange* hunkAt(const std::vector<LineChange>& changes, int line)
{
    for (const LineChange& c : changes) {
        if (c.kind == LineChangeKind::Deleted) {
            if (line == c.line || line == c.line - 1)
                return &c;
        } else if (line >= c.line && line < c.line + c.count) {
            return &c;
        }
    }
    return nullptr;
}

Span changedSpan(std::string_view a, std::string_view b)
{
    std::size_t start = 0;
    const std::size_t common = std::min(a.size(), b.size());
    while (start < common && a[start] == b[start])
        ++start;
    std::size_t endA = a.size();
    std::size_t endB = b.size();
    while (endA > start && endB > start && a[endA - 1] == b[endB - 1]) {
        --endA;
        --endB;
    }
    // No partir un carácter UTF-8 por la mitad
    while (start > 0 && start < a.size() && (static_cast<unsigned char>(a[start]) & 0xC0) == 0x80)
        --start;
    while (endA < a.size() && (static_cast<unsigned char>(a[endA]) & 0xC0) == 0x80) {
        ++endA;
        ++endB;
    }
    return {start, endA, endB};
}

} // namespace edita::githunks
