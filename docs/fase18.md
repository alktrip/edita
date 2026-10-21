# Fase 18 — Repositorio y una sola instancia: resultado

Fecha: 2026-10-21

## Repositorio

`editor/` es ahora un repositorio git (rama `main`). El primer commit guarda el estado de las fases 0-17, y cada fase siguiente termina con su commit. `.gitignore` deja fuera las carpetas de compilación y los `__pycache__` de Python Script.

## Una sola instancia

Como N++ (`winmain.cpp`): si Edita ya está abierto, abrir un archivo (desde el gestor de archivos, la terminal o el menú del escritorio) lo abre en esa ventana, en lugar de lanzar otra con otra sesión.

| Parte | Qué hace |
|---|---|
| `ui/SingleInstance` | `InstanceServer` escucha en un socket local en `$XDG_RUNTIME_DIR`, uno por usuario y carpeta de configuración (con otra `XDG_CONFIG_HOME` es otra «Edita»). La segunda instancia le manda una petición —las rutas completas de los archivos, y la línea y columna de `-n`/`-c`— y termina. Es lo que N++ hace con `WM_COPYDATA` (`COPYDATA_PARAMS` y `COPYDATA_FILENAMESW`). La primera responde «ok»; si nadie responde, la segunda abre su propia ventana. Si quedó el socket de una instancia que terminó mal, se reemplaza |
| `MainWindow::openFromOtherInstance` | Abre los archivos (preguntando si se crean los que no existen, como al abrir desde la línea de comandos), va a la línea y trae la ventana al frente (`NPPM_INTERNAL_RESTOREFROMMINIMIZED`) |
| Línea de comandos | `--multiInst`, como el `-multiInst` de N++: abre otra ventana aunque ya haya una. Con varias instancias, solo la primera guarda y restaura la sesión, como en N++ |
| Preferencias > General > **Instancias** | «Abrir los archivos en la ventana que ya está abierta» (por defecto) o «Abrir siempre una ventana nueva»: `<GUIConfig name="multiInst" setting="0|2"/>`, los valores de N++ |

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Comunicación | `FindWindow` + `WM_COPYDATA` | Socket local (`QLocalSocket`) con una línea JSON | Lo que hay en Linux |
| «Abrir la sesión en una instancia nueva» (`setting="1"`) | Con `-openSession` | Se trata como una sola instancia | Edita no abre archivos de sesión desde la línea de comandos |
| Traer al frente | `SetForegroundWindow` | `raise()` + `activateWindow()` | En Wayland el compositor decide: puede marcar la ventana como «pide atención» en lugar de darle el foco |

Detalle: Qt 6.12 deja que un segundo servidor escuche en un socket que ya está en uso (sustituye el archivo). Por eso, antes de escuchar, Edita comprueba si alguien responde en ese socket.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| La petición (archivos con acentos y espacios, línea y columna) va y vuelve en JSON, una por línea; un socket por configuración, en `$XDG_RUNTIME_DIR` | ✅ `tst_singleinstance`: `requestTravelsAsJson` |
| El servidor recibe peticiones; una segunda instancia no puede escuchar en el mismo socket; al cerrarse deja de responder; sin nadie escuchando, enviar falla | ✅ `serverReceivesRequests` (con la comprobación anterior a escuchar; sin ella, falla) |
| Un socket abandonado por una instancia que terminó mal se reemplaza | ✅ `staleSocketIsReplaced` |
| La ventana abre lo que manda la otra instancia y va a la línea pedida | ✅ `windowOpensWhatTheOtherInstanceSends` |
| La preferencia se guarda como en N++ (`multiInst setting`) | ✅ `preferencePersists` |
| Con dos procesos de Edita de verdad: el segundo le pasa el archivo al primero y termina con código 0; con `--multiInst` se queda abierto y no le quita el socket al primero | ✅ `secondProcessHandsOverAndExits` |

Tests: `tst_singleinstance` tiene 8 casos (contando `initTestCase` y `cleanupTestCase`). Pasan las 26 suites y el build no da ningún warning.
