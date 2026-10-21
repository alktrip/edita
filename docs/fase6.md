# Fase 6 — Seguridad de los datos: resultado

Fecha: 2026-10-20 · Entorno: el mismo que en las fases anteriores

## Qué se construyó

| Módulo | Equivale en N++ a | Qué hace |
|---|---|---|
| `DocumentManager::checkFileState` | `Buffer::checkFileState` | Compara cada documento con el disco. Si el archivo desapareció, el documento pasa a `Deleted` y queda modificado. Si cambió de fecha (o reapareció), pasa a `Modified` y anota la fecha nueva para no avisar dos veces. También actualiza el "solo lectura" del sistema de archivos |
| `MainWindow::checkModifiedDocuments` / `handleFileState` | `Notepad_plus::checkModifiedDocument`, `notifyBufferChanged` (`DOC_MODIFIED`/`DOC_DELETED`), `doReloadOrNot`, `doCloseOrNot` | Si el archivo cambió fuera, pregunta si recargarlo. Con "No", el documento queda modificado aunque se deshagan los cambios. Con "actualizar sin preguntar" recarga sin preguntar si no hay cambios propios, y "ir al final" funciona como `tail -f`. Si el archivo se borró, pregunta si mantenerlo en el editor o cerrarlo sin más preguntas. La recarga conserva el cursor y el scroll de cada vista |
| Cuándo se comprueba | `WM_ACTIVATEAPP` → `NPPM_INTERNAL_CHECKDOCSTATUS`; `notifyBufferActivated` | Al activar la ventana y al activar una pestaña (con la detección "solo el documento actual"). Además, un `QFileSystemWatcher` avisa mientras la ventana está activa (N++ no lo hace) |
| `DocumentManager::Watcher` | `Buffer::_isModified` (`SCN_MODIFIED`) | Un `DocWatcher` de Scintilla en cada documento cuenta inserciones y borrados vengan de donde vengan: de cualquiera de las dos vistas o de la vista invisible de "reemplazar en todos los abiertos" |
| `DocumentManager::backupBuffer`/`deleteBackup` | `FileManager::backupCurrentBuffer`, `deleteBufferBackup` | Escribe en `~/.config/edita/backup/<nombre>@AAAA-MM-DD_HHMMSS` la copia de los documentos con cambios que la copia aún no tiene. La borra cuando el documento queda sin cambios, al guardarlo o al cerrar su pestaña. Los archivos grandes no se copian |
| `MainWindow::backupNow` y su temporizador | Hilo `Notepad_plus::backupDocument` + `NPPM_INTERNAL_SAVEBACKUP` | Cada `snapshotBackupTiming` ms (7000 por defecto). Si aparece o desaparece una copia, guarda también la sesión para sobrevivir a un corte |
| `DocumentManager::loadFromBackup` | `doOpen(…, backupFileName, fileNameTimestamp)` | Restaura el documento desde su copia, con cambios hasta que se guarde. Los documentos sin título recuperan su nombre ("nuevo 1"). La fecha original permite avisar si el archivo cambió mientras Edita estaba cerrado |
| Sesión | `getCurrentOpenedFiles`, `loadSession`, `fileCloseAll(false, isSnapshotMode)` | En modo snapshot, `session.xml` incluye los documentos sin título y los cambios sin guardar (`backupFilePath`, `originalFileLastModifTimestamp`/`High` en el FILETIME de Windows). Al salir no se pregunta nada por los documentos que tienen copia |
| `MainWindow::backupBeforeSave` | `Notepad_plus::doSave` (`bak_simple`/`bak_verbose`) | Antes de guardar copia la versión del disco a `archivo.bak`, o a `nppBackup/archivo.AAAA-MM-DD_HHMMSS.bak` en modo detallado, opcionalmente en una carpeta propia. Si la copia falla, pregunta si guardar igualmente |
| `core/Preferences` | `NppGUI`, `feedGUIParameters`/`createXmlTreeFromGUIParams` | Preferencias en `config.xml` con los nombres y valores de N++: `<GUIConfig name="Auto-detection">yes</GUIConfig>` (`yes`/`auto`/`Update2End`/`autoUpdate2End` y sus variantes `…Old` para todos los archivos), `RememberLastSession` y `<GUIConfig name="Backup" action useCustumDir dir isSnapshotMode snapshotBackupTiming/>`. La Fase 7 añade el diálogo |

