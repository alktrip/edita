// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// edita_rc -o recursos.cpp [-I carpeta]… [-D NOMBRE[=valor]]… archivo.rc…
//
// Lee los .rc de un plugin de Notepad++ y escribe un .cpp con sus diálogos (DIALOG,
// DIALOGEX), menús (MENU, MENUEX) y cadenas (STRINGTABLE), en el formato binario de
// Windows (DLGTEMPLATEEX, la plantilla de menú, UTF-16). Al cargarse el plugin, el .cpp
// los registra en la capa de compatibilidad (edita_compat_setResources), y FindResource,
// CreateDialogParam, DialogBoxParam, LoadMenu y LoadString los usan.
//
// Tiene su propio preprocesador, con las reglas de rc.exe: #include, #define (sin
// argumentos), #undef, #if/#ifdef/#ifndef/#elif/#else/#endif y defined(); de los archivos
// incluidos que no son .rc (resource.h…) solo se procesan las directivas. Las cabeceras de
// Windows (windows.h, winres.h, afxres.h…) no se leen: las constantes de los estilos están
// en edita_rc_constants.cpp. Define RC_INVOKED y _WIN32; APSTUDIO_INVOKED no, así que
// los bloques de Visual Studio (TEXTINCLUDE) se saltan.
//
// Los .rc pueden estar en UTF-16 (con BOM), en UTF-8 o en ANSI (Windows-1252).
// Los demás recursos (iconos, mapas de bits, VERSIONINFO, aceleradores…) se saltan.

#include "edita_rc.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace edita_rc;

[[noreturn]] void fatal(const std::string& message)
{
    std::fprintf(stderr, "edita_rc: error: %s\n", message.c_str());
    std::exit(1);
}

void warning(const std::string& where, const std::string& message)
{
    std::fprintf(stderr, "%s: edita_rc: aviso: %s\n", where.c_str(), message.c_str());
}

std::string upper(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

// --- Codificaciones ----------------------------------------------------------------------

void appendUtf8(std::string& out, uint32_t cp)
{
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

bool validUtf8(const std::string& s)
{
    std::size_t i = 0;
    while (i < s.size()) {
        const auto c = static_cast<unsigned char>(s[i]);
        const int extra = c < 0x80 ? 0 : (c >> 5) == 6 ? 1 : (c >> 4) == 14 ? 2 : (c >> 3) == 30 ? 3 : -1;
        if (extra < 0 || i + static_cast<std::size_t>(extra) >= s.size() + (extra == 0 ? 1u : 0u))
            return false;
        for (std::size_t k = 1; k <= static_cast<std::size_t>(extra); ++k) {
            if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80)
                return false;
        }
        i += static_cast<std::size_t>(extra) + 1;
    }
    return true;
}

// Windows-1252: 0x80-0x9F no son los de Latin-1
const uint16_t kCp1252[32] = {0x20AC, 0x81,   0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
                              0x2039, 0x0152, 0x8D,   0x017D, 0x8F,   0x90,   0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
                              0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x9D,   0x017E, 0x0178};

// El texto de un archivo, en UTF-8
std::string readText(const std::string& path, bool* ok)
{
    std::ifstream in(path, std::ios::binary);
    *ok = static_cast<bool>(in);
    if (!in)
        return {};
    std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto byte = [&](std::size_t i) { return static_cast<uint32_t>(static_cast<unsigned char>(data[i])); };
    std::string out;
    if (data.size() >= 2 && ((byte(0) == 0xFF && byte(1) == 0xFE) || (byte(0) == 0xFE && byte(1) == 0xFF))) {
        const bool little = byte(0) == 0xFF;
        for (std::size_t i = 2; i + 1 < data.size(); i += 2) {
            uint32_t unit = little ? (byte(i) | (byte(i + 1) << 8)) : ((byte(i) << 8) | byte(i + 1));
            if (unit >= 0xD800 && unit < 0xDC00 && i + 3 < data.size()) {
                const uint32_t low = little ? (byte(i + 2) | (byte(i + 3) << 8)) : ((byte(i + 2) << 8) | byte(i + 3));
                unit = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
                i += 2;
            }
            appendUtf8(out, unit);
        }
        return out;
    }
    if (data.size() >= 3 && byte(0) == 0xEF && byte(1) == 0xBB && byte(2) == 0xBF)
        return data.substr(3);
    if (validUtf8(data))
        return data;
    for (std::size_t i = 0; i < data.size(); ++i) {
        const uint32_t c = byte(i);
        appendUtf8(out, c >= 0x80 && c < 0xA0 ? kCp1252[c - 0x80] : c);
    }
    return out;
}

std::vector<uint32_t> codePoints(const std::string& utf8)
{
    std::vector<uint32_t> out;
    for (std::size_t i = 0; i < utf8.size();) {
        const auto c = static_cast<unsigned char>(utf8[i]);
        uint32_t cp = c;
        int extra = 0;
        if (c >= 0xF0) { cp = c & 0x07; extra = 3; }
        else if (c >= 0xE0) { cp = c & 0x0F; extra = 2; }
        else if (c >= 0xC0) { cp = c & 0x1F; extra = 1; }
        for (int k = 1; k <= extra && i + static_cast<std::size_t>(k) < utf8.size(); ++k)
            cp = (cp << 6) | (static_cast<unsigned char>(utf8[i + static_cast<std::size_t>(k)]) & 0x3F);
        out.push_back(cp);
        i += static_cast<std::size_t>(extra) + 1;
    }
    return out;
}

// --- Tokens -----------------------------------------------------------------------------

enum class Kind { Identifier, Number, String, Punct, End };

struct Token
{
    Kind kind = Kind::End;
    std::string text;    // identificador, puntuación o la cadena (UTF-8, sin escapes)
    long long number = 0;
    std::string where;   // archivo:línea
    bool spaceBefore = false; // para la conversión en cadena (#) de las macros
};

bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool isIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

// Una cadena del .rc: "" es una comilla; los escapes de C
std::string unescape(const std::string& raw, std::size_t& i)
{
    std::string out;
    ++i; // la comilla
    while (i < raw.size()) {
        const char c = raw[i];
        if (c == '"') {
            if (i + 1 < raw.size() && raw[i + 1] == '"') {
                out += '"';
                i += 2;
                continue;
            }
            ++i;
            break;
        }
        if (c == '\\' && i + 1 < raw.size()) {
            const char e = raw[i + 1];
            i += 2;
            switch (e) {
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            case 'a': out += '\a'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'v': out += '\v'; break;
            case '\\': out += '\\'; break;
            case '"': out += '"'; break;
            case '\'': out += '\''; break;
            case 'x': {
                uint32_t value = 0;
                int digits = 0;
                while (i < raw.size() && digits < 4 && std::isxdigit(static_cast<unsigned char>(raw[i]))) {
                    value = value * 16 + static_cast<uint32_t>(std::isdigit(static_cast<unsigned char>(raw[i])) ? raw[i] - '0'
                                                                    : (std::tolower(static_cast<unsigned char>(raw[i])) - 'a' + 10));
                    ++i;
                    ++digits;
                }
                appendUtf8(out, value);
                break;
            }
            default:
                if (e >= '0' && e <= '7') {
                    uint32_t value = static_cast<uint32_t>(e - '0');
                    int digits = 1;
                    while (i < raw.size() && digits < 3 && raw[i] >= '0' && raw[i] <= '7') {
                        value = value * 8 + static_cast<uint32_t>(raw[i] - '0');
                        ++i;
                        ++digits;
                    }
                    appendUtf8(out, value);
                } else {
                    out += '\\';
                    out += e;
                }
                break;
            }
            continue;
        }
        out += c;
        ++i;
    }
    return out;
}

std::vector<Token> tokenize(const std::string& line, const std::string& where)
{
    std::vector<Token> tokens;
    std::size_t i = 0;
    bool space = false;
    while (i < line.size()) {
        const char c = line[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            space = true;
            continue;
        }
        Token t;
        t.where = where;
        t.spaceBefore = space;
        space = false;
        if ((c == 'L' || c == 'l') && i + 1 < line.size() && line[i + 1] == '"') {
            ++i;
            continue; // L"…": lo mismo que "…"
        }
        if (c == '"') {
            t.kind = Kind::String;
            t.text = unescape(line, i);
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            std::size_t end = i;
            while (end < line.size() && (std::isalnum(static_cast<unsigned char>(line[end]))))
                ++end;
            std::string digits = line.substr(i, end - i);
            while (!digits.empty() && (digits.back() == 'L' || digits.back() == 'l' || digits.back() == 'U' || digits.back() == 'u'))
                digits.pop_back();
            t.kind = Kind::Number;
            t.number = std::strtoll(digits.c_str(), nullptr, 0);
            t.text = digits;
            i = end;
        } else if (isIdentStart(c)) {
            std::size_t end = i;
            while (end < line.size() && isIdentChar(line[end]))
                ++end;
            t.kind = Kind::Identifier;
            t.text = line.substr(i, end - i);
            i = end;
        } else {
            t.kind = Kind::Punct;
            // Operadores de dos caracteres de las expresiones de #if
            static const char* const kTwo[] = {"<<", ">>", "<=", ">=", "==", "!=", "&&", "||"};
            t.text = std::string(1, c);
            for (const char* op : kTwo) {
                if (line.compare(i, 2, op) == 0) {
                    t.text = op;
                    break;
                }
            }
            i += t.text.size();
        }
        tokens.push_back(t);
    }
    return tokens;
}

// --- Preprocesador -----------------------------------------------------------------------

struct Macro
{
    std::vector<Token> body;
    bool function = false;            // #define NOMBRE(a, b) …
    std::vector<std::string> params;
};

// El texto de unos tokens, con los espacios del original (para #)
std::string spell(const std::vector<Token>& tokens)
{
    std::string out;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0 && tokens[i].spaceBefore)
            out += ' ';
        if (tokens[i].kind == Kind::String) {
            out += '"';
            for (char c : tokens[i].text) {
                if (c == '"' || c == '\\')
                    out += '\\';
                out += c;
            }
            out += '"';
        } else {
            out += tokens[i].text;
        }
    }
    return out;
}

