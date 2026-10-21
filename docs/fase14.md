# Fase 14 — Administrador de plugins: resultado

Fecha: 2026-10-20 · Entorno: el mismo que en las fases anteriores, más libzip 1.11 (opcional) y Qt Network

Objetivo: instalar plugins como lo hace el **Plugins Admin** de Notepad++ (`WinControls/PluginsAdmin/pluginsAdmin.cpp` y el actualizador `gup.exe`). Hay que tener en cuenta que los plugins del catálogo oficial de N++ son DLL de Windows. Se decidió:

- copiar el mecanismo de N++ (formato de la lista, pestañas, comprobaciones e instalación);
- usar un catálogo propio de plugins `.so` para Edita;
- consultar también la lista oficial de N++, cuyos plugins se muestran como incompatibles.

## Qué se construyó

| Módulo | Equivale en N++ a | Qué hace |
|---|---|---|
| `core/PluginCatalog` | `Version` (`Common.cpp`), `getIntervalVersions`, `loadFromJson`, `PluginUpdateInfo::describe`, `PluginsAdminDlg::updateList` | **Versiones** de hasta 4 números, con la misma lectura, escritura y comparación, y la compatibilidad por intervalos (`"8.5"`, `"[4.2,6.6.6]"`, `"[8.3,]"`, `"[,8.2.1]"`). La **lista de plugins** tiene el formato de `nppPluginList.json`: `version` y `npp-plugins` con `folder-name`, `display-name`, `version`, `npp-compatible-versions`, `id` (SHA-256), `repository`, `description`, `author` y `homepage`; si falta un campo obligatorio, el plugin se ignora. El **reparto en pestañas** sigue a N++: Disponibles (compatibles con la versión de Edita y no instalados), Actualizaciones (instalados con versión menor que la de la lista), Instalados (con los datos de la lista si están en ella), Incompatibles y Desactivados |
| `core/PluginInstaller` | `gup.exe -unzipTo`, `-clean`, `-moveFolder` | Compara el SHA-256 del paquete con el `id`, lo descomprime con libzip (rechaza rutas absolutas o con `..`) y comprueba que esté `<carpeta>.so`, en la raíz o dentro de una carpeta con ese nombre. Lo deja en `plugins/<carpeta>/`; al actualizar borra antes la versión anterior. También quita, desactiva (a `plugins/disabled/`) y activa |
| `ui/PluginsAdminDialog` | `PluginsAdminDlg` (`IDD_PLUGINSADMIN_DLG`) | Búsqueda con «Siguiente» (al menos dos letras, primero en los nombres y luego en las descripciones), los botones Instalar, Actualizar, Desactivar, Activar y Quitar según la pestaña, cinco listas con casillas (Plugin y Versión, por nombre), la descripción del seleccionado y la versión de la lista. Además, **«Lista de plugins de Notepad++…»** descarga (tras confirmarlo) la lista oficial y pone sus plugins en Incompatibles, explicando por qué |
| `data/plugins/editaPluginList.json` | `nppPluginList.json` (en N++, dentro de `nppPluginList.dll`) | La lista de Edita: Python Script y Demo, con `repository` vacío porque vienen con Edita. Se puede sustituir por `~/.config/edita/plugins/Config/editaPluginList.json`. Un `repository` puede ser `https://`, `file://` o una ruta relativa a la lista |
| Versión de un plugin | Recurso de versión de la DLL (`Version::setVersionFrom`) | Exportación opcional `edita_plugin_version()` (documentada en el SDK); Python Script devuelve 1.0.0 y Demo 1.0 |
| Menú y reinicio | `IDM_SETTING_PLUGINADM`, salida a gup y relanzamiento | **Plugins > Administrador de plugins…** es el primer elemento, como en N++. Las operaciones se hacen en el acto (en Linux se puede sustituir un `.so` cargado). Al cerrar, si hubo cambios, Edita ofrece reiniciarse: guarda la sesión, pregunta por lo no guardado y se vuelve a lanzar con los mismos argumentos |
| `PluginManager` | `PluginsManager` | Guarda la versión exportada por cada plugin y, aparte, los que no se pudieron cargar y por qué (pestaña Incompatibles) |

