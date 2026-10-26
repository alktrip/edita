# Fase 37 — Servidores de lenguaje (LSP), parte 2: resultado

Fecha: 2026-10-26

Con esta fase se completa el soporte de LSP empezado en la 36: navegar por el código y cambiarlo con ayuda del servidor. Nada de esto existe en Notepad++.

## Qué se construyó

| Parte | Qué hace |
|---|---|
| **Ir a la definición** (Buscar › F12) | `textDocument/definition`: abre el archivo y deja el cursor en el nombre, con la línea a un tercio de la altura. Si hay varias definiciones, aparece una lista para elegir. Entiende `Location`, `Location[]` y `LocationLink[]` (de este usa `targetSelectionRange`) |
| **Buscar referencias** (Buscar › Mayús+F12) | `textDocument/references`, incluida la declaración. Salen en el panel de resultados de búsqueda como un «Buscar todo»: por archivo y línea, con la palabra resaltada, y F4/Mayús+F4 las recorren. El texto de cada línea sale del documento abierto o, si no está abierto, del archivo |
| **Renombrar símbolo** (Editar) | Pide el nombre nuevo y aplica el `WorkspaceEdit` del servidor en todos los archivos. Los abiertos se cambian sin cambiar de pestaña; los demás se abren y quedan sin guardar. Cada archivo se deshace de una vez |
| **Dar formato al documento** (Editar › Alt+Mayús+F) | `textDocument/formatting` con el ancho de tabulación y tabulaciones o espacios del documento, o `rangeFormatting` si hay selección y el servidor lo admite. Los cambios se aplican de atrás hacia delante, en un solo paso para deshacer, y el cursor queda en la misma línea y columna |
| **Ayuda de parámetros** | Al escribir un carácter de disparo del servidor («(», «,») o con Ctrl+Mayús+Espacio aparece la firma con el parámetro actual resaltado (por posición o por texto), «(1/2)» si hay sobrecargas y el primer párrafo de la documentación. Con «)» se cierra. Sin servidor sigue la ayuda de N++ |
| **WorkspaceEdit** | Admite `documentChanges` (`TextDocumentEdit`) y `changes`. Las operaciones con archivos (crear, renombrar, borrar) no se aplican, y se avisa en la barra de avisos. También atiende `workspace/applyEdit` cuando lo pide el servidor |
| Prompter | `askText` para pedir un texto (el nombre nuevo), con su versión falsa para las pruebas |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| F12 lleva a la definición en otro archivo (que se abre) | ✅ `tst_lsp`: `navigationAndEdits` |
| Mayús+F12: 4 referencias en el panel, de un archivo abierto y otro sin abrir | ✅ |
| Renombrar cambia los dos archivos sin cambiar de pestaña | ✅ |
| Dar formato aplica varios cambios y se deshace de una vez | ✅ |
| Ayuda de parámetros: «(1/2)», documentación, «a» resaltada y después «b» tras la coma; con «)» se cierra | ✅ |
| `WorkspaceEdit` con `changes` | ✅ |
| clangd real: F12 sobre `doble` y formato con clang-format | ✅ `clangd` |

Se revisó una captura con clangd en oscuro: las 4 referencias de `distancia` y la firma con `double y1` resaltado. Pasan las 41 suites y el build no da ningún warning.
