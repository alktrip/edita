# Plugins

Plugins add commands to Edita. They appear in the **Plugins** menu, each with its own submenu.
Edita plugins are native Linux (or macOS) libraries with an interface modeled on the one of
Notepad++; the Windows DLLs of Notepad++ cannot be loaded, but several Notepad++ plugins are
available ported or built from their source code.

## Plugins Admin

{{actionPluginsAdmin}} installs, updates and removes plugins. Its tabs:

| Tab | Contents |
|---|---|
| Available | Plugins that can be installed. |
| Updates | Installed plugins with a newer version. |
| Installed | The installed plugins. |
| Disabled | Plugins that are installed but not loaded. |
| Incompatible | Plugins that cannot be loaded, and why. |

Check the plugins and click **Install**, **Update**, **Remove**, **Disable** or **Enable**. The
**Find what** field searches the names and then the descriptions. The changes take effect when
Edita restarts; when closing the dialog, Edita offers to restart (it keeps the session).

Some plugins are marked **(from source)**: Edita downloads their original source code, checks
it, and builds it on your computer. This needs `cmake` and a C++ compiler, and the dialog shows
the progress. The built package is kept, so reinstalling it does not build it again.

### The Edita plugin catalog

When the dialog opens, Edita downloads the list of the [Edita plugin
catalog](https://github.com/alktrip/edita-plugins) without making you wait: meanwhile it shows
the last list it accepted (or the one that comes with Edita) and, when the new one arrives, it
updates the tabs. The version of the list is shown at the bottom right.

Before using a list, Edita checks that it is signed by the catalog and that it is not older than
the last one it accepted. Each package is checked with its SHA-256, which comes in the signed
list. Without a connection, the last accepted list is used.

The plugins of the catalog are Notepad++ plugins built for Linux from their original source
code. They are installed already built, except in two cases where they are built on your
computer, like the ones marked **(from source)**: when their binaries do not work on your
computer (another architecture or another version of Qt), or when their license does not allow
distributing the binary.

| Plugin | What it does |
|---|---|
| **Reverse Lines** | Reverses the order of the lines of the selection or of the document. |
| **Simplify JSON** | Escapes, unescapes, pretty prints and minifies JSON. |
| **TidyText** | Highlights and fixes invisible characters and badly encoded text (mojibake). |
| **Auto Detect Indention** | Adapts the Tab key to the indentation of each document. |
| **SQL Column Highlighter** | In an `INSERT`, highlights the values of a column or the column of a value. |
| **Select 'N' Launch** | Opens the selection with the application associated with an extension. |
| **NppPluginOpenHost** | Opens `/etc/hosts`. |
| **XML Tools** | Checks and validates XML (with errors shown under their lines), pretty print, linearize, XPath, XSLT, closing tags automatically… Needs `libxml2` and `libxslt` (almost every distribution has them). |
| **MIME Tools** | Base64, quoted-printable and URL encoding and decoding. |
| **SelectQuotedText** | Selects the quoted text around the cursor. |
| **Indent By Fold** | Indents code by its folding, in languages without indentation rules of their own. |
| **NppStripIpAndHost** | Masks IP addresses and host names, to share logs without sensitive data. |
| **ElasticTabstops** | Elastic tabstops: aligns tab-separated columns, and converts between tabs and spaces. |
| **MZC8051** | Compiles C for 8051 microcontrollers with the MZC8051 compiler (installed separately). |
| **Datamodder SQL Formatter** | Formats SQL in several dialects (T-SQL, MySQL, PostgreSQL, Oracle…) or minifies it. |
| **NppCrossCheck** | Compares two lists and marks the items that are in one but not in the other. |
| **Pork to Sausage** | Transforms the selection with an external program and replaces it with its output. |
| **HLASM Lexer** | Highlights HLASM, the IBM mainframe assembler (`.mlc`, `.mac`, `.cpy`). |
| **OCaml Lexer** | Highlights OCaml (`.ml`, `.mli`, `.mll`, `.mly`) and, if you have `olint`, marks its warnings. |

The plugins you download for Notepad++ are Windows DLLs and cannot be loaded on Linux. A Notepad++
plugin comes to Edita when it is compiled from its source code, and then it appears in the catalog.
If you copy a DLL to the plugins folder, it shows up under **Incompatible**.

**Language plugins.** Some plugins, like HLASM Lexer and OCaml Lexer, add a language instead of
commands. They appear in the Language menu, after the languages of Edita and before **User Defined
Language**, and in the status bar picker. Files with their extensions open with that language.
Their colors are changed in the Style Configurator, like those of any language, and are saved in
the configuration file of the plugin (`~/.config/edita/plugins/config/<plugin>.xml`), so they do
not change with the theme.

The other entries of the Plugins menu open the plugins folder and show information about the
loaded plugins. The plugins you install go to `~/.config/edita/plugins/`.

{{actionImportPlugins}} (in **Settings › Import**) installs a plugin you have in a file: a `.so`
library built for Linux or a `.zip` package like those of the catalog. It is copied to the plugins
folder and loaded when Edita restarts. Windows `.dll` files do not work.

## Included plugins

| Plugin | What it does |
|---|---|
| **MarkdownViewer++** | Shows a preview of the Markdown document in a panel (Ctrl+Shift+M), with synchronized scrolling, and exports it to HTML or PDF, prints it or copies it as HTML. |
| **Python Script** | Runs Python scripts that control the editor, with the API of the PythonScript plugin of Notepad++ (`notepad`, `editor`, `console`), so many of its scripts work unchanged. It has an interactive console. (Not included in the macOS version yet.) |

## Writing plugins

The plugin interface (`edita_plugin.h`) is installed with Edita (in `share/edita/plugin-sdk`),
and the source code of Edita includes a guide (`docs/plugins.md`) and an example plugin. The interface uses the
same messages as Notepad++ (`NPPM_*` and `SCI_*`), so porting a Notepad++ plugin is mostly
replacing the Windows parts.
