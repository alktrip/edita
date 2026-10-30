# Lenguajes y temas

## Resaltado de sintaxis

Edita colorea más de 90 lenguajes, con las mismas definiciones que Notepad++. El lenguaje de un
documento se elige al abrirlo, por su extensión o su nombre de archivo (`Makefile`,
`CMakeLists.txt`…), y en algunos casos por su primera línea (`#!/bin/bash`, `<?xml`). La barra de
estado lo muestra.

Para cambiarlo, elige otro en el menú **Lenguaje** (agrupados por inicial) o pulsa el campo del
lenguaje en la barra de estado, que abre una lista con buscador. {{actionLang0}} quita el
resaltado.

Cada lenguaje define más que los colores: los símbolos de comentario que usa
{{actionToggleComment}}, las palabras del autocompletado, el plegado y el analizador de la Lista de
funciones.

## Lenguajes definidos por el usuario

Si un lenguaje no está incluido, puedes definirlo: sus palabras clave, comentarios, números,
operadores, delimitadores (como las cadenas), plegado y colores. Los archivos son compatibles con
los `userDefineLang.xml` de Notepad++, así que sirven las muchas definiciones que existen para él.

- {{actionDefineLanguage}} abre el editor de lenguajes definidos por el usuario.
- **Lenguaje › Definido por el usuario › Importar…** añade las definiciones de un archivo XML.
- **Abrir la carpeta de lenguajes** abre `~/.config/edita/userDefineLangs/`, donde cada lenguaje
  es un archivo.

Edita incluye Markdown (en una variante clara y otra oscura). Los lenguajes del usuario se eligen
por extensión antes que los incluidos.

### El editor de lenguajes del usuario

Arriba, elige el lenguaje que quieres editar. **Nuevo…**, **Guardar como…**, **Renombrar…** y
**Quitar** administran los lenguajes, e **Importar…** y **Exportar…** los leen o escriben como
archivos XML. **Extensión** indica las extensiones (separadas por espacios, sin punto) que usan
este lenguaje. Las pestañas:

| Pestaña | Qué define |
|---|---|
| Plegado | Las palabras o símbolos que abren y cierran un bloque plegable (en el código y en los comentarios), y el estilo por defecto. |
| Palabras clave | Ocho grupos de palabras, cada uno con su estilo. Con **Modo prefijo**, una palabra coincide si empieza por una de la lista. |
| Comentarios y números | Comentarios de línea y de bloque, y cómo se escriben los números: prefijos (`0x`), sufijos, rangos y separador decimal. |
| Operadores y delimitadores | Operadores, y hasta ocho delimitadores (por ejemplo `"` … `"` para las cadenas) con un carácter de escape. |
| Estilos | El color, el fondo, la fuente y el anidamiento de cada elemento. |

Pulsa **Guardar** para aplicar los cambios: los documentos con ese lenguaje se vuelven a colorear.

## Apariencia y temas

Edita tiene un modo claro y uno oscuro. **Apariencia**, en la página General de
{{actionPreferences}}, elige claro, oscuro o **Seguir al sistema** (de serie), que cambia en el
mismo momento en que cambia el escritorio. La página de inicio tiene la misma opción.

Los colores del texto salen de un **tema**. El menú **Ver › Tema** los muestra: el de serie,
Edita Light y Edita Dark, los temas de Notepad++ (Monokai, Solarized, Zenburn…) y temas modernos
como One Dark, Dracula, Catppuccin, Nord y Tokyo Night. Cada modo tiene su tema: el que eliges se
usa para el modo actual, y al cambiar de modo vuelve el tema del otro modo.

El mismo menú tiene:

- {{actionReloadSyntax}}: vuelve a leer los archivos de configuración, después de editarlos a
  mano.
- **Editar el archivo de estilos** y **Editar langs.xml**: abren los archivos del tema actual y de
  los lenguajes, para editarlos como texto.

Edita usa la letra monoespaciada del sistema en lugar de las fuentes de Windows que piden muchos
temas (Courier New, Consolas). Desactiva **Usar la letra monoespaciada del sistema** en la página
Edición para usar la fuente del tema.

## Configurador de estilos

{{actionStyleConfigurator}} cambia los colores y las fuentes de un tema sin editar XML:

- **Tema**: el tema que se edita (al elegir uno también se aplica).
- **Lenguaje**: **Estilos globales** (el texto por defecto, la línea actual, la selección, los
  márgenes, la llave pareja, los estilos de token…) o un lenguaje.
- **Estilo**: un elemento del lenguaje (palabra clave, comentario, cadena…).
- A la derecha: los colores del texto y del fondo, la fuente, el tamaño, negrita, cursiva y
  subrayado. Los colores que un estilo no usa están desactivados.
- Para un lenguaje, sus **extensiones** y las **del usuario** que también lo usan. Para los estilos
  con palabras clave, **Palabras clave del usuario** para añadir las tuyas.
- **Global override** (en Estilos globales) impone una fuente, un tamaño o un color a todos los
  estilos.

Los cambios se ven en el editor y se guardan con **Guardar y cerrar**. Un tema instalado con Edita
se copia antes a `~/.config/edita/themes/`, y la copia lo sustituye.
