#!/bin/bash
# Compila Edita como Flatpak con el código de esta copia del repositorio y la instala para el
# usuario (fase 106). Necesita flatpak y el remoto flathub; la primera vez descarga
# flatpak-builder y el SDK de KDE (unos 2 GB).
#   packaging/flatpak/build.sh          compilar e instalar (flatpak run io.github.alktrip.Edita)
#   packaging/flatpak/build.sh bundle   además, Edita-<arch>.flatpak para pasarlo a otro equipo
set -euo pipefail
cd "$(dirname "$0")"
APP=io.github.alktrip.Edita
flatpak install --user -y --noninteractive flathub org.flatpak.Builder
flatpak run org.flatpak.Builder --user --install --force-clean --install-deps-from=flathub \
    --state-dir=.flatpak-builder --repo=repo build-dir "$APP.yml"
flatpak run --command=flatpak-builder-lint org.flatpak.Builder manifest "$APP.yml" || true
if [[ ${1:-} == bundle ]]; then
    flatpak build-bundle repo "Edita-$(uname -m).flatpak" "$APP"
    echo "Listo: $(pwd)/Edita-$(uname -m).flatpak (flatpak install --user Edita-$(uname -m).flatpak)"
fi
