# Fase 19 — Configurador de estilos: resultado

Fecha: 2026-10-21

Objetivo: cambiar colores y fuentes desde la interfaz, como con el **Configurador de estilos** de N++ (`WordStyleDlg`, Configuración > Configurador de estilos…), en lugar de editar los XML a mano.

## Qué se construyó

| Parte | Qué hace |
|---|---|
| `ui/StyleConfiguratorDialog` | La disposición de N++. Arriba, **Tema**. A la izquierda, el **lenguaje** («Estilos globales» y cada lexer, por su descripción) y sus **estilos**. A la derecha, para el estilo elegido: **color del texto y de fondo** (los que el estilo no usa quedan desactivados, como en N++), **fuente** y **tamaño** (vacío = el del estilo por defecto), **negrita, cursiva y subrayado**. Para «Global override», sus siete casillas «Forzar … en todos los estilos». Para un lenguaje, las **extensiones** de `langs.xml` y las **del usuario**. Para los estilos con palabras clave, las **de serie** (solo lectura) y las **del usuario**. Los cambios se ven **al momento** en el editor. **Guardar y cerrar** los escribe; **Cancelar** los deshace. Cambiar de tema con cambios sin guardar pregunta si se guardan, como en N++ |
| `StyleSheet::save` | `writeStyles`/`writeStyle2Element` de N++: escribe colores (`RRGGBB`), `colorStyle`, fuente, tamaño (`""` = 0), estilo de fuente, palabras del usuario y el `ext` de cada lexer. Actualiza el XML que ya existe, así que comentarios, orden y lo que Edita no conoce quedan como estaban. Los estilos globales se buscan por nombre, porque varios comparten `styleID="0"` |
| `SyntaxManager` | `previewStyles` (vista previa) y `saveStyles`. Un **tema de la aplicación** (fuera de `~/.config/edita`, que puede no tener permiso de escritura) se copia antes a `~/.config/edita/themes/`; esa copia sustituye al tema de la aplicación y pasa a ser el tema en uso. El **Global override** se lee y guarda en `config.xml` (`<GUIConfig name="globalOverride" fg=… bg=… font=… fontSize=… bold=… italic=… underline=…/>`, como en N++). Antes, Edita no lo leía nunca |
| Menú | Configuración > **Configurador de estilos…** (`IDM_LANGSTYLE_CONFIG_DLG`), con icono |

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Ventana | No modal: se puede seguir editando | Modal; los cambios se ven igual en el editor de detrás | Más simple; la vista previa cubre el uso habitual |
| Selector de color | Paleta propia con «más colores» | El diálogo de color de Qt | El de la plataforma |
| «Ir a la configuración» (estilos ligados a una preferencia) | Botón | No | Se deja para más adelante |
| Tema de la aplicación sin permiso de escritura | Lo guarda en `%APPDATA%\themes` | Copia en `~/.config/edita/themes` | Lo mismo en Linux |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Guardar escribe color, estilo de fuente, tamaño, palabras del usuario, extensiones del usuario y un estilo global, y deja el resto del archivo igual (comentarios, número de lexers y estilos, estilos no tocados); la extensión del usuario sirve para detectar el lenguaje | ✅ `tst_styleconfigurator`: `styleSheetSavesOnlyWhatChanged` |
| Un cambio se ve al momento en el editor (color, subrayado de `SCE_C_WORD` en un `.cpp`); Cancelar lo deshace y no escribe nada | ✅ `changesPreviewLiveAndCancelRestores` |
| Guardar y cerrar escribe `stylers.xml` y el Global override en `config.xml`; una ventana nueva los usa, y un archivo con la extensión añadida por el usuario se abre como C++ | ✅ `saveWritesTheStylesFile` |
| Un tema de la aplicación (Monokai) se copia a `~/.config/edita/themes` antes de guardar y pasa a ser el tema en uso; el original no cambia. Cambiar de tema con cambios sin guardar pregunta | ✅ `applicationThemesAreCopiedBeforeSaving` |
| Está en el menú Configuración, con icono | ✅ `menuOpensIt` |

Se revisaron capturas del diálogo con un estilo de C++ (palabras clave) y con el Global override. Todo está traducido al español.

Tests: `tst_styleconfigurator` tiene 7 casos (contando `initTestCase` y `cleanupTestCase`). Pasan las 27 suites y el build no da ningún warning.
