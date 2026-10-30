# Fase 1 — Análisis de Notepad++ y plan del MVP

Referencia analizada: `referencia/npp` @ `6ea1128f` (19-oct-2026), Scintilla 5.6.6, Lexilla 5.5.3.
Todas las rutas de código son relativas a `referencia/npp/PowerEditor/src/` salvo que se indique otra cosa.

---

## 0. Decisión que necesito antes de la Fase 0: QScintilla vs. Scintilla 5 (port Qt oficial)

Pediste Qt6 + QScintilla. Al revisar el código encontré un problema serio para el objetivo de que "el núcleo de edición se comporte igual":

| | **QScintilla 2.14.1** (`qscintilla-qt6` en Arch) | **Scintilla 5.6.6 `qt/ScintillaEditBase` + Lexilla 5.5.3** (lo que ya trae `referencia/npp`) |
|---|---|---|
| Versión de Scintilla | Rama 3.x (por verificar en Fase 0, creo que 3.10) | 5.6.6, la **misma** que usa N++ |
| Lexers | Integrados en la propia librería, sin `SCI_SETILEXER` ni Lexilla | Lexilla, `CreateLexer(nombre)`, igual que en `ScintillaEditView.cpp:2326-2333` |
| Parches de N++ | No se pueden aplicar sin recompilar QScintilla | Se compila el árbol ya parcheado de N++ |
| Regex Boost (`SCI_OWNREGEX`) | Habría que recompilar QScintilla | Se enlaza `boostregex/` igual que N++ |
| Lexers propios de N++ (`LexSearchResult`, `LexUser`, `LexObjC`) | No están | Están en `lexilla/lexers/` |
| `KEYWORDSET_MAX` | 8 | 30 (N++ lo parchea; `langs.xml` usa hasta 8 listas + 8 substyles) |
| Change history, APIs `*FULL` de 64 bits | No | Sí |
| Qt6 | Sí | Sí (`ScintillaEditBase.pro`: `equals(QT_MAJOR_VERSION, 6): QT += core5compat`, requiere `qt6-5compat`) |

Comparé el Scintilla/Lexilla de N++ con los zips originales de `PowerEditor/scintilla.original.forUpdating/`. Los parches son pocos (unas 28 líneas de diff en `scintilla/src`, más headers, `StyleContext`/`WordList` y 3 lexers añadidos), y el directorio `scintilla/qt/` está **sin modificar**. O sea, el port Qt oficial compila sobre el Scintilla parcheado de N++.

**Recomendación:** usar `ScintillaEditBase` + Lexilla + `boostregex` copiados de `referencia/npp` (todo con licencia compatible con GPLv3) y compilarlos con CMake como librerías estáticas dentro del proyecto. Sigue siendo Qt6 + Scintilla nativo en Linux, pero con el mismo motor, los mismos lexers y la misma regex que Notepad++. QScintilla obligaría a reimplementar o renunciar a parte del comportamiento, y los `styleID` de `stylers.xml` están pensados para Lexilla 5.

El resto del documento asume esta opción. Si prefieres QScintilla, cambian las Fases 0, 3 y 4. Lo indico en cada una.

---

## 1. Arquitectura de Notepad++

### 1.1 Visión general

```
winmain.cpp ─► Notepad_plus_Window (HWND marco, bucle de mensajes)
                   └─► Notepad_plus  (núcleo: vistas, pestañas, diálogos)
                         ├─ NppCommands.cpp      command(IDM_*) – menús/atajos (IDs en menuCmdID.h)
                         ├─ NppBigSwitch.cpp     WndProc: WM_* y mensajes de plugin NPPM_*
                         ├─ NppNotification.cpp  WM_NOTIFY de Scintilla y de las pestañas
                         ├─ NppIO.cpp            abrir/guardar/cerrar/sesiones
                         └─ Notepad_plus.cpp     lógica de vistas, buscar en archivos, etc.
NppParameters (singleton, Parameters.cpp/.h) ─► carga/guarda todos los XML
FileManager (singleton MainFileManager, Buffer.cpp) ─► dueño de todos los documentos
```

Todo se comunica por mensajes de Windows. La API de plugins es la misma (`NPPM_*`, `MISC/PluginsManager/Notepad_plus_msgs.h`).

### 1.2 Buffers y documentos — `ScintillaComponent/Buffer.h/.cpp`

- **Modelo:** `FileManager` es dueño de objetos `Buffer`, y `BufferID` es simplemente `Buffer*` (`Buffer.h:27`). Cada `Buffer` envuelve un `Document` de Scintilla (`SCI_CREATEDOCUMENT`, `Buffer.cpp:1000`) y guarda los metadatos: ruta, `LangType`, `_encoding`/`_unicodeMode`, `EolType`, dirty, solo lectura, timestamps, y **posición y plegado por vista** (`setPosition`/`getPosition`, `Buffer.cpp:680-716`, indexados por `ScintillaEditView*`).
- **Recuento de referencias:** `addBufferReference`/`closeBuffer` (`Buffer.cpp:919-943`). El documento se libera cuando ninguna vista lo usa.
- **Notificaciones:** `Buffer::doNotify(mask)` con flags `BufferChange*` (`Buffer.h:42-53`) → `FileManager::beNotifiedOfBufferChange` → `Notepad_plus::notifyBufferChanged` (`Notepad_plus.cpp:6729`), que actualiza pestañas, barra de estado y menús.
- **Carga:** `FileManager::loadFile` (`Buffer.cpp:944`) → `loadFileData` (`:1915`) → `copyFileContentToScintilla` (`:2103`). Usa una vista Scintilla oculta (`_pscratchTilla`): le asigna el documento nuevo con `SCI_SETDOCPOINTER` y añade el texto por bloques con `SCI_APPENDTEXT`. **Dentro de Scintilla el texto siempre está en UTF-8** y la conversión se hace al cargar y al guardar.
- **Detección de encoding** (`Buffer.cpp:2125-2215`), en este orden:
  1. BOM → `Utf8_16_Read::determineEncodingFromBOM` (`Utf8_16.h:116`).
  2. Sin BOM y con la opción `_detectEncoding` activa → `uchardet` (`FileManager::detectCodepage`, `Buffer.cpp:1808`; desactiva TIS-620 a propósito). El nombre del charset pasa a codepage con `EncodingMapper` (`EncodingMapper.cpp`).
  3. Si no hay resultado → heurística UTF-8 válido / ASCII de 7 bits (`Utf8_16_Read`, enum `u78`), o el codepage por defecto.
  - Semántica: `_encoding == -1` ⇒ familia Unicode (`_unicodeMode`: `uni8Bit`, `uniUTF8` con BOM, `uni16BE/LE`, `uniUTF8_NoBOM`, ver `MISC/Common/NppConstants.h:180`). Si no, `_encoding` es un codepage.
