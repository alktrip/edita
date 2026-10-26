#!/usr/bin/env bash
# Edita para macOS: compila, arma Edita.app (datos, plugins, icono y Qt) y crea el .dmg.
#
# Uso: packaging/macos/build-dmg.sh [carpeta de build]   (por defecto build-macos)
# Necesita Qt 6 (qmake/macdeployqt en el PATH o QT_ROOT_DIR), CMake y Ninja. Variables:
#   EDITA_ARCHS     arquitecturas (por defecto «arm64;x86_64»: universal, si Qt lo es)
#   EDITA_TESTS=1   compila y pasa las pruebas antes de empaquetar
#   EDITA_SIGN      identidad de firma (por defecto «-»: firma ad hoc, sin notarizar)
#   EDITA_DIST      dónde dejar el .dmg (por defecto ../dist, junto al AppImage)
set -euo pipefail

SRC=$(cd "$(dirname "$0")/../.." && pwd)
BUILD=${1:-$SRC/build-macos}
ARCHS=${EDITA_ARCHS:-arm64;x86_64}
SIGN=${EDITA_SIGN:--}
VERSION=$(sed -n 's/^ *VERSION \([0-9.]*\)$/\1/p' "$SRC/CMakeLists.txt" | head -1)
ARCH_NAME=$([ "$ARCHS" = "arm64;x86_64" ] && echo universal || echo "${ARCHS//;/-}")
DIST=${EDITA_DIST:-$SRC/../dist}
log() { printf '\n==> %s\n' "$*"; }

TESTS=OFF
[ "${EDITA_TESTS:-0}" = 1 ] && TESTS=ON
log "Compilando ($ARCHS) en $BUILD"
cmake -S "$SRC" -B "$BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="$ARCHS" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
    -DEDITA_BUILD_TESTS=$TESTS \
    -DEDITA_PYTHONSCRIPT=OFF
cmake --build "$BUILD" -- -k 0 # todos los errores de una vez
if [ "$TESTS" = ON ]; then
    log "Pruebas"
    (cd "$BUILD" && QT_QPA_PLATFORM=offscreen ctest --output-on-failure -LE perf) || {
        echo "::warning::Algunas pruebas fallaron en macOS (se empaqueta igual)"
    }
fi

APP=$BUILD/Edita.app
log "Armando $APP"
CONTENTS=$APP/Contents
mkdir -p "$CONTENTS/Resources" "$CONTENTS/PlugIns/edita/plugins"
rm -rf "$CONTENTS/Resources/edita"
cp -R "$SRC/data" "$CONTENTS/Resources/edita"
mkdir -p "$CONTENTS/Resources/edita/plugin-sdk"
cp "$SRC/plugin-sdk/edita_plugin.h" "$SRC/third_party/scintilla/include/Scintilla.h" \
   "$SRC/third_party/scintilla/include/Sci_Position.h" "$CONTENTS/Resources/edita/plugin-sdk/"
cp -R "$SRC/plugin-sdk/compat" "$SRC/plugin-sdk/cmake" "$CONTENTS/Resources/edita/plugin-sdk/"
EXTRA=()
for dir in "$BUILD"/plugins/*/; do
    name=$(basename "$dir")
    [ -f "$dir/$name.so" ] || continue
    rm -rf "$CONTENTS/PlugIns/edita/plugins/$name"
    cp -R "$dir" "$CONTENTS/PlugIns/edita/plugins/$name"
    EXTRA+=("-executable=$CONTENTS/PlugIns/edita/plugins/$name/$name.so")
done

# El icono: un .icns a partir del PNG de 512 (y los tamaños menores)
ICONSET=$BUILD/edita.iconset
rm -rf "$ICONSET" && mkdir -p "$ICONSET"
for size in 16 32 128 256 512; do
    sips -z $size $size "$SRC/resources/icons/hicolor/512x512/apps/edita.png" --out "$ICONSET/icon_${size}x${size}.png" >/dev/null
    double=$((size * 2))
    sips -z $double $double "$SRC/resources/icons/hicolor/512x512/apps/edita.png" --out "$ICONSET/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "$ICONSET" -o "$CONTENTS/Resources/edita.icns"

log "Qt dentro del paquete (macdeployqt)"
MACDEPLOYQT=$(command -v macdeployqt || echo "${QT_ROOT_DIR:-}/bin/macdeployqt")
"$MACDEPLOYQT" "$APP" "${EXTRA[@]}" -verbose=1

log "Firma ($SIGN)"
# Con una identidad real, «hardened runtime» (para notarizar); ad hoc no: impediría
# cargar los plugins y Qt, que no llevan la misma firma
if [ "$SIGN" = "-" ]; then
    codesign --force --deep --sign - "$APP"
else
    codesign --force --deep --options runtime --timestamp --sign "$SIGN" "$APP"
fi
codesign --verify --deep --strict "$APP"

log "Imagen de disco"
mkdir -p "$DIST"
DMG=$DIST/Edita-$VERSION-macos-$ARCH_NAME.dmg
STAGE=$BUILD/dmg
rm -rf "$STAGE" "$DMG" && mkdir -p "$STAGE"
cp -R "$APP" "$STAGE/"
ln -s /Applications "$STAGE/Applications"
hdiutil create -volname "Edita $VERSION" -srcfolder "$STAGE" -ov -format UDZO "$DMG"
log "Listo: $DMG"