class Preprocessor
{
public:
    std::vector<std::string> includeDirs;
    std::map<std::string, Macro> macros;
    std::vector<Token> output;

    void define(const std::string& name, const std::string& value)
    {
        Macro macro;
        macro.body = tokenize(value, "<línea de órdenes>");
        macros[name] = macro;
    }

    void processFile(const std::string& path, bool statements)
    {
        bool ok = false;
        const std::string text = readText(path, &ok);
        if (!ok)
            fatal("no se puede leer " + path);
        if (_depth > 32)
            fatal("demasiados #include anidados en " + path);
        ++_depth;
        processText(text, path, statements);
        --_depth;
    }

    // Expande las macros de una línea de tokens
    std::vector<Token> expand(const std::vector<Token>& tokens, std::set<std::string> active = {}) const
    {
        std::vector<Token> out;
        for (std::size_t i = 0; i < tokens.size(); ++i) {
            const Token& t = tokens[i];
            if (t.kind == Kind::Identifier) {
                if (t.text == "defined") {
                    // defined(X) o defined X: se resuelve aquí, sin expandir X
                    std::size_t j = i + 1;
                    bool paren = j < tokens.size() && tokens[j].text == "(";
                    if (paren)
                        ++j;
                    Token value;
                    value.kind = Kind::Number;
                    value.where = t.where;
                    value.number = j < tokens.size() && macros.count(tokens[j].text) ? 1 : 0;
                    out.push_back(value);
                    i = j + (paren ? 1 : 0);
                    continue;
                }
                const auto it = macros.find(t.text);
                if (it != macros.end() && !active.count(t.text)) {
                    const Macro& macro = it->second;
                    std::vector<Token> body = macro.body;
                    if (macro.function) {
                        // Sin paréntesis detrás, el nombre se queda como está
                        if (i + 1 >= tokens.size() || tokens[i + 1].text != "(") {
                            out.push_back(t);
                            continue;
                        }
                        // Los argumentos: separados por comas fuera de paréntesis
                        std::vector<std::vector<Token>> args(1);
                        int depth = 0;
                        std::size_t j = i + 2;
                        for (; j < tokens.size(); ++j) {
                            const Token& a = tokens[j];
                            if (a.kind == Kind::Punct && a.text == "(")
                                ++depth;
                            else if (a.kind == Kind::Punct && a.text == ")" && depth-- == 0)
                                break;
                            else if (a.kind == Kind::Punct && a.text == "," && depth == 0) {
                                args.emplace_back();
                                continue;
                            }
                            args.back().push_back(a);
                        }
                        i = j; // el ")"
                        auto param = [&](const std::string& name) -> int {
                            for (std::size_t k = 0; k < macro.params.size(); ++k) {
                                if (macro.params[k] == name)
                                    return static_cast<int>(k);
                            }
                            return -1;
                        };
                        auto arg = [&](int k) { return static_cast<std::size_t>(k) < args.size() ? args[static_cast<std::size_t>(k)] : std::vector<Token>{}; };
                        std::vector<Token> substituted;
                        for (std::size_t k = 0; k < body.size(); ++k) {
                            const Token& b = body[k];
                            const bool pasteNext = k + 2 < body.size() && body[k + 1].text == "#" && body[k + 2].text == "#";
                            const bool pastePrev = k >= 2 && body[k - 1].text == "#" && body[k - 2].text == "#";
                            if (b.kind == Kind::Punct && b.text == "#" && k + 1 < body.size() && body[k + 1].text == "#") {
                                // ##: pega el token anterior con el siguiente
                                if (!substituted.empty() && k + 2 < body.size()) {
                                    const int p = param(body[k + 2].text);
                                    const std::string right = p >= 0 ? spell(arg(p)) : body[k + 2].text;
                                    const std::vector<Token> pasted = tokenize(spell({substituted.back()}) + right, t.where);
                                    substituted.pop_back();
                                    substituted.insert(substituted.end(), pasted.begin(), pasted.end());
                                }
                                k += 2;
                                continue;
                            }
                            if (b.kind == Kind::Punct && b.text == "#" && k + 1 < body.size() && param(body[k + 1].text) >= 0) {
                                // #a: el argumento tal cual, como cadena
                                Token str;
                                str.kind = Kind::String;
                                str.text = spell(arg(param(body[k + 1].text)));
                                str.where = t.where;
                                str.spaceBefore = b.spaceBefore;
                                substituted.push_back(str);
                                ++k;
                                continue;
                            }
                            const int p = b.kind == Kind::Identifier ? param(b.text) : -1;
                            if (p >= 0) {
                                // Un argumento: expandido, salvo junto a ##
                                const std::vector<Token> value = (pasteNext || pastePrev) ? arg(p) : expand(arg(p), active);
                                for (std::size_t v = 0; v < value.size(); ++v) {
                                    Token copy = value[v];
                                    if (v == 0)
                                        copy.spaceBefore = b.spaceBefore;
                                    substituted.push_back(copy);
                                }
                                continue;
                            }
                            substituted.push_back(b);
                        }
                        body = substituted;
                    }
                    std::set<std::string> nested = active;
                    nested.insert(t.text);
                    bool first = true;
                    for (Token e : expand(body, nested)) {
                        e.where = t.where;
                        if (first)
                            e.spaceBefore = t.spaceBefore;
                        first = false;
                        out.push_back(e);
                    }
                    continue;
                }
            }
            out.push_back(t);
        }
        return out;
    }

private:
    int _depth = 0;

