# Fase 44 — Manual de usuario (2): el resto de los capítulos: resultado

Fecha: 2026-10-30

Segunda de las tres fases del manual (ver [fase43.md](fase43.md)). Escribe en inglés y en español
los capítulos que faltaban, salvo el 15 (la tabla de atajos), que se generará en la fase 45 a
partir de los menús.

## Qué se construyó

| Capítulo | Contenido |
|---|---|
| 06 Lenguajes y temas | Resaltado y cómo se elige el lenguaje; lenguajes definidos por el usuario (importar, carpeta, el editor y sus pestañas); apariencia clara u oscura con un tema por modo; el menú Tema; el Configurador de estilos (Estilos globales, extensiones y palabras del usuario, Global override) |
| 07 Autocompletado y fragmentos | De dónde salen las palabras, las opciones de la página Autocompletado, las cuatro órdenes del submenú, ampliar los archivos de API; fragmentos con Tab (huecos, espejos, final), Insertar fragmento, los incluidos, y los del usuario con el formato de VS Code: sintaxis y variables |
| 08 Servidores de lenguaje | Qué son, cuáles se usan por lenguaje, la página de preferencias, la carpeta del proyecto y `compile_commands.json`; diagnósticos y Problemas; autocompletado, ayuda de parámetros, información y pistas; definición, referencias y símbolos; renombrar, formato y acciones de código con la bombilla; registro y reinicio |
| 09 Git | Marcas del margen y su navegación, blame al final de la línea y en el margen, el commit de la línea, comparar con HEAD y los estados del explorador |
| 10 Comparar documentos | Las tres formas de empezar, los colores, la alineación, el desplazamiento conjunto, el resumen, la navegación, las opciones y cuándo termina |
| 11 Terminal | Abrirla, dónde empieza, pestañas, teclas que van a la shell, copiar y pegar, historial, ratón y la shell de las preferencias |
| 12 Macros | Grabar, reproducir, un ejemplo, ejecutar muchas veces, guardar y eliminar, compatibilidad con `shortcuts.xml` de N++ |
| 13 Plugins | El administrador y sus pestañas, los que se compilan desde su código, la lista de N++, los incluidos (MarkdownViewer++, Python Script), XML Tools y MIME Tools, y cómo escribir plugins |
| 14 Preferencias y atajos | Resumen de cada página de Preferencias, la Asignación de atajos y los archivos de `~/.config/edita/` |
| 16 Solución de problemas | Arranque (FUSE, macOS, una sola instancia, sesión), texto (acentos, fines de línea, cambios perdidos, Alt+arrastrar, fuente), servidores de lenguaje, plugins, volver a la configuración de serie y dónde informar de errores |

Además, los capítulos 3 y 5 enlazan a los nuevos. Como en la fase 43, cada afirmación se comprobó en
el código y los nombres de botones y opciones son los de la aplicación en cada idioma (por ejemplo,
«Tú, sin confirmar» en blame, «Hasta el final del archivo» al ejecutar una macro, o los prefijos
`hoy` y `ahora` de los fragmentos globales). Las capturas mostraron que md4c ya trata los bloques de
código de una tabla como una sola pieza: `${1|uno,dos|}` no necesita escapar las barras, y
escapadas se veían.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Los dos idiomas tienen los mismos 15 capítulos, todas las órdenes citadas existen y todos los enlaces llevan a un capítulo y un apartado que existen | ✅ `tst_manual`: `contentIsComplete` |
| El visor sigue funcionando con todos los capítulos | ✅ `windowNavigatesAndSearches` |

Pasan las 45 suites y el build no da ningún warning.
