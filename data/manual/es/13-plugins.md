# Plugins

Los plugins añaden órdenes a Edita. Aparecen en el menú **Plugins**, cada uno con su submenú. Los
plugins de Edita son bibliotecas nativas de Linux (o de macOS) con una interfaz calcada de la de
Notepad++; los DLL de Windows de Notepad++ no se pueden cargar, pero varios plugins de Notepad++
están disponibles adaptados o compilados desde su código.

## Administrador de plugins

{{actionPluginsAdmin}} instala, actualiza y quita plugins. Sus pestañas:

| Pestaña | Contenido |
|---|---|
| Disponibles | Plugins que se pueden instalar. |
| Actualizaciones | Plugins instalados con una versión más nueva. |
| Instalados | Los plugins instalados. |
| Desactivados | Plugins instalados que no se cargan. |
| Incompatibles | Plugins que no se pueden cargar, y por qué. |

Marca los plugins y pulsa **Instalar**, **Actualizar**, **Quitar**, **Desactivar** o **Activar**.
El campo **Buscar** busca en los nombres y después en las descripciones. Los cambios se aplican al
reiniciar Edita; al cerrar el diálogo, Edita ofrece reiniciarse (conserva la sesión).

Algunos plugins llevan **(se compila)**: Edita descarga su código original, lo verifica y lo
compila en tu equipo. Hace falta `cmake` y un compilador de C++, y el diálogo muestra el avance.
El paquete compilado se guarda, así que al reinstalarlo no se vuelve a compilar.

### El catálogo de plugins de Edita

Al abrir el administrador, Edita descarga la lista del [catálogo de plugins de
Edita](https://github.com/alktrip/edita-plugins) sin hacerte esperar: mientras tanto muestra la
última lista que aceptó (o la que viene con Edita) y, cuando llega la nueva, actualiza las
pestañas. La versión de la lista aparece abajo a la derecha.

Antes de usar una lista, Edita comprueba que esté firmada por el catálogo y que no sea más antigua
que la última que aceptó. Cada paquete se comprueba con su SHA-256, que viene en la lista firmada.
Sin conexión, se usa la última lista aceptada.

Los plugins del catálogo son plugins de Notepad++ compilados para Linux desde su código original.
Se instalan ya compilados, salvo dos casos en que se compilan en tu computadora, como los que
llevan **(se compila)**: si sus binarios no sirven para tu computadora (otra arquitectura u otra
versión de Qt) o si su licencia no permite distribuir el binario.

| Plugin | Qué hace |
|---|---|
| **Reverse Lines** | Invierte el orden de las líneas de la selección o del documento. |
| **Simplify JSON** | Escapa, desescapa, da formato y minifica JSON. |
| **TidyText** | Resalta y repara caracteres invisibles y texto mal codificado (mojibake). |
| **Auto Detect Indention** | Ajusta la tecla Tab a la sangría de cada documento. |
| **SQL Column Highlighter** | En un `INSERT`, resalta los valores de una columna o la columna de un valor. |
| **Select 'N' Launch** | Abre la selección con la aplicación asociada a una extensión. |
| **NppPluginOpenHost** | Abre `/etc/hosts`. |
| **XML Tools** | Comprueba y valida XML (con los errores bajo sus líneas), da formato, lo pasa a una línea, XPath, XSLT, cierra etiquetas automáticamente… Necesita `libxml2` y `libxslt` (los traen casi todas las distribuciones). |
| **MIME Tools** | Codifica y decodifica Base64, quoted-printable y URL. |

**Lista de plugins de Notepad++…** descarga la lista oficial de Notepad++, para ver si un plugin
que usas ahí tiene versión para Edita.

Las demás entradas del menú Plugins abren la carpeta de plugins y muestran información de los
plugins cargados. Los plugins que instalas van a `~/.config/edita/plugins/`.

## Plugins incluidos

| Plugin | Qué hace |
|---|---|
| **MarkdownViewer++** | Muestra una vista previa del documento Markdown en un panel (Ctrl+Mayús+M), con desplazamiento sincronizado, y lo exporta a HTML o PDF, lo imprime o lo copia como HTML. |
| **Python Script** | Ejecuta scripts de Python que controlan el editor, con la API del plugin PythonScript de Notepad++ (`notepad`, `editor`, `console`), así que muchos de sus scripts funcionan sin cambios. Tiene una consola interactiva. (Aún no se incluye en la versión para macOS.) |

## Escribir plugins

La interfaz de plugins (`edita_plugin.h`) se instala con Edita (en `share/edita/plugin-sdk`), y el
código de Edita incluye una guía (`docs/plugins.md`) y un plugin de ejemplo. La interfaz usa los
mismos mensajes que Notepad++ (`NPPM_*` y `SCI_*`), así que adaptar un plugin de Notepad++ consiste
sobre todo en sustituir las partes de Windows.
