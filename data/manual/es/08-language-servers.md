# Servidores de lenguaje

Un **servidor de lenguaje** es un programa que entiende un lenguaje de programación: encuentra los
errores mientras escribes, completa los nombres con lo que de verdad está disponible, sabe dónde
está definida cada función y puede renombrar un símbolo en todo el proyecto. Edita usa el Language
Server Protocol (LSP), el mismo que Visual Studio Code, así que funciona con los servidores hechos
para él.

Edita no incluye los servidores: instala los de tus lenguajes con el administrador de paquetes de
tu sistema (o con `npm`, `pip`, `cargo`…). Al abrir un archivo, Edita usa el primer servidor
conocido de su lenguaje que esté instalado. No hay que configurar nada.

| Lenguaje | Servidores (por orden de preferencia) |
|---|---|
| C, C++, Objective-C | `clangd` |
| Python | `basedpyright`, `pyright`, `pylsp`, `jedi-language-server`, `ruff` |
| JavaScript, TypeScript | `typescript-language-server` |
| Rust | `rust-analyzer` |
| Go | `gopls` |
| Shell | `bash-language-server` |
| Lua | `lua-language-server` |
| JSON, CSS, HTML | los servidores de Visual Studio Code (`vscode-json-language-server`…) |
| YAML, TOML, Markdown | `yaml-language-server`, `taplo`, `marksman` |
| Java, PHP, LaTeX, Zig… | `jdtls`, `intelephense`, `texlab`, `zls`… |

La página Servidores de lenguaje de {{actionPreferences}} los muestra todos y dice cuáles están
instalados. Ahí también puedes escribir otra orden para un lenguaje (por ejemplo
`clangd --background-index`), u `off` para desactivar ese lenguaje, y apagar los servidores por
completo.

La carpeta del proyecto que se envía al servidor es la carpeta de trabajo que contiene al archivo
o, si no hay, la carpeta más cercana hacia arriba con un archivo de proyecto
(`compile_commands.json`, `Cargo.toml`, `package.json`, `.git`…). Para C y C++, clangd necesita
un `compile_commands.json` para saber las rutas de inclusión y las opciones del proyecto; CMake lo
genera con `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`.

## Diagnósticos

Los errores se subrayan con una línea ondulada roja, los avisos en amarillo y la información en
azul; las pistas se marcan con puntos grises. Deja el ratón sobre la línea para leer el mensaje.
Los errores y avisos también se marcan en la barra de desplazamiento.

La barra de estado muestra cuántos errores y avisos tiene el documento actual (`⨯ 4 ⚠ 1`);
púlsalo para abrir el panel Problemas. {{actionProblems}} muestra los diagnósticos de todos los
documentos abiertos; pulsa uno para ir a él.

## Autocompletado e información

- Mientras escribes un nombre, o después de `.`, `->` o `::`, la lista muestra lo que propone el
  servidor, con un icono por tipo (función, variable, clase, módulo…). Muchos elementos insertan un
  [fragmento](07-completion.md#fragmentos) con los parámetros: Tab los recorre. Algunos también
  añaden lo que haga falta en otro sitio, como un `#include`.
- Al escribir `(` o `,`, o con {{actionFunctionHint}}, aparece la firma de la función con el
  parámetro actual resaltado.
- Dejar el ratón medio segundo sobre un nombre muestra su tipo y su documentación.
- Con **Pistas al final de las líneas** (página Servidores de lenguaje), los tipos deducidos y los
  nombres de los parámetros de las líneas a la vista se muestran en gris al final de cada línea:
  `total: int  ·  (ancho, alto)`.

## Navegación

| Orden | Qué hace |
|---|---|
| {{actionGoToDefinition}} | Abre el archivo donde está definido el símbolo del cursor. Si hay varias definiciones, aparece una lista. |
| {{actionFindReferences}} | Muestra todos los usos del símbolo en el [panel de resultados](04-search.md#el-panel-de-resultados); F4 los recorre. |
| {{actionWorkspaceSymbol}} | Busca una función, clase o variable por su nombre en todo el proyecto, mientras escribes. |
| {{actionGoToSymbol}} | Los símbolos del documento actual. |

## Cambiar el código

| Orden | Qué hace |
|---|---|
| {{actionRenameSymbol}} | Pide un nombre nuevo y renombra el símbolo en todos los archivos del proyecto. Los abiertos se cambian en su sitio; los demás se abren sin guardar, para revisarlos. Cada archivo se deshace en un solo paso. |
| {{actionFormatDocument}} | Da formato al documento (o a la selección, si el servidor puede) con la sangría del documento. |
| {{actionCodeActions}} | Los arreglos y refactorizaciones que ofrece el servidor para la línea o la selección (añadir un include que falta, corregir un nombre mal escrito, extraer una función…). Las preferidas salen primero. |

Cuando hay acciones para la línea del cursor, aparece una **bombilla** en el margen; púlsala para
verlas.

## Cuando algo no funciona

- {{actionLspLog}} abre el registro del servidor del documento actual: los mensajes que escribió y
  sus errores.
- {{actionLspRestart}} lo reinicia, por ejemplo después de cambiar `compile_commands.json` o de
  instalar una dependencia.
- Los archivos de más de 8 MB no se envían al servidor.
