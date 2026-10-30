# Buscar y reemplazar

## La barra de búsqueda

{{actionFind}} abre una barra en la esquina superior derecha del editor. Las coincidencias se
resaltan mientras escribes, y la barra muestra cuántas hay y cuál está seleccionada («3 de 17»).

| Tecla o botón | Acción |
|---|---|
| Intro / Mayús+Intro | Coincidencia siguiente / anterior. |
| Alt+C | Distinguir mayúsculas. |
| Alt+W | Solo palabras completas. |
| Alt+R | Usar una expresión regular. |
| Esc | Cierra la barra y deja seleccionada la coincidencia actual. |

{{actionReplace}} abre la misma barra con el campo de reemplazo. Intro en ese campo reemplaza la
coincidencia actual y pasa a la siguiente, y el botón **Todas** (Ctrl+Alt+Intro) las reemplaza
todas. El botón **›** a la izquierda de la barra muestra u oculta el campo de reemplazo.

El botón **⋯** abre el diálogo Buscar con el texto de la barra. Si prefieres el diálogo de
Notepad++ para Ctrl+F y Ctrl+H, desactiva **Ctrl+F y Ctrl+H abren la barra de buscar en línea**
(página Búsqueda de {{actionPreferences}}).

## El diálogo Buscar

El diálogo tiene tres pestañas: **Buscar**, **Reemplazar** y **Buscar en archivos**. Comparten el
texto que se busca y estas opciones:

- **Solo palabras completas** y **Coincidir mayúsculas y minúsculas**.
- **Dar la vuelta**: después de la última coincidencia, seguir desde el principio del documento.
- **Hacia atrás**: buscar hacia arriba.
- **En la selección**: buscar o reemplazar solo dentro del texto seleccionado.
- **Modo de búsqueda**: Normal, Extendido o Expresión regular (ver abajo).

