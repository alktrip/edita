# Fase 61 — Sin la lista de plugins de Notepad++

Fecha: 2026-11-04

## Por qué

Desde la [fase 14](fase14.md), el administrador de plugins tenía el botón **Lista de plugins de Notepad++…**. Descargaba la lista oficial (`pl.x64.json`, de github.com/notepad-plus-plus/nppPluginList) y ponía sus plugins en **Incompatibles**, para ver si alguno tenía versión para Edita. Era útil antes de que hubiera un catálogo propio. Desde las fases [55](fase55.md) y [56](fase56.md), los plugins de N++ que funcionan en Edita llegan por el catálogo firmado, compilados desde su código ([fase 60](fase60.md): 17).

Los de la lista de N++ son DLL de Windows, y ninguno se puede cargar en Linux. El botón llenaba Incompatibles con unos 200 plugins que nadie puede instalar, y hacía una petición a un servicio de terceros. Lo que sí sirve de esa lista ya está en la [auditoría del catálogo](https://github.com/alktrip/edita-plugins/tree/main/audit), que dice qué plugins se pueden compilar para Edita y qué los frena.

## Qué cambió

| Dónde | Qué |
|---|---|
| **Administrador de plugins** | Sin el botón ni la descarga (`loadNppCatalog`, `nppCatalogUrl`, la variable `EDITA_NPP_PLUGIN_LIST`). Incompatibles muestra solo los plugins instalados que no se pudieron cargar, como un DLL copiado a mano a la carpeta de plugins |
| **Manual** (en los dos idiomas) | En vez del botón, explica que los plugins de N++ son DLL de Windows, y que llegan a Edita compilados desde su código, por el catálogo |
| **Pruebas** | `tst_pluginsadmin`: `incompatibleAndSearch` sustituye a las dos pruebas de la lista de N++. Comprueba que el botón ya no está, que Incompatibles tiene solo el plugin roto y que se puede quitar, y la búsqueda (por nombre y por descripción), ahora en Disponibles |
| **Traducciones** | Desaparecen las 6 cadenas del botón y sus avisos |

El formato de las listas sigue siendo el de `nppPluginList.json`: el catálogo de Edita lo usa, con sus campos propios.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| El administrador no tiene el botón ni descarga nada de Notepad++ | ✅ `incompatibleAndSearch`; no queda ninguna URL de nppPluginList en `src/` |
| Incompatibles sigue mostrando los plugins que no cargan, y se pueden quitar | ✅ `incompatibleAndSearch` |
| La búsqueda sigue funcionando | ✅ `incompatibleAndSearch` |
| Pasan todas las suites y el build no da avisos | ✅ |
