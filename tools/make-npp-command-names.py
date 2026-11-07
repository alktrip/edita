#!/usr/bin/env python3
# Genera src/core/NppCommandNames.inc: el nombre en inglés de cada orden del menú principal
# de Notepad++ (la sección <Commands> de su english.xml) con su id (IDM_*). contextMenu.xml
# nombra las órdenes así (MenuEntryName/MenuItemName, siempre en inglés).
#
# Uso: tools/make-npp-command-names.py ../referencia/npp/PowerEditor/installer/nativeLang/english.xml
import html
import re
import sys

text = open(sys.argv[1], encoding="utf-8").read()
start = text.index("<Commands>")
commands = text[start:text.index("</Commands>", start)]
seen = set()
out = []
for ident, name in re.findall(r'<Item id="(\d+)" name="([^"]*)"', commands):
    name = html.unescape(name).replace("&", "").replace("...", "").replace("…", "").strip()
    key = (int(ident), name)
    if key in seen or not name:
        continue
    seen.add(key)
    out.append('    {"%s", %s},' % (name.replace("\\", "\\\\").replace('"', '\\"'), ident))
header = ("// Generado por tools/make-npp-command-names.py desde english.xml de Notepad++\n"
          "// (Copyright (C) Don HO and Notepad++ team, GPL). No editar a mano.\n")
open("src/core/NppCommandNames.inc", "w", encoding="utf-8").write(header + "\n".join(out) + "\n")
print(len(out))
