# Fase 97 — Filtro en Carpeta como espacio de trabajo

Fecha: 2026-11-13

## Por qué

En un proyecto grande, encontrar un archivo en el árbol de Carpeta como espacio de trabajo obliga a abrir carpeta tras carpeta. «Ir a archivo» (Ctrl+P) lo abre, pero no lo muestra en su sitio ni deja usar el menú del árbol (abrir la carpeta, la terminal, Buscar en archivos…).

## Cómo lo hace Notepad++

El panel Folder as Workspace de Notepad++ no tiene filtro: solo el árbol, con abrir, localizar el archivo actual y plegar todo.

## Qué se hizo

- **Campo «Filtrar»** sobre el árbol (`folderFilter`). Busca 250 ms después de la última tecla.
  - **Palabras:** cada una tiene que estar en la ruta relativa a su carpeta, sin importar mayúsculas (`src main` encuentra `src/main.cpp`).
  - **Comodines** (`*.cpp *.h`): por nombre, con `wildcardMatch`, como en Buscar en archivos.
- **El recorrido** va en otro hilo, con el de «Ir a archivo» (`scanFiles`: sin carpetas ocultas ni `node_modules`). Se cancela si cambia el texto.
- **El árbol filtrado** tiene cada carpeta de trabajo, con las carpetas de los archivos que coinciden, todo desplegado. Son los mismos elementos que el normal: doble clic, menú y estado de Git funcionan igual. Muestra hasta 5000 archivos y, si hay más, lo dice.
- **El árbol de antes:**
  - mientras se filtra, `state()` (lo que va a la sesión) devuelve el árbol de antes;
  - al vaciar el filtro, vuelve con sus carpetas desplegadas y la selección;
  - agregar o quitar una carpeta, localizar el archivo actual o restaurar una sesión terminan el filtro.
- **Manual y traducción:** el manual (Explorador de carpetas), en los dos idiomas, y la traducción.

## Pruebas

`tst_panels` (`folderFilter`), con un proyecto con `node_modules`:

- **Palabras**, también en mayúsculas y varias a la vez.
- **Comodines.**
- **Mientras se filtra:** el estado de la sesión no cambia, las raíces tampoco, y el doble clic abre el archivo.
- **Sin coincidencias:** la carpeta queda vacía.
- **Al vaciar:** el árbol de antes, con la selección.
