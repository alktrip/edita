# Fase 24 — AppImage portable: resultado

Fecha: 2026-10-21

Objetivo: que el AppImage funcione en la mayoría de las distribuciones, no solo en las tan recientes como la del equipo donde se genera. El de la fase 17 se generaba en Arch (glibc 2.44) y no arrancaba en sistemas con una glibc anterior.

## Qué se construyó

| Parte | Qué hace |
|---|---|
| `packaging/appimage/build-portable.sh` | Genera el AppImage en **Ubuntu 22.04** (glibc 2.35). No necesita Docker ni privilegios. Descarga el sistema base oficial de Ubuntu (`ubuntu-base-22.04.5`), comprueba su SHA-256 con el `SHA256SUMS` de Ubuntu y lo descomprime en `build-portable/rootfs`. Entra en él con un espacio de usuario (`unshare --user --map-auto --map-root-user`) y `chroot`. La primera vez instala con apt g++ 12, las bibliotecas de desarrollo y Python 3.10. Con pip instala CMake 3.31 y aqtinstall, y con aqtinstall el **Qt 6.8.3 oficial**, compilado contra una glibc antigua. Después ejecuta dentro `build-appimage.sh` |
| `build-appimage.sh` | Acepta `QMAKE_BIN` (el qmake del Qt que se empaqueta) y `CMAKE_EXTRA` (opciones para cmake). Instala con `CMAKE_INSTALL_LIBDIR=lib`, para que los plugins vayan en `usr/lib/edita/plugins` también en Ubuntu. Incluye el plugin de Wayland con el nombre de cada versión de Qt: `libqwayland.so` desde Qt 6.10; antes, `libqwayland-generic.so` y `libqwayland-egl.so` |

```sh
editor/packaging/appimage/build-portable.sh     # deja dist/Edita-0.0.1-x86_64.AppImage
```

La primera vez descarga unos 30 MB del sistema base, unos 300 MB de paquetes y unos 200 MB de Qt. Las siguientes solo compilan.

## Cambios en el código

- **libzip 1.7** (la de Ubuntu 22.04) no tiene `ZIP_LENGTH_TO_END`, que es de la 1.10. Se usa `-1`, que ya significaba «hasta el final».
- **Plugins instalados**: Edita los busca también en `<prefijo>/<CMAKE_INSTALL_LIBDIR>/edita/plugins` (`kInstallLibDir` en `AppInfo.h`). En Debian y Ubuntu, GNUInstallDirs usa `lib/x86_64-linux-gnu`, y una instalación normal (`cmake --install`) no los encontraba.

## Resultado

| Comprobación | Resultado |
|---|---|
| Versión de glibc más alta que piden los binarios del AppImage | `GLIBC_2.35` |
| libstdc++ que piden Edita y sus plugins | `GLIBCXX_3.4.29` (GCC 11; Ubuntu 22.04 trae la de GCC 12) |
| Arranca en Ubuntu 22.04 (el mismo sistema base, sin pantalla): `--version` y la ventana con un archivo | ✅ |
| Arranca en Arch (glibc 2.44) | ✅ |
| Tamaño | 48 MB (antes 60 MB: el Qt oficial trae menos dependencias que el de Arch) |

Funciona en Ubuntu 22.04 o posterior, Debian 12 o posterior, Fedora 36 o posterior y en las distribuciones de esa época en adelante.

## Divergencias

Ninguna con N++: es empaquetado propio de Linux.

## Limitaciones

- Necesita `unshare` con `--map-auto`, es decir, rangos para el usuario en `/etc/subuid` y `/etc/subgid` (vienen de serie en la mayoría de las distribuciones).
- Las recetas de plugins (fase 15) se compilan en el equipo del usuario, con su Qt. Dentro del AppImage se cargan con el Qt 6.8 del AppImage, así que funcionan si el Qt del sistema no es más nuevo que el 6.8. Con un Qt del sistema más nuevo, el plugin compilado puede pedir símbolos que el 6.8 no tiene. En ese caso se puede usar Edita instalado desde el código.
