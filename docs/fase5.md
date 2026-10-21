# Fase 5 — Vista dividida y sesión: resultado

Fecha: 2026-10-20 · Entorno: el mismo que en las fases anteriores

## Qué se construyó

| Módulo | Equivale en N++ a | Qué hace |
|---|---|---|
| `MainWindow` (dos `EditorPane` en un `QSplitter`) | `_mainDocTab`/`_mainEditView` y `_subDocTab`/`_subEditView`, `SplitterContainer` | Vista principal y secundaria; la activa es la que tiene el foco (`_activeView`) |
| `MainWindow::transferToOtherView` | `docGotoAnotherEditView(TransferMove/TransferClone)` | Mover o clonar el documento a la otra vista; el clon es el **mismo documento de Scintilla**, con cursor y scroll propios en cada vista |
| `updatePaneVisibility` | `showView`/`hideView` | La vista que se queda sin pestañas se oculta (si la otra tiene) |
| `closeBuffer`/`closeBuffers` | `fileClose`, `fileCloseAll`, `removeBufferFromView` | Cerrar un clon solo lo quita de esa vista, sin preguntar; el documento se libera cuando ya no está en ninguna |
| `core/Session` | `Session`, `sessionFileInfo`, `NppParameters::writeSession`/`getSessionFromXmlTree` | `session.xml` con el **mismo esquema que N++**: copia `.inCaseOfCorruption.bak` antes de escribir, comprobación tras escribir con restauración de la copia si falla, y lectura de sesiones de N++ ignorando lo que Edita no usa |
| `MainWindow::captureSession`/`restoreSession` | `getCurrentOpenedFiles`, `loadSession` | Archivos y orden por vista, pestaña y vista activas, cursor/selección/primera línea visible, lenguaje (por el texto del menú, como N++), codificación forzada, solo lectura y clones |
| `core/ConfigXml` (`<FindHistory>`, `AppPosition`) | `feedFindHistoryParameters`, `GUIConfig AppPosition` | Historial y opciones de búsqueda, y tamaño/maximizado de la ventana entre sesiones |

## En la interfaz

- **Ver:**
  - Mover a la otra vista.
  - Clonar en la otra vista.
  - Cambiar a la otra vista (F8).
  - Girar la división (lado a lado ↔ arriba/abajo).
- Lo mismo en el **menú contextual de la pestaña**.
- **Arranque:**
  - Se restaura la sesión anterior y después se abren los archivos de la línea de comandos, como N++.
  - El "nuevo 1" inicial desaparece si la sesión abre algo.
  - `edita --nosession` no lee ni guarda la sesión (`-nosession` de N++).
- **Al salir** se guardan la sesión (`~/.config/edita/session.xml`), el historial de búsqueda y el tamaño de la ventana (`config.xml`). Si el usuario cancela al preguntarle por cambios sin guardar, no se escribe nada.
- **Archivos de la sesión que ya no existen** (o rutas de Windows de una sesión de N++) se omiten sin avisar, como N++. Una sesión dañada recurre a la copia de seguridad y avisa.

## Divergencias y pendientes

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Documentos sin título y cambios sin guardar en la sesión | Con "snapshot" y copias periódicas, sí | No (como N++ sin snapshot: al salir se pregunta) | Fuera del alcance del MVP (`backupFilePath` se escribe vacío) |
| Estado de plegado y marcadores en la sesión | Se restauran | Se leen y se conservan en el formato, pero no se aplican | Edita aún no tiene marcadores |
| `activeIndex` | Índice de la pestaña (cuenta pestañas que no se guardan) | Índice entre los archivos guardados | Evita activar la pestaña equivocada cuando hay documentos sin título |
| Arrastrar pestañas entre vistas | Sí | Con el menú (Mover/Clonar) | Pendiente |
| Indicador visual de la vista activa | Borde de color en la pestaña activa | El foco del editor | Pendiente |
| `-nosession` | Un guion | `--nosession` | Convención de QCommandLineParser, compatible con `-n línea` |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Clonar un documento: editar en una vista se refleja al instante en la otra, y cada vista mantiene su propio cursor | ✅ `cloneSharesTheDocumentWithIndependentCarets` |
| Mover la última pestaña de la vista secundaria la oculta (`hideView`) | ✅ `moveToOtherViewAndBack`, `closingTheLastDocumentOfTheMainViewHidesIt` |
| Ida y vuelta: 2 archivos en la principal, 1 en la secundaria y 1 clonado, con cursores y selecciones distintos y un lenguaje forzado → guardar → recrear la ventana → estado **idéntico** | ✅ `sessionRoundTripRestoresIdenticalState`: archivos y orden por vista, pestaña y vista activas, `startPos`/`endPos`/`firstVisibleLine`, lenguaje (Bash forzado en un `.py`) y codificación |
| Un archivo que ya no existe se omite sin error; un `session.xml` dañado recurre al `.bak` | ✅ `missingFilesAreSkippedAndEncodingIsForced`, `corruptSessionUsesTheBackup`, `writingKeepsABackupAndCorruptFallsBack` |
| Un `session.xml` escrito por Notepad++ se carga (al menos los campos del subconjunto) | ✅ `readsSessionWrittenByNotepadPlusPlus`: sesión con todos los atributos de N++ 8.x (mapas, colores de pestaña, `Mark`, `Fold`, marcas de tiempo) y una ruta de Windows, que se normaliza y se omite al abrir |

Además, con tests:
- Escritura al cerrar y restauración al arrancar.
- Modo sin sesión.
- Historial de búsqueda y tamaño de ventana entre sesiones.
- `<FindHistory>` con los nombres de atributo de N++ (incluido su `fifRecuisive`).
- Abrir un archivo que ya está en la otra vista la activa.
- F8 y girar la división.
- Regresión de la altura de la barra de pestañas al dividir antes de mostrar la ventana. Fallo encontrado con una captura y corregido en `EditorPane`.

Tests: `tst_session` 6 y `tst_splitview` 15, y siguen en verde los de las fases anteriores (379 en total en 12 suites). El build desde cero no da ningún warning.
