# Vistas y paneles

## Paleta de órdenes

{{actionCommandPalette}} abre un campo de búsqueda arriba de la ventana con todas las órdenes de
los menús, de los plugins y de las macros. Escribe parte del nombre, en cualquier orden y aunque
falten letras (`dupl lin` encuentra **Duplicar la línea**), y pulsa Intro para ejecutarla. Cada
orden muestra su menú y su atajo, así que la paleta también sirve para aprenderlos.

![La paleta de órdenes](images/palette.png)

El mismo campo hace otras cosas según el primer carácter:

| Empieza con | Para |
|---|---|
| (nada) | Abrir un archivo: documentos abiertos, recientes y archivos de las carpetas de trabajo ({{actionGoToFile}}). |
| `>` | Ejecutar una orden (es lo que abre {{actionCommandPalette}}). |
| `@` | Ir a un símbolo del documento ({{actionGoToSymbol}}). |
| `:` | Ir a una línea: `:120`, o `:120:5` para línea y columna. |

## Pestañas

El submenú **Ver › Pestaña** tiene todo lo de las pestañas:

- {{actionNextTab}} y {{actionPreviousTab}} van a la pestaña siguiente o anterior;
  {{actionFirstTab}} y {{actionLastTab}}, a la primera o la última; {{actionTab1}} y las
  siguientes, a la pestaña de ese número (con Ctrl y el número del teclado numérico).
- Arrastra una pestaña para cambiarla de lugar, o usa {{actionTabForward}}, {{actionTabBackward}},
  {{actionTabToStart}} y {{actionTabToEnd}}.
- {{actionPinTab}} la fija: las pestañas fijadas van al principio, llevan un alfiler en vez del
  botón de cerrar (pulsarlo la desfija) y {{actionCloseAllButPinned}} no las cierra. Una
  pestaña no pasa arrastrándola de las fijadas a las demás, ni al revés.
- {{actionTabColor1}} y los siguientes marcan la pestaña con uno de cinco colores, para
  distinguir grupos de documentos; {{actionTabColorNone}} lo quita.
- La sesión recuerda qué pestañas están fijadas y sus colores.
- El clic derecho en una pestaña muestra un menú: cerrarla, cerrar las demás o las de un lado,
  moverla o clonarla en la otra vista, guardarla, volver a cargarla, copiar su ruta, su nombre o
  su carpeta, abrir su carpeta en el administrador de archivos o en una terminal, y hacerla de
  solo lectura.

La página General de {{actionPreferences}} controla la barra de pestañas: ocultarla, mostrar un
botón de cerrar en cada pestaña, cerrar un documento con doble clic y los iconos por tipo de
archivo.

## Vista dividida

Edita puede mostrar dos documentos uno al lado del otro (o uno encima del otro):

| Orden | Qué hace |
|---|---|
| {{actionMoveToOtherView}} | Mueve el documento actual a la otra vista. |
| {{actionCloneToOtherView}} | Muestra el mismo documento en las dos vistas: lo que escribes en una aparece en la otra. Sirve para ver dos partes de un archivo largo. |
| {{actionSwitchView}} | Pasa el foco a la otra vista. |
| {{actionOpenInNewInstance}} | Abre el documento en otra ventana de Edita, en la misma línea y con el mismo lenguaje. |
| {{actionMoveToNewInstance}} | Lo mismo, y lo cierra aquí (y esta ventana, si era el único documento). |
| {{actionRotateSplit}} | Pone las vistas una al lado de la otra o una encima de la otra. |

La otra vista se cierra cuando se cierra su último documento. La división se puede arrastrar. La
ventana nueva lee el archivo del disco, así que las dos últimas órdenes lo necesitan guardado; la
ventana nueva no restaura ni guarda la sesión.

Con {{actionSyncScrollV}} y {{actionSyncScrollH}}, las dos vistas se desplazan juntas,
manteniendo la diferencia que tenían al activarlo: sirve para comparar dos versiones de un
archivo línea a línea.

## Pantalla completa, Post-It y siempre visible

- {{actionFullScreen}} ocupa toda la pantalla; la misma tecla vuelve.
- {{actionPostIt}} deja solo el texto, sin marco, menús, barras ni pestañas, como una nota
  adhesiva; la misma tecla vuelve.
- {{actionAlwaysOnTop}} mantiene la ventana de Edita encima de las demás.

## Resumen y seguimiento

- {{actionSummary}} muestra la ruta y las fechas del archivo, y cuenta sus caracteres (sin los
  fines de línea), palabras, líneas y bytes, y lo seleccionado.
