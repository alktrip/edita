# Introducción

Edita es un editor de texto y de código fuente para Linux y macOS, inspirado en Notepad++. Usa el
mismo motor de edición que Notepad++ (Scintilla y Lexilla, con expresiones regulares de Boost) y
una interfaz nativa en Qt 6. Lee los formatos de configuración de Notepad++ (`langs.xml`,
`stylers.xml`, temas, `shortcuts.xml`, lenguajes definidos por el usuario, archivos de
autocompletado), así que lo que ya sabes de Notepad++ también sirve aquí.

Además de lo que ofrece Notepad++, Edita incluye funciones de los editores modernos: paleta de
órdenes, servidores de lenguaje (diagnósticos, autocompletado, ir a la definición…), integración
con Git, comparación de archivos, fragmentos de código, terminal integrada y un probador de
expresiones regulares.

Este manual describe todas las funciones. Usa el campo de búsqueda que está sobre el índice para
encontrar un tema: no distingue mayúsculas ni acentos. Las órdenes de los menús aparecen con su
atajo actual, así que si cambias un atajo en {{actionShortcutMapper}}, el manual muestra el nuevo.

## Instalación

### Linux

La forma más sencilla es el **AppImage**: un solo archivo que funciona en Ubuntu 22.04 o
posterior, Debian 12 o posterior, Fedora 36 o posterior y la mayoría de las distribuciones
actuales.

1. Descarga `Edita-<versión>-x86_64.AppImage` de la página de Releases del proyecto.
2. Dale permiso de ejecución: clic derecho › Propiedades › Permitir ejecutar, o en una terminal
   `chmod +x Edita-*.AppImage`.
3. Ábrelo con doble clic o desde una terminal.

También puedes compilar Edita desde el código con CMake (Qt 6, un compilador de C++20 y Ninja);
el archivo `README.md` del proyecto explica los pasos y los paquetes necesarios.

### macOS

Descarga `Edita-<versión>-macos-universal.dmg` (funciona en Mac con Apple Silicon y con Intel),
ábrelo y arrastra **Edita** a la carpeta Aplicaciones. La aplicación no está notarizada por
Apple, así que la primera vez hay que abrirla con clic derecho › **Abrir**, o permitirla en
Ajustes del Sistema › Privacidad y seguridad › **Abrir igualmente**.

En macOS, los atajos que en Linux usan Ctrl usan la tecla Comando (⌘). Este manual muestra
siempre los atajos del sistema en el que estás.

## Iniciar Edita

Ejecuta `edita` para abrir el editor. En la línea de órdenes puedes pasar archivos y opciones:

```
edita [archivos…] [-n línea] [-c columna] [--nosession] [--multiInst]
```

| Opción | Significado |
|---|---|
| `archivos…` | Los archivos que se abren. Las carpetas no se abren como archivos. |
| `-n línea` | Pone el cursor en esa línea del (último) archivo. |
| `-c columna` | Pone el cursor en esa columna. |
| `--nosession` | Ni restaura ni guarda la sesión (los archivos que estaban abiertos). |
| `--multiInst` | Abre otra ventana aunque Edita ya esté abierto. |

Edita funciona como **una sola instancia**: si ya hay una ventana abierta, los archivos que abras
desde el administrador de archivos o desde la línea de órdenes se abren como pestañas en esa
ventana. Para abrir siempre otra ventana, cambia **Instancias** en {{actionPreferences}} (página
General) o usa `--multiInst`.

Al arrancar, Edita restaura la sesión anterior: los archivos que estaban abiertos, la posición
del cursor en cada uno y los documentos que nunca se guardaron (ver
[Sesiones y copias de seguridad](02-files.md#sesiones-y-copias-de-seguridad)).

## La ventana

De arriba abajo, la ventana de Edita tiene estas partes:

- **Barra de menús**: todas las órdenes están en un menú. También aparecen en la
  [paleta de órdenes](05-views.md#paleta-de-ordenes).
- **Barra de herramientas**: botones para las órdenes más usadas (nuevo, abrir, guardar, cortar,
  copiar, pegar, deshacer, buscar, zoom, ajuste de línea, mostrar todos los caracteres, paneles y
  macros). Se puede ocultar o hacer más pequeña en {{actionPreferences}}.
- **Barra de pestañas**: una pestaña por documento abierto. Un punto en la pestaña indica que el
  documento tiene cambios sin guardar. Arrastra las pestañas para cambiarlas de orden; el clic
  central cierra una pestaña y el clic derecho muestra un menú con más órdenes (cerrar las demás,
  copiar la ruta, abrir la carpeta que la contiene o una terminal en ella…).
- **Ruta de navegación** (opcional): las carpetas, el archivo y la clase y la función donde está
  el cursor. Pulsa una carpeta para ver la lista de sus archivos, o el archivo o un símbolo para
  ver la lista de los símbolos del documento, con buscador.
- **Editor**: el texto, con los números de línea, un margen para los marcadores y otros símbolos,
  el margen del historial de cambios y el de plegado.
- **Paneles**: alrededor del editor se pueden mostrar la Lista de documentos, la Lista de
  funciones, el Explorador de carpetas, el Mapa del documento, los Resultados de búsqueda,
  Problemas y la Terminal (ver [Vistas y paneles](05-views.md)).
- **Barra de estado**: el lenguaje, la posición del cursor, la selección, el fin de línea, la
  codificación, la sangría y el modo insertar/sobrescribir.

### La barra de estado

Casi todos los campos de la barra de estado son botones:

| Campo | Al pulsarlo… |
|---|---|
| Lenguaje | Elige el lenguaje del resaltado de sintaxis en una lista con buscador. |
| `Lín: … Col: …` | Va a una línea ({{actionGoTo}}). |
| Fin de línea | Convierte los fines de línea a Windows (CR LF), Unix (LF) o Macintosh (CR). |
| Codificación | Vuelve a abrir el archivo con otra codificación o lo convierte. |
| Sangría | Elige tabulaciones o espacios y el ancho de la sangría. |
| `INS` / `SOB` | Cambia entre insertar y sobrescribir (también con la tecla Insert). |

## La página de inicio

Cuando no hay documentos abiertos, Edita muestra la página de inicio en lugar de un documento
vacío. Tiene accesos para crear un archivo y abrir un archivo o una carpeta, los archivos y las
carpetas recientes, la apariencia (claro, oscuro o seguir al sistema, y el tema de colores) y una
lista de atajos útiles. Empieza a escribir para hacerlo en un documento nuevo, o pulsa Esc para
cerrar la página.

La página se abre en cualquier momento con {{actionWelcomePage}}, y se desactiva con la casilla
que está al pie de la página o en {{actionPreferences}} (página General).

## Obtener ayuda

- {{actionUserManual}} abre este manual. Las flechas van atrás y adelante por las páginas que
  visitaste; la casa va al primer capítulo. {{key:actionCommandPalette}} abre la paleta de
  órdenes, la forma más rápida de encontrar una orden cuando no recuerdas en qué menú está.
- {{actionAbout}} muestra la versión de Edita.
