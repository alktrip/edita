# Fase 17 — Interfaz moderna: resultado

Fecha: 2026-10-21 · Entorno: el mismo que en la fase 16, más Qt Svg (`qt6-svg`)

Objetivo: que Edita no parezca un programa antiguo, sin cambiar su organización. Siguen los mismos menús, atajos, pestañas, barra de estado y diálogos que N++. Se eligió:

- un acabado **moderno y limpio**;
- el modo claro u oscuro **siguiendo al sistema**.

## Qué se construyó

| Parte | Qué hace |
|---|---|
| `ui/Appearance` | Estilo **Fusion** en todos los escritorios (y en el AppImage), con paletas propias clara y oscura y el **verde del logo** como acento. Una hoja de estilos da el acabado: menús y barra de menús con esquinas redondeadas, barra de herramientas con botones planos, pestañas planas con el acento bajo la activa, barra de estado limpia, botones redondeados, barras de desplazamiento finas y la lista de Preferencias con selección redondeada. Los campos de texto y las listas desplegables usan el dibujo de Fusion con la paleta: cambiarles el borde con la hoja de estilos haría que Qt deje de dibujar sus flechas |
| Modo claro u oscuro | Es el «Modo oscuro» de N++ (`<GUIConfig name="DarkMode" enable=… enableWindowsMode=… darkThemeName=… lightThemeName=…/>`). Con «seguir al sistema» (`enableWindowsMode`, que en N++ es «seguir a Windows» y en Edita viene activado), Edita sigue al escritorio y cambia en el momento en que el escritorio cambia (`QStyleHints::colorSchemeChanged`). **Cada modo tiene su tema de colores**: en oscuro, `DarkModeDefault.xml` de N++; en claro, el de serie. El tema que se elige en Ver > Tema queda como el del modo actual. Un `config.xml` anterior conserva su tema como el del modo claro |
| Lenguajes del usuario | Como `getUserDefinedLangNameFromExt` de N++: si hay dos para la misma extensión, uno claro y otro oscuro (`darkModeTheme="yes"`, como los dos Markdown que trae N++), se usa el del modo actual. Al cambiar de modo, los documentos abiertos pasan al otro, salvo los que tienen el lenguaje elegido a mano |
| `ui/Icons` y `resources/icons/ui/` | 40 **iconos de línea propios** en SVG (dibujados para Edita), que se pintan con el color del texto de la paleta y se vuelven a pintar al cambiar de modo. Van en la barra de herramientas, en los menús y en la lista de Preferencias |
| Barra de herramientas | Los botones de N++ (`toolBarIcons` de `Notepad_plus.cpp`), en su orden y grupos: Nuevo, Abrir, Guardar, Guardar todo, Cerrar, Cerrar todo, Imprimir · Cortar, Copiar, Pegar · Deshacer, Rehacer · Buscar, Reemplazar · Ampliar, Reducir · Ajuste de línea, Todos los caracteres, Guías de sangría · Lenguaje de usuario · Grabar, Reproducir, Ejecutar varias veces y Guardar la macro. Quedan fuera los de los paneles que Edita no tiene. Usan las mismas acciones que los menús, así que los botones de activar y desactivar muestran su estado. `<GUIConfig name="ToolBar" visible="yes">standard</GUIConfig>`: se puede ocultar o elegir iconos pequeños (16 px), normales (20) o grandes (24) |
| Pestañas | En lugar de los disquetes azul, rojo y gris de N++: nada si el documento está guardado (un hueco del mismo tamaño, para que el texto no salte), un **punto** ámbar si tiene cambios sin guardar y un **candado** si es de solo lectura. La «X» de cerrar es un icono de línea con fondo al pasar el ratón |
| Letra del editor | Los temas de N++ piden fuentes de Windows: Courier New (`stylers.xml`) o Consolas (`DarkModeDefault.xml`). Edita usa en su lugar la **monoespaciada del sistema** (en Arch con KDE, Noto Sans Mono), aunque esas fuentes estén instaladas. Se puede desactivar: `<GUIConfig name="EditaSystemMonospaceFont">` |
| Preferencias > General | **Apariencia** (seguir al sistema, claro u oscuro) y **Barra de herramientas** (oculta o con iconos pequeños, normales o grandes). En Edición: **Usar la letra monoespaciada del sistema…**. Todo traducido al español |

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Aspecto de los controles | Los de Windows, o su modo oscuro propio | Fusion con paleta y hoja de estilos propias | El mismo aspecto moderno en KDE, GNOME y el AppImage |
| Modo oscuro por defecto | Desactivado | Sigue al sistema | Lo eligió el usuario |
| Colores del modo oscuro | Tonos configurables (`colorTone`, `customColor*`) | Una paleta oscura fija | Se dejan para más adelante |
| Iconos | Varios juegos (Fluent, estándar, pequeños/grandes) con colores | Un juego de línea, en el color del texto, en tres tamaños | Uno solo, propio y sin problemas de licencia |
| Indicador de las pestañas | Disquetes de colores | Punto, candado o nada | El aspecto anticuado del que se quejó el usuario |
| Fuente del editor | La que pida el tema | La monoespaciada del sistema en lugar de las de Windows (opcional) | Courier New es la causa principal del aspecto antiguo |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| `DarkMode`, `ToolBar` y la opción de la fuente se leen y guardan con los nombres de N++. Los valores por defecto: seguir al sistema, `DarkModeDefault.xml`, barra visible con iconos normales y la letra del sistema. Un `config.xml` anterior conserva su tema como el del modo claro | ✅ `tst_appearance`: `preferencesUseTheNamesOfNotepadPlusPlus` |
| El modo sale del sistema o de lo elegido (todas las combinaciones, también si el sistema no lo dice). Las paletas son legibles (fondo oscuro con texto claro y al revés) y la hoja de estilos no deja colores sin sustituir | ✅ `darkModeFollowsTheSystemOrTheChoice` |
| En oscuro se aplican la paleta, la hoja de estilos y `DarkModeDefault.xml`. Un tema elegido en el menú queda para ese modo; al pasar a claro vuelve el de serie y al volver a oscuro, el elegido. Se recuerda al reiniciar | ✅ `eachModeHasItsTheme` |
| La barra de herramientas tiene los botones y grupos de N++, en su orden, todos con icono; iconos grandes y ocultarla desde Preferencias; sin el menú de QMainWindow que la oculta por su cuenta | ✅ `toolbarHasTheButtonsOfNotepadPlusPlus` |
| Los iconos se pintan oscuros en el modo claro y claros en el oscuro, y cambian al cambiar de modo | ✅ `iconsFollowThePalette` |
| Pestañas sin disquetes: guardado sin indicador; con cambios, un punto del color de «sin guardar» | ✅ `tabsShowTheStateWithoutFloppies` |
| Markdown claro u oscuro según el modo, y se cambia al cambiar de modo, salvo si se eligió a mano | ✅ `userLanguagesHaveALightAndADarkVersion` |
| El editor usa la monoespaciada del sistema en lugar de Courier New, y Courier New si se desactiva | ✅ `editorUsesTheSystemMonospaceFont` (y `tst_syntax`) |
| Preferencias se abre con todo visible: los nombres de las páginas enteros con su icono (la lista se mide con la hoja de estilos ya aplicada) y cabe la página más grande, sin agrandar el diálogo; en español e inglés, en claro y en oscuro | ✅ `preferencesShowEverythingWithoutResizing` (falla con el tamaño fijo de antes: lista de 200 px y diálogo de 720×480) |
| Seguir al sistema en vivo | ⏭ `followsTheSystemLive` se salta sin pantalla: la plataforma `offscreen` no cambia el esquema de colores. La decisión está probada arriba y la señal se conectó a `applyAppearance` |

