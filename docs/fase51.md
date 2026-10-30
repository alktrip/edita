# Fase 51 — Capa de compatibilidad v2, parte 1: Unicode y Win32 sin ventanas

Fecha: 2026-10-30

## Por qué

La [auditoría de la lista oficial de plugins de N++](https://github.com/alktrip/edita-plugins/tree/main/audit) mostró que, con la capa de compatibilidad de la fase 15, solo compilaban 3 de los 136 plugins nativos (C/C++). El problema estaba en la capa, no en los plugins: 118 de los 133 que fallaban tropezaban con `wchar_t`.

Desde la versión 8.0, la API de plugins de N++ es Unicode, y la capa se había diseñado con `TCHAR = char`. Además, casi todos los plugins usan una parte de KERNEL32, SHLWAPI y el CRT de MSVC que la capa no tenía.

Esta fase es la primera de la capa v2. La siguiente se ocupa de las ventanas: las clases de la plantilla de N++ (`Window`, `StaticDialog`, `DockingDlgInterface`) sobre Qt y los diálogos de los `.rc`.

## Qué se construyó

| Parte | Qué hace |
|---|---|
| **Modo Unicode**: `edita_npp_plugin(… UNICODE)` | Define `UNICODE` y `_UNICODE`. `TCHAR`, `TEXT()`, `LPTSTR` y los nombres genéricos (`MessageBox`, `CreateFile`, `lstrcpy`, `_tcs*`…) pasan a `wchar_t` y a las funciones `W`, como en Windows. El modo ANSI (`TCHAR = char`, UTF-8) se mantiene: lo usan MIME Tools y XML Tools |
| **Frontera con Edita** (`compat/npp_compat.cpp`) | En modo Unicode convierte a UTF-8 `getName()` y la tabla de órdenes (`FuncItem` con `wchar_t[64]`). Edita da los `cmdID` en su copia, y se pasan a la tabla del plugin en la primera notificación o el primer `SendMessage`. Convierte también, en los dos sentidos, los mensajes `NPPM_*` con cadenas: rutas (tamaño en `wchar_t`, `FALSE` si no cabe, como en N++), `NPPM_GETFULLPATHFROMBUFFERID` y los demás que devuelven una longitud, abrir, guardar, sesiones, barra de estado, `NPPM_CREATELEXER` y `NPPM_MSGTOPLUGIN` (con los nombres de módulo de N++, `"Otro.dll"`, y de Edita, `"Otro.so"`). `DllMain` recibe el `HINSTANCE` del plugin |
| **Win32 sin ventanas** (`compat/npp_win32.cpp`, nuevo) | Cada función en sus variantes `A` (UTF-8) y `W`. **Archivos**: `CreateFile`, `ReadFile`, `WriteFile`, punteros, tamaños, `FindFirstFile` (sin distinguir mayúsculas; `*.*` es todo), atributos, copiar, mover, borrar y directorios, con `\` en las rutas. **`.ini`**: `Get/WritePrivateProfileString`, `Int`, `Section` y `SectionNames`, con la semántica de Windows (claves sin distinguir mayúsculas, comillas, listas con doble nulo, borrar con `NULL`); en UTF-8, y en UTF-16 si el archivo ya lo estaba. **Texto**: `MultiByteToWideChar`/`WideCharToMultiByte`, `lstr*`, `CharUpper`/`CharLower`. **`printf` de MSVC**: en las funciones anchas, `%s` es `wchar_t` y `%S` es `char` (en C es al revés); también `%I64d`. **CRT de MSVC**: `*_s` (también con arreglos), `_itow`, `_wtoi`, `_wfopen`… **shlwapi** (`Path*`, `Str*`), **strsafe.h**, **módulos** (`GetModuleFileName` con `dladdr`), **entorno**, **tiempo**, **memoria global**, **portapapeles** de Qt (`CF_UNICODETEXT`, `CF_TEXT` y formatos registrados), `GetKeyState`, `MessageBox` y `ShellExecute` |
| Cabeceras de Windows | `commdlg.h`, `strsafe.h`, `winuser.h`, `winbase.h`, `winnls.h`, `wingdi.h`, `uxtheme.h`, `dwmapi.h`, `sdkddkver.h`, `versionhelpers.h`, `io.h`, `direct.h`, `process.h`, `crtdbg.h`… Si el plugin usa una API que no está, falla con «no declarado» y no con «falta la cabecera» |
| **`#include` de Windows** (`fixWindowsIncludes`) | Antes de compilar una receta se corrigen los `#include` que solo funcionan en Windows: `<Windows.h>`, `"..\PluginInterface.h"`, `"Resource.H"`. Un `#include` se reescribe solo si en Linux no encuentra su archivo: con la ruta que existe sin distinguir mayúsculas, con el nombre de la cabecera del SDK o con el único archivo del código que se llama así. Los bytes del archivo se conservan. Se desactiva con `"fix-includes": false` |

## Divergencias

| Tema | Windows | Edita | Motivo |
|---|---|---|---|
| Tamaño de `wchar_t` | 2 bytes (UTF-16) | 4 bytes (UTF-32) | Es el `wchar_t` de Linux y macOS. Con `-fshort-wchar` no funcionarían ni la biblioteca de C ni Qt. El código que usa las funciones de Win32 funciona igual; el que da por hecho UTF-16 necesita un parche en su receta |
| Páginas de código | La del sistema (1252…) | `CP_ACP` y `CP_OEMCP` son UTF-8; 1252 y 28591, Latin-1; las demás fallan con `ERROR_INVALID_PARAMETER` | En Linux el sistema es UTF-8 |
| Separador de rutas | `\` | Las funciones de la capa aceptan `\` y `/`, y las de `shlwapi` añaden `/` | Un plugin que arma una ruta con `L"\\"` funciona igual |
| `.ini` nuevos | ANSI | UTF-8 | — |
| `GetModuleFileName(NULL)` | `notepad++.exe` | El ejecutable de Edita | — |
| Diálogos, ventanas, GDI, COM, registro, hilos de Win32 | — | Todavía no | Siguiente fase: ventanas. COM y el registro no están previstos |
| Paneles acoplables en modo Unicode | `tTbData` | `NPPM_DMMREGASDCKDLG` devuelve 0 | Su `hClient` es una ventana de la plantilla de N++: llega con la siguiente fase |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| La capa en modo Unicode: `TCHAR`, conversiones (con `-1`, consulta de tamaño, búfer corto, UTF-8 inválido, Latin-1), `printf` de MSVC (`%s`/`%S`/`%I64d`, truncado), `.ini` (rutas con `\`, mayúsculas, comillas, listas, borrar, UTF-16 con BOM, comentarios), archivos (crear, leer, mover, copiar, `FindFirstFile`, `_wfopen` con `ccs=`), rutas, módulos, entorno, portapapeles, `strsafe.h`, el CRT y las macros de memoria | ✅ `tst_npp_compat` (9 casos), compilada con los avisos del proyecto con GCC y clang |
| Un plugin moderno de N++ (`FuncItem` con `wchar_t`, `#include` de Windows) se compila desde su receta con `UNICODE`. Edita lo carga con su nombre y sus órdenes en UTF-8. `NPPM_GETFULLCURRENTPATH` + `swprintf` + `PathFindFileName` insertan `[prueba ñ.txt] ¡hola ñandú!`. `NPPM_GETPLUGINSCONFIGDIR` + `PathAppend` + `WritePrivateProfileString` escriben el `.ini`. `NPPM_SETMENUITEMCHECK` con el `cmdID` de su tabla marca la orden. `GetModuleFileName` con el `HINSTANCE` de `DllMain` da `UniDemo.so`. `NPPM_MSGTOPLUGIN` llega con los nombres `.dll` | ✅ `tst_recipes`: `builderCompilesAUnicodePlugin` |
| `fixWindowsIncludes`: mayúsculas, `\`, rutas relativas, cabeceras del SDK; no toca lo que existe ni lo que no encuentra, y conserva los bytes que no son UTF-8 | ✅ `fixWindowsIncludes` |
| El modo ANSI sigue igual: el plugin de prueba de la fase 15, MIME Tools y XML Tools | ✅ `builderCompilesAnUpstreamPlugin`, `realNotepadPlusPlusPlugins` |
| La cabecera compila en C (C11) y en C++, en los dos modos | ✅ comprobado con GCC y clang |

## Auditoría repetida

Se repitió la compilación de prueba de la auditoría, con `UNICODE`, sobre el mismo código de los 136 plugins nativos:

| SDK | Compilan (A) | Solo falla la interfaz (B1) | Necesitan un port (B2) |
|---|---:|---:|---:|
| Fase 49 | 2 | 2 | 103 |
| Fase 51 | **5** | **4** | 98 |

- **Nuevos en la clase A:** selectNLaunch, SimplifyJson y TidyText (GPL v2). Ya estaban Reverse Lines y NppPluginOpenHost.
- **Nuevos en la clase B1:** SelectQuotedText y NppJumpList.
- **Los errores de `wchar_t` casi desaparecieron.** Lo que frena ahora a los de la clase B2 son las ventanas: `GetParent`, `ScreenToClient`, `SetWindowLongPtr`/`GWLP_USERDATA`, recursos (`FindResource`, `DLGTEMPLATE`), GDI y monitores. Así le pasa a 68 de los 98. 15 usan COM y 14 necesitan dependencias que no vienen en su repositorio.

La mejora es pequeña en número, pero era la condición para todo lo demás: sin Unicode, ningún plugin actual pasaba del primer archivo. El detalle está en [edita-plugins/audit](https://github.com/alktrip/edita-plugins/tree/main/audit).

## Siguiente

**Capa v2, parte 2: ventanas.** Las clases de la plantilla de N++ (`Window`, `StaticDialog`, `DockingDlgInterface`) sobre Qt, con los paneles acoplables en el `DockingManager` de Edita (`tTbData` → `EditaDockData`), los mensajes de ventana más repetidos y, después, un intérprete de los diálogos de los `.rc`. De paso, lo barato que salió en la auditoría: `SCI_GETTEXTRANGE` (Scintilla 5 lo retiró; lo usan 18 plugins), `FormatMessage`, `MulDiv`, `MAKELANGID`, `CreateProcess` y `WaitForSingleObject`, y las macros `min`/`max` de `windows.h`.

Pasan las 51 suites y el build no da ningún warning.
