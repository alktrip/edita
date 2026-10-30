# Troubleshooting

## Starting Edita

**The AppImage does not start.** Check that it has execute permission (`chmod +x`). AppImages need
FUSE 2: if your system only has FUSE 3, install the `libfuse2` (or `fuse2`) package, or run
`./Edita-*.AppImage --appimage-extract-and-run`. Running it from a terminal shows the reason when
it fails.

**macOS says the application cannot be opened.** Edita is not notarized by Apple. Open it with right
click › Open, or allow it in System Settings › Privacy & Security › Open Anyway (see
[Installation](01-introduction.md#installation)).

**Files open in the window that is already open, and I wanted another one.** Edita runs as a single
instance. Use `--multiInst`, or choose **Always open a new window** in **Instances** (General page
of {{actionPreferences}}).

**Edita does not restore my documents.** Check **Remember current session for next launch** (Backup
page), and that you did not start it with `--nosession`.

## Text and files

**The accents look wrong (`Ã©` instead of `é`).** The file was opened with the wrong encoding. Use
**Encoding › Encode in UTF-8** or the right character set; don't use *Convert to*, which would keep
the wrong characters. See [Encodings](02-files.md#encodings).

**A file saved in Edita has other line endings in another program.** Check the end of line in the
status bar and convert it with the commands of [Ends of line](02-files.md#ends-of-line).

**I closed Edita without saving and lost my changes.** With snapshot mode (on by default) the
documents come back the next time with their changes. If they don't, look in
`~/.config/edita/backup/`.

**Alt+drag moves the window instead of selecting a column.** Your desktop uses Alt+drag. Use
Alt+Shift+arrows, or change the shortcut of the desktop (see
[Selection](03-editing.md#selection)).

**The font of the editor is not the one of the theme.** Edita uses the monospaced font of the system.
Turn off **Use the system monospace font** in the Editing page.

## Language servers

**There are no diagnostics or completions of the server.** Check that the server is installed and in
the `PATH` (the Language Servers page of {{actionPreferences}} says whether Edita finds it), that
the servers are on, and that the file has been saved with its extension. {{actionLspLog}} shows
what the server says; {{actionLspRestart}} starts it again.

**clangd does not find the headers.** It needs a `compile_commands.json` in the project; with CMake,
configure with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` (see [Language servers](08-language-servers.md)).

## Plugins

**A plugin appears as incompatible.** The Incompatible tab of {{actionPluginsAdmin}} explains why.
Windows DLLs cannot be loaded; look for the plugin in the list of Edita.

**A plugin built from source fails.** The build window shows the log. Usually a tool or a library is
missing (`cmake`, a C++ compiler, `libxml2`…): the message names it.

## Starting over

To go back to the default configuration, close Edita and rename `~/.config/edita/` (see
[Configuration files](14-preferences.md#configuration-files)). Your files are not affected.

## Reporting a problem

Report errors and suggestions at <https://github.com/alktrip/edita/issues>. Include the version
({{actionAbout}}), your system, and the steps to reproduce the problem.
