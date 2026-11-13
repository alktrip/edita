# Fase 87 — Más plugins de Notepad++: lo que pedían a la capa de compatibilidad

Fecha: 2026-11-13

## Por qué

Se cruzó el catálogo de Edita con los plugins más descargados de Notepad++ (las descargas de GitHub de cada uno, la mejor aproximación pública). Los cuatro primeros que faltaban y se pueden compilar desde su código son NppExport, JSON Viewer, Npp Converter y HEX-Editor; también se miró NppFTP. Esta fase añade a la capa de compatibilidad y al SDK lo que pedían los tres primeros. Las recetas están en [edita-plugins](https://github.com/alktrip/edita-plugins).

## Qué se hizo

**SDK.** `edita_npp_plugin(… CXX_STANDARD 20)`: los plugins se compilan como C++17 salvo que la receta pida otro estándar. JSON Viewer usa `starts_with` y `ends_with` de C++20.

**Capa de compatibilidad:**

| Qué | Cómo |
|---|---|
| `GetFileVersionInfoSize`, `GetFileVersionInfo`, `VerQueryValue` y `VS_FIXEDFILEINFO` (winver) | Los `.so` no tienen recurso `VERSIONINFO`. Se responde como Windows con un archivo sin él: tamaño 0, `FALSE` y `ERROR_RESOURCE_TYPE_NOT_FOUND`. JSON Viewer muestra entonces su «Acerca de» sin número de versión |
| `GetIconInfo` e `ICONINFO` | Los iconos de la capa no tienen mapas de bits de Windows: los deja a `NULL` |
| `EnableThemeDialogTexture` y `ETDT_*` (uxtheme) | No hace nada: los diálogos ya tienen el aspecto de Qt |
| `itoa` y `ltoa` | Los nombres antiguos, que MSVC acepta. Son funciones, no macros, para no chocar con variables o métodos que se llamen igual. Como en MSVC, el signo solo se escribe en base 10 |
| `SWP_NOCOPYBITS`, `TVHT_ONITEMBUTTON` y los demás `TVHT_*` | Constantes |

**Recetas nuevas** (en edita-plugins):
- **JSON Viewer 2.2.0.0** (MIT, binario). Sus submódulos (rapidjson y la plantilla de N++) entran como `extra-sources`. Lleva un parche de dos líneas para no usar `std::format`, que GCC 12 no tiene.
- **Npp Converter 4.7** (GPL-3.0-or-later, binario), sin cambios.
- **NppExport 0.4.0** (GPL-2.0-only), sin cambios. Va como receta, que se compila en el equipo del usuario, como Simplify JSON: su licencia no permite meterlo en un `.so` con la capa, que es GPL-3.
- Además, `build-all.sh` usaba `| tee` sin `pipefail` y daba por buena una receta cuya prueba fallaba. Ahora no.

## Lo que queda: HEX-Editor y NppFTP

HEX-Editor 0.9.14 compila hasta 98 errores. Lo que falta no son constantes sueltas, sino partes enteras de Win32:
- los controles que dibuja el plugin (`WM_DRAWITEM`, `DRAWITEMSTRUCT`, `ODA_*`/`ODS_*`), que es su vista hexadecimal;
- `EnumFontFamiliesEx`, el registro (`RegOpenKeyEx`…), `EnumDisplayMonitors`, `GetVersionEx`/`GetProductInfo`;
- los tooltips que siguen al ratón (`TTM_TRACKPOSITION`), `ModifyMenu`, `IsTextUnicode`, `DPI_AWARENESS_CONTEXT` y `zmouse.h`.

NppFTP necesita además sockets de Windows y libssh. Son la siguiente parte de la capa: el dibujo por el propio plugin era ya uno de los bloqueos más frecuentes en la auditoría de la [fase 58](fase58.md).

## Pruebas

- `tst_npp_compat`, prueba nueva: `versionInfoIconsAndOldCrtNames`.
- Las tres recetas compilan y pasan su `check` con `build-all.sh`:
  - JSON Viewer: abrir el panel, formatear, comprimir (con un caso que comprueba el resultado) y ordenar;
  - Converter: «Hi» ↔ «4869» y el panel de conversión;
  - NppExport: las tres órdenes de copiar al portapapeles.
- Las 19 recetas anteriores siguen pasando con `pipefail`.
