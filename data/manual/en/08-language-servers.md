# Language servers

A **language server** is a program that understands a programming language: it finds errors as
you type, completes names with what is really available, knows where each function is defined
and can rename a symbol in the whole project. Edita uses the Language Server Protocol (LSP), the
same as Visual Studio Code, so it works with the servers made for it.

Edita does not include the servers: install the ones for your languages with the package manager
of your system (or with `npm`, `pip`, `cargo`…). When you open a file, Edita uses the first known
server of its language that is installed. Nothing has to be configured.

| Language | Servers (in order of preference) |
|---|---|
| C, C++, Objective-C | `clangd` |
| Python | `basedpyright`, `pyright`, `pylsp`, `jedi-language-server`, `ruff` |
| JavaScript, TypeScript | `typescript-language-server` |
| Rust | `rust-analyzer` |
| Go | `gopls` |
| Shell | `bash-language-server` |
| Lua | `lua-language-server` |
| JSON, CSS, HTML | the servers of Visual Studio Code (`vscode-json-language-server`…) |
| YAML, TOML, Markdown | `yaml-language-server`, `taplo`, `marksman` |
| Java, PHP, LaTeX, Zig… | `jdtls`, `intelephense`, `texlab`, `zls`… |

The Language Servers page of {{actionPreferences}} lists all of them and says which are
installed. There you can also write another command for a language (for example
`clangd --background-index`), or `off` to disable that language, and turn the servers off
completely.

The project folder sent to the server is the workspace folder that contains the file or, if there
is none, the nearest folder upwards with a project file (`compile_commands.json`, `Cargo.toml`,
`package.json`, `.git`…). For C and C++, clangd needs a `compile_commands.json` to know the
include paths and flags of the project; CMake generates it with
`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`.

## Diagnostics

The errors are underlined with a red wavy line, the warnings in yellow and the information in
blue; hints are marked with gray dots. Rest the mouse on the line to read the message. The errors
and warnings are also marked in the scroll bar.

The status bar shows how many errors and warnings the current document has (`⨯ 4 ⚠ 1`); click it
to open the Problems panel. {{actionProblems}} lists the diagnostics of every open document; click
one to go to it.

## Completion and information

- While you type a name, or after `.`, `->` or `::`, the list shows the completions of the server,
  with an icon for each kind (function, variable, class, module…). Many insert a
  [snippet](07-completion.md#snippets) with the parameters: Tab goes through them. Some also add
  what is needed elsewhere, such as an `#include`.
- When you type `(` or `,`, or with {{actionFunctionHint}}, the signature of the function appears
  with the current parameter highlighted.
- Resting the mouse on a name for half a second shows its type and documentation.
- With **Hints at the end of the lines** (Language Servers page), the deduced types and the names
  of the parameters of the visible lines are shown in gray at the end of each line:
  `total: int  ·  (width, height)`.

## Navigation

| Command | What it does |
|---|---|
| {{actionGoToDefinition}} | Opens the file where the symbol at the cursor is defined. If there are several definitions, a list appears. |
| {{actionFindReferences}} | Lists every use of the symbol in the [search results panel](04-search.md#the-search-results-panel); F4 goes through them. |
| {{actionWorkspaceSymbol}} | Finds a function, class or variable by name in the whole project, while you type. |
| {{actionGoToSymbol}} | The symbols of the current document. |

## Changing the code

| Command | What it does |
|---|---|
| {{actionRenameSymbol}} | Asks for a new name and renames the symbol in every file of the project. Open files are changed in place; the others are opened unsaved, to review them. Each file is undone in one step. |
| {{actionFormatDocument}} | Formats the document (or the selection, if the server can) with the indentation of the document. |
| {{actionCodeActions}} | The fixes and refactorings the server offers for the line or the selection (add a missing include, fix a spelling, extract a function…). The preferred ones are listed first. |

When there are actions for the line of the cursor, a **lightbulb** appears in the margin; click it
to see them.

## When something does not work

- {{actionLspLog}} opens the log of the server of the current document: the messages it wrote and
  its errors.
- {{actionLspRestart}} restarts it, for example after changing `compile_commands.json` or
  installing a dependency.
- Files larger than 8 MB are not sent to the server.