    struct Conditional
    {
        bool active;      // la rama actual se procesa
        bool taken;       // alguna rama ya se tomó
        bool parentActive;
    };

    // Como en Windows: sin distinguir mayúsculas, carpeta a carpeta, y con '\\' o '/'
    static std::string resolveCaseInsensitive(const std::filesystem::path& base, const std::string& relative)
    {
        namespace fs = std::filesystem;
        fs::path current = base.empty() ? fs::path(".") : base;
        std::stringstream parts(relative);
        std::string part;
        std::error_code error;
        while (std::getline(parts, part, '/')) {
            if (part.empty() || part == ".")
                continue;
            if (part == "..") {
                current /= "..";
                continue;
            }
            if (fs::exists(current / part, error)) {
                current /= part;
                continue;
            }
            bool found = false;
            for (const fs::directory_entry& entry : fs::directory_iterator(current, error)) {
                if (upper(entry.path().filename().string()) == upper(part)) {
                    current = entry.path();
                    found = true;
                    break;
                }
            }
            if (!found)
                return {};
        }
        return fs::is_regular_file(current, error) ? current.string() : std::string();
    }

    std::string findInclude(const std::string& name, const std::string& fromDir) const
    {
        std::string normalized = name;
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        std::vector<std::string> dirs{fromDir};
        dirs.insert(dirs.end(), includeDirs.begin(), includeDirs.end());
        for (const std::string& dir : dirs) {
            const std::string found = resolveCaseInsensitive(dir, normalized);
            if (!found.empty())
                return found;
        }
        return {};
    }

    static bool knownWindowsHeader(const std::string& name)
    {
        static const std::set<std::string> headers = {
            "WINDOWS.H", "WINRES.H", "WINRESRC.H", "AFXRES.H", "WINUSER.H", "WINVER.H", "COMMCTRL.H", "RICHEDIT.H",
            "VERRSRC.H", "DLGS.H", "WINDEF.H", "MINWINDEF.H", "WINNT.H", "WINBASE.H", "WINGDI.H", "WINNLS.H", "AFXRES.RC", "PRSHT.H", "SHLOBJ.H",
            "SHELLAPI.H", "OLECTL.H", "NTVERP.H", "COMMDLG.H", "WINUSER.RH", "WINNT.RH", "SDKDDKVER.H", "TARGETVER.H"};
        std::string base = name;
        const std::size_t slash = base.find_last_of("/\\");
        if (slash != std::string::npos)
            base = base.substr(slash + 1);
        return headers.count(upper(base)) > 0;
    }

    long long evaluate(const std::vector<Token>& tokens, const std::string& where) const;

    void processText(const std::string& text, const std::string& path, bool statements)
    {
        const std::size_t slash = path.find_last_of('/');
        const std::string dir = slash == std::string::npos ? std::string() : path.substr(0, slash);
        // Líneas lógicas: sin comentarios y con las continuaciones unidas
        std::vector<std::pair<std::string, int>> lines;
        std::string current;
        int lineNumber = 1;
        int startLine = 1;
        bool inBlockComment = false;
        bool inString = false;
        for (std::size_t i = 0; i <= text.size(); ++i) {
            const char c = i < text.size() ? text[i] : '\n';
            if (inBlockComment) {
                if (c == '*' && i + 1 < text.size() && text[i + 1] == '/') {
                    inBlockComment = false;
                    ++i;
                    current += ' ';
                } else if (c == '\n') {
                    ++lineNumber;
                }
                continue;
            }
            if (!inString && c == '/' && i + 1 < text.size() && text[i + 1] == '*') {
                inBlockComment = true;
                ++i;
                continue;
            }
            if (!inString && c == '/' && i + 1 < text.size() && text[i + 1] == '/') {
                while (i < text.size() && text[i] != '\n')
                    ++i;
                --i;
                continue;
            }
            if (c == '"') {
                if (inString && i + 1 < text.size() && text[i + 1] == '"') {
                    current += "\"\"";
                    ++i;
                    continue;
                }
                inString = !inString;
            }
            if (c == '\\' && i + 1 < text.size() && (text[i + 1] == '\n' || (text[i + 1] == '\r' && i + 2 < text.size() && text[i + 2] == '\n'))) {
                i += text[i + 1] == '\r' ? 2u : 1u;
                ++lineNumber;
                continue;
            }
            if (c == '\n') {
                ++lineNumber;
                // Como rc.exe: una cadena puede seguir en la línea siguiente
                if (inString && i < text.size()) {
                    if (!current.empty() && current.back() == '\r')
                        current.pop_back();
                    current += "\\n";
                    continue;
                }
                lines.emplace_back(current, startLine);
                current.clear();
                inString = false;
                startLine = lineNumber;
                continue;
            }
            if (c != '\r')
                current += c;
        }

        std::vector<Conditional> stack;
        auto active = [&] { return stack.empty() || stack.back().active; };
        for (const auto& [line, number] : lines) {
            const std::string where = path + ":" + std::to_string(number);
            std::size_t first = line.find_first_not_of(" \t");
            if (first != std::string::npos && line[first] == '#') {
                std::size_t p = line.find_first_not_of(" \t", first + 1);
                std::size_t end = p;
                while (end < line.size() && isIdentChar(line[end]))
                    ++end;
                const std::string directive = p == std::string::npos ? std::string() : line.substr(p, end - p);
                const std::string rest = end < line.size() ? line.substr(end) : std::string();
                if (directive == "if" || directive == "ifdef" || directive == "ifndef") {
                    const bool parent = active();
                    bool value = false;
                    if (parent) {
                        if (directive == "if") {
                            value = evaluate(expand(tokenize(rest, where)), where) != 0;
                        } else {
                            const std::vector<Token> name = tokenize(rest, where);
                            const bool isDefined = !name.empty() && macros.count(name.front().text);
                            value = directive == "ifdef" ? isDefined : !isDefined;
                        }
                    }
                    stack.push_back({parent && value, parent && value, parent});
                } else if (directive == "elif") {
                    if (stack.empty())
                        fatal(where + ": #elif sin #if");
                    Conditional& c = stack.back();
                    if (c.parentActive && !c.taken && evaluate(expand(tokenize(rest, where)), where) != 0) {
                        c.active = true;
                        c.taken = true;
                    } else {
                        c.active = false;
                    }
                } else if (directive == "else") {
                    if (stack.empty())
                        fatal(where + ": #else sin #if");
                    Conditional& c = stack.back();
                    c.active = c.parentActive && !c.taken;
                    c.taken = true;
                } else if (directive == "endif") {
                    if (stack.empty())
                        fatal(where + ": #endif sin #if");
                    stack.pop_back();
                } else if (!active()) {
                    continue;
                } else if (directive == "define") {
                    const std::vector<Token> tokens = tokenize(rest, where);
                    if (tokens.empty() || tokens.front().kind != Kind::Identifier)
                        continue;
                    // NOMBRE(…) sin espacio antes del paréntesis: una macro con argumentos
                    Macro macro;
                    std::size_t bodyStart = 1;
                    if (tokens.size() > 1 && tokens[1].text == "(" && !tokens[1].spaceBefore) {
                        macro.function = true;
                        std::size_t k = 2;
                        for (; k < tokens.size() && tokens[k].text != ")"; ++k) {
                            if (tokens[k].kind == Kind::Identifier)
                                macro.params.push_back(tokens[k].text);
                        }
                        bodyStart = k + 1;
                    }
                    if (bodyStart < tokens.size())
                        macro.body.assign(tokens.begin() + static_cast<std::ptrdiff_t>(bodyStart), tokens.end());
                    macros[tokens.front().text] = macro;
                } else if (directive == "undef") {
                    const std::vector<Token> tokens = tokenize(rest, where);
                    if (!tokens.empty())
                        macros.erase(tokens.front().text);
                } else if (directive == "include") {
                    std::string name = rest;
                    const std::size_t open = name.find_first_of("\"<");
                    const std::size_t close = open == std::string::npos ? std::string::npos : name.find_first_of("\">", open + 1);
                    if (open == std::string::npos || close == std::string::npos)
                        continue;
                    name = name.substr(open + 1, close - open - 1);
                    if (knownWindowsHeader(name))
                        continue;
                    const std::string found = findInclude(name, dir);
                    if (found.empty()) {
                        warning(where, "no se encuentra " + name + " (se ignora)");
                        continue;
                    }
                    std::string lowered = found;
                    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                    const bool resourceFile = lowered.size() > 3
                                              && (lowered.rfind(".rc") == lowered.size() - 3 || lowered.rfind(".rc2") == lowered.size() - 4
                                                  || lowered.rfind(".dlg") == lowered.size() - 4);
                    processFile(found, statements && resourceFile);
                }
                // #pragma, #error y los demás: nada que hacer
                continue;
            }
            if (!active() || !statements)
                continue;
            for (const Token& t : expand(tokenize(line, where)))
                output.push_back(t);
        }
        if (!stack.empty())
            warning(path, "falta #endif");
    }
};

