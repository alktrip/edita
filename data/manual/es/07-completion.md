# Autocompletado y fragmentos

## Autocompletado

Mientras escribes, Edita puede mostrar una lista de palabras que completan la que estás
escribiendo. Elige una con las flechas e insértala con Intro o Tab; Esc cierra la lista.

Las palabras salen de dos sitios:

- El **archivo de API** del lenguaje: las funciones y palabras clave que Notepad++ conoce para él
  (`autoCompletion/<lenguaje>.xml`). Las funciones tienen su propio icono.
- Las **palabras del documento**.

Si hay un [servidor de lenguaje](08-language-servers.md) para el documento, sus sugerencias
sustituyen a las del archivo de API.

La página Autocompletado de {{actionPreferences}} elige:

| Opción | Significado |
|---|---|
| Mostrar la lista | Nunca, con las funciones del lenguaje, con las palabras del documento o con las dos. |
| A partir de | Cuántos caracteres hay que escribir antes de que aparezca la lista. |
| Ignorar los números | No ofrecer números como palabras. |
| Lista breve | Mostrar solo las palabras que empiezan por lo escrito. |
| Ayuda de parámetros al escribir "(" | Mostrar los parámetros de una función al escribir `(`. |

Las órdenes del submenú **Editar › Autocompletado** abren cada lista cuando quieras:

| Orden | Qué hace |
|---|---|
| {{actionCompleteFunction}} | La lista de funciones y palabras clave (o la del servidor de lenguaje). |
| {{actionCompleteWord}} | La lista de palabras del documento. Si solo hay una, la escribe directamente. |
| {{actionFunctionHint}} | Los parámetros de la función del cursor, con el actual resaltado. Si hay varias sobrecargas, pulsa la ayuda o usa las flechas para recorrerlas. |
| {{actionCompletePath}} | Completa una ruta de archivo que empieza por `/`, `~/` o `./`. |

Las palabras del archivo de API se pueden ampliar: copia el archivo del lenguaje a
`~/.config/edita/autoCompletion/` y edítalo ahí. El formato es el mismo que en Notepad++.

## Fragmentos

Un fragmento es un trozo de código con huecos para rellenar. Escribe su **prefijo** y pulsa
**Tab**: por ejemplo, `for` + Tab en C++ inserta

```
for (int i = 0; i < n; ++i) {

}
```

con `int` seleccionado. Lo que escribas lo sustituye. El hueco siguiente es `i`: lo que escribas
ahí aparece también en los demás sitios donde se usa `i`. **Tab** va al hueco siguiente y
**Mayús+Tab** al anterior. El fragmento termina en su último hueco, al pulsar Esc o cuando el
cursor sale de él, y se deshace entero en un solo paso.

- El prefijo tiene que ser una palabra entera: `forwhile` no expande `while`.
- Si la lista de autocompletado está abierta al pulsar Tab, gana el fragmento.
- Cada línea del fragmento lleva la sangría de la línea actual, y sus tabulaciones se convierten en
  la sangría del documento (tabulaciones o espacios).
- Desactiva **Tab tras el prefijo de un fragmento lo inserta** (página Autocompletado) para usar
  solo la lista.

{{actionInsertSnippet}} muestra la lista de fragmentos del lenguaje con un buscador. Si hay texto
seleccionado, el fragmento lo sustituye, y muchos fragmentos lo usan (por ejemplo, para rodearlo
con una etiqueta o un bloque).

Edita incluye fragmentos para C, C++, Python, JavaScript, TypeScript, HTML, CSS, Bash, Markdown,
Java, Rust, Go, PHP, SQL y Lua, y algunos para todos los lenguajes: `hoy` (la fecha), `ahora` (la
fecha y la hora) y `uuid`.

### Tus propios fragmentos

{{actionEditSnippets}} abre tu archivo de fragmentos del lenguaje del documento actual
(`~/.config/edita/snippets/<lenguaje>.json`); si no existe, se crea con un ejemplo. Al guardarlo,
los fragmentos se vuelven a leer. Tus fragmentos sustituyen a los de Edita con el mismo nombre o
prefijo. El archivo `global.json` guarda fragmentos para todos los lenguajes.

El formato es el de Visual Studio Code, así que sus archivos de fragmentos se pueden copiar tal
cual:

```json
{
  // El nombre del fragmento
  "Mostrar en la consola": {
    "prefix": "log",
    "body": ["console.log('${1:mensaje}', $2);", "$0"],
    "description": "Escribe en la consola"
  }
}
```

- `prefix`: la palabra que lo expande (o una lista de palabras).
- `body`: el texto, como cadena o como lista de líneas.
- `description`: se ve en la lista de {{actionInsertSnippet}}.

Se admiten comentarios (`//`, `/* */`) y comas sobrantes. En el cuerpo:

| Sintaxis | Significado |
|---|---|
| `$1`, `$2`… | Huecos, en el orden en que Tab los recorre. Un número repetido es un espejo: se escribe en todos sus sitios a la vez. |
| `${1:texto}` | Un hueco con un texto por defecto, que queda seleccionado al llegar. Los huecos se pueden anidar. |
| `${1|uno,dos,tres|}` | Una elección; se escribe la primera opción. |
| `$0` | Donde termina el cursor (de serie, al final). |
| `$NOMBRE`, `${NOMBRE:por defecto}` | Una variable (ver abajo). |
| `\$`, `\}`, `\\` | Un `$`, `}` o `\` literal. |

Las variables:

| Variable | Valor |
|---|---|
| `TM_SELECTED_TEXT` | El texto seleccionado |
| `TM_CURRENT_LINE`, `TM_CURRENT_WORD` | La línea actual, la palabra del cursor |
| `TM_LINE_NUMBER`, `TM_LINE_INDEX` | El número de línea, desde 1 / desde 0 |
| `TM_FILENAME`, `TM_FILENAME_BASE` | El nombre del archivo, con / sin extensión |
| `TM_DIRECTORY`, `TM_FILEPATH` | La carpeta, la ruta completa |
| `CLIPBOARD` | El texto del portapapeles |
| `CURRENT_YEAR`, `CURRENT_YEAR_SHORT`, `CURRENT_MONTH`, `CURRENT_MONTH_NAME`, `CURRENT_DATE`, `CURRENT_DAY_NAME`, `CURRENT_HOUR`, `CURRENT_MINUTE`, `CURRENT_SECOND` | La fecha y la hora |
| `UUID`, `RANDOM`, `RANDOM_HEX` | Un identificador o un número al azar |
