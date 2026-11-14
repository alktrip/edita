# Fase 98 — Historial en el panel Git

Fecha: 2026-11-13

## Por qué

El panel Git (fase 85) prepara y confirma cambios, pero para ver qué pasó antes había que salir a la terminal: `git log`, `git show`. Ver el historial del archivo que estás editando, con sus diffs, es de lo más común al trabajar con Git.

## Cómo lo hace Notepad++

Notepad++ no tiene Git integrado; existen plugins aparte. Esto es propio de Edita.

## Qué se hizo

- **El panel Git, con dos pestañas** (`gitTabs`): **Cambios** (lo de la fase 85) e **Historial**.
- **El historial:**
  - **Los commits:** los últimos 200 (`git log -n 200 --date=short`), con asunto, autor, fecha y hash corto.
  - **Solo el archivo actual:** con **Solo el archivo actual**, `--follow -- <archivo>`, que incluye sus nombres anteriores, y sigue al documento activo.
  - **Al elegir un commit,** abajo aparecen sus archivos: `git diff --name-status -z -M` contra el primer padre; en el primer commit, `git diff-tree --root`. Los renombrados se ven como «viejo → nuevo».
  - **Doble clic en un archivo** abre `git show --format=fuller -M --stat -p <commit> -- <archivo>` en un documento nuevo, de solo lectura, sin cambios pendientes y coloreado como Diff, con el commit en el nombre. **Doble clic en el commit** abre el commit entero.
- **`parseLog` y `parseNameStatus`**, sin interfaz:
  - **El formato del log** separa los campos con 0x1f y los commits con 0x1e. Los campos se cuentan desde los dos extremos, así que un asunto con un 0x1f no rompe la lectura.
  - **`--name-status -z`** se lee con los renombrados y copiados de tres campos.
- **Desde un diff abierto** (un documento sin repositorio), el historial recuerda en qué repositorio se leyó.
- **Traducción y manual:** la traducción y el manual (Git › Historial), en los dos idiomas.

## Pruebas

`tst_githistory` (5), con un repositorio de tres commits (crear, añadir y cambiar más renombrar):

- **`parseLog` y `parseNameStatus`**, también con un asunto con 0x1f, un commit sin padres y un renombrado.
- **El historial completo** y el del archivo actual, con autor, fecha y hash.
- **Los archivos de un commit**, con el renombrado, y los del primero.
- **El diff de un archivo y el del commit:** de solo lectura, sin cambios, Diff, con el commit en el nombre, y con el contenido justo.
- **Fuera de un repositorio:** el historial vacío.
