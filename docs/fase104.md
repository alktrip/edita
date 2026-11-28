# Fase 104 — Metadatos de AppStream

Fecha: 2026-11-28

## Por qué

AppStream es el formato con el que una aplicación se presenta en las tiendas de Linux: GNOME Software, KDE Discover y Flathub muestran su nombre, su resumen, la descripción, las capturas, el desarrollador, las versiones con sus notas y la licencia a partir de un archivo `<id>.metainfo.xml` en `/usr/share/metainfo`. Sin él, Edita instalada desde un .deb o un .rpm no aparece en esas tiendas como aplicación, y Flathub no la acepta. Es el primer paso para darla a conocer y para la versión de Flatpak (fases 105 y 106).

## Qué se hizo

- **`resources/edita.metainfo.xml.in`**, que CMake convierte en `io.github.alktrip.Edita.metainfo.xml` y se instala en `share/metainfo`:
  - El ID `io.github.alktrip.Edita`: un dominio invertido, el que Flathub verifica con la cuenta de GitHub del proyecto.
  - Nombre, resumen y descripción en inglés y en español, el desarrollador, la licencia (GPL-3.0-or-later; la de los metadatos, CC0-1.0), las URL del proyecto, los issues y la guía para colaborar.
  - Seis capturas en inglés, las del README, con su texto en los dos idiomas.
  - Categorías y palabras clave, colores para la tienda (claro y oscuro), la clasificación por edades (OARS: sin nada que declarar), teclado y ratón, y una pantalla de al menos 768 píxeles.
  - Las versiones: de la beta 1 a la rc 7. Las de prueba, con `type="development"` y la forma de AppStream `1.0.0~rc.7`, para que `1.0.0` cuente como más nueva.
- **`EDITA_DESKTOP_ID`** (CMake): el nombre del `.desktop` y del icono. En los .deb, .rpm y AppImage sigue siendo `edita` (las asociaciones de archivos que ya hicieron los usuarios lo usan). Flatpak pondrá `io.github.alktrip.Edita`, como exige Flathub. El metainfo lo enlaza con `<launchable>`, y `setDesktopFileName` lo usa para que Wayland una la ventana con su icono.
- **El AppImage** ya no deja que `appimagetool` valide los metadatos: lo haría con el AppStream 0.15 de Ubuntu 22.04, que no conoce las etiquetas de AppStream 1.0 que piden las tiendas actuales (`vcs-browser`, `contribute`, `developer`). Los valida la prueba nueva con un AppStream actual. Los .deb y .rpm, que salen del mismo AppDir, llevan el archivo.

## Al publicar una versión

Añadir su `<release>` al principio de `<releases>` en `resources/edita.metainfo.xml.in` (con `type="development"` si es de prueba). `tst_metainfo` falla si la versión de `CMakeLists.txt` no está.

## Pruebas

`tst_metainfo`: el metainfo se corresponde con la aplicación (el ID, el `.desktop` de `<launchable>` y el ejecutable), la versión de la compilación es la primera de `<releases>` y, si es de prueba, está marcada; `appstreamcli validate --strict` lo acepta (con AppStream 1.0 o posterior); y el `.desktop` tiene el icono y la orden correctos y pasa `desktop-file-validate`.
