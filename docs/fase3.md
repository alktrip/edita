# Fase 3 — Resaltado de sintaxis: resultado

Fecha: 2026-10-19 · Entorno: el mismo que en las fases anteriores

## Qué se construyó

| Módulo | Equivale en N++ a | Qué hace |
|---|---|---|
| `core/LangType` | `enum LangType` + `ScintillaEditView::_langNameInfoArray` + menú Lenguaje (`Notepad_plus.rc`) | 98 entradas en el mismo orden y con los mismos valores que N++ (se guardan en `session.xml`). La tabla **se generó con un script a partir del código de N++** para no transcribir a mano |
| `syntax/LanguageRegistry` | `NppParameters::feedKeyWordsParameters`, `getKwClassFromName`, `getWordList`, `getLangFromExt` | Lee `langs.xml`: 96 lenguajes con extensiones, palabras clave (`instre1`…`substyle8`), comentarios y `tabSettings` |
| `syntax/StyleSheet` | `feedStylerArray`, `LexerStylerArray`, `StyleArray::addStyler`, `GlobalOverride` | Lee `stylers.xml` y los temas (mismo esquema): 93 `LexerType` + `GlobalStyles`. Colores "RRGGBB" → formato de Scintilla, con las mismas reglas que N++ para valores vacíos o inválidos |
| `syntax/LanguageDetection` | `Buffer::setFileName`, `FileManager::detectLanguageFromTextBeginning` | Extensión (primero las añadidas por el usuario en `stylers.xml`, luego `langs.xml`), nombres especiales (Makefile, CMakeLists.txt, PKGBUILD, Rakefile…), shebang y primeras líneas (`<?xml`, `<?php`, `<!DOCTYPE html`…) |
| `syntax/SyntaxStyler` | `defineDocType`, los `setXxxLexer`, `setStyle` / `setSpecialStyle` (incluido *Global override*), `makeStyle`, `concatToBuildKeywordList`, `populateSubStyleKeywords`, `setTabSettings`, `performGlobalStyles` | Aplica lexer, palabras clave, subestilos, propiedades de plegado y estilos. Unos 75 lenguajes van en una tabla declarativa y los especiales (C/C++ y familia, JavaScript, TypeScript, Objective-C, TCL, HTML/PHP/ASP/JSP con su JS/PHP/VBScript embebido, XML, JSON/JSON5, Bash, arte ASCII) con su función propia |
| `syntax/SyntaxManager` | La parte de `NppParameters::load` que prepara `langs.xml`/`stylers.xml` | Primer arranque: copia `langs.model.xml` y `stylers.model.xml` a `~/.config/edita/`. Si el archivo del usuario está dañado usa el modelo y avisa. Gestiona temas y recargas |
| `core/ConfigXml` | `config.xml` (`GUIConfig name="stylerTheme"`) | Recuerda el tema; conserva lo demás que haya en el archivo |
| `third_party/pugixml`, `data/` | `PowerEditor/src/pugixml`, `langs.model.xml`, `stylers.model.xml`, `installer/themes/*.xml` | Importados con `tools/import-third-party.sh` |

## En la interfaz

- **Lenguaje:** "Ninguno (texto normal)" y los 95 lenguajes del menú de N++, agrupados por inicial como en su menú compacto. Elegir uno a mano lo fija: ya no se redetecta al cambiar el nombre del archivo (`_hasLangBeenSetFromMenu`).
- **Ver > Tema:** el predeterminado (`stylers.xml`) y los 22 temas de N++, más los que el usuario ponga en `~/.config/edita/themes/` (sustituyen a los del mismo nombre). Incluye *Recargar langs.xml y estilos*, *Editar el archivo de estilos* y *Editar langs.xml*. **Guardar uno de esos archivos desde Edita los recarga al momento.**
- **Barra de estado:** la descripción del lenguaje ("C++ source file", "Python file"…), como N++.
- **Diálogos Abrir/Guardar:** filtros por lenguaje generados a partir de `langs.xml` (`setFileOpenSaveDlgFilters`). Al guardar se preselecciona el del documento.
- **Comportamientos de N++ portados:**
  - Margen de plegado con los marcadores "box" y la rama activa resaltada, oculto en los lenguajes que no pliegan (`isNeededFolderMargin`). Se pliega con clic.
  - Línea actual resaltada.
  - Guías de sangría ("look forward" en lenguajes tipo Python, `isPythonStyleIndentation`).
  - Resaltado de la llave pareja, o en rojo si no tiene pareja (`braceMatch`).
  - Tabulación por lenguaje según `tabSettings`.
  - Colores de selección, cursor, margen de marcadores y margen de plegado según los estilos globales.
