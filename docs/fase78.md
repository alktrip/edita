# Fase 78 — Al día con Notepad++ 8.9.9

Fecha: 2026-11-11

## Por qué

La referencia (`referencia/npp`) era Notepad++ 8.9.8.1, del 19 de octubre. Desde entonces N++ sacó la 8.9.9 y llevaba 77 cambios más. Antes de la 1.0 conviene estar al día, sobre todo con Scintilla: cambiarla después de la versión estable sería más delicado.

La mayoría de esos cambios son traducciones o arreglos propios de Windows: escalado por pantalla (PerMonitorV2), permisos de administrador (UAC), mensajes `WM_COPYDATA` entre procesos, fugas de GDI y el diálogo Ventanas. Esta fase trae lo que aplica a Edita.

## Qué se hizo

### El diálogo de búsqueda: dos casillas nuevas

| N++ 8.9.9 | Edita |
|---|---|
| `IDC_HIGHLIGHT_MODE` («Highlight all matches of the current search in the active document while typing»): una casilla sin texto a la derecha de «Find what», en las pestañas Find y Replace | Igual (`highlightMatchesCheck`). Mientras el diálogo está a la vista, resalta lo que coincide con las opciones del diálogo, sin «en la selección»: `highlightFindMatches` en `ui/SmartHighlighter`. Como en N++, solo busca de la primera línea visible hasta una pantalla más abajo, y vuelve a buscar al desplazarse (`SC_UPDATE_V_SCROLL`). Al escribir en el campo o al editar el documento, espera 250 ms (`updateMatchHighlightingLater`). Lo demás (las opciones, la pestaña, mostrar el diálogo) actualiza al momento. Al cerrar el diálogo o pasar a otra pestaña, se quita. Sigue al documento activo y se limpia en las dos vistas, porque los indicadores son del documento. En un archivo grande respeta la regla del resaltado inteligente (`allowSmartHilite`) |
| `FINDDLG_HIGHLIGHT_INDIC` 30, con el color del estilo 28, que pasa a llamarse «Find matches highlight all» | `kIndicatorFindDialog` = 30, redondeado, translúcido y bajo el texto, con el color del 28. El nombre nuevo llega con `stylers.model.xml`. Edita busca los estilos de los indicadores por número, así que los temas y el `stylers.xml` del usuario con el nombre viejo siguen sirviendo |
| `findDlgHighlightMatches` en `<FindHistory>` | Igual, en `config.xml`; viene apagada |
| `IDC_REPLACEANDSTOP` («After replacing, don't move to the following occurrence»): la opción `replaceStopsWithoutFindingNext` pasa de Preferencias › Búsqueda a una casilla junto a Replace | Edita no la tenía. Ahora es una casilla junto a **Reemplazar** (`replaceAndStopCheck`), guardada como en N++ en `<GUIConfig name="Searching" replaceStopsWithoutFindingNext=…>`. `replaceNext(…, stopAfterReplace)` deja el cursor al final de lo reemplazado y muestra «Replace: 1 occurrence was replaced.» |

Las dos casillas van sin texto, como en N++. Tienen ayuda emergente y nombre accesible, para que un lector de pantalla las anuncie.

### Scintilla 5.6.9, Lexilla 5.5.5 y boost 1.92

`tools/import-third-party.sh` las trae desde la referencia nueva. Los ocho parches de `third_party/patches` siguen aplicando sin cambios.

Al importar se vio que el script nunca había aplicado los tres parches con rutas desde la raíz del repositorio (`a/third_party/…`: 0005, 0006 y 0008). Funcionaban porque `third_party/` ya los traía aplicados en git. Ahora el script reconoce los dos formatos.

Entre lo nuevo de Lexilla, Markdown reconoce los metadatos del principio del archivo (*front matter*) en YAML, TOML o JSON (`SCE_MARKDOWN_FRONT*`). Los temas de N++ todavía no les dan color, así que se ven con el estilo normal. También hay arreglos en los lexers de HTML, Perl, C++, SQL y YAML.

### Lo demás

| N++ 8.9.9 | Edita |
|---|---|
| Bash: la Lista de funciones perdía las funciones después de una línea con solo `#`; INI y PowerShell aceptan `+` en los nombres (`functionList/*.xml`) | Las tres definiciones nuevas. `import-third-party.sh` ahora copia también `functionList/`, que antes se actualizaba a mano |
| R: `lexer.r.escape.sequence=1` para colorear los escapes en las cadenas | Igual, en la tabla de lexers genéricos |
| `setWhitespaceSize`: los puntos de los espacios crecen con el zoom | `EditorView::updateWhitespaceSize`, al crear la vista y con cada `SCN_ZOOM`. La base son 2 píxeles lógicos, porque Qt ya escala con la pantalla. Antes Edita usaba el tamaño 1 de Scintilla |
| `newEmptyDocument`: si el lenguaje de los documentos nuevos es un lenguaje de usuario que ya no existe, queda en texto normal | Edita no elige lenguajes de usuario para documentos nuevos. Un `lang="15"` (L_USER) de un `config.xml` de N++ daba un «lenguaje de usuario» sin nombre; ahora da texto normal |
| Historial del portapapeles: no partir un par sustituto de UTF-16 al recortar la etiqueta | El panel de Edita tenía el mismo error: `displayText` y la ayuda emergente usaban `left()`. Ahora no dejan la mitad de un emoji |

### Lo que no aplica

- **Carpeta como espacio de trabajo con un enlace simbólico** (`f031fcb`). En N++ la carga se detenía en el primer enlace. El panel de Edita lee cada carpeta solo al abrirla, así que un enlace no corta nada.
- **Pegar en espacio virtual con `SC_MULTIPASTE_EACH`** (`7981945`). Es un arreglo de Scintilla que llega con la 5.6.9.
- Lo de Windows mencionado arriba, y los cambios de las listas internas, del selector de documentos y del diálogo Ventanas, que son propios de sus controles Win32.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| La casilla de resaltar: apagada por omisión; resalta al escribir, con las opciones del diálogo, solo en lo visible y lo que entra al desplazarse; sigue a las ediciones; no en Marcar; se quita al cerrar el diálogo o al apagarla; sigue al documento activo; se guarda | ✅ `tst_findui`: `dialogHighlightsMatchesWhileOpen`, `dialogHighlightIsSaved` |
| Reemplazar sin avanzar: solo en la pestaña Reemplazar, el cursor al final de lo reemplazado, el mensaje, y se guarda | ✅ `tst_findui`: `replaceAndStopDoesNotMoveToNext`; `tst_preferences` (`replaceStopsWithoutFindingNext="yes"`) |
| `findDlgHighlightMatches` en `config.xml` | ✅ `tst_session` |
| Las listas de funciones de Bash, INI y PowerShell, con los casos que añadió N++ | ✅ `tst_panels`: `functionParsersOfNotepadPlusPlus899` |
| Los escapes de R | ✅ `tst_syntax`: `otherLanguagesColourise(r escape)` |
| El tamaño de los puntos con el zoom (2, 4, 6 y 2 con zoom 0, 10, 20 y −10) | ✅ `tst_preferences`: `viewPreferencesApplyToBothViews` |
| Documento nuevo con L_USER sin nombre | ✅ `tst_preferences`: `newDocumentSettings` |
| El historial del portapapeles no parte un emoji | ✅ `tst_onselection` |
| Pasan las 66 suites, el build no da avisos (GCC 16 y clang) y compila con el GCC 12 y Qt 6.8.3 de la CI | ✅ |
