# Fase 0 — Esqueleto y toolchain: resultado

Fecha: 2026-10-19 · Entorno: Arch/CachyOS, GCC 16.2.1, CMake 4.4.4, Ninja 1.13.2, Qt 6.12.0, Wayland (KDE)

## Decisiones tomadas

| Tema | Decisión |
|---|---|
| Componente de edición | `ScintillaEditBase` de Scintilla 5.6.6 + Lexilla 5.5.3 + `boostregex`, copiados de `referencia/npp` (ya con los parches de N++), **no** QScintilla. Ver sección 0 del análisis. |
| Nombre | `Edita`; binario `edita`; configuración en `~/.config/edita/`. Se puede cambiar con `-DEDITA_APP_ID=…`. |
| `session.xml` | Compatible con el esquema de Notepad++ (se hará en la Fase 5). |
| Tests | **QtTest** en lugar de Catch2: viene con `qt6-base` y no añade dependencias. Los tests de UI corren con `QT_QPA_PLATFORM=offscreen`. |
| pugixml | Se usará la copia de `PowerEditor/src/pugixml` (Fase 3); no hace falta el paquete del sistema. |
| Licencia | GPL v3 o posterior (`COPYING` sacado del texto SPDX del sistema), `NOTICE` con todos los componentes. |

## Hallazgos

1. **N++ compila Scintilla y Lexilla solo con MSVC**, y dos archivos dependían de eso. Se corrigieron con parches mínimos sin cambio de comportamiento (`third_party/PATCHES.md`):
   - `Sci_Position.h` usaba `intptr_t` sin `<stdint.h>`.
   - `LexUser.cxx` usaba `<windows.h>` y `_itoa`.
2. **La regex de N++ depende de la locale en Linux.** `basic_regex<wchar_t>` usa `w32_regex_traits` en Windows (clasificación Unicode del sistema) y `cpp_regex_traits` en Linux (la `std::locale` global, que por defecto es "C"). Sin corregirlo, `\b`, `\w`, `[[:alpha:]]` y "palabra completa" no reconocían `ñ`.
   - Solución: `edita::initGlobalLocale()` (`src/core/Locale.cpp`) fija solo la faceta `ctype` a una locale UTF-8 (la del usuario, o `C.UTF-8` como respaldo). Se llama en `main()`.
   - Está cubierto por `tst_engine::boostRegexUtf8`, que también pasa con `LC_ALL=C`.
3. `ScintillaEditBase` funciona en Qt 6.12 con `qt6-5compat` (usa `QTextCodec` solo para codepages que no son UTF-8). No ha hecho falta tocarlo.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Build desde cero en Arch sin warnings en `src/` (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Werror`) | ✅ 201 pasos, 0 warnings, unos 70 s con 8 hilos |
| `./build/edita` abre una ventana con un editor funcional | ✅ Comprobado en Wayland con captura: menús, margen de números de línea, cursor |
| `CreateLexer` de `cpp`, `searchResult`, `user` y `objc` no nulo | ✅ `tst_engine::lexillaHasNppLexers` |
| Regex `(?<=a)b` encuentra la coincidencia, lo que demuestra que Boost está enganchado (RESearch no admite lookbehind) | ✅ `tst_engine::boostRegexLookbehind`, además de `\R`, `(?R)`, `DOTMATCHESNL` y UTF-8 |
| `SCI_GETCODEPAGE == SC_CP_UTF8` | ✅ `tst_engine::utf8IsDefaultCodePage` |
| Escribir, deshacer y teclado | ✅ `tst_mainwindow::typingGoesToEditor` (escritura + Ctrl+Z con el keymap de Scintilla) |
| Entrada no ASCII / IME en Wayland y X11 | ⏳ **Pendiente de prueba manual** (QtTest solo simula ASCII). Probar con `./build/edita`: escribir `ñ á ü €`, teclas muertas (´ + e) y, si se usa, ibus/fcitx. En X11: `QT_QPA_PLATFORM=xcb ./build/edita`. |

Resultado de los tests (incluyen `initTestCase`/`cleanupTestCase`): `tst_engine` 20/20 y `tst_mainwindow` 5/5.

## Cómo se verifica

```sh
cmake -S editor -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
LC_ALL=C QT_QPA_PLATFORM=offscreen ./build/tests/tst_engine   # respaldo de locale
./build/edita
```
