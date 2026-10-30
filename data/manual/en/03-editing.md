# Editing

## Basic editing

| Command | Notes |
|---|---|
| {{actionUndo}} / {{actionRedo}} | Undo and redo have no limit while the document is open. |
| {{actionCut}}, {{actionCopy}}, {{actionPaste}} | Without a selection, cut and copy take the whole line (option **Copy or cut the entire line when there is no selection**, Editing page of {{actionPreferences}}). |
| {{actionDelete}} | Deletes the selection, or the character after the cursor. |
| {{actionSelectAll}} | Selects the whole document. |

Other keys of the editor:

| Keys | Action |
|---|---|
| Ctrl+Backspace / Ctrl+Delete | Delete the previous / next word. |
| Ctrl+Shift+Backspace / Ctrl+Shift+Delete | Delete to the start / end of the line. |
| Ctrl+L | Cut the current line. |
| Ctrl+Shift+X | Copy the current line. |
| Ctrl+Shift+L | Delete the current line (without copying it). |
| Ctrl+T | Swap the current line with the previous one. |
| Insert | Switch between insert and overwrite mode. |
| Ctrl+mouse wheel | Zoom in or out. |

All the editor keys can be changed in {{actionShortcutMapper}} (the **Scintilla commands** tab).

## Selection

- **Shift+arrows** select characters and lines; **Ctrl+Shift+arrows** select words.
- **Double click** selects a word; **triple click** selects a line. A click in the line number
  margin selects the whole line; dragging there selects several lines.
- **Rectangular (column) selection**: drag with **Alt** pressed, or use **Alt+Shift+arrows**.
  Typing with a rectangular selection writes on every line at the same time. On some desktops
  Alt+drag moves the window; then use the keyboard, or change that shortcut of the desktop.
- **Multiple cursors**: **Ctrl+click** adds another cursor (or another selection, with
  Ctrl+drag). Whatever you type, delete or paste is applied at every cursor. Esc returns to a
  single cursor. This is the **Multi-Editing** option of the Editing page.
- **Virtual space** (option of the Editing page) lets the cursor and the rectangular selection go
  beyond the end of the lines.

## Column editor

{{actionColumnEditor}} inserts text or numbers in the column of the cursor, on every line from the
cursor to the end of the document, or on each line of a rectangular selection.

- **Text to insert**: the same text on every line.
- **Number to insert**: a sequence that starts at **Initial number** and grows by **Increase by**.
  **Repeat** writes each number several times before going to the next one. **Leading** fills
  with zeros or spaces so that every number has the same width, and **Format** chooses decimal,
  hexadecimal, octal or binary.

For example, with a rectangular selection on ten lines, initial number 1, increase 1 and leading
zeros, the lines get `01` to `10`.

## Line operations

The commands of the **Edit › Line Operations** submenu work on the selected lines, or on the current
line when there is no selection:

| Command | What it does |
|---|---|
| {{actionDuplicateLine}} | Copies the line (or the selection) below. |
| {{actionLineUp}} / {{actionLineDown}} | Moves the lines up or down. |
| {{actionSplitLines}} | Splits long lines at the vertical edge (or at the width of the window). |
| {{actionJoinLines}} | Joins the selected lines into one. |
| {{actionBlankLineAbove}} / {{actionBlankLineBelow}} | Inserts an empty line above or below. |
| {{actionRemoveEmptyLines}} | Removes the empty lines. |
| {{actionRemoveBlankLines}} | Also removes the lines that only have spaces or tabs. |
| {{actionRemoveDuplicateLines}} | Keeps only the first copy of each repeated line. |
| {{actionRemoveConsecutiveDuplicates}} | Removes a line when it is equal to the previous one. |
| {{actionSortReverse}} | Reverses the order of the lines. |
| {{actionSortRandom}} | Shuffles the lines. |

### Sorting

Sorting works on the selected lines, on the whole document when nothing is selected, or on the
**columns** of a rectangular selection: the lines are sorted by the selected text, but move whole.

