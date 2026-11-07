# Fase 74 — Las órdenes cortas que faltaban de Notepad++

Fecha: 2026-11-07

## Por qué

Es el sexto bloque de [lo que tiene Notepad++ y Edita no](paridad-npp.md): las órdenes pequeñas de Editar, Buscar, Ver, Macro y Ayuda que quedaban, juntas en una fase.

## Cómo lo hace N++ y qué hace Edita

| Orden de N++ | En Edita |
|---|---|
| **Edit › Indent › Increase/Decrease Line Indent** (`IDM_EDIT_INS_TAB`, `RMV_TAB`) | **Editar › Sangría**: con una línea (o la selección en una), un ancho de tabulación más o menos (`setLineIndent`, sin bajar de cero); con varias líneas o selecciones, `SCI_TAB`/`SCI_BACKTAB`, como N++. Se graban en las macros |
| **Edit › Begin/End Select** y **in Column Mode** (`IDM_EDIT_BEGINENDSELECT[_COLUMNMODE]`) | `EditorView::beginOrEndSelect`: la primera vez marca el inicio; la segunda selecciona hasta el cursor (en columna, rectangular). Mientras espera, la orden se ve marcada y la otra no se puede usar |
| **Edit › Read-Only for All Documents** / **Clear** (`IDM_EDIT_SETREADONLYFORALLDOCS`, `CLEAR…`) | Solo lectura (la de Edita, no el atributo del archivo) para todos los abiertos, o quitarla |
| **Search › Find characters in range…** (`IDM_SEARCH_FINDCHARINRANGE`) | `ui/FindCharsDialog` (sin bloquear la ventana): no ASCII, ASCII o un rango propio; hacia arriba o abajo; dar la vuelta. N++ mira bytes (0-255); Edita, códigos Unicode (`findCharInRange` en `core/OnSelection`, con los pares sustitutos: un emoji es un carácter) |
| **View › Show Symbol › Show Non-Printing Characters** y **Show Control Characters & Unicode EOL** (`IDM_VIEW_NPC`, `NPC_CCUNIEOL`) | Con las tablas de N++ (`g_nonPrintingChars`, `g_ccUniEolChars`, en `ui/NonPrintingChars.inc`): los 49 no imprimibles (NBSP, ZWSP, LRM, RLM…) con su abreviatura en un recuadro (`SCI_SETREPRESENTATION`); los de control (C0 sin tabulador ni saltos, DEL, C1, NEL, LS, PS) con su abreviatura o, ocultos, como un espacio de ancho cero. `npcShow` y `ccShow` en `ScintillaPrimaryView`, como N++. «Mostrar todos los caracteres» incluye los dos |
| **Toggle macro record**, **Function Parameters Previous/Next Hint** (órdenes de N++ solo para atajo: 50005, 50010, 50011) | Acciones sin menú en la ventana, que salen en la Asignación de atajos en la categoría «Solo atajo» (que ahora lista también las acciones sin menú) |
| **? › Command Line Arguments…** (`IDM_CMDLINEARGUMENTS`) | **Ayuda › Argumentos de la línea de órdenes…**: el texto de `--help`. Las opciones pasan de `main.cpp` a `core/CommandLine` para no repetirlas |
| **? › Notepad++ Home** (`IDM_HOMESWEETHOME`) | **Ayuda › Página de Edita**: el repositorio. El manual en línea de N++ (`IDM_ONLINEDOCUMENT`) es el manual de Edita (F1, que lleva ese id); Edita no tiene foro (el repositorio no tiene Discusiones) |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Sangría de una línea (sin bajar de cero) y de varias | ✅ `tst_shortcommands`: `indent` |
| Marcar inicio y fin: la selección, la marca de la orden, la otra sin poder usarse, en columna | ✅ `beginEndSelect` |
| Solo lectura para todos y quitarla | ✅ `readOnlyForAll` |
| Los no imprimibles y los de control (sus representaciones), y «Mostrar todos» con los cuatro | ✅ `nonPrintingCharacters` |
| Caracteres en un rango: no ASCII, un emoji, arriba, dar la vuelta, rango propio; en el editor, la selección en bytes | ✅ `charsInRange` |
| Las órdenes solo de atajo: en la Asignación de atajos, grabar y parar la macro | ✅ `shortcutOnly` |
| Ayuda: los argumentos y la página | ✅ `help` |
| El manual cita cada orden en los dos idiomas | ✅ `tst_manual` |
| Pasan las 64 suites, el build no da avisos (GCC 16 y clang), y compila con el GCC 12 y Qt 6.8.3 de la CI | ✅ |
