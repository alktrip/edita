# Fase 4 — Buscar y reemplazar: resultado

Fecha: 2026-10-19 · Entorno: el mismo que en las fases anteriores

## Qué se construyó

| Módulo | Equivale en N++ a | Qué hace |
|---|---|---|
| `search/SearchOptions` | `FindOption`, `Searching::buildSearchFlags`, `Searching::convertExtendedToString` | Opciones, flags `SCFIND_*` y modo extendido (`\n \r \t \0 \\ \xNN \uNNNN \dNNN \oNNN \bNNNNNNNN`) |
| `search/SearchTarget` | `ScintillaEditView::searchInTarget`, `replaceTarget`, `replaceTargetRegExMode` | Interfaz con dos implementaciones: una **vista de Scintilla** y un **`Scintilla::Internal::Document` sin vista**, la misma clase y la misma regex Boost que hay detrás de cada vista, usable en otro hilo |
| `search/SearchEngine` | `processFindNext`, `processReplace`, `processAll`, `processRange`, `Finder::foundLine` | Buscar siguiente/anterior con "dar la vuelta" y sus estados; reemplazar (solo si la selección es una coincidencia, y entonces selecciona la siguiente); reemplazar todo, contar y buscar todo, con el mismo tratamiento de coincidencias vacías (`SCFIND_REGEXP_EMPTYMATCH_*`, `SKIPCRLFASONE`) |
| `search/FileSearch` | `getMatchedFileNames`, `matchInList`, `matchInExcludeDirList`, `cutString`, `findInFilelist`, `replaceInFiles` | Filtros (`*.cpp *.h`, `!*.min.js`, `!\carpeta` solo en el primer nivel, `!+\carpeta` en todos; en Linux también valen `!/` y `!+/`), recorrido alfabético en profundidad y búsqueda/reemplazo **en un hilo de trabajo cancelable** |
| `ui/FindReplaceDialog` | `FindReplaceDlg` | Diálogo no modal con pestañas Buscar / Reemplazar / Buscar en archivos que comparten campos e historial |
| `ui/SearchResultsPanel` | `Finder` | Panel acoplable con el lexer `searchResult` de N++ y el mismo formato de líneas; las coincidencias llegan al lexer por `@MarkingsStruct` como en N++ |
| `MainWindow` | `Notepad_plus::findInFiles`, `findInOpenedFiles`, `replaceInOpenedFiles`, `_invisibleEditView` | Menú Buscar, mensajes de estado y coordinación |

## En la interfaz

- **Menú Buscar:**

  | Acción | Atajo |
  |---|---|
  | Buscar… | Ctrl+F |
  | Buscar en archivos… | Ctrl+Mayús+F |
  | Buscar siguiente / anterior | F3 / Mayús+F3 |
  | Buscar la selección (siguiente / anterior) | Ctrl+F3 / Ctrl+Mayús+F3 |
  | Reemplazar… | Ctrl+H |
  | Ir a… | Ctrl+G |
  | Resultado siguiente / anterior | F4 / Mayús+F4 |
  | Panel de resultados | F7 |

- **Diálogo:**
  - *Buscar:* Buscar siguiente, Contar, Buscar todo en el documento actual, Buscar todo en todos los documentos abiertos.
  - *Reemplazar:* Buscar siguiente, Reemplazar, Reemplazar todo, Reemplazar todo en los documentos abiertos.
  - *Buscar en archivos:* filtros, directorio, subcarpetas, carpetas ocultas, "seguir el documento actual", Buscar todo, Reemplazar en archivos y Cancelar mientras trabaja.
  - Opciones: palabra completa, mayúsculas, dar la vuelta, hacia atrás, en la selección, y modo Normal / Extendido / Regex con ". coincide con salto de línea".
  - Como en N++, "palabra completa" se desactiva en modo regex.
  - Al abrirlo se rellena con la selección (si es de una línea) o con la palabra del cursor.
- **Mensajes de estado** de N++ traducidos, con sus colores: rojo para no encontrado, azul para mensajes y verde para "se alcanzó el final". Por ejemplo: «Buscar: se encontró la 1.ª aparición desde el principio. Se alcanzó el final del documento.», o expresión regular no válida con el mensaje de Boost. Una coincidencia vacía muestra «^ coincidencia de longitud cero».
- **Reemplazar todo** se deshace en un solo paso. Con "en la selección", la selección queda sobre el texto reemplazado.
- **Panel de resultados:**
  - Cada búsqueda nueva va arriba y las anteriores quedan plegadas.
  - Doble clic o F4 lleva al resultado, abriendo el archivo si hace falta.
  - Varias coincidencias de la misma línea forman una sola entrada (opción por defecto de N++).
  - Las líneas de más de 2048 bytes se recortan con " …".
