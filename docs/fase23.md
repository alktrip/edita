# Fase 23 — Temas «Edita Light» y «Edita Dark»: resultado

Fecha: 2026-10-21

Objetivo: que el editor combine con la interfaz moderna de la fase 17. Con los temas de N++, el modo claro tenía márgenes grises y en el oscuro el fondo del editor (`#3F3F3F`) era más claro que la ventana.

## Qué se construyó

| Parte | Qué hace |
|---|---|
| `tools/make-edita-themes.py` | Genera los dos temas a partir de `stylers.model.xml` de N++, así que cubren sus 93 lenguajes. Hace tres cosas. Pone los **estilos globales** con la paleta de la interfaz: fondo, márgenes (sin el gris de N++), línea actual, selección y llaves en el verde de Edita, guías, marcas y realces. Cambia los **colores de sintaxis** de N++ (azul puro, verde, naranja, morado…) por una paleta más suave, en versión clara y oscura; los que no están en la tabla se ajustan de luminosidad y saturación. Y la **fuente** es `Monospace`, la monoespaciada del sistema |
| `data/themes/Edita Light.xml`, `Edita Dark.xml` | Los temas generados. Aparecen en Ver > Tema y en el Configurador de estilos como los demás |
| Temas de serie | Sin un tema elegido, Edita usa «Edita Light» en modo claro y «Edita Dark» en oscuro (antes, `stylers.xml` y `DarkModeDefault.xml`). Elegir «Predeterminado» en el menú guarda `stylers.xml`, el de N++. Los `config.xml` anteriores, que guardaban `DarkModeDefault.xml` como valor de serie, se migran una vez (`themesVersion="2"` en `<GUIConfig name="DarkMode">`) |
| Fuente | `Monospace` se resuelve con el nombre real de la monoespaciada del sistema, como ya pasaba con las fuentes de Windows |

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Tema de serie | `stylers.xml` (claro) y `DarkModeDefault.xml` (oscuro) | «Edita Light» y «Edita Dark»; los de N++ se pueden elegir | Que el editor combine con la interfaz |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Los temas se listan (25: el predeterminado, los 22 de N++ y los 2 de Edita) | ✅ `tst_syntax`: `themesAreListedAppliedAndRemembered` |
| Por defecto, «Edita Dark» en oscuro y «Edita Light» en claro; un tema elegido se recuerda por modo; la preferencia de serie es vacía (los de Edita) | ✅ `tst_appearance`: `eachModeHasItsTheme`, `preferencesUseTheNamesOfNotepadPlusPlus` |
| La fuente de los temas de Edita y la de N++ (Courier New) se resuelven a la monoespaciada del sistema; sin esa opción, Courier New | ✅ `editorUsesTheSystemMonospaceFont` |

Se revisaron capturas de C++ y Markdown en los dos modos. Pasan las 30 suites y el build no da ningún warning.
