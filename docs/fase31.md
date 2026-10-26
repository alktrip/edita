# Fase 31 — Fuentes del escritorio, barra de estado interactiva y paréntesis por colores: resultado

Fecha: 2026-10-26

## Qué se construyó

| Parte | Qué hace |
|---|---|
| **Fuentes del escritorio en el AppImage** (`syntax/DesktopFonts`) | Los avisos `QFont::fromString: Invalid description 'Ubuntu,10,…'` no venían de qt6ct sino de **KDE**: el tema de Qt lee las fuentes de `~/.config/kdeglobals`, que Plasma guarda con el formato del Qt del sistema (19 campos en Qt 6.12). El Qt 6.8 del AppImage no lo entiende, avisaba y usaba una fuente genérica. Ahora, si Qt no entiende la fuente, Edita la lee de forma tolerante (familia, tamaño, grosor —escala de Qt 5 o de Qt 6— y cursiva están siempre en los mismos campos) y la aplica: la general para la interfaz y la monoespaciada para el editor (en lugar de `QFontDatabase::systemFont(FixedFont)`). Se filtran solo esos avisos (`qInstallMessageHandler`, antes de crear la aplicación). `kdeglobals` se lee directamente, porque `QSettings` tomaría las comas como una lista |
| **Pie de la página de inicio** | «Mostrar esta página…» pasó arriba a la derecha del encabezado, y el aviso «Escribe para empezar…», junto a la versión: se ven sin desplazarse en ventanas bajas |
| **Barra de estado interactiva** (`ui/QuickPicker`) | Los campos se pueden pulsar. **Lenguaje**, **codificación** y **fin de línea** abren una lista con buscador (búsqueda difusa) encima del campo, con las órdenes de su menú (los submenús como grupo, el actual marcado): elegir una es lo mismo que elegirla en el menú. **Sangría**: un campo nuevo, «Tabulaciones: 4» o «Espacios: 4», con sangrar con espacios o tabulaciones, el ancho (2, 3, 4, 8), las conversiones de Editar › Espacios en blanco y la configuración de la sangría. La sangría elegida ahí vale para ese documento mientras esté abierto, también al cambiar de documento o de tema. **Posición**: ir a una línea (la paleta con «:»). **INS/SOB**: cambia el modo |
| **Paréntesis por colores** (`ui/RainbowBrackets`) | Cada nivel de `( [ {` con su color (tres que se repiten, los de VS Code en claro y en oscuro), y en rojo el que cierra sin pareja o con la que no le toca. Un indicador `INDIC_TEXTFORE` con el color en su valor pinta el texto de cada paréntesis. Solo en lo visible, contando la profundidad desde el principio. No cuentan los paréntesis de comentarios, cadenas, caracteres y expresiones regulares (los estilos del lexer con esos nombres en `stylers.xml`). Hasta 2 MB. Preferencia: Edición › «Paréntesis por colores según el nivel» (`EditaRainbowBrackets`) |

## Problemas encontrados

- **Un bucle sin fin con los indicadores** (desde la fase 29). La vista previa de colores, y ahora los paréntesis, volvían a calcular con cada aviso de Scintilla, y sus propios indicadores también avisan (`SC_MOD_CHANGEINDICATOR`, `SC_UPDATE_CONTENT`): cada 40–60 ms, sin parar. Además de gastar CPU, ese torrente de `SCN_UPDATEUI` reiniciaba la espera de MarkdownViewer++, que no volvía a dibujar (su prueba tardaba 15,7 s y fallaba; ahora 0,8 s). Ahora solo reaccionan a inserciones y borrados de texto y al desplazamiento. Las marcas de la barra de desplazamiento sí atienden a los indicadores (muestran «Marcar todo»), así que limitan su ritmo en lugar de esperar a que se calme. Prueba: con el editor quieto, ningún cambio de indicadores en medio segundo (36 sin la corrección).

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Fuentes en el formato de Qt 6.12, Qt 6 antiguo y Qt 5; `kdeglobals` con comas y varias secciones | ✅ `tst_statusbar`: `desktopFontsInAnyQtFormat`; el AppImage arranca en Plasma sin avisos |
| Lenguaje, fin de línea y codificación con buscador; posición abre «:»; INS/SOB cambia | ✅ `statusBarFieldsOpenPickers` |
| La sangría del documento se cambia, se ve en la barra y se mantiene al volver a él | ✅ `indentationOfTheDocument` |
| Colores por nivel, iguales al abrir y cerrar; sin color en comentarios y cadenas; el sobrante en rojo; sin bucles; desactivados | ✅ `bracketsByLevel` |

Se revisaron capturas en claro y en oscuro. Pasan las 36 suites y el build no da ningún warning.
