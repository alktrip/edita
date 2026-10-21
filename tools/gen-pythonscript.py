#!/usr/bin/env python3
# This file is part of Edita.
# Copyright (C) 2026 Edita contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Genera plugins/PythonScript/lib/Npp/_generated.py: los métodos de `editor` y los
# enums de Scintilla a partir de Scintilla.iface (como CreateWrapper.py de
# PythonScript), y LANGTYPE, NOTIFICATION y MENUCOMMAND a partir de las cabeceras
# de Notepad++ (Notepad_plus_msgs.h y menuCmdID.h).
#
# Uso: tools/gen-pythonscript.py [ruta de referencia/npp]

import keyword
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
EDITOR = os.path.dirname(HERE)
IFACE = os.path.join(EDITOR, "third_party/scintilla/include/Scintilla.iface")
NPP = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(EDITOR), "referencia/npp")
MSGS = os.path.join(NPP, "PowerEditor/src/MISC/PluginsManager/Notepad_plus_msgs.h")
MENU = os.path.join(NPP, "PowerEditor/src/menuCmdID.h")
OUT = os.path.join(EDITOR, "plugins/PythonScript/lib/Npp/_generated.py")

# Tipos de Scintilla.iface que son números en Python (también los enums)
INT_TYPES = {"int", "position", "line", "pointer", "pixels", "keymod", "colour", "colouralpha", "bool"}

# Excluidas como en CreateWrapper.py
EXCLUSIONS = {"FormatRange", "FormatRangeFull"}


# --- Lectura de Scintilla.iface ------------------------------------------------------------

class Feature:
    def __init__(self, kind, ret, name, value, params, category):
        self.kind = kind
        self.ret = ret
        self.name = name
        self.value = value
        self.params = params  # [(tipo, nombre), (tipo, nombre)]
        self.category = category


def parse_iface(path):
    features, vals, enums, events = [], {}, [], []
    category = ""
    feature_re = re.compile(r"^(fun|get|set|evt)\s+(\S+)\s+(\w+)=(\d+)\((.*)\)\s*$")
    for line in open(path, encoding="utf-8"):
        line = line.rstrip("\n")
        if line.startswith("cat "):
            category = line[4:].strip()
        elif line.startswith("val "):
            name, _, value = line[4:].partition("=")
            try:
                vals[name.strip()] = int(value.strip(), 0)
            except ValueError:
                pass
        elif line.startswith("enu "):
            name, _, prefixes = line[4:].partition("=")
            enums.append((name.strip(), prefixes.split()))
        else:
            m = feature_re.match(line)
            if not m:
                continue
            kind, ret, name, value, params = m.groups()
            parts = [p.strip() for p in params.split(",")] if kind != "evt" else []
            parsed = []
            for p in (parts + ["", ""])[:2]:
                bits = p.split()
                parsed.append((bits[0], bits[1]) if len(bits) >= 2 else ("", ""))
            f = Feature(kind, ret, name, int(value), parsed, category)
            (events if kind == "evt" else features).append(f)
    return features, vals, enums, events


def py_name(name):
    return name[:1].lower() + name[1:]


def arg_name(name):
    return name + "_" if keyword.iskeyword(name) or name in ("self",) else name


# --- Generación de los métodos ---------------------------------------------------------------

