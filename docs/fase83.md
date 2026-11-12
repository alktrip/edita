# Fase 83 — Historial local

Fecha: 2026-11-12

## Por qué

Es la primera de las funciones propias para después de la 1.0. Quien edita archivos fuera de Git (configuraciones, notas, scripts sueltos) no tiene forma de volver a lo que guardó hace una hora. Notepad++ solo guarda la versión anterior al guardar, y únicamente si se activa.

## Cómo lo hace Notepad++

Preferencias › Copia de seguridad › «Al guardar, copiar la versión anterior» (`Notepad_plus::doSave`, `BackupFeature`). Con `bak_simple` copia `archivo.bak` junto al archivo. Con `bak_verbose` deja `nppBackup/archivo.AAAA-MM-DD_HHMMSS.bak` en cada guardado. Edita ya lo tiene desde la fase 6 (`core/Backup`).

| N++ (`.bak`) | Edita, historial local |
|---|---|
| Apagado por omisión | Encendido |
| Archivos junto a los del usuario (o en una carpeta fija) | `<configuración>/localHistory/`, fuera de las carpetas de trabajo y de la nube |
| Una copia por guardado, aunque no cambie nada; sin límite | Sin copias repetidas; 50 versiones y 30 días por archivo, nada de más de 2 MB (configurable) |
| Copia lo que había antes de guardar | Guarda cada versión guardada y, la primera vez, también lo que había antes |
| Sin interfaz | Panel para comparar, restaurar, abrir y borrar |
| Al renombrar, las copias se quedan con el nombre viejo | El historial sigue al archivo |

## Qué se hizo

- **`core/LocalHistory`**: una carpeta por archivo (SHA-1 de la ruta), con `path` (la ruta) y una copia por versión (`AAAAMMDD-HHMMSS-mmm.v`, en UTC, con sufijo `-1`, `-2`… si coinciden). `record` no guarda lo que es igual a la última versión ni lo que pasa del límite, y después poda. La versión más nueva no se borra nunca. Además `versions`, `read`, `rename`, `removeVersion`, `clear` y `totalSize`.
- **Al guardar** (`saveBuffer` y `saveBufferTo`): si el archivo todavía no tiene historial, primero se guarda lo que hay en el disco y después lo guardado. Con **Archivo › Renombrar**, el historial pasa al nombre nuevo.
- **Archivo › Historial local** (`actionLocalHistory`) muestra el panel `LocalHistoryPanel` (a la derecha). Tiene las versiones del documento actual («Hoy 14:32:05», tamaño) y cuatro botones:
  - **Comparar** (o doble clic) usa la comparación de la fase 39: `startCompare` se partió en `compareWithBytes` y `beginCompare`, y la versión se abre como referencia de solo lectura en la otra vista.
  - **Restaurar** cambia todo el texto en un solo paso de deshacer y no guarda.
  - **Abrir** abre una pestaña de solo lectura.
  - **Borrar** pide confirmación.
- **Preferencias › Copia de seguridad**: sección «Historial local» (encendido, versiones, días, tamaño máximo) y un botón para borrar todo el historial, con lo que ocupa. Se guarda en `config.xml` como `<GUIConfig name="EditaLocalHistory" enabled versions days maxKB>`.
- **Manual** (Archivos › Historial local, en los dos idiomas) con una captura nueva, `localhistory.png`.

## Pruebas

`tst_localhistory` (16):

- **El núcleo:** el orden, sin repetidas, dos guardados en el mismo milisegundo, los límites (versiones, días y tamaño, sin perder la última), renombrar, borrar y la carpeta, que no es la de la nube.
- **Las preferencias.**
- **En la ventana:** el original más cada guardado; apagado no guarda nada; el panel sigue al documento y a cada guardado; restaurar y deshacer; comparar; renombrar.

Bug encontrado al escribir las pruebas: si el panel se abría desde el menú con la ventana sin mostrar, no se llenaba, porque `visibilityChanged` no llega. Ahora se llena al abrirlo.
