# Fase 29 — Mejoras visuales rápidas: resultado

Fecha: 2026-10-21

Objetivo: seis mejoras visuales que **no existen en Notepad++**, de poco riesgo, que cambian el aspecto del editor.

## Qué se construyó

| Mejora | Qué hace |
|---|---|
| **Iconos por tipo de archivo** (`ui/FileIcons`) | Una hoja con la etiqueta del tipo («C++», «PY», «MD», «{}», «CSS», «CM»…) en su color (los de GitHub Linguist, ajustados para leerse en claro y en oscuro). Unos 50 tipos, por extensión o por nombre (Makefile, CMakeLists.txt, Dockerfile, .gitignore…); los demás llevan una hoja sin etiqueta. Se dibujan con QPainter en varios tamaños, sin archivos de imagen, y se guardan por tipo, estado y modo. En las **pestañas** llevan además el estado: un punto del color de «sin guardar» o un candado. También en la **Lista de documentos**, el **Explorador de carpetas** e **Ir a archivo**. Se pueden desactivar (General › Barra de pestañas) para volver a los iconos de estado |
| **Vista previa de colores** (`ui/ColorPreview`) | `#rgb`, `#rgba`, `#rrggbb`, `#rrggbbaa` (transparencia al final, como en CSS), `rgb()`/`rgba()` (con comas o espacios y `/ alfa`, también en %) y `hsl()`/`hsla()` se enmarcan con su propio color: un indicador de Scintilla con `SC_INDICFLAG_VALUEFORE`, debajo del texto. Si el color casi no se distingue del fondo, lleva además un borde del color del texto. Solo en CSS, HTML, XML, JavaScript, TypeScript, JSON, PHP, ASP, Python, YAML, INI y propiedades. Los `#abc` de 3 o 4 cifras, solo donde `#` no es un comentario. Se busca en lo visible al desplazarse o editar. Al pasar el ratón, una muestra grande con los valores en hex, rgb y hsl; **Ctrl+clic** abre el selector (con transparencia) y reescribe el color en el mismo formato (`#ABC` → `#F00`, `rgb` → `rgba` si hay transparencia…) |
| **Número de la línea actual** | Un marcador translúcido, del color de los números, sobre el número de la línea del cursor. Usa el marcador 15: N++ reserva 1–14 para los plugins (Edita dejaba asignar hasta el 15 y se corrigió) |
| **Guía de sangría del bloque actual** | Si el cursor no está junto a una llave (eso ya lo resaltaba), se resalta la guía del bloque en el que está: el que abre la línea anterior con menos sangría, hasta su última línea con más sangría. Las líneas en blanco toman la sangría de sus vecinas. Scintilla solo resalta la guía entre las dos «llaves» de `SCI_BRACEHIGHLIGHT`: se le dan los finales de la primera y la última línea del bloque, que no cambian de color. Funciona igual en Python que con llaves |
| **Temas modernos** | **One Dark, Dracula, Catppuccin Mocha, Catppuccin Latte, Nord y Tokyo Night**, inspirados en esas paletas (MIT). `tools/make-edita-themes.py` ahora define cada tema como colores por papel (texto, palabra clave, tipo, comentario, cadena, número, preprocesador, atributo…) y por elemento de la interfaz (fondo, márgenes, selección, línea actual, marcas, búsquedas…). Así cubren los 93 lenguajes de `stylers.model.xml`. Edita Light y Edita Dark pasaron al mismo esquema y salen idénticos byte a byte |
| **Avisos sin ventanas modales** (`ui/NotificationBar`) | Una barra encima del editor, con un fondo suave del color del aviso (azul informativo, ámbar advertencia), icono, texto, botones opcionales y ✕. Los informativos sin botones se ocultan a los 8 segundos y los demás esperan a que se cierren. Si hay muchas líneas, se ven tres y «…y N más», con el texto completo en el aviso emergente. Si llegan varios avisos, se muestran uno tras otro. Los avisos de configuración al arrancar (un tema que ya no existe…) y al recargar estilos van ahí; las preguntas (guardar, sobrescribir…) siguen siendo diálogos |

Preferencias nuevas (todas activadas de serie, en `config.xml`): `EditaFileTypeIcons`, `EditaColorPreview`, `EditaCurrentLineNumber` y `EditaActiveIndentGuide`.

## Divergencias

Ninguna con N++: son añadidos. Los iconos de estado de las pestañas (sin los disquetes de N++, desde la fase 17) siguen disponibles.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Tipos por extensión y nombre; iconos distintos por tipo y por estado; en las pestañas siguen al estado y se desactivan | ✅ `tst_visual`: `fileTypeIcons` |
| Colores en hex (3, 4, 6, 8 cifras), rgb(a) con comas, espacios y %, hsl(a); sin falsos en `#include`, `&#123;` o palabras; reescritura en el mismo formato | ✅ `colorLiterals` |
| En el editor: el indicador lleva el color; el selector reescribe el valor; nada en texto normal ni desactivada | ✅ `colorPreviewInTheEditor` |
| El marcador sigue al cursor; la guía del bloque (también en una línea en blanco) y ninguna en el primer nivel; desactivados | ✅ `currentLineNumberAndBlockGuide` |
| Los seis temas se listan, cargan y cubren todos los lenguajes; el fondo de Dracula es el de su paleta | ✅ `modernThemes`; `tst_syntax` cuenta 31 temas |
| Avisos en cola, «…y N más»; el aviso de un tema perdido va a la barra y no a un cuadro | ✅ `notificationsDoNotBlock` |

Se revisaron capturas en claro, en oscuro y con Tokyo Night. En ellas se vio que un color muy parecido al fondo (`#fff8c5` sobre blanco) casi no se distinguía, y se añadió el borde. Pasan las 34 suites y el build no da ningún warning.
