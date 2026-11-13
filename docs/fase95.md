# Fase 95 — F1 contextual

Fecha: 2026-11-13

## Por qué

F1 abría siempre el manual en su primera página. En un diálogo con muchas opciones (Preferencias, el Configurador de estilos, Buscar) o en un panel (la terminal, Git, la lista de funciones), lo útil es ir directo a la sección que lo explica.

## Cómo lo hace Notepad++

En Notepad++, F1 abre **Acerca de** y no hay ayuda contextual. La documentación está en su sitio web, sin enlaces desde los diálogos. Esto es propio de Edita.

## Qué se hizo

- **`Manual::placeOf(orden)`.** Al cargar el manual, se anota el apartado donde se cita cada orden por primera vez (`{{actionFind}}`, `{{key:…}}`, `{{item:…}}`). Los anclajes se cuentan igual que los del índice, también los de `{{shortcuts}}`, así que funciona en cualquier idioma sin mantener anclajes a mano. `tst_manual` ya exige que cada orden del menú esté citada en cada idioma.
- **`ui/ContextHelp`.** Una tabla relaciona 28 diálogos y paneles, por su clase o su nombre, con la orden que los abre: el diálogo Buscar con `actionFind`, Preferencias con `actionPreferences`, la lista de funciones con `actionFunctionList`… `commandFor` sube desde el widget con el foco hasta el primero que está en la tabla. Los paneles de los plugins van al capítulo de los plugins.
- **F1 en la ventana principal** (el atajo de Ayuda › Manual de usuario): la sección del panel con el foco. En el editor, el manual como siempre.
- **F1 en un diálogo:** el atajo del menú no llega a otra ventana. Un filtro de eventos (`HelpKeyFilter`) atrapa F1 en los diálogos abiertos desde esa ventana principal.
- **Diálogos modales:** si hay un diálogo modal abierto (Preferencias…), el manual se abre modal, encima de él. Si no, el diálogo lo bloquearía. Desde la ventana principal sigue siendo una ventana normal.
- **Manual** (Ayuda, en la introducción), en los dos idiomas.

## Pruebas

En `tst_manual`:

- `placesOfCommands`: `actionFind` está en el capítulo de búsqueda y su anclaje es un encabezado de ese capítulo, en los dos idiomas.
- `everyContextIsDocumented`: cada entrada de la tabla tiene su sección en los dos idiomas.
- `f1InAPanelOpensItsSection`: con el foco en la lista de funciones, F1 abre su apartado, y el manual no es modal; en el editor, el manual.
- `f1InADialogOpensItsSection`: F1 en Preferencias abre su apartado, con el manual modal, y después, desde la ventana principal, ya no es modal.
