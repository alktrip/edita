# Fase 53 — Capa de compatibilidad v2, parte 3: los recursos de los .rc

Fecha: 2026-10-30

## Por qué

Después de la [fase 52](fase52.md), las ventanas de los plugins de N++ funcionaban, pero sus diálogos se creaban vacíos: casi todos se definen en un `.rc` (`DIALOGEX` con sus controles), que la capa no leía. Era lo que separaba a los plugins de la clase B1 de los plugins completos.

## Qué se construyó

| Parte | Qué hace |
|---|---|
| **`edita_rc`** (`plugin-sdk/tools/`, nuevo) | Convierte los `.rc` de un plugin en un `.cpp` con sus recursos en el **formato binario de Windows**: `DLGTEMPLATEEX` para `DIALOG`/`DIALOGEX`, la plantilla clásica para `MENU`/`MENUEX` y las cadenas de `STRINGTABLE`, en UTF-16 y alineados como en Windows. Al cargarse el plugin, el `.cpp` los registra en la capa (`edita_compat_setResources`). Es C++17 sin dependencias, y se compila con el compilador del equipo |
| Su preprocesador | Las reglas de `rc.exe`: `#include` (sin distinguir mayúsculas, con `\`), `#define` sin argumentos, `#undef`, `#if`/`#ifdef`/`#ifndef`/`#elif`/`#else`/`#endif` con `defined()` y expresiones. De los archivos incluidos que no son `.rc` (`resource.h`…) solo lee las directivas. No lee las cabeceras de Windows: las constantes de los estilos salen de las cabeceras de la capa (`edita_rc_constants.cpp`), así que no hay dos copias de los valores. Define `RC_INVOKED` y `_WIN32`; `APSTUDIO_INVOKED` no, así que salta los bloques de Visual Studio |
| Su intérprete | `DIALOG`/`DIALOGEX` con `STYLE`, `EXSTYLE`, `CAPTION`, `FONT`, `MENU`, `CLASS` y todos los controles: `CONTROL` (con las clases `WC_*`), `LTEXT`/`RTEXT`/`CTEXT`, los botones, `EDITTEXT`, `COMBOBOX`, `LISTBOX`, `SCROLLBAR` e `ICON`, con los estilos por defecto de `rc.exe` y `NOT`. `MENU`/`MENUEX` con `POPUP`, `MENUITEM`, `SEPARATOR` y sus opciones. `STRINGTABLE` y `LANGUAGE`: si un recurso está en varios idiomas, se queda con el neutro o el inglés. Salta lo demás: iconos, mapas de bits, `VERSIONINFO`, aceleradores, `TEXTINCLUDE`, `DESIGNINFO`… Lee los `.rc` en UTF-16 con BOM, en UTF-8 y en ANSI (Windows-1252) |
| **`edita_npp_plugin(… RESOURCES …)`** | Compila `edita_rc` y genera los recursos del plugin al compilar la receta |
| **Recursos en la capa** (`npp_window.cpp`) | `FindResource`/`LoadResource`/`LockResource`/`SizeofResource` con la semántica de Windows (la ruta RTL de `StaticDialog`, que copia la plantilla, funciona). Un intérprete de plantillas `DLGTEMPLATE` y `DLGTEMPLATEEX` crea el diálogo y sus controles con las ventanas de la fase 52. Convierte las unidades de diálogo a píxeles con la fuente de Edita, como hace Windows con la del diálogo (`MapDialogRect`, `GetDialogBaseUnits`). `WM_INITDIALOG` recibe el primer control con `WS_TABSTOP`. La altura de los combos, que en el `.rc` es la de su lista, se ajusta. Un control de una clase que la capa no tiene es un recuadro con su nombre y su id |
| Diálogos modales | `DialogBoxParam` y `DialogBoxIndirectParam`, modales de verdad hasta `EndDialog` (también si se llama en `WM_INITDIALOG`), con el valor de `EndDialog` |
| `LoadString`, `LoadMenu` | Las cadenas (`LoadStringW` con tamaño 0 devuelve un puntero, como en Windows) y los menús del `.rc`, con submenús, separadores y estados (`LoadMenuIndirect` para las plantillas en memoria) |
| **Lo barato de la auditoría** | `SetWindowSubclass`/`GetWindowSubclass`/`RemoveWindowSubclass`/`DefSubclassProc` sobre la cadena de procedimientos. `GetOpenFileName` (con selección múltiple en el formato de Windows y los desplazamientos `nFileOffset`/`nFileExtension`)/`GetSaveFileName`/`SHBrowseForFolder`/`ChooseColor` sobre los diálogos de Qt, que siguen la preferencia de Edita (los del sistema o los de Qt). `CreateProcess` (`fork` + `exec`) con las tuberías de `CreatePipe`, `GetExitCodeProcess`, `TerminateProcess` y `WaitForSingleObject`. La línea de órdenes se divide con las reglas de `CommandLineToArgvW`, y `cmd /c …` se ejecuta con `/bin/sh -c`. `GetMenuItemInfo`/`SetMenuItemInfo`/`InsertMenuItem`. `InitCommonControlsEx`, `CoInitialize`, `CoTaskMemFree` |

## Divergencias

| Tema | Windows | Edita | Motivo |
|---|---|---|---|
| Fuente del diálogo | La del `.rc` (`MS Shell Dlg` 8) | La de Edita: los diálogos salen algo más grandes, con las mismas proporciones | Las fuentes de Windows no están en Linux; las unidades de diálogo se calculan con la fuente que se usa |
| Controles comunes en un diálogo | Sí | Un recuadro con el nombre de la clase | Siguiente fase |
| Iconos y mapas de bits del `.rc` | Sí | No (`LoadIcon`/`LoadImage` del plugin devuelven `NULL`) | — |
| `ReadFile` de una tubería | Devuelve lo que haya | Igual (los archivos leen hasta llenar el búfer) | — |
| `PeekNamedPipe` | Puede copiar los datos | Solo dice cuántos hay | Una tubería no se puede mirar sin leerla |
| Entorno de `CreateProcess` | El bloque que se le pasa | El de Edita | Casi ningún plugin lo usa |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| `edita_rc` convierte `tests/fixtures/rc` y lo hace bien: `resource.h` con identificadores calculados y código C (que se ignora), `#ifdef APSTUDIO_INVOKED`, todos los controles, `NOT WS_BORDER`, un diálogo hijo, un menú con submenú, separador y estados, comillas dobles en las cadenas, recursos que se saltan (icono, `VERSIONINFO`), el mismo diálogo en dos idiomas, macros con argumentos (`#` y `##`), una cadena de dos líneas y un `.rc` en UTF-16 | ✅ compilado con los avisos del proyecto con GCC y clang; los recursos los comprueban las pruebas siguientes |
| `FindResource`/`LockResource` dan la plantilla. `CreateDialogParam` del diálogo neutro (no del francés), con su título, sus controles, sus estilos y sus ids; `WM_INITDIALOG` con el primer `WS_TABSTOP`; la geometría coincide con `MapDialogRect`; el recuadro de la clase que no hay; Intro → `IDOK`. El diálogo hijo, dentro de su padre. `DialogBoxParam` modal devuelve el valor de `EndDialog`. `CreateDialogIndirectParam` con la plantilla en memoria | ✅ `tst_npp_compat`: `resourceDialogs` |
| `LoadMenu` (submenús, separador, marcado, gris) y `LoadString` (escapes, búfer corto, puntero con tamaño 0, UTF-16, ANSI, la que no existe) | ✅ `resourceMenusAndStrings` |
| `SetWindowSubclass` en cadena (orden, `refData`, quitar), `MENUITEMINFO` | ✅ `subclassesAndMenuItemInfo` |
| `GetOpenFileName` (filtros de Windows, desplazamientos, título, selección múltiple, búfer corto), `GetSaveFileName` con la extensión por defecto, `SHBrowseForFolder`, `ChooseColor` | ✅ `commonDialogs` |
| `CreateProcess` con una tubería (`cmd /c echo … && exit 3`: la salida y el código), argumentos entre comillas y carpeta de trabajo, `TerminateProcess` y un programa que no existe | ✅ `processes` |
| El panel «Go To Line #» de la plantilla oficial de N++, **sin cambios** (`GoToLineDlg.cpp`, `StaticDialog.cpp` y el diálogo de `goLine.rc`), se compila desde su receta con `RESOURCES` y aparece como panel de Edita con su título, su texto, su campo y su botón en el sitio del `.rc`. El campo solo admite dígitos (`ES_NUMBER`), y «Go» o Intro llevan el cursor a la línea con el código original | ✅ `tst_recipes`: `builderCompilesTheTemplateGoToLineDialog` |
| Todo lo anterior sigue igual | ✅ `tst_recipes`, `tst_npp_compat` |

## Auditoría repetida

Se repitió la auditoría completa, con el código de los plugins descargado de nuevo: la carpeta temporal de la medición anterior se perdió en un reinicio. La compilación de prueba ya pasa los `.rc` de cada plugin a `edita_rc`.

| SDK | Compilan (A) | Solo falla la interfaz (B1) | Necesitan un port (B2) |
|---|---:|---:|---:|
| Fase 49 | 2 | 2 | 103 |
| Fase 51 | 5 | 4 | 98 |
| Fase 52 | 7 | 7 | 93 |
| Fase 53 | **9** | **6** | 92 |

- **El criterio de la clase A cambió.** Antes pedía «como mucho un diálogo», porque los diálogos salían vacíos. Ahora un plugin que compila entero (sin MFC) ya es de la clase A, con sus diálogos. Por eso **BatPathIntelliSense**, que ya compilaba, pasa de B1 a A. **NppExport** compila por primera vez. Ninguno de los dos declara licencia.
- **La cifra casi no se mueve, y es lo esperado.** Los `.rc` no hacen compilar a más plugins: hacen que los que compilan **tengan su interfaz**. Por ejemplo, nppAutoDetectIndent y selectNLaunch compilan ahora con el diálogo de su `.rc` convertido. No se abrieron en Edita: lo que está probado en ejecución es el mecanismo, con las pruebas de esta fase y el «Go To Line #» de la plantilla.
- **`SetWindowSubclass`, `CreateProcess` y `OPENFILENAME` ya no aparecen entre los bloqueos**, aunque cada plugin de la clase B2 tiene varios, y lo que quede lo sigue frenando.
- **La auditoría encontró errores en `edita_rc`, que se corrigieron en esta fase:**
  - las macros con argumentos (`#define TO_STR(X) #X`, con `#` y `##`), que usa ComparePlus para el texto de su versión;
  - las cadenas que siguen en la línea siguiente, que usa Papyrus;
  - constantes de `FONT` que faltaban (`FW_DONTCARE`, `DEFAULT_CHARSET`, `FALSE`) y más idiomas.

  Quedaron en `tests/fixtures/rc`.
- **Lo que frena ahora a los de la clase B2:**

  | Qué falta | Plugins |
  |---|---:|
  | Controles comunes: pestañas (`TCN_SELCHANGE`, `TabCtrl_*`), listas con columnas (`ListView_*`), *tooltips*, `NM_CLICK` | unos 25 |
  | Dibujo propio (`BeginPaint`, `FillRect`, `DrawText`, `CreateCompatibleDC`) | unos 22 |
  | Hooks (`SetWindowsHookEx`) | 15 |
  | COM (`CoCreateInstance`) | 14 |
  | `GetProcAddress` | 13 |
  | Bucles de mensajes propios (`TranslateMessage`) | 13 |

El detalle está en [edita-plugins/audit](https://github.com/alktrip/edita-plugins/tree/main/audit).

## Siguiente

Con esta fase, la capa cubre lo que traen casi todos los plugins: su API, Win32 sin ventanas, las ventanas, la plantilla de N++ y sus `.rc`. Lo que queda es más específico de cada plugin. Hay dos caminos:

- **Seguir con la capa:** primero los controles comunes más usados (pestañas y listas con columnas, sobre `QTabBar` y `QTreeWidget`) y los bucles de mensajes (`GetMessage`/`TranslateMessage`/`DispatchMessage` sobre el de Qt), que son los que más plugins frenan. Después, el dibujo básico sobre `QPainter`.
- **Empezar a empaquetar:** el primer lote con licencia (Reverse Lines, NppPluginOpenHost, selectNLaunch, SimplifyJson, TidyText, nppAutoDetectIndent y SqlColumnHighlighter): mudar las recetas a `edita-plugins`, la CI por plataforma y la lista firmada.

Pasan las 51 suites y el build no da ningún warning.
