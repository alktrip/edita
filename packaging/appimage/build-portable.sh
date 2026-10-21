#!/usr/bin/env bash
# This file is part of Edita.
# Copyright (C) 2026 Edita contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Genera el AppImage en Ubuntu 22.04 (glibc 2.35) para que funcione en la mayoría
# de las distribuciones (Ubuntu 22.04+, Debian 12+, Fedora 36+…), sin Docker ni
# privilegios: el sistema base oficial de Ubuntu en una carpeta, al que se entra
# con un espacio de usuario (unshare + chroot), con Qt 6.8 oficial (aqtinstall,
# compilado contra una glibc antigua). Dentro se ejecuta build-appimage.sh.
#
# Uso: editor/packaging/appimage/build-portable.sh
# Variables: WORK (por defecto build-portable junto a editor/), OUT (dist/).
# Necesita: unshare con --map-auto (rangos en /etc/subuid y /etc/subgid), curl,
# sha256sum y conexión a internet la primera vez.

set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SRC=$(cd "$HERE/../.." && pwd)
WORK=${WORK:-$(dirname "$SRC")/build-portable}
OUT=${OUT:-$(dirname "$SRC")/dist}
ROOT=$WORK/rootfs

UBUNTU_RELEASE=22.04
UBUNTU_BASE=ubuntu-base-22.04.5-base-amd64.tar.gz
UBUNTU_URL=https://cdimage.ubuntu.com/ubuntu-base/releases/$UBUNTU_RELEASE/release
QT_VERSION=6.8.3
CMAKE_VERSION=3.31.6
AQT_VERSION=3.2.1

log() { printf '\n==> %s\n' "$*"; }

for tool in unshare curl sha256sum; do
    command -v "$tool" >/dev/null || { echo "Falta $tool" >&2; exit 1; }
done
mkdir -p "$WORK/download" "$WORK/work" "$OUT"

# Ejecuta una orden dentro del sistema base, como root del espacio de usuario
inside() {
    unshare --user --map-auto --map-root-user --mount --pid --fork bash -s "$ROOT" "$SRC" "$WORK/work" "$OUT" "$1" <<'EOS'
set -euo pipefail
ROOT=$1 SRC=$2 WORK=$3 OUT=$4 CMD=$5
mkdir -p "$ROOT/src" "$ROOT/work" "$ROOT/out"
mount --rbind /dev "$ROOT/dev"
mount -t proc proc "$ROOT/proc"
mount --bind "$SRC" "$ROOT/src"
mount --bind "$WORK" "$ROOT/work"
mount --bind "$OUT" "$ROOT/out"
cp /etc/resolv.conf "$ROOT/etc/resolv.conf"
exec chroot "$ROOT" /usr/bin/env -i HOME=/root TERM=dumb LANG=C.UTF-8 \
    PATH=/root/.local/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin \
    DEBIAN_FRONTEND=noninteractive bash -c "$CMD"
EOS
}

# --- 1. El sistema base (una vez) ---------------------------------------------------------
if [[ ! -f $ROOT/.edita-base ]]; then
    log "Descargando $UBUNTU_BASE"
    curl -fL --retry 3 -o "$WORK/download/$UBUNTU_BASE" "$UBUNTU_URL/$UBUNTU_BASE"
    curl -fL --retry 3 -o "$WORK/download/SHA256SUMS" "$UBUNTU_URL/SHA256SUMS"
    (cd "$WORK/download" && grep " \*\?$UBUNTU_BASE\$" SHA256SUMS | sha256sum -c -)
    rm -rf "$ROOT"
    mkdir -p "$ROOT"
    unshare --user --map-auto --map-root-user tar -C "$ROOT" -xzf "$WORK/download/$UBUNTU_BASE"
    touch "$ROOT/.edita-base"
fi

# --- 2. Compilador, bibliotecas, CMake y Qt (una vez) -----------------------------------------
if [[ ! -f $ROOT/.edita-ready ]]; then
    log "Preparando Ubuntu $UBUNTU_RELEASE (compilador, bibliotecas, CMake $CMAKE_VERSION, Qt $QT_VERSION)"
    inside "
        set -e
        apt-get update
        apt-get install -y --no-install-recommends ca-certificates curl file git g++-12 make ninja-build pkg-config \
            python3-dev python3-pip python3-venv libzip-dev zlib1g-dev \
            libgl-dev libegl-dev libglx-dev libopengl-dev libxkbcommon-dev libxkbcommon-x11-0 \
            libfontconfig1-dev libfreetype-dev libdbus-1-3 libglib2.0-0 \
            libxcb1 libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 libxcb-render-util0 \
            libxcb-shape0 libxcb-shm0 libxcb-sync1 libxcb-xfixes0 libxcb-xinerama0 libxcb-xkb1 libx11-xcb1 \
            libwayland-client0 libwayland-cursor0 libwayland-egl1 libcups2
        python3 -m pip install --user --no-warn-script-location cmake==$CMAKE_VERSION aqtinstall==$AQT_VERSION
        aqt install-qt linux desktop $QT_VERSION linux_gcc_64 -m qt5compat --outputdir /opt/Qt
    "
    touch "$ROOT/.edita-ready"
fi

# --- 3. El AppImage ---------------------------------------------------------------------
log "Compilando y empaquetando en Ubuntu $UBUNTU_RELEASE"
QT=/opt/Qt/$QT_VERSION/gcc_64
inside "
    set -e
    export CC=gcc-12 CXX=g++-12
    export QMAKE_BIN=$QT/bin/qmake
    export CMAKE_EXTRA='-DCMAKE_PREFIX_PATH=$QT'
    export PATH=$QT/bin:\$PATH
    export LD_LIBRARY_PATH=$QT/lib  # para que linuxdeploy encuentre las bibliotecas de Qt
    export WORK=/work OUT=/out
    /src/packaging/appimage/build-appimage.sh
"

APPIMAGE=$(ls -t "$OUT"/Edita-*-x86_64.AppImage | head -1)
log "Listo: $APPIMAGE"
