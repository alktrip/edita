# Fase 65 — Kotlin y Delphi

Fecha: 2026-11-06

## Por qué

Un usuario los echó de menos en el menú Lenguaje. Delphi sí estaba, pero con el nombre que le da Notepad++, «Pascal», y con un modelo de lenguajes de hace años. Kotlin no estaba: ni Notepad++ ni Lexilla lo traen.

## Qué cambió

| Dónde | Qué |
|---|---|
| **Kotlin** (`data/userDefineLangs/kotlin._preinstalled*.udl.xml`, generados por `tools/make-kotlin-udl.py`) | Un lenguaje definido por el usuario, como los Markdown preinstalados de N++: así no hace falta un `LangType` nuevo (los números tienen que coincidir con los `L_*` de N++, que usan los plugins), y el archivo sirve también en Notepad++. Palabras reservadas y modificadores, tipos de la biblioteca estándar, anotaciones (`@…`), números (decimales, hexadecimales y binarios con `_`, sufijos `L`, `f`, `u`), cadenas normales y crudas (`"""`), caracteres, nombres entre acentos graves, comentarios de línea y de bloque, plegado por llaves y por `//region`. Variante clara y oscura con los colores de C++ de Edita Light y Edita Dark; se elige según el modo, como Markdown. Se abre con `.kt` y `.kts`; el servidor de lenguaje de Kotlin ya estaba registrado para esas extensiones |
| **Delphi** (`data/langs.model.xml`, Pascal) | Las extensiones `.dpk`, `.dfm` y `.lfm`; el comentario de línea `//` (Comentar usaba solo el de bloque); y las palabras del Object Pascal actual que faltaban (`strict`, `sealed`, `helper`, `reintroduce`, `operator`, `reference`, `dispinterface`…). El lexer ya era el de Delphi. En el menú se llama **Pascal / Delphi**; por dentro sigue siendo «Pascal», como en N++, para que las sesiones sean compatibles |
| **langs.xml del usuario** (`updateLangsFromModel`) | Como `NppParameters::updateLangXml`: si el `modelDate` del modelo es más nuevo, el `langs.xml` del usuario recibe lo que le falte (lenguajes, listas, palabras, atributos y extensiones), sin quitar ni cambiar nada suyo. Sin esto, quien ya tenía Edita no recibía nunca una mejora de los lenguajes. El modelo pasa a `modelDate="20261106"` |
| **LexUser** (`third_party/patches/0007-lexuser-eol-una-vez.patch`) | Error de N++ que salió al probar Kotlin: un comentario de bloque de una línea (`/** … */`) hacía que el plegado de todo lo que seguía quedara una línea más abajo. Al cerrar justo antes del fin de línea, el lexer procesaba ese fin de línea dos veces. Ahora una vez |
| **`tools/import-third-party.sh`** | Borraba `data/` entera antes de copiar los datos de N++ (con el manual, los fragmentos y los temas de Edita dentro). Ya no: copia encima y aplica los cambios de Edita de `tools/data-patches/` (el de Delphi) y vuelve a generar Kotlin |

## Lo que no se puede

En un lenguaje definido por el usuario, `LexUser` no admite un número decimal con `_` (`1_000`): los caracteres extra de un número sin prefijo tienen que acabar en un sufijo. Los hexadecimales con `_` (`0xFF_FF`) sí se colorean. Las plantillas de las cadenas (`"${x}"`) se ven como parte de la cadena.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Un `.kt` se abre con Kotlin, colorea cada elemento y pliega por las llaves (también tras un comentario de documentación); en oscuro, su variante oscura; Comentar usa `//` | ✅ `tst_morelanguages`: `kotlin` |
| Delphi: el menú dice «Pascal / Delphi», sus extensiones se detectan, sus palabras nuevas se colorean y Comentar usa `//` | ✅ `delphi` |
| Un `langs.xml` viejo recibe lo nuevo del modelo sin perder nada del usuario, y solo una vez | ✅ `langsXmlIsUpdatedFromTheModel` |
| El parche de LexUser y el de datos se aplican sobre los originales | ✅ (reproducen los archivos) |
| Pasan todas las suites y el build no da avisos | ✅ |
