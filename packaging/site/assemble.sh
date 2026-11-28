#!/bin/bash
# Arma la página del proyecto en la carpeta $1: site/ y las capturas (inglés en images/,
# español en es/images/) y el icono, que viven en docs/images y resources (fase 107)
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${1:?carpeta de salida}
rm -rf "$OUT"
mkdir -p "$OUT/images" "$OUT/es/images"
cp -r "$ROOT/site/." "$OUT/"
cp "$ROOT"/docs/images/en/*.png "$OUT/images/"
cp "$ROOT"/docs/images/*.png "$OUT/es/images/"
cp "$ROOT/resources/icons/hicolor/256x256/apps/edita.png" "$OUT/images/edita.png"
touch "$OUT/.nojekyll"
echo "Página en $OUT"
