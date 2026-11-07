# Fase 73 — Corrector ortográfico

Fecha: 2026-11-07

## Por qué

Es la segunda función propia de Edita de este bloque. En Notepad++ la ortografía es un plugin (DSpellCheck). Sirve en los comentarios y las cadenas del código, y en todo el texto de un documento normal o Markdown.

## Qué hace Edita

| Parte | Cómo |
|---|---|
| **Hunspell** (`core/SpellChecker`) | La biblioteca de LibreOffice, Firefox y casi todo Linux, por su API de C. Opcional al compilar (`pkg_check_modules(hunspell)`, `EDITA_HUNSPELL`): sin ella Edita compila igual y la página lo dice. El AppImage la lleva dentro (`libhunspell-dev` en la CI y en el sistema base de `build-portable.sh`) |
| **Diccionarios** | Los del sistema: `$DICPATH`, `~/.local/share/hunspell`, `/usr/share/hunspell`, `/usr/share/myspell(/dicts)` (un `.dic` con su `.aff`; no los `hyph_*`). Varios a la vez: una palabra está bien si algún diccionario la tiene. De serie, el del idioma del sistema (el exacto, otro del mismo idioma o el idioma solo: `es_MX` → `es_MX`, `es_AR` o `es`) y el inglés. Los que no están en UTF-8 se convierten con iconv |
| **Diccionario personal** | `personalDictionary.txt` en la configuración, una palabra por línea; una palabra con mayúscula inicial vale si está en minúscula. «Pasar por alto», hasta cerrar Edita |
| **Qué palabras** (`wordsToCheck`) | Letras (con acentos y marcas), con apóstrofos dentro («don't», «l'eau»). No las que llevan dígitos o `_`, ni las de código (una mayúscula después de una minúscula: `getValue`), ni las siglas en mayúsculas, ni las de una letra, ni las URL, los correos o las rutas |
| **Dónde** (`ui/SpellCheck`) | Las líneas visibles, un momento (200 ms) después de escribir o desplazarse, con el indicador 20 en onda roja (los lexers de los plugins se quedan con el 9-19). En código, solo los estilos de comentario y de cadena: lo que dice el lexer (`SCI_TAGSOFSTYLE`, `SCI_NAMEOFSTYLE`) y los nombres de `stylers.xml` (COMMENT…, STRING…, sin los de palabras clave, regex, operadores o etiquetas). Sin lexer (texto normal), todo. Los archivos grandes, no |
| **Clic derecho** | Sobre una palabra subrayada, al principio del menú: sus sugerencias (en negritas; una la cambia), «Añadir al diccionario» y «Pasar por alto» |
| **Ver › Revisar la ortografía** y **Preferencias › Ortografía** | Activarlo (de serie, sí), solo comentarios y cadenas en el código (de serie, sí) y los diccionarios instalados, para marcar (sin ninguno, los de serie; los que se marcaron y ya no están se conservan). `<GUIConfig name="EditaSpellCheck" enabled dictionaries onlyCommentsAndStrings>` |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Qué palabras se revisan: acentos, apóstrofos, código, siglas, dígitos, URL, correos, rutas | ✅ `tst_spelling`: `words` |
| Los diccionarios de serie según el idioma; los instalados, sin los de guiones | ✅ `defaultDictionaries` |
| Hunspell: varios diccionarios, uno en ISO-8859-1, sugerencias, el personal (se guarda y se lee), pasar por alto | ✅ `checker` |
| En el editor: subrayadas en texto normal; en C, el comentario y la cadena pero no el identificador; todo el código si se quiere; apagado, nada | ✅ `underlined` |
| El clic derecho: la sugerencia cambia la palabra, añadir al diccionario la quita y la guarda, sin palabra el menú de siempre | ✅ `contextMenu` |
| La página Ortografía y sus valores en `config.xml` | ✅ `preferencesPage`, `tst_preferences` |
| El manual lo explica en los dos idiomas | ✅ `tst_manual` |
| Pasan las 63 suites, el build no da avisos (GCC 16 y clang, con y sin Hunspell), y compila y pasa con el GCC 12, Qt 6.8.3 y Hunspell 1.7.0 de Ubuntu 22.04 | ✅ |
