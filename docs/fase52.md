# Fase 52 — Capa de compatibilidad v2, parte 2: ventanas

Fecha: 2026-10-30

## Por qué

Después de la [fase 51](fase51.md), lo que frenaba a casi todos los plugins de N++ eran las ventanas. Casi todos traen las clases de la plantilla de N++ (`Window`, `StaticDialog`, `DockingDlgInterface`), que usan una parte grande de USER32: `GetParent`, `ScreenToClient`, `SetWindowLongPtr`/`GWLP_USERDATA`, monitores, métricas, recursos… Con la capa anterior, ninguna de ellas compilaba.

## Qué se construyó

| Parte | Qué hace |
|---|---|
| **Ventanas** (`compat/npp_compat_user.h`, `compat/npp_window.cpp`, nuevos) | Un `HWND` de la capa es una ventana con su `QWidget`, su procedimiento (`WNDPROC` o `DLGPROC`), sus datos (`GWLP_USERDATA`, `cbWndExtra`, `DWLP_*`, `SetProp`), sus hijos y sus temporizadores. `SendMessage` llama al procedimiento, que deja lo que no trata a `DefWindowProc`. `WM_NCCREATE`/`WM_CREATE` llevan el `CREATESTRUCT`; `WM_SIZE`, `WM_MOVE`, `WM_SHOWWINDOW`, el ratón, el teclado, el foco y `WM_CONTEXTMENU` llegan de Qt. `DestroyWindow` manda `WM_DESTROY` a la ventana y a sus hijos. Las `Window` no se liberan: un `HWND` destruido lo rechaza `IsWindow`, y el plugin lo puede tocar desde sus destructores estáticos |
| **Controles estándar** | `BUTTON` (`QPushButton`, `QCheckBox` normal o de tres estados, `QRadioButton`, `QGroupBox`), `EDIT` (`QLineEdit` o `QPlainTextEdit`, con contraseña, solo lectura, solo números), `STATIC` (`QLabel` sin el `&` de los atajos, o una línea), `LISTBOX` (`QListWidget`: simple, múltiple, extendida, ordenada) y `COMBOBOX` (`QComboBox`, editable o no, ordenado). Tienen sus mensajes: `BM_*`, `EM_*` (con las posiciones de Windows, que cuentan `\r\n` como dos), `LB_*` y `CB_*` (cadenas, selección, búsqueda, datos por elemento). Sus notificaciones van al padre como `WM_COMMAND`, y los cambios que hace el plugin no notifican, como en Windows. `BS_CHECKBOX` y `BS_RADIOBUTTON`, a diferencia de los automáticos, no cambian solos |
| **Subclassing** | `SetWindowLongPtr(GWLP_WNDPROC)` devuelve el procedimiento del control, y `CallWindowProc` lo llama. El procedimiento del plugin recibe el teclado del control (`WM_KEYDOWN`, `WM_CHAR`, `WM_KEYUP`): si no lo deja llegar al procedimiento de la clase, Qt no lo procesa. Es el patrón de los buscadores que reaccionan a Intro |
| **Diálogos** | `CreateDialogParam` y `CreateDialogIndirectParam` crean el diálogo y le mandan `WM_INITDIALOG` con su `lParam`. Hay `DWLP_MSGRESULT`, `DefDlgProc` (cerrar = `IDCANCEL`), Intro (el botón `BS_DEFPUSHBUTTON` o `IDOK`), Esc (`IDCANCEL`), `EndDialog` y los ayudantes de `GetDlgItem*`/`SetDlgItem*`, `CheckDlgButton`, `CheckRadioButton` y `SendDlgItemMessage`. Los `.rc` todavía no se leen: el diálogo se crea vacío, con un aviso que se quita cuando el plugin le pone controles. `DialogBoxParam` (modal) muestra un aviso |
| **Paneles acoplables** (`npp_compat.cpp`) | `NPPM_DMMREGASDCKDLG` con el `tTbData` de N++, en los dos modos, se convierte en un panel de Edita con el `QWidget` del diálogo. El título sale del `tTbData`, del diálogo o del plugin, y el nombre del módulo pasa de `.dll` a `.so`. `NPPM_DMMSHOW`/`HIDE`/`UPDATEDISPINFO` y `NPPM_DMMGETPLUGINHWNDBYNAME` convierten entre `HWND` y `QWidget*`. Cerrar el panel envía `WM_NOTIFY` con `DMN_CLOSE`, y soltarlo o acoplarlo, `DMN_FLOAT`/`DMN_DOCK`, como el DockingManager de N++. Si el plugin destruye la ventana, Edita quita el panel |
| **Ventanas de Edita y editores** | `nppData._nppHandle` y los editores sirven para medir, como padre y en `GetMenu`. `NPPM_CREATESCINTILLAHANDLE` acepta una ventana de la capa como padre, y `CreateWindowEx("Scintilla")` crea un editor de Edita. `NPPM_GETMENUHANDLE` y `GetMenu(nppData._nppHandle)` devuelven el menú de Edita: `CheckMenuItem` sobre él marca las órdenes del plugin (`NPPM_SETMENUITEMCHECK`). `NPPM_MODELESSDIALOG` se responde en la capa |
| **Sistema** | `SetTimer`/`KillTimer` (con `TIMERPROC` o `WM_TIMER`). `PostMessage` es asíncrono para las ventanas de la capa. Menús emergentes de Qt (`CreatePopupMenu`, `AppendMenu`, `TrackPopupMenu` con y sin `TPM_RETURNCMD`, `CheckMenuItem`…). Monitores (`QScreen`), `GetSystemMetrics`, `SystemParametersInfo` (área de trabajo, fuentes), cursores del sistema (`IDC_WAIT` es el reloj de Qt) |
| **GDI sin dibujo** | Fuentes (`CreateFont`, `CreateFontIndirect`, `GetObject`, `WM_SETFONT`), `GetDC`/`SelectObject`/`GetTextExtentPoint32`/`GetTextMetrics` con `QFontMetrics`, colores del sistema (la paleta de Qt), `GetDeviceCaps` y DPI (siempre 96: las coordenadas son píxeles lógicos de Qt). No hay `WM_PAINT` ni funciones de dibujo: un plugin que dibuja no compila, en vez de compilar y quedar en blanco |
| **Hilos y varios** (`npp_win32.cpp`) | `CreateThread`, eventos, mutex, `WaitForSingleObject`/`WaitForMultipleObjects`, secciones críticas (recursivas), `Interlocked*`, `FormatMessage` (mensajes de sistema e inserciones `%1`), `MAKELANGID` y el idioma del usuario, `MulDiv` (redondea como Windows), `min`/`max` como funciones (las macros romperían la biblioteca de C++), y `SCI_GETTEXTRANGE`, `SCI_FINDTEXT`, `Sci_TextRange`… que N++ quitó de su `Scintilla.h` y Scintilla todavía atiende |

