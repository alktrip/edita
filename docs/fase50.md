# Fase 50 — Menú Ventana: resultado

Fecha: 2026-10-30

Última fase del bloque «lo que falta de Notepad++» (46 Marcar, 47 sesiones, 48 Ejecutar, 49
ocultar líneas).

## Qué se construyó

| Parte | Equivale en N++ a | Qué hace |
|---|---|---|
| Menú **Ventana** | `IDR_WINDOWS_MENU` | Después de Ejecutar, antes de Ayuda |
| Ventana › **Ordenar por** | `IDM_WINDOW_SORT_FN_ASC` … `FD_DSC` | Las diez órdenes de N++: nombre, ruta y tipo de la A a la Z y al revés (sin distinguir mayúsculas), tamaño del contenido y fecha de modificación, ascendente y descendente. Ordenan las pestañas de la vista actual sin cambiar el documento activo (`EditorPane::reorder`) |
| Lista de documentos | `WindowsMenu::initPopupMenu`, `IDM_WINDOW_MRU_FIRST` | Al abrir el menú, los documentos de la vista actual (hasta 40), «&1: nombre» con «*» si tienen cambios y el actual marcado; elegir uno cambia a él |
| Ventana › **Ventanas…** | `WindowsDlg` | Lista con nombre, ruta, tipo (la descripción del lenguaje) y tamaño, ordenable por columnas, con varios elegidos: **Activar** (o doble clic), **Guardar**, **Cerrar** (pregunta por los cambios sin guardar), **Ordenar las pestañas** (en el orden de la lista) y **Aceptar** |
| Atajos | `IDM_WINDOW_*` | Las órdenes tienen sus números de N++ (11001–11011) en `shortcuts.xml` |

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Columnas del diálogo | Nombre, Ruta, Tipo, Tamaño | Igual; el tipo es la descripción del lenguaje | — |
| Copiar nombres o rutas desde el diálogo (menú contextual) | Sí | No | El menú de las pestañas ya lo hace |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Ordenar por nombre (sin mayúsculas), tamaño, ruta y tipo; el documento activo no cambia | ✅ `tst_windows`: `sortsTheTabs` |
| El menú muestra los documentos numerados, con «*» y el actual marcado; elegir uno cambia a él; se actualiza al cerrar | ✅ `listsTheDocumentsInTheMenu` |
| Diálogo: filas, ordenar por columna y pasarlo a las pestañas, guardar el elegido, cerrar dos a la vez, activar | ✅ `windowsDialog` |
| Manual en los dos idiomas | ✅ `tst_manual` |

Pasan las 50 suites y el build no da ningún warning.
