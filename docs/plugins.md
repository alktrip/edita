# Escribir (o portar) un plugin para Edita

Los plugins de Edita son bibliotecas compartidas (`.so`) con la **misma estructura que los de Notepad++**: las mismas funciones exportadas, los mismos `FuncItem`/`ShortcutKey` y los mismos números de mensaje `NPPM_*` y de notificación `NPPN_*`. Un plugin de N++ no se puede cargar tal cual (es una DLL de Windows que llama a la API Win32), pero portar la parte que habla con el editor es casi mecánico.

La cabecera es [`plugin-sdk/edita_plugin.h`](../plugin-sdk/edita_plugin.h). Se instala en `<prefijo>/include/edita/` junto con `Scintilla.h` y `Sci_Position.h`.

## Dónde se buscan

En este orden; si dos plugins tienen el mismo nombre, gana el primero que se encuentra:

1. Las carpetas de `$EDITA_PLUGIN_PATH`, separadas por `:`.
2. `~/.config/edita/plugins/`.
3. `plugins/` junto al ejecutable (build de desarrollo).
4. `<prefijo>/lib/edita/plugins/` (instalado).

Cada plugin va en una carpeta con su nombre: `plugins/MiPlugin/MiPlugin.so`. Si uno no se puede cargar, se dice por qué en **Plugins > Información de los plugins…**.

## Funciones que hay que exportar

```c
#include <edita_plugin.h>

EDITA_PLUGIN_EXPORT int edita_plugin_api_version(void) { return EDITA_PLUGIN_API_VERSION; }
EDITA_PLUGIN_EXPORT void setInfo(EditaData data);              // se llama la primera
EDITA_PLUGIN_EXPORT const char* getName(void);                 // nombre del submenú (UTF-8)
EDITA_PLUGIN_EXPORT EditaFuncItem* getFuncsArray(int* count);  // órdenes
EDITA_PLUGIN_EXPORT void beNotified(SCNotification* n);        // opcional: NPPN_* y SCN_*
EDITA_PLUGIN_EXPORT intptr_t messageProc(unsigned int m, uintptr_t w, intptr_t l); // opcional
```

`edita_plugin_api_version` no existe en N++. Si un plugin se compiló para otra versión de la API, Edita no lo carga.

Opcional: `EDITA_PLUGIN_EXPORT const char* edita_plugin_version(void)` devuelve la versión del plugin (`"1.2.3"`). Es lo que en N++ es el recurso de versión de la DLL, y el administrador de plugins la usa para ofrecer actualizaciones.

## Distribuirlo con el administrador de plugins

**Plugins > Administrador de plugins…** instala paquetes `.zip` desde una lista con el formato de `nppPluginList.json` de N++ (ver [fase14.md](fase14.md)). El paquete lleva `MiPlugin.so` en la raíz o dentro de `MiPlugin/`. En la lista, `id` es el `sha256sum` del `.zip` y `repository` su URL.