def gen_method(f):
    """Código de un método de Editor, o None si no se puede envolver"""
    (t1, n1), (t2, n2) = f.params
    name, msg = py_name(f.name), f.value
    a1, a2 = arg_name(n1), arg_name(n2)
    out = []

    def ret(expr):
        if f.ret == "void":
            return "        " + expr
        if f.ret == "bool":
            return "        return " + expr + " != 0"
        if f.ret == "colour":
            return "        return _colour_tuple(" + expr + ")"
        if f.ret == "colouralpha":
            return "        return _colouralpha_tuple(" + expr + ")"
        return "        return " + expr

    def convert(t, a):
        if t == "string":
            return "_bytes(" + a + ")"
        if t == "colour":
            return "_rgb(" + a + ")"
        if t == "colouralpha":
            return "_rgba(" + a + ")"
        if t == "bool":
            return "(1 if " + a + " else 0)"
        return "int(" + a + ")"

    def doc():
        return '        """See Scintilla documentation for SCI_%s"""' % f.name.upper()

    # Casos especiales (specialCases de CreateWrapper.py)
    special = {
        "GetLine": ("line", "        return self._getLine(line)"),
        "GetCharacterPointer": ("", "        return self.getText()"),
        "GetRangePointer": ("start, lengthRange", "        return self._range(start, start + lengthRange)"),
        "GetStyledText": ("start, end", "        return self._styled(start, end)"),
        "GetStyledTextFull": ("start, end", "        return self._styled(start, end)"),
    }
    # AnnotationSetText(line, None) borra la anotación (annotationSetTextBody)
    if f.name in ("AnnotationSetText", "EOLAnnotationSetText"):
        return ["    def %s(self, line, text):" % name, doc(),
                "        if text is None:",
                "            self._sci(%d, int(line), 0)" % msg,
                "        else:",
                "            self._sci_ls(%d, int(line), _bytes(text))" % msg]
    if f.name in special:
        params, body = special[f.name]
        return ["    def %s(self%s):" % (name, ", " + params if params else ""), doc(), body]

    sig = (t1, n1, t2, n2)
    # (position length, string): solo el texto
    if t1 == "position" and n1 == "length" and t2 == "string":
        return ["    def %s(self, %s):" % (name, a2), doc(), "        _s = _bytes(%s)" % a2,
                ret("self._sci_ls(%d, len(_s), _s)" % msg)]
    # Cadenas de resultado
    if t2 == "stringresult":
        if t1 == "position" and n1 == "length":
            return ["    def %s(self):" % name, doc(), "        _n = self._sci(%d, 0, 0)" % msg,
                    "        return _str(self._sci_res(%d, _n + 1, _n))" % msg]
        if t1 == "string":
            return ["    def %s(self, %s):" % (name, a1), doc(), "        _k = _bytes(%s)" % a1,
                    "        _n = self._sci_ws(%d, _k, 0)" % msg,
                    "        return _str(self._sci_wres(%d, _k, _n))" % msg]
        if t1 == "":
            return ["    def %s(self):" % name, doc(), "        _n = self._sci(%d, 0, 0)" % msg,
                    "        return _str(self._sci_res(%d, 0, _n))" % msg]
        if t1 in INT_TYPES or t1[:1].isupper():
            return ["    def %s(self, %s):" % (name, a1), doc(), "        _n = self._sci(%d, int(%s), 0)" % (msg, a1),
                    "        return _str(self._sci_res(%d, int(%s), _n))" % (msg, a1)]
        return None
    if t2 == "cells":
        return ["    def %s(self, %s):" % (name, a2), doc(), "        _c = bytes(%s)" % a2,
                ret("self._sci_ls(%d, len(_c), _c)" % msg)]
    if t2 in ("findtext", "findtextfull"):
        return ["    def %s(self, %s, start, end, %s):" % (name, a1, a2), doc(),
                "        return self._find(int(%s), start, end, %s)" % (a1, a2)]
    if t2 in ("textrange", "textrangefull"):
        return ["    def %s(self, start, end):" % name, doc(), "        return self._range(start, end)"]
    if "formatrange" in (t1, t2) or "formatrangefull" in (t1, t2):
        return None

    args = [a for (t, a) in ((t1, a1), (t2, a2)) if t]
    head = "    def %s(self%s):" % (name, "".join(", " + a for a in args))
    w = convert(t1, a1) if t1 else "0"
    l = convert(t2, a2) if t2 else "0"
    if t1 == "string" and t2 == "string":
        call = "self._sci_ss(%d, %s, %s)" % (msg, w, l)
    elif t1 == "string":
        call = "self._sci_ws(%d, %s, %s)" % (msg, w, l)
    elif t2 == "string":
        call = "self._sci_ls(%d, %s, %s)" % (msg, w, l)
    else:
        call = "self._sci(%d, %s, %s)" % (msg, w, l)
    return [head, doc(), ret(call)]


# --- Cabeceras de Notepad++ --------------------------------------------------------------

def npp_langtypes(path):
    text = open(path, encoding="utf-8", errors="replace").read()
    body = re.search(r"enum\s+LangType\s*\{(.*?)\}", text, re.S).group(1)
    body = re.sub(r"//[^\n]*", "", body)
    names, value = {}, 0
    for item in body.split(","):
        item = item.strip()
        if not item:
            continue
        name, _, expr = item.partition("=")
        name = name.strip()
        if expr.strip():
            value = int(expr.strip(), 0)
        names[name] = value
        value += 1
    return names


def npp_notifications(path):
    text = open(path, encoding="utf-8", errors="replace").read()
    first = 1000
    out = {}
    for m in re.finditer(r"#define\s+NPPN_(\w+)\s+\(NPPN_FIRST\s*\+\s*(\d+)\)", text):
        out[m.group(1)] = first + int(m.group(2))
    return out


