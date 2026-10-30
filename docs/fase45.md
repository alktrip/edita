# Fase 45 — Manual de usuario (3): capturas, atajos y cobertura: resultado

Fecha: 2026-10-30

Última de las tres fases del manual (ver [fase43.md](fase43.md) y [fase44.md](fase44.md)).

## Qué se construyó

| Parte | Qué hace |
|---|---|
| **Capturas automáticas** (`tests/manual_screenshots.cpp`) | Un programa que abre la aplicación de verdad, sin pantalla, con una configuración vacía y un proyecto de ejemplo, y guarda siete capturas en `data/manual/<idioma>/images/`: la ventana con el explorador, la barra de búsqueda con coincidencias, la paleta de órdenes buscando «dupl», el probador de regex con grupos, una comparación con la versión guardada, el Configurador de estilos y Preferencias. Cada captura es un paso (preparar, esperar a que se dibuje, guardar); los diálogos modales se capturan con un temporizador programado antes de abrirlos, y lo que flota (la paleta) se compone encima. `cmake --build build --target edita_manual_images` las regenera en inglés y español; se guardan en el repositorio |
| Imágenes en los capítulos | En los capítulos 1, 4 (dos), 5, 6, 10 y 14 de los dos idiomas |
| Imágenes en el visor | Se reducen con suavizado al ancho de la página al cargarlas (`ManualBrowser::loadResource`); con un `width` en el HTML, Qt las reducía sin suavizar y el texto de las capturas no se leía |
| **Tabla de atajos** (capítulo 15) | `{{shortcuts}}` se convierte en un apartado por menú con una tabla de las órdenes que tienen atajo, en el orden de los menús, con los nombres traducidos y los atajos actuales (`Manual::shortcutTables`; `Command::order` guarda el lugar de cada orden). Los apartados salen en el índice. Debajo, a mano: las teclas del editor (Scintilla), del ratón, de la barra de búsqueda, de la terminal y del manual |
| **Cobertura** | Una prueba comprueba que cada orden con nombre de los menús aparece en el manual de cada idioma (`{{…}}`), salvo las familias que se crean solas y que el manual describe en grupo: lenguajes, juegos de caracteres y macros guardadas. Ya estaban las 317 |

## Error encontrado por las capturas

En la captura de la comparación, el documento de referencia salía entero en rojo. Los marcadores de
comparar (21, 22 y 23, de la fase 39) y la bombilla de las acciones de código (24, de la fase 42)
usaban los números que Scintilla reserva para el **historial de cambios** (21-24): las líneas que el
historial marcaba como «guardadas» o «modificadas» se pintaban como quitadas o cambiadas, y la
bombilla podía aparecer en las líneas «revertidas». Ahora comparar usa 12, 13 y 14 y la bombilla el
11, tomados del final del rango de los plugins, que queda en 1-10 (`NPPM_ALLOCATEMARKER`; N++ da
1-14). Además, el documento de referencia ya no muestra historial de cambios: su texto no es un
cambio. Prueba: `tst_compare` (`withLastSavedVersion`), que falla con los números de antes.

Las capturas mostraron también que la casilla «Usar los diálogos de archivos de Edita» viene
desactivada (el manual decía lo contrario), que la paleta y la barra de búsqueda rellenan su campo
al abrirse (el programa escribe un momento después) y que los encabezados de la tabla de atajos
faltaban en la traducción.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| La tabla de atajos: por menús, en su orden, solo lo que tiene atajo | ✅ `tst_manual`: `shortcutTables` |
| El capítulo 15 tiene un apartado por menú con los atajos reales | ✅ `shortcutChapterUsesTheMenus` |
| Cada orden de los menús está en el manual, en los dos idiomas | ✅ `everyMenuCommandIsInTheManual` |
| Las imágenes enlazadas existen; se muestran reducidas al ancho de la página | ✅ `contentIsComplete`, `windowNavigatesAndSearches` |
| Los marcadores de comparar no coinciden con el historial de cambios; la referencia no tiene historial | ✅ `tst_compare`: `withLastSavedVersion` |

Pasan las 45 suites y el build no da ningún warning.
