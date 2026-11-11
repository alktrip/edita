# Lo que tiene Notepad++ y Edita no

Fecha: 2026-11-05. Base: Notepad++ de `referencia/npp`, todas las órdenes del menú principal según su `english.xml` (339) y las páginas de Preferencias. Del lado de Edita, todas las acciones de sus menús (482, sacadas de la ventana real) y el menú contextual de las pestañas. Una orden cuenta como hecha si Edita tiene la misma (`IDM_*`) o una con el mismo nombre. Después se revisaron a mano las que tienen otro nombre: las codificaciones («Encode in UTF-8»), la búsqueda incremental (la barra de búsqueda en línea de la [fase 33](fase33.md)), cerrar a la izquierda o a la derecha y copiar la ruta (en el menú de las pestañas), y los archivos recientes.

**Resultado: Edita tiene unas 220 de las 339 órdenes (✅: las que se hicieron después, desde la [fase 67](fase67.md); la [69](fase69.md) añadió las de Archivo, Sobre la selección y los paneles de Editar, la [70](fase70.md), cinco páginas de Preferencias, la [71](fase71.md), importar, el menú contextual y las macros, la [74](fase74.md), las órdenes cortas, la [75](fase75.md), otra ventana, el pegado especial y `tabContextMenu.xml`, y la [76](fase76.md), la asociación de archivos). De lo que tiene sentido en Linux faltan el texto de derecha a izquierda y los paneles de proyecto; otras 20 no aplican o se descartaron.**

## Faltan, y tienen sentido en Linux

### Archivo y pestañas
| Orden de N++ | Qué hace |
|---|---|
| ✅ Close All Unchanged | Cierra los documentos sin cambios |
| ✅ Pin Tab / Close All BUT Pinned | Fijar pestañas (N++ 8.6) y cerrar todas menos las fijadas |
| ✅ Rename… | Cambia el nombre del archivo en disco |
| ✅ Move to Recycle Bin | Lo manda a la papelera (en Linux, la de freedesktop) |
| ✅ Open in Default Viewer | Lo abre con la aplicación del sistema |
| ✅ Move Tab Forward/Backward, Move to Start/End | Mover la pestaña |
| ✅ 1st…9th Tab, First Tab, Last Tab | Ir a la pestaña N (Ctrl+1…9 en N++) |
| ✅ Apply Color 1…5 / Remove Color | Colores de pestaña |
| ✅ Open/Move in New Instance | Abrir el documento en otra ventana de Edita |

### Editar
| Orden de N++ | Qué hace |
|---|---|
| ✅ Increase/Decrease Line Indent | En el menú (Tab y Mayús+Tab ya lo hacen en el editor) |
| ✅ Begin/End Select (y en modo columna) | Marcar el inicio y el fin de una selección larga sin arrastrar |
| ✅ Date Time (short, long, customized) | Insertar fecha y hora; el formato se elige en Preferencias |
| ✅ Copy Current File Path/Filename/Dir. Path | En el menú Editar (hoy solo en el menú de la pestaña) |
| ✅ Copy All Filenames / File Paths | De todos los documentos abiertos |
| ✅ Multi-select: seleccionar la siguiente coincidencia (4 variantes ×2), deshacer la última añadida, saltar la actual | Selección múltiple como en VS Code (Ctrl+D) |
| ✅ Read-Only for All Documents / Clear | Solo lectura para todos |
| ✅ Search on Internet / Change Search Engine | Buscar la selección en la web (y página de Preferencias «Search Engine») |
| ✅ Open File / Open Containing Folder (de la selección) | Abrir la ruta seleccionada |
| ✅ Character Panel | Panel con la tabla ASCII y sus códigos |
| ✅ Clipboard History | Panel con lo último copiado |
| ✅ Paste HTML/RTF Content, Copy/Cut/Paste Binary Content | Pegar el formato enriquecido como texto, y copiar texto con caracteres NUL |
| Text Direction RTL/LTR | Texto de derecha a izquierda (árabe, hebreo) |
| ✅ Redact Selection | Tapar la selección con █ |

### Buscar
| Orden de N++ | Qué hace |
|---|---|
| ✅ Find characters in range… | Buscar caracteres por rango de código (no ASCII, de control…) |

### Ver
| Orden de N++ | Qué hace |
|---|---|
| ✅ Toggle Full Screen Mode (F11) | Pantalla completa |
| ✅ Post-It (F12) | Ventana sin barras ni bordes |
| ✅ Always on Top | Siempre visible |
| ✅ Synchronize Vertical/Horizontal Scrolling | Desplazar las dos vistas juntas |
| ✅ Summary… | Líneas, palabras, caracteres, tamaño, fechas del documento |
| ✅ Monitoring (tail -f) | Seguir un archivo de registro que crece |
| ✅ Show Non-Printing Characters, Show Control Characters & Unicode EOL | Las opciones de símbolos nuevas de N++ 8.x |
| Project Panel 1…3 | Los paneles de proyecto (`.workspace` de N++) |

### Configuración, Macro, Ayuda
| Orden de N++ | Qué hace |
|---|---|
| ✅ Import plugin(s)… / Import style theme(s)… | Instalar un plugin o un tema desde un archivo |
| ✅ Modify Shortcut/Delete Macro… | Editar las macros guardadas (para el menú Ejecutar ya existe) |
| ✅ Edit Popup ContextMenu | Personalizar el menú contextual (`contextMenu.xml`) |
| ✅ Generate MD5/SHA-1/SHA-256/SHA-512 | Calcular hashes del texto, de archivos o de la selección |
| ✅ Toggle macro record | Empezar y parar con una sola orden |
| ✅ Function Parameters Previous/Next Hint | Recorrer las sobrecargas en la ayuda de parámetros |
| ✅ Debug Info… | La versión, el sistema, los plugins y la configuración, para copiar en un reporte |
| ✅ Command Line Arguments… | La ayuda de la línea de órdenes |
| ✅ Home / Project Page / Online Manual / Forum | Enlaces (en Edita: el repositorio, las discusiones y los reportes) |

### Páginas de Preferencias que faltan
| Página de N++ | Qué tiene |
|---|---|
| ✅ Toolbar | Ocultar la barra, tamaño de los iconos |
| ✅ Language | Menú de lenguajes compacto y lenguajes ocultos del menú |
| ✅ Multi-Instance & Date | El formato de la fecha y hora personalizada |
| ✅ Delimiter | Qué caracteres forman una palabra (doble clic, resaltado) |
| ✅ Performance | Los límites para archivos grandes (Edita los tiene, sin página) |
| ✅ Cloud & Link | Configuración en una carpeta de la nube; enlaces que se pueden pulsar |
| ✅ Search Engine | El buscador de «Search on Internet» |
| ✅ File Association | Asociar extensiones con Edita (en Linux, el tipo MIME del escritorio) |

## No aplican en Linux, o se descartaron

| Orden de N++ | Por qué |
|---|---|
| Explorer, cmd, PowerShell (abrir la carpeta del archivo) | De Windows; en Edita, Archivo › Abrir la carpeta contenedora tiene el administrador de archivos y la terminal ([fase 69](fase69.md)) |
| Read-Only Attribute in Windows | El atributo de Windows; en Linux son los permisos |
| Set Updater Proxy… | Edita usa el proxy del sistema |
| Get More Plugins, Notepad++ User Defined Languages Collection | Enlaces al sitio de N++ (los plugins de Edita llegan por su catálogo) |
| Distraction Free Mode | Descartado (el «modo Zen»: decisión del proyecto) |
| Validate shortcuts.xml | Interna de N++ |