- **Fin de línea:** `getEOLFormatForm` (`Buffer.cpp:69`). Decide **el primer** fin de línea que aparece (CRLF→windows, CR→macos, LF→unix). Al activar el buffer se aplica con `SCI_SETEOLMODE` (`setCRLF`).
- **Guardado:** `FileManager::saveBuffer` (`Buffer.cpp:1453`). Toma `SCI_GETCHARACTERPOINTER`, convierte con `Utf8_16_Write` (familia Unicode, escribe el BOM si toca) o con `WcharMbcsConvertor::encode` (codepages), y escribe con `Win32_IO_File` (`MISC/Common/FileInterface.h:25`).
- **Detección de lenguaje:** por extensión con `NppParameters::getLangFromExt` (`Parameters.cpp:4457`; primero las extensiones de usuario de `stylers.xml` y después `langs.xml`), y por contenido con `detectLanguageFromTextBeginning` (shebang, `<?xml`, …; `Buffer.cpp:1821`).
- **Cambios externos:** `Buffer::checkFileState` (`Buffer.cpp:430`) y `Notepad_plus::checkModifiedDocument` (`Notepad_plus.cpp:4719`). *Queda fuera del MVP.*

### 1.3 Pestañas — `ScintillaComponent/DocTabView.*`, `WinControls/TabBar/TabBar.cpp`

- `DocTabView : TabBarPlus` (`DocTabView.h:32`). Cada pestaña guarda su `BufferID`, y la clase tiene `addBuffer`/`closeBuffer`/`activateBuffer`/`bufferUpdated`/`getIndexByBuffer`/`setBuffer` (`DocTabView.h:49-61`).
- **Clave del diseño: N documentos para solo 2 widgets editores.** `ScintillaEditView::activateBuffer` (`ScintillaEditView.cpp:2337`) guarda la posición y el plegado del buffer saliente, cambia el documento con `SCI_SETDOCPOINTER`, llama a `defineDocType` (estilos) y restaura la posición del entrante. El undo es por documento, así que sobrevive al cambiar de pestaña.
- `Notepad_plus::loadBufferIntoView` (`Notepad_plus.cpp:4842`) sustituye el "new 1" vacío si es la única pestaña. `Notepad_plus::activateBuffer` (`:5183`).
- Ciclo de vida: `doOpen` (`NppIO.cpp:213`), `doSave` (`:681`), `doClose` (`:852`), `fileSave/SaveAs/SaveAll/Close/CloseAll` (`NppIO.cpp:1139-2097`).

### 1.4 Vista dividida — `WinControls/SplitterContainer/*`, `Notepad_plus.cpp`

- Dos parejas fijas: `_mainDocTab`/`_mainEditView` y `_subDocTab`/`_subEditView` (`Notepad_plus.h:300-306`). Punteros a la pareja activa: `_pEditView`/`_pDocTab`, y `_activeView ∈ {MAIN_VIEW, SUB_VIEW}` (`:439`).
- `SplitterContainer` las contiene y permite girar la orientación. Funciones: `showView`/`hideView` (`Notepad_plus.cpp:4732/4776`), `switchEditViewTo` (`:4958`), y `docGotoAnotherEditView(TransferMove|TransferClone)` (`:5099`).
- **Clonar** = el mismo `Document` en las dos vistas (multivista nativa de Scintilla). Cada vista tiene su propio cursor gracias a la posición por vista del `Buffer`.
- Existe además `_invisibleEditView`, una vista oculta para operaciones en segundo plano (buscar en archivos, recoger marcadores para la sesión).

### 1.5 Lexers y estilos — `ScintillaComponent/ScintillaEditView.cpp`

- Tabla `LangType` → nombres: `ScintillaEditView::_langNameInfoArray` (`:99`). `_langName` enlaza con `langs.xml`/`stylers.xml`, y `_lexerID` es el nombre del lexer en Lexilla.
- `defineDocType(LangType)` (`:1750`):
  1. Aplica `<GlobalStyles>` de `stylers.xml` (Default Style → `STYLE_DEFAULT`, `SCI_STYLECLEARALL`, indicadores de búsqueda, etc.).
  2. Un `switch` de unos 96 casos llama a `setXxxLexer()`. La mayoría son una línea `setLexer(L_X, LIST_0|LIST_1…, baseStyleID, numSubStyles)` definida inline en `ScintillaEditView.h:741-1000`.
- `setLexer` (`:1524`):
  - `setLexerFromLangID` hace `SCI_SETILEXER(CreateLexer(lexerID))` (`:2326`).
  - `makeStyle` (`:1587`) aplica cada `<WordsStyle>` del `<LexerType>` (pasando por `setStyle`, `:896`, que gestiona el *Global override*).
  - `setKeywords` (`:1502`) une las keywords de `langs.xml` con las keywords de usuario del `keywordClass` en `stylers.xml`.
  - Substyles (`SCI_ALLOCATESUBSTYLES`) y propiedades de plegado.
