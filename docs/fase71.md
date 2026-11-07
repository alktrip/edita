# Fase 71 — Importar, el menú contextual y las macros guardadas

Fecha: 2026-11-07

## Por qué

Es el quinto bloque de [lo que tiene Notepad++ y Edita no](paridad-npp.md): las órdenes de Configuración y Macro que faltaban. Con `contextMenu.xml`, además, el clic derecho en el editor deja de ser el menú mínimo de Scintilla (deshacer, cortar, copiar, pegar…) y pasa a ser el de Notepad++, personalizable.

## Cómo lo hace N++ y qué hace Edita

| Orden de N++ | En Edita |
|---|---|
| **Settings › Import › Import plugin(s)…** (`IDM_SETTING_IMPORTPLUGIN`, `addNppPlugins`) | **Configuración › Importar › Importar plugins…**: un `.so` (se comprueba que sea ELF) va a `plugins/<nombre>/<nombre>.so`; un paquete `.zip` (el del catálogo o el de una receta) se instala con `plugin_installer::installPackage`, con el nombre de su `.so`. Lo que ya estaba se sustituye. Avisa de que hay que reiniciar, como N++. Van a la carpeta local (`paths::baseConfigDir`), también con la configuración en la nube |
| **Settings › Import › Import style theme(s)…** (`IDM_SETTING_IMPORTSTYLETHEMES`) | **Importar temas…**: los `.xml` con `<NotepadPlus><LexerStyles>` van a `themes/` de la configuración; si ya hay uno con ese nombre, pregunta. Aparecen al momento en el menú de temas y en el Configurador de estilos |
| **Settings › Edit Popup ContextMenu** (`IDM_SETTING_EDITCONTEXTMENU`) | **Configuración › Editar el menú contextual**: avisa y abre `contextMenu.xml` (si no está, se crea con el de serie, `data/contextMenu.xml`, que es el de N++). A diferencia de N++, no hace falta reiniciar: el menú se arma cada vez que se abre |
| El menú contextual del editor (`contextMenu.xml`, `getContextMenuFromXmlTree`) | `core/ContextMenuXml` lee `<ScintillaContextMenu>`; `MainWindow::buildEditorContextMenu` resuelve cada `<Item>`: `id` (los `IDM_*` de N++, por las tablas de Edita), `MenuEntryName`/`MenuItemName` (los nombres en inglés de N++: la tabla `core/NppCommandNames.inc`, generada de su `english.xml` con `tools/make-npp-command-names.py`, sin distinguir mayúsculas y primero en el rango de ids del menú; si no, el texto del menú de Edita en el idioma de ahora), `PluginEntryName`/`PluginCommandItemName`, `FolderName` (submenú; los `TranslateID` de serie, traducidos), `ItemNameAs` (otro texto) e `id="0"` (separador, sin dobles ni al principio). Lo que Edita no tiene se salta; si no queda nada, el menú de siempre (deshacer, rehacer, cortar, copiar, pegar, borrar, seleccionar todo). `EditorView` lo muestra en el clic derecho del texto (no en los márgenes), con el cursor movido al clic si no cae en la selección, como Scintilla |
| **Macro › Modify Shortcut/Delete Macro…** (`IDM_SETTING_SHORTCUT_MAPPER_MACRO`) | **Macro › Modificar atajo/Borrar macro…**: la Asignación de atajos en la pestaña Macros (que ya cambiaba el atajo y borraba) |

El menú contextual de las pestañas (`tabContextMenu.xml` de N++) sigue siendo el de Edita.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Los nombres en inglés de N++, sin mayúsculas, con & o «...», en su menú; el texto sin & ni atajo | ✅ `tst_importmenu`: `commandNames` |
| El `contextMenu.xml` de serie: se crea; cortar, copiar, pegar, borrar, seleccionar todo, los submenús de estilos, mayúsculas, abrir archivo, buscar en internet, ocultar líneas; sin los plugins que no están ni separadores dobles | ✅ `defaultContextMenu` |
| Uno propio: id, nombre de N++ con `ItemNameAs` (y hace lo mismo), carpeta, lo que no existe; vacío, el menú de siempre | ✅ `customContextMenu` |
| El clic derecho en el texto abre ese menú | ✅ `rightClickOpensIt` |
| Importar plugins: `.so`, `.zip`, lo que no es un plugin (aviso), sustituir, el aviso de reiniciar | ✅ `importPlugins` |
| Importar temas: a `themes/` y en la lista, lo que no es un tema, ya existe (pregunta) | ✅ `importThemes` |
| Las órdenes de los menús; Editar el menú contextual abre el archivo | ✅ `menuActions` |
| El manual cita cada orden en los dos idiomas | ✅ `tst_manual` |
| Pasan las 61 suites, el build no da avisos (GCC 16 y clang), y compila con el GCC 12 y Qt 6.8.3 de la CI | ✅ |
