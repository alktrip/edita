# Fase 106 — El paquete Flatpak

Fecha: 2026-11-28

## Por qué

Flathub es la tienda de aplicaciones que comparten casi todas las distribuciones de Linux. Estar en ella es la forma más directa de que alguien encuentre Edita e instale versiones nuevas sin hacer nada. Las fases 104 (metadatos de AppStream) y 105 (el puente al sistema) la prepararon; esta crea el paquete y lo prueba dentro de una sandbox real.

## Qué se hizo

- **`packaging/flatpak/io.github.alktrip.Edita.yml`**: Edita con el runtime de KDE 6.11 (Qt 6.11), más dos módulos:
  - **libzip**, para instalar los paquetes de plugins;
  - **host-spawn** 1.6.2 (MIT-0), compilado desde su código con las dependencias incluidas (sin red, como exige Flathub).

  Edita se compila con `EDITA_DESKTOP_ID=io.github.alktrip.Edita` (el `.desktop` y el icono con el ID de la aplicación) y sin el aviso de versiones.
- **Permisos**:
  - Wayland, con X11 de reserva, y la GPU.
  - La red: el catálogo de plugins, el plugin de FTP y los servidores de lenguaje que descargan cosas.
  - `--filesystem=host`: un editor abre y guarda archivos en cualquier sitio.
  - `org.freedesktop.Flatpak`: el puente al sistema.
  - Las notificaciones del escritorio.
- **`build.sh`** compila e instala el paquete para el usuario, y con `bundle` genera un `.flatpak` para llevarlo a otro equipo. El `README.md` de la carpeta explica cómo enviarlo a Flathub, con el texto que justifica el permiso del puente.
- **CI** (`.github/workflows/flatpak.yml`): en cada cambio se compila en el contenedor oficial de Flathub (`kde-6.11`), se pasa el linter de Flathub al manifiesto y se guarda el `.flatpak` como artefacto.

## Pruebas en una sandbox real

Con el paquete compilado e instalado en este equipo:

- **Arranca** e informa su versión.
- **El puente**: 4507 programas del sistema enlazados en una carpeta privada de la aplicación (no en la del sistema). `git log` y `uname` desde dentro se ejecutan fuera, en la carpeta correcta.
- **La terminal**: la shell del sistema recibe con `host-spawn` un pseudoterminal propio del sistema, con el tamaño de la ventana (41×123) y en primer plano (`Rs+`). Funcionan Ctrl+C y Ctrl+Z.
- **Los plugins**:
  - Los cinco incluidos están en el paquete y no les falta ninguna biblioteca.
  - Los **21 plugins compilados del catálogo**, hechos en Ubuntu 22.04 con Qt 6.8, encuentran todas sus bibliotecas y **pasan sus comprobaciones** (`edita --check-plugin` con su receta) dentro del Flatpak. XML Tools incluido, que usa libxml2.
- **El linter de Flathub**:
  - Al manifiesto solo le marca el aviso esperado del permiso `flatpak-spawn` (se justifica al enviarlo).
  - Al paquete, además, las capturas y el icono «no replicados». Es lo normal en una compilación local: el servidor de Flathub los copia al compilar la versión oficial, y el catálogo generado ya apunta a `https://dl.flathub.org/media/`.

## Pendiente

El envío a Flathub, con la versión 1.0.0: un PR a `flathub/flathub` con este manifiesto y la fuente de Edita en la etiqueta `v1.0.0`. Los pasos están en `packaging/flatpak/README.md`.
