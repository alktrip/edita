# Fase 8 — Autocompletado y macros: resultado

Fecha: 2026-10-20 · Entorno: el mismo que en las fases anteriores

## Qué se construyó

### Autocompletado

| Módulo | Equivale en N++ a | Qué hace |
|---|---|---|
| `core/ApiFile` | `AutoCompletion::setLanguage`, `getApiFileName` | Lee los archivos de API de N++ (`<AutoComplete><Environment …/><KeyWord name func><Overload retVal descr><Param name/>`). Busca primero en `~/.config/edita/autoCompletion/` y luego en `share/edita/autoCompletion/`. Los 34 archivos de `installer/APIs` se copian con `tools/import-third-party.sh` |
| `ui/AutoCompletion` | `AutoCompletion`, `FunctionCallTip`, `InsertedMatchedChars` | Uno por vista, cargado con el lenguaje del documento visible. Ofrece lo siguiente |
| | `showAutoComplete`, `getWordArray` | Lista con las palabras del documento, las de la API (con su icono de función o de palabra clave) o ambas. Se abre al escribir a partir de N caracteres, ignora los números y tiene un modo breve. Respeta `ignoreCase` del archivo de API |
| | `FunctionCallTip` | Ayuda de parámetros al escribir `(`. Resalta el parámetro actual al escribir `,`, se cierra con `)` y ofrece sobrecargas con flechas (clic en la ayuda). Ignora las comas dentro de cadenas y entiende llamadas anidadas. `startFunc`, `stopFunc`, `paramSeparator`, `terminal` y `additionalWordChar` vienen del archivo |
| | `showPathCompletion` | Completa rutas que empiezan por `/`, `~/` o `./` (en N++, `C:\`) |
| | `insertMatchedChars`, `getCloseTag` | Cierre automático de `()`, `[]`, `{}`, `''` y `""`, que se "saltan" al escribir el cierre. Las etiquetas HTML/XML se cierran, salvo las vacías de HTML, los comentarios y `<?…>` |
| Menú **Editar > Autocompletado** | `IDM_EDIT_AUTOCOMPLETE…` | Completar función (Ctrl+Espacio), completar palabra (Ctrl+Intro, que la escribe directamente si solo hay una), ayuda de parámetros (Ctrl+Mayús+Espacio) y completar ruta (Ctrl+Alt+Espacio) |
| Preferencias | `<GUIConfig name="auto-completion" autoCAction triggerFromNbChar autoCIgnoreNumbers autoCBrief funcParams/>`, `<GUIConfig name="auto-insert" parentheses brackets curlyBrackets quotes doubleQuotes htmlXmlTag/>` | Página nueva "Autocompletado" en el diálogo |

### Macros

| Módulo | Equivale en N++ a | Qué hace |
|---|---|---|
| `core/Macro` | `recordedMacroStep`, `NppParameters::feedMacros`/`getActions`/`insertMacro` | Macros en `~/.config/edita/shortcuts.xml` con el **formato de N++**: `<Macro name Ctrl Alt Shift Key>` con `<Action type message wParam lParam sParam/>` y la tecla como código virtual de Windows. Al guardar se conserva el resto del archivo. Se aplica la misma normalización que N++ a las macros antiguas (un `SCI_REPLACESEL` con un salto de línea pasa a `SCI_NEWLINE`). El primer arranque crea el archivo con "Trim Trailing Space and Save" (Alt+Mayús+S), como N++ |
| `ui/MacroSteps` | `recordedMacroStep(…)`, `isMacroable`, `PlayBack` | Qué mensajes de Scintilla se graban con texto y cuáles se pueden reproducir (la misma lista que N++) |
| Grabar | `IDM_MACRO_STARTRECORDINGMACRO` + `SCN_MACRORECORD` | Ctrl+Mayús+R en las dos vistas. Se graban las pulsaciones (Scintilla) y las órdenes de menú, estas con su `IDM_*` de N++ (cortar, copiar, pegar, deshacer, guardar, duplicar línea, mayúsculas, buscar siguiente…), así que las macros son intercambiables con N++. Lo que una orden envía a Scintilla no se graba dos veces |
| Reproducir | `Notepad_plus::macroPlayback` | Ctrl+Mayús+P. Todos los pasos de cada documento tocado se deshacen de una vez. Tras escribir texto se lanza `charAdded` como en N++, así que la sangría y el cierre de pares se repiten igual |
| Varias veces | `RunMacroDlg`, `WM_MACRODLGRUNMACRO` | "Ejecutar una macro varias veces…": la grabada o una guardada, N veces o hasta el final del archivo, con el mismo criterio de parada que N++ |
| Guardar y borrar | `addCurrentMacro`, `MacroShortcut::doDialog` | Nombre y atajo. Las macros guardadas aparecen en el menú con su atajo, y se pueden eliminar desde "Eliminar una macro" |

### Edición (para que las macros sirvan de algo)

En **Editar > Líneas**:
- Duplicar la línea (Ctrl+D).
- Unir las líneas (Ctrl+J).
- Subir o bajar la línea (Ctrl+Mayús+↑/↓).
- Quitar los espacios del final.

En **Editar > Convertir**:
- MAYÚSCULAS (Ctrl+Mayús+U).
- minúsculas (Ctrl+U).

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Intro al grabar | `SCI_NEWLINE` (Win32) | ScintillaQt lo envía como texto `"\r"`; se graba como `SCI_NEWLINE` | Si no, al reproducir insertaría un CR suelto en documentos LF |
| Texto que añaden la sangría automática o el cierre de pares | Se graba además de la pulsación, y al reproducir puede duplicarse | No se graba; al reproducir se vuelve a generar | Evita `"())"` |
| Lista de autocompletado mientras se reproduce una macro | Puede abrirse | No se abre | Un Intro de la macro no debe aceptar una sugerencia |
| Pasos del diálogo Buscar (`mtSavedSnR`) | Se graban | Se leen y se conservan en `shortcuts.xml`, pero no se graban ni se reproducen | Pendiente. Buscar siguiente/anterior (F3) sí se graba como orden de menú |
| Validación HMAC de `shortcuts.xml` | Pide revisar el archivo si cambió a mano | No | Las macros solo envían órdenes del editor, no ejecutan programas |
| Carpetas de macros (`FolderName`) y editor de atajos | Sí | No | Gestor de atajos fuera de alcance |
| Opciones "insertar con Intro/Tab" | Configurables | Siempre las dos (lo normal en Scintilla) | — |
| Pares definidos por el usuario (`UserDefinePair`) | Sí | No | Poco usados |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Los archivos de API de N++ se cargan (entorno, funciones, sobrecargas y parámetros) y uno del usuario tiene prioridad | ✅ `loadsNotepadPlusPlusApiFiles` |
| Lista de palabras del documento; Ctrl+Intro completa directamente si solo hay una; los números se ignoran | ✅ `wordCompletionFromTheDocument` |
| La lista aparece al escribir según "a partir de N caracteres", o no aparece si está desactivada | ✅ `listAppearsWhileTyping` |
| En Python: completar `isinstance` de la API, ayuda de parámetros con `(` y `,`, que se cierra con `)` y se vuelve a pedir con Ctrl+Mayús+Espacio | ✅ `apiCompletionAndCallTips` |
| Completar rutas | ✅ `pathCompletion` |
| Cierre y salto de paréntesis y comillas; no se cierra delante de una palabra; etiquetas HTML | ✅ `matchedPairsAreInsertedAndSkipped`, `htmlTagsAreClosed` |
| `shortcuts.xml` de N++: lectura, normalización de macros antiguas, escritura que conserva lo demás, teclas virtuales | ✅ `macrosFileRoundTripAndNotepadPlusPlusFormat` |
| Grabar pulsaciones, Intro y una orden de menú (con su IDM de N++, sin duplicar lo que envía a Scintilla); reproducir; deshacer de una vez | ✅ `recordAndPlayBack`, `autoInsertedTextIsNotRecordedTwice` |
| Ejecutar N veces y hasta el final del archivo, deshaciendo todo junto | ✅ `runTimesAndUntilEndOfFile` |
| Macro de N++ incluida y funcionando, guardar con nombre y atajo, persistir entre ventanas y eliminar | ✅ `savedMacrosLiveInShortcutsXml` |
| Órdenes de edición nuevas | ✅ `editCommands` |

Tests: `tst_editing` 15 (contando `initTestCase` y `cleanupTestCase`). `tst_mainwindow` y `tst_preferences` se han adaptado al menú Macro y a la página nueva. Pasan las 15 suites y el build no da ningún warning.