- **Archivos grandes** (≥ 200 MB): sin lenguaje ni estilos, como N++.

## Divergencias y pendientes

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Fuente que no está instalada ("Courier New" en `stylers.model.xml`) | Cae a "Courier New" | Cae a la **monoespaciada del sistema** (`QFontDatabase::FixedFont`) | En Linux "Courier New" no suele estar |
| Lenguajes definidos por el usuario (UDL) y lexers externos (plugins) | Sí | Se muestran como texto normal | Fuera del alcance del MVP |
| Configurador de estilos (diálogo para editar colores) | Sí | Se edita el XML y se guarda (se recarga solo) | Fuera del alcance del MVP |
| Interruptores de *Global override* | En `config.xml` | Implementado y probado, pero sin interfaz (apagado por defecto) | Llegará con las preferencias |
| Menú Lenguaje | Lista plana por defecto; compacta por inicial como opción | Compacta por inicial | 95 entradas en una lista plana no caben en pantalla |
| Indicadores de búsqueda (marcar, resaltado inteligente) | En `defineDocType` | Fase 4, con la búsqueda | |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Los 96 `<Language>` de `langs.model.xml` y los 93 `<LexerType>` de `stylers.model.xml` se leen sin errores; cada lenguaje tiene lexer o está en una lista documentada | ✅ `allLanguagesAndLexerTypesParse`, `everyLanguageNameIsKnown`, `everyLanguageHasALexer`. Sin lexer a propósito: texto normal, arte ASCII (solo estilos), UDL y externos. Además, `everyLanguageCanBeAppliedAndColourised` aplica y colorea los 98 sin errores |
| En `int main() { return 0; }` con C++, el estilo de cada palabra es el de su lista en `langs.xml` y el color coincide con `stylers.xml` | ✅ `cppKeywordsGetStylersColours`: `return` → INSTRUCTION WORD (5, azul, negrita), `int` → TYPE WORD (16, violeta), comentario → 2. Y otros 15 casos en 14 lenguajes (`otherLanguagesColourise`) |
| `foo.py` → Python, `Makefile` → Makefile, sin extensión con `#!/bin/bash` → Bash, `<?xml` → XML | ✅ `detection` (23 casos), `userExtensionInStylersWins`, `contentDetectionForFilesWithoutExtension` |
| Cambiar un color en `~/.config/edita/stylers.xml` y recargar cambia el color en el editor | ✅ `managerCopiesModelsOnFirstRunAndReloads` y, desde la interfaz, `savingStylersXmlReloadsStyles` (editar y guardar `stylers.xml` en Edita) |
| Manual: cargar `themes/Deep Black.xml` cambia todo el esquema; "Courier New" cae a una monoespaciada del sistema | ✅ `themesAreListedAppliedAndRemembered`, `themeChangeRestylesAndIsRemembered` (también se recuerda entre sesiones), `missingFontFallsBackToSystemMonospace`. Comprobado en Wayland con captura |

Además, con tests:
- Palabras clave añadidas por el usuario en `stylers.xml`.
- *Global override*.
- Tabulación por lenguaje.
- Archivo del usuario dañado, con respaldo al modelo.
- Cada pestaña conserva su lexer.
- Lenguaje elegido a mano que se mantiene tras "Guardar como".
- "Guardar como" `.sh` que pasa a Bash, con filtros por lenguaje.
- Margen de plegado según el lenguaje.
- Estilos globales (estilo por defecto, números de línea, línea actual).

Tests: `tst_syntax` 53 y `tst_mainwindow` 45 (con configuración aislada en un directorio temporal), y siguen en verde los de las fases anteriores. El build desde cero no da ningún warning.