## Divergencias

| Tema | Windows | Edita | Motivo |
|---|---|---|---|
| Diálogos de los `.rc` | La plantilla del recurso | Vacíos, con un aviso; `DialogBoxParam` avisa | Llega con la fase siguiente |
| Dibujo propio (`WM_PAINT`, `BeginPaint`, GDI) | Sí | No | Un plugin que dibuja necesita un port de esa parte |
| Controles comunes (`SysListView32`, `SysTreeView32`, pestañas…) | Sí | `CreateWindowEx` falla (`ERROR_CANNOT_FIND_WND_CLASS`) | Siguientes fases, según la auditoría |
| DPI | El del monitor | 96, con coordenadas en píxeles lógicos | Qt ya escala |
| `PostMessage` a Edita | Asíncrono | Síncrono, como hasta ahora | Edita atiende sus mensajes en el momento |
| `IsDialogMessage` | Navega con Tab y atajos | Devuelve `FALSE`: Qt ya hace la navegación, y la capa trata Intro y Esc | — |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Clases registradas (`WM_NCCREATE`/`WM_CREATE` con `lpCreateParams`, `cbWndExtra`, `SetProp`, `WM_SIZE`). Botones, casillas automáticas y manuales, opciones, campos (EN_CHANGE, selección, reemplazo, enteros, multilínea con `\r\n`), listas ordenadas (búsqueda, datos, notificación solo del usuario), combos y estáticos (`&`), con sus `WM_COMMAND`. Subclassing que se come Intro. `EnumChildWindows`, `GetWindow`, `DestroyWindow` | ✅ `tst_npp_compat`: `windowsAndControls` |
| Diálogos: `WM_INITDIALOG`, `DWLP_USER`, `DWLP_MSGRESULT`, el aviso sin plantilla, Intro/Esc/cerrar y el botón por defecto, `EndDialog` | ✅ `dialogs` |
| Temporizadores (`WM_TIMER` y `TIMERPROC`) y `PostMessage` en cola | ✅ `timersAndPostMessage` |
| Menús: añadir, separadores, submenús, marcar, deshabilitar, estado, `TrackPopupMenu` con `TPM_RETURNCMD`, borrar | ✅ `menus` |
| GDI sin dibujo (fuentes, medidas, colores, DPI), métricas, área de trabajo, monitores, `MulDiv`, `min`/`max`, `Sci_TextRange`, cursores y recursos que todavía no hay | ✅ `gdiAndSystem` |
| Hilos, eventos manuales y automáticos, mutex, secciones críticas, `Interlocked*`, `FormatMessage` | ✅ `threadsAndFormatMessage` |
| Un plugin con un panel hecho con `Window.h`, `StaticDialog.cpp` y `DockingDlgInterface.h` de la plantilla oficial de N++, sin cambios (`tests/fixtures/npp-plugintemplate`), se compila desde su receta y aparece como panel de Edita a la derecha, con el nombre del plugin. El botón que crea en `WM_INITDIALOG` inserta el texto de su campo en el editor (`BN_CLICKED` → `GetDlgItemText` → `SCI_REPLACESEL`), Intro pulsa el botón por defecto, `WM_SIZE` coloca los controles, y cerrar el panel le envía `DMN_CLOSE` y desmarca la orden | ✅ `tst_recipes`: `builderCompilesATemplateDockingPlugin` |
| Todo lo anterior sigue igual: modo ANSI, MIME Tools, XML Tools, el plugin Unicode de la fase 51 | ✅ `tst_recipes` |
| El código de la capa compila sin avisos con GCC y clang, y la cabecera en C | ✅ |

