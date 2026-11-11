# Parches locales sobre el código de terceros

`third_party/` se regenera con `tools/import-third-party.sh` a partir de
`referencia/npp`, que ya trae los parches de Notepad++ sobre Scintilla 5.6.9 y
Lexilla 5.5.5, y su copia de uchardet (`PowerEditor/src/uchardet`). Encima de eso, Edita aplica estos parches mínimos, casi todos de
portabilidad a Linux/GCC y sin cambios de comportamiento (los que sí cambian algo lo dicen):

| Parche | Archivo | Motivo |
|---|---|---|
| `0001-lexuser-sin-windows-h.patch` | `lexilla/lexers/LexUser.cxx` | Incluía `<windows.h>` solo para obtener `strlen`/`strcmp` de forma indirecta y usaba `_itoa` de MSVC. Se sustituye por `<cstdio>`/`<cstring>` y un `_itoa` equivalente en base 10. |
| `0002-sci-position-stdint.patch` | `scintilla/include/Sci_Position.h` | N++ cambió `Sci_PositionCR` a `intptr_t` (para soportar más de 2 GB) sin incluir `<stdint.h>`; en MSVC llegaba de forma indirecta. |
| `0003-regex-error-thread-local.patch` | `scintilla/include/BoostRegexSearch.h`, `boostregex/BoostRegExSearch.cxx` | `g_exceptionMessage` (mensaje de la última regex inválida) era una variable global que se vacía en cada búsqueda. Edita busca en archivos en otro hilo mientras se puede buscar en el editor: pasa a `thread_local`. Ese es el único cambio de comportamiento: cada hilo ve su propio mensaje. |

| `0004-scintilla-h-c-compatible.patch` | `scintilla/include/Scintilla.h` | N++ añadió al final una estructura de C++ (`SearchResultMarkings`, con `std::vector`) para su panel de resultados, lo que impide incluir la cabecera desde C. Se rodea de `#ifdef __cplusplus` para que los plugins en C (como el de ejemplo) puedan usar `Scintilla.h`. Para C++ no cambia nada. |
| `0005-regex-match-groups.patch` | `scintilla/include/BoostRegexSearch.h`, `boostregex/BoostRegExSearch.cxx` | Edita guarda dónde cae cada grupo de la última coincidencia (`g_lastMatchGroups`, por hilo) para el probador de expresiones regulares (fase 34). No cambia cómo se busca. |
| `0006-pugixml-charconv-macos.patch` | `pugixml/pugiconfig.hpp` | `PUGIXML_CHARCONV_FLOAT` (que activa N++) no se activa en macOS: la libc++ de Apple no tiene `from_chars`/`to_chars` para decimales antes de macOS 13.3, y Edita pide macOS 12. pugixml usa entonces `strtod`/`snprintf`; Edita no lee ni escribe decimales con pugixml. |
| `0007-lexuser-eol-una-vez.patch` | `lexilla/lexers/LexUser.cxx` | **Corrige un error** (cambia el comportamiento): en los lenguajes definidos por el usuario, una secuencia que cierra justo antes del fin de línea (el `*/` de un comentario de bloque de una línea, como la documentación `/** … */`) dejaba el cursor en el fin de línea con `dontMove`, y la vuelta siguiente lo procesaba otra vez. `lineCurrent` avanzaba dos, y el plegado de todo lo que seguía quedaba una línea más abajo. Ahora cada fin de línea se procesa una sola vez (fase 65). Notepad++ tiene el mismo error. |
| `0008-regex-compiled-cache.patch` | `boostregex/BoostRegExSearch.cxx` | Guarda las 8 últimas expresiones compiladas, no solo la última. La Lista de funciones de N++ alterna dos (la de la función y la de su nombre) en cada coincidencia, y cada vez se recompilaba la otra: un archivo de C++ con 200 000 funciones tardaba minutos en analizarse (fase 77). No cambia qué se encuentra. |

Además, sin tocar el fuente, en `third_party/CMakeLists.txt`:

| Ajuste de build | Objetivo | Motivo |
|---|---|---|
| `-include stdint.h` | `uchardet_npp` | Usa `uintptr_t` sin incluir `<stdint.h>` (en MSVC llega de forma indirecta). |

Versiones importadas: ver `UPSTREAM.txt`.

Para actualizar: actualizar `referencia/npp`, ejecutar `tools/import-third-party.sh`
y, si algún parche ya no aplica, rehacerlo o borrarlo si el cambio llegó aguas arriba.
