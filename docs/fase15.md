# Fase 15 — Plugins de Notepad++ compilados en el equipo (recetas): resultado

Fecha: 2026-10-20 · Entorno: el mismo que en la fase 14, más cmake y un compilador de C++ en el equipo del usuario (para compilar plugins)

Objetivo: instalar plugins de Notepad++ cuyo código fuente es público, sin publicar ningún binario ni fork. El administrador de plugins hace lo siguiente en el equipo del usuario:

1. descarga el código original (la etiqueta publicada en GitHub);
2. comprueba su SHA-256;
3. le aplica el port de Edita;
4. lo compila;
5. lo reempaqueta en una lista local;
6. lo instala.

Edita solo distribuye el port: archivos de compilación, parches y, cuando hace falta, código propio. Para probar todo el proceso hay dos plugins reales: MIME Tools (sin cambios en su código) y XML Tools (con un port).

## Qué se construyó

| Módulo | Qué hace |
|---|---|
| `plugin-sdk/compat/` | **Capa de compatibilidad Win32** para compilar en Linux código escrito para la API de plugins de N++. `npp_compat.h` define los tipos (`HWND`, `TCHAR`, `WPARAM`/`LPARAM`, `BOOL`, `DLGPROC`…), `__declspec(dllexport)`, `TEXT()`, las funciones de cadena (`lstrcpy`, `_tcs*`…) y `::SendMessage`/`PostMessage`, que van a `EditaData::send`. `MessageBox` es un `QMessageBox`. Los diálogos Win32 (`CreateDialogParam`, `DialogBox`…) muestran un aviso con el nombre y la página del plugin. `windows.h`, `tchar.h`, `shlwapi.h`, `commctrl.h`, `windowsx.h`, `shellapi.h` y `shlobj.h` solo incluyen `npp_compat.h`, de modo que los `#include` del original no se tocan. Las exportaciones de N++ (`setInfo`, `getName`, `getFuncsArray`, `beNotified`, `messageProc`, `isUnicode`) se renombran a `npp_plugin_*`. `npp_compat.cpp` exporta las de Edita: convierte `EditaData` en el `NppData` del plugin, llama a su `DllMain(DLL_PROCESS_ATTACH)` si lo tiene y pasa el `FuncItem*` del plugin tal cual, porque tiene la misma disposición en memoria con `TCHAR = char` |
| `plugin-sdk/cmake/EditaNppPlugin.cmake` | `edita_npp_plugin(nombre SOURCES … [INCLUDES …] [LIBRARIES …] [DEFINITIONS …])` compila un plugin de N++ con la capa de compatibilidad: un módulo `.so` con `compat/` delante en las cabeceras, Qt Widgets, `-fpermissive` solo en C++, el código original sin warnings y la versión de la receta en `edita_plugin_version()`. `edita_native_plugin(…)` hace lo mismo para un port que usa directamente `edita_plugin.h` |
| `core/PluginRecipe` | **Recetas** (`recipe.json`) y el **compilador** (`RecipeBuilder`), que ejecuta los pasos en orden y deja un registro: requisitos del sistema → SHA-256 del código → descomprimir (con `strip-components`) → quitar archivos → copiar el *overlay* → parches (`patch -p1 --forward`) → `cmake` (Ninja si existe) → `cmake --build --parallel` → empaquetar `<carpeta>.so` en `<carpeta>-<versión>.zip`. Cada paso es un `QProcess` asíncrono y se puede cancelar. `findPluginSdk()` encuentra el SDK en `EDITA_PLUGIN_SDK`, en el instalado (`share/edita/plugin-sdk`) o en el del código. `missingRequirements()` comprueba órdenes y módulos de `pkg-config` |
| `ui/PluginsAdminDialog` | Las recetas aparecen en **Disponibles** y **Actualizaciones** como cualquier plugin, con «(se compila)» junto a la versión y una nota en la descripción. Al instalar se abre una ventana con el registro de la compilación en directo y un botón Cancelar. El paquete resultante se guarda en `~/.config/edita/plugins/local/`: al reinstalar la misma versión se usa directamente, sin compilar otra vez. Después se instala con el mismo instalador de la fase 14 (SHA-256, carpeta y reinicio) |
| `MainWindow::pluginsAdminContext` | Lee las recetas de `$EDITA_PLUGIN_RECIPES`, `~/.config/edita/plugins/recipes/` y `<datos>/plugins/recipes/` (gana la primera que tenga un nombre). Compila en `~/.cache/edita/plugin-builds/` |
| Instalación | `share/edita/plugin-sdk/` (`edita_plugin.h`, `Scintilla.h`, `Sci_Position.h`, `compat/`, `cmake/`) y `share/edita/plugins/recipes/` |

### Formato de una receta

```json
{
  "folder-name": "mimeTools", "display-name": "MIME Tools", "version": "3.1",
  "description": "…", "author": "Don HO", "homepage": "https://github.com/npp-plugins/mimetools",
  "source": { "url": "https://github.com/npp-plugins/mimetools/archive/refs/tags/v3.1.zip",
              "sha256": "dae7ce6a…", "strip-components": 1 },
  "remove":   ["src/Scintilla.h", "src/Sci_Position.h"],
  "patches":  ["algo.patch"],
  "overlay":  "overlay",
  "build-dir": "edita",
  "requires": ["cmake", "c++", "pkg-config:libxml-2.0"]
}
```

- `remove` quita del original las cabeceras de Scintilla de Windows, para que se usen las del SDK.
- El *overlay* se copia encima del código.
- `build-dir` es la carpeta (del overlay) con el `CMakeLists.txt` que llama a `edita_npp_plugin` o `edita_native_plugin`.

