#!/usr/bin/env bash
# This file is part of Edita.
# Copyright (C) 2026 Edita contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Genera Edita-<versión>-x86_64.AppImage:
#   1. compila Edita en Release y lo instala en un AppDir (prefijo /usr);
#   2. copia la biblioteca estándar de Python, para que Python Script funcione en
#      sistemas con otra versión de Python (o sin Python);
#   3. linuxdeploy y su plugin de Qt copian Qt (con sus plugins de X11 y Wayland y sus
#      traducciones) y las bibliotecas de las que dependen Edita y sus plugins;
#   4. empaqueta el AppDir en el AppImage.
#
# Uso: editor/packaging/appimage/build-appimage.sh
# Los plugins van en usr/lib/edita/plugins también en Debian y Ubuntu (donde
# GNUInstallDirs usaría lib/x86_64-linux-gnu).
# Variables: WORK (carpeta de trabajo, por defecto build-appimage junto a editor/),
#            OUT (dónde se deja el AppImage, por defecto dist/ junto a editor/),
#            QMAKE_BIN (el qmake del Qt que se empaqueta; por defecto qmake6),
#            CMAKE_EXTRA (más opciones para cmake, p. ej. -DCMAKE_PREFIX_PATH=…).
#
# El AppImage usa la glibc del sistema donde se ejecuta: funciona en sistemas con una
# glibc igual o más nueva que la del equipo donde se generó. Para que sirva en más
# distribuciones, hay que generarlo en una antigua (p. ej. en un contenedor).

set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SRC=$(cd "$HERE/../.." && pwd)
WORK=${WORK:-$(dirname "$SRC")/build-appimage}
OUT=${OUT:-$(dirname "$SRC")/dist}
TOOLS=$WORK/tools
APPDIR=$WORK/AppDir
ARCH=x86_64

log() { printf '\n==> %s\n' "$*"; }

# --- Herramientas ------------------------------------------------------------------------
# Releases "continuous" de linuxdeploy (no publica versiones fijas); se descargan una vez
LINUXDEPLOY_URL=https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$ARCH.AppImage
LINUXDEPLOY_QT_URL=https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-$ARCH.AppImage

fetch() {
    local url=$1 file=$TOOLS/$(basename "$1")
    if [[ ! -x $file ]]; then
        log "Descargando $(basename "$file")"
        curl -fL --retry 3 -o "$file.part" "$url"
        chmod +x "$file.part"
        mv "$file.part" "$file"
    fi
}

QMAKE_BIN=${QMAKE_BIN:-qmake6}
for tool in cmake ninja curl "$QMAKE_BIN" python3; do
    command -v "$tool" >/dev/null || { echo "Falta $tool" >&2; exit 1; }
done
mkdir -p "$TOOLS" "$OUT"
fetch "$LINUXDEPLOY_URL"
fetch "$LINUXDEPLOY_QT_URL"
# Sin FUSE: las herramientas (también AppImage) se ejecutan extrayéndose
export APPIMAGE_EXTRACT_AND_RUN=1
# El strip de linuxdeploy no entiende las bibliotecas de distribuciones recientes
# (sección .relr.dyn); las del sistema ya vienen sin símbolos de depuración
export NO_STRIP=1

# --- 1. Compilar e instalar en el AppDir ----------------------------------------------------
log "Compilando Edita (Release)"
cmake -S "$SRC" -B "$WORK/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DEDITA_BUILD_TESTS=OFF \
    ${CMAKE_EXTRA:-}
cmake --build "$WORK/build"
VERSION=$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$WORK/build/CMakeCache.txt")

log "Instalando en $APPDIR"
rm -rf "$APPDIR"
DESTDIR="$APPDIR" cmake --install "$WORK/build"
# Las cabeceras del SDK para C/C++ no hacen falta dentro del AppImage (el SDK de las
# recetas, en share/edita/plugin-sdk, sí)
rm -rf "$APPDIR/usr/include"

# Las cabeceras de Qt (Core, Gui y Widgets, sin las privadas) para las recetas de plugins:
# se compilan con el Qt del AppImage y no con el del sistema, que si es más nuevo da
# plugins que este Qt no puede cargar
QT_HEADERS=$("$QMAKE_BIN" -query QT_INSTALL_HEADERS)
QT_SDK=$APPDIR/usr/share/edita/plugin-sdk/qt/include
mkdir -p "$QT_SDK"
for module in QtCore QtGui QtWidgets; do
    tar -C "$QT_HEADERS" --exclude="$module/[0-9]*" -cf - "$module" | tar -C "$QT_SDK" -xf -
done

