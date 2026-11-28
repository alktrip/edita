# Edita en Flatpak

`io.github.alktrip.Edita.yml` compila Edita con el runtime de KDE de Flathub (Qt 6.11). Incluye:

- **libzip**: instalar los paquetes de plugins.
- **host-spawn**, de 1player/host-spawn, con licencia MIT-0. Ejecuta programas fuera de la sandbox con un pseudoterminal, para la terminal integrada.

## Probar en este equipo

```sh
packaging/flatpak/build.sh          # compila e instala para el usuario
flatpak run io.github.alktrip.Edita
packaging/flatpak/build.sh bundle   # además, un .flatpak para otro equipo
```

La CI (`.github/workflows/flatpak.yml`) compila el mismo manifiesto en el contenedor de Flathub, pasa el linter y guarda el `.flatpak` como artefacto.

## Cómo funciona dentro de la sandbox

Ver [docs/fase105.md](../../docs/fase105.md):

- Al arrancar, Edita crea un lanzador por cada programa del `PATH` del sistema, que lo ejecuta fuera con `host-spawn`, y pone esa carpeta primero en su `PATH`. Así llegan a los del sistema `git`, los servidores de lenguaje, los formateadores, las tareas y los programas que llaman los plugins.
- La terminal abre la shell del sistema.
- En Flatpak se desactivan las asociaciones de archivos (las hace el escritorio), los plugins que hay que compilar (no hay compilador) y el aviso de versiones (actualiza Flathub).
- Los 21 plugins compilados del catálogo cargan y pasan sus comprobaciones dentro del Flatpak (fase 106).

## Enviar a Flathub

1. Haz un fork de [flathub/flathub](https://github.com/flathub/flathub) y crea una rama desde `new-pr`.
2. Añade a la rama `io.github.alktrip.Edita.yml`, copiado de este manifiesto, cambiando solo la fuente de Edita por la etiqueta de la versión:

   ```yaml
       sources:
         - type: git
           url: https://github.com/alktrip/edita.git
           tag: v1.0.0
           commit: <el commit de la etiqueta: git rev-list -n1 v1.0.0>
   ```

3. Abre el PR contra `new-pr` con el título «Add io.github.alktrip.Edita».
4. En la descripción, justifica el permiso `--talk-name=org.freedesktop.Flatpak`, que el linter marca (`finish-args-flatpak-spawn-access`). Se puede usar este texto:

   > Edita is a code editor. Its integrated terminal opens the user's shell on the host, and its Git integration, language servers (LSP), external formatters and project tasks (make, npm, cargo…) run the user's own tools, which are installed on the host and cannot be bundled. They are started on the host with host-spawn, as VS Code and GNOME Builder do on Flathub. `--filesystem=host` is needed for the same reason any text editor needs it: opening and saving files anywhere, including configuration files outside the home directory.

5. La verificación del ID: `io.github.alktrip` se comprueba con la cuenta de GitHub `alktrip`. Flathub lo pide en el PR. Después del alta, la aplicación vive en el repositorio `flathub/io.github.alktrip.Edita`, y cada versión nueva es un PR allí con la etiqueta nueva.

En la compilación local, el linter da dos errores sobre las capturas y el icono «no replicados» (`appstream-external-screenshot-url`, `appstream-remote-icon-not-mirrored`). Es lo esperado: el servidor de Flathub copia esos archivos al compilar el paquete oficial. El catálogo que se genera ya apunta a `https://dl.flathub.org/media/`.
