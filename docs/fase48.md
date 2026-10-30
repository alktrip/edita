# Fase 48 — Menú Ejecutar: resultado

Fecha: 2026-10-30

## Qué se construyó

| Parte | Equivale en N++ a | Qué hace |
|---|---|---|
| Ejecutar › **Ejecutar…** (F5) | `RunDlg` | Diálogo con la orden (lista editable con las ya ejecutadas en la sesión), «…» para elegir el programa (con comillas si la ruta tiene espacios), «$( )» con el menú de variables, y **Ejecutar**, **Guardar…** y **Cancelar**. Si la orden se lanza, queda la primera de la lista y el diálogo se cierra, como en N++ |
| Variables (`core/UserCommands`) | `whichVar`, `expandNppEnvironmentStrs` | `FULL_CURRENT_PATH`, `CURRENT_DIRECTORY`, `FILE_NAME`, `NAME_PART`, `EXT_PART`, `CURRENT_WORD` (la selección o la palabra), `CURRENT_LINE` y `CURRENT_COLUMN` (desde 1), `CURRENT_LINESTR`, `NPP_DIRECTORY` y `NPP_FULL_FILE_PATH` (el AppImage, si se ejecuta desde uno). Lo que no es una variable se deja igual |
| Ejecutar | `Command::run` (ShellExecute) | Una orden que es solo una dirección se abre con el programa del escritorio (N++ la manda al navegador). Lo demás se lanza con `/bin/sh -c`, separado de Edita, en la carpeta del documento (o la personal), sin las variables del AppImage. Si no se puede lanzar, un aviso |
| Órdenes guardadas | `UserCommand`, `<UserDefinedCommands>` | **Guardar…** pide nombre y atajo (el mismo diálogo que las macros, ahora con título propio) y la orden aparece en el menú Ejecutar con su atajo. Se guardan en `shortcuts.xml` con el formato de N++, así que las de N++ se pueden copiar; el resto del archivo se conserva |
| De serie | `shortcuts.xml` de N++ | En el primer arranque, las tres de N++ con sus teclas: «Get PHP help» (Alt+F1), «Wikipedia Search» (Alt+F3) y «Open selected file path in new instance» (Alt+F6), esta adaptada: `"edita" --multiInst --nosession "$(CURRENT_WORD)"` |
| Ejecutar › **Modificar el atajo o eliminar una orden…** | `IDM_SETTING_SHORTCUT_MAPPER_RUN` | La Asignación de atajos en la pestaña nueva **Órdenes de Ejecutar** (entre Macros y Órdenes de plugins, como en N++), con Modificar, Borrar y Eliminar |
| Menú | `MENUINDEX_RUN` | Después de Macro. Las órdenes del menú están en la paleta de órdenes |

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Cómo se lanza | `ShellExecute`: primer elemento = programa, resto = argumentos; `%VAR%` de Windows | `/bin/sh -c` con la línea entera | Lo natural en Linux y macOS: comillas, tuberías, `&&`, `$VAR` |
| Carpeta de trabajo | La de Notepad++ | La del documento | Más útil para compilar o ejecutar el archivo |
| La tercera orden de serie | `-nosession -multiInst` con un guion | `--multiInst --nosession` | Las opciones de Edita llevan dos guiones |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Todas las variables; lo desconocido (`$(date)`) se queda; reconocer direcciones | ✅ `tst_run`: `expandsVariables` |
| `<UserDefinedCommands>` se lee y escribe sin tocar las macros; las tres de serie | ✅ `shortcutsXmlKeepsTheRest` |
| En el primer arranque están las de serie; variables del documento y del cursor; `/bin/sh` en la carpeta del documento; una dirección va al navegador | ✅ `runsCommands` |
| Guardar con nombre y atajo; otra ventana las lee; la pestaña de la Asignación de atajos; cambiar el atajo; eliminar | ✅ `savedCommandsAndTheirShortcuts` |
| El diálogo ejecuta, se cierra y guarda el historial | ✅ `runDialog` |
| El manual describe el menú en los dos idiomas | ✅ `tst_manual` |

Pasan las 48 suites y el build no da ningún warning.
