# Edita

Editor de texto nativo para Linux inspirado en Notepad++. Usa el mismo motor de
edición que Notepad++ (Scintilla 5 + Lexilla, con sus parches y su regex Boost)
y una interfaz en Qt 6. Licencia GPL v3 o posterior (ver `COPYING` y `NOTICE`).

Estado: **completo** (MVP en las fases 0-5 y ampliaciones en las 6-22): pestañas, abrir/guardar con detección
de codificación y fin de línea idéntica a Notepad++, resaltado de sintaxis con
`langs.xml`/`stylers.xml` y sus temas, buscar/reemplazar (normal, extendido y
regex de Boost) también en archivos, vista dividida con clones y restauración
de la sesión al volver a abrir.

Después del MVP:
- Fase 6: aviso de cambios externos, copias de los cambios sin guardar que se recuperan al volver a abrir (modo "snapshot") y `.bak` al guardar.
- Fase 7: diálogo de preferencias (Configuración > Preferencias…), archivos recientes, resaltado inteligente y sangría automática.
- Fase 8: autocompletado con las API de N++ (palabras, funciones, parámetros, rutas, cierre de pares) y macros compatibles con `shortcuts.xml`.
- Fase 9: imprimir (con cabecera, pie, márgenes y modo de color) y exportar a PDF.
- Fase 10: lenguajes definidos por el usuario, compatibles con los `userDefineLang.xml` de N++, y su editor.
- Fase 11: plugins nativos (`.so`) con una API calcada de la de N++ y un plugin de ejemplo. Ver [docs/plugins.md](docs/plugins.md).
- Fase 12: órdenes de edición de N++: ordenar y depurar líneas, mayúsculas, comentar (Ctrl+Q), espacios y tabulaciones, marcadores (Ctrl+F2/F2), historial de cambios en el margen, etiqueta XML/HTML pareja, estilos de token, plegado por niveles (Alt+0…8) y zoom.
- Fase 13: plugin **Python Script** compatible con el PythonScript de N++ (`notepad`, `editor`, `console`, callbacks, consola acoplable); necesita las cabeceras de Python 3.
- Fase 14: **administrador de plugins** como el de N++ (Plugins > Administrador de plugins…): lista con el formato de `nppPluginList.json`, instalar, actualizar, quitar, desactivar y activar paquetes `.zip` verificados con SHA-256; la lista oficial de N++ se puede consultar (sus DLL salen como incompatibles).
- Fase 15: plugins de N++ **compilados en el equipo desde su código** (recetas): el administrador descarga la etiqueta original, verifica su SHA-256, aplica el port, compila y reempaqueta en local. Incluye MIME Tools (código original sin cambios) y XML Tools (port con libxml2). Requiere cmake y un compilador de C++.
- Fase 16: **interfaz en inglés y español** (completa). El código está en inglés y el español es una traducción de Qt (`translations/edita_es.ts`). El idioma es el del sistema o el de Preferencias > General > Idioma. Opción para usar los diálogos de archivos de Qt, que siguen el idioma de Edita.
- Fase 17: **interfaz moderna**: modo claro u oscuro siguiendo al sistema (con un tema de colores para cada modo), barra de herramientas con los botones de N++, iconos de línea propios, pestañas planas con un punto para «sin guardar» y la letra monoespaciada del sistema en el editor.
- Fase 18: repositorio git y **una sola instancia**: con Edita abierto, los archivos se abren en esa ventana (`--multiInst` o Preferencias para abrir otra).
- Fase 19: **Configurador de estilos** como el de N++ (Configuración > Configurador de estilos…): colores, fuentes, Global override, extensiones y palabras clave del usuario, con vista previa; guarda el tema (los de la aplicación, en una copia en `~/.config/edita/themes`).
- Fase 20: **Asignación de atajos** (Shortcut Mapper de N++): menú, macros, plugins y órdenes de Scintilla, guardados en `shortcuts.xml` con el formato de N++.
- Fase 21: **Editor de columnas** (Alt+C): texto o números (incremento, repetición, base, relleno) en la columna del cursor o en cada línea de una selección rectangular.
- Fase 22: **paneles** de N++: Lista de documentos, Mapa del documento, Lista de funciones (con los analizadores `functionList` de N++) y Explorador de carpetas (Archivo > Abrir carpeta como espacio de trabajo).

