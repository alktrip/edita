# Fase 42 — LSP, parte 3: acciones de código, símbolos del proyecto y pistas: resultado

Fecha: 2026-10-30

Completa lo que faltaba del Language Server Protocol (fases 36 y 37). Nada de esto existe en Notepad++.

## Qué se construyó

| Parte | Qué hace |
|---|---|
| **Acciones de código** | Editar › «Acciones de código…» (Ctrl+.): `textDocument/codeAction` para la selección o la línea, con los diagnósticos de esas líneas en el contexto, para que el servidor dé sus arreglos. La lista muestra primero las preferidas (marcadas) y luego las demás, con su tipo (quickfix, refactor…). Al elegir una se aplica su `edit` (`WorkspaceEdit`) y se ejecuta su `command` (`workspace/executeCommand`; si el servidor responde con `workspace/applyEdit`, se aplica). Si la acción no trae ninguno de los dos, se pide `codeAction/resolve`. También acepta Commands sueltos |
| Bombilla | Cuando hay acciones en la línea del cursor (se pregunta 400 ms después de moverse o editar), aparece una bombilla amarilla en el margen de símbolos. Un clic en ella abre la lista de acciones en lugar de poner un marcador |
| **Símbolos del proyecto** | Buscar › «Ir a un símbolo del proyecto…» (Ctrl+Alt+O): `workspace/symbol` mientras se escribe (200 ms de espera), con el nombre y, como detalle, el contenedor, el archivo y la línea. Al elegir uno se abre en su sitio. Usa el servidor del documento o, si ese no sabe, cualquiera que sepa. La lista con buscador ahora puede cambiar sus elementos sin cerrarse (`QuickPicker::setItems`, señal `queryChanged`) |
| **Pistas** (`textDocument/inlayHint`) | Los tipos deducidos y los nombres de los parámetros de las líneas a la vista, **al final de cada línea**, en gris y cursiva: «`total: int  ·  (a, b)`». El tipo lleva delante el nombre de lo que está justo antes, y los parámetros seguidos se juntan entre paréntesis. Los designadores (`[0]=`, `.x=`) no se muestran, porque solo tienen sentido delante de cada valor. Se piden al desplazarse, al editar y al cambiar de documento |
| Por qué al final de la línea | Scintilla no puede dibujar texto «virtual» dentro de una línea (reservar espacio entre dos caracteres sin cambiar el documento); para eso habría que reescribir su motor de dibujo. Sí tiene anotaciones de fin de línea, que son las que se usan |
| **Anotaciones de fin de línea** (`ui/EolAnnotations`) | Las comparten las pistas y Git blame (fase 40): cada uno pone su parte y se juntan en la misma línea, con un estilo propio. Sus números de línea siguen al texto cuando se insertan o borran líneas |
| Cliente | Declara las capacidades `codeAction` (con tipos, preferidas, `data` y resolución de `edit`), `inlayHint`, `workspace.symbol` y `workspace.executeCommand` |
| Preferencia | Servidores de lenguaje › «Pistas (tipos, nombres de parámetros) al final de las líneas» (`EditaInlayHints`), activada de serie |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Con el servidor de mentira: pistas «x: int» y «(a, b)»; bombilla en la línea; Ctrl+. con la preferida primero, que arregla el error; una orden del servidor que responde con `workspace/applyEdit`; una acción que se completa con `codeAction/resolve`, abierta desde la bombilla (y sin poner un marcador); símbolos pedidos al escribir; la misma lista, usada después para otra cosa, ya no pide símbolos; sin la preferencia, sin pistas | ✅ `tst_lsp`: `actionsSymbolsAndHints` |
| Con clangd real: «v: int» y «(n)», y `doble` en los símbolos del proyecto | ✅ `clangd` |

Las pruebas encontraron un error: los símbolos del proyecto se pedían sin enviar antes el texto recién escrito, y el servidor respondía con las líneas de antes. Las capturas con clangd (claro y oscuro: pistas, bombilla y la lista con «change 'totl' to 'total'») mostraron los designadores, que ahora se filtran. Pasan las 44 suites y el build no da ningún warning.
