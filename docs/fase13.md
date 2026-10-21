# Fase 13 — Python Script: resultado

Fecha: 2026-10-20 · Entorno: el mismo que en las fases anteriores, más Python 3.14 (cabeceras de `python3-embed`)

Objetivo: un plugin de scripting compatible con **PythonScript** de Notepad++ (Dave Brotherstone), para que los scripts escritos para N++ funcionen en Edita sin cambios. Referencia: `referencia/PythonScript` (su código y su documentación). La implementación es propia: la de PythonScript usa boost::python y Win32. Se reproduce su API, sus enums, su `startup.py`, su archivo de configuración y su modelo de hilos.

## Qué se construyó

| Módulo | Equivale en PythonScript a | Qué hace |
|---|---|---|
| `plugins/PythonScript/PythonScript.cpp` | `PythonScript.cpp`, `PythonHandler`, `MenuManager`, `ConsoleDialog` | El plugin (`PythonScript.so`): menú, consola, CPython integrado y el módulo nativo `_npp`. Ese módulo es el puente con `data.send()`: mensajes `SCI_*` a cualquier editor, `NPPM_*` a la ventana, cadenas de entrada y de resultado, `FINDTEXTFULL`, `GETTEXTRANGEFULL`, `GETSTYLEDTEXTFULL`, diálogos y menús |
| `lib/Npp/__init__.py` | `NotepadPython.cpp`, `ScintillaPython.cpp`, `ScintillaWrapper.cpp`, `MatchPython.cpp` | El módulo `Npp`: `notepad`, `editor` (la vista activa), `editor1`, `editor2`, `console`, `Editor`, `Notepad`, `Console`, `Cell` y los enums propios (`FORMATTYPE`, `BUFFERENCODING`, `STATUSBARSECTION`, `MESSAGEBOXFLAGS`…) |
| `lib/Npp/_generated.py` + `tools/gen-pythonscript.py` | `CreateWrapper.py` | Generados desde `Scintilla.iface`: los **817 métodos** de `editor` con las mismas reglas de PythonScript (`addText(text)`, `getText()`, colores como tuplas, cadenas de resultado devueltas y casos especiales como `getLine`, `getStyledText`, `annotationSetText(línea, None)` o `findText`). También los **73 enums de Scintilla** con sus nombres de PythonScript (`FINDOPTION`, `MODIFICATIONFLAGS`…), `SCINTILLANOTIFICATION` y `SCINTILLAMESSAGE`. Desde las cabeceras de N++ salen `LANGTYPE`, `NOTIFICATION` y `MENUCOMMAND` (todos los `IDM_*`) |
| `Config.{h,cpp}` | `ConfigFile.cpp` | `PythonScriptStartup.cnf` con su formato: `ITEM/` para los scripts del menú (relativos a los del usuario) y `SETTING/STARTUP/LAZY\|ATSTARTUP`, `OPENCONSOLEONERROR`, `DISABLEPOPUPWARNING`… |
| `scripts/startup.py` | `scripts/startup.py` | `from Npp import *` en `__main__`, `sys.stdout` a la consola y `sys.stderr` a la consola en rojo |
| `NppPythonScript.h` | `NppPythonScript.h` | `PYSCR_EXECSCRIPT`, `PYSCR_EXECSTATEMENT`, `PYSCR_SHOWCONSOLE` y `PythonScript_Exec` (con los mismos números), para que otros plugins ejecuten Python con `NPPM_MSGTOPLUGIN`. Se instala en `include/edita/` |
| Edita (host) | `NppBigSwitch.cpp`, `DockingManager` | Unos 50 mensajes `NPPM_*` nuevos, con los números de N++ (ver [plugins.md](plugins.md)), entre ellos: paneles acoplables (`NPPM_DMMREGASDCKDLG`, `DMMSHOW`/`DMMHIDE`), editores de Scintilla para los plugins (`NPPM_CREATESCINTILLAHANDLE`), `NPPM_CREATELEXER`, `NPPM_GETMENUHANDLE`, `NPPM_MSGTOPLUGIN`, documentos por posición, codificación, formato, sesiones y `NPPM_ALLOCATE*`. Además se corrigió `NPPM_SETBUFFERLANGTYPE`, que rechazaba los lenguajes con número mayor que `L_USER` |

### Cómo funciona, igual que PythonScript

