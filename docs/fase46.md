# Fase 46 — Pestaña Marcar y Copiar el texto con estilo: resultado

Fecha: 2026-10-30

Primera fase del bloque «lo que falta de Notepad++». Al escribir el manual se vio que las órdenes
«Ir al siguiente/anterior estilo › Marcado al buscar» no tenían a dónde ir: faltaba la pestaña
Marcar, que es la que usa ese estilo.

## Qué se construyó

| Parte | Equivale en N++ a | Qué hace |
|---|---|---|
| Pestaña **Marcar** del diálogo Buscar | `MARK_DLG` de `FindReplaceDlg` | Cuarta pestaña (N++ la pone después de Buscar en proyectos, que Edita no tiene). Con el campo de búsqueda, las opciones y los modos de siempre, más **Poner un marcador en la línea** (`_doMarkLine`) y **Borrar las marcas en cada búsqueda** (`_doPurge`), y los botones **Marcar todo**, **Borrar todas las marcas** y **Copiar el texto marcado**. Intro en el campo marca |
| Marcar todo | `processAll(ProcessMarkAll)` | Pinta cada coincidencia con el indicador 31 («Find Mark Style», `SCE_UNIVERSAL_FOUND_STYLE`). Con «marcador en la línea», pone un marcador en cada línea de la coincidencia (también en las vacías, como `^$`, que no se pueden pintar). «En la selección» se aplica siempre, como en N++ (`RangeOperation::MarkAll` en el motor). Mensaje «Marcar: 3 coincidencias en todo el documento» |
| Borrar las marcas | `clearMarks` | En la selección o en todo el documento, y los marcadores si la casilla está activada |
| Menú Buscar › **Marcar…** (Ctrl+M) | `IDM_SEARCH_MARK` | Abre la pestaña, con la selección en el campo si así lo piden las preferencias |
| Buscar › **Copiar el texto con estilo** | `IDM_SEARCH_STYLE1..5TOCLIP`, `ALLSTYLESTOCLIP`, `MARKEDTOCLIP` y `markedTextToClipboard` | Los textos de un estilo de token, de todos o los marcados, en el orden del documento, uno por línea (con un salto al final si hay más de uno) y separados por `----` si alguno ocupa varias líneas |
| `config.xml` | `<FindHistory bookmarkLine purge>` | Las dos casillas se recuerdan con los nombres de N++ |
| Atajos y macros | `IDM_*` | Las órdenes nuevas tienen sus números de N++ (43054–43061) en `shortcuts.xml`; las de copiar se pueden grabar en macros |

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Fin de línea al copiar | Siempre CR LF | El del documento | En Linux, CR LF en el portapapeles estorba |
| Grabar «Marcar todo» en una macro | Sí | No | Edita aún no graba las búsquedas del diálogo en macros (tampoco Buscar siguiente ni Reemplazar) |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Marca todas las coincidencias; se acumulan o se borran antes; «Marcado al buscar» salta a ellas; regex inválida | ✅ `tst_mark`: `marksEveryMatch` |
| Marcadores en las líneas, borrar en la selección con sus marcadores, marcar en la selección, borrar todo con y sin marcadores, coincidencias vacías | ✅ `bookmarksAndSelection` |
| Copiar lo marcado y los estilos de token, en orden, con «----» entre textos de varias líneas | ✅ `copiesStyledText` |
| Ctrl+M abre la pestaña; sus botones solo en ella; Marcar todo y Borrar desde el diálogo; Intro; las casillas se guardan | ✅ `markTabOfTheDialog` |
| El manual describe la pestaña y las órdenes nuevas en los dos idiomas | ✅ `tst_manual` |

Pasan las 46 suites y el build no da ningún warning.
