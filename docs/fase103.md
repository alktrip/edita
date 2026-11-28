# Fase 103 — Al día con Notepad++ (finales de noviembre)

Fecha: 2026-11-27

## Por qué

Después de la rc.7, Notepad++ añadió 11 cambios a su rama principal (hasta `29009bf96`, del 28 de noviembre). Antes de la 1.0 se revisaron uno por uno.

## Qué cambió en Notepad++ y qué pasa en Edita

| Cambio en N++ | En Edita |
|---|---|
| **Ajuste de línea por documento** (c3ed07bda; cierra cuatro issues de años) | **Hecho** (abajo) |
| La lista de funciones de Bash se rompía con un here-document con el delimitador entre comillas (`<<'EOF'`, `<<"EOF"`, `<<\EOF`) | **Edita tenía el mismo fallo** (usa el mismo `bash.xml`): corregido |
| Iconos de solo lectura y seguimiento iguales en las pestañas, la Lista de documentos y Ctrl+Tab | Edita calculaba el estado en tres sitios, con el mismo resultado, pero sin el de seguimiento: ahora hay uno solo, con un icono propio para Ver › Seguimiento |
| Structured Text y nombres al día en el tema Deep Black | **Hecho**: el tema de N++ |
| Al cambiar entre modo claro y oscuro, los documentos abiertos con un UDL de dos versiones no cambiaban | Edita ya lo hacía (`applyAppearance`) |
| Las marcas del menú se perdían al cambiar el idioma | No aplica: en Edita el idioma se aplica al reiniciar |
| «Guardar como» no añadía la extensión con puntos en la carpeta | No aplica: Edita la lee con `QFileInfo::suffix` |
| Paneles, iconos de la lista de funciones, tamaño mínimo de la ventana, pdb | Solo de Windows |

## Qué se hizo

- **Ajuste de línea por documento**, como el próximo Notepad++:
  - Cada documento tiene el suyo (`Buffer::wordWrap`). Ver › Ajuste de línea cambia solo el actual, en las dos vistas si está clonado, con la misma línea arriba; la marca del menú sigue al documento.
  - La preferencia pasa a ser «Activar el ajuste de línea por omisión en cada documento» (Preferencias › Edición): con ella empiezan los documentos nuevos y los que se abren; cambiarla no toca los abiertos.
  - La sesión guarda el de cada archivo (`wordWrap="yes"`/`"no"`, el mismo atributo que N++); una sesión sin él usa la preferencia.
  - Rendimiento › Desactivar el ajuste de línea: un archivo grande se abre sin ajuste y los demás conservan el suyo (antes se quitaba a todos).
- **Bash:** el `bash.xml` de N++ con el delimitador entre comillas.
- **Estado de los documentos** (`DocTabBar::stateOf`): seguimiento, solo lectura, con cambios o guardado, el mismo en las pestañas, la Lista de documentos y la paleta de archivos. Seguimiento tiene su icono (un ojo), también en los iconos por tipo de archivo.
- **Deep Black:** el de N++ (Structured Text y los nombres de los estilos al día), con `modelDate="20261127"` para que llegue a las copias del usuario.

## Pruebas

- `tst_preferences` (`wordWrapPerDocument`): dos documentos con ajuste distinto, el menú al cambiar de uno a otro, las dos vistas con un documento clonado, la preferencia que no toca los abiertos, un documento nuevo con ella y la sesión con `wordWrap` de cada archivo al guardarla y al restaurarla.
- `tst_preferencepages`: un archivo grande se abre sin ajuste y el otro documento lo conserva.
- `tst_panels`: el `unitTest` de N++ de los here-documents entre comillas (cinco funciones; con el `bash.xml` anterior salía una).
- `tst_edittools`: el estado de seguimiento en la pestaña, al activarlo y al quitarlo.
