# Autocompletion and snippets

## Autocompletion

While you type, Edita can show a list of words that complete the one you are writing. Use the
arrows to choose one and Enter or Tab to insert it; Esc closes the list.

The words come from two places:

- The **API file** of the language: the functions and keywords that Notepad++ knows for it
  (`autoCompletion/<language>.xml`). Functions have their own icon.
- The **words of the document**.

If a [language server](08-language-servers.md) is running for the document, its suggestions
replace those of the API file.

The Auto-Completion page of {{actionPreferences}} sets:

| Option | Meaning |
|---|---|
| Show the list | Never, with the functions of the language, with the words of the document, or with both. |
| From the | How many characters must be typed before the list appears. |
| Ignore numbers | Don't offer numbers as words. |
| Brief list | Show only the words that start with what was typed. |
| Function parameters hint on input "(" | Show the parameters of a function when typing `(`. |

The commands of the **Edit › Auto-Completion** submenu open each list on demand:

| Command | What it does |
|---|---|
| {{actionCompleteFunction}} | The list of functions and keywords (or of the language server). |
| {{actionCompleteWord}} | The list of words of the document. If there is only one, it is written directly. |
| {{actionFunctionHint}} | The parameters of the function at the cursor, with the current one highlighted. With several overloads, click the hint or use the arrows to go through them. |
| {{actionCompletePath}} | Completes a file path that starts with `/`, `~/` or `./`. |

The words of the API file can be extended: copy the file of the language to
`~/.config/edita/autoCompletion/` and edit it there. The format is the same as in Notepad++.

## Snippets

A snippet is a piece of code with gaps to fill in. Type its **prefix** and press **Tab**: for
example, `for` + Tab in C++ inserts

```
for (int i = 0; i < n; ++i) {

}
```

with `int` selected. Whatever you type replaces it. The next gap is `i`: what you type there
appears also in the other places where `i` is used. **Tab** goes to the next gap and **Shift+Tab** to the previous one. The snippet ends at
its last gap, when you press Esc or when the cursor leaves it, and the whole snippet is undone in
one step.

- The prefix must be a whole word: `forwhile` does not expand `while`.
- If the autocompletion list is open when you press Tab, the snippet wins.
- Each line of the snippet gets the indentation of the current line, and its tabs become the
  indentation of the document (tabs or spaces).
- Turn off **Tab after the prefix of a snippet inserts it** (Auto-Completion page) to use only the
  list.

{{actionInsertSnippet}} shows the list of snippets of the language with a search field. If text is
selected, the snippet replaces it, and many snippets use it (for example, to surround it with a
tag or a block).

Edita includes snippets for C, C++, Python, JavaScript, TypeScript, HTML, CSS, Bash, Markdown,
Java, Rust, Go, PHP, SQL and Lua, and some for every language: `hoy` (today's date), `ahora`
(date and time) and `uuid`.

### Your own snippets

{{actionEditSnippets}} opens your snippets file for the language of the current document
(`~/.config/edita/snippets/<language>.json`); if it does not exist, it is created with an example.
When you save it, the snippets are read again. Your snippets replace those of Edita with the same
name or prefix. The file `global.json` holds snippets for every language.

The format is the one of Visual Studio Code, so its snippet files can be copied as they are:

```json
{
  // The name of the snippet
  "Print to console": {
    "prefix": "log",
    "body": ["console.log('${1:message}', $2);", "$0"],
    "description": "Log output to console"
  }
}
```

- `prefix`: the word that expands it (or a list of words).
- `body`: the text, as a string or a list of lines.
- `description`: shown in the list of {{actionInsertSnippet}}.

Comments (`//`, `/* */`) and trailing commas are allowed. In the body:

| Syntax | Meaning |
|---|---|
| `$1`, `$2`… | Gaps, in the order Tab visits them. A repeated number is a mirror: it is written everywhere at once. |
| `${1:text}` | A gap with a default text, selected when you reach it. Gaps can be nested. |
| `${1|one,two,three|}` | A choice; the first option is written. |
| `$0` | Where the cursor ends (by default, at the end). |
| `$NAME`, `${NAME:default}` | A variable (see below). |
| `\$`, `\}`, `\\` | A literal `$`, `}` or `\`. |

The variables:

| Variable | Value |
|---|---|
| `TM_SELECTED_TEXT` | The selected text |
| `TM_CURRENT_LINE`, `TM_CURRENT_WORD` | The current line, the word at the cursor |
| `TM_LINE_NUMBER`, `TM_LINE_INDEX` | The line number, from 1 / from 0 |
| `TM_FILENAME`, `TM_FILENAME_BASE` | The file name, with / without extension |
| `TM_DIRECTORY`, `TM_FILEPATH` | The folder, the full path |
| `CLIPBOARD` | The text of the clipboard |
| `CURRENT_YEAR`, `CURRENT_YEAR_SHORT`, `CURRENT_MONTH`, `CURRENT_MONTH_NAME`, `CURRENT_DATE`, `CURRENT_DAY_NAME`, `CURRENT_HOUR`, `CURRENT_MINUTE`, `CURRENT_SECOND` | The date and time |
| `UUID`, `RANDOM`, `RANDOM_HEX` | A random identifier or number |