- **Menú Plugins > Python Script:**
  - New Script, Show Console, Stop Script, Run Previous Script (con el nombre del último).
  - El submenú **Scripts**: primero los de la máquina, luego los del usuario, con las subcarpetas como submenús. Ctrl+clic abre el script para editarlo, y guardar un script actualiza el menú.
  - Los scripts configurados, Configuration…, Context-Help y About.
- **Carpetas:**
  - Scripts y `lib` de la máquina: junto al `.so`.
  - Del usuario: `~/.config/edita/plugins/config/PythonScript/{scripts,lib}`.
  - Las cuatro van delante en `sys.path`.
- **Inicio:**
  - Con `LAZY` (por defecto), Python se inicia la primera vez que se usa.
  - Con `ATSTARTUP`, al terminar de arrancar (`NPPN_READY`).
  - En los dos casos se ejecutan el `startup.py` de la máquina y luego el del usuario.
- **Hilos:**
  - Los scripts (menú, Run Previous, consola, `PYSCR_*` sin `PYSCRF_SYNC`) se ejecutan en un hilo propio, uno a la vez. Si ya hay uno ejecutándose aparece el aviso de PythonScript («Another script is currently running…»).
  - **Stop Script** lanza `KeyboardInterrupt` en ese hilo.
  - Los callbacks de `editor.callback` y los de `notepad.callback` se ejecutan en otro hilo, en orden. Los de `editor.callbackSync`, en el acto.
  - Todo lo que los hilos piden al editor o a la ventana se pasa al hilo de la interfaz, que es lo que en Windows hace `SendMessage`.
  - `startup.py` y `PYSCRF_SYNC` se ejecutan en el hilo de la interfaz.
- **Consola:**
  - Es un panel acoplable abajo, con un editor de Scintilla de Edita (`console.editor`) y una línea de entrada con historial. Las sentencias de varias líneas muestran el indicador `...`.
  - Los errores se ven en rojo y abren la consola (`OPENCONSOLEONERROR`).
  - `console.run(orden)` ejecuta un programa externo.
- **`editor.replace`, `rereplace`, `research` y `search`:**
  - Usan el módulo `re` de Python sobre el texto del documento. Las posiciones son las del documento, en bytes.
  - `^` y `$` son por línea, incluso con CRLF o CR.
  - Se traduce la sintaxis de Boost que difiere: `(?<nombre>…)`, `\k<nombre>`, `\<` y `\>`.
  - El reemplazo usa el **formato de Boost** (`format_all`, como PythonScript y N++): `$1`, `${nombre}`, `$&`, `\1`, condicionales `(?1sí:no)`, `\U…\E`, `\l`/`\u`, y paréntesis para agrupar.
  - Con una función de reemplazo, esta recibe un objeto parecido a `re.Match` y todo se deshace de una vez.

## Uso rápido

```python
# Plugins > Python Script > New Script, guardar y ejecutarlo desde Scripts
editor.rereplace(r'X([0-9]+)', lambda m: 'Y' + str(int(m.group(1)) + 1))
for (filename, bufferID, index, view) in notepad.getFiles():
    console.write(filename + '\n')
notepad.callback(lambda a: console.write('Guardado %d\n' % a['bufferID']), [NOTIFICATION.FILESAVED])
```

## Compatibilidad comprobada con la batería de PythonScript

Se ejecutaron en Edita los tests de `PythonScript/python_tests` (197 tests de 25 archivos): **pasan 195**. Los dos restantes:

- `test_NotepadWrapperTestCase` no carga: usa `ctypes.WINFUNCTYPE` y llamadas a Win32.
- `test_scintillawrapper_void_void_int_in_callback` llama dentro del callback a un test marcado con `@unittest.skip("NPP BUG STILL EXISTS")`. En N++ falla igual.

Los 24 tests de callbacks (síncronos y asíncronos) pasan, como los de reemplazo (UTF-8, función, flags, inicio y fin, recuento, `lastindex`, condicionales), búsqueda y líneas en CR/LF/CRLF.

## Divergencias

