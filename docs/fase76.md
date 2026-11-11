# Fase 76 — Asociación de archivos y paquetes .deb y AUR

Fecha: 2026-11-10

## Por qué

Es lo último de [lo que tiene Notepad++ y Edita no](paridad-npp.md) que más se usa: abrir los archivos con Edita desde el administrador de archivos. En Linux eso depende de un `.desktop` instalado, que el AppImage no deja. Por eso la asociación va junto con dos paquetes que sí lo instalan: un .deb para Debian y Ubuntu, y un `PKGBUILD` para AUR.

## Cómo lo hace N++ y qué hace Edita

N++ tiene la página **Preferences › File Association** (`RegExtDlg`, `MISC/RegExt/regExtDlg.cpp`):

- a la izquierda, los grupos de extensiones de `defExtArray` («Notepad», «c, c++, objc», «web script»… y «customize», que es un campo para escribir otra);
- en medio, las extensiones del grupo, con **→** y **←** (o doble clic) para pasarlas a la lista de asociadas.

Al asociar, N++ escribe en el registro de Windows: `HKEY_CLASSES_ROOT\.ext` pasa a `Notepad++_file`, y el valor de antes queda en `Notepad++_backup` para devolverlo al quitar la asociación. Necesita ser administrador.

| N++ (Windows) | Edita (Linux, XDG) |
|---|---|
| La página y sus listas | Igual: **Preferencias › Asociación de archivos**, con los mismos grupos y extensiones. Como en N++, en «customize» el punto se pone solo al escribir la primera letra, y los cambios se hacen al momento, sin esperar a Aceptar |
| `.ext` → `Notepad++_file` | La extensión no se asocia sola: se asocia su tipo MIME. Es el primer tipo de texto que el sistema conoce para ella (`QMimeDatabase`). Edita pasa a ser su programa por omisión en `~/.config/mimeapps.list` (`[Default Applications]`) y se añade al principio de `[Added Associations]`, para **Abrir con**. Lo demás del archivo (comentarios, otras secciones) queda igual. La página lo explica: al asociar `.cpp` también se abren con Edita los `.cc` |
| `Notepad++_backup` | El programa de antes se apunta en `~/.config/edita/fileAssociations.xml` y vuelve al quitar la asociación. Si dos extensiones comparten tipo, el tipo sigue con Edita hasta quitar la última |
| Una extensión que no es de nadie | Si la extensión no tiene un tipo de texto (`.png`, o una inventada), Edita le crea uno propio, `application/x-edita-<ext>`, derivado de `text/plain`. Lo escribe en `~/.local/share/mime/packages/edita-associations.xml`, con peso 60 para ganarle a otro tipo que la tenga, como N++ se queda con la extensión, y ejecuta `update-mime-database`. Si no está (paquete shared-mime-info), avisa y no la asocia |
| `Notepad++_file\shell\open\command` = la ruta de `notepad++.exe` | Hace falta un `edita.desktop`. Los paquetes .deb y AUR lo instalan. Con el AppImage (sin `edita.desktop` en `$XDG_DATA_DIRS`), Edita escribe el suyo en `~/.local/share/applications`: `Exec` con la ruta del AppImage, entre comillas y con los escapes de la especificación (`desktopExec`), y el ícono en `~/.local/share/icons`. Al arrancar, si ese `.desktop` apunta a un AppImage que ya no está (otra versión, otra carpeta), lo pasa al de ahora (`refreshLauncher`). Uno escrito a mano no se toca |
| Ser administrador | Todo es del usuario: no hace falta |

Algunos escritorios guardan su elección en un archivo propio (`kde-mimeapps.list`), que manda sobre `mimeapps.list`. Edita no los toca; el manual explica qué hacer si un tipo sigue abriéndose con otro programa.

## Los paquetes

- **.deb** (`packaging/deb/build-deb.sh`). Debian 12 y Ubuntu 22.04 y 24.04 traen un Qt más viejo que el 6.7 que necesita Edita, así que el paquete lleva el AppDir del AppImage, con su Qt, en `/opt/edita`. Además:
  - `/usr/bin/edita` es un enlace a `/opt/edita/usr/bin/edita`;
  - el `.desktop` y los íconos van en `/usr/share`;
  - `dpkg-shlibdeps` calcula las dependencias;
  - la versión lleva `~` en lugar de guion (`0.9.0~beta.9`).

  `build-appimage.sh` lo genera al terminar si hay `dpkg-deb`, también dentro de `build-portable.sh` y en la CI, que lo sube a cada release ([empaquetado](empaquetado.md)).
