# Fase 9 — Imprimir y exportar a PDF: resultado

Fecha: 2026-10-20 · Entorno: el mismo que en las fases anteriores (más Qt PrintSupport, incluido en `qt6-base`)

## Qué se construyó

| Módulo | Equivale en N++ a | Qué hace |
|---|---|---|
| `ui/Printer` (`printDocument`) | `Printer::doPrint` | Pagina el documento de la vista con `SCI_FORMATRANGEFULL` sobre cualquier `QPagedPaintDevice`: impresora (`QPrinter`) o PDF (`QPdfWriter`). En la versión Qt de Scintilla, `hdc` es el `QPaintDevice` y se reutiliza el `QPainter` activo, así que texto, colores y números de línea los dibuja el propio Scintilla, como en Windows |
| | márgenes | Márgenes del usuario en mm (`margeLeft/Top/Right/Bottom`); la impresora impone los suyos como mínimo. Margen interior de línea y media (`printMarge`) |
| | cabecera y pie | Izquierda, centro y derecha con fuente, tamaño y estilo propios (Arial a 9 pt por defecto) y una raya que los separa del texto. Variables: `$(FULL_CURRENT_PATH)`, `$(CURRENT_DIRECTORY)`, `$(FILE_NAME)`, `$(NAME_PART)`, `$(EXT_PART)`, `$(SHORT_DATE)`, `$(LONG_DATE)`, `$(TIME)` (formato del idioma del sistema) y `$(CURRENT_PRINTING_PAGE)` |
| | opciones | Modo de color (`SCI_SETPRINTCOLOURMODE`: como en pantalla, invertido, negro sobre blanco, sin fondo). Números de línea opcionales: se oculta el margen y luego se restaura. Saltos de página en `\f`. Solo la selección o un intervalo de páginas. Las líneas largas se ajustan, como en N++ |
| Menú **Archivo** | `IDM_FILE_PRINT`, `IDM_FILE_PRINTNOW` | **Imprimir…** (Ctrl+P) con el diálogo del sistema; "Selección" solo se ofrece si hay algo seleccionado. **Imprimir ahora** usa la impresora predeterminada sin diálogo. **Exportar a PDF…** propone `<nombre>.pdf` junto al archivo, pregunta si exportar solo la selección cuando la hay, y usa A4 (Carta en sistemas de EE. UU.) a 300 ppp |
| Preferencias | `<GUIConfig name="Print" lineNumber formFeedPageBreak printOption headerLeft … footerFontSize margeLeft …/>` | Página nueva "Impresión" en el diálogo, con los mismos nombres de N++ en `config.xml`. Una fuente que no existe en este sistema se conserva |

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Exportar a PDF | Solo mediante una impresora PDF del sistema o el plugin NppExport | Orden propia con `QPdfWriter` | En Linux no siempre hay impresora PDF; sale gratis con la misma paginación |
| Documento vacío | No imprime nada | Una página con cabecera y pie | Un PDF necesita al menos una página |
| Variables de la cabecera | Todas las de `expandNppEnvironmentStrs` (también `$(CURRENT_WORD)`, `$(NPP_DIRECTORY)`…) | Las de archivo, fecha, hora y página | Las demás no tienen sentido al imprimir |
| Texto de derecha a izquierda (`_isRTL`) | Alineación especial | No | Edita aún no tiene modo RTL |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Variables de cabecera y pie (también en documentos sin archivo) | ✅ `headerVariables` |
| Un documento largo da varias páginas; márgenes mayores dan más; la selección o un intervalo de páginas se respetan | ✅ `pagination` (cuenta los objetos `/Type /Page` del PDF) |
| `\f` empieza página nueva solo con la opción, y cada trozo queda en su página | ✅ `formFeedStartsANewPage` (texto comprobado con `pdftotext`) |
| Cabecera izquierda y derecha y pie con número de página en el PDF; números de línea solo si se piden, y el margen de la vista queda como estaba | ✅ `headerFooterAndLineNumbers` |
| Documento vacío; exportar desde el menú añade `.pdf`, y cancelar no escribe nada | ✅ `emptyDocumentGivesOnePage`, `exportFromTheMenuAsksForAName` |
| `config.xml` con los nombres de N++ | ✅ `printSettingsUseNotepadPlusPlusNames`, y la ida y vuelta en `tst_preferences` |

Además se revisó visualmente: un `.cpp` exportado y rasterizado con `pdftoppm` muestra cabecera con ruta y fecha, números de línea, colores del tema, ajuste de líneas largas y pie con la página.

Tests: `tst_printing` 9 (contando `initTestCase` y `cleanupTestCase`). Los que leen el texto del PDF usan `pdftotext` si está instalado, y si no se saltan esa parte. Pasan las 16 suites y el build no da ningún warning.
