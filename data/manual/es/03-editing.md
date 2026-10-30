# Edición

## Edición básica

| Orden | Notas |
|---|---|
| {{actionUndo}} / {{actionRedo}} | Deshacer y rehacer no tienen límite mientras el documento está abierto. |
| {{actionCut}}, {{actionCopy}}, {{actionPaste}} | Sin selección, cortar y copiar toman la línea entera (casilla **Copiar o cortar la línea entera si no hay selección**, página Edición de {{actionPreferences}}). |
| {{actionDelete}} | Borra la selección, o el carácter que sigue al cursor. |
| {{actionSelectAll}} | Selecciona todo el documento. |

Otras teclas del editor:

| Teclas | Acción |
|---|---|
| Ctrl+Retroceso / Ctrl+Supr | Borrar la palabra anterior / siguiente. |
| Ctrl+Mayús+Retroceso / Ctrl+Mayús+Supr | Borrar hasta el principio / el final de la línea. |
| Ctrl+L | Cortar la línea actual. |
| Ctrl+Mayús+X | Copiar la línea actual. |
| Ctrl+Mayús+L | Borrar la línea actual (sin copiarla). |
| Ctrl+T | Intercambiar la línea actual con la anterior. |
| Insert | Cambiar entre insertar y sobrescribir. |
| Ctrl+rueda del ratón | Ampliar o reducir. |

Todas las teclas del editor se pueden cambiar en {{actionShortcutMapper}} (pestaña **Órdenes de
Scintilla**).

## Selección

- **Mayús+flechas** seleccionan caracteres y líneas; **Ctrl+Mayús+flechas**, palabras.
- **Doble clic** selecciona una palabra; **triple clic**, una línea. Un clic en el margen de los
  números de línea selecciona la línea entera; arrastrando ahí se seleccionan varias.
- **Selección rectangular (de columna)**: arrastra con **Alt** pulsada, o usa **Alt+Mayús+flechas**.
  Lo que escribas con una selección rectangular se escribe en todas las líneas a la vez. En
  algunos escritorios Alt+arrastrar mueve la ventana; entonces usa el teclado, o cambia ese atajo
  del escritorio.
- **Varios cursores**: **Ctrl+clic** añade otro cursor (u otra selección, con Ctrl+arrastrar).
  Lo que escribas, borres o pegues se aplica en todos los cursores. Esc vuelve a un solo cursor.
  Es la casilla **Edición múltiple** de la página Edición.
- **Espacio virtual** (casilla de la página Edición) deja que el cursor y la selección
  rectangular pasen del final de las líneas.

## Editor de columnas

{{actionColumnEditor}} inserta texto o números en la columna del cursor, en cada línea desde el
cursor hasta el final del documento, o en cada línea de una selección rectangular.

- **Texto que insertar**: el mismo texto en cada línea.
- **Número que insertar**: una serie que empieza en **Número inicial** y crece de **Incremento** en
  incremento. **Repetir** escribe cada número varias veces antes de pasar al siguiente.
  **Relleno** completa con ceros o espacios para que todos los números tengan el mismo ancho, y
  **Formato** elige decimal, hexadecimal, octal o binario.

Por ejemplo, con una selección rectangular de diez líneas, número inicial 1, incremento 1 y
relleno con ceros, las líneas reciben de `01` a `10`.

## Operaciones con líneas

Las órdenes del submenú **Editar › Líneas** trabajan sobre las líneas seleccionadas, o sobre la
línea actual si no hay selección:

| Orden | Qué hace |
|---|---|
| {{actionDuplicateLine}} | Copia la línea (o la selección) debajo. |
| {{actionLineUp}} / {{actionLineDown}} | Sube o baja las líneas. |
| {{actionSplitLines}} | Parte las líneas largas en la línea de margen (o en el ancho de la ventana). |
| {{actionJoinLines}} | Une las líneas seleccionadas en una. |
| {{actionBlankLineAbove}} / {{actionBlankLineBelow}} | Inserta una línea vacía encima o debajo. |
| {{actionRemoveEmptyLines}} | Quita las líneas vacías. |
| {{actionRemoveBlankLines}} | Quita también las que solo tienen espacios o tabulaciones. |
| {{actionRemoveDuplicateLines}} | Deja solo la primera copia de cada línea repetida. |
| {{actionRemoveConsecutiveDuplicates}} | Quita una línea cuando es igual a la anterior. |
| {{actionSortReverse}} | Invierte el orden de las líneas. |
| {{actionSortRandom}} | Desordena las líneas. |

### Ordenar

Se ordenan las líneas seleccionadas, todo el documento si no hay selección, o las **columnas** de
una selección rectangular: las líneas se ordenan según el texto seleccionado, pero se mueven
enteras.

