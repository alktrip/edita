# Fase 54 — Las primeras recetas del catálogo

Fecha: 2026-11-03

## Por qué

Con la [fase 53](fase53.md), la capa de compatibilidad compila con su interfaz los plugins de la clase A de la [auditoría](https://github.com/alktrip/edita-plugins/tree/main/audit). El siguiente paso es empaquetarlos: tener sus recetas en el catálogo de plugins ([alktrip/edita-plugins](https://github.com/alktrip/edita-plugins)) y una forma de compilarlas y probarlas sin la interfaz, que es lo que necesita la CI que las publicará (fase 55).

## Qué se construyó

| Parte | Qué hace |
|---|---|
| **Siete recetas** (`edita-plugins/recipes/`) | Los plugins de la clase A con una licencia que permite redistribuirlos: Reverse Lines, NppPluginOpenHost, Select 'N' Launch, Simplify JSON, TidyText, Auto Detect Indention y SQL Column Highlighter. Cada una fija la etiqueta que publicó el autor con su SHA-256, quita su `Scintilla.h` y compila los fuentes originales con un `CMakeLists.txt` de unas líneas (`edita_npp_plugin(… UNICODE SOURCES … RESOURCES …)`). Los BatPathIntelliSense y NppExport, también de la clase A, no declaran licencia y quedan fuera |
| **`edita --build-recipe receta.json --output carpeta`** | Compila una receta sin abrir la interfaz: descarga el código y sus fuentes adicionales (las rutas locales son relativas a la receta), comprueba los SHA-256 y compila con el `RecipeBuilder` del administrador de plugins. Deja `<carpeta>-<versión>.zip`, su `.sha256` (en el formato de `sha256sum`) y la compilación en `work/` |
| **`edita --check-plugin paquete.zip --recipe receta.json`** | Instala el paquete en una configuración temporal (la del usuario no se toca), abre Edita sin ventana visible (`QT_QPA_PLATFORM=offscreen` si no se indica otra), carga el plugin, lista sus órdenes y ejecuta la prueba de la receta. Los diálogos modales que abra el plugin se aceptan solos y su texto sale en la salida. Termina con código 0 si la prueba pasa |
| **`"check"`** en la receta | La prueba: órdenes que tienen que estar en el menú del plugin (`commands`) y casos (`cases`). Cada caso pone un texto en un documento nuevo, lo selecciona entero o deja el cursor al principio (`"select": "none"`), ejecuta la orden y compara el documento con `expect`, o el archivo abierto con `expect-file`. Si falla, muestra lo esperado y lo obtenido, con los saltos de línea y los tabuladores visibles |
| **`"extra-sources"`** | Más `.zip` fijados con su SHA-256, que se copian dentro del código del autor (`dest`) antes de `remove` y de los parches. Son los submódulos de git, que GitHub no incluye en el `.zip` de una etiqueta: Auto Detect Indention incluye la plantilla de plugins de N++ como submódulo |
| **`"license"` y `"license-files"`** | La licencia del plugin (SPDX) y los archivos del código que van en el paquete junto al `.so`. Así cada binario que se publique lleva la licencia de su autor |
| **`scripts/build-all.sh`** (edita-plugins) | Compila y prueba todas las recetas (o las indicadas) con un ejecutable de Edita, y termina con error si alguna falla. Es lo que ejecutará la CI |

## Los plugins

| Plugin | Licencia | Prueba | Parches |
|---|---|---|---|
| Reverse Lines 1.0.0.0 | GPL-3.0-or-later | `Selection` y `Document` invierten las líneas | 1 línea (ver abajo) |
| NppPluginOpenHost 1.1 | Apache-2.0 | `Open` abre `/etc/hosts` | 1 línea: la ruta del archivo hosts, que en el original es la de Windows |
| Select 'N' Launch 2.2 | GPL-2.0-or-later | Sus órdenes (abren aplicaciones externas: no se ejecutan en la prueba) | — |
| Simplify JSON 1.0.1 | GPL-2.0-only | `Beautify` y `Minify` sobre el documento; `Minify`, `Escape` y `Unescape` sobre la selección | 3 líneas (ver abajo) |
| TidyText 1.2.0 | GPL-2.0-or-later | `Replace Non-Printing Characters` cambia el espacio duro y el mojibake (`cafÃ©` → `café`) y respeta las comillas tipográficas y la raya | 2 líneas (ver abajo) |
| Auto Detect Indention 2.3 | GPL-3.0-or-later | Sus órdenes (trabaja al cambiar de documento) | — |
| SQL Column Highlighter 1.0.0 | MIT | Sus órdenes (trabaja con el ratón) | — |

**El NUL de `SCI_GETSELTEXT`.** Al probar Simplify JSON, `Beautify` sobre una selección perdía el último carácter. El plugin pide el tamaño con `SCI_GETSELTEXT(0, NULL)` y da por hecho que incluye el NUL final, como en versiones antiguas de Scintilla; en la actual (la serie 5, la de Notepad++ y la de Edita) no lo incluye, pero lo sigue escribiendo. El mismo fallo, en Notepad++ también:

- **Simplify JSON** recorta un carácter de más: pierde el último de la selección.
- **Reverse Lines** reserva `longitud` bytes y Scintilla escribe `longitud + 1`: un byte fuera del búfer.
- **TidyText**, en *Send Selection to…*, igual que Reverse Lines.

Los tres parches reservan el byte del NUL y no cambian nada más. La capa no puede corregirlo por su cuenta, porque el valor de `SCI_GETSELTEXT` es el de Scintilla, que también usan los plugins nativos. Select 'N' Launch reserva bien el búfer.

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| NppPluginOpenHost | Abre `C:\Windows\system32\drivers\etc\hosts` | Abre `/etc/hosts` | Es el archivo hosts de Linux y macOS (parche) |
| Reverse Lines, Simplify JSON, TidyText | El fallo de `SCI_GETSELTEXT` | Corregido | Parches de la receta |
| Prueba de un plugin | No hay | `--check-plugin` | Para la CI del catálogo |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Las siete recetas compilan con `--build-recipe` desde el código del autor (y el submódulo de Auto Detect Indention), con los parches aplicados, y sus paquetes llevan el `.so` y la licencia | ✅ `scripts/build-all.sh` |
| Los siete plugins cargan en Edita con `--check-plugin`, sus órdenes están en el menú y pasan sus casos | ✅ `scripts/build-all.sh`: «Las 7 recetas compilan y pasan su prueba» |
| `extra-sources`: verifica el SHA-256, se copia en `dest` antes de `remove` y la receta falla con un SHA-256 distinto, con menos `.zip` de los que pide o con un `dest` que sale del código | ✅ `tst_recipes`: `recipeCommandsBuildAndCheck` |
| `--build-recipe` con rutas relativas a la receta deja el paquete y su `.sha256`; el paquete lleva `license-files` | ✅ `recipeCommandsBuildAndCheck` |
| `--check-plugin`: pasa con una prueba buena; con una orden que no existe y un caso que no pasa termina con 1 y muestra lo esperado y lo obtenido; no cambia la configuración de quien lo llama | ✅ `recipeCommandsBuildAndCheck` |
| Las órdenes sin sus argumentos muestran el uso y terminan con 2; los demás argumentos abren Edita como siempre | ✅ `recipeCommandsBuildAndCheck` |
| Cadenas nuevas traducidas al español | ✅ `edita_es.ts` |

Pasan las 51 suites, y el build no da ningún warning ni con GCC ni con clang.

## Pendiente para la fase 55

- **Paquetes reproducibles.** Dos compilaciones de la misma receta dan `.zip` distintos (las fechas del `.zip` y las rutas absolutas dentro del `.so`). No impide publicar, porque la lista firmada llevará el SHA-256 del paquete que publique la CI, pero conviene fijarlos (`SOURCE_DATE_EPOCH`, `-ffile-prefix-map`) para que cualquiera pueda comprobar un binario.
- **La licencia de la capa en los binarios.** La capa de compatibilidad (`plugin-sdk/compat`) es GPL-3.0-or-later y queda dentro de cada `.so`. Es compatible con seis de los siete plugins, pero no con Simplify JSON, que se publica como «GPLv2» sin «o posterior». Compilarlo en el equipo del usuario no es distribuirlo, así que la receta sirve. Para publicar su binario, hay que licenciar la capa también como GPL-2.0-or-later (o con una excepción para plugins), o que el autor aclare la versión.

## Siguiente

La fase 55: la CI de `edita-plugins` compila las recetas para Linux x86_64 y macOS con `scripts/build-all.sh`, publica los paquetes en los *Releases* y genera la lista de plugins firmada. Después, en la fase 56, el administrador de plugins de Edita descarga esa lista, verifica la firma e instala el binario de la plataforma, o compila la receta si no hay binario.