### Los dos plugins

| Plugin | Original | Qué hace la receta | Funciona |
|---|---|---|---|
| **MIME Tools** 3.1 | [npp-plugins/mimetools](https://github.com/npp-plugins/mimetools) (GPL v3) | Solo un `CMakeLists.txt` de seis líneas: el código original compila **sin cambios** con la capa de compatibilidad | Su menú completo (Base64, Quoted-printable, URL encode/decode, SAML). Probados: codificar y decodificar Base64, URL y Quoted-printable |
| **XML Tools** 3.1.1.13 | [morbac/xmltools](https://github.com/morbac/xmltools) (GPL v3) | Un parche (`std::tolower` es ambiguo en GCC) y un port nativo, `XMLToolsEdita.cpp`. Usa su biblioteca **QuickXml sin cambios** para formatear, linealizar y obtener la ruta actual. Donde el original usa MSXML (COM de Windows) se usa **libxml2**: sintaxis, validación con DTD o XSD, y XPath. Para XSLT usa **libxslt**, que es opcional. Los diálogos MFC se sustituyen por diálogos de Qt. Las opciones se guardan en `XMLTools.ini` | Comprobación de la sintaxis (manual y al guardar, con el error en la barra de estado y el cursor en la línea), validación, pretty print (normal, con atributos sangrados, solo sangría), linearize, ruta XML actual (con y sin predicados), evaluar XPath, XSLT, escapar y desescapar, y comentar y descomentar, con los atajos del original (Ctrl+Alt+Mayús+B, A, L, P, M, C, R) |

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| De dónde sale el plugin | DLL compilada por el autor, en la lista oficial | Se compila en el equipo desde la etiqueta publicada por el autor, con el SHA-256 del código fijado en la receta | Las DLL no se pueden cargar en Linux. Así no hay que publicar binarios ni forks |
| Requisitos | Ninguno | cmake y un compilador de C++. Algunos plugins necesitan además bibliotecas de desarrollo (XML Tools: libxml2). El administrador dice qué falta antes de empezar | Compilar es el precio de usar el código original |
| Primera instalación | Unos segundos | Lo que tarde la compilación (unos segundos para MIME Tools; algo más para XML Tools) | Reinstalar usa el paquete local |
| XML Tools: menú | 31 elementos | 16 órdenes y el «Acerca de». Faltan la navegación entre errores (primero/anterior/siguiente/último), el cierre y la sangría automáticos de etiquetas, el autocompletado de atributos, «Set XML type automatically», «Prevent XXE» (libxml2 no carga entidades externas por defecto), «Allow huge files», «Apply to all open files», «Tokenize (debug)», el diálogo de opciones y la ventana de depuración | Estas órdenes dependen de MFC o de MSXML. Se dejan para más adelante |
| Diálogos Win32 en la capa de compatibilidad | — | No se emulan: un plugin que abre uno muestra un aviso | Para plugins con diálogos, la receta trae un port de su interfaz (como XML Tools) |

## Escribir una receta

Está explicado en [plugins.md](plugins.md#compilar-un-plugin-de-notepad-desde-su-código-recetas).

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Las recetas se leen (una sin `source` se descarta con un aviso). Las de Edita apuntan a GitHub con un SHA-256 completo. El SDK sin instalar se encuentra y está completo. Se detectan los requisitos que faltan | ✅ `tst_recipes`: `recipesAndSdk` |
| Un plugin con la estructura de los de N++ se compila con la receta (`DllMain`, `::SendMessage`, `lstrcpy`, su propio `PluginInterface.h` y una `Scintilla.h` de Windows que hay que quitar), con parche y overlay. El paquete exporta las funciones de Edita. Edita lo carga con su versión y sus órdenes modifican el texto; `beNotified` recibe `NPPN_READY` | ✅ `builderCompilesAnUpstreamPlugin` |
| La compilación falla con un mensaje claro si el SHA-256 del código no coincide, si falta un requisito o si hay un error de compilación (que queda en el registro), y se puede cancelar | ✅ `builderFailures` |
| Desde el administrador: aparece en Disponibles con «(se compila)»; al instalar se compila y se guarda en `plugins/local/`; al quitarlo y reinstalarlo se usa el paquete local; con una receta más nueva aparece en Actualizaciones y se compila otra vez | ✅ `pluginsAdminBuildsAndCaches` |
| Las recetas reales de MIME Tools y XML Tools compilan el código original. MIME Tools codifica y decodifica Base64 y URL; XML Tools hace pretty print, linearize y escapar | ✅ `realNotepadPlusPlusPlugins`: usa `referencia/plugins` o, con `EDITA_TEST_PLUGIN_ARCHIVES`, los zip de GitHub con su SHA-256 |

Además:

- Se instaló en un prefijo temporal y se repitieron las compilaciones con el SDK instalado (`EDITA_PLUGIN_SDK=<prefijo>/share/edita/plugin-sdk`). Todas pasan.
- Se compilaron los dos plugins con los zip descargados de GitHub. El SHA-256 coincide con el de las recetas.
- Se probaron en Edita los dos plugins:
  - MIME Tools, desde Python Script con `notepad.runPluginCommand`;
  - XML Tools, desde el menú: formatear, linealizar, comentar y la comprobación de la sintaxis al guardar.

Tests: `tst_recipes` tiene 7 casos (contando `initTestCase` y `cleanupTestCase`); cada vez compila plugins de verdad (unos 10 s). `tst_pluginsadmin` deja fuera las recetas para seguir probando solo la lista. Pasan las 23 suites y el build no da ningún warning.
