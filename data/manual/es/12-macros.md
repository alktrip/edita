# Macros

Una macro graba lo que escribes y las órdenes de edición que usas, para repetirlas después. Son
útiles para cambios demasiado irregulares para un buscar y reemplazar.

## Grabar y reproducir

1. {{actionRecordMacro}} empieza a grabar. La misma orden (ahora **Detener la grabación**) la
   detiene.
2. Haz los cambios una vez: escribe, muévete con el teclado, usa órdenes de los menús (cortar,
   pegar, duplicar una línea, cambiar mayúsculas, buscar siguiente…).
3. {{actionPlayMacro}} los repite donde está el cursor.

Toda la reproducción se deshace en un solo paso.

Un ejemplo típico: para convertir cada línea `nombre = valor` en `"nombre": "valor",`, empieza al
principio de la primera línea, graba Inicio, `"`, Ctrl+Derecha, `"` … Fin, `,`, Abajo e Inicio, y
reprodúcela las veces que haga falta.

## Ejecutar una macro muchas veces

{{actionRunMacro}} ejecuta la macro grabada, o una guardada, un número de veces o **hasta el final
del archivo**: se detiene cuando el cursor llega a la última línea o deja de moverse.

## Guardar macros

{{actionSaveMacro}} guarda la macro grabada con un nombre y, si quieres, un atajo. Las macros
guardadas aparecen al final del menú Macro y en la [paleta de órdenes](05-views.md#paleta-de-ordenes).
**Eliminar una macro** quita una.

Edita incluye {{actionMacro0}}, que quita los espacios del final y guarda.

Las macros se guardan en `~/.config/edita/shortcuts.xml`, con el mismo formato que Notepad++: las
macros de Notepad++ se pueden copiar a Edita y al revés. Sus atajos se cambian en
{{actionShortcutMapper}} (pestaña Macros).
