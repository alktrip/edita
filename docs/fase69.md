# Fase 69 — Archivo y Editar › Sobre la selección como en Notepad++

Fecha: 2026-11-06

## Por qué

Es el tercer bloque de [lo que tiene Notepad++ y Edita no](paridad-npp.md): las órdenes que faltaban del menú Archivo (renombrar, mandar a la papelera, abrir con otra aplicación), el submenú **Editar › On Selection** y los dos paneles del menú Editar (caracteres e historial del portapapeles).

## Cómo lo hace N++ y qué hace Edita

| Orden de N++ | En Edita |
|---|---|
| **Archivo › Rename…** (`IDM_FILE_RENAME`, `fileRename`) | **Archivo › Renombrar…**: con el archivo en el disco, el diálogo de guardar en su carpeta y con su nombre; `DocumentManager::moveFile` lo mueve (`MoveFileEx` con `MOVEFILE_REPLACE_EXISTING`: el diálogo ya preguntó si sustituir), el documento sigue abierto con la ruta nueva y el lenguaje se vuelve a detectar (salvo si se eligió a mano). No deja ponerle el nombre de otro documento abierto. Sin archivo, cambia el nombre de la pestaña («Rename Current Tab»), sin espacios alrededor, no vacío, sin `/` y no repetido. Avisa a los plugins: `NPPN_FILEBEFORERENAME`, `NPPN_FILERENAMED` o `NPPN_FILERENAMECANCEL` |
| **Archivo › Move to Recycle Bin** (`IDM_FILE_DELETE`, `fileDelete`) | **Archivo › Mover a la papelera**: pregunta, lo manda a la papelera de freedesktop (`QFile::moveToTrash`) y cierra el documento sin preguntar por sus cambios (también si está clonado en la otra vista). `NPPN_FILEBEFOREDELETE`, `NPPN_FILEDELETED` o `NPPN_FILEDELETEFAILED` |
| **Archivo › Open in Default Viewer** (`IDM_FILE_OPEN_DEFAULT_VIEWER`, `ShellExecute "open"`) | **Archivo › Abrir con la aplicación predeterminada** (`QDesktopServices::openUrl` con el archivo) |
| **Archivo › Open Containing Folder › Explorer / cmd / Folder as Workspace** (`IDM_FILE_OPEN_FOLDER`, `…_CMD`, `…CONTAININGFOLDERASWORKSPACE`) | **Archivo › Abrir la carpeta contenedora › Administrador de archivos / Terminal / Explorador de carpetas** (antes solo estaba en el menú de la pestaña). La terminal es la integrada; PowerShell no aplica |
| **Editar › On Selection › Open File** y **Open Containing Folder in Explorer** (`IDM_EDIT_OPENSELECTEDFILE*`) | **Editar › Sobre la selección › Abrir el archivo** / **Abrir la carpeta contenedora**. La ruta es la selección o, sin ella, la que hay alrededor del cursor, con los mismos delimitadores que `NPPM_GETFILENAMEATCURSOR` (empieza tras espacio, tabulador, `[`, `(`, `"`, `<`; acaba en espacio, tabulador, `:`, paréntesis, corchetes, `<`, `>`, `"`). En lugar de `ExpandEnvironmentStrings`, `$VAR`, `${VAR}` y `~`; también `file://`. Si no existe y es relativa, junto al documento. La carpeta se abre en el administrador de archivos (Windows además selecciona el archivo; aquí se abre la carpeta) |
| **Editar › On Selection › Redact Selection █ (Shift: ●)** (`IDM_EDIT_REDACT_SELECTION`) | **Tapar la selección**: cada carácter (no cada byte: la `ñ` o un emoji son uno) cambia por █, o ● con Mayús, salvo `\r` y `\n`; todas las selecciones, en un paso de deshacer. Se graba en las macros |
| **Editar › On Selection › Search on Internet** y **Change Search Engine…** (`IDM_EDIT_SEARCHONINTERNET`, `…CHANGESEARCHENGINE`) | **Buscar en internet**: la selección (o la palabra del cursor), con varias selecciones nada, como N++. El texto va codificado para la URL (N++ lo pone tal cual). **Cambiar el buscador…** abre la página nueva **Buscador** de Preferencias: DuckDuckGo, Google (el de serie), Yahoo!, Stack Overflow o uno propio con `$(CURRENT_WORD)`; si el propio no empieza por `http://` o `https://`, Google. Se guarda como N++ (`<GUIConfig name="searchEngine" searchEngineChoice="2" searchEngineCustom="">`); Bing, que N++ quitó, pasa a DuckDuckGo |
| **Editar › Character Panel** (`IDM_EDIT_CHAR_PANEL`, `AnsiCharPanel`) | **Editar › Panel de caracteres** (`ui/EditPanels`): un panel acoplable con los 256 de la tabla ANSI (Windows-1252, la de N++ en un sistema occidental): valor, hexadecimal, carácter (NULL, TAB, Space… los de control), nombre HTML y número HTML decimal y hexadecimal, con las mismas tablas de `AsciiListView`. Doble clic en el carácter o Intro lo inserta; en otra columna, inserta lo que pone |
| **Editar › Clipboard History** (`IDM_EDIT_CLIPBOARDHISTORY_PANEL`) | **Editar › Historial del portapapeles**: desde que se abre la primera vez, lo copiado (en Edita o en otra aplicación, con `QClipboard::dataChanged`), lo último arriba, sin repetir, hasta 100 (N++ no pone límite); se muestran 64 caracteres en una línea y el resto en la ayuda emergente. Doble clic o Intro lo pega |

