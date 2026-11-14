# Fase 96 — Panel TODO

Fecha: 2026-11-13

## Por qué

Los comentarios `TODO` y `FIXME` son la lista de pendientes que vive dentro del código. Un panel que los reúna, del documento o de todo el proyecto, ahorra buscarlos con Buscar en archivos cada vez.

## Cómo lo hace Notepad++

Notepad++ no lo trae. Hay plugins aparte para eso, y lo más parecido es **Buscar en archivos** con una expresión regular.

## Qué se hizo

- **`core/TodoScan`:**
  - **Palabras:** `TODO`, `FIXME`, `HACK`, `XXX`, `BUG` y `NOTE`, en mayúsculas y enteras, justo después de una marca de comentario (`//`, `#`, `/*`, `*` al principio de la línea, `--`, `;`, `<!--`, `%`), con solo espacios entre las dos.
  - **Lo que no cuenta:** la marca va al principio de la línea o después de un espacio, así que no cuentan `TODO_LIST`, `"TODO"` en una cadena, «TODOS» en un texto ni `http://x/TODO`.
  - **El texto:** lo que sigue, sin `:` ni el cierre del comentario (`*/`, `-->`).
  - **Archivos:** se saltan los binarios (un byte 0 en los primeros 8 KB) y los de más de 2 MB.
- **`ui/TodoPanel`** (Ver › TODO, abajo):
  - el alcance (documento actual o carpetas de trabajo), recargar y el número de comentarios;
  - un árbol por archivo, con la palabra, el texto y la línea;
  - doble clic o Intro van a la línea y la columna.
- **`MainWindow`:**
  - **Documento actual:** se busca al mostrarse el panel, al cambiar de documento y al escribir (400 ms después de la última tecla).
  - **Carpetas de trabajo:** en otro hilo, con el recorrido de «Ir a archivo» (`scanFiles`: sin carpetas ocultas ni `node_modules`). Los documentos abiertos cuentan con su texto sin guardar.
  - El alcance se recuerda en `config.xml` (`EditaTodo`).
  - `workspaceRoots()` reúne las carpetas de trabajo.
- **F1** en el panel abre su apartado del manual (fase 95).
- **Traducción y manual:** la traducción al español, con el plural de «%n comentario(s)», y el manual (Vistas y paneles), en los dos idiomas.

## Pruebas

`tst_todo` (7):

- **El núcleo:**
  - los comentarios de C, Python, SQL, HTML, ini y `/* */`;
  - lo que no cuenta (identificador, minúsculas, cadena, texto, palabra más larga, URL);
  - los archivos (binario, tamaño, inexistente).
- **El panel:**
  - **Documento actual:** lo que hay, lo que se escribe, y el doble clic a la línea.
  - **Carpetas de trabajo:** un proyecto con `node_modules` y `.git` (que no se miran) y un archivo abierto cambiado sin guardar; el doble clic abre otro archivo en la línea, y el alcance se guarda.