- **Buscar en archivos** usa el texto actual de los documentos abiertos, con los cambios sin guardar, como N++.
- **Reemplazar en archivos:** pide confirmación. Como N++, los documentos abiertos se reemplazan en el propio documento y se guardan; los demás se reescriben en disco **conservando su codificación, BOM y fin de línea**.

## Decisiones y divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Buscar/reemplazar en archivos | Hilo de la interfaz con un diálogo de progreso | **Hilo de trabajo**, con progreso en el diálogo y cancelación inmediata | La interfaz no se bloquea. El resultado es idéntico: misma clase `Document`, misma regex y mismo recorrido |
| Mensaje de regex inválida (`g_exceptionMessage`) | Variable global | **`thread_local`** (parche `0003`) | La variable se vaciaba en cada búsqueda: con dos hilos buscando a la vez era una carrera de datos |
| Directorios ocultos | Atributo "oculto" de Windows | Los que empiezan por punto (`.git`…) | Convención de Linux |
| Enlaces simbólicos a directorios | Se siguen | No se siguen | Evita ciclos infinitos |
| Mayúsculas en los filtros | Sin distinguir (`PathMatchSpec`) | Sin distinguir | Igual que N++, aunque el sistema de archivos sí distinga |
| Separador en exclusiones de carpeta | `!\carpeta` | `!\carpeta` y `!/carpeta` | Más natural en Linux |
| Mensajes con plurales | Textos fijos en inglés | "1 coincidencia" / "3 coincidencias" | Español correcto |
| Marcar, resaltado inteligente, búsqueda incremental, buscar en proyectos, buscar en el panel | Sí | Aún no | Fuera del alcance del MVP |
| Historial de búsqueda entre sesiones | En `config.xml` | En memoria | Se guarda con la sesión (Fase 5) |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Tabla de ≥ 25 casos regex: lookbehind, `\R`, `^`/`$` por línea, `(?s)`/`(?-s)`, recursión `(?R)`, coincidencias vacías sin bucles y referencias en el reemplazo | ✅ `regexFindAll` (28 casos) y `regexReplaceAll` (10, con `$1`, `\1`, `\U…\E`, condicionales y `$&`). **Cada caso se ejecuta con la vista y con el documento sin vista, y los resultados coinciden** |
| Modo extendido: `\t \n \r \0 \x41 é` | ✅ `extendedConversion` (13 casos) |
| Buscar en archivos sobre un árbol con anidados, ocultos y varios encodings da **exactamente** la lista esperada, con y sin recursión ni ocultos | ✅ `listingOrderRecursionAndHidden`, `findInFilesExactHits`, `findInFilesUsesOpenDocumentText`, `findInFilesAsyncAndOpenResult` |
| Cancelar una búsqueda sobre 10 000 archivos termina en < 500 ms y la UI no se bloquea | ✅ `cancelOnTenThousandFilesIsFast`: el hilo principal sigue procesando eventos y la cancelación llega en milisegundos |
| Reemplazar en archivos conserva encoding, BOM y EOL (comparado byte a byte) | ✅ `replaceInFilesPreservesEncodingBomAndEol` (Windows-1252, UTF-8 BOM, UTF-16 LE BOM, Shift-JIS, CRLF, CR) y `replaceInFilesSavesOpenAndClosedFiles` |

Además, con tests:
- Buscar siguiente con "dar la vuelta" y sus estados.
- Reemplazar paso a paso como N++.
- Reemplazar todo en un solo paso de deshacer.
- Contar y reemplazar en la selección.
- Documento de solo lectura.
- Regex inválida.
- Panel: formato, colores del lexer y navegación.
- Búsquedas nuevas arriba.
- Documentos abiertos, incluidos los sin título y los que no están a la vista.
- Diálogo: relleno, pestañas, regex frente a palabra completa, historial y directorio por defecto.

Tests: `tst_search` 67 y `tst_findui` 21, y siguen en verde los de las fases anteriores. El build desde cero no da ningún warning.
