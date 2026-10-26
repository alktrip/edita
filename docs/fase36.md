# Fase 36 — Servidores de lenguaje (LSP), parte 1: resultado

Fecha: 2026-10-26

Una mejora que **no existe en Notepad++**: Edita habla el Language Server Protocol 3.17 con los servidores instalados (clangd, pyright, rust-analyzer, gopls…). Esta fase trae el cliente, los diagnósticos, el autocompletado y la información al pasar el ratón. La fase 37 agrega ir a la definición, las referencias, renombrar, dar formato y la ayuda de firmas.

## Qué se construyó

| Parte | Qué hace |
|---|---|
| **Protocolo** (`lsp/JsonRpc`, `lsp/LspClient`, biblioteca `edita_lsp`) | Mensajes JSON-RPC con `Content-Length` (que lleguen a trozos o varios juntos). El cliente lanza el servidor con `QProcess` y hace el saludo `initialize`/`initialized`; lo que se pide antes espera en una cola. Pide posiciones en UTF-8 (`positionEncodings`, y `offsetEncoding` para clangd antiguo) y, si el servidor no acepta, usa UTF-16. Contesta lo que pide el servidor: `workspace/configuration`, `workspaceFolders`, `registerCapability`, `workDoneProgress/create` y `applyEdit`; lo que no conoce, con −32601. Al cerrar manda `shutdown` y `exit`, y si el servidor no termina, lo mata. Guarda un registro (stderr, `logMessage`, errores) |
| **Servidores** (`lsp/LspServers`) | 27 servidores conocidos, cada uno con sus órdenes en orden de preferencia: la primera que esté en el PATH. Entre ellos: clangd (C, C++, Objective-C), basedpyright/pyright/pylsp/jedi/ruff (Python), typescript-language-server (JavaScript y TypeScript), rust-analyzer, gopls, bash-language-server, lua-language-server, JSON/CSS/HTML de VS Code, yaml-language-server, taplo, marksman (Markdown), jdtls, intelephense, texlab, zls… El languageId sale del lenguaje de N++ y, si hace falta, de la extensión (`.tsx`, `.mm`, `.md`). La raíz del proyecto es la carpeta de trabajo que contiene al archivo; si no, la primera carpeta hacia arriba con un marcador (`compile_commands.json`, `Cargo.toml`, `go.mod`, `.git`…). Hay un servidor por orden y raíz |
| **Documentos** (`ui/LspManager`) | Cada documento con archivo se abre en su servidor (`didOpen`) al abrirse o al cambiar de lenguaje o de nombre. El texto completo se envía 300 ms después de editar (`didChange`, según la versión de contenido del Buffer, que cuenta también los cambios de los plugins) y siempre antes de cada petición. Al guardar se manda `didSave` y al cerrar, `didClose`. Los archivos de más de 8 MB no se envían |
| **Diagnósticos** | Subrayado ondulado rojo (error), amarillo (aviso) o azul (información), y puntos grises para las pistas, con colores para tema claro y oscuro (indicadores 1–4, que N++ no usa). Los errores y avisos también se marcan en la barra de desplazamiento |
| **Panel Problemas** (`ui/ProblemsPanel`) | Ver › «Problemas»: los diagnósticos por archivo, con su icono de gravedad, mensaje, origen y código, y la posición. Elegir uno lleva a él. En la barra de estado, junto al lenguaje, aparece «⨯ 4 ⚠ 1» (con el nombre del servidor en la descripción emergente); pulsarlo muestra u oculta el panel |
| **Autocompletado** | Al escribir un identificador (desde las letras que diga la preferencia de N++), tras un carácter de disparo del servidor («.», «->», «::») o con Ctrl+Espacio, aparece la lista del servidor en su orden, con un icono por tipo (función, variable, clase, módulo, palabra clave…). Al elegir un elemento se respetan `textEdit`, los fragmentos (`${1:x}` queda escrito y con la «x» seleccionada) y los cambios adicionales (p. ej. el `#include` que añade clangd), todo en un solo paso para deshacer. Sin servidor, sigue el autocompletado de N++ |
| **Al pasar el ratón** | Medio segundo encima de un símbolo: los diagnósticos de ese punto y lo que diga el servidor (`textDocument/hover`), con el Markdown convertido a texto con formato |
| Menú | Configuración › «Servidor de lenguaje»: «Reiniciar el servidor de lenguaje» y «Mostrar el registro del servidor de lenguaje» (en un documento nuevo) |
| Preferencias | Página «Servidores de lenguaje»: activarlos (`EditaLsp`) y una tabla con cada servidor, su orden y si está instalado. Una orden escrita sustituye a las conocidas, y «off» apaga ese lenguaje (`EditaLspServers`, «id=orden» por línea) |
| Pruebas | Las demás suites corren con `EDITA_LSP=0` para no lanzar los servidores que haya en la máquina |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| JSON-RPC: mensajes a trozos y varios juntos, cabecera sin distinguir mayúsculas | ✅ `tst_lsp`: `jsonRpc` |
| Posiciones UTF-16 y UTF-8 con «ñ» y «😀»; URI con espacios y acentos | ✅ `positions` |
| Fragmentos: marcadores, anidados, elecciones, variables, `\$`, `$0` | ✅ `snippets` |
| languageId, orden de la preferencia, «off», orden inexistente, raíz por carpeta de trabajo o marcador | ✅ `serverSelection` |
| Cliente con un servidor de mentira (`fixtures/fake_lsp.py`): cola antes del saludo, capacidades, diagnósticos, respuesta y error −32601, registro, cierre | ✅ `client` |
| En el editor: subrayados en el byte correcto tras caracteres no ASCII, barra de estado, Problemas lleva al sitio, información con el diagnóstico, al editar desaparece el error, lista al escribir con el fragmento seleccionado, tras «.», Ctrl+Espacio con `textEdit` y un `import` adicional que se deshacen juntos, `didSave`, registro, desactivado | ✅ `inTheEditor` |
| clangd real (si está instalado): «'x'» sin declarar en la línea 3, posiciones UTF-8, información de `doble` | ✅ `clangd` |

Se revisaron capturas con clangd en claro (diagnósticos, Problemas y la lista de `std::vector` con iconos) y en oscuro (la información de `push_back`). Pasan las 41 suites y el build no da ningún warning.