Además:
- **Identificadores de Notepad++:** todas estas órdenes tienen el suyo (`IDM_*`). Tapar la selección se graba en las macros.
- **SDK de plugins:** `NPPN_FILEBEFORERENAME` … `NPPN_FILEDELETED` (21-26) en `edita_plugin.h`.
- **Preferencias:** `showPreferences` puede abrir una página concreta.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Las direcciones de cada buscador, la personalizada y el texto codificado | ✅ `tst_onselection`: `searchEngines` |
| La ruta alrededor del cursor con los delimitadores de N++ | ✅ `fileNameAtCaret` |
| Variables de entorno, `~`, `file://` y rutas relativas | ✅ `resolvePaths` |
| Tapar: por caracteres, sin los fines de línea, varias selecciones, un paso de deshacer, ● | ✅ `redact` |
| Buscar en internet con el buscador elegido; sin selección, la palabra; con varias, nada | ✅ `searchOnInternet` |
| La página Buscador (Bing → DuckDuckGo, el campo propio solo con «Otro buscador») | ✅ `searchEnginePreferences`, `tst_preferences` |
| Abrir la ruta seleccionada o junto al cursor, su carpeta, y el aviso si no existe | ✅ `openSelectedPath` |
| Renombrar: el archivo en disco con el lenguaje nuevo, cancelar, no sobre otro abierto | ✅ `renameFile` |
| Renombrar la pestaña de un documento sin archivo, y los nombres que no valen | ✅ `renameUntitled` |
| Papelera: pregunta, aviso si falla, cierra sin preguntar por los cambios | ✅ `moveToTrash` |
| Abrir con la aplicación predeterminada y la carpeta contenedora | ✅ `defaultViewerAndFolder` |
| El panel de caracteres: la tabla de N++, insertar por columna y con Intro | ✅ `characterPanel` |
| El historial: sin repetir, con límite, lo copiado aparece arriba y se pega con doble clic | ✅ `clipboardHistory` |
| El manual cita cada orden en los dos idiomas | ✅ `tst_manual` |
| Pasan las 59 suites, el build no da avisos (GCC 16 y clang), y compila con el GCC 12 y Qt 6.8.3 de la CI | ✅ |
