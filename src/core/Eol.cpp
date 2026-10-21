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

#include "core/Eol.h"

namespace edita {

EolType detectEol(std::string_view text, EolType fallback)
{
    EolDetector detector;
    detector.feed(text);
    return detector.finish(fallback);
}

void EolDetector::feed(std::string_view chunk)
{
    if (isDecided() || chunk.empty())
        return;
    std::size_t i = 0;
    if (_pendingCr) {
        _result = chunk[0] == '\n' ? EolType::Windows : EolType::MacOs;
        return;
    }
    for (; i < chunk.size(); ++i) {
        if (chunk[i] == '\r') {
            if (i + 1 < chunk.size()) {
                _result = chunk[i + 1] == '\n' ? EolType::Windows : EolType::MacOs;
            } else {
                _pendingCr = true; // se decide con el siguiente bloque
            }
            return;
        }
        if (chunk[i] == '\n') {
            _result = EolType::Unix;
            return;
        }
    }
}

EolType EolDetector::finish(EolType fallback)
{
    if (_pendingCr && !isDecided())
        _result = EolType::MacOs; // CR al final del archivo
    _pendingCr = false;
    return isDecided() ? _result : fallback;
}

std::string_view eolSequence(EolType eol)
{
    switch (eol) {
    case EolType::Windows: return "\r\n";
    case EolType::MacOs: return "\r";
    case EolType::Unix:
    case EolType::Unknown: break;
    }
    return "\n";
}

std::string convertEol(std::string_view text, EolType eol)
{
    const std::string_view newEol = eolSequence(eol);
    std::string out;
    out.reserve(text.size() + text.size() / 32);
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n')
                ++i;
            out.append(newEol);
        } else if (c == '\n') {
            out.append(newEol);
        } else {
            out.push_back(c);
        }
    }
    return out;
}

const char* eolDisplayName(EolType eol)
{
    switch (eol) {
    case EolType::Windows: return "Windows (CR LF)";
    case EolType::MacOs: return "Macintosh (CR)";
    case EolType::Unix:
    case EolType::Unknown: break;
    }
    return "Unix (LF)";
}

} // namespace edita
