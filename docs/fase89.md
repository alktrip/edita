# Fase 89 — Editor hexadecimal

Fecha: 2026-11-13

## Por qué

HEX-Editor es el octavo plugin más descargado de Notepad++ (2,2 millones de descargas en GitHub). La fase 87 intentó portarlo y no alcanzaba: su vista hexadecimal es una lista de Windows que el plugin dibuja y maneja a mano, lo más difícil de imitar con la capa de compatibilidad. Escribirlo desde cero en Qt era menos trabajo y menos riesgo, y da un resultado nativo.

## Cómo lo hace Notepad++

HEX-Editor 0.9.14 (`chcg/NPP_HexEditor`) cambia la vista del documento (Ctrl+Alt+Mayús+H) a una lista con la posición, los bytes y el ASCII. Edita el texto del documento de Scintilla, buscar y reemplazar, ir a, comparar y colores.

| HEX-Editor (N++) | Editor hexadecimal de Edita |
|---|---|
| Edita el texto del documento de Scintilla | Edita los **bytes del archivo en el disco**. Edita guarda el texto en UTF-8, así que un binario o un archivo en otra codificación no tendría sus bytes reales |
| Ocupa la vista del documento | Un panel acoplable (abajo, se puede soltar) y el documento sigue a la vista |
| Ctrl+Alt+Mayús+H | Igual |
| Buscar y reemplazar | Buscar bytes o texto, hacia delante o atrás, dando la vuelta; reemplazar no |
| Comparar dos archivos, colores configurables | No, por ahora (Edita ya compara documentos de texto) |
| — | Inspector de valores: enteros de 8 a 64 bits (LE/BE), flotantes y el carácter UTF-8 |

## Qué se hizo

`plugins/EditaHex`, un plugin propio incluido con Edita:

- **`HexBuffer`** (sin interfaz):
  - Los bytes y sus cambios: sobrescribir, insertar, borrar y un dígito hexadecimal, con deshacer y rehacer. Los cambios seguidos son un solo paso, como al escribir, y los dos dígitos de un byte también.
  - «Sin cambios» se calcula por el identificador del paso de deshacer: deshacer hasta lo guardado vuelve a «sin cambios».
  - Buscar con vuelta, `parseHex`/`toHex` y `valuesAt` para el inspector.
  - Carga hasta 512 MB y guarda con `QSaveFile`.
- **`HexView`** (`QAbstractScrollArea`):
  - Dibuja solo las filas visibles: posición, bytes agrupados de 8 en 8 y ASCII.
  - El cursor se ve en la columna activa (recuadro al sobrescribir, barra al insertar) y punteado en la otra. Tab cambia de columna.
  - Teclado completo (flechas, inicio y fin, páginas), selección con Mayús o el ratón, copiar en hex o texto y pegar hex o texto. Insert cambia de modo.
- **El panel:** abrir, guardar, guardar como, ir a una posición (`0x1F`, `1Fh`, `31`), buscar en hex o texto, bytes por fila (8, 16, 24, 32), el modo y una línea con el archivo, el tamaño, la posición y la selección, más el inspector. Al cerrar Edita con cambios, pregunta.
- **Traducciones** al español y plurales del inglés dentro del `.so`, el **manual** (Plugins › Editor hexadecimal) y los **README**.
- El borrador de la receta de HEX-Editor en edita-plugins se retira.

## Pruebas

`tst_hexeditor` (12):

- **El modelo:** cargar, guardar y el estado de cambios con deshacer; los pasos agrupados; los dígitos; buscar con vuelta; hex ↔ bytes y el inspector.
- **La vista, con teclado y ratón simulados:** escribir hex en los dos modos, retroceso y deshacer; la columna ASCII; seleccionar, copiar, pegar y borrar todo; un clic en el byte correcto.
- **El plugin en Edita:** abrir el archivo actual, cambiar un byte y guardar (el archivo en el disco cambia), ir a una posición, buscar texto y el inspector.
