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

**Notepad++ Plugin List…** downloads the official list of Notepad++, to see whether a plugin you
use there has a version for Edita.

The other entries of the Plugins menu open the plugins folder and show information about the
loaded plugins. The plugins you install go to `~/.config/edita/plugins/`.

## Included plugins

| Plugin | What it does |
|---|---|
| **MarkdownViewer++** | Shows a preview of the Markdown document in a panel (Ctrl+Shift+M), with synchronized scrolling, and exports it to HTML or PDF, prints it or copies it as HTML. |
| **Python Script** | Runs Python scripts that control the editor, with the API of the PythonScript plugin of Notepad++ (`notepad`, `editor`, `console`), so many of its scripts work unchanged. It has an interactive console. (Not included in the macOS version yet.) |

## Plugins built from source

| Plugin | What it does |
|---|---|
| **XML Tools** | Checks and validates XML (with errors shown under their lines), pretty print, linearize, XPath, closing tags automatically… Needs `libxml2` and `libxslt`. |
| **MIME Tools** | Base64, quoted-printable and URL encoding and decoding. |

## Writing plugins

The plugin interface (`edita_plugin.h`) is installed with Edita (in `share/edita/plugin-sdk`),
and the source code of Edita includes a guide (`docs/plugins.md`) and an example plugin. The interface uses the
same messages as Notepad++ (`NPPM_*` and `SCI_*`), so porting a Notepad++ plugin is mostly
replacing the Windows parts.
