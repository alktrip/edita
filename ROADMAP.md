# Edita roadmap

This is the planned work, not a promise of dates. Priorities change with what testers report. Finished work is in the [release notes](https://github.com/alktrip/edita/releases) and, phase by phase, in [docs/historial.md](docs/historial.md).

## Now: 1.0

- **Release candidates.** [1.0.0-rc.4](https://github.com/alktrip/edita/releases/latest) is out. If testers find nothing serious, 1.0.0 is the same version with a new number; otherwise there will be another candidate.
- **Brought forward from 1.1** (in rc.4): panels remember their position and size, F1 opens the manual section of the current dialog or panel, a TODO panel, a filter for Folder as Workspace, a history tab in the Git panel, and tools for CSV, JSON and YAML.
- **Plugins on arm64.** Done: the catalog builds every plugin for arm64 too, and Edita on arm64 installs them prebuilt (from rc.4 on).
- **Memory checks in CI.** Done: every test also runs with AddressSanitizer and UndefinedBehaviorSanitizer, and packages are not published if they fail.

## Next: 1.1

Planned features, none of them in Notepad++ yet:

- More interface languages, from the Notepad++ translation files
- Run a macro on all the files of a folder
- A fuzzy (typo-tolerant) mode in the Find dialog
- Multi-line tab bar
- Accessibility review (screen readers, keyboard only, contrast)

## Notepad++ parity

Almost every Notepad++ command is in Edita. What is still missing is listed in [docs/paridad-npp.md](docs/paridad-npp.md):

- Project panels (Notepad++ `.workspace` files)
- Right-to-left text direction

## Later

- **macOS**, once 1.0 for Linux is out.
- **AUR package**, when AUR registration reopens (the `PKGBUILD` is ready in [packaging/aur](packaging/aur)).
- More Notepad++ plugins in the [catalog](https://github.com/alktrip/edita-plugins).

Ideas and requests are welcome in [Issues](https://github.com/alktrip/edita/issues/new/choose).
