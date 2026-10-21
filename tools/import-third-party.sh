#!/usr/bin/env bash
# Copia Scintilla, Lexilla, boostregex, uchardet y pugixml desde el clon de Notepad++ a third_party/
# y aplica los parches locales de third_party/patches/. Idempotente.
#
# Uso: tools/import-third-party.sh [ruta-al-clon-de-npp]   (por defecto ../referencia/npp)
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
npp="${1:-$here/../referencia/npp}"
dst="$here/third_party"

[[ -d "$npp/scintilla" && -d "$npp/lexilla" && -d "$npp/boostregex" ]] \
    || { echo "No encuentro Scintilla/Lexilla/boostregex en: $npp" >&2; exit 1; }

rm -rf "$dst/scintilla" "$dst/lexilla" "$dst/boostregex" "$dst/uchardet" "$dst/pugixml"
mkdir -p "$dst/scintilla/qt" "$dst/lexilla/src"

cp -r "$npp/scintilla/include" "$npp/scintilla/src" "$dst/scintilla/"
cp -r "$npp/scintilla/qt/ScintillaEditBase" "$dst/scintilla/qt/"
cp "$npp/scintilla/License.txt" "$npp/scintilla/version.txt" "$dst/scintilla/"

cp -r "$npp/lexilla/include" "$npp/lexilla/lexlib" "$npp/lexilla/lexers" "$dst/lexilla/"
cp "$npp/lexilla/src/Lexilla.cxx" "$dst/lexilla/src/"
cp "$npp/lexilla/License.txt" "$npp/lexilla/version.txt" "$dst/lexilla/"

cp -r "$npp/boostregex" "$dst/"
rm -f "$dst/boostregex/"*.mak

# uchardet: la versión (antigua, de Mozilla) que usa N++, para detectar igual que él
cp -r "$npp/PowerEditor/src/uchardet" "$dst/"
cp -r "$npp/PowerEditor/src/pugixml" "$dst/"

# Datos de N++ que se usan tal cual: definiciones de lenguajes, estilos y temas
data="$here/data"
rm -rf "$data"
mkdir -p "$data/themes"
cp "$npp/PowerEditor/src/langs.model.xml" "$npp/PowerEditor/src/stylers.model.xml" "$data/"
cp "$npp/PowerEditor/installer/themes/"*.xml "$data/themes/"
mkdir -p "$data/autoCompletion"
cp "$npp/PowerEditor/installer/APIs/"*.xml "$data/autoCompletion/"
mkdir -p "$data/userDefineLangs"
cp "$npp/PowerEditor/bin/userDefineLangs/"*.xml "$data/userDefineLangs/"

# Registrar de dónde viene cada copia
rev="$(git -C "$npp" rev-parse HEAD 2>/dev/null || echo desconocido)"
printf 'notepad-plus-plus %s\nscintilla %s\nlexilla %s\n' "$rev" \
    "$(cat "$npp/scintilla/version.txt")" "$(cat "$npp/lexilla/version.txt")" > "$dst/UPSTREAM.txt"

shopt -s nullglob
for p in "$dst/patches/"*.patch; do
    echo "Aplicando $(basename "$p")"
    patch -d "$dst" -p1 --binary --forward --silent < "$p"
done
echo "third_party importado desde $npp ($rev)"
