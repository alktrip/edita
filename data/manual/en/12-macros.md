# Macros

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
