# Fase 99 — Herramientas de CSV

Fecha: 2026-11-13

## Por qué

Los archivos CSV son de los más comunes que se abren en un editor (exportaciones, datos, configuraciones), y en texto plano es difícil saber a qué columna pertenece cada valor o ordenar las filas.

## Cómo lo hace Notepad++

Notepad++ no trae nada para CSV: ni lenguaje ni herramientas. Hay plugins aparte (CSV Lint, CSVQuery).

## Qué se hizo

- **`core/CsvTable`**, sin interfaz:
  - **`detectDelimiter`:** coma, punto y coma, tabulador o barra vertical, el que da el mismo número de campos (más de uno) en más de las primeras líneas. Una coma decimal en un CSV con punto y coma no lo confunde.
  - **Lectura de campos:**
    - `splitLine` y `columnAt` leen los campos de una línea con comillas, como en RFC 4180 (`""` dentro de las comillas); unas comillas que no se cierran llegan al final de la línea.
    - `parse` lee los registros, con saltos de línea dentro de un campo entre comillas.
  - **Alinear y compactar:**
    - `align` rellena cada campo con espacios hasta el ancho de su columna; la última va sin relleno. Un registro con varias líneas deja el texto igual.
    - `shrink` quita los espacios de alrededor de cada campo; lo de dentro de unas comillas no se toca.
  - **Ordenar:**
    - `hasHeader`: la primera fila es un encabezado si toda es texto y, en alguna columna, las demás filas tienen números.
    - `sortByColumn` ordena de forma estable, por número si todos los valores lo son y, si no, como texto sin distinguir mayúsculas. Los vacíos van primero, el encabezado se queda arriba y se conservan el salto de línea del documento (`\r\n`, `\n`) y el del final.
- **`ui/CsvColumns`:**
  - **Colores:** cada columna de un color (ocho que se repiten, una paleta para cada tema), con un solo indicador `INDIC_TEXTFORE` con el color en su valor (`SC_INDICFLAG_VALUEFORE`), como los paréntesis de colores.
  - **Solo lo visible:** pinta las líneas visibles, así un CSV de millones de filas no se vuelve lento.
  - **Cuándo:** en los `.csv`, `.tsv` y `.psv`. Con `.tsv` y `.psv`, el separador es el de la extensión; con `.csv`, el que se detecta.
- **Editar › CSV:**
  - colorear las columnas (se recuerda en `config.xml`, `EditaCsv`);
  - alinear y compactar las columnas;
  - ordenar por la columna del cursor, ascendente o descendente.

  Cada orden reemplaza el documento en un solo paso de deshacer y deja el cursor en su sitio.
- **Traducción y manual:** la traducción y el manual (Edición › CSV), en los dos idiomas.

## Pruebas

`tst_csv` (8):

- el separador;
- los campos y la columna de una posición;
- registros con saltos de línea;
- alinear y compactar (de ida y vuelta, con comillas, tabuladores y un campo de varias líneas);
- el encabezado;
- ordenar (números, texto, descendente, `\r\n` sin salto final, filas cortas);
- en el editor: los colores por columna, apagarlos y encenderlos, ordenar, alinear, deshacer, compactar, y un `.txt` sin colores.

Las pruebas encontraron dos errores antes del commit:

- **Repintar a mitad de un cambio:** al ordenar, el documento avisa que cambió antes de terminar de reemplazarse, y pintar en ese momento leía texto a medio cambiar. Edita cerraba.
- **Líneas inexistentes:** con más líneas en pantalla que en el documento, se pintaban líneas que no existen encima de las reales.
