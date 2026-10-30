# Archivos y sesiones

## Crear y abrir archivos

- {{actionNew}} crea un documento vacío llamado `new 1`, `new 2`… Usa el fin de línea, la
  codificación y el lenguaje elegidos en {{actionPreferences}} (página Documento nuevo).
- {{actionOpen}} abre uno o varios archivos. También puedes arrastrar archivos desde el
  administrador de archivos a la ventana, o abrirlos desde la línea de órdenes (ver
  [Iniciar Edita](01-introduction.md#iniciar-edita)).
- {{actionGoToFile}} busca un archivo por su nombre entre los documentos abiertos, los recientes y
  los de las carpetas de trabajo, con búsqueda difusa: escribir `mwcpp` encuentra `MainWindow.cpp`.
- {{actionOpenFolderAsWorkspace}} añade una carpeta al panel
  [Explorador de carpetas](05-views.md#explorador-de-carpetas).
- {{actionReopenClosed}} vuelve a abrir el último archivo cerrado; repítelo para recuperar los
  anteriores.

Los **archivos recientes** aparecen al final del menú Archivo (o en un submenú). Cuántos se
guardan y cómo se muestran se elige en {{actionPreferences}} (página Archivos recientes). Los
diálogos de abrir y guardar empiezan en la carpeta del documento actual, en la última carpeta
usada o en una carpeta fija (página Documento nuevo).

De serie, los diálogos de abrir y guardar son los de tu escritorio. Activa **Usar los diálogos de
archivos de Edita** en la página General para usar los de Edita, que se muestran en el idioma de
Edita.

## Guardar

| Orden | Qué hace |
|---|---|
| {{actionSave}} | Guarda el documento actual. Si no tiene nombre, lo pide. |
| {{actionSaveAs}} | Guarda el documento con otro nombre; la pestaña pasa a ser la del archivo nuevo. |
| {{actionSaveCopy}} | Guarda una copia con otro nombre; la pestaña sigue siendo la del archivo original. |
| {{actionSaveAll}} | Guarda todos los documentos modificados. |

Al guardar, Edita conserva la codificación, el fin de línea y la marca de orden de bytes (BOM)
de cada archivo. Para cambiarlos, ver [Codificaciones](#codificaciones) y
[Fines de línea](#fines-de-linea).

{{actionReadOnly}} protege el documento actual contra cambios (el archivo del disco no cambia).
{{actionReload}} descarta los cambios y vuelve a leer el archivo.

## Cerrar

{{actionClose}} cierra el documento actual; {{actionCloseAll}} los cierra todos y
{{actionCloseAllBut}}, todos menos el actual. Si un documento tiene cambios sin guardar, Edita
pregunta si quieres guardarlo. El menú de las pestañas (clic derecho en una pestaña) también
tiene **Cerrar todo a la izquierda** y **Cerrar todo a la derecha**. {{actionQuit}} cierra la
ventana.

Con **Salir al cerrar el último documento** (página General), al cerrar el último documento se
cierra Edita.

## Codificaciones

Al abrir un archivo, Edita detecta su codificación: archivos Unicode con BOM, UTF-8 sin BOM y
archivos en otros juegos de caracteres (con el mismo detector que Notepad++). Si la detección
falla, se usa el juego de **Juego de caracteres de 8 bits por defecto** (página Varios).
Desactiva **Detectar la codificación al abrir** para usar siempre ese.

El menú **Codificación** tiene dos grupos de órdenes:

- **Codificar en…** (UTF-8, UTF-8-BOM, UTF-16…) y **Juegos de caracteres** *reinterpretan* los
  bytes del archivo: úsalas cuando un archivo se abrió con la codificación equivocada y los
  acentos se ven mal. Cambia el texto del editor; el archivo no se modifica hasta que lo guardes.
- **Convertir a…** y **Convertir a otro juego de caracteres** *conservan el texto* y cambian cómo
  se guarda en el archivo.

| Codificar en | Convertir a |
|---|---|
| {{actionEncode4}} | {{actionConvert4}} |
| {{actionEncode1}} | {{actionConvert1}} |
| {{actionEncode2}} | {{actionConvert2}} |
| {{actionEncode3}} | {{actionConvert3}} |

El campo de la codificación de la barra de estado abre las mismas órdenes con un buscador. La
codificación de los documentos nuevos se elige en la página Documento nuevo de
{{actionPreferences}}.

## Fines de línea

Cada archivo conserva su fin de línea: Windows (CR LF), Unix (LF) o Macintosh (CR). La barra de
estado lo muestra, y {{actionEol0}}, {{actionEol2}} y {{actionEol1}} convierten todo el documento.
{{actionShowEol}} muestra los caracteres de fin de línea.

## Cambios hechos por otros programas

Cuando otro programa modifica o borra un archivo abierto, Edita muestra una barra encima del
editor que ofrece volver a cargarlo. Se configura en {{actionPreferences}} (página Varios):

- **Avisar**: vigilar si los archivos cambian.
- **En todos los documentos abiertos**: revisar todos los documentos, no solo el visible.
- **Actualizar sin preguntar si no hay cambios sin guardar**: volver a cargar sin preguntar.
- **Ir al final tras actualizar**: útil con archivos de registro (logs).

## Sesiones y copias de seguridad

Al cerrar Edita se recuerdan los documentos abiertos, la posición del cursor en cada uno, la
vista dividida y las carpetas de trabajo, y se restauran la próxima vez (casilla **Recordar la
sesión para el próximo arranque**, página Copias de seguridad de {{actionPreferences}}).
`--nosession` en la línea de órdenes arranca sin restaurar ni guardar la sesión.

Con **Copias periódicas de los cambios sin guardar** (modo snapshot, activado de serie), Edita
guarda cada pocos segundos una copia de los documentos modificados en `~/.config/edita/backup/`.
Si Edita o el equipo se detienen de forma inesperada, los documentos vuelven con sus cambios,
incluso los que nunca se guardaron. Por eso, al cerrar Edita con documentos sin guardar tampoco
se pregunta nada: se conservan para la próxima sesión.

**Al guardar, copiar la versión anterior** conserva el contenido anterior del archivo cada vez que
lo guardas: como `archivo.bak` junto al archivo, o con la fecha en el nombre en una carpeta
`nppBackup`. Las copias pueden ir a una carpeta fija en lugar de a la del archivo.

## Imprimir y PDF

- {{actionPrint}} abre el diálogo de impresión; {{actionPrintNow}} imprime con la impresora
  predeterminada sin preguntar.
- {{actionExportPdf}} guarda el documento como PDF con el mismo formato.

Las dos conservan el resaltado de sintaxis. La página Impresión de {{actionPreferences}} elige los
números de línea, los colores (como en pantalla, invertidos, negro sobre blanco o sin fondo), los
márgenes, la cabecera y el pie. La cabecera y el pie pueden usar estas variables:

| Variable | Valor |
|---|---|
| `$(FULL_CURRENT_PATH)` | Ruta completa del archivo |
| `$(FILE_NAME)` | Nombre del archivo con extensión |
| `$(NAME_PART)` | Nombre del archivo sin extensión |
| `$(EXT_PART)` | Extensión |
| `$(CURRENT_DIRECTORY)` | Carpeta del archivo |
| `$(SHORT_DATE)`, `$(LONG_DATE)`, `$(TIME)` | Fecha y hora de la impresión |
| `$(CURRENT_PRINTING_PAGE)` | Número de página |
