# Fase 41 — Fragmentos de código: resultado

Fecha: 2026-10-29

Fragmentos de código (*snippets*) con el formato de VS Code. **No existen en Notepad++.**

## Qué se construyó

| Parte | Qué hace |
|---|---|
| **Sintaxis** (`core/Snippet`) | La de VS Code y LSP: marcadores `$1`, `${1:texto}` (también anidados), elecciones `${1\|uno,dos\|}` (se pone la primera), el final `$0` (si falta, al final del texto), variables `$NOMBRE`, `${NOMBRE}` y `${NOMBRE:por defecto}`, y los escapes `\$`, `\}`, `\\`. Un marcador repetido es un espejo: se escribe en todos sus sitios a la vez |
| **Archivos** | Los JSON de VS Code: `prefix` (texto o lista), `body` (texto o lista de líneas) y `description`, con comentarios `//` y `/* */` y comas sobrantes. Edita trae `data/snippets/` para C, C++, Python, JavaScript, TypeScript, HTML, CSS, Bash, Markdown, Java, Rust, Go, PHP, SQL y Lua, más `global.json` (fecha, fecha y hora, UUID). Los del usuario van en `~/.config/edita/snippets/<lenguaje>.json` y sustituyen a los de Edita con el mismo nombre o prefijo. Se pueden copiar los de VS Code tal cual |
| **Tab** (`ui/Snippets`, `SnippetController`) | El prefijo antes del cursor y Tab insertan el fragmento. El prefijo tiene que ser una palabra entera: «forwhile» no expande `while`. Si está abierta la lista de autocompletado (al escribir «for» aparece la palabra clave), el fragmento gana. Cada línea del fragmento lleva la sangría de la línea actual, y sus tabuladores se convierten en la sangría del documento (tabuladores o espacios) |
| Marcadores | Tab va al siguiente y Mayús+Tab al anterior. Los espejos quedan seleccionados a la vez (con varias selecciones de Scintilla), y los sitios del fragmento se marcan con un recuadro punteado. Los marcadores siguen al texto mientras se escribe. Al llegar a `$0`, al pulsar Esc o al sacar el cursor del fragmento, se termina. El fragmento entero se deshace en un solo paso |
| Variables | `TM_FILENAME`, `TM_FILENAME_BASE`, `TM_DIRECTORY`, `TM_FILEPATH`, `TM_SELECTED_TEXT`, `TM_CURRENT_LINE`, `TM_CURRENT_WORD`, `TM_LINE_NUMBER`, `TM_LINE_INDEX`, `CLIPBOARD`, `CURRENT_YEAR`, `CURRENT_YEAR_SHORT`, `CURRENT_MONTH`, `CURRENT_MONTH_NAME`, `CURRENT_DATE`, `CURRENT_DAY_NAME`, `CURRENT_HOUR`, `CURRENT_MINUTE`, `CURRENT_SECOND`, `UUID`, `RANDOM` y `RANDOM_HEX` |
| Órdenes | Editar › «Insertar fragmento…» (Ctrl+Alt+J): la lista de fragmentos del lenguaje con buscador. Lo seleccionado se sustituye por el fragmento y es `$TM_SELECTED_TEXT`. Configuración › «Editar fragmentos…»: abre el archivo del usuario del lenguaje actual (si no existe, lo crea con una explicación y un ejemplo); al guardarlo, se vuelven a leer |
| LSP | Los elementos de autocompletado que son fragmentos (`${1:x}`) se insertan con el mismo controlador, así que Tab recorre sus marcadores |
| Preferencia | Autocompletado › «Tab tras el prefijo de un fragmento lo inserta» (`EditaSnippetsTab`), activada de serie |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Marcadores, espejos, anidados, `$0`, elecciones, variables, escapes; archivos con comentarios, comas sobrantes, body en lista y varios prefijos | ✅ `tst_snippets`: `parsing` |
| «for» + Tab en una línea sangrada: texto y sangría, primer marcador seleccionado, los tres espejos a la vez, Mayús+Tab, `$0` termina, Tab normal después, deshacer, prefijo entero, Esc | ✅ `tabExpandsAndNavigates` |
| Los del usuario mandan y se suman los globales; «Insertar fragmento» con la selección y `$TM_FILENAME`; «Editar fragmentos» abre el del lenguaje; sin la preferencia, Tab no expande | ✅ `userSnippetsAndPicker` |
| Varias definiciones de LSP: la lista para elegir (fase 37) | ✅ `tst_lsp`: `navigationAndEdits` |

Las pruebas encontraron dos errores:
- Con la lista de autocompletado abierta, Tab elegía la palabra de la lista en lugar de expandir el fragmento.
- La lista con buscador de «Insertar fragmento» e «Ir a la definición» (con varias definiciones) usaba la de la barra de estado antes de que existiera, y fallaba si aún no se había abierto. Ahora se crea al pedirla. El caso de varias definiciones de la fase 37 no tenía prueba; ya la tiene.

Se revisaron capturas en claro y en oscuro. Pasan las 44 suites y el build no da ningún warning.