def npp_menu_commands(path):
    defines = {}
    for line in open(path, encoding="utf-8", errors="replace"):
        line = line.split("//")[0]
        m = re.match(r"\s*#define\s+(\w+)\s+(.+)", line)
        if m:
            defines[m.group(1)] = m.group(2).strip()

    cache = {}

    def evaluate(name, depth=0):
        if name in cache:
            return cache[name]
        expr = defines.get(name)
        if expr is None or depth > 20:
            return None
        tokens = re.findall(r"[A-Za-z_]\w*|0x[0-9A-Fa-f]+|\d+|[-+()]", expr)
        py = ""
        for tok in tokens:
            if re.match(r"[A-Za-z_]", tok):
                v = evaluate(tok, depth + 1)
                if v is None:
                    return None
                py += str(v)
            else:
                py += tok
        try:
            cache[name] = int(eval(py, {"__builtins__": {}}))
        except Exception:
            return None
        return cache[name]

    out = {}
    for name in defines:
        if name.startswith("IDM_") and name not in ("IDM",):
            v = evaluate(name)
            if v is not None and v > 0 and name != "IDM":
                out[name[4:]] = v
    return out


# Nombres de LANGTYPE en PythonScript (NotepadPython.cpp) → LangType de N++
LANGTYPE_NAMES = [
    ("TXT", "L_TEXT"), ("PHP", "L_PHP"), ("C", "L_C"), ("CPP", "L_CPP"), ("CS", "L_CS"), ("OBJC", "L_OBJC"),
    ("JAVA", "L_JAVA"), ("RC", "L_RC"), ("HTML", "L_HTML"), ("XML", "L_XML"), ("MAKEFILE", "L_MAKEFILE"),
    ("PASCAL", "L_PASCAL"), ("BATCH", "L_BATCH"), ("INI", "L_INI"), ("USER", "L_USER"), ("ASP", "L_ASP"),
    ("SQL", "L_SQL"), ("VB", "L_VB"), ("JS", "L_JS_EMBEDDED"), ("CSS", "L_CSS"), ("PERL", "L_PERL"),
    ("PYTHON", "L_PYTHON"), ("LUA", "L_LUA"), ("TEX", "L_TEX"), ("FORTRAN", "L_FORTRAN"), ("BASH", "L_BASH"),
    ("FLASH", "L_FLASH"), ("NSIS", "L_NSIS"), ("TCL", "L_TCL"), ("LISP", "L_LISP"), ("SCHEME", "L_SCHEME"),
    ("ASM", "L_ASM"), ("DIFF", "L_DIFF"), ("PROPS", "L_PROPS"), ("PS", "L_PS"), ("RUBY", "L_RUBY"),
    ("SMALLTALK", "L_SMALLTALK"), ("VHDL", "L_VHDL"), ("KIX", "L_KIX"), ("AU3", "L_AU3"), ("CAML", "L_CAML"),
    ("ADA", "L_ADA"), ("VERILOG", "L_VERILOG"), ("MATLAB", "L_MATLAB"), ("HASKELL", "L_HASKELL"),
    ("INNO", "L_INNO"), ("SEARCHRESULT", "L_SEARCHRESULT"), ("CMAKE", "L_CMAKE"), ("YAML", "L_YAML"),
    ("COBOL", "L_COBOL"), ("GUI4CLI", "L_GUI4CLI"), ("D", "L_D"), ("POWERSHELL", "L_POWERSHELL"), ("R", "L_R"),
    ("JSP", "L_JSP"), ("COFFEESCRIPT", "L_COFFEESCRIPT"), ("JSON", "L_JSON"), ("JAVASCRIPT", "L_JAVASCRIPT"),
    ("FORTRAN_77", "L_FORTRAN_77"), ("BAANC", "L_BAANC"), ("SREC", "L_SREC"), ("IHEX", "L_IHEX"),
    ("TEHEX", "L_TEHEX"), ("SWIFT", "L_SWIFT"), ("ASN1", "L_ASN1"), ("AVS", "L_AVS"),
    ("BLITZBASIC", "L_BLITZBASIC"), ("PUREBASIC", "L_PUREBASIC"), ("FREEBASIC", "L_FREEBASIC"),
    ("CSOUND", "L_CSOUND"), ("ERLANG", "L_ERLANG"), ("ESCRIPT", "L_ESCRIPT"), ("FORTH", "L_FORTH"),
    ("LATEX", "L_LATEX"), ("MMIXAL", "L_MMIXAL"), ("NIM", "L_NIM"), ("NNCRONTAB", "L_NNCRONTAB"),
    ("OSCRIPT", "L_OSCRIPT"), ("REBOL", "L_REBOL"), ("REGISTRY", "L_REGISTRY"), ("RUST", "L_RUST"),
    ("SPICE", "L_SPICE"), ("TXT2TAGS", "L_TXT2TAGS"), ("VISUALPROLOG", "L_VISUALPROLOG"),
    ("TYPESCRIPT", "L_TYPESCRIPT"), ("JSON5", "L_JSON5"), ("MSSQL", "L_MSSQL"), ("GDSCRIPT", "L_GDSCRIPT"),
    ("HOLLYWOOD", "L_HOLLYWOOD"), ("GOLANG", "L_GOLANG"), ("RAKU", "L_RAKU"), ("TOML", "L_TOML"),
    ("SAS", "L_SAS"), ("ERRORLIST", "L_ERRORLIST"), ("EXTERNAL", "L_EXTERNAL"),
]


