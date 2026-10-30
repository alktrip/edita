# Solución de problemas

## Al arrancar

**El AppImage no arranca.** Comprueba que tenga permiso de ejecución (`chmod +x`). Los AppImage
necesitan FUSE 2: si tu sistema solo tiene FUSE 3, instala el paquete `libfuse2` (o `fuse2`), o
ejecuta `./Edita-*.AppImage --appimage-extract-and-run`. Al ejecutarlo desde una terminal se ve el
motivo cuando falla.

**macOS dice que no se puede abrir la aplicación.** Edita no está notarizada por Apple. Ábrela con
clic derecho › Abrir, o permítela en Ajustes del Sistema › Privacidad y seguridad › Abrir
igualmente (ver [Instalación](01-introduction.md#instalacion)).

**Los archivos se abren en la ventana que ya está abierta, y quería otra.** Edita funciona como una
sola instancia. Usa `--multiInst`, o elige **Abrir siempre una ventana nueva** en **Instancias**
(página General de {{actionPreferences}}).

**Edita no restaura mis documentos.** Comprueba **Recordar la sesión para el próximo arranque**
(página Copias de seguridad), y que no lo hayas arrancado con `--nosession`.

## Texto y archivos

**Los acentos se ven mal (`Ã©` en lugar de `é`).** El archivo se abrió con la codificación
equivocada. Usa **Codificación › Codificar en UTF-8** o el juego de caracteres correcto; no uses
*Convertir a*, que conservaría los caracteres equivocados. Ver
[Codificaciones](02-files.md#codificaciones).

**Un archivo guardado en Edita tiene otros fines de línea en otro programa.** Mira el fin de línea
en la barra de estado y conviértelo con las órdenes de [Fines de línea](02-files.md#fines-de-linea).

**Cerré Edita sin guardar y perdí mis cambios.** Con el modo snapshot (activado de serie) los
documentos vuelven la próxima vez con sus cambios. Si no vuelven, mira en
`~/.config/edita/backup/`.

**Alt+arrastrar mueve la ventana en lugar de seleccionar una columna.** Tu escritorio usa
Alt+arrastrar. Usa Alt+Mayús+flechas, o cambia el atajo del escritorio (ver
[Selección](03-editing.md#seleccion)).

**La fuente del editor no es la del tema.** Edita usa la letra monoespaciada del sistema. Desactiva
**Usar la letra monoespaciada del sistema** en la página Edición.

## Servidores de lenguaje

**No hay diagnósticos ni autocompletado del servidor.** Comprueba que el servidor esté instalado y en
el `PATH` (la página Servidores de lenguaje de {{actionPreferences}} dice si Edita lo encuentra),
que los servidores estén activados y que el archivo esté guardado con su extensión.
{{actionLspLog}} muestra lo que dice el servidor; {{actionLspRestart}} lo vuelve a arrancar.

**clangd no encuentra las cabeceras.** Necesita un `compile_commands.json` en el proyecto; con CMake,
configura con `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` (ver
[Servidores de lenguaje](08-language-servers.md)).

## Plugins

**Un plugin sale como incompatible.** La pestaña Incompatibles de {{actionPluginsAdmin}} explica por
qué. Los DLL de Windows no se pueden cargar; busca el plugin en la lista de Edita.

**Falla un plugin que se compila desde su código.** La ventana de compilación muestra el registro.
Normalmente falta una herramienta o una biblioteca (`cmake`, un compilador de C++, `libxml2`…): el
mensaje dice cuál.

## Empezar de cero

Para volver a la configuración de serie, cierra Edita y cambia el nombre de `~/.config/edita/` (ver
[Archivos de configuración](14-preferences.md#archivos-de-configuracion)). Tus archivos no se ven
afectados.

## Informar de un problema

Informa de errores y sugerencias en <https://github.com/alktrip/edita/issues>. Incluye la versión
({{actionAbout}}), tu sistema y los pasos para reproducir el problema.