**El catálogo de Edita.** El administrador descarga además la lista firmada del [catálogo de plugins de Edita](https://github.com/alktrip/edita-plugins) (ver [fase55.md](fase55.md) y [fase56.md](fase56.md)). Para que un plugin esté en él, basta un *pull request* con su receta en `recipes/<Carpeta>/`, con su `license` y su `check`. La CI lo compila y lo prueba, y al aceptarse lo publica. No hace falta publicar una versión de Edita.

Con estas variables se puede usar otro catálogo (uno propio, o uno local para probar):

- `EDITA_PLUGIN_CATALOG_URL`: la URL o la ruta de su `plugins.json`. La firma se busca en la misma URL con `.minisig`. Con `none`, Edita no usa ningún catálogo.
- `EDITA_PLUGIN_CATALOG_PUBKEY`: la clave pública de minisign con la que se comprueba la firma.

Con `catalog.py keygen` de edita-plugins se crea una clave.

## Compilar un plugin de Notepad++ desde su código (recetas)

Muchos plugins de N++ tienen el código en GitHub. Una **receta** le dice al administrador de plugins cómo descargar ese código, adaptarlo y compilarlo en el equipo del usuario, sin publicar ningún binario (ver [fase15.md](fase15.md)). Se ponen en `~/.config/edita/plugins/recipes/<Carpeta>/` (o en `share/edita/plugins/recipes/`):

```
MiPlugin/
  recipe.json            de dónde sale el código, su SHA-256 y cómo compilarlo
  overlay/edita/CMakeLists.txt
  algo.patch             opcional: cambios al original (patch -p1)
```

```json
{ "folder-name": "MiPlugin", "display-name": "Mi plugin", "version": "1.2",
  "description": "…", "author": "…", "homepage": "…",
  "source": { "url": "https://github.com/autor/miplugin/archive/refs/tags/v1.2.zip",
              "sha256": "<sha256sum del zip>", "strip-components": 1 },
  "remove": ["src/Scintilla.h", "src/Sci_Position.h"],
  "patches": ["algo.patch"], "overlay": "overlay", "build-dir": "edita",
  "requires": ["cmake", "c++"] }
```

Campos opcionales:

- **`"extra-sources"`**: más código, que se copia dentro del del autor antes de `remove` y de los parches. Son, por ejemplo, los submódulos de git, que GitHub no incluye en el `.zip` de una etiqueta. Cada fuente tiene `url`, `sha256`, `strip-components` y `dest` (una carpeta dentro del código, sin `..`):
  ```json
  "extra-sources": [{ "url": "https://github.com/npp-plugins/plugintemplate/archive/<commit>.zip",
                      "sha256": "…", "strip-components": 1, "dest": "plugintemplate" }]
  ```
- **`"license"`** y **`"license-files"`**: la licencia del plugin (identificador SPDX, para el catálogo) y los archivos del código que van en el paquete junto al `.so`. Si un archivo no está, la compilación falla.
- **`"check"`**: la prueba del paquete, que ejecuta `edita --check-plugin` (abajo). `commands` son órdenes que tienen que estar en el menú del plugin. Cada caso de `cases` pone un texto en un documento nuevo, lo selecciona entero (o deja el cursor al principio con `"select": "none"`), ejecuta la orden y compara el documento con `expect`, o el archivo abierto con `expect-file`:
  ```json
  "check": { "commands": ["Selection", "Document"],
             "cases": [{ "command": "Selection", "text": "a\nb\nc", "expect": "c\nb\na" },
                       { "command": "Open", "expect-file": "/etc/hosts" }] }
  ```

**Desde la línea de órdenes**, sin abrir la interfaz:

```sh
edita --build-recipe recipes/MiPlugin/recipe.json --output paquetes/
edita --check-plugin paquetes/MiPlugin-1.2.zip --recipe recipes/MiPlugin/recipe.json
```

`--build-recipe` descarga el código y sus fuentes adicionales, comprueba sus SHA-256 y compila la receta. Las rutas locales de `url` son relativas a la receta. Deja `<carpeta>-<versión>.zip` y su `.sha256` en la carpeta de salida, y la compilación en `work/`. `--check-plugin` instala el paquete en una configuración temporal (no toca la del usuario), abre Edita sin ventana visible, carga el plugin y ejecuta la prueba de la receta. Los diálogos modales que abra el plugin se aceptan solos, y su texto sale en la salida (`dialog: …`). Las dos terminan con código 0 si todo va bien. Las recetas del catálogo de plugins de Edita están en [alktrip/edita-plugins](https://github.com/alktrip/edita-plugins).

**Paquetes reproducibles.** El mismo código, compilado con el mismo SDK y el mismo compilador, da el mismo `.so` en cualquier carpeta: el SDK cambia las rutas de trabajo dentro del binario por nombres fijos (`-ffile-prefix-map`) y no le pone RUNPATH (el plugin lo carga Edita, que ya tiene Qt). Con `SOURCE_DATE_EPOCH`, todas las entradas del `.zip` llevan esa fecha (en hora local, así que también hace falta la misma `TZ`), y el paquete sale idéntico byte a byte. Así la CI del catálogo sabe si un plugin cambió, y cualquiera puede comprobar un binario publicado: `scripts/build-all.sh` de edita-plugins, con el AppImage de Edita con el que se compiló el catálogo y el mismo compilador (Ubuntu 22.04 con GCC 12), da los mismos SHA-256 que la lista.

El `CMakeLists.txt` del overlay compila los fuentes originales con la capa de compatibilidad Win32 del SDK:

```cmake
cmake_minimum_required(VERSION 3.20)
project(MiPlugin CXX)
include(${EDITA_PLUGIN_SDK}/cmake/EditaNppPlugin.cmake)
edita_npp_plugin(MiPlugin UNICODE
    SOURCES ../src/plugin.cpp ../src/otro.cpp
    RESOURCES ../src/plugin.rc
    INCLUDES ../src)
```

**`RESOURCES`** son los `.rc` del plugin. Al compilar, `edita_rc` (una herramienta del SDK, en `tools/`) los convierte a un `.cpp` con sus diálogos (`DIALOG`, `DIALOGEX`), menús (`MENU`, `MENUEX`) y cadenas (`STRINGTABLE`), en el formato binario de Windows. Tiene su propio preprocesador, con las reglas de `rc.exe`:

- `#include`, `#define`, `#if` y compañía;
- de los archivos incluidos que no son `.rc`, como `resource.h`, solo lee las directivas;
- no lee las cabeceras de Windows (`windows.h`, `winres.h`, `afxres.h`): ya conoce sus estilos;
- lee los `.rc` en UTF-16, en UTF-8 y en ANSI (Windows-1252);
- si un recurso está en varios idiomas, se queda con el neutro o el inglés;
- se salta los iconos, los mapas de bits y `VERSIONINFO`.

**`UNICODE`** compila como los plugins de N++ desde la 8.0: `TCHAR` es `wchar_t`, y también lo son `getName()`, `FuncItem._itemName` y las cadenas de los mensajes `NPPM_*`. Es lo que necesita casi cualquier plugin actual. Sin `UNICODE`, `TCHAR` es `char` en UTF-8, como en los plugins antiguos (MIME Tools 3.1). En Linux y macOS `wchar_t` mide 4 bytes y no 2. El código que usa las funciones de Win32 (`MultiByteToWideChar`, `lstrlen`…) funciona igual. El que da por hecho UTF-16 (`sizeof(wchar_t) == 2`, pares sustitutos) necesita un parche en la receta.

`compat/windows.h` y compañía (`tchar.h`, `shlwapi.h`, `strsafe.h`, `commdlg.h`…) hacen que compile el código que usa:

- **la API de plugins:** `NppData`, `FuncItem`, `DllMain` (recibe el `HINSTANCE` del plugin) y `::SendMessage` a N++ y a Scintilla. En modo `UNICODE`, la capa convierte a UTF-8 los nombres, las órdenes y los mensajes con cadenas: rutas, sesiones, barra de estado, `NPPM_MSGTOPLUGIN` (`"Otro.dll"` ↔ `"Otro.so"`)… Los `cmdID` que da Edita aparecen en la tabla del plugin;
- **archivos:** `CreateFile`, `ReadFile`, `WriteFile`, `FindFirstFile` (sin distinguir mayúsculas), `GetFileAttributes`, copiar, mover, borrar y directorios. Las rutas pueden usar `\`;
- **archivos `.ini`:** `GetPrivateProfileString`/`Int`/`Section`/`SectionNames` y `WritePrivateProfileString`, en UTF-8. Leen también los `.ini` en UTF-16 que deja N++ y los conservan en UTF-16;
- **texto:** `MultiByteToWideChar` y `WideCharToMultiByte` (las páginas de código del sistema son UTF-8; 1252 es Latin-1), las funciones `lstr*`, `CharUpper` y `CharLower`;
- **`printf` de MSVC:** en `swprintf`, `wsprintf`, `_snwprintf`, `swprintf_s`, `StringCchPrintf`…, `%s` es `wchar_t` y `%S` es `char`, como en Windows (en C es al revés); `%I64d` también funciona;
- **el CRT de MSVC:** `wcscpy_s` y compañía (también con arreglos: `wcscpy_s(buf, L"…")`), `_itow`, `_wtoi`, `_wfopen` (con `ccs=UTF-8`), `_wcsicmp`…;
- **rutas (`shlwapi`):** `PathAppend`, `PathCombine`, `PathFindFileName`, `PathRemoveFileSpec`, `PathFileExists`… Aceptan `\` y `/`; los separadores que añaden son `/`;
- **módulos y sistema:** `GetModuleFileName` (con el `HINSTANCE` del plugin, su `.so`; con `NULL`, Edita), `GetEnvironmentVariable`, `ExpandEnvironmentStrings`, `SHGetFolderPath` (`CSIDL_APPDATA` es `~/.config`), `GetTickCount`, `GetLocalTime`, `Sleep`, `GlobalAlloc`/`LocalAlloc`;
- **interfaz sin ventanas:** `MessageBox` (Qt), el portapapeles (`OpenClipboard`, `SetClipboardData` y `GetClipboardData` con `CF_UNICODETEXT`, `CF_TEXT` y formatos registrados), `GetKeyState` (Mayús, Ctrl, Alt) y `ShellExecute` (abre archivos y direcciones con la aplicación del sistema o lanza un programa).

Antes de compilar, el administrador corrige los `#include` que solo funcionan en Windows: `<Windows.h>`, `"..\PluginInterface.h"`, `"Resource.H"`. Un `#include` se reescribe solo si en Linux no encuentra su archivo. Se desactiva con `"fix-includes": false` en la receta.

**Ventanas** (`compat/npp_compat_user.h`, `npp_window.cpp`). Un `HWND` de la capa es una ventana con su `QWidget`, su procedimiento y sus datos, y los mensajes llegan como en Windows: `SendMessage` llama al procedimiento, que deja lo que no trata a `DefWindowProc`.

- **Controles estándar:** `BUTTON` (botón, casilla, opción, grupo), `EDIT` (de una línea y multilínea, con `\r\n`), `STATIC`, `LISTBOX` y `COMBOBOX`, con sus mensajes (`BM_*`, `EM_*`, `LB_*`, `CB_*`). Sus notificaciones llegan al padre como `WM_COMMAND`: `BN_CLICKED`, `EN_CHANGE`, `LBN_SELCHANGE` (con `LBS_NOTIFY`), `CBN_SELCHANGE`… Como en Windows, los cambios que hace el plugin no notifican.
- **Otras clases:** `"Scintilla"` es un editor de Edita (le llegan los `SCI_*`). Las clases registradas (`RegisterClass`) reciben el ratón, el teclado y `WM_SIZE`. El *subclassing* (`SetWindowLongPtr(GWLP_WNDPROC)`) ve también el teclado de los controles, y si no lo pasa a `CallWindowProc`, el control no lo procesa.
- **Diálogos:** `WM_INITDIALOG`, `DWLP_USER`/`DWLP_MSGRESULT`, `DefDlgProc`, Intro (el botón `BS_DEFPUSHBUTTON` o `IDOK`), Esc (`IDCANCEL`) y `EndDialog` (ver los recursos, abajo).
- **Plantilla de N++:** sus clases (`Window`, `StaticDialog`, `DockingDlgInterface`) compilan y funcionan sin cambios. `NPPM_DMMREGASDCKDLG` con un `tTbData` crea un panel de Edita con el `QWidget` del diálogo. Si el diálogo no tiene título, el panel lleva el nombre del plugin. Cerrar el panel envía `WM_NOTIFY` con `DMN_CLOSE`, y soltarlo o acoplarlo, `DMN_FLOAT`/`DMN_DOCK`. `NPPM_CREATESCINTILLAHANDLE` acepta una ventana de la capa como padre.
- **Lo demás:** geometría, visibilidad, foco, jerarquía (`GetParent`, `GetDlgItem`, `EnumChildWindows`), propiedades (`SetProp`), temporizadores (`SetTimer`), `PostMessage` en cola, menús emergentes (`CreatePopupMenu`, `TrackPopupMenu`), `CheckMenuItem` sobre `GetMenu(nppData._nppHandle)` (marca las órdenes del plugin), monitores, métricas del sistema, cursores del sistema, y una parte de GDI **sin dibujo**: fuentes (`CreateFont`, `WM_SETFONT`), medidas de texto (`GetTextExtentPoint32`), colores del sistema y DPI (siempre 96: las coordenadas son píxeles lógicos de Qt).
- **Hilos y sistema** (`npp_win32.cpp`): `CreateThread`, eventos, mutex, `WaitForSingleObject`, secciones críticas, `Interlocked*`, `FormatMessage`, `MulDiv`, `min`/`max` (como funciones) y los `SCI_GETTEXTRANGE`/`SCI_FINDTEXT` que N++ quitó de su `Scintilla.h`.

**Recursos** (`npp_window.cpp`). `FindResource`, `LoadResource`, `LockResource` y `SizeofResource` devuelven las plantillas binarias de los `.rc`.

- `CreateDialogParam` y `DialogBoxParam` (modal, hasta `EndDialog`) crean el diálogo con sus controles y le mandan `WM_INITDIALOG`. `CreateDialogIndirectParam` y `DialogBoxIndirectParam` hacen lo mismo con una plantilla en memoria (`DLGTEMPLATE` o `DLGTEMPLATEEX`).
- Las coordenadas de la plantilla son unidades de diálogo, que se convierten a píxeles con la fuente de Edita, como hace Windows con la del diálogo (`MapDialogRect`, `GetDialogBaseUnits`).
- Un control de una clase que la capa no tiene aparece como un recuadro con su nombre (`[SysListView32]`), con su id.
- `LoadString` y `LoadMenu` (y `LoadMenuIndirect`) usan las cadenas y los menús del `.rc`. Sin plantilla, `CreateDialogParam` crea un diálogo vacío con un aviso y `DialogBoxParam` avisa.

**Más de Windows:**

- `SetWindowSubclass`/`DefSubclassProc` (comctl32);
- los diálogos comunes sobre los de Qt: `GetOpenFileName` (también con selección múltiple) y `GetSaveFileName`, `SHBrowseForFolder` con `SHGetPathFromIDList` y `ChooseColor`. Son los del sistema o los de Qt, según la preferencia de diálogos de archivos de Edita;
- `CreateProcess` con las tuberías de `CreatePipe` (`STARTF_USESTDHANDLES`), `GetExitCodeProcess` y `TerminateProcess`. La línea de órdenes se divide con las reglas de Windows, y `cmd /c …` se ejecuta con `/bin/sh -c`;
- `GetMenuItemInfo`/`SetMenuItemInfo`/`InsertMenuItem`.

**Controles comunes** (`compat/npp_compat_comctl.h`). Cada uno es un widget de Qt con los mensajes y las notificaciones (`WM_NOTIFY`) de Windows:

- pestañas (`SysTabControl32`, sobre `QTabWidget`): `TCM_*` y `TabCtrl_*`; `TCN_SELCHANGING` todavía ve la pestaña anterior y, si el plugin devuelve `TRUE`, no cambia. `TCM_ADJUSTRECT` da el área de la página, donde el plugin coloca sus diálogos;
- listas con columnas (`SysListView32` en modo informe, sobre `QTreeWidget`): columnas, elementos y subelementos, `lParam`, selección y foco, casillas (`LVS_EX_CHECKBOXES`), buscar, ordenar con la función del plugin, `LVM_HITTEST`; notificaciones `LVN_ITEMCHANGED` (también cuando el cambio lo hace el plugin, como en Windows), `LVN_COLUMNCLICK`, `LVN_KEYDOWN`, `LVN_ITEMACTIVATE`, `LVN_DELETEITEM`, `NM_CLICK`, `NM_DBLCLK`, `NM_RCLICK` y `NM_RETURN`;
- árboles (`SysTreeView32`, sobre `QTreeWidget`): insertar (`TVI_FIRST`, `TVI_LAST`, `TVI_SORT`), navegar (`TVGN_*`), expandir, seleccionar, casillas y elementos con hijos por cargar (`cChildren = 1`, que el plugin rellena en `TVN_ITEMEXPANDING`); notificaciones `TVN_SELCHANGING`/`TVN_SELCHANGED` (con `TVC_BYMOUSE`, `TVC_BYKEYBOARD` o `TVC_UNKNOWN`), `TVN_ITEMEXPANDING`/`TVN_ITEMEXPANDED`, `TVN_KEYDOWN`, `TVN_DELETEITEM` y los `NM_*`. `TVM_EXPAND` no notifica, como en Windows;
- barras de progreso (`msctls_progress32`), deslizadores (`msctls_trackbar32`, con `WM_HSCROLL`/`WM_VSCROLL` y los códigos `TB_*`) y tooltips (`tooltips_class32`: el texto de cada herramienta es el `toolTip` de su ventana);
- las macros de `windowsx.h` (`Edit_SetText`, `ComboBox_AddString`, `Button_GetCheck`…).

Los controles comunes de los `.rc` (`CONTROL "", id, "SysListView32", …`) se crean igual.

**Bucles de mensajes propios.** `GetMessage`, `PeekMessage`, `DispatchMessage`, `TranslateMessage`, `PostQuitMessage`, `PostThreadMessage`, `WaitMessage` y `MsgWaitForMultipleObjects` funcionan sobre el bucle de eventos de Qt: mientras el plugin espera, la interfaz sigue viva y lo que llega por `PostMessage` se entrega. `GetMessage` devuelve los mensajes de la cola del hilo (`WM_QUIT`, `PostThreadMessage`) y, cada vez que Qt atiende algo, un `WM_NULL`, para que un bucle del tipo `while (!terminado && GetMessage(…))` vuelva a mirar su variable.

**Dibujo** (GDI sobre `QPainter`). Las clases registradas y los diálogos reciben `WM_ERASEBKGND` (con el pincel de la clase, `hbrBackground`) y `WM_PAINT`; `BeginPaint` devuelve un DC que dibuja en el widget. Hay `FillRect`, `FrameRect`, `Rectangle`, `RoundRect`, `Ellipse`, `MoveToEx`/`LineTo`, `Polyline`, `Polygon`, `SetPixel`, `TextOut`, `ExtTextOut`, `DrawText` (con `DT_CALCRECT`, la alineación, el ajuste de línea y los puntos suspensivos), `DrawEdge`, `DrawFocusRect`, plumas con estilo y grosor, pinceles (también `(HBRUSH)(COLOR_xxx + 1)`), `SaveDC`/`RestoreDC`, `IntersectClipRect` y los DC en memoria (`CreateCompatibleDC`, `CreateCompatibleBitmap`) con `BitBlt` y `StretchBlt`, para el doble búfer. Las coordenadas son las de GDI: los rectángulos no incluyen el borde derecho ni el de abajo. `InvalidateRect` pide repintar y `UpdateWindow` repinta en el acto. Fuera de `WM_PAINT`, un DC (`GetDC`) mide texto pero no dibuja. También están las funciones de `RECT` (`PtInRect`, `IntersectRect`, `InflateRect`…).

**Todavía no hay:** el dibujo propio de los controles (`NM_CUSTOMDRAW`, `WM_DRAWITEM`), las listas virtuales (`LVS_OWNERDATA`), las listas de imágenes, editar etiquetas, arrastrar, hooks ni COM más allá de `CoInitialize` y `CoTaskMemFree`. Si el plugin los necesita, el overlay trae un port de esa parte en Qt, compilado con `edita_native_plugin` sobre `edita_plugin.h`, como el de XML Tools en [`recipes/XMLTools`](https://github.com/alktrip/edita-plugins/tree/main/recipes/XMLTools) del catálogo. Para probar una receta sin red, su `source.url` puede ser una ruta local al zip.

## De Notepad++ a Edita

| Notepad++ | Edita |
|---|---|
| `NppData { _nppHandle, _scintillaMainHandle, _scintillaSecondHandle }` | `EditaData { host, mainEditor, subEditor, send }` |
| `::SendMessage(nppData._nppHandle, NPPM_X, w, l)` | `data.send(data.host, NPPM_X, w, l)` |
| `::SendMessage(scintilla, SCI_X, w, l)` | `data.send(data.mainEditor /* o subEditor */, SCI_X, w, l)` |
| `wchar_t` / `TCHAR` en nombres y rutas | `char` en UTF-8 (un plugin de N++ compilado con `UNICODE` sigue usando `wchar_t`: la capa convierte) |
| Mensajes que devuelven una ruta: tamaño en `wchar_t` | tamaño del búfer en bytes, contando el nulo (devuelven 0 si no cabe) |
| `FuncItem._itemName` (`wchar_t[64]`) | `EditaFuncItem.itemName` (`char[64]`, UTF-8) |
| Diálogos y ventanas Win32 | Lo que quiera el plugin: Qt, GTK… o nada (el de ejemplo usa la barra de estado) |

Un `EditaFuncItem` con `pFunc == NULL` es un separador. `cmdID` lo rellena Edita al cargar el plugin, y el plugin lo usa con `NPPM_SETMENUITEMCHECK`. Las órdenes de los plugins se graban en las macros, como en N++.

## Mensajes disponibles

Mismos números que en N++:
- `NPPM_GETCURRENTSCINTILLA`, `NPPM_GETCURRENTVIEW`.
- `NPPM_GETCURRENTLANGTYPE`, `NPPM_SETCURRENTLANGTYPE`, `NPPM_GETBUFFERLANGTYPE`, `NPPM_SETBUFFERLANGTYPE`.
- `NPPM_GETNBOPENFILES`, `NPPM_GETCURRENTBUFFERID`, `NPPM_GETFULLPATHFROMBUFFERID`.
- `NPPM_GETFULLCURRENTPATH`, `NPPM_GETCURRENTDIRECTORY`, `NPPM_GETFILENAME`, `NPPM_GETNAMEPART`, `NPPM_GETEXTPART`, `NPPM_GETCURRENTWORD`.
- `NPPM_GETCURRENTLINE`, `NPPM_GETCURRENTCOLUMN`.
- `NPPM_DOOPEN`, `NPPM_SWITCHTOFILE`, `NPPM_RELOADFILE`, `NPPM_SAVECURRENTFILE`, `NPPM_SAVEALLFILES`.
- `NPPM_MENUCOMMAND`, que acepta los `IDM_*` de N++ que tienen equivalente en Edita y los `cmdID` de otros plugins.
- `NPPM_SETMENUITEMCHECK`, `NPPM_SETSTATUSBAR`, `NPPM_GETPLUGINSCONFIGDIR`, `NPPM_GETNPPVERSION`, `NPPM_GETNPPDIRECTORY`.
- Documentos por vista y posición: `NPPM_GETCURRENTDOCINDEX`, `NPPM_ACTIVATEDOC`, `NPPM_GETBUFFERIDFROMPOS`, `NPPM_GETPOSFROMBUFFERID`, `NPPM_RELOADBUFFERID`, `NPPM_MAKECURRENTBUFFERDIRTY`.
- Codificación y fin de línea: `NPPM_GETBUFFERENCODING`, `NPPM_SETBUFFERENCODING` (solo documentos nuevos sin cambios, como en N++), `NPPM_GETBUFFERFORMAT`, `NPPM_SETBUFFERFORMAT`.
- Guardar: `NPPM_SAVECURRENTFILEAS`, `NPPM_SAVEFILE`. Sesiones: `NPPM_SAVECURRENTSESSION`, `NPPM_LOADSESSION`, `NPPM_GETNBSESSIONFILES`.
- Lenguajes: `NPPM_GETLANGUAGENAME`, `NPPM_GETLANGUAGEDESC`, `NPPM_GETNBUSERLANG`, `NPPM_CREATELEXER` (un `ILexer5*` de Lexilla para `SCI_SETILEXER`).
- Interfaz: `NPPM_GETMENUHANDLE`, `NPPM_HIDETABBAR`/`NPPM_ISTABBARHIDDEN`, `NPPM_HIDEMENU`/`NPPM_ISMENUHIDDEN`, `NPPM_HIDESTATUSBAR`/`NPPM_ISSTATUSBARHIDDEN`, `NPPM_GETEDITORDEFAULTFOREGROUNDCOLOR`/`BACKGROUNDCOLOR`, `NPPM_ISAUTOINDENTON`, `NPPM_ISDARKMODEENABLED`, `NPPM_GETCURRENTCMDLINE`, `NPPM_GETBOOKMARKID`, `NPPM_LAUNCHFINDINFILESDLG`.
- Recursos compartidos: `NPPM_ALLOCATECMDID` (desde 23000), `NPPM_ALLOCATEMARKER` (1-10; N++ da 1-14, pero Edita usa 11-14 para comparar y la bombilla), `NPPM_ALLOCATEINDICATOR` (9-20), `NPPM_ADDSCNMODIFIEDFLAGS`, `NPPM_GETPLUGINHOMEPATH`.
- Entre plugins: `NPPM_MSGTOPLUGIN` (wParam = archivo del destino, p. ej. `"PythonScript.so"`; el destino lo recibe en `messageProc`).

Cualquier otro mensaje devuelve 0.

### Ventanas: lo que en N++ es un HWND

En Edita los manejadores de ventana son objetos de Qt del mismo proceso, y el plugin puede enlazar con Qt 6 para usarlos:

| Mensaje | Devuelve o recibe |
|---|---|
| `NPPM_GETMENUHANDLE` | `QMenu*` del menú Plugins (`NPPPLUGINMENU`) o `QMenuBar*` (`NPPMAINMENU`) |
| `NPPM_CREATESCINTILLAHANDLE(0, padre)` | un editor de Scintilla (`QWidget*`) creado por Edita; se le habla con `data.send(editor, SCI_*, …)` |
| `NPPM_DESTROYSCINTILLAHANDLE(0, editor)` | lo destruye |
| `NPPM_DMMREGASDCKDLG(0, EditaDockData*)` | registra un panel acoplable: `hClient` es el `QWidget*`, `uMask` la zona (`DWS_DF_CONT_BOTTOM`…) |
| `NPPM_DMMSHOW` / `NPPM_DMMHIDE` / `NPPM_DMMUPDATEDISPINFO` | muestran, ocultan o actualizan el panel (`lParam` = el `QWidget*`) |
| `NPPM_DMMGETPLUGINHWNDBYNAME(nombre, módulo)` | el `QWidget*` de un panel registrado |

Edita destruye los paneles antes de descargar los plugins.

## Notificaciones

- **Del editor:** `NPPN_READY`, `NPPN_FILEOPENED`, `NPPN_FILEBEFORECLOSE`, `NPPN_FILECLOSED`, `NPPN_FILEBEFORESAVE`, `NPPN_FILESAVED`, `NPPN_BUFFERACTIVATED`, `NPPN_LANGCHANGED`, `NPPN_READONLYCHANGED`, `NPPN_BEFORESHUTDOWN`, `NPPN_CANCELSHUTDOWN` y `NPPN_SHUTDOWN`, con `nmhdr.hwndFrom = data.host` y `nmhdr.idFrom` = el documento.
- **De Scintilla:** las `SCN_*` llegan todas, con `nmhdr.hwndFrom` = la vista que las produce.

## Ejemplos

- [`plugins/PythonScript`](../plugins/PythonScript): Python con la API del PythonScript de N++ (ver [fase13.md](fase13.md)). Usa paneles acoplables, `NPPM_CREATESCINTILLAHANDLE`, `NPPM_GETMENUHANDLE` y `NPPM_MSGTOPLUGIN`.
- [`plugins/MarkdownViewerPlusPlus`](../plugins/MarkdownViewerPlusPlus): port nativo de MarkdownViewer++ (ver [fase26.md](fase26.md)). Un panel acoplable que sigue al documento con `SCN_UPDATEUI`, `SCN_MODIFIED` y `NPPN_BUFFERACTIVATED`, sus opciones en un `.ini` de `NPPM_GETPLUGINSCONFIGDIR` y órdenes para otros plugins por `NPPM_MSGTOPLUGIN`.

[`plugins/EditaDemo/EditaDemo.c`](../plugins/EditaDemo/EditaDemo.c) es un plugin completo en C:

- "Hola, Edita": documento nuevo con un saludo.
- Insertar fecha y hora (Ctrl+Alt+D).
- Ordenar líneas.
- Contar palabras.
- "Informar al guardar", que se puede marcar y reacciona a `NPPN_FILESAVED`.

Se compila con el proyecto (`EDITA_BUILD_PLUGINS=ON`), o aparte con:

```sh
cc -std=c11 -fPIC -shared -fvisibility=hidden -I/usr/local/include/edita MiPlugin.c -o MiPlugin.so
mkdir -p ~/.config/edita/plugins/MiPlugin && cp MiPlugin.so ~/.config/edita/plugins/MiPlugin/
```

## Idioma

Edita trae la interfaz en inglés y en español (ver [fase16.md](fase16.md)). Un plugin puede averiguar el idioma de dos maneras:

- **`NPPM_GETNATIVELANGFILENAME`**, como en N++: devuelve el nombre del `nativeLang.xml` equivalente (`"spanish.xml"`) o 0 si la interfaz está en inglés. Lo usa el plugin de ejemplo (`setInfo` en `EditaDemo.c`).
- **`QLocale()`**, si el plugin usa Qt: Edita fija ahí su idioma antes de cargar los plugins. Así carga Python Script su `pythonscript_es.qm`, con `QTranslator::load(QLocale(), …)`.

Los nombres de las órdenes que se ven en el menú Plugins los decide el plugin. Si se traducen, `notepad.runPluginCommand` (Python Script) tiene que usar el nombre traducido. Por eso Python Script deja sus menús en inglés, como en N++.

