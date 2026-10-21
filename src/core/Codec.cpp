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

#include "core/Codec.h"

#include "core/EncodingDetection.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <iconv.h>
#include <map>
#include <mutex>
#include <unordered_map>

namespace edita {

namespace detail {

class IconvHandle
{
public:
    IconvHandle(const char* to, const char* from) : _cd(iconv_open(to, from)) {}
    ~IconvHandle()
    {
        if (isOpen())
            iconv_close(_cd);
    }
    IconvHandle(const IconvHandle&) = delete;
    IconvHandle& operator=(const IconvHandle&) = delete;

    bool isOpen() const { return _cd != reinterpret_cast<iconv_t>(-1); }

    // Convierte lo que pueda. Devuelve 0 si terminó o el errno que lo detuvo.
    int convert(const char*& in, std::size_t& inLeft, std::string& out, std::size_t growHint)
    {
        while (inLeft > 0) {
            const std::size_t oldSize = out.size();
            out.resize(oldSize + growHint + 16);
            char* outPtr = out.data() + oldSize;
            std::size_t outLeft = out.size() - oldSize;
            // glibc declara la entrada como char** aunque no la modifica
            char* inPtr = const_cast<char*>(in);
            const std::size_t r = iconv(_cd, &inPtr, &inLeft, &outPtr, &outLeft);
            in = inPtr;
            out.resize(static_cast<std::size_t>(outPtr - out.data()));
            if (r != static_cast<std::size_t>(-1))
                return 0;
            if (errno != E2BIG)
                return errno;
            growHint = inLeft * 2 + 64;
        }
        return 0;
    }