Además, se revisaron capturas de la ventana, el menú Archivo, Buscar y Preferencias, en claro y en oscuro.

Tests: `tst_appearance` tiene 12 casos (uno con tres variantes) (contando `initTestCase` y `cleanupTestCase`; uno se salta sin pantalla). Pasan las 25 suites y el build no da ningún warning.

## Corrección: carpetas con acentos en el diálogo de archivos de Qt

Problema: en el diálogo de archivos de Qt (el que se usa sin el portal del escritorio o con la preferencia de usar el diálogo de Qt), el acceso directo a «Música» aparecía como «MÃ�Âºsica», en gris, y no abría nada. El diálogo guarda los accesos directos, el historial y la última carpeta en `~/.config/QtProject.conf`, que comparten todas las aplicaciones de Qt. Las de Qt 5 leen ese archivo como Latin-1 y guardan la «ú» con escapes (`\xc3\xba`), que Qt 6 lee como dos caracteres («Ãº»). Cada vez que una aplicación de Qt 5 y una de Qt 6 lo guardan por turnos, la ruta se estropea un poco más. Se comprobó que Qt 6.8 y 6.12 guardan bien y que Qt 5.15 añade una capa.

Solución: `paths::useOwnQtSettings()`, al arrancar, lleva los `QSettings` sin ruta propia (los del diálogo, «QtProject») a la carpeta de Edita: `~/.config/edita/QtProject.conf`. Solo Edita escribe ahí, así que las rutas con acentos no se estropean. Edita no usa `QSettings` para nada más, y los plugins usan rutas propias. La primera vez, el diálogo empieza con los accesos directos de serie. Prueba: `tst_paths` `qtSettingsAreNotShared`.
