# Fase 40 — Git en el explorador y Git blame: resultado

Fecha: 2026-10-29

Dos mejoras que **no existen en Notepad++**: el estado de Git de los archivos en el explorador de carpetas y quién cambió cada línea (blame).

## Qué se construyó

| Parte | Qué hace |
|---|---|
| **Estado de Git** (`ui/GitStatus`) | Para cada carpeta de trabajo, `git rev-parse --show-toplevel --abbrev-ref HEAD` y `git status --porcelain=v1 -z --untracked-files=all --ignored=matching`, en segundo plano. Los estados son: modificado, añadido al índice, sin seguimiento, renombrado, borrado, en conflicto e ignorado (también lo que está dentro de una carpeta ignorada). Cada carpeta toma el estado más importante de lo que contiene. Una consulta nueva descarta la que estuviera en marcha |
| **Explorador** | Cada archivo con estado lleva el nombre en color (los de VS Code, para tema claro y oscuro) y una letra a la derecha: M, A, U, R, D o !. Las carpetas con cambios llevan «•» y lo ignorado sale en gris. La descripción emergente dice el estado, y la de la carpeta raíz, la rama. Se vuelve a leer al agregar una carpeta, al guardar, al volver a la ventana y cuando cambia algo en una carpeta desplegada (400 ms después) |
| **Blame** (`ui/GitBlame`) | `git blame --porcelain --contents - -- archivo`, con el texto del editor por la entrada estándar (en la codificación del archivo), así que las líneas cuadran aunque haya cambios sin guardar: esas salen como «Tú, sin confirmar». Se repite 1,5 s después de editar, al guardar y al volver a la ventana. Archivos de más de 5 MB: no |
| En la línea del cursor | Al final de la línea, en gris y cursiva: «Beto Ruiz, hace 9 días • Lee con codificación» (una anotación de fin de línea de Scintilla, solo en la línea del cursor) |
| **Margen de blame** | Buscar › Historial de cambios › «Mostrar Git blame»: un margen de texto (el 6.º, entre las marcas de Git y el texto) con hash, autor y fecha en la primera línea de cada bloque del mismo commit. El color va del de acento (menos de una semana) al gris (más de dos años), en cinco tramos. Al pasar el ratón: autor, correo, fecha, mensaje y hash |
| «Mostrar el commit de la línea actual» | `git show` de ese commit en un documento nuevo de solo lectura, con el resaltado de diff y con el hash corto como nombre |
| Preferencias | Márgenes › «Estado de Git de los archivos en Carpeta como espacio de trabajo» (`EditaGitExplorer`) y «Quién cambió la línea actual (Git blame), al final de la línea» (`EditaGitBlameInline`), activadas de serie |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| `git status --porcelain -z` (modificado, sin seguimiento, renombrado con su ruta anterior, conflicto, ignorado, añadido) y `git blame --porcelain` (commit repetido, sin confirmar); tiempos relativos | ✅ `tst_gitexplorer`: `parsing` |
| Un repositorio real con todos los estados: archivos, carpetas (el más importante dentro), ignorados por carpeta y por patrón, rama, carpeta que no es repositorio | ✅ `statusOfTheRepository` |
| El explorador: «•» en la raíz con la rama en la descripción, M y A con color, sin letra lo que no cambió; al guardar, el archivo pasa a M | ✅ `explorerShowsTheStatus` |
| Blame de dos autores; al final solo de la línea del cursor; una línea sin guardar «sin confirmar» y las demás desplazadas; margen con hash y fecha; el commit en un documento de solo lectura | ✅ `blame` |

La prueba de blame encontró un error: «Mostrar el commit» usaba los datos del commit después de abrir el documento nuevo, y para entonces el blame de esa vista ya se había vaciado. Ahora copia el hash antes. Se revisaron capturas en claro y en oscuro. Pasan las 43 suites y el build no da ningún warning.
