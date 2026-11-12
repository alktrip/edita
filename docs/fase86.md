# Fase 86 — Paquetes .rpm y versión para arm64

Fecha: 2026-11-12

## Por qué

Hasta ahora Edita se publicaba para PC (x86_64), como AppImage y como .deb. Faltaban dos cosas:
- un paquete para Fedora, openSUSE y las demás distribuciones con rpm, para instalar Edita con su gestor de paquetes y tenerlo en el menú;
- una versión para ARM de 64 bits: Raspberry Pi 4 y 5, portátiles con procesadores ARM y servidores.

## Cómo lo hace Notepad++

N++ es solo para Windows (x86, x64 y ARM64) y se distribuye con su instalador NSIS y en zip. En Linux no hay un equivalente que copiar: cada distribución tiene su formato.

## Qué se hizo

- **`packaging/rpm/build-rpm.sh`**: el .rpm con el mismo AppDir que el AppImage y el .deb, en `/opt/edita`, con `/usr/bin/edita`, el menú, los iconos y las licencias en `/usr/share/licenses/edita`.
  - Las dependencias las calcula rpmbuild a partir de los ELF, con nombres que valen en cualquier distribución (`libGL.so.1()(64bit)`), sin las bibliotecas que ya van dentro.
  - No anuncia las bibliotecas de `/opt` como propias y no pasa `strip` ni genera bytecode.
  - La versión lleva `~` (`1.0.0~rc.1`), igual que el .deb.
  - Se probó en el chroot de Ubuntu 22.04: el paquete pide 23 bibliotecas del sistema (glibc, libstdc++, OpenGL/EGL, X11/xcb, Wayland, fontconfig, freetype, zlib…), ninguna de Qt.
- **La arquitectura del equipo** en `build-appimage.sh` (`uname -m`: `x86_64` o `aarch64`, para elegir linuxdeploy y su plugin de Qt), en `build-deb.sh` (`amd64` o `arm64`) y en el .rpm. `build-portable.sh` instala `rpm` en el chroot y también deja el .rpm.
- **La CI** (`.github/workflows/linux.yml`):
  - **pruebas** y **paquetes** corren para x86_64 (`ubuntu-22.04`) y arm64 (`ubuntu-24.04-arm`), con el Qt 6.8.3 oficial de cada una (`linux_gcc_64` y `linux_gcc_arm64`). arm64 va en Ubuntu 24.04 porque el Qt oficial para arm64 pide glibc 2.38 (en 22.04, `moc` no arranca): la versión ARM necesita glibc 2.39 (Ubuntu 24.04, Debian 13, Raspberry Pi OS 13, Fedora 40). Para que llegara a Debian 12 y Raspberry Pi OS 12 habría que compilar Qt en Ubuntu 22.04.
  - El AppImage se comprueba arrancándolo (`--version`).
  - **instalar** instala el .rpm en Fedora 41 y el .deb en Debian 12 (x86_64) o 13 (arm64), en contenedores limpios, y comprueba que `edita --version` funciona y que el menú quedó instalado.
  - **publicar** sube al release, solo si todo lo anterior pasa, el AppImage, el .deb y el .rpm de las dos arquitecturas.
  - `apt-mirror.sh` usa `ports.ubuntu.com` en arm64.
- **El catálogo de plugins** ([edita-plugins](https://github.com/alktrip/edita-plugins)) descarga del release continuo solo el AppImage de x86_64, que es para el que compila sus binarios. En arm64 el administrador ofrece compilar desde la receta.
- **Documentación**: README, PROBAR.md, el manual (Introducción › Instalación, en los dos idiomas) y `docs/empaquetado.md`.

## Pruebas

No hay pruebas nuevas en `ctest`: lo nuevo son los paquetes, y los prueba la CI instalándolos en Fedora y Debian en las dos arquitecturas. Toda la suite corre ahora también en arm64.
