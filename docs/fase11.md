# Fase 11 — Plugins: resultado

Fecha: 2026-10-20 · Entorno: el mismo que en las fases anteriores

## Qué se construyó

| Módulo | Equivale en N++ a | Qué hace |
|---|---|---|
| `plugin-sdk/edita_plugin.h` | `PluginInterface.h`, `Notepad_plus_msgs.h` | API en C (también se puede usar desde C++) con las funciones exportadas de N++ (`setInfo`, `getName`, `getFuncsArray`, `beNotified`, `messageProc`), `FuncItem`/`ShortcutKey`, y los mismos números de `NPPM_*` y `NPPN_*`. `::SendMessage` se sustituye por `data.send(destino, mensaje, w, l)`, y las cadenas son UTF-8. Se instala con `Scintilla.h` y `Sci_Position.h` |
| `ui/PluginManager` | `PluginsManager` (`loadPlugins`, `notify`, `runPluginCommand`) | Carga `<carpeta>/<Nombre>/<Nombre>.so` desde `$EDITA_PLUGIN_PATH`, `~/.config/edita/plugins`, junto al binario y `<prefijo>/lib/edita/plugins`. Comprueba las funciones exportadas, la versión de la API y que no haya nombres repetidos. Llama a `setInfo` antes de pedir las órdenes, asigna los `cmdID` (desde 22000, como `ID_PLUGINS_CMD`) y convierte los atajos |
| `MainWindow::handlePluginMessage` | `Notepad_plus::process` (`NppBigSwitch.cpp`) | Contesta unos 30 mensajes `NPPM_*`: vista y documento actual, lenguaje, número de documentos, rutas y partes del nombre, palabra, línea y columna, abrir, cambiar de documento, recargar, guardar, órdenes de menú (con los `IDM_*` de N++ y las de los plugins), marcar órdenes, barra de estado, carpeta de configuración y versión. Los mensajes `SCI_*` van directos a la vista de Scintilla indicada |
| Notificaciones | `_pluginsManager.notify(…)` en `NppNotification.cpp` y `NppIO.cpp` | Del editor: `NPPN_READY`, `FILEOPENED`, `FILEBEFORECLOSE`, `FILECLOSED`, `FILEBEFORESAVE`, `FILESAVED`, `BUFFERACTIVATED`, `LANGCHANGED`, `READONLYCHANGED`, `BEFORESHUTDOWN`, `CANCELSHUTDOWN` y `SHUTDOWN`. Además, todas las `SCN_*` de las dos vistas |
| Menú **Plugins** | `PluginsManager::setMenu` | Un submenú por plugin, con separadores, atajos y órdenes marcables. También **Abrir la carpeta de plugins** e **Información de los plugins…**, que muestra los cargados, los que fallaron (con el motivo) y las carpetas. Las órdenes de plugin se graban en las macros |
| `plugins/EditaDemo/EditaDemo.c` | "Plugin Template"/"Plugin Demo" de N++ | Plugin de ejemplo en C con: "Hola, Edita" (`NPPM_MENUCOMMAND` y `SCI_REPLACESEL`), insertar fecha y hora (Ctrl+Alt+D), ordenar líneas (con un solo deshacer), contar palabras (`NPPM_SETSTATUSBAR`) e "Informar al guardar" (`NPPM_SETMENUITEMCHECK`, y en `NPPN_FILESAVED` usa `NPPM_GETFULLPATHFROMBUFFERID`). Se compila con el proyecto (`EDITA_BUILD_PLUGINS`) y se instala en `lib/edita/plugins/EditaDemo/` |
| `docs/plugins.md` | — | Guía para escribir o portar plugins: tabla de equivalencias con N++, mensajes y notificaciones disponibles, y cómo compilar e instalar |
| `third_party/patches/0004-scintilla-h-c-compatible.patch` | — | La estructura de C++ que N++ añadió a `Scintilla.h` queda dentro de `#ifdef __cplusplus`, para que los plugins en C puedan incluirla |

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Binarios | DLL de Windows | `.so` de Linux | **Los plugins de N++ no se pueden cargar**: son código Win32. Se pueden portar con poco trabajo (ver `docs/plugins.md`) |
| Cadenas | `wchar_t` (UTF-16) | UTF-8 | En Linux `wchar_t` es de 32 bits y nadie lo usa; Scintilla ya trabaja en UTF-8 |
| `SendMessage` | Función de Win32 sobre `HWND` | Puntero `data.send(destino, …)` | Misma forma, sin ventanas |
| Versión de la API | `isUnicode()` | `edita_plugin_api_version()` | Rechaza plugins incompatibles |
| Mensajes | Más de 100 | El subconjunto con equivalente en Edita; el resto devuelve 0 | Barra de herramientas, paneles acoplables, Plugins Admin, modo oscuro… no existen |
| Plugins Admin (descarga), barra de herramientas para plugins (`NPPN_TBMODIFICATION`), paneles acoplables | Sí | No | Fuera de alcance |
| Aislamiento | Ninguno (un plugin puede tirar N++) | Igual | Mismo modelo de confianza: el usuario instala lo que quiere |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| El plugin de ejemplo se carga desde `$EDITA_PLUGIN_PATH` y aparece en su submenú, con órdenes, separador y atajo | ✅ `loadsTheDemoPluginAndBuildsItsMenu` |
| Sus órdenes funcionan en las dos vistas mediante mensajes `SCI_*` y `NPPM_*`, y ordenar se deshace de una vez | ✅ `pluginCommandsUseScintillaAndHostMessages` |
| Las órdenes marcables (`NPPM_SETMENUITEMCHECK`) y la reacción a `NPPN_FILESAVED` | ✅ `checkedItemsAndSaveNotification` |
| Llegan las notificaciones del editor (apertura, activación, lenguaje, guardado, cierre, `READY`, cierre de la ventana) y las `SCN_*` | ✅ `notificationsReachThePlugin` |
| Los mensajes `NPPM_*` devuelven lo mismo que en N++ (con el búfer pequeño devuelven 0, y `MENUCOMMAND` acepta un `IDM_*` de N++) | ✅ `hostMessages` |
| Una orden de plugin se graba en una macro como orden, no como sus mensajes, y se reproduce | ✅ `pluginCommandsCanBeRecordedInMacros` |
| Plugins rotos o con nombre repetido se informan sin cargar | ✅ `brokenAndDuplicatePluginsAreReported` |

Además se comprobó que:
- El menú del plugin se ve bien en una captura.
- La instalación deja el plugin en `lib/edita/plugins/` y el SDK en `include/edita/`.
- Un plugin en C y otro en C++ compilan solo con las cabeceras instaladas.
- El binario instalado arranca.

Tests: `tst_plugins` 9 (contando `initTestCase` y `cleanupTestCase`). `tst_mainwindow` se ha adaptado al menú Plugins. Pasan las 18 suites y el build no da ningún warning.
