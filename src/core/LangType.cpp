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

#include "core/LangType.h"

#include <QtGlobal>

#include <array>

namespace edita {

namespace {

// Generado a partir de ScintillaEditView::_langNameInfoArray y del menú
// Lenguaje de Notepad_plus.rc (referencia/npp @ 6ea1128f)
constexpr std::array<LanguageInfo, kLangTypeCount> kLanguages{{
    {"normal", "Normal text", QT_TRANSLATE_NOOP("LangType", "Normal text file"), "null", "None (Normal Text)"}, // Text
    {"php", "PHP", QT_TRANSLATE_NOOP("LangType", "PHP Hypertext Preprocessor file"), "phpscript", "PHP"}, // Php
    {"c", "C", QT_TRANSLATE_NOOP("LangType", "C source file"), "cpp", "C"}, // C
    {"cpp", "C++", QT_TRANSLATE_NOOP("LangType", "C++ source file"), "cpp", "C++"}, // Cpp
    {"cs", "C#", QT_TRANSLATE_NOOP("LangType", "C# source file"), "cpp", "C#"}, // CSharp
    {"objc", "Objective-C", QT_TRANSLATE_NOOP("LangType", "Objective-C source file"), "objc", "Objective-C"}, // ObjC
    {"java", "Java", QT_TRANSLATE_NOOP("LangType", "Java source file"), "cpp", "Java"}, // Java
    {"rc", "RC", QT_TRANSLATE_NOOP("LangType", "Windows Resource file"), "cpp", "Resource file"}, // Rc
    {"html", "HTML", QT_TRANSLATE_NOOP("LangType", "Hyper Text Markup Language file"), "hypertext", "HTML"}, // Html
    {"xml", "XML", QT_TRANSLATE_NOOP("LangType", "eXtensible Markup Language file"), "xml", "XML"}, // Xml
    {"makefile", "Makefile", QT_TRANSLATE_NOOP("LangType", "Makefile"), "makefile", "Makefile"}, // Makefile
    {"pascal", "Pascal", QT_TRANSLATE_NOOP("LangType", "Pascal source file"), "pascal", "Pascal"}, // Pascal
    {"batch", "Batch", QT_TRANSLATE_NOOP("LangType", "Batch file"), "batch", "Batch"}, // Batch
    {"ini", "ini", QT_TRANSLATE_NOOP("LangType", "MS ini file"), "props", "INI file"}, // Ini
    {"nfo", "NFO", QT_TRANSLATE_NOOP("LangType", "MSDOS Style/ASCII Art"), "null", "MS-DOS Style"}, // Ascii
    {"udf", "udf", QT_TRANSLATE_NOOP("LangType", "User Defined language file"), "user", "User-Defined"}, // User
    {"asp", "ASP", QT_TRANSLATE_NOOP("LangType", "Active Server Pages script file"), "hypertext", "ASP"}, // Asp
    {"sql", "SQL", QT_TRANSLATE_NOOP("LangType", "Structured Query Language file"), "sql", "SQL"}, // Sql
    {"vb", "Visual Basic", QT_TRANSLATE_NOOP("LangType", "Visual Basic file"), "vb", "Visual Basic"}, // Vb
    {"javascript", "Embedded JS", QT_TRANSLATE_NOOP("LangType", "Embedded JavaScript"), "cpp", nullptr}, // JsEmbedded
    {"css", "CSS", QT_TRANSLATE_NOOP("LangType", "Cascade Style Sheets File"), "css", "CSS"}, // Css
    {"perl", "Perl", QT_TRANSLATE_NOOP("LangType", "Perl source file"), "perl", "Perl"}, // Perl
    {"python", "Python", QT_TRANSLATE_NOOP("LangType", "Python file"), "python", "Python"}, // Python
    {"lua", "Lua", QT_TRANSLATE_NOOP("LangType", "Lua source File"), "lua", "Lua"}, // Lua
    {"tex", "TeX", QT_TRANSLATE_NOOP("LangType", "TeX file"), "tex", "TeX"}, // Tex
    {"fortran", "Fortran free form", QT_TRANSLATE_NOOP("LangType", "Fortran free form source file"), "fortran", "Fortran (free form)"}, // Fortran
    {"bash", "Shell", QT_TRANSLATE_NOOP("LangType", "Unix script file"), "bash", "Shell"}, // Bash
    {"actionscript", "ActionScript", QT_TRANSLATE_NOOP("LangType", "Flash ActionScript file"), "cpp", "ActionScript"}, // Flash
    {"nsis", "NSIS", QT_TRANSLATE_NOOP("LangType", "Nullsoft Scriptable Install System script file"), "nsis", "NSIS"}, // Nsis
    {"tcl", "TCL", QT_TRANSLATE_NOOP("LangType", "Tool Command Language file"), "tcl", "TCL"}, // Tcl
    {"lisp", "Lisp", QT_TRANSLATE_NOOP("LangType", "List Processing language file"), "lisp", "LISP"}, // Lisp
    {"scheme", "Scheme", QT_TRANSLATE_NOOP("LangType", "Scheme file"), "lisp", "Scheme"}, // Scheme
    {"asm", "Assembly", QT_TRANSLATE_NOOP("LangType", "Assembly language source file"), "asm", "Assembly"}, // Asm
    {"diff", "Diff", QT_TRANSLATE_NOOP("LangType", "Diff file"), "diff", "Diff"}, // Diff
    {"props", "Properties file", QT_TRANSLATE_NOOP("LangType", "Properties file"), "props", "Properties"}, // Props
    {"postscript", "PostScript", QT_TRANSLATE_NOOP("LangType", "PostScript file"), "ps", "PostScript"}, // PostScript
    {"ruby", "Ruby", QT_TRANSLATE_NOOP("LangType", "Ruby file"), "ruby", "Ruby"}, // Ruby
    {"smalltalk", "Smalltalk", QT_TRANSLATE_NOOP("LangType", "Smalltalk file"), "smalltalk", "Smalltalk"}, // Smalltalk
    {"vhdl", "VHDL", QT_TRANSLATE_NOOP("LangType", "VHSIC Hardware Description Language file"), "vhdl", "VHDL"}, // Vhdl
    {"kix", "KiXtart", QT_TRANSLATE_NOOP("LangType", "KiXtart file"), "kix", "KIXtart"}, // Kix
    {"autoit", "AutoIt", QT_TRANSLATE_NOOP("LangType", "AutoIt"), "au3", "AutoIt"}, // AutoIt
    {"caml", "CAML", QT_TRANSLATE_NOOP("LangType", "Categorical Abstract Machine Language"), "caml", "Caml"}, // Caml
    {"ada", "Ada", QT_TRANSLATE_NOOP("LangType", "Ada file"), "ada", "Ada"}, // Ada
    {"verilog", "Verilog", QT_TRANSLATE_NOOP("LangType", "Verilog file"), "verilog", "Verilog"}, // Verilog
    {"matlab", "MATLAB", QT_TRANSLATE_NOOP("LangType", "MATrix LABoratory"), "matlab", "Matlab"}, // Matlab
    {"haskell", "Haskell", QT_TRANSLATE_NOOP("LangType", "Haskell"), "haskell", "Haskell"}, // Haskell
    {"inno", "Inno Setup", QT_TRANSLATE_NOOP("LangType", "Inno Setup script"), "inno", "Inno Setup"}, // Inno
    {"searchResult", "Internal Search", QT_TRANSLATE_NOOP("LangType", "Internal Search"), "searchResult", nullptr}, // SearchResult
    {"cmake", "CMake", QT_TRANSLATE_NOOP("LangType", "CMake file"), "cmake", "CMake"}, // CMake
    {"yaml", "YAML", QT_TRANSLATE_NOOP("LangType", "YAML Ain't Markup Language"), "yaml", "YAML"}, // Yaml
    {"cobol", "COBOL", QT_TRANSLATE_NOOP("LangType", "COmmon Business Oriented Language"), "COBOL", "COBOL"}, // Cobol
    {"gui4cli", "Gui4Cli", QT_TRANSLATE_NOOP("LangType", "Gui4Cli file"), "gui4cli", "Gui4Cli"}, // Gui4Cli
    {"d", "D", QT_TRANSLATE_NOOP("LangType", "D programming language"), "d", "D"}, // D
    {"powershell", "PowerShell", QT_TRANSLATE_NOOP("LangType", "Windows PowerShell"), "powershell", "PowerShell"}, // PowerShell
    {"r", "R", QT_TRANSLATE_NOOP("LangType", "R programming language"), "r", "R"}, // R
    {"jsp", "JSP", QT_TRANSLATE_NOOP("LangType", "JavaServer Pages script file"), "hypertext", "JSP"}, // Jsp
    {"coffeescript", "CoffeeScript", QT_TRANSLATE_NOOP("LangType", "CoffeeScript file"), "coffeescript", "CoffeeScript"}, // CoffeeScript
    {"json", "json", QT_TRANSLATE_NOOP("LangType", "JSON file"), "json", "JSON"}, // Json
    {"javascript.js", "JavaScript", QT_TRANSLATE_NOOP("LangType", "JavaScript file"), "cpp", "JavaScript"}, // JavaScript
    {"fortran77", "Fortran fixed form", QT_TRANSLATE_NOOP("LangType", "Fortran fixed form source file"), "f77", "Fortran (fixed form)"}, // Fortran77
    {"baanc", "BaanC", QT_TRANSLATE_NOOP("LangType", "BaanC File"), "baan", "BaanC"}, // BaanC
    {"srec", "S-Record", QT_TRANSLATE_NOOP("LangType", "Motorola S-Record binary data"), "srec", "S-Record"}, // Srec
    {"ihex", "Intel HEX", QT_TRANSLATE_NOOP("LangType", "Intel HEX binary data"), "ihex", "Intel HEX"}, // IHex
    {"tehex", "Tektronix extended HEX", QT_TRANSLATE_NOOP("LangType", "Tektronix extended HEX binary data"), "tehex", "Tektronix extended HEX"}, // TeHex
    {"swift", "Swift", QT_TRANSLATE_NOOP("LangType", "Swift file"), "cpp", "Swift"}, // Swift
    {"asn1", "ASN.1", QT_TRANSLATE_NOOP("LangType", "Abstract Syntax Notation One file"), "asn1", "ASN.1"}, // Asn1
    {"avs", "AviSynth", QT_TRANSLATE_NOOP("LangType", "AviSynth scripts files"), "avs", "AviSynth"}, // Avs
    {"blitzbasic", "BlitzBasic", QT_TRANSLATE_NOOP("LangType", "BlitzBasic file"), "blitzbasic", "Blitzbasic"}, // BlitzBasic
    {"purebasic", "PureBasic", QT_TRANSLATE_NOOP("LangType", "PureBasic file"), "purebasic", "Purebasic"}, // PureBasic
    {"freebasic", "FreeBasic", QT_TRANSLATE_NOOP("LangType", "FreeBasic file"), "freebasic", "Freebasic"}, // FreeBasic
    {"csound", "Csound", QT_TRANSLATE_NOOP("LangType", "Csound file"), "csound", "CSound"}, // Csound
    {"erlang", "Erlang", QT_TRANSLATE_NOOP("LangType", "Erlang file"), "erlang", "Erlang"}, // Erlang
    {"escript", "ESCRIPT", QT_TRANSLATE_NOOP("LangType", "ESCRIPT file"), "escript", "ESCRIPT"}, // EScript
    {"forth", "Forth", QT_TRANSLATE_NOOP("LangType", "Forth file"), "forth", "Forth"}, // Forth
    {"latex", "LaTeX", QT_TRANSLATE_NOOP("LangType", "LaTeX file"), "latex", "LaTeX"}, // Latex
    {"mmixal", "MMIXAL", QT_TRANSLATE_NOOP("LangType", "MMIXAL file"), "mmixal", "MMIXAL"}, // Mmixal
    {"nim", "Nim", QT_TRANSLATE_NOOP("LangType", "Nim file"), "nimrod", "Nim"}, // Nim
    {"nncrontab", "Nncrontab", QT_TRANSLATE_NOOP("LangType", "extended crontab file"), "nncrontab", "Nncrontab"}, // Nncrontab
    {"oscript", "OScript", QT_TRANSLATE_NOOP("LangType", "OScript source file"), "oscript", "OScript"}, // OScript
    {"rebol", "REBOL", QT_TRANSLATE_NOOP("LangType", "REBOL file"), "rebol", "REBOL"}, // Rebol
    {"registry", "registry", QT_TRANSLATE_NOOP("LangType", "registry file"), "registry", "Registry"}, // Registry
    {"rust", "Rust", QT_TRANSLATE_NOOP("LangType", "Rust file"), "rust", "Rust"}, // Rust
    {"spice", "Spice", QT_TRANSLATE_NOOP("LangType", "spice file"), "spice", "Spice"}, // Spice
    {"txt2tags", "txt2tags", QT_TRANSLATE_NOOP("LangType", "txt2tags file"), "txt2tags", "txt2tags"}, // Txt2tags
    {"visualprolog", "Visual Prolog", QT_TRANSLATE_NOOP("LangType", "Visual Prolog file"), "visualprolog", "Visual Prolog"}, // VisualProlog
    {"typescript", "TypeScript", QT_TRANSLATE_NOOP("LangType", "TypeScript file"), "cpp", "TypeScript"}, // TypeScript
    {"json5", "json5", QT_TRANSLATE_NOOP("LangType", "JSON5 file"), "json", "JSON5"}, // Json5
    {"mssql", "mssql", QT_TRANSLATE_NOOP("LangType", "Microsoft Transact-SQL (SQL Server) file"), "mssql", "Microsoft Transact-SQL"}, // MsSql
    {"gdscript", "GDScript", QT_TRANSLATE_NOOP("LangType", "GDScript file"), "gdscript", "GDScript"}, // GdScript
    {"hollywood", "Hollywood", QT_TRANSLATE_NOOP("LangType", "Hollywood script"), "hollywood", "Hollywood"}, // Hollywood
    {"go", "Go", QT_TRANSLATE_NOOP("LangType", "Go source file"), "cpp", "Go"}, // Go
    {"raku", "Raku", QT_TRANSLATE_NOOP("LangType", "Raku source file"), "raku", "Raku"}, // Raku
    {"toml", "TOML", QT_TRANSLATE_NOOP("LangType", "Tom's Obvious Minimal Language file"), "toml", "TOML"}, // Toml
    {"sas", "SAS", QT_TRANSLATE_NOOP("LangType", "SAS file"), "sas", "SAS"}, // Sas
    {"errorlist", "ErrorList", QT_TRANSLATE_NOOP("LangType", "ErrorList file"), "errorlist", "ErrorList"}, // ErrorList
    {"escseq", "EscapeSequence (ANSI)", QT_TRANSLATE_NOOP("LangType", "Escape Sequence (ANSI) file"), "escseq", "Escape Sequence (ANSI)"}, // EscSeq
    {"fcST", "Structured Text", QT_TRANSLATE_NOOP("LangType", "IEC 61131-3 Structured Text file"), "fcST", "Structured Text"}, // StructuredText
    {"ext", "External", QT_TRANSLATE_NOOP("LangType", "External"), "null", nullptr}, // External
}};

} // namespace

const LanguageInfo& languageInfo(LangType lang)
{
    const int index = static_cast<int>(lang);
    return kLanguages[static_cast<std::size_t>(index >= 0 && index < kLangTypeCount ? index : 0)];
}

LangType langTypeFromName(std::string_view name)
{
    for (int i = 0; i < kLangTypeCount - 1; ++i) { // External no tiene nombre propio
        if (name == kLanguages[static_cast<std::size_t>(i)].name)
            return static_cast<LangType>(i);
    }
    return LangType::Text;
}

LangType langTypeFromMenuName(std::string_view menuName, bool* found)
{
    for (int i = 0; i < kLangTypeCount; ++i) {
        const char* menu = kLanguages[static_cast<std::size_t>(i)].menuName;
        if (menu && menuName == menu) {
            if (found)
                *found = true;
            return static_cast<LangType>(i);
        }
    }
    if (found)
        *found = false;
    return LangType::Text;
}

bool isKnownLanguageName(std::string_view name)
{
    return name == "normal" || langTypeFromName(name) != LangType::Text;
}

} // namespace edita