- Algunos lenguajes tienen lógica propia: `setCppLexer` (`:1242`), `setHTMLLexer`, `setXmlLexer`, JS/PHP/ASP embebido, `setJsonLexer`, `setTypeScriptLexer`.
- Los lenguajes definidos por el usuario (UDL: `UserDefineDialog.cpp` + `LexUser`) *quedan fuera del MVP*.

### 1.6 Buscar / reemplazar — `ScintillaComponent/FindReplaceDlg.*`, `boostregex/`, `Notepad_plus.cpp`

- Opciones: `FindOption` (`FindReplaceDlg.h:85`). Modo `SearchType {FindNormal, FindExtended, FindRegex}` (`:82`). Ámbito `InWhat {ALL_OPEN_DOCS, FILES_IN_DIR, CURRENT_DOC, CURR_DOC_SELECTION, …}` (`:64`).
- Buscar siguiente: `processFindNext` (`FindReplaceDlg.cpp:2943`). Fija `SCI_SETSEARCHFLAGS` y usa `SCI_SEARCHINTARGET`, con las flags propias `SCFIND_REGEXP_EMPTYMATCH_*`, `SKIPCRLFASONE` (`:3038-3057`).
- Reemplazar: `processReplace` (`:3156`). Reemplazar todo, contar, marcar y buscar todo: `processAll`/`processRange` (`:3312/:3435`).
- Modo extendido (`\n \t \0 \xNN …`): clase `Searching` (`FindReplaceDlg.h:120`).
- **Regex:** es **Boost.Regex (standalone, incluido en `boostregex/boost`) conectado dentro de Scintilla**. Se compila Scintilla con `-DSCI_OWNREGEX -DBOOST_REGEX_STANDALONE` y `boostregex/BoostRegExSearch.cxx:266` implementa `CreateRegexSearch`, que `scintilla/src/Document.cxx:2338` llama en lugar del motor propio (ver `boostregex/nppSpecifics_mingw.mak`). Iteración sobre UTF-8: `UTF8DocumentIterator.cxx`. Así la sintaxis (lookbehind, `\R`, `(?s)`, recursión `(?R)`) es la de Boost/Perl.
- **Buscar en archivos:**
  - `Notepad_plus::findInFiles` (`Notepad_plus.cpp:2204`) → `createFilelistForFiles` → `getMatchedFileNames` (`:1871`, recorrido de directorio con filtros, recursivo y ocultos opcionales) → `findInFilelist`.
  - Por cada archivo, `FileManager::loadFileContentForSearch` (`Buffer.cpp:2250`) lo carga en un documento temporal en `_invisibleEditView` y se ejecuta `processAll(ProcessFindAll)`.
  - Los resultados van al panel `Finder` (`FindReplaceDlg.h:138`), que es otra vista Scintilla con el lexer `searchResult` (`lexilla/lexers/LexSearchResult.cxx`): `addFileNameTitle`/`addSearchLine`/`addFileHitCount` (`FindReplaceDlg.cpp:5702-5750`) y `finishFilesSearch` (`:6067`).
  - Reemplazar en archivos: `replaceInFiles` (`Notepad_plus.cpp:1987`).
  - Todo es **síncrono en el hilo de UI**, con un diálogo `Progress`.

### 1.7 Sesiones — `Parameters.h/.cpp`, `NppIO.cpp`

- Estructuras: `Session` (`Parameters.h:159`: `_activeView`, `_activeMainIndex`, `_activeSubIndex`, `_mainViewFiles`, `_subViewFiles`) y `sessionFileInfo` (`:123`: ruta, lenguaje, encoding, `Position` con `firstVisibleLine/startPos/endPos/selMode/xOffset…`, marcadores, plegados, readonly, fijada, ruta de backup).
- Guardar al salir: `saveCurrentSession` (`NppIO.cpp:3003`) → `getCurrentOpenedFiles` (`Notepad_plus.cpp:6509`) → `NppParameters::writeSession` (`Parameters.cpp:4830`) → `session.xml` (antes hace una copia `.inCaseOfCorruption.bak`).
  ```xml
  <NotepadPlus><Session activeView="0">
    <mainView activeIndex="1"><File firstVisibleLine=".." startPos=".." endPos=".." selMode=".." lang="C++" encoding="-1" filename="/ruta"/>…</mainView>
    <subView activeIndex="0">…</subView>
  </Session></NotepadPlus>
  ```
- Cargar: `NppParameters::loadSession` (`Parameters.cpp:3199`) / `getSessionFromXmlTree` (`:3416`) → `Notepad_plus::loadLastSession`/`loadSession` (`NppIO.cpp:2483/2494`).
- Los snapshots o backups periódicos de documentos sin guardar (`FileManager::backupCurrentBuffer`, `Buffer.cpp:1287`) *quedan fuera del MVP*.

### 1.8 Configuración — `Parameters.cpp`, `NppXml.h`

- `NppParameters::load` (`Parameters.cpp:1298`) resuelve `_userPath` (`%APPDATA%\Notepad++`, o la carpeta del exe si existe `doLocalConf.xml`). En el primer arranque **copia los `*.model.xml` al directorio del usuario** (`langs.model.xml` → `langs.xml`, `:1427-1467`; lo mismo con `stylers`, `shortcuts` y `contextMenu`).
- Archivos:
  - `langs.xml`: `<Language name ext commentLine…><Keywords name="instre1|instre2|type1..7|substyle1..8">`, 96 lenguajes.
  - `stylers.xml` y `installer/themes/*.xml` (mismo esquema): `<LexerStyles><LexerType name desc ext><WordsStyle name styleID fgColor bgColor fontName fontStyle fontSize keywordClass/>` y `<GlobalStyles><WidgetStyle…>`. Los colores van en **hex RGB**; N++ los convierte a `COLORREF` BGR.
  - `config.xml`: preferencias de GUI, historial de búsqueda, archivos recientes.
  - `session.xml`, `shortcuts.xml`, `contextMenu.xml`.
