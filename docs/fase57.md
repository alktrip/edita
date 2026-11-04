# Fase 57 — Las pruebas intermitentes

Fecha: 2026-11-04

## Por qué

Desde la [fase 55](fase55.md), la CI de Linux falla si falla una prueba. Dos pruebas fallaban de vez en cuando sin que el código cambiara: `tst_pythonscript` se quedaba colgada hasta el límite de 5 minutos y `tst_terminal` no encontraba lo que esperaba en pantalla. Con fallas al azar, una CI en rojo deja de decir algo. Las dos se reprodujeron y se corrigieron midiendo, no suponiendo.

## Cómo se reprodujeron

| Prueba | Condiciones | Antes | Después |
|---|---|---|---|
| `tst_pythonscript` | 120 instancias a la vez, con la CPU cargada (6 × `yes`) | 8 a 13 de 120 se colgaban, todas en `scriptsFromMenusAndOtherPlugins` (como en la CI) | 0 de 120 |
| `tst_terminal` | En el Ubuntu 22.04 de `build-portable.sh` (allí `/bin/sh` es `dash`, como en la CI; aquí es `bash`), 4 a la vez, con la CPU cargada | 4 de 40 fallaban en `realShell` | 0 de 60 |

`gdb` escondía el cuelgue: cambia los tiempos lo suficiente (ninguno en 56 corridas). Para ver las pilas sin frenar el proceso, se instrumentó la prueba temporalmente con `prctl(PR_SET_PTRACER_ANY)` (para tomar la pila con `eu-stack` desde fuera) y el `faulthandler` de Python (las pilas de los hilos de Python). Esa instrumentación no forma parte del código.

## Las causas

**`tst_pythonscript`: un aviso modal que nadie cerraba.** En las pilas, el hilo de la interfaz estaba en `QDialog::exec()` dentro de `submit()`, y el hilo de los scripts, libre en `queue.get()`. Era el aviso «Another script is currently running», que PythonScript muestra si se lanza un script mientras otro sigue en marcha. El intérprete es el mismo en todas las pruebas, y con la máquina cargada un script tarda en terminar después de hacer lo suyo: la prueba veía el texto que había escrito el script y lanzaba el siguiente mientras el anterior aún estaba en marcha. El plugin hacía lo correcto. Lo que faltaba era que la prueba esperara.

- La ventana de la prueba tiene `idle()` (ningún script en marcha), y se espera con `QTRY_VERIFY(x.idle())` antes de cada envío asíncrono. Faltaba en tres sitios.

**`tst_terminal`: lo que se esperaba ver en pantalla.** Al pegar dos órdenes, la prueba esperaba ver `echo pegado2` en pantalla antes de pulsar Intro. Con `bash` 5, que pide *bracketed paste*, lo pegado espera a Intro y se ve entero. Con `dash`, lo pegado se ejecuta al momento, y el eco de la segunda línea se mezcla en pantalla con la salida de la primera (`echo ppegado1` / `# egado2`), como al escribir por adelantado en cualquier terminal. Las órdenes se ejecutaban bien.

- La prueba acepta las dos cosas: lo pegado a la espera de Intro o su salida ya escrita. Si falla, muestra la pantalla.

## Además: Stop Script, solo dentro del script

Al buscar el cuelgue apareció otro riesgo, que un comentario del plugin ya mencionaba («un KeyboardInterrupt que llega justo fuera del trabajo»). Stop Script interrumpe el script con `PyThreadState_SetAsyncExc`, que lanza un `KeyboardInterrupt` en el primer punto de control del intérprete en ese hilo. Si el script terminaba entre la comprobación y ese momento, la excepción caía ya fuera, en `queue.get()` o en un `with lock:` del hilo de los scripts. En esos sitios, el código de `threading` y `queue` no está preparado para una excepción a destiempo y puede dejar un cerrojo tomado. No fue la causa del cuelgue medido, pero el riesgo es real, así que se cerró:

- `_npp.run_job(job)` (C++) ejecuta cada trabajo del hilo de los scripts. Marca el hilo como «ejecutando un script» y, al volver del trabajo, sin soltar el GIL, quita la marca y la excepción que aún estuviera pendiente (`PyThreadState_SetAsyncExc(hilo, NULL)`).
- Stop Script, con el GIL tomado, solo lanza la excepción si el hilo está marcado. Si el trabajo ya salió de la cola pero no empezó, deja un aviso que `run_job` atiende sin ejecutarlo. Al sacar un trabajo nuevo de la cola, el aviso se borra (`_npp.clear_stop()`), para que un Stop pulsado sin nada en marcha no se lleve el siguiente script.
- La prueba de «Stop Script justo al lanzar» pasa de 30 a 120 repeticiones.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| `tst_pythonscript`: 120 instancias a la vez con la CPU cargada, sin cuelgues | ✅ 0 de 120 (antes, 8 a 13) |
| `tst_terminal` con `dash` (Ubuntu 22.04) y la CPU cargada | ✅ 0 de 60 (antes, 4 de 40) |
| Stop Script: el script se detiene; al lanzarlo y pararlo 120 veces seguidas, ninguno se queda en marcha y el hilo de los scripts sigue vivo | ✅ `scriptsRunInTheirOwnThread` |
| Pasan todas las suites y el build no da avisos (GCC y clang) | ✅ |

## Siguiente

La fase 58: la capa de compatibilidad v2, parte 4 (controles comunes, bucles de mensajes y dibujo básico).
