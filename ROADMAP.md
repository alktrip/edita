# Edita roadmap

This is the planned work, not a promise of dates. Priorities change with what testers report. Finished work is in the [release notes](https://github.com/alktrip/edita/releases) and, phase by phase, in [docs/historial.md](docs/historial.md).

## Now: 1.0

- **Release candidates.** [1.0.0-rc.2](https://github.com/alktrip/edita/releases/latest) is out. If testers find nothing serious, 1.0.0 is the same version with a new number; otherwise there will be another candidate.
- **Plugins on arm64.** The catalog has prebuilt plugins only for x86_64. On arm64 they are built from their recipe, and that still needs testing.

## Next: 1.1

Planned features, none of them in Notepad++ yet:

- Remember the position and size of panels between sessions
- A history tab (`git log`) in the Git panel
- A filter for Folder as Workspace
- Contextual help (F1 opens the manual chapter of the current dialog or panel)
- A TODO panel for the project (`TODO`, `FIXME`…)
- Tools for CSV, JSON and YAML: aligned and colored columns, sorting by column, validation, formatting, the path of the current node, conversion between JSON and YAML
- More interface languages, from the Notepad++ translation files
- Run a macro on all the files of a folder
- A fuzzy (typo-tolerant) mode in the Find dialog
- Multi-line tab bar

## Notepad++ parity

Almost every Notepad++ command is in Edita. What is still missing is listed in [docs/paridad-npp.md](docs/paridad-npp.md):

- Project panels (Notepad++ `.workspace` files)
- Right-to-left text direction

## Later

- **macOS**, once 1.0 for Linux is out.
- **AUR package**, when AUR registration reopens (the `PKGBUILD` is ready in [packaging/aur](packaging/aur)).
- More Notepad++ plugins in the [catalog](https://github.com/alktrip/edita-plugins).

Ideas and requests are welcome in [Issues](https://github.com/alktrip/edita/issues/new/choose).