- Parseo: `feedKeyWordsParameters` (`:5980`), `feedStylerArray` (`:5276`), `feedGUIParameters` (`:6034`), `feedFindHistoryParameters` (`:3927`). Escritura: `createXmlTreeFromGUIParams` (`:7622`), `writeFindHistory` (`:8304`), `writeStyles` (`:8845`).
- XML: pugixml (incluido en `src/pugixml`) envuelto en `NppXml.h`.

---

## 2. Qué es portable y qué depende de Win32

### 2.1 Reutilizable tal cual (datos)

| Recurso | Ruta en la referencia |
|---|---|
| Definición de lenguajes y keywords | `PowerEditor/src/langs.model.xml` |
| Estilos por defecto | `PowerEditor/src/stylers.model.xml` |
| Temas (mismo esquema que stylers) | `PowerEditor/installer/themes/*.xml` |
| (post-MVP) localización, function list, APIs de autocompletado | `installer/nativeLang/`, `installer/functionList/`, `installer/APIs/` |

### 2.2 Código portable, reutilizable con poca adaptación

| Componente | Ruta | Nota |
|---|---|---|
| Scintilla 5.6.6 (parcheado) + port Qt | `scintilla/src`, `scintilla/include`, `scintilla/qt/ScintillaEditBase` | sin cambios |
| Lexilla 5.5.3 (parcheado) | `lexilla/` | incluye `LexSearchResult`, `LexUser` |
| Regex Boost | `boostregex/*` | solo depende de los internos de Scintilla |
| uchardet | `src/uchardet` | también existe como paquete en Arch (`uchardet 0.0.8`) |
| pugixml | `src/pugixml` | también como paquete (`pugixml 1.16`) |
| BOM / UTF-16 ↔ UTF-8 | `Utf8_16.cpp/.h` | 1 tipo Win32 |
| Nombres de charset | `EncodingMapper.cpp` | portable, pero mapea a **codepages de Windows**; hay que traducir a nombres de iconv |
| Lógica pura | `getEOLFormatForm`, `detectLanguageFromTextBeginning`, `getLangFromExt`, `_langNameInfoArray`, máscaras de `setXxxLexer`, `Searching::convertExtendedToString`, flags de `processFindNext` | se porta como tablas y funciones |
| Esquema `session.xml` | `Parameters.cpp:3416/4830` | se reimplementa compatible |

### 2.3 Hay que reescribir (atado a Win32)

- **Toda la UI:** `WinControls/*` (TabBar, SplitterContainer, DockingWnd, StaticDialog, StatusBar, ToolBar, Preference…), los `.rc`, `Notepad_plus_Window`, `NppBigSwitch` (WndProc), la UI de `FindReplaceDlg`, `DarkMode/`, `dpiManagerV2`.
- **E/S y sistema:** `Win32_IO_File`, `_wfopen`, `FILETIME`, `GetFullPathName`/`GetLongPathName`, `PathIsNetworkPath`, `CreateThread`, `ReadDirectoryChanges` (→ `QFileSystemWatcher`), registro.
- **Conversión de codepages:** `WcharMbcsConvertor` (`MISC/Common/Common.cpp`, usa `MultiByteToWideChar`) → **iconv** (glibc).
- **Cadenas:** `wchar_t`/`std::wstring` en todo el código → UTF-8 `std::string` en el núcleo y `QString` en la UI.
- Plugins (DLL + `NPPM_*`), impresión, `MiniDumper`, verificación de firmas: *fuera del MVP*.

Medida aproximada: `Buffer.cpp` tiene unos 63 puntos Win32 en 2 426 líneas (lógica muy aprovechable); `FindReplaceDlg.cpp`, unos 458 en 7 181 (casi todo UI); `EncodingMapper.cpp`, 0.

---

## 3. Alcance del MVP (y nada más)

| Funcionalidad | Referencia en N++ |
|---|---|
| Pestañas con varios archivos; nuevo, abrir, guardar, guardar como, guardar todo, cerrar | `DocTabView.*`, `NppIO.cpp:213/681/852/1139-2097`, `Notepad_plus.cpp:4842/5183` |
| Detección de encoding (BOM → uchardet → UTF-8/defecto) y conservación al guardar | `Buffer.cpp:1808, 2103-2232, 1453`, `Utf8_16.*`, `EncodingMapper.*` |
| Detección de EOL y conservación; conversión de EOL y de encoding desde menú | `Buffer.cpp:69`, `ScintillaEditView::setCRLF` |
| Resaltado con `langs.xml` + `stylers.xml` (+ temas con el mismo esquema) | `ScintillaEditView.cpp:99, 896, 1502-1640, 1750, 2326`, `Parameters.cpp:4457, 5276, 5980` |
| Buscar/reemplazar normal, extendido y regex (Boost) en el documento | `FindReplaceDlg.cpp:2943, 3156, 3312, 3435`, `boostregex/` |
| Buscar (y reemplazar) en archivos, con panel de resultados navegable | `Notepad_plus.cpp:1871, 1987, 2204`, `Buffer.cpp:2250`, `Finder` en `FindReplaceDlg.cpp:5702-6067` |
| Vista dividida: mover/clonar a la otra vista, girar orientación | `Notepad_plus.cpp:4732, 4776, 4958, 5099`, `SplitterContainer.*` |
| Restauración de sesión al reabrir (archivos, vista, pestaña activa, cursor, lenguaje, encoding) | `Parameters.h:123-172`, `Parameters.cpp:3416, 4830`, `NppIO.cpp:2483-2520, 3003`, `Notepad_plus.cpp:6509` |

**Explícitamente fuera:** plugins, macros, UDL, autocompletado, function list, paneles acoplables (salvo los resultados de búsqueda), impresión, diálogo de preferencias, modo oscuro propio, localización, backups/snapshots de no guardados, vigilancia de cambios externos, edición en columna avanzada, marcadores en sesión, actualizaciones.

