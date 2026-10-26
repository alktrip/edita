# Fase 33 — Mapa del documento mejorado y buscar en línea: resultado

Fecha: 2026-10-26

Dos mejoras que **no existen en Notepad++**. Con ellas se termina la lista de mejoras visuales que se acordó antes de las funcionales.

## Qué se construyó

| Parte | Qué hace |
|---|---|
| **Mapa del documento** (`ui/Panels`, `DocumentMapPanel`) | El mapa ahora tiene un margen de 4 px que muestra los cambios de Git (añadido, modificado, borrado) con los colores del margen del editor, y también los marcadores. Las búsquedas («Marcar todo», el resaltado inteligente y el de la barra de búsqueda) se pintan con relleno (`INDIC_FULLBOX`), así que se ven aunque el texto sea diminuto. La capa de la zona visible ahora cubre todo el mapa: atenúa lo que queda fuera de la zona, la enmarca con el color de acento y dibuja una raya en la línea del cursor. Se adapta al tema claro y al oscuro |
| **Buscar en línea** (`ui/FindBar`) | Una barra flotante arriba a la derecha de cada vista que reemplaza al diálogo para Ctrl+F y Ctrl+H. Resalta todas las coincidencias mientras se escribe (con 80 ms de espera e indicador 28), salta a la más cercana desde donde estaba el cursor y muestra «3 de 17», «Sin resultados» o «Expresión regular no válida» (en rojo). Tiene interruptores para mayúsculas, palabra completa y expresión regular (Alt+C, Alt+W, Alt+R); Intro va a la siguiente, Mayús+Intro a la anterior y Esc cierra la barra y quita el resaltado. La fila de reemplazar tiene «Reemplazar» (Intro: reemplaza una y pasa a la siguiente) y «Todas» (Ctrl+Alt+Intro, que se deshace en un solo paso). El botón «⋯» abre el diálogo completo de N++ con el mismo texto. Usa el mismo motor que el diálogo (`search/SearchEngine`) |
| F3 / Mayús+F3 | Repiten la última búsqueda, sea de la barra o del diálogo |
| Barra de desplazamiento | Las marcas de la barra de desplazamiento incluyen las coincidencias de la barra de búsqueda |
| Preferencia | En Búsqueda: «Ctrl+F y Ctrl+H abren la barra de buscar en línea» (`EditaInlineFind`), activada de serie. Sin ella, Ctrl+F abre el diálogo, como en N++ |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Ctrl+F abre la barra y no el diálogo. Al escribir, se resaltan todas, se selecciona la primera y aparece «1 de 4». Intro y Mayús+Intro recorren las coincidencias dando la vuelta; Alt+C y Alt+W filtran; una expresión regular válida y una inválida dan lo esperado; F3 sigue a la barra; Esc cierra y limpia | ✅ `tst_findbar`: `findWhileTyping` |
| Ctrl+H muestra la fila de reemplazar. Intro reemplaza una aunque la búsqueda esté pendiente; «Todas» reemplaza el resto y se deshace en un paso; «⋯» abre el diálogo con el mismo texto; sin la preferencia, Ctrl+F abre el diálogo | ✅ `replaceAndMoreOptions` |
| El mapa tiene el margen de marcas, las búsquedas con relleno y la capa de zona en todo el mapa | ✅ `documentMapShowsMarks` |

Las pruebas encontraron un error: si se pulsaba Intro en reemplazar justo después de escribir, no se reemplazaba nada, porque la búsqueda todavía estaba esperando su turno. Ahora siguiente, anterior y reemplazar hacen esa búsqueda pendiente antes de actuar. También se revisaron capturas en claro y en oscuro. Pasan las 38 suites y el build no da ningún warning.
