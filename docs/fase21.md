# Fase 21 — Editor de columnas: resultado

Fecha: 2026-10-21

Objetivo: el **Editor de columnas** de N++ (`ColumnEditorDlg`, Editar > Editor de columnas…, **Alt+C**): insertar un texto o una serie de números en varias líneas a la vez.

## Qué se construyó

| Parte | Qué hace |
|---|---|
| `core/ColumnEditor` | Los números de N++: valor inicial, incremento y repetición (cada número en varias líneas seguidas; 0 cuenta como 1, como en N++). Base decimal, hexadecimal (minúsculas o mayúsculas), octal o binaria. Relleno con ceros o espacios hasta el ancho del número más largo (`getNbDigits`, `variedFormatNumber2String`). Los ceros van detrás del signo en los negativos. También lee los campos en la base elegida |
| `columnInsertCount` / `columnInsert` (`EditCommands`) | El `IDOK` de `ColumnEditorDlg`. **Con selección rectangular o varias selecciones**: cada una se sustituye por su texto (`columnReplace`); el espacio virtual se rellena con espacios, y al terminar quedan seleccionados los textos puestos (`setMultiSelections`). **Sin ellas**: desde la línea del cursor hasta la última, en la columna del cursor, rellenando con espacios las líneas más cortas. Todo en una sola acción de deshacer |
| `ui/ColumnEditorDialog` | El diálogo de N++: **Texto que insertar** o **Número que insertar**, con número inicial, incremento, repetir, relleno y formato (Dec/Hex con a-f o A-F/Oct/Bin). Un número que no vale en la base elegida marca el campo en rojo y no inserta nada. Recuerda lo último en `config.xml` |
| Menú | Editar > **Editor de columnas…**, Alt+C (`IDM_EDIT_COLUMNMODE`, también en el Shortcut Mapper). Para que Alt+C no choque con la letra del menú, Codificación pasa a «E&ncoding», como en N++, y en español a «C&odificación» |

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Dónde se guarda | `<GUIConfig name="ColumnEditor" choice="…"><text content=…/><number initial=… increment=… repeat=… formatChoice=… leadingChoice=…/></GUIConfig>` | Lo mismo como atributos de `<GUIConfig name="ColumnEditor">` | El `config.xml` de Edita guarda atributos por `GUIConfig` |
| Ventana | No modal | Modal | Más simple; se inserta al aceptar |
| Hex en mayúsculas | `formatChoice` con un bit aparte | `formatChoice="4"` | Lo mismo, en un solo valor |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Números como N++: incremento, repetición (y 0 como 1), relleno con ceros o espacios al ancho del más largo, hex (minúsculas y mayúsculas), octal, binario, negativos con relleno; lectura en cada base (y rechazo de lo que no vale) | ✅ `tst_columneditor`: `numbersLikeNotepadPlusPlus` |
| Sin selección: en la columna del cursor desde su línea hasta el final, con espacios en las líneas cortas; un solo deshacer; desde una línea intermedia | ✅ `insertsAtTheCaretColumnDownToTheEnd` |
| Selección rectangular: cada línea con su texto, que queda seleccionado; con espacio virtual pasado el final de una línea corta, se rellena | ✅ `replacesRectangularSelections` |
| El diálogo inserta números en hex mayúsculas, recuerda las opciones en `config.xml`, no inserta si un número no vale en la base, y también inserta texto | ✅ `dialogRemembersItsSettings` |
| Editar > Editor de columnas con Alt+C, y ningún menú de la barra usa la C como letra | ✅ `menuHasItWithAltC` |

Tests: `tst_columneditor` tiene 7 casos (contando `initTestCase` y `cleanupTestCase`). Pasan las 29 suites y el build no da ningún warning.