Plan en [docs/fase1-analisis.md](docs/fase1-analisis.md);
registro de cada fase: [0](docs/fase0.md), [1](docs/fase1.md), [2](docs/fase2.md),
[3](docs/fase3.md), [4](docs/fase4.md), [5](docs/fase5.md), [6](docs/fase6.md), [7](docs/fase7.md), [8](docs/fase8.md), [9](docs/fase9.md), [10](docs/fase10.md), [11](docs/fase11.md), [12](docs/fase12.md), [13](docs/fase13.md), [14](docs/fase14.md), [15](docs/fase15.md), [16](docs/fase16.md), [17](docs/fase17.md), [18](docs/fase18.md), [19](docs/fase19.md), [20](docs/fase20.md), [21](docs/fase21.md), [22](docs/fase22.md).

Uso: `edita [archivos…] [-n línea] [-c columna] [--nosession] [--multiInst]`

Configuración del usuario en `~/.config/edita/` (`langs.xml`, `stylers.xml`,
`config.xml`, `session.xml`, `shortcuts.xml`, `themes/`, `backup/`, `autoCompletion/`, `userDefineLang.xml`, `userDefineLangs/`, `plugins/`), con el mismo formato que Notepad++.


## Compilar (Arch Linux)

Desde el directorio que contiene `editor/`:

```sh
sudo pacman -S --needed qt6-base qt6-5compat qt6-svg qt6-tools qt6-translations cmake ninja gcc python libzip libxml2 libxslt   # opcionales: python (Python Script), libzip (paquetes de plugins), libxml2/libxslt (receta de XML Tools)
cmake -S editor -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure      # -LE perf para saltar el de rendimiento; poppler (pdftotext) opcional
./build/edita
```

Opciones de CMake:

| Opción | Por defecto | |
|---|---|---|
| `EDITA_WERROR` | `ON` | warnings del código propio como errores |
| `EDITA_BUILD_TESTS` | `ON` | compila los tests (QtTest, sin pantalla) |
| `EDITA_BUILD_PLUGINS` | `ON` | compila los plugins: el de ejemplo (`plugins/EditaDemo`) y Python Script (si hay cabeceras de Python 3) |
| `EDITA_APP_ID` | `edita` | nombre del binario y de `~/.config/<id>/` |

## AppImage

`editor/packaging/appimage/build-appimage.sh` genera `dist/Edita-<versión>-x86_64.AppImage`, con Qt, los plugins y Python incluidos. Funciona en sistemas con una glibc igual o más nueva que la del equipo donde se generó. Ver [docs/empaquetado.md](docs/empaquetado.md).

## Estructura

```
src/core/      núcleo sin widgets: encodings, fin de línea, E/S de archivos, rutas
src/doc/       documentos abiertos sobre Scintilla (FileManager/Buffer de N++)
src/syntax/    langs.xml, stylers.xml/temas, detección de lenguaje, lexers y estilos
src/search/    motor de búsqueda (FindReplaceDlg sin interfaz) y buscar en archivos
src/ui/        ventana, editor (ScintillaEditBase), diálogos
tests/         QtTest, se ejecutan con QT_QPA_PLATFORM=offscreen; fixtures/ con generate.py
third_party/   Scintilla, Lexilla, boostregex, uchardet y pugixml copiados de Notepad++ (no editar a mano)
data/          langs.model.xml, stylers.model.xml, themes/, autoCompletion/ y userDefineLangs/ de Notepad++, y plugins/ (editaPluginList.json y las recetas; se instalan en share/edita)
translations/  edita_es.ts (español) y edita_en.ts (plurales del inglés): se compilan dentro del binario
resources/     logo e ícono de Edita; icons/ui/: iconos de línea de la interfaz (SVG) (en el binario; se instalan en share/icons/hicolor con edita.desktop)
tools/         import-third-party.sh: regenera third_party/ desde referencia/npp
plugin-sdk/    edita_plugin.h: API para escribir plugins (se instala en include/edita); compat/ y cmake/: capa Win32 y funciones de CMake para compilar plugins de N++ (en share/edita/plugin-sdk)
plugins/       plugin de ejemplo en C (EditaDemo) y Python Script
docs/          análisis, registro de cada fase, guía de plugins y empaquetado
packaging/     appimage/: script para generar el AppImage
```

`third_party/` se regenera con `tools/import-third-party.sh`. Los parches
locales están documentados en [third_party/PATCHES.md](third_party/PATCHES.md).