- {{actionMonitoring}} sigue un archivo que crece, como un registro: se pone en solo lectura y,
  cada vez que cambia en el disco, se vuelve a leer sin preguntar y va al final, también con
  Edita en segundo plano. Para seguirlo, el archivo tiene que estar guardado.

## El menú Ventana

El menú **Ventana** muestra los documentos de la vista actual, numerados (Alt+N y el número abre
uno), con un `*` en los que tienen cambios sin guardar; el actual está marcado.

**Ordenar por** ordena las pestañas de la vista actual:

| Ascendente | Descendente |
|---|---|
| {{item:actionSortTabsNameAsc}} | {{item:actionSortTabsNameDesc}} |
| {{item:actionSortTabsPathAsc}} | {{item:actionSortTabsPathDesc}} |
| {{item:actionSortTabsTypeAsc}} | {{item:actionSortTabsTypeDesc}} |
| {{item:actionSortTabsSizeAsc}} | {{item:actionSortTabsSizeDesc}} |
| {{item:actionSortTabsModifiedAsc}} | {{item:actionSortTabsModifiedDesc}} |

{{actionWindowsList}} abre una lista de los documentos de la vista actual con su nombre, ruta, tipo y
tamaño. Pulsa una columna para ordenar la lista por ella. Elige uno o varios documentos (con Ctrl o
Mayús) y:

- **Activar** (o doble clic) cambia a ese documento.
- **Guardar** guarda los elegidos que tienen cambios.
- **Cerrar** los cierra (preguntando por los cambios sin guardar).
- **Ordenar las pestañas** deja las pestañas en el orden de la lista.

## Opciones de visualización

| Orden | Qué hace |
|---|---|
| {{actionWrap}} | Ajusta las líneas largas al ancho de la ventana. |
| {{actionShowWhiteSpace}} | Muestra los espacios como puntos y las tabulaciones como flechas. |
| {{actionShowEol}} | Muestra los caracteres de fin de línea (CR, LF). |
| {{actionShowIndentGuides}} | Muestra líneas verticales en cada nivel de sangría. |
| {{actionShowWrapSymbol}} | Marca las líneas que siguen en la fila siguiente. |
| {{actionShowNpc}} | Muestra los caracteres invisibles que no son espacios normales (espacio de no separación, espacio de ancho cero, marcas de dirección…) con su abreviatura en un recuadro: NBSP, ZWSP, RLM… |
| {{actionShowControlChars}} | Muestra los caracteres de control (NUL, ESC, los C1…) y los fines de línea Unicode (NEL, LS, PS) con su abreviatura; sin ella, no se ven. De serie, sí. |
| {{actionShowAll}} | Muestra todo lo anterior a la vez: espacios, tabulaciones, fines de línea y caracteres no imprimibles y de control. |
| {{actionZoomIn}} / {{actionZoomOut}} | Agranda o achica el texto (también Ctrl+rueda del ratón). |
| {{actionZoomRestore}} | Vuelve al tamaño normal. |

Las páginas Edición y Márgenes de {{actionPreferences}} tienen más opciones: resaltar la línea
actual, el grosor y el parpadeo del cursor, desplazar más allá de la última línea, el margen de
los números de línea, el de los marcadores, una línea vertical en una columna (para no escribir
líneas demasiado largas), el estilo del margen de plegado y la vista previa de colores
`#rrggbb` y `rgb()` (pulsa el color para abrir un selector de color).

### Ruta de navegación y encabezado fijo

- **Ruta de navegación encima del editor** muestra las carpetas, el archivo y la clase y la
  función del cursor. Pulsa una parte para abrir la lista de archivos de una carpeta o la de los
  símbolos del documento. En un documento de más de 2 MB, la clase y la función se actualizan al
  volver a su pestaña, no mientras escribes, y solo cuando el cursor está en su primera línea.
- **Encabezado fijo** deja la primera línea del bloque actual (la función, la clase, el
  encabezado) arriba del editor mientras te desplazas dentro de él.

Las dos son casillas de la página Edición.

### Marcas en la barra de desplazamiento

La barra de desplazamiento vertical muestra marcas de los cambios de Git, los marcadores, los
resultados de búsqueda, el resaltado inteligente y la posición del cursor, para ver dónde están
en todo el documento y pulsar para ir ahí. Se desactivan con **Marcas en la barra de
desplazamiento** (página Márgenes).

## Plegado

El margen de plegado muestra un símbolo al principio de cada bloque (una función, una clase, una
etiqueta, una sección de un archivo JSON…). Púlsalo para plegar o desplegar el bloque.

