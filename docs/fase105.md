# Fase 105 — Edita dentro de Flatpak: el puente al sistema

Fecha: 2026-11-28

## Por qué

Flatpak ejecuta cada aplicación en una sandbox: ve sus propias bibliotecas (el runtime) y, con permiso, los archivos del usuario, pero no los programas del sistema. Un editor de código vive de ellos: Edita llama a `git`, a los servidores de lenguaje (`clangd`, `pylsp`, `rust-analyzer`…), a los formateadores, a `make` o `npm` en las tareas y a la shell del usuario en la terminal; y algunos plugins hacen lo mismo (FTP usa `ssh` y `sftp`; el detector de secretos, `git`). Sin un puente, en Flatpak nada de eso funcionaría.

## Cómo lo resuelven otros

VS Code y GNOME Builder en Flathub ejecutan esos programas fuera de la sandbox con `flatpak-spawn --host` (que pide el permiso `org.freedesktop.Flatpak`). Para la terminal, VS Code usa `host-spawn`, que además le da al programa un pseudoterminal en el sistema y le reenvía el tamaño de la ventana y las señales: con `flatpak-spawn` a secas, la shell no tiene control de trabajos (Ctrl+C, Ctrl+Z) y no se entera de que la terminal cambió de tamaño.

## Qué se hizo

- **`core/Sandbox`**:
  - `isFlatpak()`: `/.flatpak-info` (la variable `EDITA_SANDBOX=flatpak|none` lo fuerza en las pruebas).
  - `hostLauncher()`: `/app/bin/host-spawn`, que irá dentro del paquete; si no está, `flatpak-spawn --host --watch-bus` (con `--watch-bus`, lo lanzado termina si Edita termina).
  - **El puente** (`setup` y `createBridge`): al arrancar, Edita pregunta al sistema su `PATH` y su `SHELL`, y crea en `$XDG_RUNTIME_DIR/edita-host-bin` un lanzador y un enlace a él por cada ejecutable de esas carpetas. El lanzador ejecuta el programa con el nombre del enlace, con los mismos argumentos y la misma carpeta de trabajo. Esa carpeta va primero en el `PATH` de Edita, así que cualquier `QProcess` que pida «git» o «clangd», de Edita o de un plugin, llega al del sistema sin cambiar cada llamada. Las carpetas del sistema se leen donde la sandbox las ve (`/usr` y las demás en `/run/host`; la carpeta personal y `/opt`, donde están); si un programa está en dos carpetas, gana la primera del `PATH`, como en el sistema. Quedan dentro: el propio Edita, los lanzadores y `xdg-open`/`xdg-email`, que en Flatpak pasan por los portales.
- **La terminal integrada**: la shell del sistema (`$SHELL` de fuera, o la de Preferencias), con `host-spawn`.
- **Diccionarios**: los de Hunspell del sistema, en `/run/host/usr/share/hunspell` y `myspell`.
- **Lo que no tiene sentido en Flatpak, desactivado con su explicación**:
  - Preferencias › Asociación de archivos: los tipos de archivo los declara el `.desktop` del paquete y la aplicación predeterminada se elige en el escritorio.
  - Los plugins que hay que compilar (las recetas): no hay compilador en la sandbox. Los plugins ya compilados del catálogo se instalan igual.
  - El aviso de versiones nuevas: lo desactiva el manifiesto (`EDITA_UPDATE_CHECK=OFF`), porque Flathub actualiza la aplicación.

## Pruebas

`tst_sandbox`:

- la detección de Flatpak;
- las carpetas del sistema vistas desde la sandbox;
- el puente con un «host-spawn» falso que dice qué recibe: un programa llamado por el `PATH` llega con sus argumentos (también con espacios) y su carpeta de trabajo, gana la primera carpeta del `PATH`, se omiten los excluidos y lo que no es ejecutable, y se rehace en cada arranque;
- un lanzador con comillas en su ruta;
- los diccionarios del sistema solo en Flatpak;
- la página de asociaciones, desactivada en Flatpak y activa fuera.

La prueba de verdad, dentro de un Flatpak, es la de la fase 106.
