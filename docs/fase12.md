# Fase 12 — Órdenes de edición: resultado

Fecha: 2026-10-20 · Entorno: el mismo que en las fases anteriores

Objetivo: añadir las órdenes de edición de Notepad++ que más se usan y que Edita no tenía, aprovechando su código siempre que fuera posible. Varias partes de N++ no dependen de Win32 y se han portado casi línea a línea: `Sorters.h`, `xmlMatchedTagsHighlighter.cpp`, `doBlockComment`, `changeCase` y `wsTabConvert`.

## Qué se construyó

| Módulo | Equivale en N++ a | Qué hace |
|---|---|---|
| `core/LineOperations` | `MISC/Common/Sorters.h`, `SortLocale`, `vecRemoveDuplicates`, `ScintillaEditView::changeCase`, el bucle de `Notepad_plus::wsTabConvert` | Funciones puras, sin Scintilla, que se prueban aparte. **Ordenar** con los nueve ordenadores de N++: alfabético, sin distinguir mayúsculas, según el idioma (`QCollator`), enteros dentro del texto (comparación de `IntegerSorter` portada tal cual, con negativos, ceros a la izquierda y números de cualquier longitud), decimales con coma o con punto (las líneas vacías al principio), longitud, invertir y aleatorio. También **quitar duplicadas** (cualquiera o solo las consecutivas) y **quitar vacías**. **Mayúsculas**: tipo Título y tipo oración, cada una normal o respetando las mayúsculas que ya haya, más invertir y aleatorio, con las excepciones de N++ ("don't", la "i" inglesa). **Tabulaciones y espacios**, con el ajuste del cursor |
| `ui/EditCommands` | `Notepad_plus::doBlockComment`, `doStreamComment`, `undoStreamComment`, `doTrim`, `eol2ws`, `removeEmptyLine`, `bookmark*`, `*MarkedLines`, `changedHistoryGoTo`, `clearChangesHistory`, `goToNextIndicator`/`goToPreviousIndicator`, `FindReplaceDlg::markAll`; `ScintillaEditView::sortLines`, `getSelectionLinesRange`, `insertNewLine*CurrentLine`, `convertSelectedTextTo`, `foldAll`, `foldLevel`, `foldIndentationBasedLevel` y `fold` | Las órdenes sobre una vista de Scintilla, con la misma lógica de selección que N++. Si hay líneas seleccionadas se aplican a ellas y, si no, a todo el documento. Una sola línea seleccionada no se ordena. Con una selección rectangular se ordena por el texto de esas columnas, y con varias selecciones cada una se convierte por separado. Cada orden se deshace de una vez |
| `ui/XmlTagMatcher` | `XmlMatchedTagsHighlighter` | Port directo: busca la etiqueta pareja sin expresiones regulares y salta los `<` y `>` que estén dentro de cadenas, comentarios o CDATA. Cuenta las etiquetas anidadas del mismo nombre, reconoce las que se cierran a sí mismas, marca los atributos y la guía de sangría, y en PHP, ASP y JSP no actúa dentro del código. XML y XHTML distinguen mayúsculas; HTML no |
| Márgenes | `ScintillaEditView::init`, `performGlobalStyles`, `showMargin`, `marginClick` | Mismo orden que N++: números, marcadores (16 px), historial de cambios (9 px) y plegado. El de plegado pasa del 2 al 3. Los colores salen de `stylers.xml`: "Bookmark margin" y "Change History margin/modified/revert modified/revert origin/saved". Un clic en el margen de marcadores pone o quita uno |
| Historial de cambios | `isChangeHistoryEnabled` + `SCI_SETCHANGEHISTORY` en `activateBuffer` | Margen, indicadores en el texto o las dos cosas. Se configura al mostrar el documento, porque Scintilla solo empieza a registrar si aún no hay nada que deshacer |
| Indicadores | `SCE_UNIVERSAL_TAGMATCH` (27), `TAGATTR` (26), `FOUND_STYLE_EXT1`…`EXT5` (25…21) | Redondeados, translúcidos y bajo el texto, con el `bgColor` de "Tags match highlighting", "Tags attribute" y "Mark Style 1…5" |
| Menús | `Notepad_plus.rc` | **Editar > Líneas**: duplicar, quitar duplicadas y consecutivas, dividir (Ctrl+I), unir, subir, bajar, quitar vacías (también con espacios), línea vacía encima o debajo (Ctrl+Alt+Intro, Ctrl+Alt+Mayús+Intro), invertir, aleatorio y los 16 órdenes. **Editar > Convertir**: 8 tipos de mayúsculas, con los atajos de N++. **Editar > Comentar**: Ctrl+Q, Ctrl+K, Ctrl+Mayús+K, Ctrl+Mayús+Q y quitar el de bloque. **Editar > Espacios**: quitar al final, al principio o en los dos lados, fin de línea a espacio, quitar sobrantes y fines de línea, y tabulaciones ↔ espacios. **Buscar**: ir a la llave pareja (Ctrl+B) o seleccionar entre llaves (Ctrl+Alt+B), estilos de token 1…5 (todas las apariciones, solo esta, quitar, siguiente y anterior), **Marcadores** (Ctrl+F2, F2, Mayús+F2, quitar todos, cortar, copiar, pegar sobre, eliminar las marcadas o las no marcadas, invertir) e **Historial de cambios** (siguiente o anterior con Ctrl+Alt+↓/↑, borrar). **Ver**: Zoom (Ctrl+más, Ctrl+menos, Ctrl+/), plegar o desplegar todo (Alt+0, Alt+Mayús+0), el bloque actual (Ctrl+Alt+F, Ctrl+Alt+Mayús+F) y cada nivel (Alt+1…8, Alt+Mayús+1…8) |
| Macros y plugins | `IDM_*` de `menuCmdID.h` | Las más de 100 órdenes nuevas llevan su `IDM_*` de N++. Así se graban en las macros como orden de menú, las macros de `shortcuts.xml` de N++ funcionan y los plugins las ejecutan con `NPPM_MENUCOMMAND` |
| Sesión | `<Mark line>` de `session.xml` | Los marcadores se guardan y se restauran. Antes se leían, pero se perdían |
| Preferencias | `ScintillaPrimaryView bookMarkMargin`/`isChangeHistoryEnabled`, `TagsMatchHighLight` (`TagAttrHighLight`), `MarkAll` (`matchCase`, `wholeWordOnly`) | Con los nombres de N++ en `config.xml` y en el diálogo, en las páginas Márgenes y Resaltado. `isChangeHistoryEnabled="yes"`, el formato antiguo, se lee como "margen" |

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Salir | Alt+F4 (Ctrl+Q es comentar) | Igual: Ctrl+Q deja de ser Salir | Fidelidad a los atajos de N++ |
| Fines de línea al ordenar o quitar líneas | Parte por el fin de línea del documento; con fines mezclados el resultado es irregular | Lee las líneas una a una y las une con el fin de línea del documento | Más robusto. Solo cambia algo en archivos con fines de línea mezclados |
| Orden por columnas con caracteres no ASCII | Columna en bytes aplicada a `wstring` | El texto exacto de cada selección rectangular | Correcto con UTF-8 |
| Ordenar según el idioma | `CompareStringEx` con opciones (dígitos como números, ignorar diacríticos…) | `QCollator` del idioma del sistema, sin opciones | No hay diálogo de opciones |
| Cambiar el historial de cambios | Se aplica en la siguiente sesión | Se aplica al mostrar cada documento: desactivarlo borra el historial y activarlo solo afecta a los documentos sin cambios | Scintilla no puede empezar el historial de un documento con cambios |
| Icono del marcador | Imagen RGBA azul | Forma `SC_MARK_BOOKMARK` de Scintilla, en azul | No hace falta añadir imágenes |
| Copiar el texto con un estilo al portapapeles (`IDM_SEARCH_STYLE*TOCLIP`), marcar en el diálogo Buscar (pestaña "Marcar"), ocultar líneas, editor de columnas, sincronizar el zoom de las dos vistas y guardarlo | Sí | No | Pendientes para una fase futura |
| Plegado restaurado desde la sesión (`<Fold>`) | Sí | No; se ignora | El plegado es de cada vista y hay que colorear el documento antes |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Los nueve ordenadores dan el orden de N++: enteros con signo, ceros a la izquierda y números enormes; decimales con las vacías aparte y error en la línea que no es número; longitud en caracteres; aleatorio reproducible con la misma semilla | ✅ `tst_lineops`: `lexicographicSorts`, `integerSortLikeNotepadPlusPlus`, `decimalSorts`, `lengthReverseAndRandom` |
| Quitar duplicadas y vacías; los 8 tipos de mayúsculas, con sus excepciones; tabulaciones ↔ espacios sin mover nada en pantalla y con el cursor ajustado | ✅ `duplicatesAndEmptyLines`, `caseConversions`, `tabsAndSpaces` |
| Ordenar desde el menú: todo, la selección (y conservarla), una sola línea no, CRLF conservado, aviso de la línea que impide el orden numérico, se deshace de una vez | ✅ `tst_editcommands`: `sortLinesFromTheMenu` |
| Orden por las columnas de una selección rectangular | ✅ `sortByRectangularSelection` |
| Quitar duplicadas, consecutivas, vacías y vacías con espacios; insertar líneas vacías en los extremos; unir | ✅ `lineOperations` |
| Mayúsculas con una selección (que se conserva), sin selección no hace nada, y con varias selecciones | ✅ `caseConversion` |
| Comentarios en C++: alternar, comentar dos veces, descomentar (con y sin espacio), de bloque con y sin selección, y descomentar el de bloque | ✅ `lineCommentsInCpp` |
| Python, `.bat` (sin distinguir mayúsculas), HTML con `<!-- -->` en cada línea, texto normal y solo lectura | ✅ `commentsInOtherLanguages` |
| Lenguaje del usuario: casillas 00/03/04 de "Comments" | ✅ `commentsInUserLanguage` |
| Espacios: principio, final, los dos solo en la selección, fin de línea a espacio, quitar sobrantes y tabulaciones ↔ espacios | ✅ `blankOperations` |
| Marcadores: poner y quitar, navegar dando la vuelta, clic en el margen, copiar, invertir, eliminar las no marcadas, pegar sobre ellas, cortar, eliminar, quitar todos y ocultar el margen | ✅ `bookmarks` |
| Los marcadores se guardan en `session.xml` y vuelven al restaurar | ✅ `bookmarksAreKeptInTheSession` |
| Historial de cambios: marcas en las líneas editadas, siguiente y anterior con vuelta, "guardado" al guardar, borrar (y vaciar el deshacer), desactivar | ✅ `changeHistory` |
| Etiqueta pareja en XML desde la apertura, desde el cierre y en una que se cierra a sí misma; atributos con comillas dobles y simples; HTML sin distinguir mayúsculas; preferencia desactivada; otros lenguajes | ✅ `xmlTagsAreMatched` |
| Estilos de token: todas las apariciones (palabra completa, sin distinguir mayúsculas), solo una, siguiente y anterior, quitar uno o todos | ✅ `tokenStyles` |
| Llave pareja: ir y seleccionar | ✅ `matchingBraces` |
| Plegar y desplegar todo, por nivel y el bloque actual, también en Python (niveles por sangría) | ✅ `folding` |
| Zoom: ampliar, reducir y restablecer | ✅ `zoom` |
| Las órdenes nuevas se graban en una macro como `IDM_*` de N++, se reproducen y los plugins las ejecutan con `NPPM_MENUCOMMAND` | ✅ `newCommandsWorkInMacrosAndPlugins` |
| Las preferencias nuevas se leen y se escriben en `config.xml` y pasan por el diálogo | ✅ `tst_preferences` (ampliado) |

Además se revisó en una captura un XML con dos marcadores, la marca naranja del historial en la línea editada y la etiqueta `<item>` con su cierre y sus atributos resaltados.

Tests: `tst_lineops` 9 y `tst_editcommands` 19 (contando `initTestCase` y `cleanupTestCase`). `tst_syntax` y `tst_preferences` se han adaptado al nuevo índice del margen de plegado. Pasan las 20 suites y el build no da ningún warning.