// --- Expresiones ----------------------------------------------------------------------------

long long constantValue(const std::string& name, bool* found)
{
    for (std::size_t i = 0; i < kConstantCount; ++i) {
        if (name == kConstants[i].name) {
            *found = true;
            return kConstants[i].value;
        }
    }
    *found = false;
    return 0;
}

class ExpressionParser
{
public:
    ExpressionParser(const std::vector<Token>& tokens, std::size_t& pos) : _tokens(tokens), _pos(pos) {}

    long long parse(int minLevel = 0) { return binary(minLevel); }

private:
    const std::vector<Token>& _tokens;
    std::size_t& _pos;

    const Token* peek() const { return _pos < _tokens.size() ? &_tokens[_pos] : nullptr; }

    static int level(const std::string& op)
    {
        if (op == "||") return 1;
        if (op == "&&") return 2;
        if (op == "|") return 3;
        if (op == "^") return 4;
        if (op == "&") return 5;
        if (op == "==" || op == "!=") return 6;
        if (op == "<" || op == ">" || op == "<=" || op == ">=") return 7;
        if (op == "<<" || op == ">>") return 8;
        if (op == "+" || op == "-") return 9;
        if (op == "*" || op == "/" || op == "%") return 10;
        return 0;
    }

    long long binary(int minLevel)
    {
        long long left = unary();
        for (;;) {
            const Token* t = peek();
            if (!t || t->kind != Kind::Punct)
                break;
            const int l = level(t->text);
            if (l == 0 || l < minLevel)
                break;
            const std::string op = t->text;
            ++_pos;
            const long long right = binary(l + 1);
            if (op == "||") left = left || right;
            else if (op == "&&") left = left && right;
            else if (op == "|") left |= right;
            else if (op == "^") left ^= right;
            else if (op == "&") left &= right;
            else if (op == "==") left = left == right;
            else if (op == "!=") left = left != right;
            else if (op == "<") left = left < right;
            else if (op == ">") left = left > right;
            else if (op == "<=") left = left <= right;
            else if (op == ">=") left = left >= right;
            else if (op == "<<") left <<= right;
            else if (op == ">>") left >>= right;
            else if (op == "+") left += right;
            else if (op == "-") left -= right;
            else if (op == "*") left *= right;
            else if (op == "/") left = right ? left / right : 0;
            else if (op == "%") left = right ? left % right : 0;
        }
        return left;
    }

    long long unary()
    {
        const Token* t = peek();
        if (!t)
            return 0;
        if (t->kind == Kind::Punct) {
            if (t->text == "(") {
                ++_pos;
                const long long value = binary(0);
                if (peek() && peek()->text == ")")
                    ++_pos;
                return value;
            }
            if (t->text == "-") { ++_pos; return -unary(); }
            if (t->text == "+") { ++_pos; return unary(); }
            if (t->text == "~") { ++_pos; return ~unary(); }
            if (t->text == "!") { ++_pos; return !unary(); }
            return 0;
        }
        ++_pos;
        if (t->kind == Kind::Number)
            return t->number;
        if (t->kind == Kind::Identifier) {
            if (upper(t->text) == "NOT")
                return ~unary();
            bool found = false;
            const long long value = constantValue(t->text, &found);
            if (!found)
                warning(t->where, "símbolo desconocido " + t->text + " (vale 0)");
            return value;
        }
        return 0;
    }
};

long long Preprocessor::evaluate(const std::vector<Token>& tokens, const std::string&) const
{
    std::size_t pos = 0;
    // En #if, un identificador que no es una macro vale 0 (sin aviso)
    std::vector<Token> resolved;
    for (Token t : tokens) {
        if (t.kind == Kind::Identifier) {
            bool found = false;
            const long long value = constantValue(t.text, &found);
            t.kind = Kind::Number;
            t.number = found ? value : 0;
        }
        resolved.push_back(t);
    }
    ExpressionParser parser(resolved, pos);
    return parser.parse();
}

// --- Recursos --------------------------------------------------------------------------

struct ResourceName
{
    bool isNumber = true;
    unsigned id = 0;
    std::string name;
};

struct Control
{
    std::string className;   // "BUTTON"… o el nombre de la clase
    unsigned atom = 0;       // 0x80…0x85 si es una clase estándar
    std::string text;
    bool textIsOrdinal = false;
    unsigned textOrdinal = 0;
    long long id = 0;
    uint32_t style = 0;
    uint32_t exStyle = 0;
    uint32_t helpId = 0;
    int x = 0, y = 0, cx = 0, cy = 0;
};

