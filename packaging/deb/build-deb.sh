#!/usr/bin/env bash
# This file is part of Edita.
# Copyright (C) 2026 Edita contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Genera edita_<versión>_amd64.deb con el AppDir del AppImage (lo llama build-appimage.sh
# al terminar). Edita necesita Qt 6.7 o más nuevo y Debian 12 y Ubuntu 22.04/24.04 traen
# uno más viejo, así que el paquete lleva su Qt, como el AppImage:
#   /opt/edita/usr/…                    el AppDir (Edita, Qt, plugins, Python, datos)
#   /usr/bin/edita                      enlace a /opt/edita/usr/bin/edita
#   /usr/share/applications/edita.desktop y /usr/share/icons/hicolor/…: el menú, los tipos
#                                       de archivo y el ícono del escritorio
# Las dependencias (glibc, OpenGL, X11, fontconfig… que el AppImage tampoco lleva) las
# calcula dpkg-shlibdeps, si está (paquete dpkg-dev).
#
# Uso: build-deb.sh <AppDir> <versión> <carpeta de salida>

set -euo pipefail

APPDIR=$(cd "$1" && pwd)
VERSION=$2
OUT=$3
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SRC=$(cd "$HERE/../.." && pwd)
# 0.9.0-beta.8 → 0.9.0~beta.8: para dpkg, con ~ va antes que 0.9.0
DEB_VERSION=${VERSION//-/\~}
ARCH=amd64

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
PKG=$WORK/pkg

mkdir -p "$PKG/opt/edita" "$PKG/usr/bin" "$PKG/usr/share/doc/edita" "$PKG/DEBIAN"
cp -a "$APPDIR/usr" "$PKG/opt/edita/"
ln -s /opt/edita/usr/bin/edita "$PKG/usr/bin/edita"
# El menú y el ícono, donde los busca el escritorio (Edita no los necesita dentro)
mv "$PKG/opt/edita/usr/share/applications" "$PKG/usr/share/applications"
mv "$PKG/opt/edita/usr/share/icons" "$PKG/usr/share/icons"
# Las cabeceras de Qt del SDK de plugins y las bibliotecas no llevan permisos de escritura
# para otros; los directorios, 755
find "$PKG" -type d -exec chmod 755 {} +
find "$PKG/opt" -type f -perm /u+x -exec chmod 755 {} +
find "$PKG/opt" -type f ! -perm /u+x -exec chmod 644 {} +

cat > "$PKG/usr/share/doc/edita/copyright" <<EOF
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: Edita
Source: https://github.com/alktrip/edita

Files: *
Copyright: 2026 Edita contributors; Notepad++: Don HO and the Notepad++ team
License: GPL-3.0-or-later
 On Debian systems, the complete text of the GNU General Public License version 3
 can be found in /usr/share/common-licenses/GPL-3.
 .
 Edita includes Qt, Scintilla, Lexilla and other libraries; their licenses are in
 /opt/edita/usr/share/doc and in the NOTICE file below.
EOF
cat "$SRC/NOTICE" >> "$PKG/usr/share/doc/edita/copyright"

# --- Dependencias -------------------------------------------------------------------------
DEPENDS=""
if command -v dpkg-shlibdeps >/dev/null; then
    ELF=()
    while IFS= read -r -d '' file; do
        if [[ $(head -c 4 "$file" | od -An -c | tr -d ' ') == 177ELF ]]; then
            ELF+=("$file")
        fi
    done < <(find "$PKG/opt" -type f \( -name '*.so*' -o -perm /u+x \) -print0)
    LIBDIRS=()
    while IFS= read -r dir; do
        LIBDIRS+=("-l$dir")
    done < <(find "$PKG/opt" -name '*.so*' -printf '%h\n' | sort -u)
    mkdir -p "$WORK/debian"
    printf 'Source: edita\n\nPackage: edita\nArchitecture: %s\n' "$ARCH" > "$WORK/debian/control"
    DEPENDS=$(cd "$WORK" && dpkg-shlibdeps -O --ignore-missing-info "${LIBDIRS[@]}" "${ELF[@]}" 2>/dev/null \
              | sed -n 's/^shlibs:Depends=//p')
fi
if [[ -z $DEPENDS ]]; then
    # Sin dpkg-shlibdeps: lo que pide el AppImage al sistema (Ubuntu 22.04 y más nuevos)
    DEPENDS="libc6 (>= 2.35), libstdc++6 (>= 12), libgl1, libegl1, libopengl0, libfontconfig1, libfreetype6, \
libx11-6, libx11-xcb1, libxcb1, libxkbcommon0, libxkbcommon-x11-0, libglib2.0-0, libdbus-1-3, zlib1g"
fi

cat > "$PKG/DEBIAN/control" <<EOF
Package: edita
Version: $DEB_VERSION
Architecture: $ARCH
Maintainer: Edita contributors <edita@users.noreply.github.com>
Installed-Size: $(du -sk "$PKG" | cut -f1)
Depends: $DEPENDS
Recommends: shared-mime-info, desktop-file-utils
Section: editors
Priority: optional
Homepage: https://github.com/alktrip/edita
Description: Text and source code editor inspired by Notepad++
 Edita brings Notepad++ to Linux: tabs and split views, syntax highlighting for
 more than 90 languages, search and replace with regular expressions in files
 and folders, macros, column editing, sessions, plugins and its configuration
 files.
 .
 This package includes the Qt libraries Edita needs.
EOF

mkdir -p "$OUT"
DEB=$OUT/edita_${DEB_VERSION}_$ARCH.deb
dpkg-deb --root-owner-group -Zxz --build "$PKG" "$DEB" >/dev/null
echo "$DEB"
