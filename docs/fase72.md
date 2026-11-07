# Fase 72 — Dar formato con herramientas externas

Fecha: 2026-11-07

## Por qué

Es la primera función propia de Edita después del bloque de paridad (no existe en Notepad++, donde hace falta un plugin por lenguaje). «Dar formato al documento» ya existía, pero solo con el servidor de lenguaje: sin servidor, o con uno que no da formato, no hacía nada. Las herramientas de formato de cada lenguaje (clang-format, prettier, black…) son lo que la gente usa a diario y leen la configuración del proyecto.

## Qué hace Edita

| Parte | Cómo |
|---|---|
| **Las herramientas** (`core/Formatters`) | Una tabla como la de los servidores de lenguaje (`knownFormatters`): para cada grupo de lenguajes (por el id de `lsp::languageIdFor`), las órdenes por orden de preferencia, y se usa la primera instalada. clang-format (C, C++, Objective-C, Java, C#, CUDA), prettier (JavaScript, TypeScript, CSS, SCSS, Less, HTML, Markdown, YAML), prettier o jq (JSON), ruff o black (Python), rustfmt, gofmt, shfmt, xmllint (con `XMLLINT_INDENT`), stylua, taplo, zig fmt, dart format, ktfmt o ktlint. En las órdenes, `$(FILE_PATH)`, `$(FILE_NAME)`, `$(TAB_SIZE)` y `$(INDENT)` |
| **Cómo se ejecutan** (`runFormatter`) | El documento por la entrada estándar y el resultado por la salida, en la carpeta del archivo (para que encuentren `.clang-format`, `pyproject.toml`, `.editorconfig`…), con 10 s como máximo. Si la herramienta falla, su mensaje; si no devuelve nada para un documento que no está vacío, es un error (no se borra el documento) |
| **Aplicar el resultado** (`replaceChangedLines`) | Solo se sustituyen los bloques de líneas distintos (`core/LineDiff`, con los fines de línea, para notar también el último salto), de abajo arriba y en un paso de deshacer: los marcadores, los plegados y el historial de cambios de las demás líneas se quedan. El cursor vuelve a su línea y columna, y la vista no se mueve |
| **Editar › Dar formato al documento** (Alt+Mayús+F) | Primero la herramienta; si el lenguaje no tiene ninguna instalada, el servidor de lenguaje (como antes); si tampoco, dice cuál instalar. Con una selección, clang-format formatea solo esas líneas (`--lines`); las demás herramientas, todo el documento |
| **Dar formato al guardar** | Una casilla en Preferencias › Formato; con la herramienta (no con el servidor, que responde después) y para los documentos que se ven en una de las vistas. Si falla, se guarda igual y lo dice la barra de estado. «Guardar una copia como» no da formato |
| **Preferencias › Formato** | Como la de los servidores de lenguaje: cada grupo, su orden (vacía, las conocidas; `off`, ninguna) y si está instalada. `<GUIConfig name="EditaFormatOnSave">` y `<GUIConfig name="EditaFormatters">` (`id=orden` por línea) |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| La herramienta de cada lenguaje, otra orden, `off`, la que no está; clang-format sabe de líneas | ✅ `tst_formatting`: `resolve` |
| Entrada y salida, las variables y la carpeta, `$(INDENT)` con tabuladores, `--lines`, el mensaje de error, sin salida, el tiempo máximo, un programa que no existe | ✅ `run` |
| Solo las líneas distintas (los marcadores de las demás se quedan), un paso de deshacer, el último salto, `\r\n` | ✅ `replaceOnlyChangedLines` |
| Dar formato: la herramienta, el cursor se queda, se deshace; si falla, el aviso; sin herramienta, lo dice | ✅ `formatDocument` |
| Al guardar: el archivo queda formateado; si la herramienta falla, se guarda igual | ✅ `formatOnSave` |
| La página Formato y sus valores en `config.xml` | ✅ `tst_preferences` |
| El manual lo explica en los dos idiomas | ✅ `tst_manual` |
| Pasan las 62 suites, el build no da avisos (GCC 16 y clang), y compila con el GCC 12 y Qt 6.8.3 de la CI | ✅ |
