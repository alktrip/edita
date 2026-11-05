# Fase 63 — Lexers de plugins

Fecha: 2026-11-04

## Por qué

Varios plugins de Notepad++ no agregan órdenes: agregan un lenguaje. Traen su propio lexer (el código que reconoce palabras clave, comentarios, cadenas…) y un XML con sus extensiones, palabras clave y estilos. N++ los llama lenguajes «externos». Edita tenía el tipo `LangType::External`, pero no lo usaba: estos plugins compilaban y no hacían nada.

## Cómo lo hace N++

`PluginsManager::loadPluginFromPath`, después de `setInfo` y `getFuncsArray`, busca en la DLL las funciones de Lexilla.

- **Las funciones:** si encuentra `GetLexerCount`, también tienen que estar `GetLexerName` y `CreateLexer`. Cada lexer da un lenguaje con su nombre; uno repetido o vacío se ignora, y caben 30 (`NB_MAX_EXTERNAL_LANG`).
- **El XML:** lee `plugins\Config\<módulo>.xml` (`getExternalLexerFromXmlTree`). Sus `<Languages>` se tratan como los de `langs.xml`, y sus `<LexerStyles>` como los de `stylers.xml`. Algunos plugins escriben ese XML en `setInfo`, si no está.
- **Al aplicar el lenguaje:** `ScintillaEditView::setExternalLexer` llama a `CreateLexer(nombre)` y lo pasa con `SCI_SETILEXER`. Después aplica los estilos del `LexerType` con ese nombre, y las palabras clave de cada estilo con `keywordClass`, junto con las del `<Language>`.
- **La detección:** `getLangFromExt` mira primero las extensiones de los estilos, y entre ellas el `ext` del `LexerType` de un lenguaje externo. Después mira las de `langs.xml`.
- **Para los plugins:** el lenguaje es `L_EXTERNAL` más su índice (`NPPM_GETCURRENTLANGTYPE`), y `NPPM_GETLANGUAGENAME` da su nombre.
- **Los estilos editados** se guardan en el XML del plugin, no en `stylers.xml` ni en el tema.

## Qué cambió

| Dónde | Qué |
|---|---|
| **`syntax/ExternalLexers`** | Los lenguajes de los plugins: su `CreateLexer`, su `<Language>` (extensiones, palabras clave, comentarios) y sus estilos, leídos del XML con los mismos lectores que `langs.xml` y `stylers.xml`. Detección por extensión; guardar los estilos en el XML de cada plugin |
| **`PluginManager`** | Tras `setInfo`, como en N++: `GetLexerCount`, `GetLexerName` y `CreateLexer`, si el plugin los exporta |
| **`SyntaxManager`, `SyntaxStyler`** | Los estilos de los plugins se añaden a la hoja en uso (también al cambiar de tema: no son del tema). `setExternalLexer`, como el de N++. Al guardar desde el Configurador de estilos, los de cada plugin van a su XML (`StyleSheet::save` solo cambia los `LexerType` que el archivo ya tiene) |
| **Documento** | `LangType::External` con el nombre del lenguaje, como los del usuario (`Buffer::setExternalLanguage`). Se detecta por extensión después de los del usuario y antes que `langs.xml`. La sesión lo guarda por su nombre |
| **Interfaz** | Menú Lenguaje (después de los de Edita, antes de «Lenguaje definido por el usuario»), el selector de la barra de estado (se arma con ese menú), la barra de estado (el nombre), la lista de ventanas, comentar con el comentario del XML |
| **Mensajes** | `NPPM_GETCURRENTLANGTYPE` y `NPPM_GETBUFFERLANGTYPE` dan `L_EXTERNAL` + índice; `NPPM_SETCURRENTLANGTYPE` lo acepta; `NPPM_GETLANGUAGENAME`/`DESC`, el nombre |
| **SDK** | `edita_npp_plugin(… EXPORTS ../exports.def)`: el `.so` exporta lo que el `.def` de la DLL (con un *version script*), porque los plugins de lexer exportan las funciones de Lexilla así y no con `__declspec`. `compat/msvc_crt.h` (`_stricmp`, `_wcsicmp`…), incluido antes de cada fuente: con MSVC existen sin incluir nada. `GetStdHandle` y `GetModuleHandleEx` (con `FROM_ADDRESS`, por `dladdr`) |
| **Recetas** | En `check`: `languages` (los que el plugin tiene que añadir) y casos con `language` y `expect-styles` (`[posición, estilo]` tras colorear); con `extension`, el texto se abre como archivo y el lenguaje tiene que detectarse |

## Los plugins

| Plugin | Licencia | Qué hizo falta | Prueba |
|---|---|---|---|
| HLASM Lexer 1.1.1 | Apache-2.0 | `EXPORTS`, `_stricmp` antes de `windows.h`; el overlay genera el XML incrustado, como su `build.sh` | el lenguaje, cinco estilos (etiqueta, operación, comentario, registro) y la detección de `.mlc` |
| OCaml Lexer 1.0.0 | Apache-2.0 | `EXPORTS`, `GetStdHandle`, `GetModuleHandleEx`; un parche de una línea: su registro de depuración escribe en `C:\dev\ocaml_lexer.log`, que en Linux sería un archivo con ese nombre en la carpeta actual | el lenguaje, seis estilos y la detección de `.ml` |

HiddenLexers (pryrt), el tercer plugin de lenguaje de la auditoría, no es un plugin de lexer: es un plugin normal que activa lexers de Lexilla con `NPPM_CREATELEXER`. Va con los demás plugins.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Un plugin que exporta lexers añade sus lenguajes: menú, extensiones, estilos y palabras clave del XML, sin repetidos | ✅ `tst_externallexers` (con un plugin de prueba, `fixtures/lexer-plugin`) |
| Sin XML, el lexer funciona con los estilos por defecto | ✅ `withoutXml` |
| Los estilos editados se guardan en el XML del plugin y no cambian con el tema | ✅ `stylesAreSavedInThePluginXml` |
| Los mensajes de los plugins ven `L_EXTERNAL` + índice y el nombre | ✅ `languageMenuAndPluginMessages` |
| `GetStdHandle` y `GetModuleHandleEx` | ✅ `tst_npp_compat`: `stdHandlesAndModuleFromAddress` |
| HLASM Lexer y OCaml Lexer compilan desde su código, colorean y se detectan por extensión | ✅ `edita --check-plugin` (también compilados con el GCC 12 de Ubuntu 22.04) |
| Pasan todas las suites, el build no da avisos y clang no tiene nada que decir | ✅ |

## Siguiente

La capa de compatibilidad, parte 5 (hooks, barras de herramientas, listas de imágenes, controles con dibujo propio, información de versión), o el siguiente lote de plugins normales, con HiddenLexers. Y lo que reporten los usuarios de la beta.
