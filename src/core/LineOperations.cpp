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

#include "core/LineOperations.h"

#include <QCollator>

#include <algorithm>
#include <charconv>
#include <numeric>
#include <optional>
#include <random>
#include <unordered_set>

namespace edita {

namespace {

bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

// stringTakeWhileAdmissable (Common.cpp)
std::string takeWhileAdmissable(std::string_view input, std::string_view admissable)
{
    const std::size_t idx = input.find_first_not_of(admissable);
    return std::string(idx == std::string_view::npos ? input : input.substr(0, idx));
}

// stodLocale con el locale "C": falla si no empieza por un número
std::optional<double> toDouble(const std::string& text)
{
    std::size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return std::nullopt;
    double value = 0;
    const char* first = text.data() + start;
    const auto [ptr, ec] = std::from_chars(first, text.data() + text.size(), value);
    if (ptr == first || ec != std::errc())
        return std::nullopt;
    return value;
}

// NumericSorter::considerStringEmpty
bool isBlank(std::string_view text)
{
    return text.find_first_not_of(" \t\r\n") == std::string_view::npos;
}

} // namespace

int compareWithIntegers(std::string_view a, std::string_view b)
{
    // IntegerSorter::sort (la comparación es la misma con columnas o sin ellas)
    long long compareResult = 0;
    std::size_t aNumIndex = 0;
    std::size_t bNumIndex = 0;
    auto at = [](std::string_view s, std::size_t i) { return static_cast<unsigned char>(s[i]); };
    while (compareResult == 0) {
        if (aNumIndex >= a.length() || bNumIndex >= b.length()) {
            compareResult = a.substr(std::min(aNumIndex, a.length())).compare(b.substr(std::min(bNumIndex, b.length())));
            break;
        }

        bool aChunkIsNum = isDigit(a[aNumIndex]);
        bool bChunkIsNum = isDigit(b[bNumIndex]);

        // Puede ser el principio de un número negativo
        int aNumSign = 1;
        if (!aChunkIsNum && aNumIndex + 1 < a.length()) {
            aChunkIsNum = a[aNumIndex] == '-' && isDigit(a[aNumIndex + 1]);
            aNumSign = -1;
        }
        int bNumSign = 1;
        if (!bChunkIsNum && bNumIndex + 1 < b.length()) {
            bChunkIsNum = b[bNumIndex] == '-' && isDigit(b[bNumIndex + 1]);
            bNumSign = -1;
        }

        if (aChunkIsNum != bChunkIsNum) {
            // Uno es número y el otro texto ("-0" es número pero "-" no: si los dos
            // empiezan por "-" el resultado es 0 y se sigue)
            compareResult = at(a, aNumIndex) - at(b, bNumIndex);
            ++aNumIndex;
            ++bNumIndex;
        } else if (aChunkIsNum) {
            if (aNumSign != bNumSign) {
                compareResult = aNumSign == 1 ? 1 : -1;
            } else {
                if (aNumSign == -1) {
                    ++aNumIndex;
                    ++bNumIndex;
                }
                std::size_t aNumEnd = a.find_first_not_of("1234567890", aNumIndex);
                if (aNumEnd == std::string_view::npos)
                    aNumEnd = a.length();
                std::size_t bNumEnd = b.find_first_not_of("1234567890", bNumIndex);
                if (bNumEnd == std::string_view::npos)
                    bNumEnd = b.length();

                int aZeroNum = 0;
                while (aNumIndex < a.length() && a[aNumIndex] == '0') {
                    ++aZeroNum;
                    ++aNumIndex;
                }
                int bZeroNum = 0;
                while (bNumIndex < b.length() && b[bNumIndex] == '0') {
                    ++bZeroNum;
                    ++bNumIndex;
                }

                const std::size_t aNumLength = aNumEnd - aNumIndex;
                const std::size_t bNumLength = bNumEnd - bNumIndex;
                // El número más largo es mayor (menor si son negativos); con la misma
                // longitud se comparan cifra a cifra porque pueden no caber en un long long
                if (aNumLength > bNumLength) {
                    compareResult = aNumSign;
                } else if (aNumLength < bNumLength) {
                    compareResult = -aNumSign;
                } else {
                    while (compareResult == 0 && aNumIndex < a.length() && isDigit(a[aNumIndex]) && bNumIndex < b.length()
                           && isDigit(b[bNumIndex])) {
                        compareResult = (at(a, aNumIndex) - at(b, bNumIndex)) * aNumSign;
                        ++aNumIndex;
                        ++bNumIndex;
                    }
                    if (compareResult == 0)
                        compareResult = bZeroNum - aZeroNum;
                }
            }
        } else {
            // Los dos son texto: se compara hasta el siguiente número o "-"
            if (a[aNumIndex] == '-')
                ++aNumIndex;
            if (b[bNumIndex] == '-')
                ++bNumIndex;
            const std::size_t aChunkEnd = a.find_first_of("1234567890-", aNumIndex);
            const std::size_t bChunkEnd = b.find_first_of("1234567890-", bNumIndex);
            const std::string_view aChunk = aNumIndex < a.length() ? a.substr(aNumIndex, aChunkEnd - aNumIndex) : std::string_view();
            const std::string_view bChunk = bNumIndex < b.length() ? b.substr(bNumIndex, bChunkEnd - bNumIndex) : std::string_view();
            compareResult = aChunk.compare(bChunk);
            aNumIndex = aChunkEnd;
            bNumIndex = bChunkEnd;
        }
    }
    return compareResult < 0 ? -1 : (compareResult > 0 ? 1 : 0);
}

std::ptrdiff_t sortLines(std::vector<std::string>& lines, const std::vector<std::string>& keys, const SortRequest& request)
{
    const std::vector<std::string>& sortKeys = keys.empty() ? lines : keys;
    std::vector<std::size_t> order(lines.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    const bool desc = request.descending;

    // Ordena los índices con una comparación "a < b" y aplica el sentido
    auto sortBy = [&](auto&& less) {
        std::stable_sort(order.begin(), order.end(), [&](std::size_t x, std::size_t y) {
            return desc ? less(y, x) : less(x, y);
        });
    };

    switch (request.kind) {
    case SortKind::Lexicographic:
        sortBy([&](std::size_t x, std::size_t y) { return sortKeys[x] < sortKeys[y]; });
        break;
    case SortKind::LexicographicCaseInsensitive: {
        // _wcsicmp compara en minúsculas
        std::vector<QString> folded;
        folded.reserve(sortKeys.size());
        for (const std::string& key : sortKeys)
            folded.push_back(QString::fromStdString(key).toLower());
        sortBy([&](std::size_t x, std::size_t y) { return folded[x] < folded[y]; });
        break;
    }
    case SortKind::Locale: {
        QCollator collator;
        collator.setCaseSensitivity(Qt::CaseSensitive);
        std::vector<QString> text;
        text.reserve(sortKeys.size());
        for (const std::string& key : sortKeys)
            text.push_back(QString::fromStdString(key));
        sortBy([&](std::size_t x, std::size_t y) { return collator.compare(text[x], text[y]) < 0; });
        break;
    }
    case SortKind::Integer:
        sortBy([&](std::size_t x, std::size_t y) { return compareWithIntegers(sortKeys[x], sortKeys[y]) < 0; });
        break;
    case SortKind::DecimalComma:
    case SortKind::DecimalDot: {
        // NumericSorter: las vacías se apartan y van al principio (al final si es descendente)
        std::vector<std::pair<std::size_t, double>> numbers;
        std::vector<std::size_t> empties;
        for (std::size_t i = 0; i < lines.size(); ++i) {
            std::string prepared;
            if (request.kind == SortKind::DecimalComma) {
                prepared = takeWhileAdmissable(sortKeys[i], " \t\r\n0123456789,-");
                std::replace(prepared.begin(), prepared.end(), ',', '.');
            } else {
                prepared = takeWhileAdmissable(sortKeys[i], " \t\r\n0123456789.-");
            }
            if (isBlank(prepared)) {
                empties.push_back(i);
                continue;
            }
            const std::optional<double> value = toDouble(prepared);
            if (!value)
                return static_cast<std::ptrdiff_t>(i);
            numbers.emplace_back(i, *value);
        }
        std::stable_sort(numbers.begin(), numbers.end(), [desc](const auto& x, const auto& y) {
            return desc ? x.second > y.second : x.second < y.second;
        });
        order.clear();
        if (!desc)
            order = empties;
        for (const auto& [index, value] : numbers)
            order.push_back(index);
        if (desc)
            order.insert(order.end(), empties.begin(), empties.end());
        break;
    }
    case SortKind::Length: {
        // Por longitud en caracteres (N++ cuenta wchar_t)
        std::vector<qsizetype> lengths;
        lengths.reserve(sortKeys.size());
        for (const std::string& key : sortKeys)
            lengths.push_back(QString::fromStdString(key).size());
        sortBy([&](std::size_t x, std::size_t y) { return lengths[x] < lengths[y]; });
        break;
    }
    case SortKind::Reverse:
        std::reverse(order.begin(), order.end());
        break;
    case SortKind::Random:
        std::shuffle(order.begin(), order.end(), std::default_random_engine(request.seed));
        break;
    }

    std::vector<std::string> sorted;
    sorted.reserve(lines.size());
    for (const std::size_t i : order)
        sorted.push_back(std::move(lines[i]));
    lines = std::move(sorted);
    return -1;
}

bool removeAnyDuplicateLines(std::vector<std::string>& lines)
{
    std::unordered_set<std::string> seen;
    const auto newEnd = std::remove_if(lines.begin(), lines.end(), [&seen](const std::string& line) {
        return !seen.insert(line).second;
    });
    const bool changed = newEnd != lines.end();
    lines.erase(newEnd, lines.end());
    return changed;
}

bool removeConsecutiveDuplicateLines(std::vector<std::string>& lines)
{
    const auto newEnd = std::unique(lines.begin(), lines.end());
    const bool changed = newEnd != lines.end();
    lines.erase(newEnd, lines.end());
    return changed;
}

bool removeEmptyLines(std::vector<std::string>& lines, bool alsoBlank)
{
    const auto newEnd = std::remove_if(lines.begin(), lines.end(), [alsoBlank](const std::string& line) {
        return alsoBlank ? line.find_first_not_of(" \t") == std::string::npos : line.empty();
    });
    const bool changed = newEnd != lines.end();
    lines.erase(newEnd, lines.end());
    return changed;
}

QString changeCase(const QString& input, TextCase textCase, unsigned seed)
{
    QString text = input;
    const qsizetype n = text.size();
    auto isSingleQuote = [](QChar c) { return c == u'\'' || c == u'’' || c == u'‘'; };
    auto isSpace = [](QChar c) { return c.isSpace(); };

    switch (textCase) {
    case TextCase::Upper:
        for (qsizetype i = 0; i < n; ++i)
            text[i] = text[i].toUpper();
        break;
    case TextCase::Lower:
        for (qsizetype i = 0; i < n; ++i)
            text[i] = text[i].toLower();
        break;
    case TextCase::ProperForce:
    case TextCase::ProperBlend: {
        const bool force = textCase == TextCase::ProperForce;
        for (qsizetype i = 0; i < n; ++i) {
            if (!text[i].isLetter())
                continue;
            // Excepción: la letra tras un apóstrofo dentro de una palabra ("don't")
            if (i >= 2 && isSingleQuote(text[i - 1]) && text[i - 2].isLetterOrNumber()) {
                if (force)
                    text[i] = text[i].toLower();
            } else if (i < 1 || !text[i - 1].isLetterOrNumber()) {
                text[i] = text[i].toUpper();
            } else if (force) {
                text[i] = text[i].toLower();
            }
        }
        break;
    }
    case TextCase::SentenceForce:
    case TextCase::SentenceBlend: {
        const bool force = textCase == TextCase::SentenceForce;
        bool isNewSentence = true;
        bool wasEolR = false;
        bool wasEolN = false;
        for (qsizetype i = 0; i < n; ++i) {
            const QChar c = text[i];
            if (c.isLetter()) {
                if (isNewSentence) {
                    text[i] = c.toUpper();
                    isNewSentence = false;
                } else if (force) {
                    text[i] = c.toLower();
                }
                wasEolR = false;
                wasEolN = false;
                // Excepción: el pronombre inglés "i" suelto
                if (text[i] == u'i' && i >= 1 && (isSpace(text[i - 1]) || text[i - 1] == u'(' || text[i - 1] == u'"')
                    && i + 1 < n && (isSpace(text[i + 1]) || text[i + 1] == u'\''))
                    text[i] = u'I';
            } else if (c == u'.' || c == u'!' || c == u'?') {
                isNewSentence = !(i + 1 == n || text[i + 1].isLetterOrNumber());
            } else if (c == u'\r') {
                if (wasEolR)
                    isNewSentence = true;
                else
                    wasEolR = true;
            } else if (c == u'\n') {
                if (wasEolN)
                    isNewSentence = true;
                else
                    wasEolN = true;
            }
        }
        break;
    }
    case TextCase::Invert:
        for (qsizetype i = 0; i < n; ++i)
            text[i] = text[i].isLower() ? text[i].toUpper() : text[i].toLower();
        break;
    case TextCase::Random: {
        std::mt19937 random(seed);
        for (qsizetype i = 0; i < n; ++i) {
            if (text[i].isLetter())
                text[i] = (random() & 1) ? text[i].toUpper() : text[i].toLower();
        }
        break;
    }
    }
    return text;
}

std::string convertTabs(std::string_view line, int tabWidth, TabConversion conversion, std::ptrdiff_t* caretOffset)
{
    // Notepad_plus::wsTabConvert, el cuerpo del bucle por línea. `s` acaba en '\0'
    // como el búfer de N++, que se lee más allá del carácter actual.
    const std::string source(line);
    const char* s = source.c_str();
    const std::ptrdiff_t caret = caretOffset ? *caretOffset : -1;
    std::ptrdiff_t newCaret = 0;
    std::string out;
    out.reserve(source.size() + 8);
    std::ptrdiff_t column = 0;

    if (conversion == TabConversion::TabsToSpaces) {
        if (source.find('\t') == std::string::npos)
            return source;
        for (std::ptrdiff_t i = 0; s[i] != '\0'; ++i) {
            if (s[i] == '\t') {
                const std::ptrdiff_t insertTabs = tabWidth - (column % tabWidth);
                for (std::ptrdiff_t j = 0; j < insertTabs; ++j) {
                    out += ' ';
                    if (i < caret)
                        ++newCaret;
                }
                column += insertTabs;
            } else {
                out += s[i];
                if (i < caret)
                    ++newCaret;
                if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) // UTF-8: solo los bytes iniciales
                    ++column;
            }
        }
    } else {
        const bool onlyLeading = conversion == TabConversion::SpacesToTabsLeading;
        std::ptrdiff_t tabStop = tabWidth - 1; // contando desde cero
        bool nextChar = false;
        std::ptrdiff_t counter = 0;
        bool nonSpaceFound = false;
        for (std::ptrdiff_t i = 0; s[i] != '\0'; ++i) {
            if (!nonSpaceFound) {
                while (s[i + counter] == ' ') {
                    if (column + counter == tabStop) {
                        tabStop += tabWidth;
                        if (counter >= 1) {
                            out += '\t';
                            i += counter;
                            column += counter + 1;
                            counter = 0;
                            nextChar = true;
                            if (i <= caret)
                                ++newCaret;
                            break;
                        }
                        if (s[i + 1] == ' ' || s[i + 1] == '\t') {
                            // Seguido de espacio o tabulación: también un espacio suelto pasa a tabulación
                            out += '\t';
                            ++i;
                            column += 1;
                            counter = 0;
                            if (i <= caret)
                                ++newCaret;
                        } else {
                            // Un espacio suelto no se convierte
                            out += s[i];
                            column += 1;
                            counter = 0;
                            nextChar = true;
                            if (i <= caret)
                                ++newCaret;
                            break;
                        }
                    } else {
                        ++counter;
                    }
                }
                if (nextChar) {
                    nextChar = false;
                    continue;
                }
                if (s[i] == ' ' && s[i + counter] == '\t') {
                    // Espacios "absorbidos" por una tabulación a su derecha
                    out += '\t';
                    i += counter;
                    column = tabStop + 1;
                    tabStop += tabWidth;
                    counter = 0;
                    if (i <= caret)
                        ++newCaret;
                    continue;
                }
            }

            if (onlyLeading && !nonSpaceFound)
                nonSpaceFound = true;

            if (s[i] == '\t') {
                out += s[i];
                column = tabStop + 1;
                tabStop += tabWidth;
                counter = 0;
            } else {
                out += s[i];
                counter = 0;
                if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) {
                    ++column;
                    if (column > 0 && column % tabWidth == 0)
                        tabStop += tabWidth;
                }
            }
            if (i < caret)
                ++newCaret;
        }
    }

    if (caretOffset && out != source)
        *caretOffset = newCaret;
    return out;
}

} // namespace edita