| Tema | PythonScript | Edita | Motivo |
|---|---|---|---|
| Expresiones regulares de `rereplace`/`research` | Boost.Regex | Módulo `re` de Python (traduciendo la sintaxis de Boost que difiere) y formato de Boost para el reemplazo | No hay boost::python. Las diferencias que quedan son poco frecuentes (`\K`, clases POSIX `[[:alpha:]]`) |
| Documentos ANSI | Conversión según la codificación del documento | Scintilla en Edita trabaja siempre en UTF-8 | Ver la Fase 2: Edita convierte al cargar y al guardar |
| `ctypes` con Win32, `notepad.getWindowsVersion()`, barra de herramientas, iconos de la barra (`TOOLBAR/`), modo oscuro | Sí | No hay; las funciones devuelven valores neutros | Son de Windows o no existen en Edita |
| `notepad.getMenuHandle()` | `HMENU` | Puntero a `QMenu`/`QMenuBar` | No hay `HMENU` en Linux |
| `console.editor` | Su `HWND` | Un `Editor` de la consola | — |
| Atajos para los scripts del menú (Shortcut Mapper) | Sí | No | Edita no tiene editor de atajos |
| Configuración | Diálogo con árbol de scripts y barra de herramientas | Diálogo con la lista de scripts del menú y el modo de inicio | Más sencillo |
| Intérprete | Python incluido con el plugin | El Python del sistema (`libpython3.x`) | Es lo habitual en Linux |
| Varias ventanas en un proceso | — | El intérprete sigue vivo; `Npp` se vuelve a importar en cada ventana | CPython no se puede reiniciar de forma fiable |
| Un script que espera con `join()` a un hilo que usa el editor | Funciona (el script tiene su hilo) | Igual en el hilo de scripts; con `PYSCRF_SYNC` se bloquearía | En `PYSCRF_SYNC` el script ocupa el hilo de la interfaz |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| El plugin se carga con el menú de PythonScript (en su orden) y el submenú Scripts con los de la máquina | ✅ `tst_pythonscript`: `pluginLoadsWithItsMenu` |
| `editor` maneja Scintilla: texto, posiciones en bytes, selección, colores como tuplas y helpers (`replaceLine`, `deleteLine`, `forEachLine`, `getUserLineSelection`, `getCurrentWord`, `findText`); `editor1` es la vista principal | ✅ `editorObjectDrivesScintilla` |
| `rereplace` con función y una sola acción de deshacer, referencias de Boost y de Python, `^`/`$` por línea con CRLF, `research` con posiciones en bytes, `replace` literal con flags y máximo, errores como en PythonScript | ✅ `searchAndReplace` |
| `notepad`: abrir, archivo actual, lenguaje y lexer (también por encima de `L_USER`), nombres de lenguaje, `getFiles`, activar por archivo y por bufferID, formato, codificación, `menuCommand` con IDM de N++, nuevo, guardar como, `runMenuCommand` por texto, versión, carpetas, `allocateMarker`, barra de estado | ✅ `notepadObject` |
| Consola: `print`, errores en rojo que la abren, `console.editor`, `console.run`, entrada interactiva de varias líneas | ✅ `consoleAndErrors` |
| Callbacks: `notepad.callback`, `editor.callback` (asíncrono), `callbackSync`, `clearCallbacks`; un hilo de Python puede usar el editor | ✅ `callbacks` |
| Los scripts se ejecutan en su hilo: un script puede esperar con `sleep` a su callback asíncrono; Stop Script interrumpe un bucle sin fin | ✅ `scriptsRunInTheirOwnThread` |
| Los enums tienen los valores de N++ y de Scintilla | ✅ `enumsMatchNotepadPlusPlus` |
| `ATSTARTUP` ejecuta el `startup.py` del usuario; los scripts de `ITEM/` en el menú; Run Previous Script; `PYSCR_EXECSCRIPT` y `PYSCR_EXECSTATEMENT` síncronos y asíncronos desde otro plugin; un módulo inexistente se rechaza | ✅ `scriptsFromMenusAndOtherPlugins` |
| Una segunda ventana empieza con `Npp` limpio | ✅ `secondWindowStartsFresh` |
| La batería original de PythonScript | ✅ 195 de 197 (ver arriba) |

Además se comprobó en una captura la consola acoplada, con la salida y un error en rojo.

Tests: `tst_pythonscript` 12 (contando `initTestCase` y `cleanupTestCase`). `tst_plugins` se adaptó para que admita más de un plugin en el build. Pasan las 21 suites y el build no da ningún warning.