## Auditoría repetida

Se repitió la compilación de prueba de la auditoría, con `UNICODE`, sobre el mismo código de los 136 plugins nativos:

| SDK | Compilan (A) | Solo falla la interfaz (B1) | Necesitan un port (B2) |
|---|---:|---:|---:|
| Fase 49 | 2 | 2 | 103 |
| Fase 51 | 5 | 4 | 98 |
| Fase 52 | **7** | **7** | 93 |

- **Nuevos en la clase A:** nppAutoDetectIndent (GPL v3) y SqlColumnHighlighter (MIT).
- **Nuevos en la clase B1:** ElasticTabstops (GPL v2), BatPathIntelliSense (sin licencia declarada) y nppConverter (licencia por revisar). BatPathIntelliSense compila entero, pero sus diálogos son de un `.rc`.
- **La plantilla oficial de N++ (NppPluginTemplate) compila entera** (con su `GoToLineDlg`). Edita ya la cubre, por eso está en la clase E. Junto con `builderCompilesATemplateDockingPlugin`, muestra que las clases que traen casi todos los plugins compilan y funcionan.
- **Lo que frena ahora a los de la clase B2:**

  | Qué falta | Plugins |
  |---|---:|
  | Dibujo propio (`BeginPaint`, `FillRect`, `DrawText`, `CreateCompatibleDC`) | unos 22 |
  | Controles comunes (pestañas, listas con columnas, *tooltips*, `NM_CLICK`, `InitCommonControlsEx`) | unos 25 |
  | `SetWindowSubclass`/`DefSubclassProc` de comctl32 | 17 |
  | `CreateProcess` | 18 |
  | Hooks (`SetWindowsHookEx`) | 14 |
  | COM | 14 |
  | Diálogos de abrir y guardar (`OPENFILENAME`) | 13 |

El detalle está en [edita-plugins/audit](https://github.com/alktrip/edita-plugins/tree/main/audit).

## Siguiente

- **Diálogos de los `.rc`:** el intérprete de `DIALOG`/`DIALOGEX`, que los cree con los controles de esta fase. Es lo que convierte a los de la clase B1 en plugins completos, y a la mayoría de los paneles, en paneles con contenido.
- **Lo barato que salió ahora:** `SetWindowSubclass`, los diálogos de abrir y guardar archivo (`GetOpenFileName` sobre `QFileDialog`), `CreateProcess` sobre `QProcess`, `GetMenuItemInfo`.
- **Después, según la auditoría:** los controles comunes más usados (pestañas, listas con columnas) y el dibujo básico sobre `QPainter`.

Pasan las 51 suites y el build no da ningún warning.
