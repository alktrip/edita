# Fase 56 — El administrador de plugins usa el catálogo firmado

Fecha: 2026-11-03

## Por qué

Con la [fase 55](fase55.md), el catálogo de plugins ([alktrip/edita-plugins](https://github.com/alktrip/edita-plugins)) publica sus paquetes y una lista firmada, pero Edita todavía no la usaba: el administrador de plugins solo conocía la lista que trae Edita y las recetas instaladas con él. Esta fase cierra el ciclo: un plugin nuevo en el catálogo aparece en Edita sin publicar una versión de Edita.

## Cómo lo hace Notepad++

N++ trae la lista de plugins en `plugins/Config/nppPluginList.dll`, firmada con Authenticode, y la actualiza con cada versión de N++ (`PluginsAdminDlg::initFromJson` la lee y comprueba la firma de la DLL). Los paquetes los descarga `gup.exe`, que comprueba el SHA-256 de la lista (`id`). N++ no descarga la lista por su cuenta.

Edita mantiene el formato de la lista y la comprobación del SHA-256, pero la descarga. Así una lista firmada vale aunque llegue por cualquier camino: un espejo, una caché o un proxy.

## Qué se construyó

| Parte | Qué hace |
|---|---|
| **`core/Minisign`** (nuevo) | Comprueba firmas Ed25519 con el formato de minisign, las que produce `catalog.py` del catálogo: la firma del archivo, tanto la prehasheada con BLAKE2b-512 («ED») como la directa («Ed»), y la del comentario de confianza. Usa OpenSSL (`libcrypto`), que tiene Ed25519 y BLAKE2b desde la 1.1.1 y está en cualquier distribución. También firma, para las pruebas |
| **`core/RemoteCatalog`** (nuevo) | La clave pública del catálogo (`F1C3EC604B709A70`, la de `edita-plugins/keys/catalog.pub`) y la URL de la lista (`releases/latest/download/plugins.json`). Funciones: `accept` (firma, formato y que la versión no sea anterior a la conocida); `adapt` (los binarios que este equipo no puede cargar pasan a compilarse con su receta); `store` y `loadCached` (la última lista aceptada, con su firma, en `<plugins>/Config/catalog/`, comprobada otra vez al leerla); `merge` (con la lista que trae Edita); `unpackRecipes` (`recipes.zip`, comprobado con el SHA-256 de la lista firmada) |
| **`PluginCatalog`** | Lee los campos del catálogo: `arch`, `edita-plugin-api`, `edita-qt`, `edita-recipes` y su SHA-256, y en cada plugin `license` y `edita-recipe`. La descripción muestra la licencia y de dónde sale el binario |
| **Administrador de plugins** | Se abre en el acto con la última lista aceptada (o la que trae Edita) y descarga la nueva y su firma sin bloquear el diálogo (`refreshCatalog`, señal `catalogRefreshed`). Si es válida, la guarda y actualiza las pestañas; si no, lo dice en la barra de estado y sigue con la que tenía. Un plugin sin binario para este equipo se instala compilando su receta de `recipes.zip`, con el mismo diálogo de compilación de las recetas locales. La versión de la lista, abajo a la derecha, dice en su tooltip de dónde viene |
| **`EDITA_PLUGIN_CATALOG_URL`, `EDITA_PLUGIN_CATALOG_PUBKEY`** | Otro catálogo (una URL o una ruta) y otra clave. Con `none` no se usa ningún catálogo, como en las pruebas (`tests/CMakeLists.txt` lo pone en todas) |
| **CI y AppImage** | `libssl-dev` en la CI de Linux y en el sistema base de `build-portable.sh` (que se lo añade a un sistema ya preparado). linuxdeploy incluye `libcrypto` en el AppImage |

### Qué se comprueba

1. **La firma** de `plugins.json` con la clave que trae Edita. Una lista modificada, firmada con otra clave o con el comentario de confianza alterado se rechaza.
2. **Que no se vuelve atrás.** La versión de la lista (`2026.11.03.6`, que la CI aumenta en cada catálogo) no puede ser menor que la de la última aceptada. Así nadie puede servir una lista vieja, aunque esté bien firmada, para volver a ofrecer un paquete con un fallo ya corregido.
3. **Que los binarios sirven en este equipo.** Lo comprueba `adapt`, con la arquitectura (`linux-x86_64`), la API de plugins (`EDITA_PLUGIN_API_VERSION`) y el Qt (el de Edita no puede ser más antiguo que el de la compilación, ni de otra versión mayor). Si no sirven, los plugins con receta se compilan en el equipo y los demás no se listan. La barra de estado explica por qué.
4. **Cada paquete** con su SHA-256 (el `id` de la lista firmada), como siempre, y `recipes.zip` con el suyo.

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| La lista de plugins | Viene con N++ (una DLL firmada) y cambia con cada versión | Se descarga al abrir el administrador, firmada con minisign; la que trae Edita es el respaldo | Un plugin nuevo no necesita una versión de Edita; la firma no depende de Windows |
| Volver a una lista anterior | Lo impide la versión de N++ | Lo impide la versión de la lista | La lista se descarga |
| Plugin sin binario para el equipo | No se lista | Se compila con su receta | Otra arquitectura, otro Qt o una licencia que no deja distribuir el binario |
| Lista sin conexión | Siempre (va con N++) | La última aceptada, comprobada al leerla | — |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| La lista que publicó la CI (firmada en Python) se comprueba en C++ con la clave que trae Edita; un byte cambiado en la lista o en el comentario de confianza, u otra clave, no pasan | ✅ `tst_catalog`: `verifiesThePublishedCatalog` (con `tests/fixtures/catalog/`) |
| Firmar y comprobar en C++: el vector 1 del RFC 8032, el comentario de confianza, formatos que no son de minisign, un algoritmo desconocido | ✅ `signsAndVerifies` |
| Una lista anterior a la conocida no se acepta; la misma o una posterior, sí; una bien firmada que no es una lista, tampoco | ✅ `acceptsOnlyNewerLists` |
| Otra arquitectura, otra API, un Qt más antiguo u otra versión mayor de Qt: el binario con receta pasa a compilarse y el que no tiene receta se quita; un Qt más nuevo sí sirve | ✅ `adaptsToThisComputer` |
| La lista de Edita gana a la del catálogo con el mismo plugin; la guardada se vuelve a comprobar; `none`; los valores de serie; `recipes.zip` con otro SHA-256 no se extrae | ✅ `mergesWithTheBundledList`, `storesAndReloads`, `unpacksTheRecipes` |
| El administrador se abre sin esperar a la red, recibe la lista, la muestra (versión, «(from source)», licencia), instala el binario comprobando su SHA-256 y guarda la lista; sin red usa la guardada y lo dice; una lista anterior o mal firmada no cambia nada; una nueva sustituye a la guardada | ✅ `tst_pluginsadmin`: `signedCatalog` |
| Un plugin cuyo binario es para otra arquitectura se compila con la receta de `recipes.zip` y se instala | ✅ `tst_recipes`: `pluginsAdminBuildsCatalogRecipes` |
| Con el catálogo publicado de verdad (red): la lista firmada de GitHub, y Reverse Lines se instala con su licencia | ✅ `tst_pluginsadmin`: `publishedCatalog`, con `EDITA_NETWORK_TESTS=1` (sin esa variable no usa la red) |
| Cadenas nuevas traducidas al español; el manual explica el catálogo en los dos idiomas | ✅ `edita_es.ts`, `13-plugins.md` |
| Pasan todas las suites y el build no da avisos (GCC y clang) | ✅ |

## Siguiente

El ciclo del catálogo está completo: receta → CI → lista firmada → Edita. A partir de aquí:

- **Más plugins:** el siguiente lote es el de la clase B1 de la auditoría, a medida que la capa de compatibilidad cubra lo que les falta (controles comunes, bucles de mensajes, dibujo).
- **La capa de compatibilidad v2, parte 4:** pestañas y listas con columnas, `GetMessage`/`DispatchMessage` y dibujo básico sobre `QPainter`. Es lo que más plugins frena.
- **Mudar MIME Tools y XML Tools** de `data/plugins/recipes` al catálogo.
