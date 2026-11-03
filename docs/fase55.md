# Fase 55 — El catálogo de plugins: CI, releases y lista firmada

Fecha: 2026-11-03

## Por qué

Con la [fase 54](fase54.md), las recetas de los primeros siete plugins están en [alktrip/edita-plugins](https://github.com/alktrip/edita-plugins) y se compilan y prueban sin la interfaz. Falta que una máquina lo haga sola y publique los binarios y una lista que Edita pueda creer: la que el administrador de plugins descargará en la fase 56.

El desarrollo se centra en Linux (macOS se retoma cuando la aplicación esté completa en Linux), así que el catálogo es para Linux x86_64.

## Qué se construyó

| Parte | Qué hace |
|---|---|
| **Release continuo de Edita** (`.github/workflows/linux.yml`) | Cada push a `main` que pasa las pruebas deja su AppImage en el release `continuous` (marcado como *prerelease*). Lo usa la CI del catálogo y sirve para probar la versión en desarrollo. Dos pushes seguidos no se pisan (`concurrency`) |
| **Paquetes reproducibles** (`EditaNppPlugin.cmake`, `createZip`) | El SDK compila los plugins con `-ffile-prefix-map` (las carpetas del SDK, del código, de la compilación, de Scintilla y del Qt de Edita pasan a nombres fijos) y sin RUNPATH. `createZip` usa `SOURCE_DATE_EPOCH` como fecha de todas las entradas. El `RecipeBuilder` le pasa a CMake la raíz del código (`EDITA_PLUGIN_SOURCE_ROOT`). Resultado: la misma receta, compilada en dos carpetas distintas, da el mismo `.zip` byte a byte |
| **`scripts/catalog.py`** (edita-plugins) | Genera la lista (`list`), empaqueta las recetas en un `.zip` reproducible (`pack-recipes`), decide si hay que publicar (`changed`), y crea, firma y verifica con Ed25519 en el formato de minisign (`keygen`, `sign`, `verify`). Solo publica binarios de licencias compatibles con la GPL-3. Sus pruebas: `python3 -m unittest discover -s scripts` |
| **`.github/workflows/catalog.yml`** (edita-plugins) | Descarga el AppImage del release continuo de Edita, prueba los scripts, compila y prueba todas las recetas con `scripts/build-all.sh`, genera la lista y, si algo cambió respecto al último catálogo, la firma, la comprueba con `catalog.py` y con `minisign`, y publica el release `catalog-<fecha>.<n>` con los paquetes, sus `.sha256`, `recipes.zip`, `plugins.json`, su firma y la clave pública. En los pull requests solo compila y prueba. Los lunes recompila con el Edita más reciente |
| **La clave del catálogo** | La pública está en `edita-plugins/keys/catalog.pub` (y va en cada release); la secreta, en el secreto `EDITA_CATALOG_KEY` del repositorio y en una copia fuera de él |

## La lista

`https://github.com/alktrip/edita-plugins/releases/latest/download/plugins.json`, con su firma en `plugins.json.minisig`. Es el formato de `nppPluginList.json` de Notepad++, que el administrador de plugins ya lee, con campos de Edita:

```json
{
	"name": "Edita plugin catalog",
	"version": "2026.11.03.12",
	"arch": "linux-x86_64",
	"edita-plugin-api": 1,
	"edita-qt": "6.8.3",
	"edita-recipes": "https://github.com/alktrip/edita-plugins/releases/download/catalog-2026.11.03.12/recipes.zip",
	"edita-recipes-sha256": "…",
	"npp-plugins": [
		{
			"folder-name": "qkNppReverseLines", "display-name": "Reverse Lines", "version": "1.0.0.0",
			"id": "<SHA-256 del paquete>",
			"repository": "https://github.com/alktrip/edita-plugins/releases/download/catalog-2026.11.03.12/qkNppReverseLines-1.zip",
			"license": "GPL-3.0-or-later", "edita-build": "binary",
			"edita-recipe": "recipes/qkNppReverseLines/recipe.json", "…": "…"
		},
		{ "folder-name": "SimplifyJson", "id": "", "repository": "", "license": "GPL-2.0-only",
		  "edita-build": "recipe", "edita-recipe": "recipes/SimplifyJson/recipe.json", "…": "…" }
	]
}
```

- `edita-build`: `binary` si hay paquete, o `recipe` si el plugin se compila en el equipo del usuario a partir de su receta (que está en `recipes.zip`, cuyo SHA-256 va en la lista firmada).
- `edita-plugin-api` y `edita-qt`: la API de plugins y el Qt con los que se compilaron los binarios. Un Edita con otra API o con un Qt más antiguo no debe instalarlos (fase 56).
- `version`: aumenta con cada catálogo; en la fase 56 servirá para no aceptar una lista más antigua que la que ya se tiene.

**La firma.** Ed25519 sobre el BLAKE2b-512 del archivo, en el formato de minisign. El comentario de confianza (firmado también) lleva la fecha, el archivo y la versión del catálogo. Se comprueba con:

```sh
minisign -Vm plugins.json -p catalog.pub
python3 scripts/catalog.py verify --public keys/catalog.pub plugins.json
```

**Las licencias.** La capa de compatibilidad del SDK (GPL-3.0-or-later) va dentro de cada `.so`, así que solo se publican binarios de licencias compatibles con la GPL-3: GPL-2.0-or-later, GPL-3.0, LGPL, AGPL-3.0, Apache-2.0, MIT, BSD, ISC, Zlib, MPL-2.0 y otras permisivas (la lista está en `catalog.py`). Una licencia que no está en la lista no impide tener la receta: el plugin se compila en el equipo del usuario, que no es distribuirlo. Es el caso de Simplify JSON, publicado como «GPLv2» aunque incluye los archivos de la interfaz de N++, que son GPL-3 o posterior (su autor creyó que la plantilla de N++ era GPLv2). Si aclara la licencia, pasa a binario.

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Lista de plugins | `nppPluginList.json` dentro de una DLL firmada con Authenticode | `plugins.json` con una firma minisign aparte | No hay Authenticode en Linux; minisign es un formato abierto que cualquiera puede comprobar |
| Binarios | Los compila y publica cada autor | Los compila la CI del catálogo desde el código del autor, y son reproducibles | No hay binarios de los plugins para Linux; así cada binario se puede comprobar |
| Plugins sin binario | No se listan | Se listan y se compilan en el equipo | La receta funciona aunque la licencia no permita distribuir el binario |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| La misma receta compilada en dos carpetas da el mismo paquete; el `.so` no tiene las carpetas de trabajo ni del SDK, ni RUNPATH | ✅ `tst_recipes`: `packagesAreReproducible`; a mano, Reverse Lines, TidyText y Auto Detect Indention en dos carpetas: los mismos SHA-256 y ninguna ruta de la máquina en el `.so` |
| Con `SOURCE_DATE_EPOCH`, `createZip` no depende de la fecha de los archivos; sin ella, sí | ✅ `packagesAreReproducible` |
| La firma: el vector 1 del RFC 8032, ida y vuelta, el formato de minisign (algoritmo `ED`, identificador, clave pública), y se rechazan otro contenido, otro comentario de confianza y otra clave | ✅ `test_catalog.py` (8 pruebas); en la CI, también con `minisign` |
| La lista: binario solo con licencia compatible y paquete; los campos que exige Edita son cadenas; un paquete que no coincide con su `.sha256` es un error; `recipes.zip` es reproducible; `changed` ve los paquetes nuevos y no la versión del catálogo | ✅ `test_catalog.py` |
| Ensayo del flujo completo con la clave real: las 7 recetas compilan y pasan, la lista sale con 6 binarios y Simplify JSON como receta, la firma se verifica | ✅ local |
| El AppImage del release continuo compila y prueba las recetas | ✅ local con el AppImage de la CI |
| Pasan todas las suites y el build no da avisos (GCC y clang) | ✅ |

## Pendiente

- **El secreto `EDITA_CATALOG_KEY`** en el repositorio edita-plugins: lo tiene que poner el dueño del repositorio (`gh secret set EDITA_CATALOG_KEY -R alktrip/edita-plugins < ~/.local/share/edita-catalog/catalog.key`). Sin él, la CI compila y prueba, pero no publica.
- **Una copia de la clave secreta** fuera de esta computadora: si se pierde, hay que generar otra y publicar una versión de Edita con la nueva clave pública.

## Siguiente

La fase 56: el administrador de plugins descarga `plugins.json` y su firma, la comprueba con la clave pública del catálogo incluida en Edita, rechaza listas más antiguas que la que ya tiene, e instala el binario (comprobando su SHA-256, la API y la versión de Qt) o, si el plugin es `recipe`, compila la receta de `recipes.zip`.
