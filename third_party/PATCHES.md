# Parches locales sobre el código de terceros

`third_party/` se regenera con `tools/import-third-party.sh` a partir de
`referencia/npp`, que ya trae los parches de Notepad++ sobre Scintilla 5.6.6 y
Lexilla 5.5.3, y su copia de uchardet (`PowerEditor/src/uchardet`). Encima de eso, Edita aplica estos parches mínimos, todos de
portabilidad a Linux/GCC y **sin cambios de comportamiento**:

| Parche | Archivo | Motivo |
|---|---|---|
| `0001-lexuser-sin-windows-h.patch` | `lexilla/lexers/LexUser.cxx` | Incluía `<windows.h>` solo para obtener `strlen`/`strcmp` de forma indirecta y usaba `_itoa` de MSVC. Se sustituye por `<cstdio>`/`<cstring>` y un `_itoa` equivalente en base 10. |
| `0002-sci-position-stdint.patch` | `scintilla/include/Sci_Position.h` | N++ cambió `Sci_PositionCR` a `intptr_t` (para soportar más de 2 GB) sin incluir `<stdint.h>`; en MSVC llegaba de forma indirecta. |
| `0003-regex-error-thread-local.patch` | `scintilla/include/BoostRegexSearch.h`, `boostregex/BoostRegExSearch.cxx` | `g_exceptionMessage` (mensaje de la última regex inválida) era una variable global que se vacía en cada búsqueda. Edita busca en archivos en otro hilo mientras se puede buscar en el editor: pasa a `thread_local`. Ese es el único cambio de comportamiento: cada hilo ve su propio mensaje. |

| `0004-scintilla-h-c-compatible.patch` | `scintilla/include/Scintilla.h` | N++ añadió al final una estructura de C++ (`SearchResultMarkings`, con `std::vector`) para su panel de resultados, lo que impide incluir la cabecera desde C. Se rodea de `#ifdef __cplusplus` para que los plugins en C (como el de ejemplo) puedan usar `Scintilla.h`. Para C++ no cambia nada. |

Además, sin tocar el fuente, en `third_party/CMakeLists.txt`:

| Ajuste de build | Objetivo | Motivo |
|---|---|---|
| `-include stdint.h` | `uchardet_npp` | Usa `uintptr_t` sin incluir `<stdint.h>` (en MSVC llega de forma indirecta). |

Versiones importadas: ver `UPSTREAM.txt`.

Para actualizar: actualizar `referencia/npp`, ejecutar `tools/import-third-party.sh`
y, si algún parche ya no aplica, rehacerlo o borrarlo si el cambio llegó aguas arriba.