`isSnapshotMode()` es como en N++: snapshot activado, recordar la sesión y no haber arrancado con `--nosession`.

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Qué documentos se copian | Solo el actual (el único que se edita a mano) | Todos los que tengan cambios nuevos | "Reemplazar en todos los abiertos" también modifica los demás. Solo se escriben los que cambiaron |
| Detección mientras la ventana está activa | No (solo al activarla, salvo en el modo de vigilancia "tail") | Sí, con `QFileSystemWatcher` | En Linux es habitual cambiar archivos desde un terminal integrado o con herramientas en segundo plano |
| Contenido de la copia de un documento en un juego de caracteres de un byte | En esa codificación | En UTF-8; la sesión guarda el codepage y se recupera al restaurar | Así no se pierde lo que no cabe en ese juego de caracteres (por ejemplo "☃" en Windows-1252) |
| Deshacer en un documento restaurado o que se decidió no recargar | Al volver al punto de guardado se marca "sin cambios", aunque no coincida con el disco | Sigue modificado hasta guardarlo (`SCI_SETUNDOSAVEPOINT -1`) | Evita perder cambios por creer que el documento está guardado |
| Copias huérfanas (por ejemplo, tras un corte antes de guardar la sesión) | Se quedan en la carpeta | Igual | Mismo comportamiento; se pueden abrir a mano desde `~/.config/edita/backup` |
| UTF-16 sin BOM en una copia | Se escribe y se relee con la sesión | Se relee con detección automática | Caso muy raro; la sesión solo guarda el codepage |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Un archivo que otro programa cambia: se pregunta; "Sí" recarga conservando el cursor; "No" deja el texto del editor marcado como modificado y no vuelve a preguntar por el mismo cambio | ✅ `externalChangeAsksToReload` |
| "Actualizar sin preguntar" recarga los documentos sin cambios propios y, con "ir al final", deja el cursor al final; con cambios propios se pregunta | ✅ `autoUpdateReloadsSilentlyAndGoesToEnd` |
| Un archivo borrado: mantener (queda modificado) o cerrar sin preguntar; si reaparece se ofrece recargarlo | ✅ `deletedFileCanBeKeptOrClosed` |
| Detección solo del documento visible (por defecto), de todos o desactivada; comprobación al activar una pestaña | ✅ `currentFileModeChecksOnActivation` |
| Guardar desde Edita no se toma por un cambio externo, y el archivo sigue vigilado aunque `QSaveFile` lo sustituya | ✅ `ownSaveIsNotAnExternalChange` |
| Con la ventana activa, un cambio en disco se detecta solo | ✅ `watcherDetectsChangesWhileActive` |
| Snapshot: solo se copian los documentos con cambios nuevos (también los hechos con la vista invisible); guardar, deshacer hasta el original o cerrar la pestaña borran la copia | ✅ `snapshotBackupsOnlyWhatChanged` |
| Salir sin preguntar y, al volver a abrir, recuperar un documento sin título, un archivo con cambios (con su lenguaje) y uno en Windows-1252 con caracteres que no caben, todos modificados y con el archivo original intacto; guardar escribe el archivo y borra la copia | ✅ `snapshotRestoresUnsavedWorkAcrossRestarts` |
| Si el archivo cambió mientras Edita estaba cerrado, se avisa tras restaurar la copia | ✅ `fileChangedWhileClosedIsDetectedAfterRestore` |
| Sin snapshot (o con `--nosession`, o sin recordar la sesión) no hay copias y al salir se pregunta como antes | ✅ `withoutSnapshotExitAsksAndKeepsNoBackups`, `notRememberingTheSessionSkipsIt`, `exitingWithDirtyDocumentsAsks` |
| Si no se puede escribir la copia al salir, se pregunta por ese documento | ✅ `deletedBackupMakesExitAsk` |
| Copia al guardar: `.bak` simple, detallada con fecha en `nppBackup/`, carpeta propia, y pregunta si falla | ✅ `backupOnSaveKeepsThePreviousVersion` |
| `config.xml` y `session.xml` con los nombres de N++ (incluidos `useCustumDir` y el FILETIME partido en dos enteros) | ✅ `preferencesUseNotepadPlusPlusNames`, `backupNamesAndFileTime`, `sessionKeepsBackupFields` |

Tests: `tst_datasafety` 18 (contando `initTestCase` y `cleanupTestCase`), y siguen en verde los de las fases anteriores (13 suites). El build no da ningún warning.
