# Fase 39 — Comparar documentos: resultado

Fecha: 2026-10-29

Comparar dos documentos lado a lado, como el plugin Compare de Notepad++ (en N++ es un plugin aparte; en Edita viene incluido).

## Qué se construyó

| Parte | Qué hace |
|---|---|
| **Órdenes** (Ver › Comparar) | «Comparar con la otra vista» (Ctrl+Alt+C), «con la última versión guardada» (Ctrl+Alt+D) y «con Git (HEAD)» (Ctrl+Alt+H); diferencia siguiente y anterior; «Ignorar espacios» e «Ignorar mayúsculas»; «Quitar la comparación» (Ctrl+Alt+X). Mientras se compara, Ctrl+Alt+↓/↑ (ir al cambio siguiente/anterior) recorren las diferencias |
| Qué se compara | El documento de la vista izquierda (la referencia) con el de la derecha. Con una sola vista y varias pestañas, el documento actual pasa a la otra vista, como hace el plugin. La versión guardada (leída del disco con la codificación del documento) y la de HEAD (`git cat-file`) se abren en la otra vista como documentos de solo lectura llamados «main.cpp (guardado)» o «main.cpp (HEAD)», con el lenguaje del original. Se cierran solos al terminar |
| **Vista** (`ui/CompareSession`) | Las diferencias se calculan por líneas con el Myers de las marcas de Git (`core/LineDiff`). Las líneas tienen fondo verde (añadidas), rojo (quitadas) o amarillo (cambiadas), con colores para tema claro y oscuro. En cada par de líneas cambiadas se resaltan los caracteres distintos, con un segundo Myers por caracteres. Donde a una vista le faltan líneas, se añaden líneas en blanco (anotaciones de Scintilla) para que las dos queden alineadas. Las dos vistas se desplazan juntas, en vertical y en horizontal. Las diferencias también se marcan en la barra de desplazamiento |
| Al editar | 300 ms después de cada cambio, en cualquiera de los dos documentos, se vuelve a comparar. Al cambiar el tema, también |
| Fin | Al quitar la comparación, cambiar de pestaña en una de las dos vistas o cerrar uno de los documentos, se quitan las marcas, aunque el documento ya no esté a la vista |
| Barra de estado | «Comparar: 3 líneas cambiadas, 1 añadida y 1 quitada en 4 bloques» o «los documentos son idénticos». Además, el campo del lenguaje se oculta mientras se muestra un mensaje temporal: antes el mensaje se dibujaba encima de él |

Limitación: Scintilla solo pone anotaciones debajo de una línea, así que lo que se añade antes de la primera línea se alinea a partir de la segunda.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Una línea cambiada, dos añadidas y una quitada: fondos, caracteres distintos, líneas de relleno, alineadas («cuatro» en la misma línea visible), desplazamiento juntas, diferencia siguiente y anterior, se recompara al editar, ignorar mayúsculas y espacios, quitar la comparación quita todo | ✅ `tst_compare`: `twoDocumentsSideBySide` |
| Documentos idénticos; cambiar de pestaña termina la comparación | ✅ `identicalAndSwitchingTabs` |
| Con la versión guardada: referencia de solo lectura en la otra vista, se cierra sin preguntar; sin archivo, aviso | ✅ `withLastSavedVersion` |
| Con Git (HEAD) en un repositorio de prueba; fuera de Git, aviso | ✅ `withGitHead` |

En las capturas (claro y oscuro) se vio que el fondo de las líneas no aparecía: Scintilla solo pinta un marcador de fondo si no está en la máscara de ningún margen, y el margen de símbolos de Edita los acepta todos. Ahora se quitan de las máscaras. Pasan las 42 suites y el build no da ningún warning.
