# Fase 101 — Revisión de errores

Fecha: 2026-11-17

## Por qué

Tras la rc.5 (la ventana negra en Wayland por una miga de pan de miles de caracteres) se revisó el código en busca de fallos del mismo tipo y de otros que pudieran hacer que la aplicación funcione mal: textos que ensanchan la ventana, trabajo pesado en el hilo de la interfaz, procesos que se esperan con un tiempo límite, memoria sin tope y casos raros al guardar. Además de la lectura del código, se pasó `clang-tidy` (712 avisos, casi todos de estilo o falsos positivos).

## Qué se corrigió

- **Textos largos en la barra de estado y en el panel de tareas.** Un complemento puede poner cualquier texto en la barra de estado (`NPPM_SETSTATUSBAR`), y el nombre de un lenguaje definido por el usuario o de una tarea también puede ser largo. La barra pedía todo ese ancho y la ventana, el de la barra: con un texto de 3000 caracteres, la ventana pedía 95 412 px de ancho mínimo (el mismo fallo de la rc.5). Ahora cada campo muestra 80 caracteres como mucho, en una línea, y no tiene un ancho mínimo por su texto: con la ventana estrecha se recorta. Los getters siguen dando el texto completo. El estado del panel de tareas, igual (120 caracteres, el nombre completo en la descripción emergente).
- **Memoria del diff.** El algoritmo de Myers guarda el camino con memoria que crece con el cuadrado de las ediciones. Aplicar la salida de un formateador externo pedía hasta 20 000 ediciones: con un archivo de 20 000 líneas en el que cambia la sangría de todas, **1.5 GB** y casi un segundo, para luego rendirse y reemplazar todo de una vez. Ahora hay un tope (`kMaxDiffEdits`, 5000: unos 100 MB en el peor caso) que ningún llamador puede pasar.
- **`git commit` sin esperar.** El commit del panel Git se ejecutaba esperando y se mataba a los 30 s: un hook pre-commit lento (linters, pruebas) o una firma GPG esperando la frase de paso abortaban el commit, y mientras tanto la ventana no respondía. Ahora corre aparte, sin límite de tiempo; el botón dice «Confirmando…» y se apaga, y el mensaje no se puede editar hasta que termina. Si falla, el aviso muestra lo que dijo el hook y el mensaje se queda. Al cerrar la ventana con un commit en curso, se espera a que termine.
- **Historial de Git sin esperar.** `git log --follow` del archivo actual se pedía al cambiar de documento y se esperaba; en un repositorio grande tarda segundos. Ahora corre aparte y, si se pide otro, el anterior se cancela.
- **Panel TODO con documentos grandes.** Con el alcance «documento», cada pausa al escribir buscaba en todo el texto en el hilo de la interfaz: en uno de 60 MB, casi medio segundo. Los documentos de más de 4 MB se buscan en otro hilo.
- **Guardar a través de una cadena de enlaces.** Con `a → b → real`, guardar `a` escribía en `b` (`symLinkTarget` solo da el primer destino): el enlace `b` pasaba a ser un archivo y `real` no cambiaba. Ahora se guarda al final de la cadena; un enlace roto crea su destino, como antes.
- **Menú Ver › Tema.** Se rehace al abrirse y dejaba un `QActionGroup` de más cada vez (`QMenu::clear` no borra los grupos).
- **Ordenar CSV.** La comparación de números ya no desreferencia un `optional` (era seguro, porque antes se comprueba que todos son números, pero `clang-tidy` lo marcaba y así no depende de eso).

## Lo que se revisó y está bien

- Los avisos de `clang-tidy` sobre `optional` en el panel de resultados de búsqueda, la «fuga» del diálogo del editor de columnas (tiene padre), el hijo de `forkpty` en el terminal, el `catch` vacío de la configuración regional y las conversiones de índices acotados: falsos positivos o intencionados.
- Guardar con `QSaveFile` y la codificación por bloques (con la parte de un carácter UTF-8 entre dos bloques), las copias de cadenas de la API de plugins y la barra de notificaciones.
- Quedan esperando con tiempo límite dos órdenes de Git que el usuario pide a propósito (comparar con HEAD y ver el commit de una línea de blame): son rápidas y es razonable esperar.

## Pruebas

- `tst_statusbar`: textos de 3000 caracteres en los seis campos y en el panel de tareas; la ventana pide menos de 1500 px (sin la corrección, 95 412).
- `tst_gitchanges`: un `maxEdits` enorme se baja al tope.
- `tst_githunks`: un hook pre-commit de un segundo no para la ventana (el clic vuelve enseguida, el botón se apaga, un segundo clic no lanza otro commit) y el commit se hace; un hook que falla da el aviso con su salida y deja el mensaje. El commit del panel y el del detector de secretos esperan a que termine.
- `tst_todo`: un documento de 5 MB se busca en otro hilo, también al escribir.
- `tst_textfile`: guardar por una cadena de dos enlaces y por un enlace roto.
- `tst_mainwindow`: abrir el menú de temas cinco veces deja un solo grupo.
