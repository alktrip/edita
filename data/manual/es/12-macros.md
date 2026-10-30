# Macros y el menú Ejecutar

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


## El menú Ejecutar

El menú Ejecutar lanza otros programas con el documento o con el texto del cursor: compilar o
ejecutar el archivo actual, abrir la palabra del cursor en una página web, dar formato con una
herramienta externa…

{{actionRun}} abre un diálogo donde escribes la orden. El botón **…** elige el programa, y el botón
**$( )** inserta una variable, que se sustituye al ejecutar la orden:

| Variable | Valor |
|---|---|
| `$(FULL_CURRENT_PATH)` | Ruta completa del documento actual |
| `$(CURRENT_DIRECTORY)` | Su carpeta |
| `$(FILE_NAME)` | Su nombre, con extensión (`main.cpp`) |
| `$(NAME_PART)` | Su nombre sin extensión (`main`) |
| `$(EXT_PART)` | Su extensión (`.cpp`) |
| `$(CURRENT_WORD)` | El texto seleccionado, o la palabra del cursor |
| `$(CURRENT_LINE)`, `$(CURRENT_COLUMN)` | La línea y la columna del cursor (desde 1) |
| `$(CURRENT_LINESTR)` | El texto de la línea actual |
| `$(NPP_DIRECTORY)`, `$(NPP_FULL_FILE_PATH)` | La carpeta y la ruta del propio Edita |

La orden se ejecuta con `/bin/sh` en la carpeta del documento actual, así que se pueden usar
comillas, tuberías, `&&` y variables de entorno (`$HOME`). Las rutas con espacios necesitan
comillas: `g++ "$(FULL_CURRENT_PATH)" -o "$(NAME_PART)"`. Una orden que es solo una dirección
(`https://…`) se abre en el navegador. Las órdenes que ejecutas quedan en la lista del diálogo.

El programa se ejecuta por su cuenta: su salida no se muestra. Para verla, ejecuta la orden en la
[terminal](11-terminal.md), o termínala con algo que muestre el resultado, como `… ; read` dentro
de un programa de terminal.

**Guardar…** guarda la orden en el menú Ejecutar con un nombre y, si quieres, un atajo. Edita
incluye las tres órdenes de Notepad++: buscar la palabra del cursor en el manual de PHP y en
Wikipedia, y abrir la ruta del cursor en otra ventana de Edita. {{actionEditRunCommands}} abre la
pestaña **Órdenes de Ejecutar** de la {{actionShortcutMapper}}, para cambiar sus atajos o
eliminarlas.

Las órdenes se guardan en la sección `<UserDefinedCommands>` de `~/.config/edita/shortcuts.xml`,
como en Notepad++.
