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

#include <string_view>

namespace edita {

// Lenguajes. Mismo orden y valores que enum LangType de Notepad++
// (MISC/PluginsManager/Notepad_plus_msgs.h): se guardan en session.xml y los
// usan los plugins, así que no se puede reordenar.
enum class LangType : int
{
    Text = 0, // L_TEXT
    Php = 1, // L_PHP
    C = 2, // L_C
    Cpp = 3, // L_CPP
    CSharp = 4, // L_CS
    ObjC = 5, // L_OBJC
    Java = 6, // L_JAVA
    Rc = 7, // L_RC
    Html = 8, // L_HTML
    Xml = 9, // L_XML
    Makefile = 10, // L_MAKEFILE
    Pascal = 11, // L_PASCAL
    Batch = 12, // L_BATCH
    Ini = 13, // L_INI
    Ascii = 14, // L_ASCII
    User = 15, // L_USER
    Asp = 16, // L_ASP
    Sql = 17, // L_SQL
    Vb = 18, // L_VB
    JsEmbedded = 19, // L_JS_EMBEDDED
    Css = 20, // L_CSS
    Perl = 21, // L_PERL
    Python = 22, // L_PYTHON
    Lua = 23, // L_LUA
    Tex = 24, // L_TEX
    Fortran = 25, // L_FORTRAN
    Bash = 26, // L_BASH
    Flash = 27, // L_FLASH
    Nsis = 28, // L_NSIS
    Tcl = 29, // L_TCL
    Lisp = 30, // L_LISP
    Scheme = 31, // L_SCHEME
    Asm = 32, // L_ASM
    Diff = 33, // L_DIFF
    Props = 34, // L_PROPS
    PostScript = 35, // L_PS
    Ruby = 36, // L_RUBY
    Smalltalk = 37, // L_SMALLTALK
    Vhdl = 38, // L_VHDL
    Kix = 39, // L_KIX
    AutoIt = 40, // L_AU3
    Caml = 41, // L_CAML
    Ada = 42, // L_ADA
    Verilog = 43, // L_VERILOG
    Matlab = 44, // L_MATLAB
    Haskell = 45, // L_HASKELL
    Inno = 46, // L_INNO
    SearchResult = 47, // L_SEARCHRESULT
    CMake = 48, // L_CMAKE
    Yaml = 49, // L_YAML
    Cobol = 50, // L_COBOL
    Gui4Cli = 51, // L_GUI4CLI
    D = 52, // L_D
    PowerShell = 53, // L_POWERSHELL
    R = 54, // L_R
    Jsp = 55, // L_JSP
    CoffeeScript = 56, // L_COFFEESCRIPT
    Json = 57, // L_JSON
    JavaScript = 58, // L_JAVASCRIPT
    Fortran77 = 59, // L_FORTRAN_77
    BaanC = 60, // L_BAANC
    Srec = 61, // L_SREC
    IHex = 62, // L_IHEX
    TeHex = 63, // L_TEHEX
    Swift = 64, // L_SWIFT
    Asn1 = 65, // L_ASN1
    Avs = 66, // L_AVS
    BlitzBasic = 67, // L_BLITZBASIC
    PureBasic = 68, // L_PUREBASIC
    FreeBasic = 69, // L_FREEBASIC
    Csound = 70, // L_CSOUND
    Erlang = 71, // L_ERLANG
    EScript = 72, // L_ESCRIPT
    Forth = 73, // L_FORTH
    Latex = 74, // L_LATEX
    Mmixal = 75, // L_MMIXAL
    Nim = 76, // L_NIM
    Nncrontab = 77, // L_NNCRONTAB
    OScript = 78, // L_OSCRIPT
    Rebol = 79, // L_REBOL
    Registry = 80, // L_REGISTRY
    Rust = 81, // L_RUST
    Spice = 82, // L_SPICE
    Txt2tags = 83, // L_TXT2TAGS
    VisualProlog = 84, // L_VISUALPROLOG
    TypeScript = 85, // L_TYPESCRIPT
    Json5 = 86, // L_JSON5
    MsSql = 87, // L_MSSQL
    GdScript = 88, // L_GDSCRIPT
    Hollywood = 89, // L_HOLLYWOOD
    Go = 90, // L_GOLANG
    Raku = 91, // L_RAKU
    Toml = 92, // L_TOML
    Sas = 93, // L_SAS
    ErrorList = 94, // L_ERRORLIST
    EscSeq = 95, // L_ESCSEQ
    StructuredText = 96, // L_STTXT
    External = 97, // L_EXTERNAL
};

inline constexpr int kLangTypeCount = static_cast<int>(LangType::External) + 1;

// Fila de ScintillaEditView::_langNameInfoArray
struct LanguageInfo
{
    const char* name;      // nombre en langs.xml / stylers.xml ("cpp", "python"…)
    const char* shortName; // "C++", "Python"…
    const char* longName;  // "C++ source file" (barra de estado)
    const char* lexerId;   // lexer de Lexilla ("cpp", "hypertext"…); "null" = sin lexer
    const char* menuName;  // texto del menú Lenguaje de N++; nullptr = no aparece
};

const LanguageInfo& languageInfo(LangType lang);

// NppParameters::getLangIDFromStr: nombre de langs.xml → LangType (Text si no existe)
LangType langTypeFromName(std::string_view name);

// Texto del menú Lenguaje de N++ ("C++", "None (Normal Text)") → LangType, como
// se guarda en session.xml. `found` = false si no corresponde a ningún lenguaje.
LangType langTypeFromMenuName(std::string_view menuName, bool* found = nullptr);

// true si `name` es un nombre de lenguaje conocido
bool isKnownLanguageName(std::string_view name);

} // namespace edita