| Ascending | Descending | Order |
|---|---|---|
| {{actionSortLexAsc}} | {{actionSortLexDesc}} | By character code (uppercase before lowercase). |
| {{actionSortCaseInsAsc}} | {{actionSortCaseInsDesc}} | Ignoring uppercase and lowercase. |
| {{actionSortLocaleAsc}} | {{actionSortLocaleDesc}} | Alphabetical, by the rules of your language (á next to a). |
| {{actionSortIntAsc}} | {{actionSortIntDesc}} | As integers: 2 before 10. |
| {{actionSortCommaAsc}} | {{actionSortCommaDesc}} | As decimals with a comma: `3,5`. |
| {{actionSortDotAsc}} | {{actionSortDotDesc}} | As decimals with a dot: `3.5`. |
| {{actionSortLengthAsc}} | {{actionSortLengthDesc}} | By length. |

If a line is not a number in a numeric sort, nothing is changed and Edita says which line it is.

## Uppercase and lowercase

The commands of the case submenu change the selection (or each selection, with several):

| Command | Example |
|---|---|
| {{actionUpperCase}} | `HELLO WORLD` |
| {{actionLowerCase}} | `hello world` |
| {{actionProperCase}} | `Hello World` (the rest of each word in lowercase) |
| {{actionProperCaseBlend}} | `Hello World` (the rest of each word unchanged: `iPhone` → `IPhone`) |
| {{actionSentenceCase}} | `Hello world. Bye.` |
| {{actionSentenceCaseBlend}} | As above, without lowercasing the rest. |
| {{actionInvertCase}} | `hELLO wORLD` |
| {{actionRandomCase}} | `hElLo WoRLd` |

## Comments

Edita uses the comment symbols of the language of the document (those of `langs.xml`, or those
of the user defined language).

| Command | What it does |
|---|---|
| {{actionToggleComment}} | Comments the lines, or uncomments them if they are commented. |
| {{actionLineComment}} | Adds the line comment symbol (`//`, `#`…) to each line. |
| {{actionLineUncomment}} | Removes it. |
| {{actionStreamComment}} | Encloses the selection in a block comment (`/* … */`). |
| {{actionStreamUncomment}} | Removes the block comment around the cursor or the selection. |

## Spaces and tabs

| Command | What it does |
|---|---|
| {{actionTrimTrailing}} | Removes the spaces at the end of the lines. |
| {{actionTrimLeading}} | Removes the spaces at the start of the lines. |
| {{actionTrimBoth}} | Both. |
| {{actionEolToSpace}} | Joins the lines with a space. |
| {{actionTrimAll}} | Removes the spaces at both ends and joins the lines. |
| {{actionTabToSpace}} | Replaces the tabs with spaces. |
| {{actionSpaceToTabAll}} | Replaces groups of spaces with tabs. |
| {{actionSpaceToTabLeading}} | The same, only in the indentation. |

The macro {{actionMacro0}} removes the trailing spaces and saves in one step.

## Indentation

**Tab** indents the selected lines and **Shift+Tab** unindents them. The width of the tab and
whether it inserts spaces are set in {{actionPreferences}} (Indentation page) for every language,
or for the current document from the indentation field of the status bar. Languages with their
own settings in `langs.xml` keep them.

**Auto-indent** chooses what happens when you press Enter: **Basic** keeps the indentation of
the previous line, and **Advanced** also indents after `{` (and after `:` in Python) and
unindents when you type `}`. **Backspace key unindents** deletes a whole indentation level at
once.

## Brackets and quotes

- With **Auto-Insert** (Auto-Completion page), typing `(`, `[`, `{`, `"` or `'` inserts the
  closing one, and in HTML and XML `<p>` inserts `</p>`.
- The bracket next to the cursor and its pair are highlighted. {{actionGoToMatchingBrace}} jumps
  to the pair and {{actionSelectMatchingBraces}} selects everything between them.
- **Color brackets by nesting level** (Editing page) gives a different color to each level.

## Read-only and end of line

{{actionReadOnly}} prevents changes to the document. The end of line commands are described in
[Ends of line](02-files.md#ends-of-line).
