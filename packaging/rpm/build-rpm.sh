#!/usr/bin/env bash
# This file is part of Edita.
# Copyright (C) 2026 Edita contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Genera edita-<versión>-1.<arquitectura>.rpm (Fedora, RHEL y compatibles, openSUSE) con el
# AppDir del AppImage, como el .deb (lo llama build-appimage.sh al terminar):
#   /opt/edita/usr/…                    el AppDir (Edita, Qt, plugins, Python, datos)
#   /usr/bin/edita                      enlace a /opt/edita/usr/bin/edita
#   /usr/share/applications/edita.desktop y /usr/share/icons/hicolor/…
# Las dependencias las calcula rpmbuild a partir de los ELF (libGL.so.1()(64bit)…: valen en
# todas las distribuciones), sin las bibliotecas que ya van dentro (Qt, ICU, Python…), que
# tampoco se anuncian como propias para no chocar con las del sistema.
#
# Uso: build-rpm.sh <AppDir> <versión> <carpeta de salida>
# Necesita rpmbuild (paquete rpm en Debian y Ubuntu, rpm-build en Fedora).

set -euo pipefail

APPDIR=$(cd "$1" && pwd)
VERSION=$2
OUT=$3
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SRC=$(cd "$HERE/../.." && pwd)
# 1.0.0-rc.1 → 1.0.0~rc.1: para rpm, con ~ va antes que 1.0.0 (como en dpkg)
RPM_VERSION=${VERSION//-/\~}
ARCH=$(uname -m)

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
ROOT=$WORK/root

mkdir -p "$ROOT/opt/edita" "$ROOT/usr/bin" "$ROOT/usr/share/licenses/edita"
cp -a "$APPDIR/usr" "$ROOT/opt/edita/"
ln -s /opt/edita/usr/bin/edita "$ROOT/usr/bin/edita"
mv "$ROOT/opt/edita/usr/share/applications" "$ROOT/usr/share/applications"
mv "$ROOT/opt/edita/usr/share/icons" "$ROOT/usr/share/icons"
cp "$SRC/COPYING" "$SRC/NOTICE" "$ROOT/usr/share/licenses/edita/"
find "$ROOT" -type d -exec chmod 755 {} +
find "$ROOT/opt" -type f -perm /u+x -exec chmod 755 {} +
find "$ROOT/opt" -type f ! -perm /u+x -exec chmod 644 {} +

# Las bibliotecas que van dentro: rpm no las pide al sistema
BUNDLED=$(find "$ROOT/opt" -name '*.so*' -type f -printf '%f\n' | sed -E 's/\.so.*/.so/' | sort -u \
          | sed -e 's/[.+]/\\&/g' | paste -sd'|')

# La lista de archivos: todo lo de /opt y lo que va en /usr
FILES=$WORK/files.txt
(cd "$ROOT" && find opt/edita -mindepth 1 -maxdepth 1 -printf '/%p\n'
 echo /usr/bin/edita
 find usr/share/applications usr/share/icons -type f -printf '/%p\n'
 echo /usr/share/licenses/edita) > "$FILES"

SPEC=$WORK/edita.spec
cat > "$SPEC" <<EOF
Name:           edita
Version:        $RPM_VERSION
Release:        1
Summary:        Text and source code editor inspired by Notepad++
License:        GPL-3.0-or-later
URL:            https://github.com/alktrip/edita
Recommends:     shared-mime-info
Recommends:     desktop-file-utils
AutoReqProv:    yes

# Lo que se instala ya está compilado: ni símbolos de depuración, ni strip, ni bytecode
%global debug_package %{nil}
%global __os_install_post %{nil}
%global __brp_python_bytecompile %{nil}
%global _build_id_links none
%global __provides_exclude_from ^/opt/edita/.*\$
%global __requires_exclude ^((${BUNDLED})|/usr/(local/)?bin/python.*)
%define _rpmdir $WORK/rpms
%define _build_name_fmt %%{NAME}-%%{VERSION}-%%{RELEASE}.%%{ARCH}.rpm

%description
Edita brings Notepad++ to Linux: tabs and split views, syntax highlighting for
more than 90 languages, search and replace with regular expressions in files
and folders, macros, column editing, sessions, plugins and its configuration
files.

This package includes the Qt libraries Edita needs.

%install
cp -a "$ROOT"/. %{buildroot}/

%post
update-desktop-database -q /usr/share/applications >/dev/null 2>&1 || :
gtk-update-icon-cache -q /usr/share/icons/hicolor >/dev/null 2>&1 || :

%postun
update-desktop-database -q /usr/share/applications >/dev/null 2>&1 || :
gtk-update-icon-cache -q /usr/share/icons/hicolor >/dev/null 2>&1 || :

%files -f $FILES
%defattr(-,root,root,-)
EOF

rpmbuild -bb --quiet --target "$ARCH" \
    --define "_topdir $WORK/top" \
    --define "_tmppath $WORK/tmp" \
    "$SPEC" >/dev/null 2>"$WORK/rpmbuild.log" || { cat "$WORK/rpmbuild.log" >&2; exit 1; }

mkdir -p "$OUT"
RPM=$(find "$WORK/rpms" -name '*.rpm' -print -quit)
cp "$RPM" "$OUT/"
echo "$OUT/$(basename "$RPM")"