struct Dialog
{
    ResourceName name;
    unsigned language = 0;
    bool extended = false;
    uint32_t style = 0;
    uint32_t exStyle = 0;
    uint32_t helpId = 0;
    int x = 0, y = 0, cx = 0, cy = 0;
    std::string caption;
    bool hasFont = false;
    unsigned pointSize = 8;
    unsigned weight = 0;
    unsigned italic = 0;
    unsigned charset = 1;
    std::string face;
    ResourceName menu;
    bool hasMenu = false;
    std::string windowClass;
    std::vector<Control> controls;
};

struct MenuItem
{
    bool popup = false;
    bool separator = false;
    std::string text;
    unsigned id = 0;
    unsigned flags = 0;      // MF_CHECKED, MF_GRAYED…
    std::vector<MenuItem> children;
};

struct Menu
{
    ResourceName name;
    unsigned language = 0;
    std::vector<MenuItem> items;
};

struct StringEntry
{
    unsigned id = 0;
    unsigned language = 0;
    std::string text;
};

class Parser
{
public:
    explicit Parser(const std::vector<Token>& tokens) : _t(tokens) {}

    std::vector<Dialog> dialogs;
    std::vector<Menu> menus;
    std::vector<StringEntry> strings;

    void parse()
    {
        while (!atEnd()) {
            const Token& first = peek();
            const std::string keyword = upper(first.text);
            if (first.kind == Kind::Identifier && keyword == "LANGUAGE") {
                ++_pos;
                const long long primary = expr();
                skip(",");
                const long long sub = expr();
                _language = static_cast<unsigned>(((sub & 0x3F) << 10) | (primary & 0x3FF));
                continue;
            }
            if (first.kind == Kind::Identifier && keyword == "STRINGTABLE") {
                ++_pos;
                parseStringTable();
                continue;
            }
            if (first.kind == Kind::Identifier && (keyword == "VERSION" || keyword == "CHARACTERISTICS")) {
                ++_pos;
                expr();
                continue;
            }
            // nombre TIPO …
            ResourceName name = resourceName();
            if (atEnd())
                break;
            const Token& typeToken = next();
            const std::string type = upper(typeToken.text);
            if (typeToken.kind == Kind::Identifier && (type == "DIALOG" || type == "DIALOGEX")) {
                parseDialog(name, type == "DIALOGEX");
            } else if (typeToken.kind == Kind::Identifier && (type == "MENU" || type == "MENUEX")) {
                parseMenu(name, type == "MENUEX");
            } else {
                skipResource(typeToken);
            }
        }
    }

private:
    const std::vector<Token>& _t;
    std::size_t _pos = 0;
    unsigned _language = 0;

    bool atEnd() const { return _pos >= _t.size(); }
    const Token& peek() const
    {
        static const Token end;
        return _pos < _t.size() ? _t[_pos] : end;
    }
    const Token& next()
    {
        if (atEnd())
            fatal("el .rc termina antes de tiempo");
        return _t[_pos++];
    }
    bool is(const char* text) const { return !atEnd() && peek().kind == Kind::Punct && peek().text == text; }
    bool isKeyword(const char* word) const { return !atEnd() && peek().kind == Kind::Identifier && upper(peek().text) == word; }
    void skip(const char* punct)
    {
        if (is(punct))
            ++_pos;
    }
    bool isBegin() const { return is("{") || isKeyword("BEGIN"); }
    bool isEndBlock() const { return is("}") || isKeyword("END"); }

    long long expr()
    {
        ExpressionParser parser(_t, _pos);
        return parser.parse();
    }

    // Una lista de estilos: a | b | NOT c (NOT quita los bits)
    uint32_t styleList(uint32_t value)
    {
        for (;;) {
            if (isKeyword("NOT")) {
                ++_pos;
                ExpressionParser parser(_t, _pos);
                value &= ~static_cast<uint32_t>(parser.parse(4));
            } else {
                ExpressionParser parser(_t, _pos);
                value |= static_cast<uint32_t>(parser.parse(4));
            }
            if (is("|")) {
                ++_pos;
                continue;
            }
            break;
        }
        return value;
    }

    ResourceName resourceName()
    {
        ResourceName name;
        const Token& t = peek();
        if (t.kind == Kind::Number || (t.kind == Kind::Punct && (t.text == "(" || t.text == "-"))) {
            name.id = static_cast<unsigned>(expr());
            return name;
        }
        ++_pos;
        name.isNumber = false;
        name.name = upper(t.text);
        return name;
    }

    std::string stringValue()
    {
        std::string text;
        while (!atEnd() && peek().kind == Kind::String)
            text += next().text;
        return text;
    }

    void skipBlock()
    {
        int depth = 0;
        while (!atEnd()) {
            if (isBegin())
                ++depth;
            else if (isEndBlock() && --depth == 0) {
                ++_pos;
                return;
            }
            ++_pos;
        }
    }

    // Iconos, VERSIONINFO, aceleradores…: una línea con un archivo o un bloque
    void skipResource(const Token& typeToken)
    {
        const std::string line = typeToken.where;
        // En la misma línea hay una cadena o un nombre de archivo: es un recurso de archivo
        std::size_t p = _pos;
        bool sameLineValue = false;
        while (p < _t.size() && _t[p].where == line) {
            if (_t[p].kind == Kind::String || (_t[p].kind == Kind::Identifier && !isBlockKeyword(_t[p].text)))
                sameLineValue = true;
            if (_t[p].kind == Kind::Identifier && upper(_t[p].text) == "BEGIN")
                break;
            ++p;
        }
        if (sameLineValue && !(p < _t.size() && _t[p].where == line && upper(_t[p].text) == "BEGIN")) {
            _pos = p;
            return;
        }
        while (!atEnd() && !isBegin())
            ++_pos;
        skipBlock();
    }

    static bool isBlockKeyword(const std::string& word)
    {
        const std::string w = upper(word);
        return w == "BEGIN" || w == "DISCARDABLE" || w == "MOVEABLE" || w == "PURE" || w == "PRELOAD" || w == "LOADONCALL"
               || w == "FIXED" || w == "IMPURE";
    }

    void skipMemoryOptions()
    {
        while (!atEnd() && peek().kind == Kind::Identifier && isBlockKeyword(peek().text) && upper(peek().text) != "BEGIN")
            ++_pos;
    }

    void parseStringTable()
    {
        while (!atEnd() && !isBegin())
            ++_pos; // DISCARDABLE, LANGUAGE…
        if (atEnd())
            return;
        ++_pos;
        while (!atEnd() && !isEndBlock()) {
            StringEntry entry;
            entry.id = static_cast<unsigned>(expr());
            entry.language = _language;
            skip(",");
            entry.text = stringValue();
            strings.push_back(entry);
        }
        ++_pos;
    }