---

## 4. Diseño propuesto para `./editor`

```
editor/
├─ CMakeLists.txt            (C++20, Ninja, -Wall -Wextra -Werror en código propio)
├─ COPYING                   (GPLv3) + AUTHORS / NOTICE con atribución a Notepad++, Scintilla, Lexilla, Boost, uchardet
├─ third_party/              (copiado de referencia/npp con su licencia; nunca se edita a mano)
│   ├─ scintilla/  (src, include, qt/ScintillaEditBase)
│   ├─ lexilla/
│   └─ boostregex/
├─ data/                     langs.model.xml, stylers.model.xml, themes/  → se instala en /usr/share/<app>/
├─ src/
│   ├─ core/      (sin Qt Widgets; testeable)  Document, DocumentManager, Encoding, Eol, LangRegistry, StyleSheet, Session, Search
│   ├─ config/    Paths (XDG: ~/.config/<app>/), ConfigStore
│   └─ ui/        MainWindow, EditorView (ScintillaEditBase), TabBar, SplitView, FindDialog, SearchResultsPanel
└─ tests/         QtTest con QT_QPA_PLATFORM=offscreen, fixtures/
```

Principio: `core/` replica el modelo de N++. Un `DocumentManager` es dueño de los documentos de Scintilla, y cada vista hace swap con `SCI_SETDOCPOINTER`. Los documentos se crean con una vista auxiliar invisible ("scratch"), como en N++. La UI solo traduce eventos de Qt.

Dependencias en Arch: `qt6-base qt6-5compat cmake ninja gcc`. Boost.Regex, pugixml y uchardet (la copia de N++, para detectar igual) van incluidos; iconv es de glibc; los tests usan QtTest.

---

## 5. Plan por fases con criterios de aceptación

Cada fase termina con: build limpio, `ctest` en verde y una demo manual corta. Comando de referencia para todas:

```sh
cmake -S editor -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

### Fase 0 — Esqueleto y toolchain
**Contenido:** CMake; librerías estáticas `scintilla_qt` (ScintillaEditBase con `-DSCI_OWNREGEX -DBOOST_REGEX_STANDALONE` + boostregex) y `lexilla`; ventana `QMainWindow` con un editor; COPYING/NOTICE; QtTest conectado a CTest. **✅ Completada, ver [fase0.md](fase0.md).**
**Aceptación:**
- [ ] El build compila desde cero en Arch sin warnings en `src/` (los de `third_party/` se silencian).
- [ ] `./build/editor` abre una ventana con un editor funcional (escribir, deshacer, copiar/pegar, IME en Wayland y X11).
- [ ] Test: `CreateLexer("cpp")`, `CreateLexer("searchResult")` y `CreateLexer("user")` devuelven no nulo.
- [ ] Test: buscar con regex `(?<=a)b` en `"ab"` encuentra la posición 1. El lookbehind no lo soporta el motor nativo de Scintilla, así que esto **demuestra que Boost está enganchado**.
- [ ] Test: `SCI_GETCODEPAGE == SC_CP_UTF8` por defecto.

### Fase 1 — Núcleo de documentos (sin UI)
**Contenido:** `Paths` (XDG), `Document`/`DocumentManager` (port de `FileManager`/`Buffer`: carga por bloques, metadatos, notificaciones por máscara), `Encoding` (BOM + uchardet + validación UTF-8 + iconv, tabla de nombres a partir de `EncodingMapper`), `Eol`. **✅ Completada, ver [fase1.md](fase1.md).**
**Aceptación:**
- [ ] Fixtures en `tests/fixtures/encoding/`: UTF-8, UTF-8 con BOM, UTF-16 LE/BE con BOM, Windows-1252, ISO-8859-15, Shift_JIS, GB18030, ASCII puro y archivo vacío. Para cada uno, la detección coincide con una tabla esperada y **cargar + guardar sin editar da un archivo idéntico byte a byte** (`cmp`).
- [ ] Fixtures con CRLF, LF, CR y mixto: el EOL detectado sigue la regla del "primero que aparece" de `getEOLFormatForm`, y el guardado lo conserva.
- [ ] Convertir EOL y convertir encoding (p. ej. 1252 → UTF-8) da la salida esperada.
- [ ] Un archivo de 100 MB carga en < 2 s en release (medido en el test, marcado `[.perf]`).
- [ ] Una ruta inexistente o sin permisos devuelve error, no excepción ni crash.

### Fase 2 — Pestañas, abrir/guardar en la UI
**Contenido:** `MainWindow`, `EditorView` con swap de documentos, `TabBar` con `BufferID` por pestaña, menú Archivo, menús Codificación y Fin de línea, barra de estado (lenguaje, encoding, EOL, línea:columna), arrastrar y soltar, archivos por línea de comandos. **✅ Completada, ver [fase2.md](fase2.md).**
**Aceptación (QtTest offscreen salvo lo indicado):**
- [ ] Abrir 3 archivos crea 3 pestañas; abrir uno que ya está abierto activa su pestaña sin duplicarla.
- [ ] Abrir un archivo con solo el "new 1" vacío abierto lo sustituye (comportamiento de `loadBufferIntoView`).
- [ ] Editar marca la pestaña como modificada; guardar la desmarca; **el undo de cada documento se conserva al cambiar de pestaña**; el cursor y el scroll se restauran por pestaña.
- [ ] Cerrar un documento modificado pregunta Guardar / No / Cancelar; Cancelar no cierra.
- [ ] La barra de estado muestra encoding y EOL correctos para los fixtures de la Fase 1.
- [ ] Manual: `./build/editor a.txt b.cpp` abre ambos; soltar archivos desde el gestor de archivos los abre.

### Fase 3 — Resaltado de sintaxis
**Contenido:** `LangRegistry` (parseo de `langs.xml`), `StyleSheet` (parseo de `stylers.xml`/tema, colores RGB, global override), tabla `_langNameInfoArray` portada, tabla de configuración por lenguaje (máscaras `LIST_*`, substyles, propiedades especiales de cpp/html/xml/js/php/json/ts), detección por extensión y por contenido, menú Lenguaje, selección de tema. En el primer arranque se copian los `*.model.xml` a `~/.config/<app>/`, como hace N++. **✅ Completada, ver [fase3.md](fase3.md).**
**Aceptación:**
- [ ] Test: los 96 `<Language>` de `langs.model.xml` y los 93 `<LexerType>` de `stylers.model.xml` se parsean sin errores; cada lenguaje tiene lexer (`CreateLexer` no nulo) o está en una lista documentada de no soportados.
- [ ] Test: en `int main() { return 0; }` con lenguaje C++, el estilo en `int` es el `styleID` de "TYPE WORD" o "INSTRUCTION WORD" según dónde esté la palabra en `langs.xml`, y su color de primer plano coincide con el de `stylers.xml`.
- [ ] Test: `foo.py` → Python, `Makefile` → Makefile, un archivo sin extensión con `#!/bin/bash` → Bash, `<?xml` → XML.
- [ ] Test: cambiar un color en `~/.config/<app>/stylers.xml` y recargar cambia el color en el editor.
- [ ] Manual: cargar `themes/Deep Black.xml` cambia todo el esquema; la fuente "Courier New" cae a una monoespaciada del sistema si no está instalada.

