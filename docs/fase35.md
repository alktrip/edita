# Fase 35 — Terminal integrada: resultado

Fecha: 2026-10-26

Una mejora que **no existe en Notepad++**, que solo sabe abrir la carpeta en una consola externa: una terminal de verdad dentro de la ventana, con la fuente y los colores del editor.

## Qué se construyó

| Parte | Qué hace |
|---|---|
| **Emulación** (libvterm 0.3.3, `third_party/libvterm`) | La biblioteca de emulación VT220/xterm que usa Neovim (MIT, en C, sin dependencias) se copió con sus tablas ya generadas. Entiende colores de 16, 256 y 24 bits, negrita, cursiva, subrayado, tachado, pantalla alterna, caracteres anchos y combinados, títulos (OSC 0/2), «bracketed paste», el ratón de xterm y el reajuste de líneas al cambiar el ancho |
| **Sesión** (`ui/Terminal`, `TerminalSession`) | La shell se lanza con `forkpty` en un pseudoterminal, con las señales restablecidas, en la carpeta pedida y con `TERM=xterm-256color`, `COLORTERM=truecolor` y `TERM_PROGRAM=Edita`. Lo que pone el AppImage (`APPDIR`, `APPIMAGE`, `ARGV0`, `OWD`) no pasa a la shell. La lectura no bloquea (`QSocketNotifier`), el tamaño se avisa con `TIOCSWINSZ` y, al salir, se recoge el código. Al cerrar una pestaña, la shell recibe SIGHUP y, si no termina, SIGKILL |
| **Vista** (`TerminalView`) | Dibuja la pantalla celda a celda: el ASCII por tramos del mismo estilo y lo demás en su columna, así que los caracteres anchos ocupan dos. El cursor es de bloque, subrayado o barra según lo pida el programa, y hueco sin foco. Guarda 10 000 líneas de historial, que se recorren con la rueda, la barra o Mayús+RePág/AvPág. Se selecciona con el ratón (doble clic: la palabra) y lo seleccionado queda en la selección de X11 para pegarlo con el botón central. Copiar y pegar con Ctrl+Mayús+C/V o Mayús+Insert. Menú contextual: copiar, pegar, seleccionar todo, limpiar. Los programas que piden ratón (htop, mc, vim) lo reciben; con Mayús vuelve a seleccionar. En pantalla alterna sin ratón, la rueda manda flechas (less, man) |
| Teclado | Las teclas van a la shell aunque sean atajos de la ventana (Ctrl+F, Ctrl+W, F3…). Siguen siendo de la ventana Ctrl+Mayús+… (la paleta de órdenes, etc.), Ctrl+Tab, Ctrl+RePág/AvPág y los atajos de la terminal. Tab no cambia el foco. Ctrl+letra se toma de la tecla, no del texto, y Alt+tecla se envía con Esc delante. Funcionan los métodos de entrada (acentos con IBus) |
| Tema | La fuente, el tamaño y los colores de fondo y texto son los del estilo por defecto del editor, y cambian con el tema. La paleta ANSI es la de VS Code, una para fondo claro y otra para oscuro |
| **Panel** (`ui/TerminalPanel`) | Va abajo, con una pestaña por terminal y «+» para abrir otra. El título de la pestaña lo pone la shell (o es el nombre del programa). Al salir de la shell se cierra su pestaña, y con la última se oculta el panel |
| Órdenes | Ver › «Terminal» (Ctrl+` o Ctrl+Ñ, como VS Code en teclados en español): la abre, pasa el foco a la terminal o, si ya lo tiene, la oculta. «Nueva terminal» (Ctrl+Mayús+`). «Ejecutar la selección en la terminal» envía la selección (o la línea del cursor) con Intro. En el menú de la pestaña está «Abrir la carpeta contenedora en la terminal» y en el explorador de carpetas, «Abrir en la terminal». Las terminales nuevas empiezan en la carpeta del documento; sin ella, en la primera carpeta de trabajo o en la personal |
| Preferencia | Varios › Terminal › «Shell» (`EditaTerminalShell`): programa y argumentos (`zsh -l`). Vacío: `$SHELL` o `/bin/sh` |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| El entorno de la shell no lleva lo del AppImage y sí `TERM=xterm-256color` | ✅ `tst_terminal`: `environment` |
| Colores ANSI, negrita brillante, color de 24 bits, carácter ancho de dos columnas, título, historial, pantalla alterna que devuelve lo de antes, ratón y seleccionar todo | ✅ `emulation` |
| Ctrl+F, Ctrl+W y F3 son de la terminal; Ctrl+Mayús+A, Ctrl+` y Ctrl+Ñ, de la ventana | ✅ `shortcutsStayInTheTerminal` |
| `/bin/sh` real en una carpeta con espacios: salida, sin `APPDIR`, `stty size` igual a la vista, Ctrl+C interrumpe `sleep`, el pegado espera a Intro (bracketed paste de bash), `exit 3` avisa con 3 | ✅ `realShell` |
| Desde la ventana: en la carpeta del documento, con los colores del editor, ejecutar la línea del cursor, segunda pestaña, al salir se cierran y se oculta el panel | ✅ `fromTheMainWindow` |

Se revisaron capturas en claro y en oscuro con `ls`, `git log` y una línea de colores, cursiva, subrayado y 日本語. Pasan las 40 suites y el build no da ningún warning.
