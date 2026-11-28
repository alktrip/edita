<p align="center">
  <img src="resources/logo.png" alt="Edita logo" width="112">
</p>

<h1 align="center">Edita</h1>

<p align="center">
  <b>English</b> &nbsp;|&nbsp; <a href="README.es.md"><b>Español</b></a>
</p>

<p align="center">
  <b>A Notepad++-style editor, built for Linux.</b><br>
  A native Qt 6 text and code editor with the editing engine, configuration files and plugin API of Notepad++,<br>
  plus the tools developers expect today: LSP, Git, an integrated terminal and project tasks.
</p>

<p align="center">
  <a href="https://github.com/alktrip/edita/releases/latest"><img alt="Latest release" src="https://img.shields.io/github/v/release/alktrip/edita?label=release&color=2ea44f"></a>
  <a href="https://github.com/alktrip/edita/actions/workflows/linux.yml"><img alt="Linux CI" src="https://github.com/alktrip/edita/actions/workflows/linux.yml/badge.svg"></a>
  <a href="COPYING"><img alt="License: GPL v3" src="https://img.shields.io/badge/license-GPL--3.0--or--later-blue"></a>
  <img alt="Linux x86_64 and arm64" src="https://img.shields.io/badge/Linux-x86__64%20%7C%20arm64-informational?logo=linux&logoColor=white">
</p>

<p align="center">
  <a href="https://alktrip.github.io/edita/"><b>Website</b></a> &nbsp;·&nbsp;
  <a href="#install"><b>Install</b></a> &nbsp;·&nbsp;
  <a href="https://github.com/alktrip/edita/releases"><b>Releases</b></a> &nbsp;·&nbsp;
  <a href="data/manual/en"><b>User manual</b></a> &nbsp;·&nbsp;
  <a href="docs/plugins.md"><b>Plugin SDK</b></a> &nbsp;·&nbsp;
  <a href="https://github.com/alktrip/edita/issues/new/choose"><b>Report a bug</b></a> &nbsp;·&nbsp;
  <a href="README.es.md">Español</a>
</p>

![Edita with the light theme: Folder as Workspace, C++ code with breadcrumbs and the Document Map](docs/images/en/edita-light.png)