| Orden | Qué hace |
|---|---|
| {{actionFoldAll}} / {{actionUnfoldAll}} | Pliega / despliega todos los bloques. |
| {{actionFoldCurrent}} / {{actionUnfoldCurrent}} | Pliega / despliega el bloque del cursor. |

**Plegar el nivel** y **Desplegar el nivel** pliegan o despliegan todos los bloques de un nivel:
el nivel 1 son los bloques exteriores, el 2 los que están dentro de ellos, y así sucesivamente.

| Plegar | Desplegar |
|---|---|
| {{item:actionFoldLevel1}} | {{item:actionUnfoldLevel1}} |
| {{item:actionFoldLevel2}} | {{item:actionUnfoldLevel2}} |
| {{item:actionFoldLevel3}} | {{item:actionUnfoldLevel3}} |
| {{item:actionFoldLevel4}} | {{item:actionUnfoldLevel4}} |
| {{item:actionFoldLevel5}} | {{item:actionUnfoldLevel5}} |
| {{item:actionFoldLevel6}} | {{item:actionUnfoldLevel6}} |
| {{item:actionFoldLevel7}} | {{item:actionUnfoldLevel7}} |
| {{item:actionFoldLevel8}} | {{item:actionUnfoldLevel8}} |

## Ocultar líneas

{{actionHideLines}} oculta las líneas seleccionadas (o la línea actual), para quitar de en medio una
parte del documento que no necesitas ver, sin plegarla. Un triángulo azul en el margen de los
marcadores, en la línea de arriba y en la de abajo, marca dónde están las líneas ocultas; pulsa
cualquiera de los dos para volver a mostrarlas. Ocultar líneas junto a otras ocultas las une en una
sola sección.

La primera y la última línea del documento no se pueden ocultar. Las líneas ocultas siguen ocultas
al plegar y desplegar, y al cambiar a otro documento y volver; el texto no cambia, y al buscar,
copiar o guardar se incluyen las líneas ocultas.

## Paneles

Los paneles se muestran u ocultan desde el menú Ver. Se pueden arrastrar a cualquier lado de la
ventana, ponerse como pestañas uno encima de otro o separarse como ventanas flotantes. Al volver
a abrir Edita, cada panel está donde lo dejaste: el lado, el tamaño, si flotaba y con qué otros
paneles compartía pestañas. También los paneles de los plugins.

### Lista de documentos

{{actionDocumentList}} muestra los documentos abiertos de las dos vistas. Pulsa uno para cambiar
a él. Es útil cuando hay tantas pestañas que no se leen sus nombres.

### Lista de funciones

{{actionFunctionList}} muestra las funciones, clases y métodos del documento actual, con los
mismos analizadores que Notepad++ (`functionList`). Pulsa uno para ir ahí. El campo de arriba
filtra la lista, y los botones la ordenan alfabéticamente o la vuelven a cargar.

### Mapa del documento

{{actionDocumentMap}} muestra todo el documento en miniatura, con la parte visible resaltada.
Pulsa o arrastra sobre el mapa para moverte por el documento. También muestra los cambios de
Git, los marcadores y los resultados de búsqueda.

### Explorador de carpetas

{{actionFolderAsWorkspace}} muestra un árbol con las carpetas de tu proyecto. Añade carpetas con
{{actionOpenFolderAsWorkspace}} o con el botón **Añadir carpeta** del panel. Doble clic en un
archivo lo abre.

El menú del clic derecho tiene: abrir, copiar la ruta o el nombre, abrir la carpeta que lo
contiene, **Abrir en la terminal**, **Buscar en archivos…** en esa carpeta, volver a cargar y
quitar la carpeta del espacio de trabajo. Los botones de arriba **localizan el archivo actual**
en el árbol y **pliegan todo**.

En un repositorio de Git, el panel muestra la rama y colorea los archivos modificados, nuevos e
ignorados.

### Resultados de búsqueda

Los resultados de **Buscar todo** y de **Buscar en archivos**; ver
[El panel de resultados](04-search.md#el-panel-de-resultados).

### Problemas y Terminal

{{actionProblems}} muestra los errores y avisos de los servidores de lenguaje. {{actionTerminal}}
muestra la terminal integrada y {{actionNewTerminal}} abre una pestaña nueva en ella;
{{actionRunSelectionInTerminal}} envía el texto seleccionado (o la línea actual) a la terminal y
lo ejecuta.
Ver [Servidores de lenguaje](08-language-servers.md) y [Terminal](11-terminal.md).