def write_enum(out, name, items):
    out.append("class %s(_Enum):" % name)
    written = 0
    for key, value in items:
        if key.isidentifier() and not keyword.iskeyword(key):
            out.append("    %s = %d" % (key, value))
            written += 1
    if not written:
        out.append("    pass")
    out.append("")
    out.append("")


def main():
    features, vals, enums, events = parse_iface(IFACE)
    out = [
        "# Generado por tools/gen-pythonscript.py a partir de Scintilla.iface y de las",
        "# cabeceras de Notepad++. No editar a mano.",
        "# SPDX-License-Identifier: GPL-3.0-or-later",
        "",
        "",
        "class _Enum:",
        '    """Constantes con nombre, como los enums de PythonScript (valores enteros)"""',
        "",
        "",
        "def _bytes(value):",
        "    if isinstance(value, bytes):",
        "        return value",
        "    return str(value).encode('utf-8')",
        "",
        "",
        "def _str(value):",
        "    return value.decode('utf-8', errors='replace')",
        "",
        "",
        "def _rgb(c):",
        "    if isinstance(c, int):",
        "        return c",
        "    return (int(c[0]) & 0xFF) | ((int(c[1]) & 0xFF) << 8) | ((int(c[2]) & 0xFF) << 16)",
        "",
        "",
        "def _rgba(c):",
        "    if isinstance(c, int):",
        "        return c",
        "    a = int(c[3]) if len(c) > 3 else 0xFF",
        "    return _rgb(c) | ((a & 0xFF) << 24)",
        "",
        "",
        "def _colour_tuple(v):",
        "    return (v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF)",
        "",
        "",
        "def _colouralpha_tuple(v):",
        "    return (v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, (v >> 24) & 0xFF)",
        "",
        "",
        "class ScintillaGenerated:",
        '    """Métodos de Scintilla con nombres de Python (editor.getText(), editor.addText(text)…)"""',
        "",
    ]
    count = 0
    seen = set()
    for f in features:
        if f.category == "Deprecated" or f.name in EXCLUSIONS or f.name in seen:
            continue
        code = gen_method(f)
        if code:
            out.extend(code)
            out.append("")
            seen.add(f.name)
            count += 1
    out.append("")

    # Enums de Scintilla: nombre en mayúsculas sin el prefijo (writeEnumsWrapperFile)
    for name, prefixes in enums:
        items = []
        for val, value in vals.items():
            for prefix in prefixes:
                if val.startswith(prefix):
                    key = val[len(prefix):]
                    if val in ("SC_STARTACTION", "SC_MULTISTEPUNDOREDO", "SC_LASTSTEPINUNDOREDO",
                               "SC_MULTILINEUNDOREDO", "SC_MODEVENTMASKALL"):
                        key = val[len("SC_"):]
                    items.append((key.upper(), value))
                    break
        write_enum(out, name.upper(), items)
    write_enum(out, "SCINTILLANOTIFICATION",
               [(e.name.upper(), e.value) for e in events if e.category != "Deprecated"])
    write_enum(out, "SCINTILLAMESSAGE", [("SCI_" + f.name.upper(), f.value) for f in features
                                          if f.category != "Deprecated"])

    langs = npp_langtypes(MSGS)
    write_enum(out, "LANGTYPE", [(n, langs[l]) for n, l in LANGTYPE_NAMES if l in langs])
    write_enum(out, "NOTIFICATION", sorted(npp_notifications(MSGS).items(), key=lambda kv: kv[1]))
    write_enum(out, "MENUCOMMAND", sorted(npp_menu_commands(MENU).items(), key=lambda kv: kv[1]))

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as fh:
        fh.write("\n".join(out).rstrip() + "\n")
    print("%s: %d métodos, %d enums de Scintilla" % (os.path.relpath(OUT, EDITOR), count, len(enums)))


if __name__ == "__main__":
    main()
