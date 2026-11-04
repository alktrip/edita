# Fase 60 — Ocho plugins más en el catálogo

Fecha: 2026-11-04

## Por qué

La auditoría de la [fase 58](fase58.md) mide qué le falta a la capa de compatibilidad para cada plugin de la lista de N++. Esta fase no sigue la lista por orden de frecuencia: elige plugins concretos con licencia redistribuible y quita todo lo que los frena, hasta que compilan, cargan y pasan su prueba. Así cada cosa que se agrega a la capa tiene un plugin real que la usa.

## Los plugins

| Plugin | Versión | Licencia | Qué hizo falta | Prueba |
|---|---|---|---|---|
| SelectQuotedText | 1.1.0 | GPL-2.0-or-later | Nada en la receta; en la capa, `IsCharAlpha` y compañía | sus órdenes |
| Indent By Fold | 0.7.3 | GPL-2.0-or-later | El `version_git.h` que genera su script de Windows (overlay) | sus órdenes |
| NppStripIpAndHost | 1.0.0 | GPL-2.0-or-later | — | enmascara una IP y un host |
| ElasticTabstops | 1.5.0 | GPL-2.0-or-later | `SysLink` en su diálogo «Acerca de» | sus órdenes |
| MZC8051 | 0.0.1 | GPL-3.0-or-later | Fuentes en GBK (`recode`), C con lo que MSVC tolera, `SHBrowseForFolder` con su función | sus órdenes (en chino) |
| Datamodder SQL Formatter | 1.3.2.18 | MIT | ATL (`CW2A`, `USES_CONVERSION`), `pathcch.h`, `ShellExecuteEx` | minificar y dar formato |
| NppCrossCheck | 1.3.0 | GPL-2.0-or-later | Parche de una línea: `std::ifstream` con ruta `wchar_t` es de MSVC | sus órdenes |
| Pork to Sausage | 2.6 | GPL-2.0-or-later | El mismo parche, eventos con nombre, y su `.ini` con otro nombre (`package-files`) | sus órdenes |

Todos son el código original fijado a su etiqueta. Solo dos llevan un parche, de una línea.

## Qué cambió en Edita

| Dónde | Qué |
|---|---|
| **Capa (`npp_compat*.h`, `npp_win32.cpp`)** | `IsCharAlpha`/`AlphaNumeric`/`Upper`/`Lower`; `ShellExecuteEx` (con `SEE_MASK_NOCLOSEPROCESS` devuelve el proceso); `PathCch*` y `pathcch.h`; `DisableThreadLibraryCalls`; `__ImageBase` (el `HINSTANCE` del plugin, como en MSVC: un símbolo oculto en cada `.so`); eventos y mutex con nombre (`CreateEvent`/`OpenEvent`, `CreateMutex`/`OpenMutex`, `ERROR_ALREADY_EXISTS`); `byte`, `boolean`, `EXTERN_C`, `__max`/`__min` |
| **ATL** | `atlconv.h` y `atlbase.h`: `CW2A`, `CA2W`, `CT2A`… y `USES_CONVERSION` con `W2A`, `A2W`, `T2A`…, en UTF-8. Las macros devuelven punteros a un anillo de búferes del hilo, que viven lo bastante para la expresión en la que se usan, como los de `_alloca` en ATL |
| **Ventanas (`npp_window.cpp`)** | `SysLink` sobre `QLabel`, con `NM_CLICK`/`NMLINK` y `LM_*`; `GetMenuString`, `IsWindowUnicode`, `CreateDialog`; `SHBrowseForFolder` llama a su `BFFCALLBACK` con `BFFM_INITIALIZED`, y `BFFM_SETSELECTION` elige la carpeta inicial |
| **SDK (`EditaNppPlugin.cmake`)** | El código de los plugins se compila sin avisos (`-w`), C++ con `-fpermissive -fno-operator-names` (en MSVC, `and` y `or` son nombres), y C con lo que MSVC solo avisa (punteros incompatibles, enteros como punteros, funciones sin declarar) |
| **Recetas** | `recode` pasa fuentes de la página de código del autor a UTF-8 (`-finput-charset` no sirve: también cambia la lectura de las cabeceras del SDK); `package-files` mete en el paquete otros archivos del código, en su ruta o en otra; los casos de `check` tienen `before`, órdenes que se ejecutan antes en el mismo documento |
| **`--check-plugin`** | Los diálogos que el plugin abre *al cargarse* también se cierran solos (Pork to Sausage avisa si no encuentra su `.ini`); la ventana tiene tamaño y se muestra, para los plugins que miden la vista |

## Lo que se dejó

- **NppEditorConfig** necesita editorconfig-core-c y PCRE2. Compilarlos dentro de la receta es posible (`extra-sources`), pero es la primera receta con dependencias de terceros dentro: una decisión aparte.
- **Los lexers de plugins** (HLASMLexer, OCamlLexer, HiddenLexers) necesitan que Edita cargue lexers externos de Lexilla (`GetLexerCount`, `CreateLexer`) y los ofrezca en el menú de lenguajes. Es una fase propia.
- **ElasticTabstops** no tiene caso de conversión: el resultado depende del ancho de la fuente. Se probó a mano.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Las 17 recetas del catálogo compilan y pasan su prueba | ✅ `scripts/build-all.sh` |
| Cada novedad de la capa tiene prueba | ✅ `tst_npp_compat`: `linksAndNamedObjects`, `shellAndModuleHelpers`, `browseForFolderCallback` |
| `recode`, `package-files` y `before` (también una orden de `before` que no existe) | ✅ `tst_recipes`: `recipeRecodePackageFilesAndBefore` |
| La capa compila también con clang | ✅ `-fsyntax-only` |
| Pasan todas las suites y el build no da avisos | ✅ |

## Siguiente

Los lexers de plugins: Edita carga los lexers de Lexilla que exporta un plugin y los ofrece como lenguajes, con sus estilos. Abre la puerta a HLASMLexer, OCamlLexer y los demás plugins de lenguaje.
