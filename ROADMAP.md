# Edita roadmap

This is the planned work, not a promise of dates. Priorities change with what testers report. Finished work is in the [release notes](https://github.com/alktrip/edita/releases) and, phase by phase, in [docs/historial.md](docs/historial.md).

## Now: 1.0

- **Release candidates.** The [latest release](https://github.com/alktrip/edita/releases/latest) is a release candidate for 1.0.
- **Feature freeze** since 28 November 2026: until 1.0, only bug fixes, documentation and packaging. New features wait for 1.1.
- **When 1.0 is ready.** 1.0.0 is released when all of these are true:
  1. no serious bug has been reported for 14 days on the latest release candidate (a crash, data loss, a file saved wrong, or a feature from the [testing guide](PROBAR.md) that does not work);
  2. at least two people outside the project have gone through the [testing guide](PROBAR.md) and reported the result in an issue;
  3. CI is green on x86_64 and arm64, with the sanitizers, and the packages install on Fedora and Debian.
- **Distribution.** The Flatpak package is ready ([packaging/flatpak](packaging/flatpak)) and will be submitted to Flathub with 1.0.
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
