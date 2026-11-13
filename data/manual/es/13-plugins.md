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
| **Editor hexadecimal** | Muestra y edita los bytes de un archivo, también de un binario. Ver abajo. |
| **Detector de secretos** | Subraya en rojo las claves de API, tokens, claves privadas y contraseñas escritas en tus archivos, y avisa antes de un commit desde el panel Git si lo preparado contiene alguno. Ver abajo. |

### Editor hexadecimal

Es un plugin propio de Edita, escrito desde cero. Hace lo mismo que el plugin HEX-Editor de Notepad++, pero sobre los bytes del archivo en el disco y no sobre el texto del editor, que Edita guarda en UTF-8. Por eso también abre archivos binarios y muestra los bytes reales de un archivo en otra codificación.

**Plugins › Editor hexadecimal** abre su panel con el archivo actual (Ctrl+Alt+Mayús+H, como en Notepad++). **Abrir un archivo en el editor hexadecimal…** abre cualquier otro, aunque no esté abierto en Edita.

- Tres columnas: la posición, los bytes en hexadecimal y los mismos como ASCII (los que no se pueden mostrar, como `·`). **Tab** cambia entre la de bytes y la de ASCII.
- En la columna de bytes se escriben dígitos hexadecimales y en la de ASCII, caracteres. **Insert** cambia entre sobrescribir (**SOB**) e insertar (**INS**). Retroceso y Supr borran. Ctrl+Z y Ctrl+Y deshacen y rehacen.
- Mayús con las flechas o el ratón seleccionan. Ctrl+C copia: en la columna de bytes, como `4F 6B`, y en la de ASCII, como texto. Ctrl+V pega bytes escritos en hexadecimal, o texto.
- **Ir a la posición** acepta `0x1F`, `1Fh` o `31`. **Buscar** busca bytes en hexadecimal o texto, hacia delante o hacia atrás.
- La línea de abajo muestra el valor que empieza en el cursor como entero de 8, 16, 32 y 64 bits (con dos números, little endian y big endian), como flotante y como carácter UTF-8.
- **Guardar** escribe el archivo. Si está abierto en Edita, Edita pregunta si volver a cargarlo, como con cualquier cambio hecho fuera.

Abre archivos de hasta 512 MB.

### Detector de secretos

Es un plugin propio de Edita; Notepad++ no tiene nada parecido. Mientras escribes, subraya en rojo lo que parece un secreto: tokens de GitHub, GitLab, Slack, npm y PyPI; claves de AWS, Google, OpenAI, Anthropic, Stripe, SendGrid y Azure; claves privadas (`-----BEGIN … PRIVATE KEY-----`); JWT; contraseñas en URLs (`postgres://usuario:clave@…`); y asignaciones como `password = "…"`, o cualquier `…_SECRET=` en un archivo `.env`, cuando el valor parece aleatorio. Los valores de ejemplo (`changeme`, `${VARIABLE}`, `<tu-clave>`) no cuentan.

En **Plugins › Detector de secretos**:

- **Panel de secretos** y **Analizar el documento actual** muestran la lista: línea, tipo y el valor enmascarado (`ghp_…tB`). Doble clic selecciona el secreto.
- **Analizar el proyecto** revisa el repositorio de Git del documento (lo que sigue Git y lo nuevo que no ignora) o, fuera de Git, su carpeta. Doble clic abre el archivo en el secreto.
- **Ignorar el secreto del cursor**, o el botón **Ignorar** del panel, deja de avisar de ese valor en cualquier archivo. Para permitir una línea, añade el comentario `edita:allow-secret`. También valen los de gitleaks (`gitleaks:allow`) y detect-secrets (`pragma: allowlist secret`).
- **Marcar secretos mientras escribes** enciende o apaga el subrayado.

Antes de hacer un commit desde {{actionGitCommit}}, revisa las líneas que se van a confirmar. Si encuentra algo, lo enumera y pregunta si hacer el commit de todos modos. Los commits hechos desde la terminal no pasan por aquí.

## FTP y SFTP (del catálogo)

Es un plugin propio de Edita que hace lo que NppFTP en Notepad++. No viene incluido: se instala desde **Plugins › Administrador de plugins**, en la pestaña del catálogo (**FTP and SFTP**). Usa los programas del sistema: `ssh` y `sftp` (OpenSSH) para SFTP, con tu configuración de `~/.ssh`, tus claves y tu agente, y `curl` para FTP. Casi todas las distribuciones los traen.

**Plugins › FTP › Panel de FTP** abre el panel. Con **Perfiles…** agregas los servidores: nombre, protocolo, servidor, puerto (vacío: el del protocolo), usuario, carpeta inicial y, en SFTP, la clave privada.

| Protocolo | Qué es |
|---|---|
| **SFTP** | Archivos por SSH. Es el recomendado |
| **FTPES** | FTP cifrado con TLS explícito, en el puerto 21 |
| **FTPS** | FTP cifrado con TLS implícito, en el puerto 990 |
| **FTP** | Sin cifrar: la contraseña viaja a la vista. Úsalo solo en tu red |

- **Conectar** abre la sesión y muestra la carpeta inicial. Al abrir una carpeta del árbol se lista.
- **Contraseñas:** no se guardan en el disco. En SFTP, Edita prueba primero tus claves y el agente, y si no alcanzan, pide la contraseña o la frase de paso de la clave. En FTP la pide al conectar. Se recuerdan solo mientras Edita está abierto.
- **Servidores nuevos:** la primera vez que te conectas a un servidor SFTP, Edita muestra la huella de su clave y pregunta si confiar en él. Al aceptar, la guarda en `~/.ssh/known_hosts`, como `ssh`. Si la clave de un servidor conocido cambia, Edita no se conecta y explica cómo quitar la anterior.
- **Abrir:** doble clic en un archivo lo descarga a una copia local (en `~/.cache/edita/ftp/<perfil>/`) y lo abre. **Al guardarlo en Edita, se sube solo.** Si ya no estás conectado, se guarda solo en el equipo y Edita te avisa.
- **Botones del panel:** actualizar, subir el documento actual a la carpeta seleccionada, descargar el archivo seleccionado en otra carpeta, crear carpeta, cambiar el nombre y eliminar. También están en el menú contextual del árbol. Solo se eliminan carpetas vacías.
- **Registro:** abajo se anota cada operación y cada error.

En FTPES y FTPS, si el servidor usa un certificado propio, desactiva **Verificar el certificado del servidor** en su perfil.

## Escribir plugins

La interfaz de plugins (`edita_plugin.h`) se instala con Edita (en `share/edita/plugin-sdk`), y el
código de Edita incluye una guía (`docs/plugins.md`) y un plugin de ejemplo. La interfaz usa los
mismos mensajes que Notepad++ (`NPPM_*` y `SCI_*`), así que adaptar un plugin de Notepad++ consiste
sobre todo en sustituir las partes de Windows.

Durante toda la versión 1.x la interfaz no cambia: un plugin compilado para Edita 1.0 funciona en
cualquier 1.x sin volver a compilarlo. Las versiones nuevas solo añaden mensajes.
