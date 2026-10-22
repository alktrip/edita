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

El `CMakeLists.txt` del overlay compila los fuentes originales con la capa de compatibilidad Win32 del SDK:

```cmake
cmake_minimum_required(VERSION 3.20)
project(MiPlugin CXX)
include(${EDITA_PLUGIN_SDK}/cmake/EditaNppPlugin.cmake)
edita_npp_plugin(MiPlugin SOURCES ../src/plugin.cpp ../src/otro.cpp)
```

`compat/windows.h` y compañía hacen que compile el código que usa:

- la API de plugins: `NppData`, `FuncItem`, `DllMain`, `::SendMessage` a N++ y a Scintilla;
- cadenas `TCHAR` (que aquí es `char` en UTF-8);
- `MessageBox`.

Los diálogos y las ventanas Win32 no se emulan. Si el plugin los necesita, el overlay trae un port de su interfaz en Qt, compilado con `edita_native_plugin` sobre `edita_plugin.h`, como el de XML Tools en `data/plugins/recipes/XMLTools`. Para probar una receta sin red, su `source.url` puede ser una ruta local al zip.

## De Notepad++ a Edita

| Notepad++ | Edita |
|---|---|
| `NppData { _nppHandle, _scintillaMainHandle, _scintillaSecondHandle }` | `EditaData { host, mainEditor, subEditor, send }` |
| `::SendMessage(nppData._nppHandle, NPPM_X, w, l)` | `data.send(data.host, NPPM_X, w, l)` |
| `::SendMessage(scintilla, SCI_X, w, l)` | `data.send(data.mainEditor /* o subEditor */, SCI_X, w, l)` |
| `wchar_t` / `TCHAR` en nombres y rutas | `char` en UTF-8 |
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
- Recursos compartidos: `NPPM_ALLOCATECMDID` (desde 23000), `NPPM_ALLOCATEMARKER` (1-15), `NPPM_ALLOCATEINDICATOR` (9-20), `NPPM_ADDSCNMODIFIEDFLAGS`, `NPPM_GETPLUGINHOMEPATH`.
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

