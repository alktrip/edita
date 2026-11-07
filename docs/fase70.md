# Fase 70 — Las páginas de Preferencias que faltaban

Fecha: 2026-11-07

## Por qué

Es el cuarto bloque de [lo que tiene Notepad++ y Edita no](paridad-npp.md): cinco de las páginas de Preferencias de N++ que Edita no tenía. Con ellas llegan los **enlaces que se pueden pulsar**, que N++ tiene de serie y Edita no tenía.

La asociación de archivos (la sexta página) queda para después, con los paquetes .deb y AUR: en Linux depende de cómo se instala Edita (el archivo `.desktop` y los tipos MIME).

## Cómo lo hace N++ y qué hace Edita

| Página de N++ | En Edita |
|---|---|
| **Toolbar** (ocultar, iconos Fluent de dos tamaños, colores) | **Barra de herramientas**: ocultarla y el tamaño de los iconos (pequeños, estándar, grandes), que estaban en General. Los colores son de los iconos Fluent de N++; los de Edita siguen al tema |
| **Language** (menú compacto, lenguajes ocultos, barra invertida en SQL) | **Lenguaje**: el menú agrupado por inicial (como estaba) o con todos seguidos; dos listas para quitar lenguajes del menú y volver a ponerlos (botones o doble clic; «Texto normal» no se puede quitar); la barra invertida en SQL (`sql.backslash.escapes`). Se guarda como N++: `<GUIConfig name="langsExcluded" gr0…gr12 langMenuCompact>` (un bit por `LangType`, que es el mismo número que en N++) y `backSlashIsEscapeCharacterForSql` en `MISC`. El menú se rehace al cambiar, con los lenguajes de los plugins de lexer |
| **Delimiter** (caracteres de palabra, Ctrl + doble clic) | **Delimitadores**: los caracteres ASCII que también forman parte de una palabra (`SCI_SETWORDCHARS` sobre las letras, los dígitos, `_` y todo lo que no es ASCII; va por documento, porque Scintilla lo guarda en el documento); sin espacios ni repetidos; vacío, la lista de serie. **Ctrl + doble clic** selecciona lo que hay entre el delimitador de apertura y el de cierre (por defecto paréntesis): con dos distintos, la pareja más interior que contiene el clic; con dos iguales (comillas), los más cercanos, sin contar `\"`; en la línea o, con «También en varias líneas», en todo el documento (`SCN_DOUBLECLICK` de N++). `<GUIConfig name="wordCharList">` y `<GUIConfig name="delimiterSelection">` |
| **Performance** (Large File Restriction) | **Rendimiento**: activarla, el tamaño (1-4096 MB, 200 de serie; antes fijo), y en un archivo grande, permitir o no el autocompletado, el resaltado inteligente, el paréntesis pareja (`EditorView::setBraceMatchEnabled`) y los enlaces, y apagar el ajuste de línea para todos al abrir uno. Se guarda como `<GUIConfig name="largeFileRestriction">`. El aviso de N++ para los archivos de 2 GB o más no aplica |
| **Cloud & Link** (configuración en la nube, enlaces) | **Nube y enlaces**: una carpeta donde guardar toda la configuración, como N++: se escribe en `cloud/choice` de la carpeta de siempre, la primera vez se copian allí los `.xml` y `userDefineLangs/` que no estén (`writeSettingsFilesOnCloudForThe1stTime`) y vale al reiniciar (`paths::configDir()` decide una vez). Se quedan en la carpeta local los plugins instalados (son binarios de cada equipo), el socket de la instancia única y los reportes de fallos (`paths::baseConfigDir()`). **Enlaces** (`ui/UrlLinks`): la detección de N++ (`isUrl`, `scanToUrlStart`, `scanToUrlEnd`, quitar la puntuación final y los paréntesis sin pareja), con `QUrl` en lugar de `InternetCrackUrl`; el indicador `URL_INDIC` (8) en las líneas visibles de las dos vistas, subrayado o no, con recuadro al pasar el ratón o no (`<GUIConfig name="URL">` 0-4); un doble clic lo abre; otros esquemas en `uriCustomizedSchemes` (los de N++ de serie) |

Además:
- **Indicadores:** los marcadores de los fragmentos pasan del 8 al 30, que N++ no usa: el 8 es el de los enlaces, como en N++ (algunos plugins lo leen).
- `showPreferences` guarda la carpeta de la nube al Aceptar y al Aplicar, y avisa de que vale al reiniciar.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| La detección de N++: esquemas, consulta con comillas, fragmento, puntuación y paréntesis finales, mayúsculas, no ASCII, esquemas propios, lo que no vale | ✅ `tst_preferencepages`: `detectUrls` |
| Los enlaces se marcan en lo que se ve, el modo cambia el estilo, el doble clic abre, el modo 0 los quita | ✅ `clickableLinks` |
| Ctrl + doble clic: pareja interior, comillas con escapes, en la línea y en varias | ✅ `delimiters` |
| Caracteres de palabra añadidos, también en otro documento, y vuelta a los de serie (con los no ASCII) | ✅ `wordCharacters` |
| El menú Lenguaje sin agrupar, sin los ocultos, y marcando el lenguaje del documento | ✅ `languageMenu` |
| La barra invertida en SQL llega al estilo | ✅ `sqlBackslash` |
| Archivos grandes: el tamaño, el ajuste de línea, sin paréntesis pareja ni enlaces y con ellos si se permiten; sin restricción | ✅ `largeFiles` |
| Las páginas: ocultar la barra, mover lenguajes, los caracteres y delimitadores que no valen, las casillas de los enlaces y su `urlMode`, la nube | ✅ `dialogPages` |
| La nube: carpeta que no existe, copia de la configuración, vale al reiniciar, quitarla | ✅ `settingsOnCloud` |
| Todas las opciones nuevas se guardan en `config.xml` y vuelven igual | ✅ `tst_preferences` |
| Pasan las 60 suites, el build no da avisos (GCC 16 y clang), y compila con el GCC 12 y Qt 6.8.3 de la CI | ✅ |
