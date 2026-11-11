# Edita en AUR

`PKGBUILD` compila Edita desde el código de una etiqueta, con el Qt de Arch, y lo instala en
`/usr`: el menú del escritorio, los íconos, los datos en `/usr/share/edita` y los plugins en
`/usr/lib/edita/plugins`. No usa `-Werror` ni avisa de versiones nuevas: las trae pacman.

## Publicar una versión

Después de publicar la etiqueta en GitHub (el tarball tiene que existir):

1. En `PKGBUILD`, poner la versión en `pkgver` sin guiones (`v0.9.0-beta.9` es `0.9.0beta9`;
   para pacman, `0.9.0beta9` va antes de `0.9.0`) y `pkgrel=1`.
2. Actualizar la suma y `.SRCINFO`:

   ```sh
   updpkgsums
   makepkg --printsrcinfo > .SRCINFO
   ```

3. Probarlo: `makepkg -si` en una carpeta aparte compila e instala el paquete.
4. Subir `PKGBUILD` y `.SRCINFO` al repositorio de AUR, que es un repositorio git propio:

   ```sh
   git clone ssh://aur@aur.archlinux.org/edita.git aur-edita
   cp PKGBUILD .SRCINFO aur-edita/
   cd aur-edita && git add PKGBUILD .SRCINFO && git commit -m "Edita 0.9.0 beta 9" && git push
   ```

   Hace falta una cuenta de AUR con una clave SSH. La primera vez, el `git push` crea el paquete.

Luego hay que hacer commit de los cambios también aquí, para que la próxima versión parta de ellos.
