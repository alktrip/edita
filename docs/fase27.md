# Fase 27 — Paleta de órdenes e «Ir a archivo»: resultado

Fecha: 2026-10-21

Objetivo: la primera de las mejoras que **no existen en Notepad++**. Un buscador para ejecutar cualquier orden sin buscarla en los menús, abrir archivos por su nombre y saltar a una función o a una línea.

## Qué se construyó

| Parte | Qué hace |
|---|---|
| `ui/CommandPalette` | Un buscador flotante, centrado bajo la barra de herramientas, con la lista de resultados. Según el primer carácter: nada = **archivos**, `>` = **órdenes**, `@` = **símbolos** del documento, `:` = **línea** (`:120` o `:120:5`). Las letras encontradas se resaltan en negrita y en el color de acento; a la derecha van el detalle (menú o carpeta) y el atajo. ↑/↓ (con vuelta), RePág/AvPág, Intro ejecuta y Esc cierra; también se cierra al pasar el foco a otra parte |
| `core/FuzzyMatch` | La búsqueda difusa. Cada palabra tiene que aparecer letra a letra y en orden, sin importar mayúsculas ni acentos («codificacion» encuentra «Codificación»). Las palabras van en cualquier orden («lin ord» encuentra «Ordenar líneas»). Puntúan más el principio del texto, los inicios de palabra (tras espacio, `_`, `-`, `.`, `/` o una «joroba» camelCase) y las letras seguidas. Si no coincide con el nombre, se busca también en el detalle, con menos peso, para que «ver ajuste» encuentre Ver › Ajuste de línea |
| Órdenes (`>`) | Todo lo de los menús que se puede ejecutar en ese momento: las órdenes, los plugins, las macros, los lenguajes y los temas, con su ruta de menú («Editar › Convertir»), su atajo y una ✓ si están activas. Las usadas hace poco salen primero (las 30 últimas, en `config.xml`: `<GUIConfig name="EditaCommandPalette" history="…">`) |
| Archivos | Los abiertos (el actual al final, para que Intro cambie al anterior), los recientes y los de las carpetas del Explorador de carpetas. Sin carpeta de trabajo, la carpeta del documento, salvo la carpeta personal o la raíz enteras. El recorrido (`core/FileScan`) va en otro hilo y entrega los archivos por tandas mientras se escribe. No entra en carpetas ocultas (`.git`…), `node_modules` ni `__pycache__`, no sigue enlaces, llega hasta 50 000 archivos y no repite una carpeta que está dentro de otra |
| Símbolos (`@`) | Las funciones y clases del documento, de los analizadores de la Lista de funciones (fase 22); sin búsqueda, en el orden del documento |
| Menús y atajos | Ver › **Paleta de órdenes…** (Ctrl+Mayús+A), Buscar › **Ir a archivo…** (Ctrl+E) e **Ir a símbolo…** (Ctrl+Mayús+O). Ctrl+P y Ctrl+Mayús+P, los de otros editores, son Imprimir y Reproducir macro en N++. Los tres se pueden cambiar en el Shortcut Mapper |

## Divergencias

No es de N++: no hay nada que comparar. N++ tiene Ir a (Ctrl+G) y la Lista de funciones, que siguen igual.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Búsqueda difusa: palabras en cualquier orden, sin acentos ni mayúsculas, letras en orden, posiciones para resaltar, mejor puntuación para inicios de palabra, camelCase y letras seguidas | ✅ `tst_commandpalette`: `fuzzyMatching` |
| El recorrido de carpetas salta las ocultas, `node_modules` y `__pycache__`, no repite carpetas anidadas, respeta el máximo y se puede cancelar | ✅ `scanningSkipsHiddenFoldersAndHasALimit` |
| `>upper` → UPPERCASE convierte la selección; la orden usada sale la primera después, también en otra ventana; «word wrap» activa el ajuste de línea; Esc cierra | ✅ `commandsAreRunFromThePalette` |
| `@res` salta a la función; `:3:5` va a la línea 3, columna 5; `:abc` no hace nada | ✅ `symbolsAndLines` |
| Los archivos de la carpeta de trabajo llegan desde otro hilo, sin `.git`; un archivo abierto sale una sola vez; «mwin» encuentra MainWindow.cpp y lo abre; las flechas mueven la selección | ✅ `filesFromTheWorkspaceAndRecentFiles` |

Se revisaron capturas en claro y en oscuro (órdenes, archivos, símbolos). Al revisarlas se vio que «iNVERTIR mAYÚSCULAS» salía antes que «MAYÚSCULAS» al buscar «may». La bonificación de camelCase se limitó a una mayúscula seguida de minúscula. Pasan las 32 suites y el build no da ningún warning.