    // Vacía el estado interno del convertidor (y lo que tenga retenido)
    void flush(std::string& out)
    {
        char buffer[64];
        char* outPtr = buffer;
        std::size_t outLeft = sizeof(buffer);
        iconv(_cd, nullptr, nullptr, &outPtr, &outLeft);
        out.append(buffer, static_cast<std::size_t>(outPtr - buffer));
    }

private:
    iconv_t _cd;
};

// --- Codepages de un solo byte ----------------------------------------------------
//
// Se convierten con una tabla de 256 entradas, no con iconv, por dos motivos:
//  1. glibc compone caracteres en CP1255 y CP1258 (letra + signo combinante →
//     carácter precompuesto). Windows no, así que con iconv abrir y guardar un
//     texto vietnamita o hebreo con signos combinantes lo modificaría.
//  2. Los bytes que el codepage no define se conservan, como hace Windows:
//     0x80-0x9F → U+0080-U+009F (C1) y el resto → U+F700 + byte (uso privado;
//     Windows usa otra tabla de uso privado, no documentada).
// La tabla se construye una vez por codepage decodificando cada byte por
// separado con iconv: un byte aislado no puede componerse con nada.
struct SingleByteTable
{
    std::array<char32_t, 256> toUnicode{};
    std::unordered_map<char32_t, std::uint8_t> fromUnicode;
};

} // namespace detail

namespace {

using detail::SingleByteTable;

constexpr std::string_view kReplacementUtf8 = "\xEF\xBF\xBD"; // U+FFFD
constexpr char32_t kInvalidCodepoint = 0xFFFFFFFF;
constexpr char32_t kUndefinedHighBase = 0xF700;

void appendUtf8(std::string& out, char32_t cp)
{
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

// Decodifica un carácter UTF-8 al principio de `s` (no vacío). Devuelve su
// longitud en bytes, o 0 si la secuencia está incompleta. Si es inválida deja
// kInvalidCodepoint en `cp` y devuelve 1.
std::size_t peekUtf8(std::string_view s, char32_t& cp)
{
    const auto c = static_cast<std::uint8_t>(s[0]);
    std::size_t length = 0;
    if (c < 0x80) {
        cp = c;
        return 1;
    }
    if ((c & 0xE0) == 0xC0) {
        length = 2;
        cp = c & 0x1Fu;
    } else if ((c & 0xF0) == 0xE0) {
        length = 3;
        cp = c & 0x0Fu;
    } else if ((c & 0xF8) == 0xF0) {
        length = 4;
        cp = c & 0x07u;
    } else {
        cp = kInvalidCodepoint;
        return 1;
    }
    for (std::size_t k = 1; k < length; ++k) {
        if (k >= s.size())
            return 0;
        const auto t = static_cast<std::uint8_t>(s[k]);
        if ((t & 0xC0) != 0x80) {
            cp = kInvalidCodepoint;
            return 1;
        }
        cp = (cp << 6) | (t & 0x3Fu);
    }
    return length;
}

// Codepages multibyte de la tabla de N++; el resto son de un solo byte
bool isMultiByteCodepage(int codepage)
{
    return codepage == 932 || codepage == 936 || codepage == 949 || codepage == 950 || codepage == 51949;
}

std::unique_ptr<SingleByteTable> buildSingleByteTable(const char* iconvName)
{
    detail::IconvHandle cd("UTF-32LE", iconvName);
    if (!cd.isOpen())
        return nullptr;
    auto table = std::make_unique<SingleByteTable>();
    for (unsigned b = 0; b < 256; ++b) {
        const char byte = static_cast<char>(b);
        const char* in = &byte;
        std::size_t inLeft = 1;
        std::string out;
        const int err = cd.convert(in, inLeft, out, 8);
        cd.flush(out);
        char32_t cp = 0;
        if (err == 0 && out.size() == 4) {
            for (std::size_t k = 4; k-- > 0;)
                cp = (cp << 8) | static_cast<std::uint8_t>(out[k]);
        } else {
            cp = b <= 0x9F ? char32_t{b} : kUndefinedHighBase + b;
        }
        table->toUnicode[b] = cp;
        table->fromUnicode.emplace(cp, static_cast<std::uint8_t>(b)); // el primer byte gana
    }
    return table;
}

// Las tablas se comparten entre hilos (la búsqueda en archivos cargará en segundo plano)
const SingleByteTable* singleByteTable(int codepage)
{
    static std::mutex mutex;
    static std::map<int, std::unique_ptr<SingleByteTable>> cache;
    const std::lock_guard lock(mutex);
    auto it = cache.find(codepage);
    if (it == cache.end()) {
        const char* name = iconvNameForCodepage(codepage);
        it = cache.emplace(codepage, name ? buildSingleByteTable(name) : nullptr).first;
    }
    return it->second.get();
}

bool isBigEndian(UniMode mode)
{
    return mode == UniMode::Utf16BeBom || mode == UniMode::Utf16BeNoBom;
}

// Cómo convertir una codificación
struct CodecPlan
{
    enum class Kind { PassThrough, Table, Iconv, Unsupported };
    Kind kind = Kind::PassThrough;
    const SingleByteTable* table = nullptr;
    const char* iconvName = nullptr;
};

CodecPlan planFor(const TextEncoding& encoding)
{
    CodecPlan plan;
    if (encoding.codepage == -1) {
        if (encoding.isUtf16()) {
            plan.kind = CodecPlan::Kind::Iconv;
            plan.iconvName = isBigEndian(encoding.unicodeMode) ? "UTF-16BE" : "UTF-16LE";
        }
        return plan; // UTF-8 / ASCII: tal cual
    }
    if (encoding.codepage == kCodepageUtf8)
        return plan;
    if (isMultiByteCodepage(encoding.codepage)) {
        plan.iconvName = iconvNameForCodepage(encoding.codepage);
        plan.kind = plan.iconvName ? CodecPlan::Kind::Iconv : CodecPlan::Kind::Unsupported;
        return plan;
    }
    plan.table = singleByteTable(encoding.codepage);
    plan.kind = plan.table ? CodecPlan::Kind::Table : CodecPlan::Kind::Unsupported;
    return plan;
}

// Antepone a `input` los bytes pendientes del bloque anterior
std::string_view joinPending(std::string& pending, std::string_view input, std::string& storage)
{
    if (pending.empty())
        return input;
    storage = std::move(pending);
    storage.append(input);
    pending.clear();
    return storage;
}

} // namespace

// --- Decoder -----------------------------------------------------------------

Decoder::Decoder(const TextEncoding& encoding)
    : _encoding(encoding)
    , _bomToSkip(encoding.codepage == -1 ? bomLength(encoding.unicodeMode) : 0)
{
    const CodecPlan plan = planFor(encoding);
    _table = plan.table;
    _valid = plan.kind != CodecPlan::Kind::Unsupported;
    if (plan.kind == CodecPlan::Kind::Iconv) {
        _iconv = std::make_unique<detail::IconvHandle>("UTF-8", plan.iconvName);
        _valid = _iconv->isOpen();
    }
}

Decoder::~Decoder() = default;

void Decoder::decode(std::string_view input, bool final, std::string& out)
{
    while (_bomToSkip > 0 && !input.empty()) {
        input.remove_prefix(1);
        --_bomToSkip;
    }
    if (!_valid)
        return;
    if (_table) {
        out.reserve(out.size() + input.size() * 2);
        for (const char c : input)
            appendUtf8(out, _table->toUnicode[static_cast<std::uint8_t>(c)]);
    } else if (_iconv) {
        decodeWithIconv(input, final, out);
    } else {
        out.append(input); // UTF-8 / ASCII: tal cual, sin validar (como N++)
    }
}

void Decoder::decodeWithIconv(std::string_view input, bool final, std::string& out)
{
    std::string storage;
    input = joinPending(_pending, input, storage);

    const char* in = input.data();
    std::size_t inLeft = input.size();
    const std::size_t unit = _encoding.isUtf16() ? 2 : 1;

    while (inLeft > 0) {
        const int err = _iconv->convert(in, inLeft, out, inLeft * 3);
        if (err == 0)
            break;
        if (err == EINVAL) { // carácter incompleto al final del bloque
            if (final) {
                out.append(kReplacementUtf8);
                ++_invalid;
            } else {
                _pending.assign(in, inLeft);
            }
            break;
        }
        // EILSEQ: secuencia inválida
        const std::size_t skip = std::min(unit, inLeft);
        out.append(kReplacementUtf8);
        ++_invalid;
        in += skip;
        inLeft -= skip;
    }
    if (final)
        _iconv->flush(out);
}

// --- Encoder -----------------------------------------------------------------

Encoder::Encoder(const TextEncoding& encoding)
    : _encoding(encoding)
    , _bomPending(encoding.hasBom())
{
    const CodecPlan plan = planFor(encoding);
    _table = plan.table;
    _valid = plan.kind != CodecPlan::Kind::Unsupported;
    if (plan.kind == CodecPlan::Kind::Iconv) {
        _iconv = std::make_unique<detail::IconvHandle>(plan.iconvName, "UTF-8");
        _valid = _iconv->isOpen();
    }
}

Encoder::~Encoder() = default;

void Encoder::encode(std::string_view utf8, bool final, std::string& out)
{
    if (!_valid)
        return;
    if (_bomPending) {
        switch (_encoding.unicodeMode) {
        case UniMode::Utf8Bom: out.append("\xEF\xBB\xBF"); break;
        case UniMode::Utf16BeBom: out.append("\xFE\xFF"); break;
        case UniMode::Utf16LeBom: out.append("\xFF\xFE"); break;
        default: break;
        }
        _bomPending = false;
    }
    if (_table)
        encodeWithTable(utf8, final, out);
    else if (_iconv)
        encodeWithIconv(utf8, final, out);
    else
        out.append(utf8);
}

void Encoder::encodeWithTable(std::string_view utf8, bool final, std::string& out)
{
    std::string storage;
    utf8 = joinPending(_pending, utf8, storage);

    out.reserve(out.size() + utf8.size());
    std::size_t i = 0;
    while (i < utf8.size()) {
        const auto c = static_cast<std::uint8_t>(utf8[i]);
        if (c < 0x80 && _table->toUnicode[c] == c) { // ASCII: camino rápido
            out.push_back(static_cast<char>(c));
            ++i;
            continue;
        }
        char32_t cp = 0;
        const std::size_t length = peekUtf8(utf8.substr(i), cp);
        if (length == 0) { // carácter partido al final del bloque
            if (final) {
                out.push_back('?');
                ++_unmappable;
            } else {
                _pending.assign(utf8.substr(i));
            }
            break;
        }
        const auto it = cp == kInvalidCodepoint ? _table->fromUnicode.end() : _table->fromUnicode.find(cp);
        if (it != _table->fromUnicode.end()) {
            out.push_back(static_cast<char>(it->second));
        } else {
            out.push_back('?');
            ++_unmappable;
        }
        i += length;
    }
}

void Encoder::encodeWithIconv(std::string_view utf8, bool final, std::string& out)
{
    std::string storage;
    utf8 = joinPending(_pending, utf8, storage);

    const char* in = utf8.data();
    std::size_t inLeft = utf8.size();

    while (inLeft > 0) {
        const int err = _iconv->convert(in, inLeft, out, inLeft * 2);
        if (err == 0)
            break;
        if (err == EINVAL) {
            if (final) {
                out.push_back('?');
                ++_unmappable;
            } else {
                _pending.assign(in, inLeft);
            }
            break;
        }
        // EILSEQ: UTF-8 inválido o carácter que no existe en el destino
        char32_t cp = 0;
        std::size_t length = peekUtf8(std::string_view(in, inLeft), cp);
        if (length == 0)
            length = inLeft; // no debería ocurrir: iconv habría dado EINVAL
        if (_encoding.isUtf16()) // byte UTF-8 inválido en el documento: U+FFFD
            out.append(isBigEndian(_encoding.unicodeMode) ? "\xFF\xFD" : "\xFD\xFF");
        else
            out.push_back('?');
        ++_unmappable;
        in += length;
        inLeft -= length;
    }
    if (final)
        _iconv->flush(out);
}

std::size_t countUnmappable(std::string_view utf8, const TextEncoding& encoding)
{
    if (encoding.isUnicode() || encoding.codepage == kCodepageUtf8)
        return 0;
    Encoder encoder(encoding);
    std::string scratch;
    constexpr std::size_t kBlock = 1 << 20;
    for (std::size_t pos = 0; pos < utf8.size(); pos += kBlock) {
        scratch.clear();
        const std::size_t len = std::min(kBlock, utf8.size() - pos);
        encoder.encode(utf8.substr(pos, len), pos + len == utf8.size(), scratch);
    }
    return encoder.unmappableChars();
}

std::string decodeAll(std::string_view bytes, const TextEncoding& encoding, std::size_t* invalid)
{
    Decoder decoder(encoding);
    std::string out;
    out.reserve(bytes.size());
    decoder.decode(bytes, true, out);
    if (invalid)
        *invalid = decoder.invalidSequences();
    return out;
}

std::string encodeAll(std::string_view utf8, const TextEncoding& encoding, std::size_t* unmappable)
{
    Encoder encoder(encoding);
    std::string out;
    out.reserve(utf8.size() + 4);
    encoder.encode(utf8, true, out);
    if (unmappable)
        *unmappable = encoder.unmappableChars();
    return out;
}

} // namespace edita
