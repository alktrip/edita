# Fase 10 — Lenguajes definidos por el usuario: resultado

Fecha: 2026-10-20 · Entorno: el mismo que en las fases anteriores

## Qué se construyó

| Módulo | Equivale en N++ a | Qué hace |
|---|---|---|
| `syntax/UserLanguages` | `UserLangContainer`, `NppParameters::feedUserLang`/`feedUserSettings`/`feedUserKeywordList`/`feedUserStyles`, `insertUserLang2Tree`, `getUserDefinedLangNameFromExt`, `exportUDLToFile` | Lee y escribe el **formato de N++** (`<UserLang name ext udlVersion darkModeTheme>` con `Settings`, `KeywordLists` y `Styles`), incluidas las conversiones de formatos antiguos (anteriores a 2.0: `Words1`, `Operators`, `Folder+`, `Delimiters` de 6 caracteres, `Comment` con 0/1/2, nombres de estilo como `KEYWORD1` o `DELIMINER1`). Fuentes por orden: `~/.config/edita/userDefineLang.xml`, `~/.config/edita/userDefineLangs/*.xml` y los que trae la aplicación |
| | `GlobalMappers` | Nombres de listas y estilos idénticos a N++, así que los archivos son intercambiables en los dos sentidos |
| | `retrieve`/`convertTo` de `UserDefineDialog.cpp` | Casillas de "Comments" y "Delimiters" codificadas con el número de casilla (`00// 03/* 04*/`), con grupos `((EOL))` |
| `SyntaxStyler::setUserLexer` | `ScintillaEditView::setUserLexer` | Lexer `user` de Lexilla (el `LexUser` de N++ ya incluido desde la Fase 0) con las mismas propiedades `userDefine.*`, las palabras clave en el mismo orden (con `\v`/`\b` para frases entre comillas), anidamiento por estilo y colores. `udlName` y `currentBufferID` (en N++, punteros) son aquí un identificador por definición, que cambia al editarla, y otro por documento |
| Detección | `Buffer::setFileName` | Los lenguajes del usuario se miran **antes** que los de N++, por extensión o por nombre de archivo completo; si coinciden uno claro y uno oscuro, gana el claro |
| `Buffer::userLanguage` | `Buffer::_userLangExt` | Nombre del lenguaje del usuario del documento. La barra de estado muestra "Lenguaje definido por el usuario - nombre", y `session.xml` lo guarda en `lang` como N++ |
| Menú **Lenguaje > Definido por el usuario** | menú `IDM_LANG_USER*` | Lista de lenguajes, **Definir tu lenguaje…**, **Importar…** y abrir la carpeta `userDefineLangs` |
| `ui/UserLanguageDialog` | `UserDefineDialog` y sus pestañas, `StylerDlg` | Pestañas: Plegado (código 1 y 2, comentarios, compacto), Palabras clave (8 grupos con modo prefijo), Comentarios y números (línea/bloque, plegado de comentarios, posición del comentario de línea, prefijos, extras, sufijos, rango y separador decimal), Operadores y delimitadores (operadores 1 y 2, 8 delimitadores con escape) y **Estilos** (texto y fondo, que pueden ser transparentes, fuente, tamaño, negrita, cursiva, subrayado y anidamiento en comentarios y delimitadores). Botones Nuevo, Guardar como, Renombrar, Eliminar, Importar, Exportar y Guardar |
| Al guardar | `UserDefineDialog` → recoloreado | Se escribe el XML (uno nuevo va a `userDefineLangs/<nombre>.xml`; uno de `userDefineLang.xml` se queda en ese archivo). Los documentos sin lenguaje elegido a mano se vuelven a detectar, los que usaban un lenguaje borrado pasan a texto normal y todo se recolorea |
| Lenguajes incluidos | `bin/userDefineLangs` de N++ | Los dos Markdown que N++ trae preinstalados (claro y oscuro, de Markdown-plus-plus, licencia MIT), en `share/edita/userDefineLangs`. Uno instalado no se puede borrar, y al guardarlo queda una copia en la carpeta del usuario |

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Diálogo | Ventana acoplable con vista previa en vivo y un diálogo de estilo por elemento | Diálogo con pestaña de estilos; los cambios se ven al pulsar Guardar | Más sencillo, con las mismas opciones |
| Modo oscuro (`darkModeTheme`) | Elige la variante según el modo de la interfaz | Se lee y se conserva; se elige la variante clara | Edita aún no tiene modo oscuro de la interfaz |
| Transparencia de la ventana del diálogo, "acoplar" | Sí | No | Propio de Win32 |
| Pares de autocompletado por lenguaje del usuario (`autoCompletion/<nombre>.xml`) | Sí | Se busca el archivo por el nombre corto del lenguaje de N++ | Pendiente |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| El Markdown que trae N++ se lee entero (opciones, prefijos, listas, 24 estilos) y reconoce sus extensiones | ✅ `readsTheMarkdownShippedWithNotepadPlusPlus` |
| Casillas de comentarios y delimitadores: lectura y composición idénticas a las de N++, con grupos | ✅ `slotsOfCommentsAndDelimiters` |
| Archivos de formato antiguo convertidos como N++ | ✅ `oldFormatsAreConverted` |
| Escribir y volver a leer conserva todo; el XML usa los nombres de N++ | ✅ `writeAndReadBack` |
| Carga desde el archivo del usuario, su carpeta y los instalados; se prefiere la variante clara; los instalados no se borran y se copian al guardarlos | ✅ `registryLoadsUserAndInstalledFilesAndPrefersLightThemes` |
| Un archivo con la extensión de un lenguaje del usuario se colorea con LexUser: palabras clave, cadenas, comentarios de línea y de bloque, con los colores y la negrita del lenguaje | ✅ `filesAreColouredWithTheUserLanguage` |
| Elegirlo en el menú, volver a un lenguaje de N++ y restaurarlo desde la sesión | ✅ `languageMenuAndSession` |
| Diálogo: crear, rechazar duplicados, editar, guardar (el documento abierto pasa a ese lenguaje y se colorea), renombrar y eliminar | ✅ `dialogCreatesEditsAndRemovesLanguages` |
| Importar y exportar archivos de N++ | ✅ `importNotepadPlusPlusFile` |

Además se revisó visualmente un `.md` coloreado con el Markdown preinstalado y el diálogo con ese lenguaje cargado.

Tests: `tst_udl` 11 (contando `initTestCase` y `cleanupTestCase`). Pasan las 17 suites y el build no da ningún warning.
