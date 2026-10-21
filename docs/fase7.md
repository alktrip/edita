# Fase 7 — Preferencias: resultado

Fecha: 2026-10-20 · Entorno: el mismo que en las fases anteriores

## Qué se construyó

| Módulo | Equivale en N++ a | Qué hace |
|---|---|---|
| `core/Preferences` | `NppGUI`, `ScintillaViewParams`, `feedGUIParameters`/`createXmlTreeFromGUIParams` | Todas las preferencias en `config.xml` con los **nombres y valores de N++**: `StatusBar`, `TabBar`, `ScintillaPrimaryView`, `Caret`, `NewDocDefaultSettings`, `openSaveDir`, `<History>`, `TabSetting`, `MaintainIndent` (también la errata `MaitainIndent` y los valores antiguos `yes`/`no`), `SmartHighLight`, `Searching`, `Backup`, `RememberLastSession`, `Auto-detection` y `DetectEncoding`. Lo que Edita no usa se conserva al guardar |
| `ui/PreferencesDialog` | `PreferenceDlg` | Diez páginas: General, Edición, Márgenes, Documento nuevo, Archivos recientes, Tabulación y sangría, Resaltado, Búsqueda, Copias de seguridad y Varios. Botones Aceptar, Cancelar y Aplicar. Los controles que dependen de otros se desactivan |
| `MainWindow::applyPreferences` | Los `case` de `PreferenceDlg` que aplican cada opción | Aplica las preferencias a las dos vistas, a los estilos, a los documentos nuevos, a la detección de codificación, a los menús y al temporizador de copias |
| `EditorView::applyPreferences` | `ScintillaEditView` (`wrap`, `showWSAndTab`, `showEOL`, `setCaretWidth`, `setVirtualSpace`…) | Ajuste de línea y su sangría (`SCI_SETWRAPINDENTMODE`), símbolo de ajuste, espacios y fin de línea visibles, cursor de bloque o de 1-3 px y su parpadeo, multiselección, espacio virtual, desplazar más allá de la última línea, suavizado y margen de números |
| `EditorView::updateLineNumberWidth` | `ScintillaEditView::updateLineNumberWidth` | Margen de números con ancho dinámico, según la última línea visible y como mínimo 3 dígitos. Sin ancho dinámico, según el total de líneas y como mínimo 4 dígitos |
| `SyntaxStyler` (`StylerOptions`) | `performGlobalStyles`, `setCRLineHiliteMode`, `_markersArray`, `setEdge…` | Línea actual con fondo, con marco de 1-6 px o sin resaltar. Plegado simple, con flechas, círculos o cajas, o sin margen. Líneas de margen: una columna como línea o como fondo, o varias columnas. Indicadores de búsqueda (29, 31 y 28) con los colores de `stylers.xml`. Tabulación global (los lenguajes con `tabSettings` en `langs.xml` mantienen la suya) |
| `ui/SmartHighlighter` | `SmartHighlighter::highlightView` | Al seleccionar una palabra se marcan sus apariciones en las líneas visibles. Por defecto solo palabras completas y sin distinguir mayúsculas. También puede usar las opciones del diálogo Buscar y marcar en la otra vista. Se actualiza al desplazarse |
| `ui/Indentation` | `Notepad_plus::maintainIndentation`, `isConditionExprLine`, `setLineIndent` | Sangría automática en tres modos. Ninguna. Básica, que copia la sangría de la línea anterior no vacía. Avanzada, para los lenguajes con llaves y Python: un nivel más tras `{` o tras `if/for/while (…)` sin llave (solo para la línea siguiente), `{|}` + Intro baja la llave de cierre, `}` se alinea con su `{`, y en Python un nivel más tras `:` que sea un operador |
| Archivos recientes | `LastRecentFileList`, `<History>` | Se añaden al cerrarlos y salen de la lista al abrirlos. Se muestran abajo en el menú Archivo o en un submenú, con la ruta completa, solo el nombre o acortada. Incluyen "Abrir todos" y "Vaciar la lista", y Ctrl+Mayús+T restaura el último cerrado. Al salir recordando la sesión, los archivos abiertos no se añaden. Si un archivo ya no existe, se ofrece quitarlo |
| Menús | `IDM_VIEW_WRAP`, `IDM_VIEW_TAB_SPACE`, `IDM_VIEW_EOL`, `IDM_VIEW_ALL_CHARACTERS`, `IDM_VIEW_INDENT_GUIDE`, `IDM_VIEW_WRAP_SYMBOL` | **Ver > Mostrar símbolo** y el ajuste de línea son preferencias: se aplican a las dos vistas y se guardan. Hay un menú nuevo, **Configuración > Preferencias…** |
| Edición | `IDM_EDIT_COPY`/`CUT` con `lineCopyCutWithoutSelection` | Copiar o cortar sin selección copia o corta la línea entera (`SCI_COPYALLOWLINE`) |
| General | `TabBar` | Se pueden ocultar la barra de estado y la de pestañas, quitar el botón de cerrar de las pestañas y cerrar con doble clic. Con "salir al cerrar el último documento", la ventana se cierra cuando ya no queda ninguno |
| Documento nuevo y directorio | `NewDocDefaultSettings`, `openSaveDir` | Fin de línea, codificación (UTF-8, con o sin BOM, UTF-16 o un juego de caracteres) y lenguaje de los documentos nuevos. El diálogo Abrir/Guardar empieza en el directorio del documento actual, en el último usado (`lastUsedDirPath` se guarda) o en uno fijo |
| Búsqueda | `Searching` | Rellenar el campo Buscar con la selección, confirmar "Reemplazar todo en todos los documentos abiertos" y fuente monoespaciada en el diálogo |

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Fin de línea de los documentos nuevos por defecto | Windows (CR LF) | Unix (LF) | Sistema destino |
| Opciones sin equivalente o sin función todavía (barra de herramientas, modo oscuro, idioma, asociación de archivos, multi-instancia, nube, padding, `rightClickKeepsSelection`, historial de cambios, marcadores…) | Sí | No están en el diálogo (las de `config.xml` se conservan) | Fuera de alcance o dependen de funciones que no existen aún. El autocompletado llega en la Fase 8 y la impresión en la 9 |
| Tabulación por lenguaje | Se edita en la página Lenguaje | Se respeta la de `langs.xml`, pero no se edita en el diálogo | Habría que reescribir `langs.xml` |
| Juego de caracteres de 8 bits por defecto | El ANSI de Windows | Opción propia `<GUIConfig name="EditaFallbackCodepage" codepage="1252"/>` | Linux no tiene codepage ANSI |
| Aplicar | Cada control se aplica al instante | Botones Aplicar y Aceptar | Más sencillo de deshacer con Cancelar |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Todas las preferencias se guardan y se leen igual en `config.xml`, con los nombres y valores de N++ | ✅ `everyPreferenceRoundTripsThroughConfigXml` |
| Un `config.xml` escrito por Notepad++ se carga y, al guardar, se conserva lo que Edita no usa | ✅ `readsAConfigWrittenByNotepadPlusPlus` |
| El diálogo muestra y devuelve cada preferencia, con controles dependientes y "Aplicar" | ✅ `dialogShowsAndReturnsEveryPreference` |
| Las opciones de vista se aplican a las dos vistas (también a un clon), persisten y la siguiente ventana arranca con ellas | ✅ `viewPreferencesApplyToBothViews`, `viewMenuSymbolsArePreferences` |
| Barra de estado, pestañas, documentos nuevos, directorio de Abrir/Guardar, copiar sin selección y salir al quedar vacío | ✅ `generalPreferencesAffectTheWindow`, `newDocumentSettings`, `openSaveDirectoryFollowsThePreference`, `copyWithoutSelectionCopiesTheLine`, `quitOnEmptyClosesTheWindow` |
| Archivos recientes: orden, máximo, abrir desde el menú, Ctrl+Mayús+T, submenú, persistencia y archivo desaparecido | ✅ `recentFilesList`, `missingRecentFileOffersToRemoveIt` |
| Resaltado inteligente con mayúsculas y palabra completa, que se borra sin selección y se puede desactivar | ✅ `smartHighlightMarksOtherOccurrences` |
| Sangría automática básica, C-like (llaves y condiciones sin llave) y Python | ✅ `basicAutoIndentKeepsThePreviousIndentation`, `cLikeAutoIndentFollowsBraces`, `pythonAutoIndentAfterColon` |
| Confirmar antes de reemplazar en todos los documentos abiertos | ✅ `replaceAllInOpenDocumentsAsksFirst` |

Tests: `tst_preferences` 20 (contando `initTestCase` y `cleanupTestCase`). `tst_mainwindow` se ha adaptado al nuevo menú y al ancho dinámico del margen, que es el valor por defecto de N++. Pasan las 14 suites y el build no da ningún warning. El diálogo se ha revisado con capturas.
