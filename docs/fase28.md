# Fase 28 — Git en el margen y marcas en la barra de desplazamiento: resultado

Fecha: 2026-10-21

Objetivo: la segunda mejora que **no existe en Notepad++**. Ver qué cambió respecto al último commit de Git y, en la barra de desplazamiento, dónde está en todo el documento lo que interesa. El historial de cambios de N++ solo cuenta desde que se abre el archivo.

## Qué se construyó

| Parte | Qué hace |
|---|---|
| `core/LineDiff` | Diferencias por líneas con el algoritmo de Myers (O(ND)). Antes descarta lo común del principio y del final, y numera las líneas para comparar enteros. Agrupa las ediciones seguidas: solo inserciones = **añadidas**; solo borrados = **borradas** (marcadas en la línea siguiente); las dos cosas = **modificadas**. Los fines de línea (`\n`, `\r\n`, `\r`) no cuentan como cambio. Con más de 2000 líneas distintas, lo del medio sale como un solo cambio. La reconstrucción del camino guarda solo lo necesario de cada paso (memoria proporcional a D², no a D·(N+M)) |
| `ui/GitChanges` | Uno por vista. Pide la versión de HEAD con `git -C <carpeta> cat-file blob HEAD:./<archivo>`, en segundo plano. No hace falta saber dónde está la raíz del repositorio, y sin repositorio, sin commits o con el archivo fuera de HEAD, git falla y no hay marcas. Sin git instalado, no se vuelve a intentar. La guarda por archivo (compartida por las dos vistas) y la decodifica con la codificación del documento (UTF-8, UTF-16, Windows-1252…), sin BOM. Compara con el **texto en memoria**, con una pausa de 250 ms tras cada cambio, así que las marcas cambian mientras se escribe. Hasta 20 MB |
| Margen | El margen 4, de 5 px junto al texto. Solo aparece con archivos que están en un repositorio. Verde = añadida, azul = modificada, triángulo rojo = líneas borradas (los colores de GitHub, más claros en modo oscuro). Usa los marcadores 16, 17 y 0: los de los plugins (1–15) y los de N++ (18–20) quedan libres. El fondo es el de los números de línea |
| Aviso | Al pasar el ratón por una marca: «Modificado desde el último commit. Antes:» o «Borrado…», con las líneas originales (hasta 20); en las añadidas, cuántas son |
| Cuándo se actualiza | Al cambiar de documento en una vista, al guardar con otro nombre o con otra codificación y al **volver a la ventana**, porque puede haber un commit nuevo hecho en una terminal |
| Navegación | Buscar › Historial de cambios › **Cambio de Git siguiente / anterior** (dando la vuelta). Sin atajo, pero se pueden asignar en el Shortcut Mapper o usar desde la paleta de órdenes |
| `ui/ScrollBarMarks` | Una capa transparente encima de la barra de desplazamiento vertical, que no recibe el ratón: la barra funciona igual. Tres carriles: a la izquierda Git (sus colores), en el centro lo marcado con «Marcar todo» y el resaltado inteligente (el color de su estilo) y a la derecha los marcadores. Además, una raya a lo ancho donde está el cursor. Las posiciones cuentan con el plegado y el ajuste de línea (líneas visibles, como la barra). Se recalcula poco después de cada cambio o movimiento del cursor; hasta 4000 marcas |
| Preferencias | Márgenes › «Cambios desde el último commit de Git» y «Marcas en la barra de desplazamiento». Activadas de serie; en `config.xml`, `EditaGitMargin` y `EditaScrollBarMarks` |

## Divergencias

No es de N++. Su historial de cambios (margen naranja y verde) sigue igual, al lado.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Diferencias: añadidas, modificadas y borradas (también al principio y al final), varios cambios separados, fines de línea distintos sin cambios, límite de ediciones | ✅ `tst_gitchanges`: `lineDiff` |
| Con un repositorio real: sin cambios al abrir; las marcas siguen al texto mientras se escribe (modificadas, añadidas, borradas); el aviso muestra la línea original; siguiente/anterior cambio dan la vuelta | ✅ `marksFollowTheRepository` |
| La barra lleva Git, un marcador, «Marcar todo» y el cursor en su carril | ✅ |
| Tras un commit nuevo, las marcas desaparecen; desactivado en Preferencias no hay margen ni marcas; un archivo sin commit no tiene margen | ✅ |
| Un archivo en Windows-1252 en el repositorio no da cambios falsos | ✅ `otherEncodings` |

Se revisaron capturas en claro y en oscuro. En ellas se vio que el fondo del margen y los colores se calculaban antes de aplicar el tema: el margen salía gris en claro y blanco en oscuro. Ahora se aplican después de que la vista se vuelve a pintar con el tema y al cambiar de documento. Pasan las 33 suites y el build no da ningún warning.
