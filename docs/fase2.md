# Fase 2 — Pestañas y abrir/guardar en la interfaz: resultado

Fecha: 2026-10-19 · Entorno: el mismo que en las fases anteriores

## Qué se construyó

| Módulo | Equivale en N++ a | Qué hace |
|---|---|---|
| `ui/DocTabBar` | `DocTabView`, `TabBarPlus` | Pestañas con el `BufferId` de cada documento e icono de estado (disquete azul: guardado, rojo: modificado, gris: solo lectura). Se pueden mover, se cierran con clic central, doble clic en la zona vacía crea un documento, y tienen menú contextual |
| `ui/EditorPane` | Una "vista" (`_mainDocTab` + `_mainEditView`) | Pestañas + editor. Cambia de documento con `SCI_SETDOCPOINTER` (`activateBuffer`), guardando y restaurando cursor, selección y scroll por documento y por vista. Lo usará también la vista dividida de la Fase 5 |
| `ui/DocStatusBar` | `Notepad_plus::updateStatusBar`, `setUniModeText`, `setDisplayFormat` | Tipo de documento · longitud y líneas · Lín, Col, Pos / Sel · fin de línea · codificación · INS/SOB. Los campos de fin de línea y codificación abren su menú |
| `ui/Prompter` | `MessageBox`, `CustomFileDialog` | Todas las preguntas al usuario, tras una interfaz (los tests usan `FakePrompter`) |
| `ui/MainWindow` | `Notepad_plus` (NppIO.cpp, NppCommands.cpp) | Menús y lógica: abrir, guardar, cerrar, codificación, fin de línea, arrastrar y soltar, título |
| `main.cpp` | `winmain.cpp` / `CmdLineParams` | `edita [archivos…] [-n línea] [-c columna]` |

## Menús

- **Archivo:** Nuevo · Abrir… · Recargar desde el disco · Guardar · Guardar como… · Guardar una copia como… · Guardar todo · Cerrar · Cerrar todo · Cerrar todo menos el actual · Salir.
- **Editar:** Deshacer · Rehacer · Cortar · Copiar · Pegar · Eliminar · Seleccionar todo · Conversión de fin de línea (Windows/Unix/Macintosh) · Solo lectura.
- **Ver:** Ajuste de línea · Pestaña siguiente (Ctrl+RePág) / anterior (Ctrl+AvPág).
- **Codificación:** Codificar en UTF-8 / UTF-8-BOM / UTF-16 BE BOM / UTF-16 LE BOM · Juegos de caracteres (las 16 categorías y 46 codepages del menú de N++) · Convertir a UTF-8 / UTF-8-BOM / UTF-16 BE BOM / UTF-16 LE BOM · Convertir a otro juego de caracteres.
- **Menú contextual de pestaña:** Cerrar · Cerrar todo menos este · a la izquierda · a la derecha · Guardar · Guardar como · Recargar · Copiar ruta completa / nombre / directorio · Abrir la carpeta contenedora · Solo lectura.

## Comportamientos portados de N++

- **Arranque:** siempre hay un documento ("nuevo 1").
- **Abrir:**
  - Un archivo ya abierto activa su pestaña.
  - Si la única pestaña es un "nuevo" sin cambios, el archivo ocupa su lugar (`loadBufferIntoView`).
  - Un directorio abre los archivos que contiene.
  - Desde la línea de comandos se ofrece crear los archivos que no existen.
- **Cerrar:**
  - Si hay cambios, pregunta Guardar / No guardar / Cancelar, mostrando antes el documento.
  - Cerrar todo, y salir, añaden "Guardar todo" / "No guardar ninguno" cuando hay varios documentos modificados.
  - Al cerrar el último documento queda un "nuevo 1". Cerrar un "nuevo" vacío que es el único no hace nada.
- **Guardar:**
  - Un documento sin título pide nombre.
  - No se puede "guardar como" encima de un archivo abierto en otra pestaña.
  - "Guardar todo" activa cada documento sin título antes de pedir su nombre.
