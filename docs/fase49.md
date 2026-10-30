# Fase 49 — Ocultar líneas: resultado

Fecha: 2026-10-30

## Qué se construyó

| Parte | Equivale en N++ a | Qué hace |
|---|---|---|
| Ver › **Ocultar líneas** (Alt+H) | `IDM_VIEW_HIDELINES`, `ScintillaEditView::hideLines` | Oculta las líneas de la selección (o la del cursor), nunca la primera ni la última del documento. Pone la marca de apertura (19, `MARK_HIDELINESBEGIN`) en la línea de antes y la de cierre (18, `MARK_HIDELINESEND`) en la de después. Port del algoritmo de N++: las marcas que había dentro se quitan y, si la sección toca otra oculta, se unen en una |
| Marcas en el margen | `hidelines_begin/end` | Un triángulo azul hacia abajo encima de la sección y otro hacia arriba debajo, en el margen de los marcadores |
| Mostrar | `hidelineMarkerClicked`, `showHiddenLines` | Un clic en cualquiera de las dos marcas muestra la sección (sin abrir lo que esté plegado dentro) y quita sus marcas, en lugar de poner un marcador |
| Seguir ocultas | `restoreHiddenLines`, `hideMarkedLines` | Las marcas viven en el documento y que las líneas se oculten es de cada vista: al cambiar de documento en una vista y al desplegar (Desplegar todo, el bloque actual, un nivel, o un clic en el margen) se vuelven a ocultar. Un clon en la otra vista también las oculta |
| Clic en el margen de plegado | `ScintillaEditView::marginClick` | Lo hace ahora la vista, como N++, con las mismas teclas que Scintilla (Mayús: desplegar los hijos; Ctrl: alternar los hijos; Ctrl+Mayús: todo), para volver a ocultar después: el plegado automático de Scintilla no avisa al desplegar |
| Teclas de acceso | — | Alt+H era la tecla de acceso del menú «Help» en inglés: Help (y Ayuda) quedan sin tecla de acceso, como el menú «?» de N++. En español, Ejecutar pasa a «Ejecuta&r», porque Alt+J ya era Lenguaje |

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Mostrar una sección en un clon | La muestra en las dos vistas | En la vista del clic; la otra la muestra al volver a ese documento | Las vistas de Edita no se avisan entre sí de esto |
| Recordar las líneas ocultas en la sesión | No | No | — |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Oculta la selección con sus marcas; nunca la primera ni la última línea; con menos de tres líneas, nada | ✅ `tst_hidelines`: `hidesTheSelectedLines` |
| Secciones contiguas o separadas por una línea se unen | ✅ `adjacentSectionsMerge` |
| Un clic en cualquiera de las marcas las muestra (sin poner un marcador) | ✅ `clickingAMarkerShowsTheSection` |
| Siguen ocultas al cambiar de documento, al plegar y desplegar con el margen y con Plegar/Desplegar todo | ✅ `survivesSwitchingDocumentsAndUnfolding` |
| Alt+H funciona (antes chocaba con la tecla de acceso de Help) | ✅ `altHShortcut` |
| Manual en los dos idiomas | ✅ `tst_manual` |

Pasan las 49 suites y el build no da ningún warning.
