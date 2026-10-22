// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/LineDiff.h"

#include <algorithm>
#include <functional>
#include <unordered_map>

namespace edita {

std::vector<std::string_view> splitLines(std::string_view text)
{
    std::vector<std::string_view> lines;
    size_t start = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n' || text[i] == '\r') {
            lines.push_back(text.substr(start, i - start));
            if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
                ++i;
            start = i + 1;
        }
    }
    if (start < text.size())
        lines.push_back(text.substr(start));
    return lines;
}

namespace {

enum class Op { Equal, Delete, Insert };

// Myers: el camino más corto de ediciones entre a y b (enteros: cada línea distinta
// tiene su número). Devuelve false si hacen falta más de maxEdits
bool myers(const std::vector<int>& a, const std::vector<int>& b, int maxEdits, std::vector<Op>& script)
{
    const int n = static_cast<int>(a.size());
    const int m = static_cast<int>(b.size());
    const int max = std::min(n + m, maxEdits);
    const int offset = max + 1;
    std::vector<int> v(static_cast<size_t>(2 * max + 3), 0);
    // Antes de cada paso d, los valores de V para k en [-d, d] (en k + d): lo que hace
    // falta para reconstruir el camino, sin guardar V entera cada vez
    std::vector<std::vector<int>> trace;
    for (int d = 0; d <= max; ++d) {
        trace.emplace_back(v.begin() + (offset - d), v.begin() + (offset + d + 1));
        for (int k = -d; k <= d; k += 2) {
            int x;
            if (k == -d || (k != d && v[static_cast<size_t>(k - 1 + offset)] < v[static_cast<size_t>(k + 1 + offset)]))
                x = v[static_cast<size_t>(k + 1 + offset)]; // bajar: insertar de b
            else
                x = v[static_cast<size_t>(k - 1 + offset)] + 1; // derecha: borrar de a
            int y = x - k;
            while (x < n && y < m && a[static_cast<size_t>(x)] == b[static_cast<size_t>(y)]) {
                ++x;
                ++y;
            }
            v[static_cast<size_t>(k + offset)] = x;
            if (x >= n && y >= m) {
                // Reconstrucción hacia atrás con las V guardadas
                script.clear();
                int cx = n;
                int cy = m;
                for (int dd = d; dd > 0; --dd) {
                    const std::vector<int>& pv = trace[static_cast<size_t>(dd)];
                    auto at = [&pv, dd](int kk) { return pv[static_cast<size_t>(kk + dd)]; };
                    const int ck = cx - cy;
                    int prevK;
                    if (ck == -dd || (ck != dd && at(ck - 1) < at(ck + 1)))
                        prevK = ck + 1;
                    else
                        prevK = ck - 1;
                    const int prevX = at(prevK);
                    const int prevY = prevX - prevK;
                    while (cx > prevX && cy > prevY) {
                        script.push_back(Op::Equal);
                        --cx;
                        --cy;
                    }
                    script.push_back(cx == prevX ? Op::Insert : Op::Delete);
                    cx = prevX;
                    cy = prevY;
                }
                while (cx > 0 && cy > 0) {
                    script.push_back(Op::Equal);
                    --cx;
                    --cy;
                }
                std::reverse(script.begin(), script.end());
                return true;
            }
        }
    }
    return false;
}

} // namespace

std::vector<LineChange> diffLines(const std::vector<std::string_view>& oldLines, const std::vector<std::string_view>& newLines,
                                  int maxEdits, bool* complete)
{
    if (complete)
        *complete = true;
    // Lo común del principio y del final no entra en Myers
    size_t prefix = 0;
    while (prefix < oldLines.size() && prefix < newLines.size() && oldLines[prefix] == newLines[prefix])
        ++prefix;
    size_t suffix = 0;
    while (suffix < oldLines.size() - prefix && suffix < newLines.size() - prefix
           && oldLines[oldLines.size() - 1 - suffix] == newLines[newLines.size() - 1 - suffix])
        ++suffix;
    const size_t oldCount = oldLines.size() - prefix - suffix;
    const size_t newCount = newLines.size() - prefix - suffix;
    std::vector<LineChange> changes;
    if (oldCount == 0 && newCount == 0)
        return changes;

    auto change = [&](int newStart, int added, int oldStart, int removed) {
        LineChange c;
        c.line = newStart;
        c.oldLine = oldStart;
        c.oldCount = removed;
        if (added == 0) {
            c.kind = LineChangeKind::Deleted;
            c.count = 0;
        } else {
            c.kind = removed == 0 ? LineChangeKind::Added : LineChangeKind::Modified;
            c.count = added;
        }
        changes.push_back(c);
    };

    // Cada línea distinta, un número
    std::unordered_map<std::string_view, int> ids;
    auto id = [&ids](std::string_view line) { return ids.emplace(line, static_cast<int>(ids.size())).first->second; };
    std::vector<int> a(oldCount);
    std::vector<int> b(newCount);
    for (size_t i = 0; i < oldCount; ++i)
        a[i] = id(oldLines[prefix + i]);
    for (size_t i = 0; i < newCount; ++i)
        b[i] = id(newLines[prefix + i]);

    std::vector<Op> script;
    if (!myers(a, b, maxEdits, script)) {
        if (complete)
            *complete = false;
        change(static_cast<int>(prefix), static_cast<int>(newCount), static_cast<int>(prefix), static_cast<int>(oldCount));
        return changes;
    }
    // Las ediciones seguidas forman un cambio: solo inserciones = añadidas, solo borrados =
    // borradas, las dos cosas = modificadas
    int x = 0; // en a
    int y = 0; // en b
    size_t i = 0;
    while (i < script.size()) {
        if (script[i] == Op::Equal) {
            ++x;
            ++y;
            ++i;
            continue;
        }
        const int startX = x;
        const int startY = y;
        while (i < script.size() && script[i] != Op::Equal) {
            if (script[i] == Op::Delete)
                ++x;
            else
                ++y;
            ++i;
        }
        change(static_cast<int>(prefix) + startY, y - startY, static_cast<int>(prefix) + startX, x - startX);
    }
    return changes;
}

} // namespace edita
