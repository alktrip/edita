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

#include "core/Encoding.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace edita {

namespace detail {
class IconvHandle;
struct SingleByteTable;
}

// Convierte bytes de un archivo a UTF-8, bloque a bloque. Sustituye a
// Utf8_16_Read y a WcharMbcsConvertor::encode(cp → UTF-8) de Notepad++.
//
// - UTF-8 y ASCII pasan tal cual, sin validar (como en N++). Así, abrir y
//   guardar un archivo con bytes inválidos no lo modifica.
// - El BOM se elimina.
// - Un carácter multibyte partido entre dos bloques se completa en la
//   siguiente llamada.
// - Codepages de un byte: por tabla (sin iconv), sin componer caracteres y
//   conservando los bytes sin definir, como Windows (ver Codec.cpp).
// - Multibyte y UTF-16: iconv; las secuencias inválidas pasan a U+FFFD.
class Decoder
{
public:
    explicit Decoder(const TextEncoding& encoding);
    ~Decoder();
    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;

    // false si el codepage no está disponible en este sistema (p. ej. CP720)
    bool isValid() const { return _valid; }

    // Añade a `out` la conversión de `input`. `final` indica el último bloque.
    void decode(std::string_view input, bool final, std::string& out);

    // Número de secuencias sustituidas por U+FFFD
    std::size_t invalidSequences() const { return _invalid; }

private:
    void decodeWithIconv(std::string_view input, bool final, std::string& out);

    TextEncoding _encoding;
    const detail::SingleByteTable* _table = nullptr;
    std::unique_ptr<detail::IconvHandle> _iconv;
    std::string _pending;       // bytes de un carácter incompleto
    std::size_t _bomToSkip = 0;
    bool _valid = true;
    std::size_t _invalid = 0;
};

// Convierte UTF-8 a los bytes del archivo, bloque a bloque. Sustituye a
// Utf8_16_Write y a WcharMbcsConvertor::encode(UTF-8 → cp) de Notepad++.
//
// - Escribe el BOM al principio cuando el modo lo lleva.
// - UTF-8 pasa tal cual.
// - Caracteres que no existen en el codepage destino: '?' (el carácter por
//   defecto de WideCharToMultiByte). Se cuentan en unmappableChars() para poder
//   avisar al usuario. Windows además aplica "best fit" (ā → a); aquí no.
class Encoder
{
public:
    explicit Encoder(const TextEncoding& encoding);
    ~Encoder();
    Encoder(const Encoder&) = delete;
    Encoder& operator=(const Encoder&) = delete;

    bool isValid() const { return _valid; }

    void encode(std::string_view utf8, bool final, std::string& out);

    std::size_t unmappableChars() const { return _unmappable; }

private:
    void encodeWithTable(std::string_view utf8, bool final, std::string& out);
    void encodeWithIconv(std::string_view utf8, bool final, std::string& out);

    TextEncoding _encoding;
    const detail::SingleByteTable* _table = nullptr;
    std::unique_ptr<detail::IconvHandle> _iconv;
    std::string _pending;
    bool _bomPending = false;
    bool _valid = true;
    std::size_t _unmappable = 0;
};

// Cuántos caracteres de `utf8` no existen en `encoding` y se guardarían como '?'.
// 0 para las codificaciones Unicode.
std::size_t countUnmappable(std::string_view utf8, const TextEncoding& encoding);

// Atajos para convertir un texto completo de una vez
std::string decodeAll(std::string_view bytes, const TextEncoding& encoding, std::size_t* invalid = nullptr);
std::string encodeAll(std::string_view utf8, const TextEncoding& encoding, std::size_t* unmappable = nullptr);

} // namespace edita
