# Fase 75 — Otra ventana, pegado especial y el menú de las pestañas

Fecha: 2026-11-10

## Por qué

Es el séptimo bloque de [lo que tiene Notepad++ y Edita no](paridad-npp.md). Se juntan tres órdenes medianas que casi completan el menú de N++:

- abrir o mover el documento en otra ventana;
- el pegado especial;
- `tabContextMenu.xml`, que no está en la lista, pero N++ lo lee.

## Cómo lo hace N++ y qué hace Edita

| Orden de N++ | En Edita |
|---|---|
| **View › Move/Clone Current Document › Move to New Instance** y **Open in New Instance** (`IDM_VIEW_GOTO_NEW_INSTANCE`, `LOAD_IN_NEW_INSTANCE`; `docOpenInNewInstance`) | **Ver › Mover a otra ventana** y **Abrir en otra ventana**, también en el menú de la pestaña. Como en N++, lanza otra instancia con la ruta. Los argumentos los arma `newInstanceArguments` en `core/CommandLine`: `--multiInst`, `--nosession`, `-l` con el lenguaje (salvo si es un lenguaje del usuario), y `-n` y `-c` con el cursor. La opción `-l/--language` es nueva en Edita y viaja también a la instancia abierta (`InstanceRequest::language`). Dentro de un AppImage se lanza `$APPIMAGE` y no el binario, porque la carpeta montada desaparece al cerrar la ventana que movió el documento. Las dos órdenes solo se pueden usar con un documento guardado y con nombre, porque la otra ventana lo lee del disco. Al mover, el documento se cierra; si solo queda el «nuevo 1» vacío, también se cierra la ventana, como `noOpenedDoc` en N++. N++ también mueve el documento al soltar la pestaña fuera de la ventana; Edita no |
| **Edit › Paste Special › Paste HTML/RTF Content** (`IDM_EDIT_PASTE_AS_HTML`, `RTF`) | **Editar › Pegado especial**. Pega el código tal cual, como N++: `text/html`, o `text/rtf`, `application/rtf` o `text/richtext`. Firefox pone el HTML en UTF-16 con BOM, así que se decodifica. Las dos órdenes solo se pueden usar cuando el portapapeles tiene ese formato |
| **Copy/Cut/Paste Binary Content** (`IDM_EDIT_COPY/CUT/PASTE_BINARY`; `CF_NPPTEXTLEN`) | Copia los bytes de las selecciones con `SCI_GETTEXTRANGEFULL`, porque `SCI_GETSELTEXT` cambia los NUL por espacios. Varias selecciones van una por línea. Los bytes se guardan en el formato propio `application/x-edita-binary` y, para los demás programas, también como texto. Pegar usa `SCI_ADDTEXT` con todos los bytes; si el portapapeles viene de otro programa, pega su texto |
| **`tabContextMenu.xml`** (`NppParameters::getContextMenuFromXmlTree` con `TabContextMenu`) | `readContextMenu(…, ContextMenuKind::Tab)` y `MainWindow::fillMenuFromXml`, la parte del menú del editor de la [fase 71](fase71.md), ahora compartida. Como en N++, solo cuenta si el archivo existe: sin él queda el menú de pestañas de Edita, que ahora trae las dos órdenes nuevas. N++ no tiene orden para editarlo. Edita añade **Configuración › Editar el menú de las pestañas**: la primera vez lo crea con `data/tabContextMenu.xml`, que es el ejemplo de N++ sin lo de Windows, y luego lo abre. Los nombres de los submenús del ejemplo se traducen |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Los argumentos de la otra ventana; `-l` en el parser, en `--help` y en la petición a la instancia abierta | ✅ `tst_transfer`: `newInstanceArguments` |
| La ventana que recibe `-l` le pone el lenguaje al archivo, y pasa por alto un lenguaje que no existe | ✅ `languageFromRequest` |
| Abrir en otra ventana con el lenguaje, la línea y la columna; mover cierra el documento | ✅ `openInNewInstance` |
| No se puede sin guardar o sin título | ✅ `notForUnsavedDocuments` |
| Mover el único documento cierra la ventana | ✅ `movingTheLastDocumentClosesTheWindow` |
| Pegar HTML, en UTF-8 y en UTF-16 con BOM, y RTF, de dos tipos MIME y nada sin RTF | ✅ `pasteHtml`, `pasteRtf` |
| Copiar, cortar y pegar con NUL, y pegar lo de otro programa | ✅ `binaryContent` |
| `<TabContextMenu>`: leerlo y no confundirlo con el del editor | ✅ `readTabContextMenu` |
| Sin archivo, el menú de siempre con las dos órdenes nuevas; con archivo, sus órdenes, submenús, separadores y `ItemNameAs`, y se salta lo que no existe | ✅ `defaultTabMenu`, `customTabMenu` |
| Editar el menú de las pestañas crea el de serie y lo abre | ✅ `editTabMenuCreatesDefault` |
| El manual cita cada orden en los dos idiomas, y las teclas de acceso no chocan | ✅ `tst_manual`, `tst_i18n` |
| Pasan las 65 suites, el build no da avisos (GCC 16 y clang) y compila con el GCC 12 y Qt 6.8.3 de la CI | ✅ |

## También en esta fase

Por decisión del proyecto, macOS espera a que haya una versión estable para Linux. Por eso se borró el `.dmg` del release 0.0.1 y se cambiaron los avisos de sus notas, del README y de `PROBAR.md`.
