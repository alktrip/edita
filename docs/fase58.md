# Fase 58 — Capa de compatibilidad v2, parte 4: controles comunes, bucles de mensajes y dibujo

Fecha: 2026-11-04

## Por qué

La [auditoría](https://github.com/alktrip/edita-plugins/tree/main/audit) repetida en la [fase 53](fase53.md) señalaba lo que más frenaba a los plugins de la clase B2: los controles comunes (pestañas, listas con columnas, tooltips; unos 25 plugins), el dibujo propio con GDI (unos 22) y los bucles de mensajes propios (13). Esta fase los añade a la capa.

## Qué se construyó

Todo está en `plugin-sdk/compat/npp_window.cpp`, con las declaraciones en el encabezado nuevo `npp_compat_comctl.h` (lo incluye `npp_compat_user.h`). Los valores de los mensajes, las estructuras y las macros son los del SDK de Windows (`commctrl.h`, `winuser.h`, `wingdi.h`, `windowsx.h`), en sus variantes A y W.

| Parte | Qué hace |
|---|---|
| **Pestañas** (`SysTabControl32`) | Sobre `QTabWidget`, con páginas vacías: las páginas de verdad son las ventanas del plugin, que las coloca con `TCM_ADJUSTRECT`. Hay `TCM_INSERTITEM`/`GETITEM`/`SETITEM` (texto y `lParam`), `DELETEITEM`, `GETCURSEL`/`SETCURSEL` (sin notificar, como en Windows), `SETCURFOCUS`, `GETITEMRECT` y `ADJUSTRECT`. Si cambia el usuario, llega `TCN_SELCHANGING` con la pestaña anterior todavía elegida (es la que oculta el plugin), y después `TCN_SELCHANGE`. Con `TRUE` en `TCN_SELCHANGING`, la pestaña no cambia |
| **Listas con columnas** (`SysListView32`, modo informe) | Sobre `QTreeWidget`. Hay columnas (insertar una en medio desplaza las demás y sus textos), alineación, anchos (`LVSCW_AUTOSIZE`), elementos y subelementos, `lParam`, estados (selección, foco, casillas como imagen de estado), `LVM_GETNEXTITEM`, `LVM_FINDITEM` (por `lParam` o por texto), `LVM_SORTITEMS` con la función del plugin, `LVM_HITTEST`, `LVM_GETITEMRECT`, colores y los estilos `LVS_SINGLESEL`, `LVS_SORTASCENDING`, `LVS_NOCOLUMNHEADER`, `LVS_EX_CHECKBOXES` y `LVS_EX_FULLROWSELECT`. Notifica `LVN_ITEMCHANGED` de cada elemento que cambia de selección, foco o casilla (también cuando lo cambia el plugin, como en Windows), `LVN_INSERTITEM`, `LVN_DELETEITEM` (antes de borrar: el plugin libera ahí su `lParam`), `LVN_DELETEALLITEMS`, `LVN_COLUMNCLICK`, `LVN_KEYDOWN`, `LVN_ITEMACTIVATE` y `NM_CLICK`/`NM_DBLCLK`/`NM_RCLICK`/`NM_RETURN` con `NMITEMACTIVATE` |
| **Árboles** (`SysTreeView32`) | Sobre `QTreeWidget`. Un `HTREEITEM` es su `QTreeWidgetItem`, y la capa lleva la cuenta de los vivos, así que uno borrado no se usa. Hay insertar (`TVI_FIRST`, `TVI_LAST`, `TVI_SORT`, después de otro), navegar (todos los `TVGN_*`), `TVM_EXPAND` (sin notificar, como en Windows), seleccionar, `GETITEM`/`SETITEM` (texto, `lParam`, estado, negrita, casillas, `cChildren`), contar, `ENSUREVISIBLE`, ordenar, `GETITEMRECT` y `HITTEST`. Los elementos con `cChildren = 1` muestran el botón de expandir sin tener hijos: el plugin los rellena en `TVN_ITEMEXPANDING`. Notifica `TVN_SELCHANGING` (en la que `TreeView_GetSelection` aún da la anterior; con `TRUE`, no cambia) y `TVN_SELCHANGED` con `TVC_BYMOUSE`, `TVC_BYKEYBOARD` o `TVC_UNKNOWN`. También `TVN_ITEMEXPANDING` (con `TRUE` no se expande) y `TVN_ITEMEXPANDED`, `TVN_KEYDOWN`, `TVN_DELETEITEM` (los hijos primero) y los `NM_*` |
| **Barras de progreso, deslizadores y tooltips** | `msctls_progress32` sobre `QProgressBar`: rangos, posición, paso y `PBM_STEPIT`, que da la vuelta como en Windows, y marquesina. `msctls_trackbar32` sobre `QSlider`: lo que hace el usuario llega al padre como `WM_HSCROLL`/`WM_VSCROLL` con los códigos `TB_*`. `tooltips_class32`: cada herramienta es el `toolTip` de su ventana (`TTF_IDISHWND`, textos de recursos) |
| **Macros** | `TabCtrl_*`, `ListView_*`, `TreeView_*` y las de `windowsx.h` (`Edit_SetText`, `ComboBox_AddString`, `Button_GetCheck`, `ListBox_*`…) |
| **Bucles de mensajes** | `GetMessage`, `PeekMessage`, `DispatchMessage`, `TranslateMessage`, `PostQuitMessage`, `PostThreadMessage` (también desde otro hilo), `WaitMessage`, `GetQueueStatus` y `MsgWaitForMultipleObjects`, sobre el bucle de eventos de Qt. Lo que llega por `PostMessage` a una ventana se entrega mientras el plugin espera. `GetMessage` devuelve los mensajes de la cola del hilo (`WM_QUIT`, `PostThreadMessage`) y, cada vez que Qt atiende algo, un `WM_NULL`: así un bucle del tipo `while (!terminado && GetMessage(…))` vuelve a mirar su variable |
| **Dibujo** | El repintado de Qt de las clases registradas y los diálogos es `WM_ERASEBKGND` (`DefWindowProc` rellena con `hbrBackground`) seguido de `WM_PAINT`. `BeginPaint` da un DC con el `QPainter` del widget. Hay rellenos y marcos (también con `(HBRUSH)(COLOR_xxx + 1)`), `Rectangle`, `RoundRect`, `Ellipse`, líneas, polígonos, píxeles, `TextOut`/`ExtTextOut` (con `SetTextAlign` y el fondo opaco o transparente), `DrawText` (alineación, ajuste de línea, `&`, puntos suspensivos y `DT_CALCRECT`), `DrawEdge`, `DrawFocusRect`, plumas con estilo y grosor, `SaveDC`/`RestoreDC` e `IntersectClipRect`. Los DC en memoria (`CreateCompatibleDC` con su mapa de bits de 1 × 1, como en Windows; `CreateCompatibleBitmap`) dibujan en un `QImage`, que `BitBlt`/`StretchBlt` copian, con las operaciones de bits más usadas. `InvalidateRect` pide repintar y `UpdateWindow` repinta en el acto. Los DC se registran: uno liberado o el de un `WM_PAINT` que ya terminó no se usan |
| **RECT** | `SetRect`, `CopyRect`, `IsRectEmpty`, `PtInRect`, `InflateRect`, `OffsetRect`, `IntersectRect`, `UnionRect`, `SubtractRect`, `EqualRect` |

Los controles comunes de los `.rc` (`CONTROL "", id, "SysTreeView32", …`) se crean igual: antes eran un recuadro con el nombre de la clase.

## Divergencias

| Tema | Windows | Edita | Motivo |
|---|---|---|---|
| Coordenadas del dibujo | GDI | Las mismas (los rectángulos excluyen el borde derecho y el de abajo; las líneas, el último punto), comprobadas píxel a píxel | — |
| Dibujar fuera de `WM_PAINT` (`GetDC`) | Pinta en la ventana | El DC mide texto, pero no dibuja | Qt solo pinta un widget dentro de su repintado |
| Texto | ClearType o en escala de grises | El antialiasing de Qt | — |
| `GetMessage` | Devuelve todos los mensajes de la ventana | Devuelve los de la cola del hilo y `WM_NULL`; los de las ventanas se entregan directamente | Los eventos de Qt no son mensajes de una cola; el plugin ve el mismo efecto |
| `TranslateMessage` / `IsDialogMessage` | Generan `WM_CHAR` / mueven el foco con Tab | No hacen nada (devuelven `FALSE`) | La capa ya entrega `WM_CHAR` y Qt mueve el foco |
| Cabecera de la lista (`LVM_GETHEADER`) | Una ventana `SysHeader32` | `NULL` | No está |
| Pestañas | El control dibuja el marco de la página | El de `QTabWidget` | — |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Pestañas: insertar (también delante de la elegida), textos sin `&`, `lParam`, `SETCURSEL` sin notificar, clic: `TCN_SELCHANGING` con la anterior y `TCN_SELCHANGE` con la nueva, `TRUE` impide el cambio, `ADJUSTRECT` de ida y vuelta, borrar | ✅ `tst_npp_compat`: `tabControl` |
| Listas: columnas insertadas en medio, alineación y anchos, elementos, subelementos, `lParam`, buscar, `LVN_ITEMCHANGED` al cambiar el estado desde el plugin, casillas, clic, doble clic y Intro (`NM_*`, `LVN_ITEMACTIVATE`, `LVN_KEYDOWN`), clic en la cabecera, `HITTEST`/`GETITEMRECT`, ordenar con la función del plugin, `LVN_DELETEITEM` de cada uno y borrar una columna | ✅ `listView` |
| Árboles: insertar (`TVI_SORT`), navegar, `cChildren`, `TVM_EXPAND` sin notificar, expandir del usuario (`TVN_ITEMEXPANDING`/`EXPANDED`, `TRUE` lo impide), seleccionar desde el plugin (`TVN_SELCHANGING` con la anterior, `TVC_UNKNOWN`, `TRUE` lo impide), con el ratón (`TVC_BYMOUSE`) y con el teclado (`TVC_BYKEYBOARD`, `TVN_KEYDOWN`), `GETITEMRECT`/`HITTEST`, `TVN_DELETEITEM` con los hijos primero, un `HTREEITEM` borrado no vale | ✅ `treeView` |
| Progreso (rango, posición, paso, `STEPIT`, `DELTAPOS`), deslizador (lo que hace el plugin no notifica; la tecla del usuario, `WM_HSCROLL` con `TB_LINEDOWN` y `TB_ENDTRACK`), tooltips (añadir, cambiar, quitar) y macros de `windowsx.h` | ✅ `progressTrackbarAndTooltips` |
| Un bucle del plugin que espera a una variable termina; `PostQuitMessage`; `PeekMessage` con y sin filtro; `PostThreadMessage`; `DispatchMessage`; `MsgWaitForMultipleObjects` con tiempo y con un evento de otro hilo | ✅ `messageLoops` |
| Dibujo comprobado píxel a píxel: relleno, rectángulo con pluma y pincel, línea sin el último punto, doble búfer con `BitBlt`, texto; `InvalidateRect` + `UpdateWindow` repintan; `DT_CALCRECT`; un mapa de bits seleccionado no se borra, y volver a seleccionar el anterior lo suelta | ✅ `painting` |
| Las funciones de `RECT` | ✅ `rects` |
| Un `SysTreeView32` de un `.rc` es un árbol de verdad; una clase que no hay (`SysIPAddress32`) sigue siendo un recuadro con su nombre | ✅ `resourceDialogs` |
| Las 7 recetas del catálogo compilan y pasan su prueba con el SDK nuevo | ✅ `scripts/build-all.sh` |
| Pasan todas las suites; la capa y sus pruebas no dan avisos con GCC ni con clang | ✅ |

## Auditoría repetida

Con el mismo código de los plugins (el de la fase 53) y el SDK nuevo:

| SDK de Edita | A (compila) | B1 (falta la interfaz) | B2 (port) |
|---|---:|---:|---:|
| Fase 53 | 9 | 6 | 92 |
| Fase 58 | **10** | **4** | 93 |

- **Lo que se buscaba ya no frena a ninguno.** `NM_CLICK` faltaba en 33 plugins, `PAINTSTRUCT` en 23, `FillRect` en 17, `ListView_InsertColumn` en 16, `TabCtrl_GetCurSel` en 14 y `PM_REMOVE` en 12; ahora, en ninguno. Los errores de compilación de la auditoría bajan de 59 039 a 54 932, y 68 plugins tienen menos que antes.
- **Pero casi todos tienen varios bloqueos**, así que solo uno más compila entero: NX ASCII Database Helper, que no declara licencia y no puede ir al catálogo. NppBplistPlugin pasa de B1 a B2 porque ahora su interfaz compila y el fallo llega después, al enlazar: le falta `libplist`, que no viene en su repositorio.
- **Lo que frena ahora:**
  - hooks (`SetWindowsHookEx`, 18 plugins);
  - los controles que la capa aún no tiene (SysLink, barras de herramientas, listas de imágenes; 16);
  - COM (14);
  - el dibujo propio de los controles (`WM_DRAWITEM`, `NM_CUSTOMDRAW`; 12);
  - la información de versión (`GetFileVersionInfo`; 12);
  - WinINet/WinHTTP (12);
  - los temas de Windows (11).

## Siguiente

La fase 59: mudar MIME Tools y XML Tools de `data/plugins/recipes` al catálogo. Después, una parte 5 de la capa con lo que mide la auditoría: hooks, SysLink, barras de herramientas, listas de imágenes, el dibujo propio de los controles y la información de versión.
