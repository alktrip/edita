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
| **SelectQuotedText** | Selecciona el texto entre comillas alrededor del cursor. |
| **Indent By Fold** | Sangra el código según su plegado, en lenguajes sin reglas de sangría propias. |
| **NppStripIpAndHost** | Enmascara las direcciones IP y los nombres de host, para compartir registros sin datos sensibles. |
| **ElasticTabstops** | Tabulaciones elásticas: alinea las columnas separadas por tabulaciones, y convierte entre tabulaciones y espacios. |
| **MZC8051** | Compila C para microcontroladores 8051 con el compilador MZC8051 (hay que instalarlo aparte). |
| **Datamodder SQL Formatter** | Da formato a SQL en varios dialectos (T-SQL, MySQL, PostgreSQL, Oracle…) o lo pasa a una línea. |
| **NppCrossCheck** | Compara dos listas y marca los elementos que están en una pero no en la otra. |
| **Pork to Sausage** | Transforma la selección con un programa externo y la sustituye por su salida. |
| **HLASM Lexer** | Colorea HLASM, el ensamblador de IBM para mainframe (`.mlc`, `.mac`, `.cpy`). |
| **OCaml Lexer** | Colorea OCaml (`.ml`, `.mli`, `.mll`, `.mly`) y, si tienes `olint`, marca sus avisos. |

Los plugins que se descargan para Notepad++ son DLL de Windows y no se pueden cargar en Linux. Un
plugin de Notepad++ llega a Edita cuando se compila desde su código, y entonces aparece en el
catálogo. Si copias un DLL a la carpeta de plugins, sale en **Incompatibles**.

**Plugins de lenguaje.** Algunos plugins, como HLASM Lexer y OCaml Lexer, no agregan órdenes sino
un lenguaje. Aparecen en el menú Lenguaje, después de los de Edita y antes de **Lenguaje definido
por el usuario**, y en el selector de la barra de estado. Los archivos con sus extensiones se abren
con ese lenguaje. Sus colores se cambian en el Configurador de estilos, como los de cualquier
lenguaje, y se guardan en el archivo de configuración del plugin
(`~/.config/edita/plugins/config/<plugin>.xml`), así que no cambian con el tema.

Las demás entradas del menú Plugins abren la carpeta de plugins y muestran información de los
plugins cargados. Los plugins que instalas van a `~/.config/edita/plugins/`.

{{actionImportPlugins}} (en **Configuración › Importar**) instala un plugin que tienes en un
archivo: una biblioteca `.so` compilada para Linux o un paquete `.zip` como los del catálogo. Se
copia a la carpeta de plugins y se carga al reiniciar Edita. Los `.dll` de Windows no funcionan.

## Plugins incluidos

| Plugin | Qué hace |
|---|---|
| **MarkdownViewer++** | Muestra una vista previa del documento Markdown en un panel (Ctrl+Mayús+M), con desplazamiento sincronizado, y lo exporta a HTML o PDF, lo imprime o lo copia como HTML. |
| **Python Script** | Ejecuta scripts de Python que controlan el editor, con la API del plugin PythonScript de Notepad++ (`notepad`, `editor`, `console`), así que muchos de sus scripts funcionan sin cambios. Tiene una consola interactiva. (Aún no se incluye en la versión para macOS.) |
| **Detector de secretos** | Subraya en rojo las claves de API, tokens, claves privadas y contraseñas escritas en tus archivos, y avisa antes de un commit desde el panel Git si lo preparado contiene alguno. Ver abajo. |

### Detector de secretos

Es un plugin propio de Edita; Notepad++ no tiene nada parecido. Mientras escribes, subraya en rojo lo que parece un secreto: tokens de GitHub, GitLab, Slack, npm y PyPI; claves de AWS, Google, OpenAI, Anthropic, Stripe, SendGrid y Azure; claves privadas (`-----BEGIN … PRIVATE KEY-----`); JWT; contraseñas en URLs (`postgres://usuario:clave@…`); y asignaciones como `password = "…"`, o cualquier `…_SECRET=` en un archivo `.env`, cuando el valor parece aleatorio. Los valores de ejemplo (`changeme`, `${VARIABLE}`, `<tu-clave>`) no cuentan.

En **Plugins › Detector de secretos**:

- **Panel de secretos** y **Analizar el documento actual** muestran la lista: línea, tipo y el valor enmascarado (`ghp_…tB`). Doble clic selecciona el secreto.
- **Analizar el proyecto** revisa el repositorio de Git del documento (lo que sigue Git y lo nuevo que no ignora) o, fuera de Git, su carpeta. Doble clic abre el archivo en el secreto.
- **Ignorar el secreto del cursor**, o el botón **Ignorar** del panel, deja de avisar de ese valor en cualquier archivo. Para permitir una línea, añade el comentario `edita:allow-secret`. También valen los de gitleaks (`gitleaks:allow`) y detect-secrets (`pragma: allowlist secret`).
- **Marcar secretos mientras escribes** enciende o apaga el subrayado.

Antes de hacer un commit desde {{actionGitCommit}}, revisa las líneas que se van a confirmar. Si encuentra algo, lo enumera y pregunta si hacer el commit de todos modos. Los commits hechos desde la terminal no pasan por aquí.

## Escribir plugins

La interfaz de plugins (`edita_plugin.h`) se instala con Edita (en `share/edita/plugin-sdk`), y el
código de Edita incluye una guía (`docs/plugins.md`) y un plugin de ejemplo. La interfaz usa los
mismos mensajes que Notepad++ (`NPPM_*` y `SCI_*`), así que adaptar un plugin de Notepad++ consiste
sobre todo en sustituir las partes de Windows.

Durante toda la versión 1.x la interfaz no cambia: un plugin compilado para Edita 1.0 funciona en
cualquier 1.x sin volver a compilarlo. Las versiones nuevas solo añaden mensajes.
