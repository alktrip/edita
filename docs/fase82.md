# Fase 82 — La API de plugins de la 1.x

Fecha: 2026-11-12

## Por qué

Con la 1.0, los plugins pasan a ser algo con lo que otros cuentan. Un plugin compilado para la 1.0 tiene que seguir cargando en la 1.1, la 1.2… sin volver a compilarlo. Hasta ahora eso dependía de que nadie tocara `edita_plugin.h` sin pensarlo. Desde esta fase queda escrito (qué no cambia) y comprobado (una prueba que falla si cambia).

## Cómo lo hace Notepad++

N++ no tiene una versión de la API, pero no cambia los números: `Notepad_plus_msgs.h` conserva los mensajes viejos con su número y el sufijo `_DEPRECATED` (`NPPM_GETOPENFILENAMES_DEPRECATED`, `NPPM_DESTROYSCINTILLAHANDLE_DEPRECATED`, `NPPM_ADDTOOLBARICON_DEPRECATED`…). Un mensaje que N++ no conoce devuelve 0, y los plugins usan `NPPM_GETNPPVERSION` para saber si tienen uno nuevo.

| N++ | Edita 1.x |
|---|---|
| Los números no cambian; lo viejo queda como `_DEPRECATED` | Igual. Además, lo dice `docs/plugins.md` y lo comprueba `tst_pluginapi` |
| Sin versión de la API | `EDITA_PLUGIN_API_VERSION` sigue en 1 toda la 1.x; un plugin de otra API no carga |
| Mensaje desconocido → 0 | Igual (comprobado) |
| `NPPM_GETNPPVERSION` | Igual: `(mayor << 16) \| menor` |

## Qué se hizo

**La promesa** está en `docs/plugins.md` («Qué no cambia en Edita 1.x»), en el comentario de `edita_plugin.h` y, en corto, en el manual (Plugins › Escribir plugins, en los dos idiomas). En la 1.x no cambian las funciones exportadas, los números y parámetros de `NPPM_*`/`NPPN_*`, las demás constantes, las estructuras (`EditaData`, `EditaFuncItem`, `EditaShortcutKey`, `EditaCommunicationInfo`, `EditaDockData`) ni los números de lenguaje y codificación. Solo se añaden cosas. La capa Win32 para plugins de N++ queda fuera: va dentro de cada `.so`, y al volver a compilar desde la receta se usa la de esa versión.

**La prueba, `tst_pluginapi`:**

- `kDefines` tiene los 122 `#define` de `edita_plugin.h` con su valor de la 1.0. La prueba compara cada uno con el valor que da hoy el preprocesador.
- Lee `edita_plugin.h` y falla si hay un `#define` que no está en la tabla. Así, lo que se añade al SDK queda también congelado desde ese momento.
- `static_assert` con el tamaño y la posición de cada campo de las estructuras (x86_64), y de `SCNotification`: es de Scintilla, pero llega tal cual a `beNotified`. Si una Scintilla nueva la cambia, la compilación falla y esa Scintilla no entra en la 1.x.
- Los 98 números de lenguaje (`L_TEXT` 0 … `L_EXTERNAL` 97) y los 8 de codificación.
- Un mensaje desconocido devuelve 0, y `NPPM_GETNPPVERSION` da la versión de Edita.

Para comprobar la prueba, se cambió el número de `NPPM_SWITCHTOFILE` y se añadió una notificación sin anotarla: las dos comprobaciones fallaron, con el nombre de lo que cambió.

## Pruebas

68 suites. `tst_pluginapi` también se compila con clang (no depende de `EDITA_BUILD_PLUGINS`).
