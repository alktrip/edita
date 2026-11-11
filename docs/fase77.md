# Fase 77 — Rendimiento con archivos grandes

Fecha: 2026-11-11

## Por qué

Con la paridad con Notepad++ casi completa, toca medir. Una prueba provisional midió lo que se hace a diario: arrancar, abrir archivos (uno grande, muchos a la vez), cambiar de pestaña, escribir, desplazarse, buscar en una carpeta, y buscar, reemplazar y ordenar en un archivo grande.

Casi todo iba bien, pero un archivo de C++ de 14 MB con 200 000 funciones (cada una con un comentario) era inusable:

| Medida (Release, GCC 16, sin pantalla) | Antes | Después |
|---|---|---|
| Abrir el archivo | **156 s** | 2.6 s |
| Volver a su pestaña | 2.5 s | 0.2 s |
| Volver a su pestaña con la Lista de funciones abierta | 2.7 s | 0.5 s |
| 50 teclas (la primera marca el documento con cambios) | **10 s** (13 s con la Lista de funciones) | 0.6 s |

Lo demás ya estaba bien y no cambió:

| Medida | Tiempo |
|---|---|
| Crear la ventana | 66 ms (170 ms la primera vez) |
| Abrir un `.txt` de 100 MB en la ventana | 1.4 s |
| Abrir 100 archivos | 1.5 s |
| Recorrer 100 pestañas | 1.8 s |
| Buscar en 2000 archivos (400 000 coincidencias) | 0.5 s |
| Con 200 000 coincidencias en el archivo grande: buscar todo / contar / reemplazar todo (normal o regex) / deshacerlo | 0.33 / 0.05 / 1.2 / 0.8 s |
| Ordenar 200 000 líneas / quitar las duplicadas | 0.18 / 0.16 s |

## Qué pasaba y qué se cambió

1. **La ruta de navegación analizaba el documento entero en cada cambio de pestaña** con los analizadores de la Lista de funciones de N++ (`functionList/*.xml`), y eso tenía dos costos que crecen con el cuadrado del archivo:
   - **La expresión regular se recompilaba en cada coincidencia.** El analizador alterna dos expresiones (la de la función y la de su nombre) y `BoostRegexSearch` solo guardaba compilada la última. El parche nuevo `0008-regex-compiled-cache.patch` guarda las ocho últimas ([PATCHES.md](../third_party/PATCHES.md)). No cambia qué se encuentra.
   - **Cada función se buscaba en todas las zonas de comentario** (`isInZones`, como en N++). Las zonas van en orden y no se pisan, así que ahora es una búsqueda binaria.
2. **Los símbolos se guardan por documento** (`MainWindow::functionsOf`). La clave son el lenguaje, la longitud y el historial de deshacer (`SCI_GETUNDOACTIONS`, `SCI_GETUNDOCURRENT`): cambian también si otra orden edita el documento en segundo plano (reemplazar en todos los documentos) o si se deshace. Los usan la ruta de navegación, la Lista de funciones y los símbolos de la paleta de órdenes. La caché guarda lo que encuentra el analizador, no la lista final: los bloques salen del plegado, que la primera vez puede no estar hecho todavía.
3. **La Lista de funciones no reanaliza el mismo documento.** La ventana se lo vuelve a dar en cada `updatePanels` (por ejemplo, cuando la primera tecla marca el documento con cambios), y eran 3 s en esa tecla. Los cambios del texto los sigue recogiendo su temporizador.
4. **Al escribir en un documento de más de 2 MB, la ruta de navegación no reanaliza en cada pausa** (bloquearía la ventana segundos). Se rehace al volver a la pestaña. Es el mismo límite que ya tenía para calcular los bloques con el plegado.
5. **El autocompletado de palabras** buscaba en cada tecla, en todo el documento, una expresión regular (`AutoCompletion::getWordArray` de N++): 200 ms por tecla con 14 MB. Ahora recorre el texto (`SCI_GETCHARACTERPOINTER`) con las mismas reglas:
   - la palabra empieza donde antes no hay un carácter de palabra (los de `SCI_GETWORDCHARS`, y los que no son ASCII cuentan como letras, como en Scintilla);
   - sigue hasta el primer separador (espacio, `. , ; : " ( ) { } = < > ' + ! ? [ ]`).

   Los duplicados se quitan con un conjunto y no recorriendo la lista. Con un archivo de API que ignora mayúsculas se sigue usando la expresión. De paso se corrige un error que N++ también tiene: con la expresión (`\<`), una palabra que empieza con ñ o con acento (`ñandú`) nunca se ofrecía.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| 200 000 funciones: abrir en menos de 10 s, volver a la pestaña en menos de 1 s, y con la Lista de funciones en menos de 3 s; 50 teclas en menos de 3 s (builds optimizados) | ✅ `tst_perf`: `sourceFileWithManyFunctions` (2.6 s, 0.2 s, 0.5 s, 0.6 s) |
| La ruta de navegación sigue los cambios: al escribir, con el documento en segundo plano (reemplazar en todos) y al deshacer | ✅ `tst_navigation`: `breadcrumbsSymbolsFollowChanges` |
| Las palabras del autocompletado: inicio de palabra, separadores, la que se escribe, mayúsculas, principio y fin del documento, y las que empiezan con ñ | ✅ `tst_editing`: `wordArrayEdges` |
| La Lista de funciones, la paleta y lo que había del autocompletado, igual | ✅ `tst_panels`, `tst_commandpalette`, `tst_editing` |
| Pasan las 66 suites, el build no da avisos (GCC 16 y clang) y compila con el GCC 12 y Qt 6.8.3 de la CI | ✅ |

## Cómo se midió

Con una prueba provisional (no se sube) que crea la ventana real sin pantalla y mide con `QElapsedTimer`. Sin `perf` ni `valgrind` en el equipo, y con `ptrace_scope=1` (gdb no se engancha a un proceso que ya corre), las pilas salieron de QtTest: al recibir SIGTERM imprime la de cada hilo. Bastan tres o cuatro muestras en el momento lento.
