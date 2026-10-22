// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/FuzzyMatch.h"

#include <algorithm>
#include <limits>
#include <vector>

namespace edita {

namespace {

constexpr int kMatch = 16;          // cada letra que coincide
constexpr int kWordStart = 24;      // al principio de una palabra
constexpr int kFirstChar = 16;      // al principio del texto
constexpr int kConsecutive = 20;    // seguida de la anterior
constexpr int kGap = 1;             // por cada letra saltada entre dos coincidencias
constexpr int kLeadingGap = 1;      // por cada letra antes de la primera (hasta un límite)
constexpr int kNone = std::numeric_limits<int>::min() / 2;

bool isSeparator(QChar c)
{
    return c.isSpace() || c == QLatin1Char('_') || c == QLatin1Char('-') || c == QLatin1Char('.') || c == QLatin1Char('/')
        || c == QLatin1Char('\\') || c == QLatin1Char(':') || c == QLatin1Char('(') || c == QLatin1Char('[')
        || c == QLatin1Char('&') || c == QChar(0x203A); // ›
}

// Bonificación por empezar palabra: tras un separador o en un cambio a mayúscula (camelCase)
int startBonus(const QString& text, int j)
{
    if (j == 0)
        return kWordStart + kFirstChar;
    const QChar previous = text.at(j - 1);
    const QChar current = text.at(j);
    if (isSeparator(previous))
        return kWordStart;
    // camelCase: una mayúscula tras una minúscula y seguida de otra minúscula (no en
    // «iNVERTIR mAYÚSCULAS», todo en mayúsculas salvo la primera letra)
    if (previous.isLower() && current.isUpper() && j + 1 < text.size() && text.at(j + 1).isLower())
        return kWordStart;
    if (!previous.isDigit() && current.isDigit())
        return kWordStart / 2;
    return 0;
}

// Una palabra del patrón: la mejor forma de encontrar sus letras en orden
bool matchToken(const QString& token, const QString& text, const QString& folded, int& score, QList<int>& positions)
{
    const int m = static_cast<int>(token.size());
    const int n = static_cast<int>(text.size());
    if (m == 0)
        return true;
    if (m > n)
        return false;
    // Descarte rápido: ¿están las letras en orden?
    for (int i = 0, j = 0; i < m; ++i, ++j) {
        while (j < n && folded.at(j) != token.at(i))
            ++j;
        if (j == n)
            return false;
    }
    // D[i][j]: mejor puntuación con la letra i del patrón en la posición j del texto
    std::vector<int> d(static_cast<size_t>(m * n), kNone);
    std::vector<int> from(static_cast<size_t>(m * n), -1);
    auto at = [n](int i, int j) { return static_cast<size_t>(i * n + j); };
    for (int i = 0; i < m; ++i) {
        // Lo mejor de la fila anterior hasta j-2, ya con la penalización por el hueco
        int bestPrev = kNone;
        int bestPrevAt = -1;
        for (int j = i; j < n; ++j) {
            if (i > 0 && j >= 2 && d[at(i - 1, j - 2)] != kNone) {
                const int candidate = d[at(i - 1, j - 2)] + kGap * (j - 2);
                if (candidate > bestPrev) {
                    bestPrev = candidate;
                    bestPrevAt = j - 2;
                }
            }
            if (folded.at(j) != token.at(i))
                continue;
            const int here = kMatch + startBonus(text, j);
            if (i == 0) {
                d[at(i, j)] = here - kLeadingGap * std::min(j, 12);
                continue;
            }
            int best = kNone;
            int prev = -1;
            if (d[at(i - 1, j - 1)] != kNone) {
                best = d[at(i - 1, j - 1)] + kConsecutive;
                prev = j - 1;
            }
            if (bestPrev != kNone) {
                const int gapped = bestPrev - kGap * (j - 1);
                if (gapped > best) {
                    best = gapped;
                    prev = bestPrevAt;
                }
            }
            if (best != kNone) {
                d[at(i, j)] = best + here;
                from[at(i, j)] = prev;
            }
        }
    }
    int bestEnd = -1;
    for (int j = m - 1; j < n; ++j) {
        if (d[at(m - 1, j)] != kNone && (bestEnd < 0 || d[at(m - 1, j)] > d[at(m - 1, bestEnd)]))
            bestEnd = j;
    }
    if (bestEnd < 0)
        return false;
    score += d[at(m - 1, bestEnd)];
    for (int i = m - 1, j = bestEnd; i >= 0; --i) {
        positions << j;
        j = from[at(i, j)];
    }
    return true;
}

} // namespace

QChar foldForSearch(QChar c)
{
    if (c.unicode() < 0x80)
        return c.toLower();
    const QString decomposed = QString(c).normalized(QString::NormalizationForm_D);
    return decomposed.isEmpty() ? c.toLower() : decomposed.at(0).toLower();
}

FuzzyMatch fuzzyMatch(const QString& pattern, const QString& text)
{
    FuzzyMatch result;
    QString folded(text.size(), QChar());
    for (qsizetype i = 0; i < text.size(); ++i)
        folded[i] = foldForSearch(text.at(i));
    const QStringList tokens = pattern.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    int score = 0;
    QList<int> positions;
    for (const QString& raw : tokens) {
        QString token(raw.size(), QChar());
        for (qsizetype i = 0; i < raw.size(); ++i)
            token[i] = foldForSearch(raw.at(i));
        if (!matchToken(token, text, folded, score, positions))
            return result;
    }
    std::sort(positions.begin(), positions.end());
    positions.erase(std::unique(positions.begin(), positions.end()), positions.end());
    // A igualdad, el texto más corto
    result.score = std::max(0, score * 4 - static_cast<int>(text.size()));
    result.positions = positions;
    return result;
}

} // namespace edita