    void parseDialog(const ResourceName& name, bool extended)
    {
        Dialog d;
        d.name = name;
        d.language = _language;
        d.extended = extended;
        skipMemoryOptions();
        d.x = static_cast<int>(expr());
        skip(",");
        d.y = static_cast<int>(expr());
        skip(",");
        d.cx = static_cast<int>(expr());
        skip(",");
        d.cy = static_cast<int>(expr());
        if (is(",")) {
            ++_pos;
            d.helpId = static_cast<uint32_t>(expr());
        }
        bool hasStyle = false;
        uint32_t style = 0;
        while (!atEnd() && !isBegin()) {
            const std::string option = upper(next().text);
            if (option == "STYLE") {
                hasStyle = true;
                style = styleList(0);
            } else if (option == "EXSTYLE") {
                d.exStyle = styleList(0);
            } else if (option == "CAPTION") {
                d.caption = stringValue();
            } else if (option == "FONT") {
                d.hasFont = true;
                d.pointSize = static_cast<unsigned>(expr());
                skip(",");
                d.face = stringValue();
                if (is(",")) { ++_pos; d.weight = static_cast<unsigned>(expr()); }
                if (is(",")) { ++_pos; d.italic = static_cast<unsigned>(expr()); }
                if (is(",")) { ++_pos; d.charset = static_cast<unsigned>(expr()); }
            } else if (option == "MENU") {
                d.hasMenu = true;
                d.menu = resourceName();
            } else if (option == "CLASS") {
                d.windowClass = peek().kind == Kind::String ? stringValue() : next().text;
            } else if (option == "LANGUAGE") {
                const long long primary = expr();
                skip(",");
                const long long sub = expr();
                d.language = static_cast<unsigned>(((sub & 0x3F) << 10) | (primary & 0x3FF));
            } else if (option == "VERSION" || option == "CHARACTERISTICS") {
                expr();
            }
        }
        // Como rc.exe: sin STYLE, WS_POPUP | WS_BORDER | WS_SYSMENU; CAPTION añade WS_CAPTION
        d.style = hasStyle ? style : static_cast<uint32_t>(0x80000000u | 0x00800000u | 0x00080000u);
        if (!d.caption.empty())
            d.style |= 0x00C00000u;
        if (d.hasFont)
            d.style |= 0x40u; // DS_SETFONT
        if (atEnd())
            fatal("falta BEGIN en el diálogo");
        ++_pos;
        while (!atEnd() && !isEndBlock())
            d.controls.push_back(parseControl(d.extended));
        ++_pos;
        dialogs.push_back(d);
    }

    Control parseControl(bool extended)
    {
        const Token& kw = next();
        const std::string type = upper(kw.text);
        Control c;
        constexpr uint32_t kChild = 0x40000000u, kVisible = 0x10000000u, kTab = 0x00010000u, kGroup = 0x00020000u, kBorder = 0x00800000u;
        auto readText = [&] {
            if (peek().kind == Kind::String) {
                c.text = stringValue();
            } else {
                c.textIsOrdinal = true;
                c.textOrdinal = static_cast<unsigned>(expr() & 0xFFFF);
            }
            skip(",");
        };
        auto readRect = [&] {
            c.x = static_cast<int>(expr()); skip(",");
            c.y = static_cast<int>(expr()); skip(",");
            c.cx = static_cast<int>(expr()); skip(",");
            c.cy = static_cast<int>(expr());
        };
        auto readStyles = [&](uint32_t base) {
            c.style = base;
            if (is(",")) {
                ++_pos;
                c.style = styleList(base);
                if (is(",")) {
                    ++_pos;
                    c.exStyle = styleList(0);
                    if (extended && is(",")) {
                        ++_pos;
                        c.helpId = static_cast<uint32_t>(expr());
                    }
                }
            }
        };
        auto standard = [&](const char* className, unsigned atom, uint32_t base, bool hasText) {
            c.className = className;
            c.atom = atom;
            if (hasText)
                readText();
            c.id = expr();
            skip(",");
            readRect();
            readStyles(base | kChild | kVisible);
        };
        if (type == "CONTROL") {
            readText();
            c.id = expr();
            skip(",");
            const std::string className = peek().kind == Kind::String ? stringValue() : next().text;
            skip(",");
            static const std::map<std::string, std::pair<std::string, unsigned>> classes = {
                {"BUTTON", {"BUTTON", 0x80}}, {"EDIT", {"EDIT", 0x81}}, {"STATIC", {"STATIC", 0x82}},
                {"LISTBOX", {"LISTBOX", 0x83}}, {"SCROLLBAR", {"SCROLLBAR", 0x84}}, {"COMBOBOX", {"COMBOBOX", 0x85}},
                {"WC_BUTTON", {"BUTTON", 0x80}}, {"WC_EDIT", {"EDIT", 0x81}}, {"WC_STATIC", {"STATIC", 0x82}},
                {"WC_LISTBOX", {"LISTBOX", 0x83}}, {"WC_SCROLLBAR", {"SCROLLBAR", 0x84}}, {"WC_COMBOBOX", {"COMBOBOX", 0x85}},
                {"WC_LISTVIEW", {"SysListView32", 0}}, {"WC_TREEVIEW", {"SysTreeView32", 0}},
                {"WC_TABCONTROL", {"SysTabControl32", 0}}, {"WC_HEADER", {"SysHeader32", 0}},
                {"PROGRESS_CLASS", {"msctls_progress32", 0}}, {"UPDOWN_CLASS", {"msctls_updown32", 0}},
                {"TRACKBAR_CLASS", {"msctls_trackbar32", 0}}, {"WC_LINK", {"SysLink", 0}},
                {"WC_COMBOBOXEX", {"ComboBoxEx32", 0}}, {"WC_IPADDRESS", {"SysIPAddress32", 0}},
                {"MONTHCAL_CLASS", {"SysMonthCal32", 0}}, {"DATETIMEPICK_CLASS", {"SysDateTimePick32", 0}},
                {"HOTKEY_CLASS", {"msctls_hotkey32", 0}}, {"ANIMATE_CLASS", {"SysAnimate32", 0}}};
            const auto it = classes.find(upper(className));
            if (it != classes.end()) {
                c.className = it->second.first;
                c.atom = it->second.second;
            } else {
                c.className = className;
            }
            c.style = styleList(kChild | kVisible);
            skip(",");
            readRect();
            if (is(",")) {
                ++_pos;
                c.exStyle = styleList(0);
                if (extended && is(",")) {
                    ++_pos;
                    c.helpId = static_cast<uint32_t>(expr());
                }
            }
            return c;
        }
        if (type == "LTEXT") { standard("STATIC", 0x82, 0x0 | kGroup, true); return c; }
        if (type == "RTEXT") { standard("STATIC", 0x82, 0x2 | kGroup, true); return c; }
        if (type == "CTEXT") { standard("STATIC", 0x82, 0x1 | kGroup, true); return c; }
        if (type == "PUSHBUTTON") { standard("BUTTON", 0x80, 0x0 | kTab, true); return c; }
        if (type == "DEFPUSHBUTTON") { standard("BUTTON", 0x80, 0x1 | kTab, true); return c; }
        if (type == "CHECKBOX") { standard("BUTTON", 0x80, 0x2 | kTab, true); return c; }
        if (type == "AUTOCHECKBOX") { standard("BUTTON", 0x80, 0x3 | kTab, true); return c; }
        if (type == "RADIOBUTTON") { standard("BUTTON", 0x80, 0x4, true); return c; }
        if (type == "STATE3") { standard("BUTTON", 0x80, 0x5 | kTab, true); return c; }
        if (type == "AUTO3STATE") { standard("BUTTON", 0x80, 0x6 | kTab, true); return c; }
        if (type == "GROUPBOX") { standard("BUTTON", 0x80, 0x7, true); return c; }
        if (type == "AUTORADIOBUTTON") { standard("BUTTON", 0x80, 0x9, true); return c; }
        if (type == "PUSHBOX") { standard("BUTTON", 0x80, 0xA | kTab, true); return c; }
        if (type == "EDITTEXT") { standard("EDIT", 0x81, 0x0 | kBorder | kTab, false); return c; }
        if (type == "COMBOBOX") { standard("COMBOBOX", 0x85, 0x1 | kTab, false); return c; }
        if (type == "LISTBOX") { standard("LISTBOX", 0x83, 0x1 | kBorder, false); return c; }
        if (type == "SCROLLBAR") { standard("SCROLLBAR", 0x84, 0x0, false); return c; }
        if (type == "ICON") {
            c.className = "STATIC";
            c.atom = 0x82;
            readText();
            c.id = expr();
            skip(",");
            c.x = static_cast<int>(expr());
            skip(",");
            c.y = static_cast<int>(expr());
            c.style = 0x3 | kChild | kVisible; // SS_ICON
            if (is(",")) {
                ++_pos;
                c.cx = static_cast<int>(expr());
                skip(",");
                c.cy = static_cast<int>(expr());
                if (is(",")) {
                    ++_pos;
                    c.style = styleList(c.style);
                }
            }
            return c;
        }
        fatal(kw.where + ": control desconocido " + kw.text);
    }

