# Macros and the Run menu

A macro records what you type and the editing commands you use, to repeat them later. They are
useful for changes that are too irregular for a search and replace.

## Recording and playing back

1. {{actionRecordMacro}} starts recording. The same command (now **Stop Recording**) stops it.
2. Do the changes once: type, move with the keyboard, use menu commands (cut, paste, duplicate a
   line, change case, find next…).
3. {{actionPlayMacro}} repeats them where the cursor is.

The whole playback is undone in one step.

A typical example: to turn every line `name = value` into `"name": "value",`, start at the
beginning of the first line, record Home, `"`, Ctrl+Right, `"` … End, `,`, Down and Home, and
play it back as many times as needed.

## Running a macro many times

{{actionRunMacro}} runs the recorded macro, or a saved one, a number of times or **until the end of
file**: it stops when the cursor reaches the last line or stops moving.

## Saving macros

{{actionSaveMacro}} saves the recorded macro with a name and, optionally, a shortcut. The saved
macros appear at the end of the Macro menu, and in the [command palette](05-views.md#command-palette).
**Delete a Macro** removes one.

Edita includes {{actionMacro0}}, which removes the trailing spaces and saves.

The macros are kept in `~/.config/edita/shortcuts.xml`, with the same format as Notepad++: the
macros of Notepad++ can be copied to Edita and the other way round. Their shortcuts can be changed
in {{actionShortcutMapper}} (Macros tab); {{actionEditMacros}} opens it right on that tab, where
they can also be deleted.


## The Run menu

The Run menu runs other programs with the document or the text of the cursor: compile or run the
current file, open the word under the cursor in a web page, format with an external tool…

{{actionRun}} opens a dialog where you write the command. The **…** button chooses the program, and
the **$( )** button inserts a variable, which is replaced when the command runs:

| Variable | Value |
|---|---|
| `$(FULL_CURRENT_PATH)` | Full path of the current document |
| `$(CURRENT_DIRECTORY)` | Its folder |
| `$(FILE_NAME)` | Its name, with extension (`main.cpp`) |
| `$(NAME_PART)` | Its name without extension (`main`) |
| `$(EXT_PART)` | Its extension (`.cpp`) |
| `$(CURRENT_WORD)` | The selected text, or the word at the cursor |
| `$(CURRENT_LINE)`, `$(CURRENT_COLUMN)` | The line and column of the cursor (from 1) |
| `$(CURRENT_LINESTR)` | The text of the current line |
| `$(NPP_DIRECTORY)`, `$(NPP_FULL_FILE_PATH)` | The folder and the path of Edita itself |

The command runs with `/bin/sh` in the folder of the current document, so you can use quotes,
pipes, `&&` and environment variables (`$HOME`). Paths with spaces need quotes:
`g++ "$(FULL_CURRENT_PATH)" -o "$(NAME_PART)"`. A command that is only an address
(`https://…`) is opened in the web browser. The commands you run stay in the list of the dialog.

The program runs on its own: its output is not shown. To see it, run the command in the
[terminal](11-terminal.md), or end it with something that shows the result, such as
`… ; read` inside a terminal program.

**Save…** keeps the command in the Run menu with a name and, optionally, a shortcut. Edita includes
the three commands of Notepad++: search the word of the cursor in the PHP manual and in Wikipedia,
and open the file path of the cursor in a new Edita window. {{actionEditRunCommands}} opens the
**Run commands** tab of the {{actionShortcutMapper}}, to change their shortcuts or delete them.

The commands are kept in the `<UserDefinedCommands>` section of `~/.config/edita/shortcuts.xml`, as
in Notepad++.

## Project tasks

{{actionRunTask}} shows what can be built, tested or run in the project of the current document: the
nearest folder, going up, with one of these files. If the document has no file, the first folder of
the Folder as Workspace is used.

| File | Tasks |
|---|---|
| `Makefile` | `make` and `make target` for each target |
| `CMakeLists.txt` | Configure (if there is no `build` folder), `cmake --build` and `ctest` |
| `package.json` | Each script, with npm, pnpm, yarn or bun depending on the lock file |
| `Cargo.toml` | `cargo build`, `test`, `run`, `check` and `clippy` |
| `go.mod` | `go build`, `test`, `vet` and `run` |
| `pyproject.toml`, `pytest.ini`, `setup.py`… | `python3 -m pytest` |
| `meson.build` | Configure, `meson compile` and `meson test` |
| `justfile` | Each recipe |

The task runs with `/bin/sh` in the project folder, and its output appears in the
{{actionTaskOutput}} panel. The errors and warnings Edita recognizes are underlined: double click
one to open the file at that line. They also appear in the Problems panel (View › Problems) next to
those of the [language server](08-language-servers.md), and in the counter of the status bar. Edita
recognizes the messages of GCC, Clang, Go, rustc, TypeScript (`tsc`), Python tracebacks and pytest,
and those of any tool that writes `file:line:column: message`.

{{actionRunLastTask}} runs the last task again, and {{actionStopTask}} stops it together with
everything it started. The panel has the same buttons. Edita does not save the documents before
running a task: save them first with {{actionSaveAll}} if the task uses them.
