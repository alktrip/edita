# Fase 93 — Pruebas con sanitizers en la CI

Fecha: 2026-11-13

## Por qué

En la fase 90, un error de memoria de una prueba (escribir en una lista ya destruida) pasó en local y falló en la CI por casualidad, con un `std::bad_alloc` que no decía dónde estaba el problema. Los errores de memoria y el comportamiento indefinido de C++ pueden pasar desapercibidos en una compilación normal y aparecer después, en el equipo de un usuario. AddressSanitizer y UndefinedBehaviorSanitizer los detectan en el momento, con la línea exacta.

## Qué se hizo

- **`EDITA_SANITIZE`** (CMake): `-DEDITA_SANITIZE=address,undefined` compila todo con los sanitizers: Edita, `third_party`, los plugins del árbol y las pruebas. Usa `-fno-sanitize-recover=all`, así que un error detiene la prueba.
- **Trabajo `sanitizers` en la CI de Linux** (x86_64, Ubuntu 22.04, GCC 12, Qt 6.8.3):
  - compila con ASan y UBSan y corre todas las pruebas;
  - los paquetes no se publican si falla.

  Usa `detect_leaks=0`, porque Qt y fontconfig dejan memoria reservada a propósito al salir y LeakSanitizer daría falsas alarmas.
- **`tst_perf` y `tst_crashreport` no se ejecutan con sanitizers:**
  - `tst_perf` mide tiempos, y con ASan todo va de 2 a 3 veces más lento.
  - `tst_crashreport` provoca fallos a propósito, que los sanitizers atrapan antes que el manejador de Edita.

## Lo que encontraron

La primera corrida completa (75 suites) dio dos errores reales y una prueba con una carrera:

1. **`SnippetController::eventFilter`** comparaba el objeto del evento con un `QPointer<EditorView>`. Mientras un editor se destruye, Qt sigue enviando eventos de foco (`~QWidget` llama a `clearFocus`), y el `QPointer` todavía no se vació. Compararlo convertía a `EditorView*` un objeto que ya solo era `QWidget`: comportamiento indefinido. Afectaba a 16 suites. Ahora se mira primero el tipo de evento (solo interesa `KeyPress`).
2. **uchardet** (la detección de codificación, código de Notepad++ en `third_party`) guarda en `nsSMState`, un enum de 0 a 2, cualquier estado de su tabla (7, 9, 12…). En C++ es comportamiento indefinido para un enum sin tipo subyacente fijo. El parche `0009-uchardet-smstate-unsigned.patch` le da `: unsigned int`. El código que se genera no cambia. Notepad++ tiene el mismo problema.
3. **`tst_lsp`** pedía las referencias justo después de abrir `lib.py` con F12, sin esperar a que el servidor tuviera el documento. Con la carga de los sanitizers el pedido llegaba antes y Edita, con razón, no hacía nada. La prueba ahora espera.

Después de esto pasan las 73 suites con sanitizers. La CI de arm64 encontró además una prueba de la fase 92 (`tst_catalog`) que daba por hecho que el equipo es x86_64.

## Cómo usarlo

```sh
cmake -S . -B build-asan -G Ninja -DEDITA_SANITIZE=address,undefined
cmake --build build-asan
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
    ctest --test-dir build-asan -j4
```
