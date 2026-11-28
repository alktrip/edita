# Contributing to Edita

Thank you for helping Edita grow. There are many ways to contribute, and most of them need no C++.

*También puedes escribir en español: los issues, los pull requests y la documentación de diseño del proyecto están en español.*

## Test and report

- Follow the [testing guide](PROBAR.md#testing-edita) and use Edita for real work.
- **Help › Report a Problem…** opens a [bug report](https://github.com/alktrip/edita/issues/new/choose) with your Edita version and system already filled in. Include the steps to reproduce it and, if Edita crashed, the report it offers when you open it again.
- Problems with a plugin from the catalog have their own form.

## Propose a feature

Open an [issue](https://github.com/alktrip/edita/issues/new/choose) that describes the problem the feature solves. If it is something Notepad++ does, say how Notepad++ does it: Edita follows its behavior and file formats on purpose. The planned work is in [ROADMAP.md](ROADMAP.md).

## Port or write a plugin

- **Port a Notepad++ plugin.** Add a recipe to [edita-plugins](https://github.com/alktrip/edita-plugins) with a pull request: its license, where its source is, the patches it needs and how to check it. The catalog CI builds it, tests it and publishes it, signed. The [plugin guide](docs/plugins.md#compilar-un-plugin-de-notepad-desde-su-código-recetas) explains recipes and the Win32 compatibility layer.
- **Write a new plugin** with the [SDK](docs/plugins.md) (`plugin-sdk/edita_plugin.h`), starting from the [example plugin](plugins/EditaDemo). The API is stable for the whole 1.x series.

## Contribute code

1. Build Edita and run the tests ([build instructions](README.md#build-from-source)).
2. Every change comes with its tests in `tests/` (QtTest, headless). A bug fix starts with a test that fails.
3. The code builds with `-Werror` (`EDITA_WERROR=ON`) on GCC and Clang. CI also builds with GCC 12 and Qt 6.8 on Ubuntu 22.04, and on arm64, and runs every test with AddressSanitizer and UndefinedBehaviorSanitizer (`-DEDITA_SANITIZE=address,undefined`, see [docs/fase93.md](docs/fase93.md)).
4. When a feature exists in Notepad++, study how Notepad++ does it first (its source is the reference) and keep its file formats and command IDs.
5. New user-visible text goes through `tr()`, with its Spanish translation in `translations/edita_es.ts`. New commands are documented in the [user manual](data/manual/en), in English and Spanish: `tst_manual` checks that every menu command is in it.
6. Keep pull requests focused on one change, and explain the why in the description.
7. Commit messages follow [Conventional Commits 1.0.0](https://www.conventionalcommits.org/en/v1.0.0/): `<type>(<optional scope>): <description>`, then an optional body and footers.
   - **Types:** `feat` (new feature), `fix` (bug fix), `docs`, `test`, `refactor`, `perf`, `build` (CMake, packaging), `ci`, `chore`, `style`, `revert`. Lowercase, in English.
   - **Scope:** the area touched, in lowercase: `ftp`, `hex`, `git`, `plugins`, `sdk`, `catalog`, `manual`, `i18n`, `release`…
   - **Description:** short, imperative, without a final period. It may be in English or Spanish; the project's own commits use Spanish.
   - **Breaking changes** (for example in the [plugin API](docs/plugins.md#qué-no-cambia-en-edita-1x)): `!` before the colon (`feat(sdk)!: …`) and a `BREAKING CHANGE:` footer.
   - **Footers** go after a blank line, as git trailers: `Refs: #123`, `Co-Authored-By: …`.

   ```
   feat(ftp): panel de FTP y SFTP con subida al guardar

   Lo que hace NppFTP en Notepad++, escrito desde cero (fase 90).

   Refs: #42
   ```

## How Edita is developed

Edita is written with an AI coding assistant, Claude (by Anthropic). Every commit made that way says so in a `Co-Authored-By: Claude …` trailer, so the history shows it openly. The maintainer sets the direction and the scope of each phase, decides what goes into a release, and uses Edita every day; problems found that way are fixed like any other report. The assistant does most of the writing: the analysis of how Notepad++ does something, the code, the tests and the documentation.

Because a lot of code is produced quickly, the project relies on checks that do not depend on anyone's attention span:

- **Tests first.** Every change comes with its tests, and a bug fix starts with a test that fails without the fix. More than 80 QtTest suites run headless.
- **Three compilers.** Every phase is built and tested with GCC and Clang (with `-Werror`) and with GCC 12 and Qt 6.8 on Ubuntu 22.04, the oldest supported system.
- **Sanitizers.** CI runs every test with AddressSanitizer and UndefinedBehaviorSanitizer, and does not publish packages if one fails.
- **Real packages.** CI builds the AppImage, `.deb` and `.rpm` for x86_64 and arm64 and installs them on Fedora and Debian; the Flatpak is built and checked with the Flathub linter.
- **A written record.** Each phase has a document in [docs/](docs/historial.md) with what Notepad++ does, what was decided and why, and how it was tested.
- **Code review passes.** Besides the tests, the code is periodically reviewed for bugs as a whole (for example, [phase 101](docs/fase101.md)).

Contributions written with the help of AI tools are welcome on the same terms as any other: you are responsible for what you submit, it must come with its tests and pass CI, and you should say so in the pull request or with a `Co-Authored-By` trailer.

## Security

Please do not report vulnerabilities in public issues. [SECURITY.md](SECURITY.md) explains how to report them privately.

By contributing, you agree that your contribution is licensed under the [GPL v3 or later](COPYING), like the rest of Edita.
