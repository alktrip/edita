# Terminal

Edita incluye una terminal en un panel abajo de la ventana, para ejecutar órdenes sin salir del
editor. Es una terminal completa: colores, ratón, programas de pantalla completa (`htop`, `vim`,
`mc`), historial y pestañas.

## Abrir la terminal

| Orden | Qué hace |
|---|---|
| {{actionTerminal}} | Abre la terminal, le pasa el foco o, si ya lo tiene, la oculta. En los teclados en español, Ctrl+Ñ hace lo mismo. |
| {{actionNewTerminal}} | Abre otra terminal en una pestaña nueva. |
| {{actionRunSelectionInTerminal}} | Envía el texto seleccionado (o la línea actual) a la terminal y lo ejecuta. |

Las terminales nuevas empiezan en la carpeta del documento actual; sin ella, en la primera carpeta
de trabajo o en tu carpeta personal. El menú de las pestañas también tiene **Abrir la carpeta
contenedora en la terminal**, y el Explorador de carpetas, **Abrir en la terminal**.

Cada terminal tiene una pestaña, con el título que pone la shell (o el nombre del programa); el
botón **+** abre otra. Cuando la shell termina, su pestaña se cierra, y con la última se oculta el
panel.

## Usarla

- Las teclas van a la shell aunque sean atajos de la ventana (Ctrl+F, Ctrl+W, F3…). Siguen
  funcionando los atajos con Ctrl+Mayús, Ctrl+Tab, Ctrl+RePág/AvPág y los atajos de la propia
  terminal.
- **Copiar** y **pegar**: Ctrl+Mayús+C y Ctrl+Mayús+V, o Mayús+Insert. Seleccionar texto con el
  ratón también lo copia a la selección de X11, para pegarlo con el botón central.
- **Historial**: 10 000 líneas, con la rueda del ratón, la barra de desplazamiento o
  Mayús+RePág/AvPág.
- **Ratón**: los programas que lo usan lo reciben; mantén Mayús para seleccionar texto en ellos.
- El menú del clic derecho tiene copiar, pegar, seleccionar todo y limpiar.

La fuente, el tamaño y los colores son los del estilo por defecto del editor, así que siguen al
tema.

## Elegir la shell

De serie, la terminal ejecuta tu shell (`$SHELL`, o `/bin/sh`). Para usar otra, escríbela en
**Shell** (página Varios de {{actionPreferences}}), con sus argumentos: `zsh -l`, `fish`,
`bash --login`.