- **AUR** (`packaging/aur/PKGBUILD`). Compila desde el tarball de la etiqueta con el Qt de Arch, sin `-Werror` y sin el aviso de versiones nuevas. [`packaging/aur/README.md`](../packaging/aur/README.md) explica cómo publicarlo; hace falta una cuenta de AUR.
- **`edita.desktop`**. Los textos sin idioma pasan a inglés (`GenericName`, `Comment`, `Keywords`), con su traducción `[es]`, como pide la especificación.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Los grupos de `defExtArray`; una extensión se normaliza (`TXT` → `.txt`) y se rechaza la que no vale (comodines, espacios, barras) | ✅ `tst_fileassociation`: `groups`, `normalize` |
| `Exec=` con comillas y escapes | ✅ `execQuoting` |
| El tipo de texto del sistema; si no hay, el propio | ✅ `mimeTypes` |
| Asociar pone a Edita por omisión y en **Abrir con**, y guarda el programa de antes; quitarla lo devuelve, y lo demás de `mimeapps.list` queda igual | ✅ `addAndRemove`, `noPreviousDefault` |
| Dos extensiones del mismo tipo | ✅ `sharedMimeType` |
| El tipo propio, `update-mime-database`, y el aviso si no está | ✅ `ownMimeType`, `ownMimeTypeWithoutTool` |
| El `.desktop` propio del AppImage (y no si Edita está instalado), y la ruta corregida al cambiar de AppImage | ✅ `ownDesktopFile`, `installedDesktop`, `refreshLauncher` |
| La página: listas, →, ←, doble clic, «customize» con el punto, y una extensión que no vale | ✅ `preferencesPage` |
| La página cabe en el diálogo sin agrandarlo, en los dos idiomas | ✅ `tst_appearance` |
| El paquete .deb se genera en Ubuntu 22.04, se instala y abre Edita | ✅ (ver «Comprobado a mano») |
| El `PKGBUILD` compila e instala en Arch | ✅ (ver «Comprobado a mano») |
| Pasan las 66 suites, el build no da avisos (GCC 16 y clang) y compila con el GCC 12 y Qt 6.8.3 de la CI | ✅ |

## Comprobado a mano

- **.deb.** `build-portable.sh` genera `edita_0.9.0~beta.8_amd64.deb` (40 MB). `dpkg-shlibdeps` pone en `Depends` glibc 2.35, OpenGL, X11, fontconfig, Wayland y poco más (sin `libopengl0`). En el chroot de Ubuntu 26.10, `apt install ./edita_*.deb` instala las dependencias y deja `/usr/bin/edita`, el `.desktop` y los íconos. Edita abre un archivo y `apt remove` lo quita todo, `/opt/edita` incluido.
- **AUR.** `makepkg` con el `PKGBUILD` y el tarball de la beta 8 compila e instala en `/usr` sin advertencias. La primera vez avisaba de «referencias a `$srcdir`» en el binario: eran las rutas de los `assert` de Scintilla (Arch no define `NDEBUG` con `CMAKE_BUILD_TYPE=None`) y la carpeta de datos del código. El `PKGBUILD` añade `-DNDEBUG`, y la opción nueva `EDITA_SOURCE_DATA=OFF` quita la carpeta (también en el AppImage).
- **Tipos propios.** El paquete de `mime/packages` que escribe Edita pasa por `update-mime-database`. Después, `xdg-mime query filetype` da `application/x-edita-zzq` para un `.zzq`, y `application/x-edita-png` para un `.png`, que gana por el peso 60.

## Para publicar en AUR

El nombre `edita` está libre en AUR. Subirlo necesita una cuenta de AUR con clave SSH, así que lo hace quien mantiene el proyecto ([packaging/aur/README.md](../packaging/aur/README.md)), con la etiqueta de la versión que lleve esta fase.
