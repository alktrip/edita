# Terminal

Edita includes a terminal in a panel at the bottom of the window, to run commands without leaving
the editor. It is a complete terminal: colors, mouse, full screen programs (`htop`, `vim`, `mc`),
history and tabs.

## Opening the terminal

| Command | What it does |
|---|---|
| {{actionTerminal}} | Opens the terminal, moves the focus to it or, if it already has it, hides it. On Spanish keyboards Ctrl+Ñ does the same. |
| {{actionNewTerminal}} | Opens another terminal in a new tab. |
| {{actionRunSelectionInTerminal}} | Sends the selected text (or the current line) to the terminal and runs it. |

New terminals start in the folder of the current document; without one, in the first workspace
folder or in your home folder. The tab menu also has **Open Containing Folder in Terminal**, and
the Folder as Workspace panel **Open in Terminal**.

Each terminal has a tab, with the title that the shell sets (or the name of the program); the
**+** button opens another. When the shell exits, its tab closes, and with the last one the panel
is hidden.

## Using it

- The keys go to the shell even when they are shortcuts of the window (Ctrl+F, Ctrl+W, F3…). The
  Ctrl+Shift shortcuts, Ctrl+Tab, Ctrl+PgUp/PgDown and the shortcuts of the terminal itself keep
  working.
- **Copy** and **paste**: Ctrl+Shift+C and Ctrl+Shift+V, or Shift+Insert. Selecting text with the
  mouse also copies it to the X11 selection, to paste it with the middle button.
- **History**: 10 000 lines, with the mouse wheel, the scroll bar or Shift+PgUp/PgDown.
- **Mouse**: programs that use it receive it; hold Shift to select text in them.
- The right click menu has copy, paste, select all and clear.

The font, the size and the colors are those of the default style of the editor, so they follow the
theme.

## Choosing the shell

By default the terminal runs your shell (`$SHELL`, or `/bin/sh`). To use another one, write it
in **Shell** (MISC. page of {{actionPreferences}}), with its arguments: `zsh -l`, `fish`,
`bash --login`.
