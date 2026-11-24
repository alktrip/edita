# Fase 102 — Al día con Notepad++ (noviembre de 2026)

Fecha: 2026-11-23

## Por qué

Desde la fase 78 (Notepad++ 8.9.9), Notepad++ publicó la 8.9.9.1 y añadió 25 cambios más a su rama principal (hasta `6ed1e3cd6`, del 23 de noviembre). Se revisaron uno por uno para ver cuáles le tocan a Edita.

## Qué cambió en Notepad++ y qué pasa en Edita

| Cambio en N++ | En Edita |
|---|---|
| 8.9.9.1: resaltar todas las coincidencias en el diálogo Buscar, Carpeta como espacio de trabajo con enlaces simbólicos, pegar en el espacio virtual con varias selecciones | Ya estaba ([fase 78](fase78.md)) |
| Estilo para los literales de usuario de C++ (`SCE_C_USERLITERAL`, 25) en el modelo y en todos los temas | **Faltaba: hecho** |
| Recortar los espacios de una selección ya no mueve la vista (#17496) | **Edita tenía el mismo fallo: corregido** (también en ordenar líneas y en convertir tabuladores) |
| «Ordenar pestañas» del diálogo Ventanas respeta las fijadas | **Edita tenía el mismo fallo, también en Ventana › Ordenar por: corregido** |
| Mover la pestaña adelante o atrás respeta las fijadas | Ya lo hacía ([fase 67](fase67.md)) |
| Se cerraba con parámetros vacíos o no numéricos en la línea de órdenes (#18627) | Probado con 15 casos (`-n ""`, `-n abc`, números enormes, `-c xyz`, `-l` desconocido, sesiones que no existen): Edita no se cierra |
| Atributos que faltan al leer los XML de configuración; cadenas sin terminar al copiarlas | Edita lee con pugixml y ya comprueba cada atributo |
| La Lista de documentos no seguía a las pestañas al moverlas | Probado: en Edita sí las sigue |
| Mayús+F4 en los resultados de búsqueda volvía a la misma coincidencia | No aplica: Edita va de línea en línea |
| Pantallas de varios DPI, arrastrar entre instancias, permisos de administrador, instalador, orden del tabulador en el diálogo Buscar | Solo de Windows |

## Qué se hizo

- **Literales de usuario de C++** (`10_km`, `1.5_kg`): el estilo 25 en `data/stylers.model.xml` y en los 30 temas. En los 23 temas que vienen de N++, su color; en los 7 propios de Edita (Edita Light y Dark, Catppuccin, Dracula, Nord, One Dark, Tokyo Night), el color de los números un 25 % más claro en un tema oscuro o más oscuro en uno claro, la misma regla que siguió N++. Todos con `modelDate="20261117"`.
- **Lo nuevo de los estilos llega a quien ya usaba Edita** (`updateStylesFromModel`, el `updateStylesXml` de N++). Hasta ahora solo `langs.xml` se ponía al día; `stylers.xml` se copiaba la primera vez y nunca más. Al arrancar, si el modelo es más nuevo, se le añade lo que falte sin cambiar lo que el usuario tiene: estilos globales, lexers enteros, estilos de un lexer y atributos (uno vacío se respeta). También a los temas de `~/.config/edita/themes`: la copia de un tema de la aplicación, desde ese tema (con sus colores); un tema propio, desde el modelo con el texto y el fondo de su «Default Style», como N++, para no poner colores de un tema claro en uno oscuro.
- **Recortar, ordenar y convertir tabuladores en una selección** conservan la primera línea visible y el sentido de la selección. Antes, con una selección hecha de abajo arriba más alta que la ventana, la vista saltaba al final y el cursor quedaba abajo.
- **Ordenar las pestañas** (Ventana › Ordenar por y el botón del diálogo Ventanas) deja las fijadas al principio, ordenadas entre ellas, y las demás después.

## Pruebas

- `tst_morelanguages`: `updateStylesFromModel` (lo del usuario y los atributos vacíos se respetan; lexers, estilos y globales nuevos; los colores del «Default Style» en un tema propio; no vuelve a tocar un archivo al día), un `stylers.xml` de la versión anterior que recibe el estilo nuevo al arrancar, `10_km` con el estilo 25 y su color, y la copia del tema Monokai que recibe el color de Monokai.
- `tst_editcommands`: recortar, ordenar y convertir tabuladores en una selección de 1400 líneas hecha de abajo arriba: la vista sigue en la línea 100 y el cursor, arriba (sin la corrección, la vista saltaba a la 1499).
- `tst_windows`: ordenar con una y con dos pestañas fijadas, por el menú y desde el diálogo Ventanas.
