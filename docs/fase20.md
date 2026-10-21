# Fase 20 — Asignación de atajos (Shortcut Mapper): resultado

Fecha: 2026-10-21

Objetivo: cambiar los atajos desde la interfaz, como con el **Shortcut Mapper** de N++ (Configuración > Shortcut Mapper…), y guardarlos en `shortcuts.xml` con su formato.

## Qué se construyó

| Parte | Qué hace |
|---|---|
| `ui/ShortcutMapperDialog` | Las pestañas de N++: **Menú principal** (todas las órdenes, con su menú como categoría), **Macros**, **Órdenes de plugins** (con el plugin) y **Órdenes de Scintilla**. Tiene filtro por nombre y los botones **Modificar** (o doble clic: se pulsa la combinación nueva), **Borrar**, **Eliminar** (macros) y **Cerrar**. Los cambios se aplican y se guardan al momento, como en N++. Si la tecla ya la usa otra orden, avisa y ofrece quitársela; N++ solo marca el conflicto |
| `ui/Shortcuts` | Atajos como los guarda N++: Ctrl/Alt/Mayús + la tecla virtual de Windows. Se convierten a y desde `QKeySequence`; se agregaron los signos de puntuación (`VK_OEM_*`) y el teclado numérico. Lectura y escritura de `shortcuts.xml`: `<InternalCommands><Shortcut id="IDM_…">`, `<PluginCommands><PluginCommand moduleName internalID>`, `<ScintillaKeys><ScintKey ScintID menuCmdID>` con `<NextKey>`, y, para las órdenes que N++ no tiene, `<EditaCommands><Shortcut name="…">` (N++ la ignora). Conserva `<Macros>` y `<UserDefinedCommands>`. Solo se guarda lo que difiere del atajo de serie |
| Órdenes del menú | Cada orden se identifica por su `IDM_*` de N++. Se agregaron los que faltaban (Abrir, Buscar, Preferencias, ver espacios…) en una tabla aparte de la de macros, para no grabarlas en macros. Las familias dinámicas (lenguajes, juegos de caracteres, codificaciones) y las propias de Edita (Exportar a PDF…) se guardan por su nombre |
| Órdenes de Scintilla | 88 órdenes con sus teclas de serie, generadas de `scintKeyDefs` de N++. No incluye las que N++ redirige a una orden del menú, que están en esa pestaña. Se aplican con `SCI_CLEARCMDKEY`/`SCI_ASSIGNCMDKEY` en las dos vistas |
| Al arrancar | Edita recuerda el atajo de serie de cada orden (también los que ponen los plugins) y aplica los de `shortcuts.xml` |
| Menú | Configuración > **Asignación de atajos…** (`IDM_SETTING_SHORTCUT_MAPPER`), con icono de teclado |

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Conflictos | Se marcan en la lista | Al asignar, se ofrece quitar la tecla a la otra orden | Evita dejar atajos que no funcionan |
| Varias teclas por orden de Scintilla | Se editan una a una | Modificar pone una; Borrar las quita. Las que vienen de `shortcuts.xml` se conservan | Lo habitual es una |
| «Run commands» (menú Ejecutar) | Pestaña | No | Edita no tiene menú Ejecutar |
| Órdenes sin `IDM_*` | — | `<EditaCommands>` por nombre | N++ no las tiene |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Atajos con Ctrl/Alt/Mayús + tecla virtual ↔ `QKeySequence` (letras, F4, signos), teclas sin equivalente rechazadas; conversión a teclas de Scintilla (`SCK_*`, `SCMOD_*`) | ✅ `tst_shortcuts`: `keyCombosLikeNotepadPlusPlus` |
| `shortcuts.xml` con el formato de N++ (`Shortcut id`, `PluginCommand`, `ScintKey` con `NextKey`, más `EditaCommands`), ida y vuelta sin pérdidas y sin tocar las macros | ✅ `shortcutsXmlKeepsTheFormatOfNotepadPlusPlus` |
| Las cuatro clases de órdenes con sus atajos y los de serie: Nuevo (Ctrl+N, menú Archivo), la macro de N++, «Insert date and time» del plugin de ejemplo (`EditaDemo.so:1`, Ctrl+Alt+D) y `SCI_LINECUT` (Ctrl+L) | ✅ `entriesListEveryKindOfCommand` |
| Un atajo del menú se guarda con su `IDM_*` y se aplica al volver a abrir; uno propio de Edita, por su nombre; volver al de serie lo quita del archivo | ✅ `menuShortcutsAreSavedAndApplied` |
| Atajos de plugins (`PluginCommand`) y de macros, guardados y aplicados al volver a abrir | ✅ `pluginAndMacroShortcuts` |
| Una orden de Scintilla con otra tecla: con pulsaciones reales, la tecla vieja ya no hace nada y la nueva sí; volver a la de serie la restaura | ✅ `scintillaKeysAreReassigned` |
| El diálogo lista todo; un conflicto pregunta, y si no se acepta no cambia nada; si se acepta, se le quita a la otra orden; Borrar; el filtro por nombre | ✅ `mapperDialogResolvesConflicts` |

Tests: `tst_shortcuts` tiene 9 casos (contando `initTestCase` y `cleanupTestCase`). Pasan las 28 suites y el build no da ningún warning.
