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

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace edita {

// Mismos valores que EolType de Notepad++ (NppConstants.h) y que SC_EOL_CRLF/CR/LF
// de Scintilla, que coinciden: 0 = CRLF, 1 = CR, 2 = LF.
enum class EolType : std::uint8_t
{
    Windows = 0, // CR LF
    MacOs = 1,   // CR
    Unix = 2,    // LF
    Unknown = 3,
};

// getEOLFormatForm (Buffer.cpp): decide el primer fin de línea que aparece.
EolType detectEol(std::string_view text, EolType fallback = EolType::Unknown);

// Igual que detectEol pero sobre un flujo por bloques. A diferencia de N++, un CR
// al final de un bloque seguido de LF al principio del siguiente se reconoce
// como CRLF (N++ lo toma como CR de Mac).
class EolDetector
{
public:
    void feed(std::string_view chunk);
    EolType finish(EolType fallback);
    bool isDecided() const { return _result != EolType::Unknown; }

private:
    EolType _result = EolType::Unknown;
    bool _pendingCr = false;
};

// Convierte todos los finales de línea de `text` a `eol` (como SCI_CONVERTEOLS)
std::string convertEol(std::string_view text, EolType eol);

// Secuencia de bytes del fin de línea
std::string_view eolSequence(EolType eol);

// Texto de la barra de estado de Notepad++
const char* eolDisplayName(EolType eol);

} // namespace edita
