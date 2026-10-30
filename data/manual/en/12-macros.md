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
in {{actionShortcutMapper}} (Macros tab).


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
