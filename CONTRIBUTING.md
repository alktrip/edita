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
3. The code builds with `-Werror` (`EDITA_WERROR=ON`) on GCC and Clang. CI also builds with GCC 12 and Qt 6.8 on Ubuntu 22.04, and on arm64.
4. When a feature exists in Notepad++, study how Notepad++ does it first (its source is the reference) and keep its file formats and command IDs.
5. New user-visible text goes through `tr()`, with its Spanish translation in `translations/edita_es.ts`. New commands are documented in the [user manual](data/manual/en), in English and Spanish: `tst_manual` checks that every menu command is in it.
6. Keep pull requests focused on one change, and explain the why in the description.

By contributing, you agree that your contribution is licensed under the [GPL v3 or later](COPYING), like the rest of Edita.
