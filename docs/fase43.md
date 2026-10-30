# Fase 43 — Manual de usuario (1): el visor y los primeros capítulos: resultado

Fecha: 2026-10-30

Primera de tres fases del manual de usuario, en inglés y español y solo dentro de la aplicación.
Esta construye el visor y escribe los capítulos 1 a 5; la 44 escribe los demás y la 45 añade las
capturas automáticas, la prueba de que cada orden de los menús está en el manual y la tabla de
atajos generada.

## Qué se construyó

| Parte | Qué hace |
|---|---|
| **Ayuda › Manual de usuario** (F1, `QKeySequence::HelpContents`) | Abre la ventana del manual (`ui/ManualWindow`), una sola por ventana principal; si ya está abierta, la trae al frente donde se quedó. `MainWindow::showManual(capítulo, apartado)` abre un apartado concreto, para enlazarlo después desde otros sitios |
| Capítulos | Archivos Markdown en `data/manual/<idioma>/NN-nombre.md` (se instalan con el resto de `data/`; las imágenes irán en `images/`). El mismo nombre de archivo en todos los idiomas, para que los enlaces entre capítulos valgan en todos. El título es el primer encabezado |
| Idioma | El de la interfaz: `es_MX` usa `es`, y un idioma sin manual usa el inglés |
| **Órdenes con su menú y su atajo reales** | `{{actionFind}}` se convierte en «**Buscar › Buscar…** (`Ctrl+F`)»: el menú traducido y el atajo actual, con los que el usuario haya cambiado en la Asignación de atajos (el manual se vuelve a leer al cerrarla) y los de macOS. `{{key:actionFind}}` pone solo el atajo, y `{{item:actionStyleAll1}}` solo el nombre dentro de su submenú («Estilo 1»), para las listas largas. `MainWindow::manualCommands()` recorre la barra de menús y da el mapa nombre → (ruta, atajo) |
| Conversión | md4c (ya estaba para MarkdownViewer++), dialecto de GitHub. Antes de cada encabezado se inserta un `<a name>` con su ancla al estilo de GitHub (minúsculas, sin acentos, espacios a guiones, «-1» en los repetidos); las tablas llevan bordes y las capturas de más de 860 px se reducen |
| Visor | `QTextBrowser` que carga los `.md` como HTML generado (`loadResource`/`doSetSource`): los enlaces relativos entre capítulos, las anclas y el historial (atrás, adelante, Alt+←/→) los da Qt; los enlaces `http` se abren en el navegador |
| Índice | Árbol con los capítulos y sus apartados (niveles 2 y 3). Sigue a la página que se ve, y navegar con el teclado por él abre cada apartado |
| **Búsqueda** | En todo el manual, por apartados, sin distinguir mayúsculas ni acentos («codificacion» encuentra «codificación»): tienen que estar todas las palabras, y las que están en el título del apartado pesan más. La lista muestra dónde (en negrita) y un extracto; al abrir un resultado, las palabras se marcan en la página. Intro abre el primero, ↓ baja a la lista y Esc borra la búsqueda |
| Tema | Los colores salen de la paleta (claro u oscuro): títulos y enlaces con el color de acento, código y cabeceras de tabla con un fondo suave, letra monoespaciada del sistema. Cambian en vivo con la paleta. Ctrl+más y Ctrl+menos cambian el tamaño |
| Contenido | Capítulos 1 a 5 en inglés y español: introducción (instalación, arranque y opciones, la ventana, la barra de estado, la página de inicio), archivos y sesiones (abrir, guardar, codificaciones, fines de línea, cambios externos, sesiones y copias, imprimir y PDF), edición (selección y varios cursores, editor de columnas, operaciones con líneas, ordenar, mayúsculas, comentarios, espacios, sangría, paréntesis), buscar y reemplazar (barra, diálogo, modos, regex, buscar en archivos, resultados, probador de regex, ir a, estilos de token, marcadores, historial de cambios) y vistas y paneles (paleta, pestañas, vista dividida, visualización, plegado, paneles). Todo comprobado contra el código: los textos de las preferencias y de los diálogos son los de la aplicación en cada idioma |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Plegar sin acentos con la misma longitud; anclas como las de GitHub; `{{…}}`, `{{key:…}}` e `{{item:…}}`, y las desconocidas se señalan | ✅ `tst_manual`: `foldAndSlug`, `expandsCommands` |
| Encabezados (no los de dentro de bloques de código), anclas repetidas, tablas, búsqueda | ✅ `loadsChaptersAndHeadings` |
| `es_MX` → `es`, `fr` → `en` | ✅ `languageFallback` |
| En los dos idiomas: los mismos capítulos, todas las órdenes citadas existen en los menús, y todos los enlaces llevan a un capítulo, un apartado o una imagen que existen | ✅ `contentIsComplete` |
| F1 abre la ventana en el primer capítulo; índice con apartados; abrir un apartado y volver atrás; el índice sigue a la página y navega; las órdenes se ven con su menú; buscar, abrir con Intro y marcar las palabras; sin resultados; Esc; un enlace a otro capítulo | ✅ `windowNavigatesAndSearches` |

Las pruebas encontraron que Qt no emite `sourceChanged` al abrir otro capítulo directamente en un
apartado (con `#ancla`), y el índice no lo seguía: ahora se sincroniza con `historyChanged`. Las
capturas (claro y oscuro, inglés y español) llevaron a dar estilo sólido a los bordes de las
tablas en modo oscuro y a dibujar los resultados con el título en negrita. Al revisar el texto
contra el código se corrigieron varias afirmaciones (el botón «Todas» de la barra reemplaza, no
selecciona; las pestañas no se arrastran a la otra vista; la sintaxis `!+\carpeta` de los
filtros). Pasan las 45 suites y el build no da ningún warning.
