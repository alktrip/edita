# Empaquetado: AppImage

`packaging/appimage/build-appimage.sh` genera `dist/Edita-<versión>-x86_64.AppImage`: un solo archivo ejecutable con Edita, Qt, sus plugins (Python Script y el de ejemplo) y Python.

```sh
editor/packaging/appimage/build-appimage.sh     # deja dist/Edita-0.0.1-x86_64.AppImage
chmod +x dist/Edita-*.AppImage && ./dist/Edita-*.AppImage
```

Necesita `cmake`, `ninja`, `curl`, `qmake6` y `python3`, además de lo que pide la compilación normal. La primera vez descarga [linuxdeploy](https://github.com/linuxdeploy/linuxdeploy) y su [plugin de Qt](https://github.com/linuxdeploy/linuxdeploy-plugin-qt) a `build-appimage/tools/`. Compila en `build-appimage/build` (Release, sin pruebas), así que no toca `build/`.

## Qué hace

1. Instala Edita en `build-appimage/AppDir` con prefijo `/usr`. Edita busca sus datos (`../share/edita`) y sus plugins (`../lib/edita/plugins`) junto al ejecutable, así que funciona igual dentro del AppImage.
2. Copia la biblioteca estándar de Python (sin pruebas, IDLE ni Tk) a `usr/lib/python3.X`. Python Script la usa cuando la encuentra junto a él; si no, usa la del sistema. Así funciona aunque el sistema tenga otra versión de Python o no tenga Python.
3. linuxdeploy copia Qt y las bibliotecas de las que dependen Edita, sus plugins y los módulos de Python. Qt va con sus plugins de X11, Wayland y `offscreen` (sin pantalla), y con sus traducciones (`qtbase_es.qm`). Las bibliotecas base del sistema (glibc, OpenGL, X11…) no se copian.
4. Pone un `AppRun` que arranca `usr/bin/edita` por su ruta real. Si se arrancara por el enlace que crea linuxdeploy, Qt no encontraría `qt.conf` ni sus plugins.

Detalles:

- Solo se incluyen los plugins de Qt propios de Qt. Los que instalan otros en la misma carpeta (los formatos de imagen `kimg_*` de KDE, el tema de Plasma, estilos) arrastrarían sus bibliotecas. El script le pasa a linuxdeploy un `qmake` que apunta a una copia filtrada.
- No se quitan los símbolos (`NO_STRIP=1`): el `strip` de linuxdeploy no entiende las bibliotecas de distribuciones recientes, que además ya vienen sin símbolos de depuración.

## En qué sistemas funciona

**El AppImage necesita una glibc igual o más nueva que la del equipo donde se generó.** Generado en Arch (glibc 2.44), solo funciona en distribuciones igual de recientes. Para que sirva en más, hay que generarlo en una distribución más antigua, por ejemplo en un contenedor, con Qt 6.7 o posterior (las distribuciones antiguas traen un Qt anterior; se puede instalar aparte).

## Limitaciones

- **Plugins compilados desde su código (recetas, fase 15):** se compilan con el Qt y el compilador del sistema, y dentro del AppImage se cargan con el Qt del AppImage. Funciona si el Qt del sistema no es más nuevo que el del AppImage. Hacen falta cmake, un compilador de C++ y las cabeceras de desarrollo de Qt 6.
- La integración con el escritorio va por el portal de XDG (`xdgdesktopportal`), que también da el diálogo de archivos del escritorio. El tema de Plasma no va incluido.
