# Fase 22 — Paneles: resultado

Fecha: 2026-10-21

Objetivo: los cuatro paneles acoplables de N++ que faltaban: **Lista de documentos**, **Mapa del documento**, **Lista de funciones** y **Explorador de carpetas** («Folder as Workspace»).

## Qué se construyó

| Parte | Equivale en N++ a | Qué hace |
|---|---|---|
| `DocumentListPanel` | `VerticalFileSwitcher` | Los documentos abiertos de las dos vistas, con el indicador de estado de las pestañas y su carpeta. Un clic lo activa y el clic central lo cierra |
| `DocumentMapPanel` | `DocumentMap` | Otra vista de Scintilla con **el mismo documento** (`SCI_SETDOCPOINTER`), zoom −10 y los estilos del lenguaje, sin márgenes, cursor ni barras. La zona visible del editor va marcada con un rectángulo del color de acento. El mapa se desplaza en proporción para que la zona siempre se vea. Clic o arrastrar lleva el editor a esa parte, y la rueda desplaza el editor |
| `FunctionParser` | `functionParser.cpp` | Port de los analizadores: `FunctionUnitParser` (funciones), `FunctionZoneParser` (zonas de clase con sus símbolos de apertura y cierre) y `FunctionMixParser` (clases y, fuera de ellas, funciones sueltas), descartando los comentarios. Buscan con `SCI_SEARCHINTARGET` y la regex de Boost, como N++, en una vista oculta con el mismo documento (para no tocar la búsqueda del editor). Cargan `functionList/overrideMap.xml` y `<lenguaje>.xml`, primero de `~/.config/edita/functionList` y luego de los datos de la aplicación |
| `data/functionList/` | `installer/functionList/` | Los 48 analizadores de N++, sin cambios |
| `FunctionListPanel` | `FunctionListPanel` | El árbol del documento activo: la raíz es el archivo, las clases son ramas y debajo van sus funciones. Un clic lleva al nombre. Tiene filtro, orden alfabético y recarga. Se vuelve a analizar solo, poco después de escribir |
| `FileBrowserPanel` | `FileBrowser` | Una o varias carpetas raíz, carpetas primero y por nombre, sin ocultos. Se leen al desplegarlas y se actualizan si cambian en disco. Doble clic abre el archivo. Botones Añadir carpeta, Localizar el archivo actual y Plegar todo. Menú contextual: abrir, copiar la ruta o el nombre, abrir la carpeta contenedora, buscar en archivos ahí, quitar y quitar todas. Las carpetas, las desplegadas y la elegida se guardan en `session.xml` (`<FileBrowser latestSelectedItem><root foldername><expanded path>`, como N++) |
| Menús y barra | `IDM_VIEW_DOC_MAP`, `IDM_VIEW_DOCLIST`, `IDM_VIEW_FUNC_LIST`, `IDM_VIEW_FILEBROWSER`, `IDM_FILE_OPENFOLDERASWORKSPACE` | Ver > Mapa del documento, Lista de documentos, Lista de funciones y Explorador de carpetas (activables). Archivo > Abrir carpeta como espacio de trabajo… Los cuatro botones de la barra en el lugar de N++ (después de «Lenguaje de usuario»), con iconos propios. Qué paneles están abiertos se recuerda en `config.xml` con los nombres de N++ (`<GUIConfig name="multiInst" documentList="yes" documentMap=… fuctionList=… folderAsWorkspace=…>`) |

## Dos problemas encontrados

- **Leer los analizadores.** El XML convierte los saltos de línea de los atributos en espacios. Las expresiones de N++ están en modo `(?x)` con comentarios `# …` hasta el final de la línea, y sin los saltos el primer comentario se come el resto. N++ tiene un lector propio (`loadFileFunctionParser`); Edita le pide a pugixml que no haga esa conversión.
- **Cerrar la ventana.** Al destruirse la ventana, Qt oculta los paneles cuando los miembros de `MainWindow` ya no existen. Sus avisos de visibilidad llegaban a código que los usaba, y fallaba de vez en cuando. Ahora los paneles se desconectan al empezar a destruir la ventana. Se comprobó con 40 ejecuciones seguidas de la suite.

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Lista de documentos | Columnas opcionales (extensión) y orden por columna | Nombre y carpeta | Lo básico |
| Mapa del documento | También en la vista secundaria, con el plegado sincronizado | Sigue al editor activo; muestra el documento desplegado | Más simple |
| Lista de funciones | Se recarga al cambiar de documento o con el botón | También poco después de escribir | Comodidad |
| Explorador de carpetas | Avisa de los cambios con `ReadDirectoryChanges` | `QFileSystemWatcher` en las carpetas desplegadas | Lo que hay en Linux |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Los `LangType` de Edita son los `langID` de N++ (los que usa `overrideMap.xml`). El analizador de C++ encuentra los métodos de una clase con su clase y las funciones sueltas, sin las comentadas, y la posición es la del nombre. El de Python encuentra métodos y funciones con sus parámetros, como en N++. Texto normal no tiene analizador | ✅ `tst_panels`: `functionParsersOfNotepadPlusPlus` |
| La Lista de funciones muestra el archivo como raíz con sus clases y funciones; un clic lleva al nombre; el filtro y el orden alfabético funcionan; al escribir una función nueva aparece sola | ✅ `functionListFollowsTheDocument` |
| La Lista de documentos tiene los documentos abiertos con el actual seleccionado y el indicador de «sin guardar»; un clic activa y el clic central cierra | ✅ `documentListShowsBothViews` |
| El Mapa comparte el documento del editor con zoom reducido; la zona visible está marcada; un clic abajo lleva el editor hacia el final; si el editor se desplaza, la zona lo sigue | ✅ `documentMapFollowsTheEditor` |
| Explorador: una carpeta (no dos veces), carpetas primero y sin ocultos, localizar un archivo despliega sus carpetas, doble clic abre, y todo va a `session.xml` y vuelve al restaurar la sesión | ✅ `folderAsWorkspace` |
| Las entradas de Ver tienen icono y activan los paneles; el estado se guarda con los nombres de N++ (`fuctionList`) y se recupera al volver a abrir | ✅ `panelsHaveMenuEntriesAndRememberTheirState` |

Se revisó una captura con los cuatro paneles abiertos. Todo está traducido al español.

Tests: `tst_panels` tiene 8 casos (contando `initTestCase` y `cleanupTestCase`). Pasan las 30 suites y el build no da ningún warning.