### Fase 4 — Buscar / reemplazar
**Contenido:** diálogo con pestañas Buscar / Reemplazar / Buscar en archivos (no modal, como en N++); modos normal, extendido y regex; mayúsculas, palabra completa, ciclo, dirección, ". incluye salto de línea"; reemplazar con referencias (`$1`, `\1`); reemplazar todo, contar. Buscar en archivos: directorio, filtros (`*.cpp *.h`, exclusiones `!*.min.js`), recursivo, ocultos; panel de resultados (lexer `searchResult`) con doble clic para ir al resultado; reemplazar en archivos. Mejora sobre N++: la búsqueda en archivos corre en un **hilo de trabajo** con su propio documento Scintilla sin vista, cancelable, y produce los mismos resultados. **✅ Completada, ver [fase4.md](fase4.md).**
**Aceptación:**
- [ ] Tabla de casos regex (≥ 25) que cubren lookbehind, `\R`, `^`/`$` por línea, `(?s)` y `(?-s)`, recursión `(?R)`, coincidencias vacías (`^`, `\b`) sin bucles infinitos, y backrefs en el reemplazo. Los resultados esperados se toman del comportamiento de N++ (las flags de `processFindNext`).
- [ ] Modo extendido: `\t`, `\n`, `\r`, `\0`, `\x41`, `é` se convierten como en `Searching::convertExtendedToString`.
- [ ] Buscar en archivos sobre `tests/fixtures/tree/` (anidado, con ocultos, binarios y varios encodings) da **exactamente** la lista esperada de (archivo, línea, texto), con y sin recursión ni ocultos.
- [ ] Cancelar a mitad de una búsqueda sobre 10 000 archivos termina en < 500 ms y la UI no se bloquea.
- [ ] Reemplazar en archivos conserva el encoding, el BOM y el EOL de cada archivo (se reutiliza el guardado de la Fase 1), comprobado con `cmp` contra los esperados.

### Fase 5 — Vista dividida y sesión
**Contenido:** `QSplitter` con dos parejas pestañas+editor; mover y clonar a la otra vista; girar la orientación; foco y vista activa; `session.xml` con el esquema de N++ (subconjunto), escrito al salir con copia de seguridad y leído al arrancar. **✅ Completada, ver [fase5.md](fase5.md).**
**Aceptación:**
- [ ] Clonar un documento: editar en una vista se refleja en la otra al instante, y cada vista mantiene su propio cursor.
- [ ] Mover la última pestaña de la vista secundaria la oculta (como `hideView`).
- [ ] Test de ida y vuelta: 2 archivos en la principal, 1 en la secundaria y 1 clonado, con cursores y selecciones distintos y lenguaje forzado en uno → guardar sesión → destruir y recrear `MainWindow` → estado **idéntico** (archivos y orden por vista, pestaña y vista activas, `startPos`/`endPos`/`firstVisibleLine`, lenguaje y encoding).
- [ ] Un archivo de la sesión que ya no existe se omite sin error; un `session.xml` corrupto recurre al `.bak`.
- [ ] Test: un `session.xml` escrito por Notepad++ (fixture) se carga con las rutas traducidas, al menos los campos del subconjunto.

### Fases posteriores al MVP

Pedidas después de terminar el MVP; cada una con el mismo método (estudiar el código de N++, criterios con tests, build sin warnings y un documento).

