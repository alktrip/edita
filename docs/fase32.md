# Fase 32 — Ruta de navegación y encabezado fijo: resultado

Fecha: 2026-10-26

Dos mejoras que **no existen en Notepad++** para orientarse en archivos largos.

## Qué se construyó

| Parte | Qué hace |
|---|---|
| **Ruta de navegación** (`ui/Breadcrumbs`) | Una barra entre las pestañas y el editor de cada vista: `carpeta › carpeta › archivo › clase › función`, según dónde está el cursor. Las carpetas empiezan en la carpeta de trabajo que contiene al archivo; fuera de ellas, las tres últimas (y «…»). **Una carpeta** abre una lista con sus archivos y subcarpetas: elegir un archivo lo abre, una subcarpeta muestra la suya. **El archivo o un símbolo** abre la lista de símbolos del documento con buscador, con el actual marcado; elegir uno va a su línea. Las listas son las mismas de la barra de estado (`QuickPicker`) |
| Símbolos | Los de la Lista de funciones (fase 22): funciones y clases. El bloque de cada uno (de qué línea a qué línea) sale del plegado, del encabezado a su último hijo, buscando el encabezado hasta dos líneas después para la llave en su propia línea. Sin analizador (JSON, YAML, Markdown…), se usan los bloques plegables de los cuatro primeros niveles, con el texto de su primera línea. Se recalculan medio segundo después de editar. Hasta 2 MB |
| Analizador de funciones | Las funciones definidas dentro de una clase ya traen dónde está su clase (`classPos`); N++ lo deja en −1. Así la ruta y la Lista de funciones pueden ir a la clase |
| **Encabezado fijo** (`ui/StickyScroll`) | Al bajar dentro de una función, clase o bloque largo, su primera línea queda fija arriba del editor. Son los encabezados de los bloques plegables (`SCI_GETFOLDPARENT`) que contienen a la primera línea que queda a la vista, hasta 5, los de más adentro. Con la llave en su propia línea, el bloque empieza en `{`, así que se muestra la línea de antes, la firma. Se dibujan con los estilos del editor (colores, fuente, negrita, cursiva, zoom, tabuladores, desplazamiento horizontal) y su número de línea en el margen, con una sombra debajo. Al pasar el ratón se resalta la fila; un clic lleva a esa línea; la rueda sigue desplazando el editor. Con ajuste de línea no se muestra |
| Preferencias | Edición › «Ruta de navegación encima del editor» y «Encabezado fijo» (`EditaBreadcrumbs`, `EditaStickyScroll`), activadas de serie |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Carpeta de trabajo › carpeta › archivo › clase › método dentro de un método; solo la función dentro de una función; nada entre dos | ✅ `tst_navigation`: `breadcrumbsFollowTheCaret` |
| La lista de símbolos con buscador lleva a la línea; la de una carpeta abre el archivo; sin analizador (JSON), los bloques; desactivada | ✅ |
| Arriba del todo, nada fijo; dentro del `for` de una función larga, la firma de la función y el `for`; un clic lleva a la función; desactivado | ✅ `stickyScrollKeepsTheHeaders` |

Se revisaron capturas en claro y en oscuro con un archivo de Edita. En ellas se vio que con la llave en su propia línea el encabezado mostraba solo `{`, y se cambió por la firma. Pasan las 37 suites y el build no da ningún warning.
