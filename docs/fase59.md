# Fase 59 — MIME Tools y XML Tools pasan al catálogo

Fecha: 2026-11-04

## Por qué

Desde la [fase 15](fase15.md), Edita traía dos recetas en `data/plugins/recipes`: MIME Tools (el código original sin cambios) y XML Tools (un port: su interfaz en Qt sobre `edita_plugin.h`, con libxml2 y libxslt donde el original usa MSXML). Desde las fases [55](fase55.md) y [56](fase56.md), las recetas tienen su sitio en el [catálogo de plugins](https://github.com/alktrip/edita-plugins): ahí se compilan, se prueban y se publican como binarios, y una versión nueva de un plugin no necesita una versión de Edita. Tener dos recetas en cada repositorio era duplicar la fuente de verdad.

## Qué cambió

| Dónde | Qué |
|---|---|
| **edita-plugins** | `recipes/mimeTools` y `recipes/XMLTools`, con lo que pide el catálogo. La licencia es GPL-3.0-or-later en las dos: el `license.txt` de MIME Tools y el `LICENSE` de XML Tools, y las cabeceras de sus fuentes. Van en el paquete (`license-files`). La prueba (`check`) es la de las órdenes principales: Base64 con y sin relleno, URL en sus variantes, formatear, linealizar, comentar y escapar XML, y que estén las órdenes con casilla y los diálogos. La CI instala `libxml2-dev` y `libxslt1-dev` para XML Tools. Las dos tienen binario |
| **Edita** | `data/plugins/recipes` desaparece. El mecanismo sigue: las recetas del usuario (`~/.config/edita/plugins/recipes`) y las que instale una distribución (`share/edita/plugins/recipes`) se leen igual |
| **Pruebas de Edita** | `recipesAndSdk` ya no cuenta las recetas de Edita; comprueba que una carpeta de recetas que no existe no da avisos. La prueba a fondo del port de XML Tools y de MIME Tools (`realNotepadPlusPlusPlugins`: el menú completo, el cierre de etiquetas, el tipo XML automático, los errores…) lee las recetas de `../edita-plugins`, junto a `editor/`, igual que ya leía el código original de `../referencia/plugins`; sin ellas, se salta. La CI de Edita ya no instala libxml2 ni libxslt |
| **Manual, NOTICE, README** | XML Tools y MIME Tools están en la tabla del catálogo del manual (en los dos idiomas). NOTICE remite al catálogo, donde cada receta tiene la licencia del plugin que adapta. La lista de dependencias del README ya no pide libxml2 y sí OpenSSL (desde la fase 56) |

## Para quien ya los tenía

Un MIME Tools o un XML Tools compilado con las recetas de antes sigue instalado (`~/.config/edita/plugins/<carpeta>`): el catálogo tiene la misma carpeta y la misma versión, así que aparece en Instalados y no se reinstala. Las versiones nuevas llegarán por el catálogo, a la pestaña de Actualizaciones.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Las 9 recetas del catálogo compilan y pasan su prueba, MIME Tools y XML Tools incluidas, con su licencia dentro del paquete | ✅ `scripts/build-all.sh` |
| La prueba a fondo de los dos plugins pasa con las recetas del catálogo | ✅ `tst_recipes`: `realNotepadPlusPlusPlugins` (MIME Tools y XML Tools) |
| La lista que trae Edita no tiene ninguno de los dos (si los tuviera, taparían los del catálogo) | ✅ `editaPluginList.json`: Python Script, MarkdownViewer++ y el de ejemplo |
| El manual cita cada orden en los dos idiomas | ✅ `tst_manual` |
| Pasan todas las suites y el build no da avisos | ✅ |

## Siguiente

La capa de compatibilidad v2, parte 5, con lo que mide la auditoría de la [fase 58](fase58.md): hooks (`SetWindowsHookEx`), SysLink, barras de herramientas y listas de imágenes, el dibujo propio de los controles (`WM_DRAWITEM`, `NM_CUSTOMDRAW`) y la información de versión (`GetFileVersionInfo`).
