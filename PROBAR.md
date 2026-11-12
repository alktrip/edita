# Probar Edita

*[English below](#testing-edita)*

Gracias por probar Edita, un editor de texto para Linux inspirado en Notepad++. Lo que encuentres nos dice qué arreglar. Antes de cada versión estable se publica una versión de prueba (beta o rc): si usas una, tus reportes llegan a tiempo para la estable.

## Instalar

1. Descarga el AppImage (`Edita-<versión>-x86_64.AppImage`, o `-aarch64.AppImage` en un equipo ARM) de la [última versión](https://github.com/alktrip/edita/releases/latest).
2. Dale permiso de ejecución: `chmod +x Edita-*.AppImage` (o en las propiedades del archivo).
3. Ábrelo con doble clic o desde una terminal: `./Edita-*.AppImage`.

En Debian y Ubuntu también puedes instalar el paquete `.deb` de la misma página (`sudo apt install ./edita_*_amd64.deb`, o `_arm64.deb`), y en Fedora y openSUSE el `.rpm` (`sudo dnf install ./edita-*.rpm` o `sudo zypper install ./edita-*.rpm`). Así queda en el menú y en **Abrir con**, y se abre con `edita` desde la terminal.

En x86_64 funciona en Ubuntu 22.04 o más nuevo, Debian 12, Fedora 36 y posteriores y openSUSE Tumbleweed. En arm64, en Ubuntu 24.04, Debian 13, Raspberry Pi OS 13 (trixie), Fedora 40 y posteriores. Tu configuración se guarda en `~/.config/edita/`.

> **macOS: todavía no hay versión.** Por limitaciones en el desarrollo, por ahora Edita es solo para Linux. El `.dmg` del release 0.0.1 se retiró; macOS se retomará cuando haya una versión estable para Linux.

Si al abrirlo dice que falta una biblioteca, repórtalo con el mensaje completo. Para ver todas las que faltan de una vez: `./Edita-*.AppImage --appimage-extract >/dev/null && ldd squashfs-root/usr/bin/edita | grep "not found"`.

## Reportar un problema

**Ayuda › Reportar un problema…** abre el formulario de GitHub con tu versión y tu sistema ya escritos. Solo hace falta una cuenta de GitHub. Puedes escribir en español o en inglés. Un buen reporte dice:

- qué hiciste, paso a paso;
- qué esperabas y qué pasó;
- si hace falta, el archivo con el que pasa (o un trozo).

Si Edita se cierra de golpe, al volver a abrirlo te ofrece reportar el fallo con los detalles ya escritos.

## Qué probar

Úsalo como tu editor de todos los días. Eso es lo más útil. Si tienes tiempo, revisa también:

- **Abrir y guardar**: archivos grandes, codificaciones (UTF-8, Latin-1, UTF-16), fines de línea de Windows, archivos que otro programa cambia.
- **Buscar y reemplazar**: con expresiones regulares, en todos los archivos de una carpeta.
- **Lo que usabas en Notepad++**: macros, edición en columna, sesiones, el mapa del documento, la lista de funciones, los atajos (Configuración › Asignación de atajos).
- **Tu escritorio**: el tema claro y el oscuro, la escala de la pantalla (HiDPI), Wayland y X11.
- **El manual** (F1): si algo no se entiende o no coincide con lo que ves, también es un error.

### Plugins

En **Plugins › Administrador de plugins › Disponibles** están los plugins del catálogo: plugins de Notepad++ compilados para Linux desde su código original. Los DLL de Windows de Notepad++ no funcionan en Linux.

Para cada plugin que instales, revisa: que se instale sin errores, que sus órdenes aparezcan en el menú Plugins, que hagan lo que dicen y que sus ventanas (opciones, «Acerca de») se vean bien y respondan. Los plugins de lenguaje (HLASM Lexer, OCaml Lexer) agregan un lenguaje en vez de órdenes: abre un archivo con su extensión y revisa que se coloree.

| Plugin | Qué probar |
|---|---|
| XML Tools | Pretty print, Linearize, Check XML syntax now, XPath, cierre automático de etiquetas |
| MIME Tools | Base64 y URL: codificar y decodificar una selección |
| Reverse Lines | Invertir las líneas de una selección y del documento |
| Simplify JSON | Dar formato y minificar JSON (este se compila en tu equipo: necesita `cmake` y un compilador de C++) |
| TidyText | Resaltar y reparar caracteres invisibles y texto mal codificado |
| Auto Detect Indention | Que la tecla Tab siga la sangría de cada archivo |
| SQL Column Highlighter | En un `INSERT`, resaltar la columna de un valor |
| Select 'N' Launch | Abrir la selección con la aplicación de una extensión |
| NppPluginOpenHost | Abrir `/etc/hosts` |
| SelectQuotedText | Seleccionar el texto entre comillas alrededor del cursor |
| Indent By Fold | Sangrar código según su plegado |
| NppStripIpAndHost | Enmascarar las IP y los nombres de host de un registro |
| ElasticTabstops | Alinear columnas separadas por tabulaciones |
| MZC8051 | Necesita el compilador MZC8051; si no lo tienes, revisa solo que el menú y sus ventanas se vean bien |
| Datamodder SQL Formatter | Dar formato a SQL y pasarlo a una línea; sus opciones |
| NppCrossCheck | Comparar dos listas |
| Pork to Sausage | Transformar la selección con un programa (por ejemplo `sort` o `tr a-z A-Z`) |
| HLASM Lexer | Plugin de lenguaje: abrir un `.mlc` (o elegir Lenguaje › HLASM) y ver que se colorea; cambiar sus colores en el Configurador de estilos |
| OCaml Lexer | Plugin de lenguaje: abrir un `.ml` y ver que se colorea; con `olint` instalado, que marque sus avisos |

También vienen con Edita **Python Script** (scripts de Python que controlan el editor) y **MarkdownViewer++** (vista previa de Markdown, Ctrl+Mayús+M).

---

# Testing Edita

Thank you for testing Edita, a text editor for Linux inspired by Notepad++. What you find tells us what to fix. Before each stable version there is a test version (beta or rc): if you use one, your reports arrive in time for the stable one.

## Install

1. Download the AppImage (`Edita-<version>-x86_64.AppImage`, or `-aarch64.AppImage` on an ARM computer) from the [latest release](https://github.com/alktrip/edita/releases/latest).
2. Make it executable: `chmod +x Edita-*.AppImage` (or in the file properties).
3. Open it with a double click or from a terminal: `./Edita-*.AppImage`.

On Debian and Ubuntu you can also install the `.deb` package from the same page (`sudo apt install ./edita_*_amd64.deb`, or `_arm64.deb`), and on Fedora and openSUSE the `.rpm` (`sudo dnf install ./edita-*.rpm` or `sudo zypper install ./edita-*.rpm`). Then Edita is in the menu and in **Open With**, and `edita` opens it from a terminal.

On x86_64 it works on Ubuntu 22.04 or newer, Debian 12, Fedora 36 and later and openSUSE Tumbleweed. On arm64, on Ubuntu 24.04, Debian 13, Raspberry Pi OS 13 (trixie), Fedora 40 and later. Your settings are stored in `~/.config/edita/`.

> **macOS: there is no version yet.** Because of development limitations, for now Edita is only for Linux. The `.dmg` of release 0.0.1 has been removed; macOS will resume once there is a stable Linux version.

If it says that a library is missing when opening it, report it with the full message. To see all the missing ones at once: `./Edita-*.AppImage --appimage-extract >/dev/null && ldd squashfs-root/usr/bin/edita | grep "not found"`.

## Report a problem

**Help › Report a Problem…** opens the GitHub form with your version and your system already filled in. You only need a GitHub account. You can write in English or Spanish. A good report says:

- what you did, step by step;
- what you expected and what happened;
- if needed, the file it happens with (or part of it).

If Edita crashes, the next time you open it, it offers to report the crash with the details already written.

## What to test

Use it as your everyday editor: that helps the most. If you have time, also check:

- **Opening and saving**: large files, encodings (UTF-8, Latin-1, UTF-16), Windows line endings, files changed by another program.
- **Find and replace**: with regular expressions, in all the files of a folder.
- **What you used in Notepad++**: macros, column editing, sessions, the document map, the function list, the shortcuts (Settings › Shortcut Mapper).
- **Your desktop**: the light and dark themes, display scaling (HiDPI), Wayland and X11.
- **The manual** (F1): if something is unclear or does not match what you see, that is a bug too.

### Plugins

**Plugins › Plugins Admin › Available** lists the catalog plugins: Notepad++ plugins compiled for Linux from their original source code. The Windows DLLs of Notepad++ do not work on Linux.

For each plugin you install, check that it installs without errors, that its commands appear in the Plugins menu, that they do what they say, and that its windows (options, "About") look right and respond. Language plugins (HLASM Lexer, OCaml Lexer) add a language instead: open a file with their extension (`.mlc`, `.ml`) and check that it is highlighted, and that you can change its colors in the Style Configurator. The table above lists what to try in each one; the commands have the same names in the menu.

Edita also comes with **Python Script** (Python scripts that control the editor) and **MarkdownViewer++** (Markdown preview, Ctrl+Shift+M).