| Fase | Contenido | Estado |
|---|---|---|
| 6 — Seguridad de los datos | Cambios externos (`checkFileState`), copias "snapshot" de los cambios sin guardar con restauración en la sesión, copia al guardar (`.bak`) | ✅ [fase6.md](fase6.md) |
| 7 — Preferencias | Diálogo de preferencias guardado en `config.xml` con los nombres de N++ | ✅ [fase7.md](fase7.md) |
| 8 — Autocompletado y macros | Palabras del documento, `APIs/*.xml` y call tips; grabar/reproducir/guardar macros en `shortcuts.xml` | ✅ [fase8.md](fase8.md) |
| 9 — Impresión | Imprimir y exportar a PDF con los colores del tema (`SCI_FORMATRANGEFULL`) | ✅ [fase9.md](fase9.md) |
| 10 — Lenguajes del usuario | Cargar `userDefineLang.xml` de N++ con `LexUser` y diálogo para editarlos | ✅ [fase10.md](fase10.md) |
| 11 — Plugins | API nativa en C para `.so` (los DLL de Windows no pueden cargarse), menú Plugins y un plugin de ejemplo | ✅ [fase11.md](fase11.md) |
| 12 — Edición | Órdenes de edición de N++ que faltaban: líneas (ordenar, duplicadas, vacías), mayúsculas, comentarios, espacios/tabulaciones, marcadores, historial de cambios, etiquetas XML parejas, estilos de token, plegado y zoom | ✅ [fase12.md](fase12.md) |
| 13 — Python Script | Plugin de scripting con la API del PythonScript de N++ (CPython integrado, `editor` generado desde `Scintilla.iface`, consola acoplable, callbacks e hilos como el original) | ✅ [fase13.md](fase13.md) |
| 14 — Administrador de plugins | Plugins Admin de N++: lista `nppPluginList.json`, pestañas, instalar/actualizar/quitar/desactivar con SHA-256 y libzip; la lista oficial de N++ se consulta como incompatible | ✅ [fase14.md](fase14.md) |
| 15 — Plugins de N++ desde su código | Recetas que descargan el código original, lo verifican, le aplican el port (capa de compatibilidad Win32, parches, overlay), lo compilan y lo reempaquetan en local; MIME Tools y XML Tools | ✅ [fase15.md](fase15.md) |
| 16 — Internacionalización | Código en inglés y traducción completa al español con el sistema de Qt (plurales, traducciones de Qt), idioma del sistema o elegido en Preferencias, `NPPM_GETNATIVELANGFILENAME` para los plugins | ✅ [fase16.md](fase16.md) |
| 17 — Interfaz moderna | Fusion con paletas y hoja de estilos propias, modo claro/oscuro (DarkMode de N++) siguiendo al sistema con un tema por modo, barra de herramientas de N++ con iconos propios, pestañas planas, letra del sistema | ✅ [fase17.md](fase17.md) |
| 18 — Una sola instancia | Repositorio git; la segunda instancia pasa los archivos a la primera por un socket local (`-multiInst` y la preferencia de N++ para abrir otra) | ✅ [fase18.md](fase18.md) |
| 19 — Configurador de estilos | `WordStyleDlg`: tema, lenguaje, estilo, colores, fuente, Global override (ahora en `config.xml`), extensiones y palabras del usuario; vista previa y guardar el tema | ✅ [fase19.md](fase19.md) |
| 20 — Asignación de atajos | Shortcut Mapper: menú (por `IDM_*`), macros, plugins y 88 órdenes de Scintilla; `shortcuts.xml` de N++ | ✅ [fase20.md](fase20.md) |
| 21 — Editor de columnas | `ColumnEditorDlg` (Alt+C): texto o números en columna, con selección rectangular, varias selecciones o desde el cursor | ✅ [fase21.md](fase21.md) |
| 22 — Paneles | Lista de documentos, Mapa del documento, Lista de funciones (port de `functionParser.cpp` y los XML de N++) y Explorador de carpetas (en `session.xml`) | ✅ [fase22.md](fase22.md) |
| 23 — Temas de Edita | «Edita Light» y «Edita Dark», generados de `stylers.model.xml`, como temas de serie | ✅ [fase23.md](fase23.md) |
| 24 — AppImage portable | Generado en Ubuntu 22.04 (glibc 2.35) con Qt 6.8 oficial, con `unshare` + `chroot` y sin Docker | ✅ [fase24.md](fase24.md) |
| 25 — XML Tools completo | Las 29 órdenes del original: errores (anotaciones y navegación), cierre de etiquetas, tipo XML automático, XXE, archivos enormes, todos los archivos, tokenizar, opciones y depuración | ✅ [fase25.md](fase25.md) |
| 26 — MarkdownViewer++ | Port nativo del plugin de N++ (C#/.NET): panel con la vista previa, sincronización, exportar a HTML y PDF, imprimir, portapapeles, opciones en su `.ini`; md4c en `third_party` | ✅ [fase26.md](fase26.md) |
| 27 — Paleta de órdenes | Órdenes (`>`), archivos (abiertos, recientes y carpetas de trabajo en otro hilo), símbolos (`@`) y líneas (`:`), con búsqueda difusa sin acentos; no existe en N++ | ✅ [fase27.md](fase27.md) |
| 28 — Git y marcas en la barra | Diferencias con HEAD (Myers) en un margen, con aviso de las líneas originales y navegación; marcas de Git, marcadores, búsquedas y cursor en la barra de desplazamiento; no existe en N++ | ✅ [fase28.md](fase28.md) |
| 29 — Mejoras visuales | Iconos por tipo de archivo, vista previa de colores, número de línea y guía del bloque actuales, seis temas modernos, barra de avisos; no existen en N++ | ✅ [fase29.md](fase29.md) |
| 30 — Página de inicio | Mientras el único documento es uno nuevo y vacío: empezar, recientes, apariencia y atajos; escribir pasa al documento; no existe en N++ | ✅ [fase30.md](fase30.md) |
| 31 — Barra de estado y paréntesis | Campos que se pulsan con listas con buscador, sangría por documento, paréntesis por colores, fuentes de kdeglobals cuando Qt no las entiende | ✅ [fase31.md](fase31.md) |
| 32 — Ruta de navegación y encabezado fijo | Barra con carpetas, archivo y símbolos (Lista de funciones y plegado); encabezados de los bloques que contienen a la primera línea visible, dibujados con los estilos del editor | ✅ [fase32.md](fase32.md) |
| 33 — Mapa del documento mejorado y buscar en línea | Margen de cambios de Git y marcadores en el mapa, búsquedas con relleno, zona visible con veladura; barra flotante de búsqueda con el mismo motor que el diálogo | ✅ [fase33.md](fase33.md) |
| 34 — Probador de expresiones regulares | Panel con texto de prueba, coincidencias y grupos (posiciones guardadas por la regex Boost), vista previa del reemplazo y referencia rápida | ✅ [fase34.md](fase34.md) |
| 35 — Terminal integrada | libvterm (MIT) para la emulación, forkpty para la shell, vista propia con historial, selección, ratón y tema del editor; panel con pestañas | ✅ [fase35.md](fase35.md) |
| 36 — Servidores de lenguaje (1) | Cliente LSP 3.17 (JSON-RPC, cola, UTF-8/UTF-16), 27 servidores conocidos, sincronización, diagnósticos, panel Problemas, autocompletado con fragmentos y hover | ✅ [fase36.md](fase36.md) |
| 37 — Servidores de lenguaje (2) | Definición, referencias en el panel de resultados, renombrar (WorkspaceEdit), formato y ayuda de parámetros | ✅ [fase37.md](fase37.md) |
| 38 — macOS | Código portable (forkpty/proc_pidinfo, ⌘ y Control en la terminal, rutas del .app, QFileOpenEvent), paquete Edita.app con Info.plist, .dmg universal con GitHub Actions | ✅ [fase38.md](fase38.md) |
| 39 — Comparar | Myers por líneas y por caracteres, alineación con anotaciones, marcadores de fondo, desplazamiento sincronizado; con la otra vista, lo guardado o HEAD | ✅ [fase39.md](fase39.md) |
| 40 — Git en el explorador y blame | git status --porcelain en segundo plano con estados propagados a carpetas; git blame --contents con anotación de fin de línea, margen de texto por antigüedad y git show | ✅ [fase40.md](fase40.md) |
| 41 — Fragmentos | Sintaxis de VS Code/LSP, archivos JSON de VS Code (de Edita y del usuario), Tab con marcadores y espejos en varias selecciones, variables, lista para insertar, también para LSP | ✅ [fase41.md](fase41.md) |
| 42 — Servidores de lenguaje (3) | codeAction con resolve y executeCommand, bombilla en el margen, workspace/symbol mientras se escribe, inlayHint en anotaciones de fin de línea compartidas con blame | ✅ [fase42.md](fase42.md) |
| 43 — Manual de usuario (1) | Visor dentro de la aplicación (F1: índice, búsqueda sin acentos, historial, colores del tema), capítulos en Markdown en inglés y español con las órdenes y atajos reales, capítulos 1-5 | ✅ [fase43.md](fase43.md) |
| 44 — Manual de usuario (2) | Capítulos 6-14 y 16 en inglés y español: lenguajes y temas, autocompletado y fragmentos, servidores de lenguaje, Git, comparar, terminal, macros, plugins, preferencias y atajos, solución de problemas | ✅ [fase44.md](fase44.md) |
| 45 — Manual de usuario (3) | Capturas automáticas en los dos idiomas, tabla de atajos generada desde los menús, prueba de que cada orden de los menús está en el manual; corrige el choque de los marcadores de comparar y la bombilla con los del historial de cambios | ✅ [fase45.md](fase45.md) |
| 46 — Pestaña Marcar | Marcar todo con «Find Mark Style», marcador en la línea, borrar en cada búsqueda, en la selección, borrar marcas, copiar el texto marcado; Buscar › Marcar… (Ctrl+M) y Copiar el texto con estilo | ✅ [fase46.md](fase46.md) |
| 47 — Sesiones con nombre | Archivo › Cargar una sesión / Guardar la sesión (con las carpetas del explorador), extensión de las sesiones y --openSession | ✅ [fase47.md](fase47.md) |
| 48 — Menú Ejecutar | Ejecutar… (F5) con variables $(…), órdenes guardadas con nombre y atajo en shortcuts.xml (<UserDefinedCommands>), las tres de N++ y su pestaña en la Asignación de atajos | ✅ [fase48.md](fase48.md) |
| 49 — Ocultar líneas | Ver › Ocultar líneas (Alt+H) con las marcas 18/19 de N++, unir secciones, mostrar con un clic, restaurar al cambiar de documento y al desplegar | ✅ [fase49.md](fase49.md) |
| 50 — Menú Ventana | Ordenar las pestañas (nombre, ruta, tipo, tamaño, fecha), la lista de documentos de la vista en el menú y el diálogo Ventanas (activar, guardar, cerrar varios, ordenar las pestañas) | ✅ [fase50.md](fase50.md) |

---

## 6. Riesgos y cómo se mitigan

| Riesgo | Mitigación |
|---|---|
| `ScintillaEditBase` con Qt 6.12 y `core5compat` (usa `QTextCodec` en `PlatQt.cpp`) | Se valida en la Fase 0. Si falla, un parche mínimo local documentado en `third_party/PATCHES.md` |
| IME / métodos de entrada en Wayland | Prueba manual en la Fase 0 (KDE/GNOME); `ScintillaQt` implementa `inputMethodEvent` |
| Codepages de Windows ↔ iconv | Tabla explícita derivada de `EncodingMapper.cpp` con test por cada entrada |
| Fuentes de Windows en `stylers.xml` (Courier New, Consolas) | Alias a `monospace` vía `QFontDatabase` si no existen (equivale a `isInFontList`) |
| `wchar_t` en todo el código de N++ | Al portar: UTF-8 en el núcleo, `QString` solo en la UI |
| Licencias | GPLv3 en el proyecto; se conservan las cabeceras de copyright de los archivos derivados, y NOTICE con Scintilla/Lexilla (licencia tipo HPND), Boost (BSL-1.0), uchardet (MPL-1.1/GPL/LGPL) y pugixml (MIT) |

---

## 7. Decisiones (tomadas al iniciar la Fase 0)

1. **Componente de edición:** Scintilla 5 `ScintillaEditBase` + Lexilla + boostregex de la referencia.
2. **Nombre:** `Edita` (binario `edita`, configuración en `~/.config/edita/`, configurable con `EDITA_APP_ID`).
3. **`session.xml`:** compatible con el esquema de Notepad++.
4. **Tests:** QtTest en lugar de Catch2 (sin dependencias extra). pugixml se toma de la copia de N++.

Detalle y hallazgos en [fase0.md](fase0.md).
