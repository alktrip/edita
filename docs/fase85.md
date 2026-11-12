# Fase 85 — Git desde el margen y el panel Git

Fecha: 2026-11-12

## Por qué

Desde la fase 28 el margen muestra qué cambió respecto al último commit, pero para deshacer un bloque o prepararlo para el commit había que ir a una terminal. Con esta fase el ciclo completo (ver, descartar, preparar y confirmar) se hace sin salir del editor.

## Cómo lo hace Notepad++

N++ no tiene nada de Git. Su plugin GitSCM abre TortoiseGit o la consola, solo en Windows. Edita ya mostraba los cambios en el margen y la barra de desplazamiento (28), Git en el explorador y blame (40) y comparar con HEAD (39).

## Qué se hizo

- **`core/GitHunks`** (funciones puras):
  - `applyHunk(viejo, nuevo, bloque)` aplica un solo bloque, conservando los fines de línea de cada texto (CRLF incluido) y el caso de la última línea sin fin de línea.
  - `inverted` sirve para deshacer un bloque, `hunkAt` encuentra el bloque de una línea (un borrado se toca desde la línea de antes o de después) y `changedSpan` da el tramo mínimo que cambia entre dos textos, sin partir caracteres UTF-8.
- **Descartar** (`revertGitHunk`): el documento con ese bloque como en HEAD, aplicado con un solo `SCI_REPLACETARGET` sobre el tramo que cambia. Es un paso de deshacer y los marcadores y el plegado del resto no se tocan. No guarda.
- **Preparar** (`stageGitHunk`):
  - El bloque se busca contra el **índice** (`git show :ruta`) y no contra HEAD. Así un segundo bloque no borra lo que ya estaba preparado.
  - El contenido nuevo se escribe con `git hash-object -w --stdin` y `git update-index --cacheinfo`, con el modo del archivo. Se conservan la codificación del documento y el BOM.
  - Si hay cambios sin guardar, pregunta si guardar, porque lo preparado es lo del disco. Un archivo sin seguimiento se prepara entero (`git add`), y los UTF-16 no se pueden preparar por bloques.
- **El menú del bloque**: un clic en una marca del margen de Git (`GitChanges::hunkMenuRequested`) abre Descartar este cambio, Preparar este cambio, Comparar con Git (HEAD) y Cambio siguiente/anterior. Para la línea del cursor están también en Buscar › Historial de cambios: `actionRevertGitChange` y `actionStageGitChange`.
- **Ver › Git** (`GitCommitPanel`, a la izquierda):
  - El repositorio y la rama, y los archivos en dos grupos, Preparados y Cambios (`git status --porcelain=v1 -z`, con renombrados).
  - Preparar, Quitar (`git reset -q HEAD --`), Preparar todo y Actualizar. Doble clic abre el archivo.
  - El mensaje, Corregir el último commit (`--amend`) y Confirmar (Ctrl+Intro, `git commit -F -`). Confirmar solo se activa con mensaje y algo preparado.
  - Después de cada operación, `reloadGitDecorations` vuelve a leer las marcas, el blame, el explorador y el panel. El panel se actualiza con `updatePanels`, como mucho cada 250 ms.
- **Manual**: Git › Descartar o preparar un bloque y El panel Git, en los dos idiomas.

## Pruebas

`tst_githunks` (15):

- **Las funciones puras:** cada tipo de bloque, deshacer, fin de archivo sin salto, CRLF, `hunkAt`, `changedSpan` con UTF-8 y `parseStatus`.
- **Con un repositorio real** (configuración de git aislada):
  - Descartar un bloque y deshacerlo.
  - Preparar dos bloques uno a uno; `git diff --cached` tiene los dos y `git diff`, ninguno.
  - Preparar con cambios sin guardar pregunta y guarda.
  - Desde el panel: Preparar todo, Quitar uno y Confirmar; se comprueban el mensaje en `git log` y los grupos.
  - Fuera de un repositorio no hace nada.