- **Codificación** (`NppCommands.cpp`, `IDM_FORMAT_*`, en el modo "UTF-8 como codepage del sistema"):
  - *Juegos de caracteres > X:* si hay cambios pide guardarlos, avisa de que se pierde el deshacer y vuelve a leer el archivo con X.
  - *Codificar en UTF-x:* en un documento Unicode solo cambia cómo se guarda. En uno con juego de caracteres vuelve a leer los bytes como UTF-8.
  - *Convertir a…:* cambia cómo se guarda y deja el documento modificado.
  - "Codificar en ANSI" y "Convertir a ANSI" no hacen nada en N++ con UTF-8 de sistema, así que se omiten.
- **Título:** `*` si hay cambios, la ruta completa y " - Edita".
- **Barra de estado:** mismos campos y formatos (`Sel: caracteres | líneas`, `Sel N : caracteres` con varias selecciones, separador de miles del idioma).
- **Margen de números de línea:** ancho dinámico según los dígitos (mínimo 4).

## Añadidos y divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Guardar en un juego de caracteres que no puede representar el texto | "Best fit" (ā → a) o `?`, sin aviso | **Aviso antes de guardar** con el número de caracteres afectados: Convertir a UTF-8 y guardar / Guardar de todos modos / Cancelar | No perder datos en silencio |
| Convertir a un juego de caracteres concreto | Solo "Convertir a ANSI" (codepage del sistema) | Submenú "Convertir a otro juego de caracteres" | En Linux no hay codepage ANSI |
| "Codificar en UTF-8-BOM/UTF-16" desde un juego de caracteres | Recarga y el resultado depende del contenido | Lee los bytes como UTF-8 y deja el modo elegido para guardar | Comportamiento predecible |
| Una sola instancia (abrir en la ventana ya abierta) | Sí, por defecto | Aún no | Fuera del alcance del MVP; candidato a una fase posterior |
| Archivos recientes | Sí | Aún no | Fuera del alcance del MVP |
| Corrección del scroll con ajuste de línea (`restoreCurrentPosPostStep`) | Sí | Solo el primer paso | Con ajuste de línea, la línea superior restaurada puede variar ligeramente |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Abrir 3 archivos → 3 pestañas; abrir uno ya abierto activa su pestaña sin duplicarla | ✅ `openingFilesCreatesTabs` |
| Con solo un "nuevo 1" vacío, abrir lo sustituye | ✅ `openingFilesCreatesTabs`, `untitledWithChangesIsNotReplaced` |
| Editar marca la pestaña; guardar la desmarca; el deshacer de cada documento se conserva al cambiar de pestaña; cursor y scroll por pestaña | ✅ `editingMarksTabAndTitle`, `undoHistoryAndCaretArePerDocument`, `scrollPositionIsRestored` |
| Cerrar un documento modificado pregunta Guardar / No / Cancelar; Cancelar no cierra | ✅ `closingDirtyDocumentAsks`, `closeAllAsksForEachDirtyDocumentAndCanSaveAll`, `exitingWithDirtyDocumentsAsks` |
| La barra de estado muestra encoding y EOL correctos para los fixtures de la Fase 1 | ✅ `statusBarShowsEncodingAndEol` (7 casos), `statusBarShowsCaretAndSelection` |
| Manual: `edita a.txt b.cpp` abre ambos; soltar archivos los abre | ✅ Probado en Wayland con 4 archivos y `-n 120 -c 5`. Además, con tests: `droppingFilesOpensThem` (sobre la ventana), `droppingFilesOnTheEditorOpensThem` (sobre el editor), `commandLineOffersToCreateMissingFile`, `goToLineAndColumn` |

Además, con tests:
- Reinterpretar con un juego de caracteres (y cancelarlo si no se guardan los cambios).
- Convertir a UTF-8-BOM y guardar.
- "Codificar en UTF-8" desde un juego de caracteres.
- Caracteres no representables (cancelar / convertir a UTF-8).
- Conversión de fin de línea desde el menú.
- Solo lectura.
- Cerrar a la izquierda, a la derecha y los demás.
- Abrir un directorio.
- Margen de números de línea dinámico.

Tests: `tst_mainwindow` 37, y siguen en verde los de las fases anteriores (`tst_encoding` 59, `tst_textfile` 44, `tst_paths` 4, `tst_documentmanager` 42, `tst_perf` 4, `tst_engine` 20). El build desde cero no da ningún warning.
