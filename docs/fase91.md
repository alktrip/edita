# Fase 91 — Acerca de Edita

Fecha: 2026-11-13

## Por qué

**Ayuda › Acerca de Edita** era un cuadro de mensaje con la versión, una línea de descripción y el enlace al proyecto. No decía:

- **la arquitectura:** Edita se publica para x86_64 y arm64;
- **cómo se instaló:** AppImage, .deb o .rpm;
- **la licencia de los componentes:** las de MIT, BSD y Boost piden reproducir su aviso.

## Cómo lo hace Notepad++

El `AboutDlg` de Notepad++ (`PowerEditor/src/WinControls/AboutDlg/AboutDlg.cpp`) muestra:

- el camaleón, con su versión para el modo oscuro;
- la versión con «(64-bit)» o «(ARM 64-bit)»;
- la fecha y hora de compilación;
- el texto de la licencia en un cuadro con desplazamiento;
- el enlace a la página.

La información para reportar errores está en otro diálogo, **Información de depuración**, que Edita ya tiene (fase 68).

| AboutDlg (N++) | Acerca de en Edita |
|---|---|
| Versión y «(64-bit)» / «(ARM 64-bit)» | Versión, arquitectura y forma de instalación: «Edita 1.0.0-rc.3 (x86_64, AppImage)» |
| Fecha y hora de compilación (`__DATE__`, `__TIME__`) | El commit y su fecha («Compilación bf10043, 2026-11-13»), que no rompen los paquetes reproducibles; y la versión de Qt |
| Camaleón claro u oscuro | El ícono de la aplicación, con transparencia (el logo con texto tiene el fondo negro), y el nombre con el color del tema |
| El texto de la licencia | La pestaña **Licencia**, con la GPL v3 completa |
| El enlace a la página | Enlaces a la página, el manual, reportar un problema, el catálogo de plugins y las notas de esta versión |
| — | La pestaña **Componentes**: Qt, Scintilla, Lexilla, Boost.Regex, uchardet, pugixml, libvterm, md4c, OpenSSL y Notepad++, con versión, para qué se usa, licencia y su texto |
| — | Si hay una versión nueva |
| — | Los plugins cargados y **Copiar la información de la versión** |

## Qué se hizo

- **`core/AboutInfo`**, sin interfaz:
  - **Los componentes.** Cada uno tiene versión, licencia (SPDX), página, para qué se usa y el recurso con el texto de su licencia.
  - **Las versiones.** Salen de `third_party/` al configurar (`generated/ThirdPartyVersions.h`): `version.txt` de Scintilla y Lexilla, `PUGIXML_VERSION`, `BOOST_LIB_VERSION`, `VTERM_VERSION_*` y el commit de Notepad++ de `UPSTREAM.txt`. Las de Qt y OpenSSL salen de las bibliotecas con las que se ejecuta.
  - **`detectInstall`:** con `$APPIMAGE` es el AppImage. En `/opt/edita` es el .deb si dpkg lo registró, y si no el .rpm (los dos paquetes llevan el AppDir ahí). En `/usr` es un paquete del sistema. En cualquier otro lugar, compilado.
  - **Los textos:** la línea de la versión, el texto que se copia y la URL de las notas.
- **`cmake/BuildInfo.cmake`** escribe el commit y su fecha en `generated/BuildInfo.h` en cada compilación, y solo si cambiaron. Sin git, los dos quedan vacíos y la línea no aparece.
- **Recursos:** `COPYING` y los avisos de licencia de Scintilla, Lexilla, pugixml, libvterm y md4c van dentro del binario. Para Boost, que no trae el suyo en el árbol, va `resources/licenses/BSL-1.0.txt`.
- **`ui/AboutDialog`**, con tres pestañas: **Acerca de**, **Componentes** (doble clic abre la página del componente) y **Licencia**. El botón **Copiar la información de la versión** copia la versión, la compilación, el sistema y los plugins.
- **Versiones nuevas.** `MainWindow` guarda el resultado de la última búsqueda (`_latestRelease`) y avisa con la señal `releaseChecked`. Al abrir **Acerca de**:
  - si ya se buscó en la sesión, lo muestra;
  - si no, busca sin la barra de avisos (`_updateQuiet`);
  - si la búsqueda está desactivada (`EDITA_UPDATE_CHECK`, `EDITA_UPDATE_URL=none`), lo dice.
- **Traducción y manual:** la traducción al español y el manual (Ayuda, en la introducción), en los dos idiomas.

## Pruebas

`tst_about` (7):

- **Los componentes:** las versiones esperadas de cada uno (que salen de `third_party/`), y que los avisos de licencia y la GPL están dentro del binario.
- **La forma de instalación:** AppImage, .deb, .rpm, sistema y compilado.
- **Los textos:** la línea de la versión, el texto copiado y la URL de las notas.
- **El diálogo:**
  - las tres pestañas, y la versión con la arquitectura;
  - la búsqueda desactivada;
  - el enlace a las notas y copiar;
  - elegir Scintilla muestra su licencia, y Qt, el enlace a la suya;
  - la GPL en la pestaña Licencia.
- **Versión nueva en el diálogo:**
  - con una respuesta falsa, muestra «Edita 99.0.0 está disponible» y la página de descarga, sin la barra de avisos;
  - la segunda vez no vuelve a preguntar;
  - con una versión más vieja, «Esta es la versión más reciente».

`tst_uipolish` se adaptó al diálogo nuevo.