PLUGINS=$APPDIR/usr/lib/edita/plugins
DEPS_ONLY=()
for plugin in "$PLUGINS"/*/; do
    DEPS_ONLY+=(--deploy-deps-only "$plugin")
done

# --- 2. Python para Python Script -----------------------------------------------------------
if [[ -f $PLUGINS/PythonScript/PythonScript.so ]]; then
    PYLIB=$(ldd "$PLUGINS/PythonScript/PythonScript.so" | sed -n 's/.*\(libpython3\.[0-9]*\)\.so.*/\1/p' | head -1)
    PYVER=${PYLIB#libpython}
    STDLIB=$(python3 -c "import sysconfig; print(sysconfig.get_paths()['stdlib'])")
    if [[ $STDLIB != */python$PYVER ]]; then
        echo "python3 ($STDLIB) no es la versión con la que se compiló Python Script ($PYVER)" >&2
        exit 1
    fi
    log "Copiando la biblioteca estándar de Python $PYVER"
    mkdir -p "$APPDIR/usr/lib/python$PYVER"
    # Sin pruebas, IDLE, Tk, ejemplos ni paquetes de terceros (site-packages)
    tar -C "$STDLIB" \
        --exclude=__pycache__ --exclude='./test' --exclude='./idlelib' --exclude='./tkinter' \
        --exclude='./turtledemo' --exclude='./ensurepip' --exclude='./site-packages' \
        --exclude='./lib-dynload/_tkinter*' --exclude='./config-*' \
        -cf - . | tar -C "$APPDIR/usr/lib/python$PYVER" -xf -
    DEPS_ONLY+=(--deploy-deps-only "$APPDIR/usr/lib/python$PYVER/lib-dynload")
fi

# --- 3 y 4. Dependencias, Qt y AppImage ---------------------------------------------------
log "Copiando Qt y las dependencias (linuxdeploy)"
# linuxdeploy-plugin-qt copia todos los plugins de la carpeta de Qt, también los que
# instalan otros (KDE: kimg_*, el tema de Plasma, estilos), que arrastran sus propias
# bibliotecas. Ve solo los de Qt: un qmake que apunta QT_INSTALL_PLUGINS a una copia
# (con enlaces) sin esos
QT_PLUGINS=$("$QMAKE_BIN" -query QT_INSTALL_PLUGINS)
FARM=$WORK/qt-plugins
rm -rf "$FARM"
for dir in "$QT_PLUGINS"/*/; do
    name=$(basename "$dir")
    for file in "$dir"*.so; do
        base=$(basename "$file")
        case $name/$base in
            imageformats/libq* | platformthemes/libq* | styles/libq*) ;;
            imageformats/* | platformthemes/* | styles/*) continue ;;
            platforminputcontexts/libqtvirtualkeyboardplugin.so) continue ;;
        esac
        mkdir -p "$FARM/$name"
        ln -s "$file" "$FARM/$name/$base"
    done
done
cat > "$TOOLS/qmake" <<EOF
#!/bin/sh
"$QMAKE_BIN" "\$@" | sed -e 's|^QT_INSTALL_PLUGINS\(/[a-z]*\)\{0,1\}:.*|QT_INSTALL_PLUGINS\1:$FARM|' -e 's|^$QT_PLUGINS\$|$FARM|'
EOF
chmod +x "$TOOLS/qmake"
export QMAKE=$TOOLS/qmake
# Wayland además de X11 (xcb); y los textos de Qt traducidos (qtbase_es…)
# (libqwayland.so desde Qt 6.10; antes, libqwayland-generic.so y libqwayland-egl.so)
PLATFORMS="libqoffscreen.so" # offscreen: sin pantalla (pruebas)
for file in "$FARM"/platforms/libqwayland*.so; do
    [[ -e $file ]] && PLATFORMS+=";$(basename "$file")"
done
export EXTRA_PLATFORM_PLUGINS="$PLATFORMS"
export EXTRA_QT_MODULES="waylandclient"
export EXTRA_QT_PLUGINS="$EXTRA_QT_MODULES" # nombre en versiones anteriores del plugin
export LDAI_OUTPUT="$OUT/Edita-$VERSION-$ARCH.AppImage"
export LINUXDEPLOY_OUTPUT_VERSION=$VERSION
rm -f "$LDAI_OUTPUT"
(
    cd "$WORK"
    "$TOOLS/linuxdeploy-$ARCH.AppImage" \
        --appdir "$APPDIR" \
        --executable "$APPDIR/usr/bin/edita" \
        --desktop-file "$APPDIR/usr/share/applications/edita.desktop" \
        --icon-file "$APPDIR/usr/share/icons/hicolor/256x256/apps/edita.png" \
        --custom-apprun "$HERE/AppRun" \
        "${DEPS_ONLY[@]}" \
        --plugin qt \
        --output appimage
)

log "Listo: $LDAI_OUTPUT ($(du -h "$LDAI_OUTPUT" | cut -f1))"