    void parseMenuItems(std::vector<MenuItem>& items, bool extended)
    {
        if (!isBegin())
            fatal(peek().where + ": falta BEGIN en el menú");
        ++_pos;
        while (!atEnd() && !isEndBlock()) {
            const std::string kw = upper(next().text);
            MenuItem item;
            if (kw == "POPUP") {
                item.popup = true;
                item.text = stringValue();
                if (extended) {
                    // POPUP "texto" [, id [, tipo [, estado [, ayuda]]]]
                    for (int field = 0; field < 4 && is(","); ++field) {
                        ++_pos;
                        const long long value = expr();
                        if (field == 2)
                            item.flags |= stateFlags(static_cast<unsigned>(value));
                    }
                } else {
                    while (!atEnd() && peek().kind != Kind::String && !isBegin()) {
                        item.flags |= optionFlag(next().text);
                        skip(",");
                    }
                }
                parseMenuItems(item.children, extended);
            } else if (kw == "MENUITEM") {
                if (isKeyword("SEPARATOR")) {
                    ++_pos;
                    item.separator = true;
                } else {
                    item.text = stringValue();
                    if (extended) {
                        // MENUITEM "texto" [, id [, tipo [, estado]]]
                        long long fields[3] = {0, 0, 0};
                        for (int field = 0; field < 3 && is(","); ++field) {
                            ++_pos;
                            fields[field] = expr();
                        }
                        item.id = static_cast<unsigned>(fields[0]);
                        if (fields[1] & 0x800) // MFT_SEPARATOR
                            item.separator = true;
                        item.flags |= stateFlags(static_cast<unsigned>(fields[2]));
                    } else {
                        skip(",");
                        item.id = static_cast<unsigned>(expr());
                        while (is(",")) {
                            ++_pos;
                            item.flags |= optionFlag(next().text);
                        }
                    }
                }
            } else {
                fatal("menú: se esperaba POPUP o MENUITEM y hay " + kw);
            }
            items.push_back(item);
        }
        ++_pos;
    }

    static unsigned optionFlag(const std::string& option)
    {
        const std::string o = upper(option);
        if (o == "CHECKED") return 0x8;
        if (o == "GRAYED") return 0x1;
        if (o == "INACTIVE") return 0x2;
        if (o == "MENUBARBREAK") return 0x20;
        if (o == "MENUBREAK") return 0x40;
        if (o == "HELP") return 0x4000;
        return 0;
    }

    static unsigned stateFlags(unsigned state)
    {
        unsigned flags = 0;
        if (state & 0x8) flags |= 0x8;   // MFS_CHECKED
        if (state & 0x3) flags |= 0x1;   // MFS_GRAYED
        return flags;
    }

    void parseMenu(const ResourceName& name, bool extended)
    {
        Menu m;
        m.name = name;
        m.language = _language;
        while (!atEnd() && !isBegin())
            ++_pos; // DISCARDABLE…
        parseMenuItems(m.items, extended);
        menus.push_back(m);
    }
};

// --- Plantillas binarias de Windows ------------------------------------------------------

class Writer
{
public:
    std::vector<unsigned char> bytes;

    void word(unsigned value)
    {
        bytes.push_back(static_cast<unsigned char>(value & 0xFF));
        bytes.push_back(static_cast<unsigned char>((value >> 8) & 0xFF));
    }
    void dword(uint32_t value)
    {
        word(value & 0xFFFF);
        word(value >> 16);
    }
    void shortValue(int value) { word(static_cast<unsigned>(static_cast<uint16_t>(static_cast<int16_t>(value)))); }
    void byte(unsigned value) { bytes.push_back(static_cast<unsigned char>(value & 0xFF)); }
    void align()
    {
        while (bytes.size() % 4)
            bytes.push_back(0);
    }
    // UTF-16 con el nulo
    void text(const std::string& utf8)
    {
        for (uint32_t cp : codePoints(utf8)) {
            if (cp >= 0x10000) {
                cp -= 0x10000;
                word(0xD800 + (cp >> 10));
                word(0xDC00 + (cp & 0x3FF));
            } else {
                word(cp);
            }
        }
        word(0);
    }
    void nameOrOrdinal(const ResourceName& name)
    {
        if (name.isNumber) {
            word(0xFFFF);
            word(name.id);
        } else {
            text(name.name);
        }
    }
};

std::vector<unsigned char> dialogTemplate(const Dialog& d)
{
    // Siempre DLGTEMPLATEEX: la capa lee los dos formatos
    Writer w;
    w.word(1);
    w.word(0xFFFF);
    w.dword(d.helpId);
    w.dword(d.exStyle);
    w.dword(d.style);
    w.word(static_cast<unsigned>(d.controls.size()));
    w.shortValue(d.x);
    w.shortValue(d.y);
    w.shortValue(d.cx);
    w.shortValue(d.cy);
    if (d.hasMenu)
        w.nameOrOrdinal(d.menu);
    else
        w.word(0);
    if (d.windowClass.empty())
        w.word(0);
    else
        w.text(d.windowClass);
    w.text(d.caption);
    if (d.style & 0x48u) { // DS_SETFONT | DS_SHELLFONT
        w.word(d.pointSize);
        w.word(d.weight);
        w.byte(d.italic);
        w.byte(d.charset);
        w.text(d.face);
    }
    for (const Control& c : d.controls) {
        w.align();
        w.dword(c.helpId);
        w.dword(c.exStyle);
        w.dword(c.style);
        w.shortValue(c.x);
        w.shortValue(c.y);
        w.shortValue(c.cx);
        w.shortValue(c.cy);
        w.dword(static_cast<uint32_t>(c.id));
        if (c.atom) {
            w.word(0xFFFF);
            w.word(c.atom);
        } else {
            w.text(c.className);
        }
        if (c.textIsOrdinal) {
            w.word(0xFFFF);
            w.word(c.textOrdinal);
        } else {
            w.text(c.text);
        }
        w.word(0); // sin datos de creación
    }
    return w.bytes;
}