Dónde se instala: en `~/.config/edita/plugins/` (el `%APPDATA%` de N++). Al cargar, esa carpeta tiene prioridad sobre la de la instalación. Así, actualizar un plugin que vino con Edita deja la versión nueva del usuario delante. Los plugins instalados con Edita (`<prefijo>/lib/edita/plugins`) se ven en Instalados, pero no se pueden quitar ni desactivar desde el diálogo. En N++ eso requiere modo administrador.

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Ejecución | Sale y deja el trabajo a `gup.exe`, que reinicia N++ | Lo hace en el acto y ofrece reiniciar al cerrar el diálogo | En Linux se puede sustituir un `.so` cargado; no hace falta un actualizador aparte |
| Lista | `nppPluginList.dll` firmada, incluida en cada versión de N++ | `editaPluginList.json` (sin firma), sustituible por el usuario. La seguridad de cada paquete la da su SHA-256 | Edita no tiene infraestructura de firma |
| Lista oficial de N++ | Es la única lista | Se consulta a petición; sus plugins van a Incompatibles | Son DLL de Windows |
| Zips | gup los descomprime siempre | Requiere libzip al compilar; sin ella, la instalación de paquetes se desactiva con un aviso | Dependencia opcional |
| Lugar de instalación | Program Files (admin) o `%APPDATA%` | La carpeta del usuario | Linux: el usuario no escribe en `/usr` |
| `old-versions-compatibility` | La usa el gestor al cargar plugins | Se lee pero no se usa | No hay versiones antiguas de plugins para Edita |

## Para quien publique plugins

Un paquete es un `.zip` con `<Nombre>.so` (y lo que necesite) en la raíz o dentro de `<Nombre>/`. Basta con añadir una entrada a una lista con el formato de `editaPluginList.json`:

```json
{ "folder-name": "MiPlugin", "display-name": "Mi plugin", "version": "1.2",
  "npp-compatible-versions": "[0.0.1,]", "id": "<sha256sum del .zip>",
  "repository": "https://ejemplo.org/MiPlugin-1.2.zip",
  "description": "…", "author": "…", "homepage": "…" }
```

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Versiones: lectura, escritura (`2.0.0.0` → «2»), formatos no válidos, comparación y los cuatro tipos de intervalo; una versión vacía no es compatible | ✅ `tst_pluginsadmin`: `versionsLikeNotepadPlusPlus` |
| La lista se lee con el formato de N++, se ignoran los plugins incompletos, y el reparto en pestañas es el de N++ (compatibilidad, versión instalada, actualizaciones, desactivados, los que no cargan) | ✅ `catalogAndLists` |
| Instalador: SHA-256 (sin distinguir mayúsculas), carpeta del plugin, actualizar con limpieza, paquete dentro de una carpeta, sin `.so`, ruta con `..` rechazada, zip no válido, mover y quitar | ✅ `installerChecksAndExtracts` |
| El menú empieza con el administrador; los plugins del build se ven instalados con su versión y los datos de la lista, y no se pueden quitar desde el diálogo | ✅ `menuAndInstalledPlugins` |
| Instalar desde una lista (un paquete con SHA-256 malo falla y otro se instala), cancelar, desactivar, activar, actualizar a una versión nueva y quitar | ✅ `installUpdateDeactivateRemove` |
| Un plugin que no carga aparece en Incompatibles y se puede quitar; la lista de N++ va a Incompatibles con el motivo; búsqueda por nombre y descripción | ✅ `incompatibleAndNotepadPlusPlusList` |
| Sin cambios no se ofrece reiniciar | ✅ `restartIsOfferedAfterChanges` |

Además:

- Se descargó la **lista oficial real de Notepad++** desde GitHub (versión 1.9.9, 214 plugins). Se lee sin errores y todos sus plugins aparecen en Incompatibles con su descripción.
- Se revisó en capturas el diálogo con las pestañas Instalados e Incompatibles.

Tests: `tst_pluginsadmin` 9 (contando `initTestCase` y `cleanupTestCase`). `tst_plugins` se adaptó al nuevo primer elemento del menú. Pasan las 22 suites y el build no da ningún warning.
