# Fase 34 — Probador de expresiones regulares: resultado

Fecha: 2026-10-26

Una mejora que **no existe en Notepad++**: un panel para escribir una expresión regular y ver en vivo qué encuentra, con sus grupos, antes de usarla en el documento.

## Qué se construyó

| Parte | Qué hace |
|---|---|
| **Panel** (`ui/RegexTester`) | Buscar › «Probador de regex» lo abre abajo; se puede mover como los demás paneles y recuerda si estaba abierto. Tiene la expresión con «Aa» (distinguir mayúsculas) y «.⏎» (el punto incluye el salto de línea), un texto de prueba editable, la lista de coincidencias y la vista previa del reemplazo. Se evalúa 150 ms después de cada cambio en la expresión, el reemplazo o el texto |
| Texto de prueba | Un editor Scintilla con los colores del tema y números de línea. Cada coincidencia se marca con un relleno que alterna entre dos colores para distinguir las que van seguidas, y el texto de cada grupo toma su color (seis colores que se repiten, distintos en claro y en oscuro). La primera vez se llena con la selección o, sin selección, con el documento (hasta 1 MB); «Del editor» lo vuelve a tomar. Si el diálogo Buscar estaba en modo regex, su expresión pasa al probador |
| Coincidencias | Cada una muestra su texto y su posición (línea y columna). Debajo van sus grupos, con su número, su nombre (`(?<mes>…)`, `(?'mes'…)`, `(?P<mes>…)`), su color y su texto, o «(no participó)». Elegir uno lo selecciona en el texto de prueba. El estado dice «3 coincidencias · 4 grupos», «Sin coincidencias» o «Expresión regular no válida: …» (con el mensaje de Boost). Se listan y resaltan las primeras 1000, pero se cuentan todas |
| Vista previa | Al escribir un reemplazo (`$1`, `$+{mes}`, `$&`, `\U…\E`, `\n`…) se muestra el texto de prueba ya reemplazado, sin tocar el original |
| Referencia rápida | El botón «?» lista los elementos más usados de la expresión y del reemplazo con su explicación; elegir uno lo escribe. Si hay texto seleccionado, los grupos lo envuelven |
| «Usar en Buscar…» | Abre el diálogo Buscar en modo regex con la expresión y las opciones, en la pestaña Reemplazar si hay reemplazo |
| Mismo motor | Busca con `processRange` y la regex Boost de Scintilla, igual que el diálogo y la barra de búsqueda: lo que coincide en el probador coincide en el documento. Para saber dónde cae cada grupo, `BoostRegExSearch.cxx` ahora guarda las posiciones de los grupos de la última coincidencia (`g_lastMatchGroups`, por hilo), y `SearchTarget::lastMatchGroups()` las entrega |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Los nombres de los grupos salen bien con grupos sin captura, lookbehind, paréntesis escapados, clases y `\Q…\E` | ✅ `tst_regextester`: `groupNames` |
| El motor da la posición de cada grupo y −1 para el que no participó | ✅ `groupPositionsFromTheEngine` |
| Fechas con tres grupos (uno con nombre): 2 coincidencias, resaltado alterno, color por grupo, lista con grupos, selección al elegir, vista previa `$3/$+{mes}/$1`, se reevalúa al editar, expresión inválida en rojo y sin resaltado | ✅ `matchesGroupsAndPreview` |
| «Aa», «.⏎» y la referencia rápida (envuelve la selección) | ✅ `toggles` |
| Desde la ventana: toma la selección, «Del editor» toma todo el documento y «Usar en Buscar» abre el diálogo en modo regex | ✅ `fromTheMainWindow` |

Se revisaron capturas en claro y en oscuro. En ellas el subrayado de los grupos casi no se veía, así que se cambió por el color del texto. También se quitó la marca del historial de cambios del texto de prueba. Pasan las 39 suites y el build no da ningún warning.