void writeMenuItems(Writer& w, const std::vector<MenuItem>& items)
{
    for (std::size_t i = 0; i < items.size(); ++i) {
        const MenuItem& item = items[i];
        unsigned flags = item.flags;
        if (item.popup)
            flags |= 0x10;   // MF_POPUP
        if (i + 1 == items.size())
            flags |= 0x80;   // MF_END
        if (item.separator) {
            // MENUITEM SEPARATOR: sin flags, id 0 y sin texto
            w.word(flags & 0x80);
            w.word(0);
            w.word(0);
            continue;
        }
        w.word(flags);
        if (!item.popup)
            w.word(item.id);
        w.text(item.text);
        if (item.popup)
            writeMenuItems(w, item.children);
    }
}

std::vector<unsigned char> menuTemplate(const Menu& m)
{
    Writer w;
    w.word(0); // versionNumber
    w.word(0); // offset
    writeMenuItems(w, m.items);
    return w.bytes;
}

// Si un recurso está en varios idiomas: neutro o inglés, si no el primero
bool preferred(unsigned language, unsigned current)
{
    auto score = [](unsigned l) { const unsigned primary = l & 0x3FF; return primary == 0 ? 2 : primary == 0x09 ? 1 : 0; };
    return score(language) > score(current);
}

std::string key(const ResourceName& n)
{
    return n.isNumber ? "#" + std::to_string(n.id) : n.name;
}

void writeBytes(std::ostream& out, const std::string& symbol, const std::vector<unsigned char>& data)
{
    out << "alignas(4) const unsigned char " << symbol << "[] = {";
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (i % 16 == 0)
            out << "\n    ";
        out << static_cast<unsigned>(data[i]) << ",";
    }
    if (data.empty())
        out << "0";
    out << "\n};\n";
}

std::string cString(const std::string& s)
{
    std::string out = "\"";
    for (const char ch : s) {
        const auto c = static_cast<unsigned char>(ch);
        if (c == '"' || c == '\\') {
            out += '\\';
            out += static_cast<char>(c);
        } else if (c < 0x20 || c >= 0x7F) {
            char buffer[8];
            std::snprintf(buffer, sizeof buffer, "\\%03o", c);
            out += buffer;
        } else {
            out += static_cast<char>(c);
        }
    }
    return out + "\"";
}

} // namespace

int main(int argc, char** argv)
{
    std::string output;
    Preprocessor pp;
    std::vector<std::string> inputs;
    pp.define("RC_INVOKED", "1");
    pp.define("_WIN32", "1");
    pp.define("WIN32", "1");
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            output = argv[++i];
        } else if (arg.rfind("-I", 0) == 0) {
            pp.includeDirs.push_back(arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : ""));
        } else if (arg.rfind("-D", 0) == 0) {
            const std::string def = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "");
            const std::size_t eq = def.find('=');
            pp.define(def.substr(0, eq), eq == std::string::npos ? "1" : def.substr(eq + 1));
        } else {
            inputs.push_back(arg);
        }
    }
    if (output.empty())
        fatal("uso: edita_rc -o recursos.cpp [-I carpeta]… [-D NOMBRE[=valor]]… archivo.rc…");

    std::vector<Dialog> dialogs;
    std::vector<Menu> menus;
    std::vector<StringEntry> strings;
    for (const std::string& input : inputs) {
        pp.output.clear();
        pp.processFile(input, true);
        Parser parser(pp.output);
        parser.parse();
        dialogs.insert(dialogs.end(), parser.dialogs.begin(), parser.dialogs.end());
        menus.insert(menus.end(), parser.menus.begin(), parser.menus.end());
        strings.insert(strings.end(), parser.strings.begin(), parser.strings.end());
    }

    // Un recurso por nombre (el idioma preferido)
    std::map<std::string, std::size_t> chosenDialogs, chosenMenus;
    for (std::size_t i = 0; i < dialogs.size(); ++i) {
        const auto it = chosenDialogs.find(key(dialogs[i].name));
        if (it == chosenDialogs.end() || preferred(dialogs[i].language, dialogs[it->second].language))
            chosenDialogs[key(dialogs[i].name)] = i;
    }
    for (std::size_t i = 0; i < menus.size(); ++i) {
        const auto it = chosenMenus.find(key(menus[i].name));
        if (it == chosenMenus.end() || preferred(menus[i].language, menus[it->second].language))
            chosenMenus[key(menus[i].name)] = i;
    }
    std::map<unsigned, std::size_t> chosenStrings;
    for (std::size_t i = 0; i < strings.size(); ++i) {
        const auto it = chosenStrings.find(strings[i].id);
        if (it == chosenStrings.end() || preferred(strings[i].language, strings[it->second].language))
            chosenStrings[strings[i].id] = i;
    }

    std::ostringstream out;
    out << "// Generado por edita_rc a partir de:";
    for (const std::string& input : inputs)
        out << " " << input;
    out << "\n// No editar: se vuelve a generar al compilar.\n\n"
           "#include \"npp_compat.h\"\n\nnamespace {\n\n";
    struct Entry { unsigned type; std::string symbol; ResourceName name; std::size_t size; };
    std::vector<Entry> entries;
    int n = 0;
    for (const auto& [name, index] : chosenDialogs) {
        const std::string symbol = "resource" + std::to_string(n++);
        const std::vector<unsigned char> data = dialogTemplate(dialogs[index]);
        writeBytes(out, symbol, data);
        entries.push_back({5, symbol, dialogs[index].name, data.size()});
    }
    for (const auto& [name, index] : chosenMenus) {
        const std::string symbol = "resource" + std::to_string(n++);
        const std::vector<unsigned char> data = menuTemplate(menus[index]);
        writeBytes(out, symbol, data);
        entries.push_back({4, symbol, menus[index].name, data.size()});
    }
    for (const auto& [id, index] : chosenStrings) {
        const std::string symbol = "resource" + std::to_string(n++);
        const std::string& text = strings[index].text;
        writeBytes(out, symbol, std::vector<unsigned char>(text.begin(), text.end()));
        ResourceName name;
        name.id = id;
        entries.push_back({6, symbol, name, text.size()});
    }
    out << "\nconst EditaResource kResources[] = {\n";
    for (const Entry& e : entries) {
        out << "    {" << e.type << ", " << (e.name.isNumber ? e.name.id : 0) << ", "
            << (e.name.isNumber ? std::string("nullptr") : cString(e.name.name)) << ", " << e.symbol << ", " << e.size << "},\n";
    }
    if (entries.empty())
        out << "    {0, 0, nullptr, nullptr, 0},\n";
    out << "};\n\n"
           "// Al cargar el plugin, los recursos pasan a FindResource, CreateDialogParam, LoadMenu y LoadString\n"
           "struct Register\n{\n    Register() { edita_compat_setResources(kResources, "
        << entries.size() << "); }\n};\nRegister registerResources;\n\n} // namespace\n";

    std::ofstream file(output, std::ios::binary);
    if (!file)
        fatal("no se puede escribir " + output);
    file << out.str();
    std::fprintf(stderr, "edita_rc: %zu diálogos, %zu menús y %zu cadenas en %s\n", chosenDialogs.size(), chosenMenus.size(),
                 chosenStrings.size(), output.c_str());
    return 0;
}