Además de **Buscar siguiente**, la pestaña Buscar tiene **Contar**, **Buscar todo en el documento
actual** y **Buscar todo en todos los documentos abiertos**; las dos últimas muestran las
coincidencias en el panel de [resultados](#el-panel-de-resultados). La pestaña Reemplazar tiene
**Reemplazar**, **Reemplazar todo** y **Reemplazar todo en todos los documentos abiertos** (Edita
pide confirmación antes de esta última, salvo que se desactive en la página Búsqueda).

Después de usar el diálogo, estas órdenes repiten la búsqueda sin abrirlo:

| Orden | Qué hace |
|---|---|
| {{actionFindNext}} / {{actionFindPrevious}} | Coincidencia siguiente / anterior de la última búsqueda. |
| {{actionFindSelectedNext}} / {{actionFindSelectedPrevious}} | Busca el texto seleccionado (o la palabra del cursor). |

**Rellenar el campo Buscar con la selección** (página Búsqueda) pone la selección en el campo de
búsqueda al abrir el diálogo o la barra.

## Modos de búsqueda

### Normal

El texto se busca tal como está escrito.

### Extendido

Como Normal, pero estas secuencias representan caracteres especiales:

| Secuencia | Carácter |
|---|---|
| `\n` | Salto de línea (LF) |
| `\r` | Retorno de carro (CR) |
| `\t` | Tabulación |
| `\0` | Carácter nulo |
| `\\` | Barra invertida |
| `\xFF` | El carácter con ese código hexadecimal |
| `\dNNN`, `\oNNN`, `\bNNNNNNNN`, `￿` | Código decimal, octal, binario o Unicode |

Por ejemplo, buscar `\r\n` y reemplazar por `\n` convierte los fines de línea de Windows a Unix.

### Expresiones regulares

Edita usa las mismas expresiones regulares que Notepad++ (Boost, compatibles con Perl). Los
elementos más usados:

| Expresión | Coincide con |
|---|---|
| `.` | Cualquier carácter (también los fines de línea con **. coincide con salto de línea**) |
| `\d`, `\w`, `\s` | Un dígito, un carácter de palabra, un espacio |
| `[abc]`, `[^abc]`, `[a-z]` | Uno de esos caracteres, cualquier otro, un rango |
| `*`, `+`, `?`, `{2,5}` | Repetición: 0 o más, 1 o más, 0 o 1, de 2 a 5 |
| `*?`, `+?` | Lo mismo, lo menos posible |
| `^`, `$` | Principio y final de línea |
| `\b` | Principio o final de una palabra |
| `(…)`, `(?<nombre>…)` | Un grupo (numerado o con nombre) |
| `a|b` | `a` o `b` |
| `(?=…)`, `(?!…)` | Seguido / no seguido de |

En el reemplazo, `$1` o `\1` es el texto del grupo 1, `$&` o `$0` la coincidencia entera, y
`\U…\E`, `\L…\E` lo pasan a mayúsculas o minúsculas. Por ejemplo, buscar `(\w+)@(\w+)\.com` y
reemplazar por `\U$2\E: $1` convierte `ana@ejemplo.com` en `EJEMPLO: ana`.

Para probar una expresión antes de usarla, usa el
[probador de expresiones regulares](#probador-de-expresiones-regulares).

## Buscar en archivos

{{actionFindInFiles}} abre la pestaña Buscar en archivos. Además del texto y las opciones:

- **Directorio**: la carpeta donde buscar. **Seguir el documento actual** usa la carpeta del
  documento actual.
- **Filtros**: los nombres de los archivos que se buscan, separados por espacios: `*.cpp *.h`. Un
  filtro que empieza por `!` excluye archivos: `*.* !*.min.js`. `!\build` excluye la carpeta
  `build` del primer nivel, y `!+\node_modules` excluye todas las carpetas con ese nombre a
  cualquier profundidad.
- **En todas las subcarpetas** y **En carpetas ocultas**.

**Buscar todo** muestra las coincidencias de todos los archivos en el panel de resultados.
**Reemplazar en archivos** reemplaza en todos los archivos encontrados, también en los que no
están abiertos, después de pedir confirmación. El menú del clic derecho del Explorador de
carpetas también tiene **Buscar en archivos…**, para buscar en una carpeta de trabajo.

## El panel de resultados

Los resultados de **Buscar todo** y de **Buscar en archivos** aparecen en el panel de resultados
({{key:actionNextResult}} va al siguiente). Cada búsqueda es un grupo con el texto buscado y sus
opciones; dentro, cada archivo con sus líneas, y el texto encontrado resaltado.

- Doble clic (o Intro) en una línea abre el archivo en esa coincidencia.
- {{actionNextResult}} y {{actionPreviousResult}} recorren los resultados sin tocar el panel.
- La orden **Panel de resultados** (F7) muestra u oculta el panel.

## Probador de expresiones regulares

{{actionRegexTester}} abre un panel para construir y probar expresiones regulares:

- **Expresión**: la expresión regular. Las coincidencias se resaltan en el texto de prueba
  mientras escribes, cada grupo con su color.
- **Texto**: el texto de prueba. **Del editor** copia la selección (o todo el documento).
- La lista muestra cada coincidencia con su posición y sus grupos (número, nombre y texto).
- **Reemplazo**: escribe un reemplazo para ver el resultado en la vista previa de abajo.
- **Referencia rápida** (el botón junto a la expresión): un menú con la sintaxis; elige un
  elemento para insertarlo en la expresión.
- **Usar en Buscar…** abre el diálogo Buscar con la expresión y las opciones.

El probador usa el mismo motor que el diálogo Buscar, así que lo que coincide aquí coincide igual
en el documento.

## Ir a

| Orden | Qué hace |
|---|---|
| {{actionGoTo}} | Pide un número de línea y va ahí. |
| {{actionGoToFile}} | Abre un archivo por su nombre (ver [Crear y abrir archivos](02-files.md#crear-y-abrir-archivos)). |
| {{actionGoToSymbol}} | Muestra las funciones, clases y otros símbolos del documento; escribe para filtrar. |
| {{actionGoToMatchingBrace}} | Va al paréntesis o llave que forma pareja con el del cursor. |

En el campo de {{actionGoToFile}}, empieza con `:` para ir a una línea (`:120`, o `:120:5` para
línea y columna), con `@` para ir a un símbolo y con `>` para ejecutar una orden.

## Marcar texto

El **resaltado inteligente** marca todas las apariciones de la palabra seleccionada en la parte
visible del documento (y de la otra vista, con **También en la otra vista**). Se configura en la
página Resaltado de {{actionPreferences}}: distinguir mayúsculas, palabras completas o las
opciones del diálogo Buscar.

Los **estilos de token** colorean una palabra en todo el documento con uno de cinco colores, para
seguirla en un texto largo:

- **Estilo para todas las apariciones** colorea todas las apariciones de la palabra seleccionada
  (o de la palabra del cursor): {{item:actionStyleAll1}}, {{item:actionStyleAll2}},
  {{item:actionStyleAll3}}, {{item:actionStyleAll4}}, {{item:actionStyleAll5}}.
- **Estilo para esta aparición** colorea solo la selección: {{item:actionStyleOne1}},
  {{item:actionStyleOne2}}, {{item:actionStyleOne3}}, {{item:actionStyleOne4}},
  {{item:actionStyleOne5}}.
- **Quitar estilo** quita un color: {{item:actionUnstyle1}}, {{item:actionUnstyle2}},
  {{item:actionUnstyle3}}, {{item:actionUnstyle4}}, {{item:actionUnstyle5}}, o todos con
  {{item:actionUnstyleAll}}.
- **Ir al siguiente estilo** e **Ir al anterior estilo** van a la palabra siguiente o anterior de
  un color: {{item:actionNextStyle1}}, {{item:actionNextStyle2}}, {{item:actionNextStyle3}},
  {{item:actionNextStyle4}}, {{item:actionNextStyle5}}; {{item:actionPreviousStyle1}},
  {{item:actionPreviousStyle2}}, {{item:actionPreviousStyle3}}, {{item:actionPreviousStyle4}},
  {{item:actionPreviousStyle5}}. {{item:actionNextFoundMark}} y {{item:actionPreviousFoundMark}}
  hacen lo mismo con los textos marcados con el estilo «Find Mark Style» (lo usan los plugins y
  los scripts).

Los colores de los estilos se cambian en {{actionStyleConfigurator}} (Estilos globales).

## Marcadores

Un marcador señala una línea para volver a ella después. Se muestra como un símbolo en el margen;
pulsa el margen para poner o quitar uno.

| Orden | Qué hace |
|---|---|
| {{actionToggleBookmark}} | Pone o quita el marcador de la línea actual. |
| {{actionNextBookmark}} / {{actionPreviousBookmark}} | Va al marcador siguiente / anterior (dando la vuelta). |
| {{actionClearBookmarks}} | Quita todos los marcadores del documento. |
| {{actionInverseBookmarks}} | Marca las líneas sin marcador y desmarca las demás. |
| {{actionCutMarkedLines}} / {{actionCopyMarkedLines}} | Corta / copia las líneas marcadas. |
| {{actionPasteMarkedLines}} | Sustituye cada línea marcada por el portapapeles. |
| {{actionDeleteMarkedLines}} | Elimina las líneas marcadas. |
| {{actionDeleteUnmarkedLines}} | Elimina las demás: deja solo las marcadas. |

## Historial de cambios

El historial de cambios muestra las líneas cambiadas desde que se abrió el archivo: modificadas y
sin guardar, guardadas, y devueltas a su texto original. En la página Márgenes de
{{actionPreferences}}, **Historial de cambios** elige si se muestra en el margen, como marcas en el
texto, en los dos sitios o en ninguno.

- {{actionNextChange}} y {{actionPreviousChange}} van al bloque de líneas cambiadas siguiente o
  anterior.
- {{actionClearChangeHistory}} olvida el historial (y también el de deshacer).