| Ascendente | Descendente | Orden |
|---|---|---|
| {{actionSortLexAsc}} | {{actionSortLexDesc}} | Por código de carácter (mayúsculas antes que minúsculas). |
| {{actionSortCaseInsAsc}} | {{actionSortCaseInsDesc}} | Sin distinguir mayúsculas y minúsculas. |
| {{actionSortLocaleAsc}} | {{actionSortLocaleDesc}} | Alfabético, con las reglas de tu idioma (á junto a a). |
| {{actionSortIntAsc}} | {{actionSortIntDesc}} | Como números enteros: 2 antes que 10. |
| {{actionSortCommaAsc}} | {{actionSortCommaDesc}} | Como decimales con coma: `3,5`. |
| {{actionSortDotAsc}} | {{actionSortDotDesc}} | Como decimales con punto: `3.5`. |
| {{actionSortLengthAsc}} | {{actionSortLengthDesc}} | Por longitud. |

Si en un orden numérico una línea no es un número, no se cambia nada y Edita dice qué línea es.

## Mayúsculas y minúsculas

Las órdenes del submenú **Editar › Convertir** cambian la selección (o cada selección, si hay
varias):

| Orden | Ejemplo |
|---|---|
| {{actionUpperCase}} | `HOLA MUNDO` |
| {{actionLowerCase}} | `hola mundo` |
| {{actionProperCase}} | `Hola Mundo` (el resto de cada palabra en minúsculas) |
| {{actionProperCaseBlend}} | `Hola Mundo` (el resto de cada palabra sin cambios: `iPhone` → `IPhone`) |
| {{actionSentenceCase}} | `Hola mundo. Adiós.` |
| {{actionSentenceCaseBlend}} | Igual, sin pasar el resto a minúsculas. |
| {{actionInvertCase}} | `hOLA mUNDO` |
| {{actionRandomCase}} | `hOlA MuNDo` |

## Comentarios

Edita usa los símbolos de comentario del lenguaje del documento (los de `langs.xml`, o los del
lenguaje definido por el usuario).

| Orden | Qué hace |
|---|---|
| {{actionToggleComment}} | Comenta las líneas, o las descomenta si ya lo están. |
| {{actionLineComment}} | Añade el símbolo de comentario de línea (`//`, `#`…) a cada línea. |
| {{actionLineUncomment}} | Lo quita. |
| {{actionStreamComment}} | Encierra la selección en un comentario de bloque (`/* … */`). |
| {{actionStreamUncomment}} | Quita el comentario de bloque que rodea al cursor o a la selección. |

## Espacios y tabulaciones

| Orden | Qué hace |
|---|---|
| {{actionTrimTrailing}} | Quita los espacios del final de las líneas. |
| {{actionTrimLeading}} | Quita los espacios del principio de las líneas. |
| {{actionTrimBoth}} | Las dos cosas. |
| {{actionEolToSpace}} | Une las líneas con un espacio. |
| {{actionTrimAll}} | Quita los espacios de los dos extremos y une las líneas. |
| {{actionTabToSpace}} | Cambia las tabulaciones por espacios. |
| {{actionSpaceToTabAll}} | Cambia grupos de espacios por tabulaciones. |
| {{actionSpaceToTabLeading}} | Lo mismo, solo en la sangría. |

La macro {{actionMacro0}} quita los espacios del final y guarda, en un solo paso.

## Sangría

**Tab** aumenta la sangría de las líneas seleccionadas y **Mayús+Tab** la reduce. El ancho de la
tabulación y si inserta espacios se eligen en {{actionPreferences}} (página Tabulación y sangría)
para cada lenguaje, o para el documento actual desde el campo de la sangría de la barra de
estado. Los lenguajes con su propia configuración en `langs.xml` la conservan.

**Sangría automática** decide qué pasa al pulsar Intro: **Básica** conserva la sangría de la
línea anterior, y **Avanzada** además aumenta la sangría después de `{` (y de `:` en Python) y la
reduce al escribir `}`. **Retroceso quita un nivel de sangría** borra un nivel completo de una
vez.

## Paréntesis y comillas

- Con **Cerrar automáticamente** (página Autocompletado), al escribir `(`, `[`, `{`, `"` o `'` se
  inserta el de cierre, y en HTML y XML `<p>` inserta `</p>`.
- El paréntesis junto al cursor y su pareja se resaltan. {{actionGoToMatchingBrace}} salta a la
  pareja y {{actionSelectMatchingBraces}} selecciona todo lo que hay entre ellos.
- **Paréntesis por colores según el nivel** (página Edición) da un color distinto a cada nivel.

## Solo lectura y fin de línea

{{actionReadOnly}} impide cambiar el documento. Las órdenes de fin de línea se describen en
[Fines de línea](02-files.md#fines-de-linea).
