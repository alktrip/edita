# Fase 84 — Tareas del proyecto con errores enlazados

Fecha: 2026-11-12

## Por qué

Compilar o pasar las pruebas desde el editor, ver los errores y llegar a cada uno con un clic es de lo más usado en un editor de código. En Notepad++ hace falta el plugin NppExec, con sus guiones y filtros escritos a mano.

## Cómo lo hace Notepad++

El menú Ejecutar (`RunDlg`, F5) lanza una orden con variables como `$(FULL_CURRENT_PATH)` y no muestra lo que sale. Edita lo tiene igual desde la fase 48. Para ver la salida y los errores, N++ depende de NppExec: una consola, guiones y «filtros de resaltado» con máscaras como `%FILE%:%LINE%:*` que cada usuario configura.

| N++ (+ NppExec) | Edita |
|---|---|
| Hay que escribir la orden o un guion | Las tareas se detectan solas: Makefile, CMake, package.json (npm/pnpm/yarn/bun), Cargo, Go, pytest, Meson y just |
| Filtros que escribe cada usuario | Se reconocen GCC/Clang, Go, rustc (con su `-->`), tsc, trazas de Python, pytest y `archivo:línea:col: mensaje` |
| Los errores, solo en la consola | En el panel de salida (subrayados, con doble clic) y en Problemas, junto a los del servidor de lenguaje |
| — | Repetir la última tarea (Ctrl+F5) y detenerla con todo lo que lanzó |

## Qué se hizo

- **`core/Tasks`**:
  - `findProjectRoot` sube desde el archivo hasta una carpeta con algún archivo de proyecto o `.git`.
  - `detectTasks` lista las tareas: los objetivos de `make` vía `makeTargets`, sin asignaciones, patrones ni especiales; las recetas de just, sin `set`, `export` ni `alias`; `cmake --build` con la carpeta de compilación que ya exista o, si no hay, primero configurar…
  - `ProblemParser` lee la salida línea a línea. Solo cuenta una posición si el archivo existe, para no tomar `http://x:80:1` ni `make: *** [Makefile:3: all]` por errores.
  - `stripAnsi` quita los colores.
- **`ui/TaskRunner`**:
  - Ejecuta con `/bin/sh -c` en la carpeta del proyecto, con `TERM=dumb` y en un grupo de procesos propio (`setsid`).
  - Detener manda `SIGTERM` al grupo y, a los 2 s, `SIGKILL`. Así se detiene también lo que lanzó `make`.
  - Las líneas reescritas con `\r` (barras de progreso) se quedan en la última versión.
- **`ui/TaskOutputPanel`** (Ejecutar › Salida de la tarea, abajo):
  - La orden, la salida y el resultado: «Terminó en 2,3 s», «Falló con el código 2 a los 1,1 s · 3 errores, 1 aviso».
  - Las líneas con un error salen subrayadas en rojo, y los avisos en ámbar. Cada línea guarda el índice de su error en el bloque de texto, así que el doble clic sigue funcionando cuando el panel recorta la salida (50 000 líneas).
  - Botones Detener, Repetir y Borrar.
- **Menú Ejecutar**:
  - **Ejecutar una tarea…** (Ctrl+Mayús+B) abre la lista con buscador, con la última tarea primero.
  - **Repetir la última tarea** (Ctrl+F5), **Detener la tarea** y **Salida de la tarea**.
- **Problemas**: `updateDiagnostics` junta los diagnósticos del servidor de lenguaje con los de la última tarea, y el contador de la barra de estado los suma. Cada tarea nueva borra los de la anterior.
- **Manual**: Macros y el menú Ejecutar › Tareas del proyecto, en los dos idiomas.

Edita no guarda los documentos antes de ejecutar una tarea, igual que el menú Ejecutar. El manual lo dice.

## Pruebas

`tst_tasks` (12):

- **Detectar las tareas:** la carpeta del proyecto (la más cercana), los objetivos de make, los ocho tipos de proyecto y CMake sin configurar.
- **Leer los errores:** GCC con colores, Go, tsc, rustc, la traza de Python, pytest, lo que no es un archivo y `stripAnsi`.
- **Ejecutar:** líneas, errores y código de salida, y que Detener termine también los procesos hijos.
- **En la ventana:** elegir `make build` en la lista; la salida, el estado, Problemas con dos entradas; el doble clic lleva a `src/util.c` línea 2, y Repetir no duplica los errores. Sin proyecto, se avisa.
