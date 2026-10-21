# Fase 25 — XML Tools completo: resultado

Fecha: 2026-10-21

Objetivo: que el port de XML Tools (fase 15) tenga el menú entero del original 3.1.1.13 (`nppMenu.cpp`). Antes tenía 16 órdenes y el «Acerca de».

## Qué se construyó

Todo está en `data/plugins/recipes/XMLTools/overlay/edita/XMLToolsEdita.cpp`, el port nativo que compila la receta.

| Orden u opción | Original | En Edita |
|---|---|---|
| **First / Previous / Next / Last error** | `ToolsErrors.cpp`: los errores de cada documento (`xmlErrors[bufferid]`) y el actual | Igual. Al comprobar o validar, los errores se guardan por documento (hasta `maxErrorsNum`, 10 de serie). Cada orden lleva el cursor a su línea y columna y centra la línea en la vista |
| Errores como **anotaciones** | `errorDisplayMode` = `Annotation` (de serie), `Dialog` o `Alert` | Los tres modos. En `Annotation`, cada error se escribe en un recuadro bajo su línea (anotación de Scintilla) y el actual se resalta. Los colores se leen bien en temas claros y oscuros. Al editar el texto, las anotaciones se quitan. `Dialog` abre una ventana que no bloquea; `Alert`, un aviso |
| **Tag auto-close** (de serie, sí) | `closeXMLTag` en `SCN_CHARADDED` | Port directo: al escribir el `>` de `<etiqueta …>` añade `</etiqueta>` tras el cursor. No actúa después de `/>`, `-->`, `?>`, `]>` ni en `</`, `<!`, `<?` |
| **Set XML type automatically** (de serie, sí) | `setAutoXMLType`: si el texto empieza por `<?xml `, el lenguaje pasa a XML | Igual: al activar un documento, al escribir y después de formatear |
| **Prevent XXE** (de serie, sí) | Opciones de MSXML | libxml2 sin red (`XML_PARSE_NONET`) y sin sustituir entidades externas. Desactivado, carga la DTD y sustituye las entidades (`XML_PARSE_DTDLOAD`, `XML_PARSE_NOENT`) |
| **Allow huge files** | Opción de MSXML | `XML_PARSE_HUGE`: sin los límites de tamaño y profundidad de libxml2 |
| **Apply to all open files** | `nppMultiDocumentCommand`: pretty print, linearize y tokenize en todos los documentos abiertos de las dos vistas | Igual (activa cada documento y salta los de solo lectura). Al terminar, vuelve al documento en el que se estaba |
| **Tokenize (debug)** | `debugTokens` de QuickXml, a la ventana de depuración | Igual |
| **Options...** | Cuadrícula de propiedades MFC | Un formulario con las opciones del original: modo de errores, máximo de errores, nivel máximo de sangría, autocierre de etiquetas vacías, conformidad, `xml:space="preserve"`, la ruta en la barra de estado, el nombre de los atributos de identidad, el índice del nodo y los atributos de identidad |
| **Debug window...** | `CDebugDlg` con Clear y Close | Una ventana que no bloquea, con el registro de lo que hace el plugin (órdenes, tiempos, errores, tokens) y los mismos botones |
| Ruta en la barra de estado | `xpathOnStatusbar` (de serie, sí), en `SCN_UPDATEUI` | Igual, en el campo del tipo de documento. No se calcula en documentos de más de 4 MB |

Todas las opciones se guardan en `XMLTools.ini`, en la sección `[XML Tools]`, con las claves del original (`doCloseTag`, `doAutoXMLType`, `doPreventXXE`, `doAllowHuge`, `doPrettyPrintAllOpenFiles`, `errorDisplayMode`, `maxErrorsNum`, `maxIndentLevel`, `identityAttributes`…).

El menú queda con las 29 órdenes y los 9 separadores del original, en su orden y con sus atajos. «Tag auto-indent» y «Auto-complete attributes» están comentados en el código del original y no aparecen en su menú, así que tampoco en este.

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Opciones | Cuadrícula de propiedades (MFC) | Formulario de Qt con las mismas opciones | No hay MFC en Linux |
| Opciones sin efecto | Motor de formateo (QuickXml, StringXml, SimpleXml), proxy, `allowDocumentFunction` de MSXML, iconos de la barra de herramientas | No están | Solo se compila QuickXml (el motor de serie); el proxy y MSXML no existen con libxml2 |
| Estilos de las anotaciones | Números de estilo (12 y 13) del lexer | Dos estilos propios (`SCI_ALLOCATEEXTENDEDSTYLES`) con colores para tema claro u oscuro | Los números del original dependen de cada lexer |
| Apply to all open files | Deja activo el último documento | Vuelve al documento de partida | No perder el sitio |
| Ruta del nodo actual | — | Con los atributos de identidad sin su nombre (`/a/b[x]/c`), como el original de serie. La fase 15 los mostraba con su nombre | Ahora se respeta `dumpAttributeName` |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| El menú tiene 29 órdenes y 9 separadores, con las nuevas; las opciones marcadas de serie son las del original | ✅ `tst_recipes`: `realNotepadPlusPlusPlugins(XML Tools)` |
| Al escribir `>` en un documento de texto que empieza por `<?xml `, pasa a XML y se cierra la etiqueta; no se cierra después de `/>` ni de un comentario | ✅ |
| La ruta del nodo actual aparece en la barra de estado | ✅ |
| Con errores de sintaxis, el primero tiene su anotación; Last y First error mueven el cursor; editar quita las anotaciones | ✅ |
| Con «Apply to all open files», Pretty print formatea dos documentos y vuelve al de partida; la opción se guarda en `XMLTools.ini` | ✅ |
| Tokenize abre la ventana de depuración con los tokens | ✅ |

Pasan las 30 suites y el build no da ningún warning.
