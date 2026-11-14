# Fase 100 — Herramientas de JSON y YAML

Fecha: 2026-11-13

## Por qué

JSON y YAML son los formatos de la configuración de casi todo (paquetes, contenedores, CI, Kubernetes, APIs). Validar que un archivo está bien escrito, formatearlo, saber la ruta del valor en el que estás o convertir entre los dos son tareas de todos los días.

## Cómo lo hace Notepad++

Notepad++ solo los colorea (lexers JSON y YAML de Lexilla). El plugin JSON Viewer (en el catálogo de Edita desde la fase 87) formatea y valida JSON, pero no YAML, ni convierte entre los dos.

## Qué se hizo

- **`core/StructuredData`**, sin interfaz:
  - **Un analizador de JSON propio.** `QJsonDocument` ordena las claves y pasa los números a `double`, así que formatear con él cambiaría el documento. El propio da un árbol en el orden del texto, conserva cada número y cada cadena tal cual (`1.50`, `é`) y da errores en línea y columna con un mensaje claro: falta `:`, coma de más, literal desconocido, cadena sin cerrar, cero a la izquierda, texto de más.
  - **Un analizador de YAML** para lo que usan los archivos de configuración:
    - mapas y listas en bloque, también una lista a la altura de su clave y `- - x`;
    - colecciones `[ ]` y `{ }`, también en varias líneas;
    - cadenas simples, entre comillas (con escapes) y en bloque `|` y `>` con `+` y `-`;
    - comentarios, anclas y alias, `!!str`, y `---` y `...`.

    Los escalares simples se leen como en el esquema core de YAML 1.2 (`~`, `null`, `true`, números). Los errores, con línea y columna: sangría inesperada, tabuladores, comillas o corchetes sin cerrar, alias sin ancla, clave repetida.
  - **Escribir:**
    - `formatJson` con sangría (espacios o tabuladores) o en una línea;
    - `toYaml`, con comillas en las cadenas que se leerían como otra cosa (`"1.0"`, `"yes"`, `""`, `": "`, `" #"`).
  - **`pathAt`:** la ruta del nodo de una posición (`$.services.web.ports[1]`, `$['a b']`).
- **Editar › JSON/YAML:**
  - **Validar:** si hay un error, el cursor va a él y un aviso dice la línea, la columna y el motivo.
  - **Copiar la ruta del cursor.**
  - **Formatear y compactar JSON:** en un solo paso de deshacer, con la sangría del documento.
  - **Convertir JSON a YAML y YAML a JSON:** en un documento nuevo, con su lenguaje.

  JSON o YAML: el lenguaje del documento, su extensión o, si no, lo que parece.
- **Traducción y manual:** la traducción de los mensajes de error y el manual (Edición › JSON y YAML), en los dos idiomas.

## Pruebas

`tst_structured` (11):

- **JSON:**
  - el orden y los números;
  - formatear con espacios y tabuladores;
  - siete errores con su línea y columna;
  - el mismo resultado que `QJsonDocument` con un JSON real.
- **YAML:**
  - un Docker Compose con todo lo de arriba;
  - anclas, etiquetas y marcadores;
  - seis errores con su línea.
- **Ida y vuelta:** de JSON a YAML y de vuelta, el mismo árbol, con las cadenas que necesitan comillas.
- **La ruta:** en JSON y en YAML.
- **En el editor:** formatear, compactar, deshacer, copiar la ruta, validar (bien y con error, con el cursor en él) y convertir en los dos sentidos.
