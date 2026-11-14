# Git

When a file belongs to a Git repository, Edita shows what changed since the last commit, who
wrote each line, and the state of the files in the Folder as Workspace panel. It only needs `git`
installed; nothing has to be configured.

## Changes in the margin

A narrow margin next to the text marks the lines that differ from the last commit (HEAD):

| Mark | Meaning |
|---|---|
| Green bar | Added lines |
| Blue bar | Modified lines |
| Red triangle | Lines deleted here |

The comparison is made with the text in the editor, so the marks follow your changes before you
save. Rest the mouse on a mark to see the original lines. The marks are updated when you switch
documents, save, or come back to the window (there may be a new commit made in a terminal).

{{actionNextGitChange}} and {{actionPreviousGitChange}} go to the next or previous changed block.
They have no shortcut; assign one in {{actionShortcutMapper}} or use the
[command palette](05-views.md#command-palette).

The marks also appear in the scroll bar and in the Document Map. Turn them off with **Changes
since the last Git commit** (Margins page of {{actionPreferences}}).

## Reverting or staging a block

Click a mark in the margin to open the menu of that block:

- **Revert This Change** puts the lines of the last commit back in the document. Only that block
  changes, the file is not saved and {{actionUndo}} undoes it.
- **Stage This Change** moves that block to the Git index (`git add` of a part of the file), to
  include it in the next commit. What was already staged is kept. If the document has unsaved
  changes, Edita asks whether to save it first, because what is staged is what is on disk. In a file
  Git does not track yet, the whole file is staged.
- **Compare with Git (HEAD)**, **Next Change** and **Previous Change**.

The same, for the block of the cursor line, is in {{actionRevertGitChange}} and
{{actionStageGitChange}}. UTF-16 files can only be staged whole.

## The Git panel

{{actionGitCommit}} shows the changed files of the repository of the current document, with the
branch. The **Staged** files are at the top and the unstaged **Changes** below:

- **Stage**, **Unstage** and **Stage All** work with the chosen files. Double click one to open it.
- Write the message and press **Commit** (or Ctrl+Enter) to commit what is staged. **Amend the last
  commit** replaces it (`git commit --amend`).
- **Refresh** reads the state again, although Edita already does it when saving and when coming back
  to the window.

The commit uses your Git settings (name, email, hooks). If Git gives an error, Edita shows it as is.

### History

The **History** tab of the panel shows the last 200 commits of the repository, with their author,
date and short hash. With **Only the current file**, the ones that changed the document, also under
its earlier names (`git log --follow`); the list changes when you move to another document.

- When you choose a commit, the files it changed appear below: **A** added, **M** modified,
  **D** deleted and **R** renamed (with the earlier name).
- Double-click a file to open its diff in that commit; double-click the commit for the whole diff,
  with the full message. They open in a new read-only document, highlighted as Diff.

## Who changed each line (blame)

At the end of the line of the cursor, in gray, Edita shows who changed it last and when:
`Ana Ruiz, 9 days ago • Read with encoding`. Lines you changed and did not commit say
**You, not committed yet**. Turn it off with **Who changed the current line (Git blame)** (Margins page).

{{actionGitBlame}} shows a margin with the commit, the author and the date at the start of each
block of lines of the same commit. The color goes from the accent color (less than a week) to gray
(more than two years). Rest the mouse on it to see the author, the email, the date, the message
and the commit.

{{actionShowLineCommit}} opens the commit of the current line (`git show`) in a new read-only
document, with the changes colored as a diff.

## Comparing with the last commit

{{actionCompareGit}} shows the document next to its version of the last commit, with the
differences aligned and highlighted. See [Comparing documents](10-compare.md).

## Git in the Folder as Workspace

In a repository, the [Folder as Workspace](05-views.md#folder-as-workspace) panel shows the
branch (in the tooltip of the root folder) and colors the files with a letter on the right:

| Letter | State |
|---|---|
| M | Modified |
| A | Added to the index |
| U | Untracked (new) |
| R | Renamed |
| D | Deleted |
| ! | Conflict |

Folders with changes inside have a dot, and ignored files are shown in gray. The state is read
again when you save, when you come back to the window and when something changes in an expanded
folder. Turn it off with **Git status of the files in Folder as Workspace** (Margins page).
