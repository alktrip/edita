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

#include "core/ColumnEditor.h"

#include <algorithm>
#include <cstdlib>

namespace edita {

namespace {

int radix(NumberBase base)
{
    switch (base) {
    case NumberBase::Hex:
    case NumberBase::HexUpper: return 16;
    case NumberBase::Octal: return 8;
    case NumberBase::Binary: return 2;
    case NumberBase::Decimal: break;
    }
    return 10;
}

} // namespace

std::string formatNumber(long long value, NumberBase base)
{
    const int r = radix(base);
    const char* digits = base == NumberBase::HexUpper ? "0123456789ABCDEF" : "0123456789abcdef";
    unsigned long long magnitude = value < 0 ? 0ULL - static_cast<unsigned long long>(value) : static_cast<unsigned long long>(value);
    std::string text;
    do {
        text.push_back(digits[magnitude % static_cast<unsigned>(r)]);
        magnitude /= static_cast<unsigned>(r);
    } while (magnitude);
    if (value < 0)
        text.push_back('-');
    std::reverse(text.begin(), text.end());
    return text;
}

bool parseNumber(const std::string& text, NumberBase base, long long& value)
{
    if (text.empty())
        return false;
    char* end = nullptr;
    value = std::strtoll(text.c_str(), &end, radix(base));
    return end && *end == '\0';
}

std::vector<std::string> columnNumbers(const ColumnNumbers& params, std::size_t count)
{
    std::vector<long long> numbers;
    long long current = params.initial;
    const int repeat = std::max(1, params.repeat); // sin esto, un bucle infinito (como en N++)
    while (numbers.size() < count) {
        for (int i = 0; i < repeat && numbers.size() < count; ++i)
            numbers.push_back(current);
        current += params.increase;
    }
    std::vector<std::string> texts;
    std::size_t width = 0;
    for (const long long n : numbers) {
        texts.push_back(formatNumber(n, params.base));
        width = std::max(width, texts.back().size());
    }
    if (params.leading != NumberLeading::None) {
        const char fill = params.leading == NumberLeading::Zeros ? '0' : ' ';
        for (std::string& t : texts) {
            if (t.size() < width) {
                // Los ceros van detrás del signo
                const std::size_t at = fill == '0' && !t.empty() && t[0] == '-' ? 1 : 0;
                t.insert(at, width - t.size(), fill);
            }
        }
    }
    return texts;
}

} // namespace edita
