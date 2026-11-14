# Git

Cuando un archivo pertenece a un repositorio de Git, Edita muestra lo que cambió desde el último
commit, quién escribió cada línea y el estado de los archivos en el Explorador de carpetas. Solo
necesita tener `git` instalado; no hay que configurar nada.

## Cambios en el margen

Un margen estrecho junto al texto marca las líneas que difieren del último commit (HEAD):

| Marca | Significado |
|---|---|
| Barra verde | Líneas añadidas |
| Barra azul | Líneas modificadas |
| Triángulo rojo | Líneas borradas en ese punto |

La comparación se hace con el texto del editor, así que las marcas siguen a tus cambios antes de
guardar. Deja el ratón sobre una marca para ver las líneas originales. Las marcas se actualizan al
cambiar de documento, al guardar y al volver a la ventana (puede haber un commit nuevo hecho en
una terminal).

{{actionNextGitChange}} y {{actionPreviousGitChange}} van al bloque cambiado siguiente o anterior.
No tienen atajo; asígnales uno en {{actionShortcutMapper}} o usa la
[paleta de órdenes](05-views.md#paleta-de-ordenes).

Las marcas también aparecen en la barra de desplazamiento y en el Mapa del documento. Se desactivan
con **Cambios desde el último commit de Git** (página Márgenes de {{actionPreferences}}).

## Descartar o preparar un bloque

Haz clic en una marca del margen para abrir el menú de ese bloque:

- **Descartar este cambio** vuelve a poner las líneas del último commit en el documento. Solo cambia
  ese bloque, el archivo no se guarda y {{actionUndo}} lo deshace.
- **Preparar este cambio** pasa ese bloque al índice de Git (`git add` de una parte del archivo),
  para incluirlo en el próximo commit. Lo que ya estaba preparado se conserva. Si el documento tiene
  cambios sin guardar, Edita pregunta si guardarlo antes, porque lo que se prepara es lo del disco.
  En un archivo que Git todavía no sigue se prepara el archivo entero.
- **Comparar con Git (HEAD)**, **Cambio siguiente** y **Cambio anterior**.

Lo mismo, para el bloque de la línea del cursor, está en {{actionRevertGitChange}} y
{{actionStageGitChange}}. Los archivos UTF-16 solo se pueden preparar enteros.

## El panel Git

{{actionGitCommit}} muestra los archivos cambiados del repositorio del documento actual, con la rama.
Arriba van los **Preparados** y abajo los **Cambios** sin preparar:

- **Preparar**, **Quitar** y **Preparar todo** trabajan con los archivos elegidos. Doble clic en uno
  lo abre.
- Escribe el mensaje y pulsa **Confirmar** (o Ctrl+Intro) para hacer el commit de lo preparado.
  **Corregir el último commit** lo reemplaza (`git commit --amend`).
- **Actualizar** vuelve a leer el estado, aunque Edita ya lo hace al guardar y al volver a la ventana.

El commit usa tu configuración de Git (nombre, correo, *hooks*). Si Git da un error, Edita lo muestra
tal cual.

### Historial

La pestaña **Historial** del panel muestra los últimos 200 commits del repositorio, con su autor, su
fecha y el hash corto. Con **Solo el archivo actual**, los que cambiaron el documento, también con
sus nombres anteriores (`git log --follow`); la lista cambia al pasar a otro documento.

- Al elegir un commit, abajo aparecen los archivos que cambió: **A** añadido, **M** modificado,
  **D** borrado y **R** renombrado (con el nombre anterior).
- Doble clic en un archivo abre su diff en ese commit; doble clic en el commit, el diff entero, con
  el mensaje completo. Se abren en un documento nuevo de solo lectura, coloreado como Diff.

## Quién cambió cada línea (blame)

Al final de la línea del cursor, en gris, Edita muestra quién la cambió por última vez y cuándo:
`Ana Ruiz, hace 9 días • Lee con codificación`. Las líneas que cambiaste y no has confirmado dicen
**Tú, sin confirmar**. Se desactiva con **Quién cambió la línea actual (Git blame)** (página
Márgenes).

{{actionGitBlame}} muestra un margen con el commit, el autor y la fecha al principio de cada
bloque de líneas del mismo commit. El color va del de acento (menos de una semana) al gris (más de
dos años). Deja el ratón encima para ver el autor, el correo, la fecha, el mensaje y el commit.

{{actionShowLineCommit}} abre el commit de la línea actual (`git show`) en un documento nuevo de
solo lectura, con los cambios coloreados como un diff.

## Comparar con el último commit

{{actionCompareGit}} muestra el documento junto a su versión del último commit, con las
diferencias alineadas y resaltadas. Ver [Comparar documentos](10-compare.md).

## Git en el Explorador de carpetas

En un repositorio, el [Explorador de carpetas](05-views.md#explorador-de-carpetas) muestra la rama
(en la descripción emergente de la carpeta raíz) y colorea los archivos con una letra a la
derecha:

| Letra | Estado |
|---|---|
| M | Modificado |
| A | Añadido al índice |
| U | Sin seguimiento (nuevo) |
| R | Renombrado |
| D | Borrado |
| ! | En conflicto |

Las carpetas con cambios dentro llevan un punto, y los archivos ignorados se ven en gris. El estado
se vuelve a leer al guardar, al volver a la ventana y cuando cambia algo en una carpeta desplegada.
Se desactiva con **Estado de Git de los archivos en Carpeta como espacio de trabajo** (página
Márgenes).