> [!NOTE]
> **Edita is an independent project, derived from Notepad++ under the GNU GPL v3. It is not affiliated with or endorsed by Don Ho or the Notepad++ project.** Download the [latest release](https://github.com/alktrip/edita/releases/latest), and [tell us what you find](https://github.com/alktrip/edita/issues/new/choose).

---

## Why Edita

Notepad++ is one of the most loved editors on Windows, and it only runs on Windows. People who move to Linux lose the workflow they know: its menus and shortcuts, its search, its macros, its themes and its plugins.

**Edita is a native Linux editor that keeps that workflow.** It is built on the same editing engine as Notepad++ (Scintilla and Lexilla), reads the same configuration files and speaks the same plugin API. On top of that, it adds what a developer expects from a modern editor.

**More than a text editor, less than an IDE.** Edita has language servers, Git, a terminal and project tasks, but it stays an editor of files and folders: there is no project model to set up, no debugger, no build system of its own (it runs yours: Make, CMake, npm, Cargo…) and no refactoring beyond what the language server offers. It opens in a fraction of a second and works the same with one file or a whole repository. If you need a debugger or deep project tooling, an IDE is the better choice.

| If you are… | Edita gives you… |
|---|---|
| **Coming from Notepad++** | The same menus, shortcuts, search modes, macros, sessions, themes and `langs.xml` / `stylers.xml` / `shortcuts.xml` files. |
| **A developer on Linux** | A fast native editor with LSP, Git, a terminal, project tasks and file comparison, for when an IDE is more than you need. |
| **A plugin author** | A documented C API compatible with Notepad++ messages, an SDK and a toolchain to port existing Notepad++ plugins. |

---

## Features

<table>
<tr>
<td width="50%" valign="top">

### The Notepad++ core
- Tabs, two views and clones, named sessions and session restore
- Encoding and line-ending detection like Notepad++, 40+ character sets
- Find and replace: normal, extended and Boost regular expressions, in files and folders, with Mark and bookmarks
- Macros, column editor, multi-editing, hide lines, folding
- Document Map, Function List, Folder as Workspace, Clipboard History
- User Defined Languages, Style Configurator and 30 themes, all in Notepad++ formats
- Run menu, Shortcut Mapper, auto-completion from Notepad++ API files, printing and PDF export
- 95 languages highlighted out of the box

</td>
<td width="50%" valign="top">

### Modern development tools
- **Language servers (LSP):** diagnostics, completion, hover, signature help, go to definition, references, rename, formatting, code actions, workspace symbols and inlay hints
- **Git:** changes in the margin, revert or stage a single block, a commit panel with the history of the repository or the file, blame and status in the file explorer
- **Integrated terminal** with tabs (libvterm)
- **Project tasks:** Make, CMake, npm/pnpm/yarn/bun, Cargo, Go, pytest, Meson and just, with clickable errors
- **Command palette**, go to file and go to symbol
- **Compare** two files, or a file with its saved or Git version
- **Local history** of every saved version
- **CSV, JSON and YAML:** colored CSV columns, align and sort by column; validate, format and convert JSON and YAML, and copy the path at the caret
- **TODO panel** for the document or the whole project, and a **filter** in Folder as Workspace
- **Secret scanner:** API keys, tokens and passwords are underlined as you type, and a commit that contains one asks first
- Snippets (VS Code format), regex tester, spell checking (Hunspell), external formatters, breadcrumbs, sticky scroll; panels remember where you put them, and F1 opens the manual section of the current dialog

</td>
</tr>
</table>

Edita is written in C++20 on **Qt 6**, with **Scintilla 5** and **Lexilla** as its editing engine. It includes a built-in [user manual](data/manual/en) (F1), and its interface is available in English and Spanish.

---

## Plugins: the Notepad++ ecosystem, on Linux

Plugins are what makes Notepad++ more than an editor, so Edita was designed around them.

**Why Windows plugins cannot just be loaded.** A Notepad++ plugin is a Windows DLL that calls the Win32 API. Linux cannot load it, so Edita does not claim binary compatibility. Edita offers **API compatibility**: the same exported functions, the same `NPPM_*` messages and `NPPN_*` notifications, and the same `FuncItem` menu entries. That way a plugin's source code can be built for Linux.

```mermaid
flowchart LR
    A["Notepad++ plugin<br/>source code"] --> B["Edita SDK<br/>+ Win32 compatibility layer"]
    B --> C["Linux .so<br/>(Edita plugin)"]
    C --> D["Signed catalog<br/>→ Plugins Admin"]
```

- **Plugin API.** [`edita_plugin.h`](plugin-sdk/edita_plugin.h) mirrors Notepad++'s plugin interface for Linux: UTF-8 strings, `NPPM_*`/`NPPN_*` with the same numbers, docking panels, inter-plugin messages and Scintilla access. It is **stable for the whole 1.x series**: a plugin built for 1.0 keeps working in any 1.x ([what is guaranteed](docs/plugins.md#qué-no-cambia-en-edita-1x)).
- **Porting toolkit.** A Win32 compatibility layer (windows, dialogs from `.rc` files, common controls, GDI drawing on Qt) and *recipes* that build a Notepad++ plugin from its original source with a few patches. `edita --build-recipe` and `edita --check-plugin` build and test them without a GUI.
- **Signed catalog.** [edita-plugins](https://github.com/alktrip/edita-plugins) builds the recipes in CI and publishes an Ed25519-signed list. **Plugins › Plugins Admin** installs from it in one click. It currently lists **22 Notepad++ plugins** (XML Tools, JSON Viewer, MIME Tools, Npp Converter, a SQL formatter, language lexers…) and **FTP and SFTP**, Edita's own take on NppFTP: browse servers over SFTP, FTP, FTPES or FTPS, open their files and upload them when you save. The plugins are prebuilt for x86_64 and arm64.
- **Bundled plugins.** **Python Script**, with the API of the Notepad++ plugin (many scripts run unchanged), **MarkdownViewer++**, a live Markdown preview, a **Hex Editor** that works on the bytes of any file, and **Secret Scanner**, Edita's own: it underlines API keys, tokens and passwords written in your files and warns before you commit one.

**Writing a plugin:** start with the [plugin guide](docs/plugins.md) and the [example plugin](plugins/EditaDemo). **Porting one:** add a recipe to [edita-plugins](https://github.com/alktrip/edita-plugins) with a pull request, and the catalog CI builds it, tests it and publishes it.

---

## Screenshots

<table>
<tr>
<td width="50%"><img src="docs/images/en/edita-dark.png" alt="Dark theme with Folder as Workspace and Function List"></td>
<td width="50%"><img src="docs/images/en/edita-palette.png" alt="Command palette searching for comment commands"></td>
</tr>
<tr>
<td align="center"><b>Dark theme</b> with the file explorer and the Function List</td>
<td align="center"><b>Command palette</b> (Ctrl+Shift+A): any command by name</td>
</tr>
<tr>
<td width="50%"><img src="docs/images/en/edita-compare.png" alt="Comparing a document with its saved version"></td>
<td width="50%"><img src="docs/images/en/edita-markdown.png" alt="Markdown preview with MarkdownViewer++"></td>
</tr>
<tr>
<td align="center"><b>Compare</b> with the saved version, with Git or with another file</td>
<td align="center"><b>Markdown preview</b> while you type (MarkdownViewer++)</td>
</tr>
<tr>
<td width="50%"><img src="docs/images/en/edita-git.png" alt="Git panel with a modified file and change marks in the margin"></td>
<td width="50%"><img src="docs/images/en/edita-tasks.png" alt="Task Output with a compiler error underlined"></td>
</tr>
<tr>
<td align="center"><b>Git</b>: changes in the margin, inline blame and the commit panel</td>
<td align="center"><b>Project tasks</b> (Ctrl+Shift+B): compiler errors you can double click</td>
</tr>
<tr>
<td width="50%"><img src="docs/images/en/edita-terminal.png" alt="Integrated terminal below the editor"></td>
<td width="50%"><img src="data/manual/en/images/localhistory.png" alt="Local History panel comparing an earlier version"></td>
</tr>
<tr>
<td align="center"><b>Integrated terminal</b> (Ctrl+`), with tabs</td>
<td align="center"><b>Local history</b>: every saved version, ready to compare or restore</td>
</tr>
</table>

<p align="center">
  <img src="docs/images/en/edita-plugins.png" alt="Plugins Admin listing the plugins of the signed catalog" width="85%"><br>
  <b>Plugins Admin</b> with the signed catalog of Notepad++ plugins built for Linux
</p>

---

## Install

Every [release](https://github.com/alktrip/edita/releases/latest) ships an AppImage, a `.deb` and an `.rpm` for **x86_64** and **arm64** (aarch64). `uname -m` tells which one your computer uses. All of them include their own Qt, Python Script and MarkdownViewer++.

| Distribution | Package | Install |
|---|---|---|
| **Debian, Ubuntu, Linux Mint** | `edita_<version>_amd64.deb` / `_arm64.deb` | `sudo apt install ./edita_*.deb` |
| **Fedora, openSUSE** and compatible | `edita-<version>-1.x86_64.rpm` / `.aarch64.rpm` | `sudo dnf install ./edita-*.rpm`<br>`sudo zypper install ./edita-*.rpm` |
| **Any distribution** | `Edita-<version>-x86_64.AppImage` / `-aarch64.AppImage` | `chmod +x Edita-*.AppImage && ./Edita-*.AppImage` |
| **Arch Linux** | [`packaging/aur/PKGBUILD`](packaging/aur/) (builds with Arch's Qt) | `makepkg -si` in that folder |
| **Flatpak** | coming to Flathub; meanwhile [`packaging/flatpak`](packaging/flatpak/) builds it | `packaging/flatpak/build.sh` |

**Requirements.** On x86_64: Ubuntu 22.04 or newer, Debian 12, Fedora 36 and later, openSUSE Tumbleweed, and in general any distribution with glibc 2.35 or newer. On arm64: Ubuntu 24.04, Debian 13, Raspberry Pi OS 13 (trixie), Fedora 40 or newer (glibc 2.39). Wayland and X11 are both supported.

The packages add Edita to the applications menu and to **Open With**. To make Edita the default for some file types, use **Settings › Preferences › File Association**. Edita tells you when a new version is available. The Arch `PKGBUILD` is not published on the AUR yet.

**Command line:**

```sh
edita [files…] [-n line] [-c column] [-l language] [--nosession] [--multiInst] [--openSession file]
```

Settings live in `~/.config/edita/`, with the same files as Notepad++ (`config.xml`, `langs.xml`, `stylers.xml`, `shortcuts.xml`, `session.xml`, `themes/`, `userDefineLangs/`, `plugins/`…), so you can bring your Notepad++ configuration with you.

---

## Project status

| | |
|---|---|
| **Current release** | The [latest release](https://github.com/alktrip/edita/releases/latest); its notes say what changed |
| **Next milestone** | See the [roadmap](ROADMAP.md), which also states when 1.0 is considered ready |
| **Platforms** | Linux x86_64 and arm64. macOS is paused until 1.0 for Linux is out |
| **Notepad++ parity** | Almost every Notepad++ command is implemented. What is still missing (right-to-left text and project panels) is listed in [docs/paridad-npp.md](docs/paridad-npp.md) |
| **Quality** | More than 80 automated test suites, run in CI on x86_64 and arm64; the packages are installed on Fedora and Debian in CI before every release |

**Release notes:** [GitHub Releases](https://github.com/alktrip/edita/releases) (English and Spanish) · **Development history:** [docs/historial.md](docs/historial.md), one entry per phase (Spanish).

---

## How Edita compares

Several projects bring Notepad++ or something close to it to Linux. Each one is good at something different:

| | Edita | Notepad++ | Nextpad++ | Notepad Next | CudaText | Kate | VS Code |
|---|---|---|---|---|---|---|---|
| **What it is** | Derived from Notepad++, rebuilt for Linux | The original | Port of Notepad++ | Reimplementation of Notepad++ | Independent editor | Independent editor | Independent editor |
| **Platforms** | Linux | Windows | Linux, macOS, iOS | Linux, Windows, macOS | Linux, Windows, macOS | Linux, Windows, macOS | Linux, Windows, macOS |
| **Toolkit** | Qt 6 | Win32 | GTK 4 (Linux), Cocoa (macOS) | Qt 6 | Lazarus / Free Pascal | Qt / KDE Frameworks | Electron |
| **Notepad++ plugins** | Ported from source with an API-compatible SDK; signed catalog | Native DLLs | Ported one by one; own catalog | — | — | — | — |
| **Extensions** | Notepad++ plugins, Python Script | Large plugin list | Its catalog of ported plugins | — | Python add-ons | KDE plugins | Very large marketplace |
| **License** | GPL-3.0 | GPL-3.0 | GPL-3.0 | GPL-3.0 | MPL-2.0 | LGPL | MIT (source code) |

- **Edita**: the Notepad++ workflow and its configuration files on Linux, Notepad++ plugins built from their source, and built-in developer tools (LSP, Git, terminal, tasks). Linux only.
- **Notepad++** is the reference on Windows. Edita follows its behavior and file formats on purpose.
- **Nextpad++** also brings Notepad++ to Linux, with a GTK 4 interface, and is the option if you also need it on macOS or iOS. It has its own catalog of ported plugins (29 for Linux, according to its site).
- **Notepad Next** reimplements Notepad++ on Qt and runs the same on Linux, Windows and macOS, which helps if you switch between systems. Its README still describes it as not ready for critical work.
- **CudaText** is a fast, independent editor with hundreds of lexers and a rich set of Python add-ons, if you do not need the Notepad++ way of working.
- **Kate** is an excellent native editor with deep KDE integration.
- **VS Code** has the largest extension marketplace and is the better choice if you depend on it.

This comparison was checked against each project's own pages in November 2026. If something here is out of date, [tell us](https://github.com/alktrip/edita/issues/new/choose).

---

## Contributing

Edita is an open source project, and it grows with the people who use it. The [contributing guide](CONTRIBUTING.md) explains each way to help.

- **Try it and report bugs.** The [testing guide](PROBAR.md#testing-edita) says what to test. **Help › Report a Problem…** opens a bug report with your version and system already filled in.
- **Propose features** in [Issues](https://github.com/alktrip/edita/issues/new/choose).
- **Port a Notepad++ plugin** with a recipe in [edita-plugins](https://github.com/alktrip/edita-plugins), or **write a new one** with the [SDK](docs/plugins.md).
- **Contribute code.** The [build instructions](#build-from-source) are below. Every change needs its tests (`ctest`), and the codebase builds with `-Werror` on GCC and Clang.

---

## Build from source

<details>
<summary><b>Build, test and package</b></summary>

On Arch Linux, from the folder that contains `editor/`:

```sh
sudo pacman -S --needed qt6-base qt6-5compat qt6-svg qt6-tools qt6-translations cmake ninja gcc openssl python libzip
cmake -S editor -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure      # -LE perf skips the performance test
./build/edita
```

Edita needs Qt 6.7 or newer, a C++20 compiler, CMake 3.25+ and Ninja. Hunspell, libzip, OpenSSL and the Python 3 headers enable spell checking, the Plugins Admin, the catalog signature and Python Script.

| CMake option | Default | |
|---|---|---|
| `EDITA_WERROR` | `ON` | Warnings in Edita's own code are errors |
| `EDITA_BUILD_TESTS` | `ON` | The tests (QtTest, headless) |
| `EDITA_BUILD_PLUGINS` | `ON` | The example plugin, Python Script (if the Python 3 headers are present) and MarkdownViewer++ |
| `EDITA_UPDATE_CHECK` | `ON` | New version notices; distribution packages turn it off |
| `EDITA_SOURCE_DATA` | `ON` | Also look for data in the source tree's `data/`, to run without installing |
| `EDITA_APP_ID` | `edita` | Name of the binary and of `~/.config/<id>/` |
| `EDITA_DESKTOP_ID` | `edita` | Name of the `.desktop` file and the icon (`io.github.alktrip.Edita` in Flatpak) |

**Packages:** `packaging/appimage/build-portable.sh` builds the AppImage, `.deb` and `.rpm` inside Ubuntu 22.04 (no Docker or root needed). The Linux CI builds them for x86_64 and arm64 on every push. `packaging/flatpak/build.sh` builds the Flatpak. See [docs/empaquetado.md](docs/empaquetado.md).

**Screenshots:** the images on this page and in the manual are taken from the real application by `edita_readme_images` and `edita_manual_images`.

</details>

<details>
<summary><b>Source layout</b></summary>

```
src/core/      core without widgets: encodings, line endings, file I/O, paths, local history, tasks
src/doc/       open documents on top of Scintilla (Notepad++'s FileManager/Buffer)
src/syntax/    langs.xml, stylers.xml and themes, language detection, lexers and styles
src/search/    search engine (FindReplaceDlg without UI) and find in files
src/lsp/       language server client
src/ui/        main window, editor (ScintillaEditBase), panels and dialogs
tests/         QtTest suites, headless (QT_QPA_PLATFORM=offscreen)
third_party/   Scintilla, Lexilla, boostregex, uchardet and pugixml from Notepad++; libvterm; md4c
data/          langs.model.xml, stylers.model.xml, themes/, autoCompletion/, userDefineLangs/, snippets/, manual/
translations/  edita_es.ts (Spanish) and edita_en.ts (English plurals)
plugin-sdk/    edita_plugin.h (the plugin API), compat/ and cmake/ (the Win32 layer for Notepad++ plugins)
plugins/       the example plugin (EditaDemo), Python Script and MarkdownViewer++
packaging/     appimage/, deb/, rpm/, aur/, flatpak/, site/ and release-notes/
docs/          design notes, one document per development phase, guides and images/
site/          the project website (GitHub Pages)
```

`third_party/` is regenerated with `tools/import-third-party.sh`; local patches are listed in [third_party/PATCHES.md](third_party/PATCHES.md).

</details>

---

## Documentation

- **[User manual](data/manual/en)**, also built into Edita (F1), in English and [Spanish](data/manual/es)
- **[Plugin guide and SDK](docs/plugins.md)** and the **[plugin catalog](https://github.com/alktrip/edita-plugins)**
- **[Packaging](docs/empaquetado.md)**: AppImage, `.deb`, `.rpm` and Flatpak, x86_64 and arm64
- **[Notepad++ parity](docs/paridad-npp.md)**: what Notepad++ has and Edita does not yet
- **[Roadmap](ROADMAP.md)** and **[contributing guide](CONTRIBUTING.md)**
- **[Development history](docs/historial.md)**: every phase, with its analysis of Notepad++
- **[Security](SECURITY.md)**: how to report a vulnerability

Most of the design documents are in Spanish; the manual, the release notes and this page are also in English.

---

<p align="center">
  <b>Edita</b> is free software, licensed under the <a href="COPYING">GNU GPL v3 or later</a>.<br>
  It is derived from <a href="https://notepad-plus-plus.org">Notepad++</a> (© Don Ho and the Notepad++ team),
  but it is an independent project, not affiliated with or endorsed by Don Ho or the Notepad++ project.<br>
  It uses <a href="https://www.scintilla.org">Scintilla and Lexilla</a> (© Neil Hodgson), Qt and other open source components,<br>
  credited in <a href="NOTICE">NOTICE</a>. Thank you to everyone who builds and maintains them.
</p>

<p align="center">
  <a href="https://github.com/alktrip/edita">Repository</a> &nbsp;·&nbsp;
  <a href="https://github.com/alktrip/edita/releases">Releases</a> &nbsp;·&nbsp;
  <a href="data/manual/en">Documentation</a> &nbsp;·&nbsp;
  <a href="https://github.com/alktrip/edita/issues">Issues</a>
</p>
