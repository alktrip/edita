# Files and sessions

## Creating and opening files

- {{actionNew}} creates an empty document called `new 1`, `new 2`… It uses the end of line,
  encoding and language set in {{actionPreferences}} (New Document page).
- {{actionOpen}} opens one or several files. You can also drag files from the file manager onto
  the window, or open them from the command line (see
  [Starting Edita](01-introduction.md#starting-edita)).
- {{actionGoToFile}} finds a file by name among the open documents, the recent files and the
  files of the workspace folders, with fuzzy matching: typing `mwcpp` finds `MainWindow.cpp`.
- {{actionOpenFolderAsWorkspace}} adds a folder to the
  [Folder as Workspace](05-views.md#folder-as-workspace) panel.
- {{actionReopenClosed}} reopens the last closed file; repeat it to reopen the previous ones.

The **recent files** appear at the end of the File menu (or in a submenu). The number of entries
and how they are displayed are set in {{actionPreferences}} (Recent Files History page). The open
and save dialogs start in the folder of the current document, in the last folder used, or in a
fixed folder (New Document page).

By default Edita uses its own file dialogs, which are shown in the language of Edita. Turn off
**Use Edita's file dialogs** in the General page to use those of your desktop.

## Saving

| Command | What it does |
|---|---|
| {{actionSave}} | Saves the current document. An untitled document asks for a name. |
| {{actionSaveAs}} | Saves the document with another name; the tab now refers to the new file. |
| {{actionSaveCopy}} | Saves a copy with another name; the tab keeps referring to the original file. |
| {{actionSaveAll}} | Saves every modified document. |

Edita keeps the encoding, the end of line and the byte order mark (BOM) of each file when saving.
To change them, see [Encodings](#encodings) and [Ends of line](#ends-of-line).

{{actionReadOnly}} protects the current document against changes (the file on disk is not
changed). {{actionReload}} discards the changes and reads the file again.

## Closing

{{actionClose}} closes the current document; {{actionCloseAll}} closes all of them and
{{actionCloseAllBut}} all but the current one. If a document has unsaved changes, Edita asks
whether to save it. The tab menu (right click on a tab) also has **Close All to the Left** and
**Close All to the Right**. {{actionQuit}} closes the window.

With **Exit on close the last tab** (General page), closing the last document closes Edita.

## Encodings

When opening a file Edita detects its encoding: Unicode files with a BOM, UTF-8 without BOM, and
files in other character sets (with the same detector as Notepad++). If the detection fails, the
character set of **Default 8-bit character set** (MISC. page) is used. Turn off **Autodetect
character encoding on open** to always use it.

The **Encoding** menu has two groups of commands:

- **Encode in…** (UTF-8, UTF-8-BOM, UTF-16…) and **Character sets** *reinterpret* the bytes of
  the file: use them when a file was opened with the wrong encoding and its accents look wrong.
  The text in the editor changes; the file is not modified until you save it.
- **Convert to…** and **Convert to another character set** *keep the text* and change how it is
  stored when the file is saved.

| Encode as | Convert to |
|---|---|
| {{actionEncode4}} | {{actionConvert4}} |
| {{actionEncode1}} | {{actionConvert1}} |
| {{actionEncode2}} | {{actionConvert2}} |
| {{actionEncode3}} | {{actionConvert3}} |

The encoding field of the status bar opens the same commands with a search field. The encoding of
new documents is set in the New Document page of {{actionPreferences}}.

## Ends of line

Each file keeps its end of line: Windows (CR LF), Unix (LF) or Macintosh (CR). The status bar
shows it, and {{actionEol0}}, {{actionEol2}} and {{actionEol1}} convert the whole document.
{{actionShowEol}} displays the end of line characters.

## Changes made by other programs

When another program modifies or deletes an open file, Edita shows a bar above the editor that
offers to reload it. You can configure this in {{actionPreferences}} (MISC. page):

- **Enable**: watch the files for changes.
- **For all opened files**: check every open document, not only the visible one.
- **Update silently if there are no unsaved changes**: reload without asking.
- **Scroll to the last line after update**: useful for log files.

## Sessions and backups

When you close Edita it remembers the open documents, the cursor position in each one, the
split view and the folders of the workspace, and restores them the next time (option **Remember
current session for next launch**, Backup page of {{actionPreferences}}). `--nosession` on the
command line starts without restoring or saving the session.

With **Periodic backup of unsaved changes** (snapshot mode, on by default), Edita saves a copy of
the modified documents every few seconds in `~/.config/edita/backup/`. If Edita or the computer
stops unexpectedly, the documents come back with their changes, even those that were never saved.
Closing Edita with unsaved documents doesn't ask either: they are kept for the next session.

**On save, back up the previous version** keeps the previous contents of the file every time you
save it: as `file.bak` next to the file, or with the date in the name in an `nppBackup` folder.
The backups can go to a fixed folder instead of the file's folder.

## Printing and PDF

- {{actionPrint}} opens the print dialog; {{actionPrintNow}} prints with the default printer
  without asking.
- {{actionExportPdf}} saves the document as a PDF file with the same layout.

Both keep the syntax highlighting. The Print page of {{actionPreferences}} sets the line numbers,
the colors (as on screen, inverted, black on white or without background), the margins, and the
header and footer. The header and footer can use these variables:

| Variable | Value |
|---|---|
| `$(FULL_CURRENT_PATH)` | Full path of the file |
| `$(FILE_NAME)` | File name with extension |
| `$(NAME_PART)` | File name without extension |
| `$(EXT_PART)` | Extension |
| `$(CURRENT_DIRECTORY)` | Folder of the file |
| `$(SHORT_DATE)`, `$(LONG_DATE)`, `$(TIME)` | Date and time of printing |
| `$(CURRENT_PRINTING_PAGE)` | Page number |
