# Fase 68 — Edición y Herramientas como en Notepad++

Fecha: 2026-11-06

## Por qué

Es el segundo bloque de [lo que tiene Notepad++ y Edita no](paridad-npp.md): las órdenes de edición y las herramientas que más se usan, más la información de depuración, que sirve para los reportes de las betas.

## Cómo lo hace N++ y qué hace Edita

| Orden de N++ | En Edita |
|---|---|
| **Editar › Insert › Date Time** (short, long, customized; `IDM_EDIT_INSERT_DATETIME_*`) | **Editar › Insertar › Fecha y hora**. La corta y la larga, con el formato del sistema (`QLocale`): la hora sin segundos y la fecha, o al revés con *reverseDefaultOrder*. La personalizada usa el formato de N++ (el de Windows: `yyyy`, `MM`, `dd`, `HH`, `ddd`, `MMMM`, `tt`…; `core/DateTimeFormat` lo pasa al de Qt: `tt` es AM/PM), que se edita en **Preferencias › Varios**, con una vista previa. Se guarda como N++: `<GUIConfig name="insertDateTime" customizedFormat="…" reverseDefaultOrder="…">`. Sustituyen la selección en un solo paso de deshacer, y en una macro se graban como orden (al reproducirla, la hora de ese momento) |
| **Editar › Copy to Clipboard** (`IDM_EDIT_FULLPATHTOCLIP`, `…FILENAMETOCLIP`, `…CURRENTDIRTOCLIP`, `IDM_EDIT_COPY_ALL_NAMES`, `…_PATHS`) | **Editar › Copiar al portapapeles**: la ruta, el nombre y la carpeta del actual; los nombres y las rutas de todos los abiertos (las dos vistas, sin repetir), uno por línea |
| **Multi-select All / Next** (4 variantes cada uno), **Undo the Latest Added Multi-Select**, **Skip Current & Go to Next** (`IDM_EDIT_MULTISELECT*`) | Igual, con `SCI_MULTIPLESELECTADDEACH`/`ADDNEXT` y las banderas de búsqueda de cada variante. Sin selección, la palabra del cursor (y no «hay selección», que con la principal vacía hace que `ADDEACH` no termine, como avisa N++). Como en N++, sin atajo: Ctrl+D es Duplicar la línea |
| **Ver › Summary…** (`IDM_VIEW_SUMMARY`) | **Ver › Resumen…**: ruta, creado, modificado, caracteres sin fines de línea, palabras (la misma expresión regular que `wordCount` de N++), líneas, tamaño y la selección (caracteres, bytes, rangos) |
| **Ver › Monitoring (tail -f)** (`IDM_VIEW_MONITORING`) | **Ver › Seguimiento (tail -f)**: solo para un archivo guardado y sin cambios; lo pone en solo lectura y, cuando cambia en el disco, lo vuelve a leer sin preguntar y va al final. Avisa el sistema de archivos y, además, una comprobación por segundo, también con Edita en segundo plano (la de los demás archivos solo se hace con la ventana activa) |
| **Tools › MD5/SHA-1/SHA-256/SHA-512** (`IDM_TOOL_*`, `HashFromTextDlg`, `HashFromFilesDlg`) | El menú **Herramientas** (`ui/HashDialog`, con `QCryptographicHash`): desde un texto, que se calcula mientras se escribe (con «Cada línea por separado»); desde archivos, con el formato de `sha256sum`; y desde la selección, al portapapeles |
| **? › Debug Info…** (`IDM_DEBUGINFO`) | **Ayuda › Información de depuración…**: versión y arquitectura, compilador, Qt (en uso y con el que se compiló), ruta (y la del AppImage), línea de órdenes, configuración, sesión y modo *snapshot*, sistema, idioma y plugins cargados, con un botón para copiarlo |

Además:
- **Avisos informativos:** el `Prompter` tiene `inform`, para el Resumen.
- **Identificadores de Notepad++:** las órdenes de las fases 67 y 68 ya los tienen (`IDM_*`). Con ellos, los atajos de un `shortcuts.xml` de N++ se entienden, y la fecha y la selección múltiple se graban en las macros.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| El formato de N++ (con `tt` y texto entre comillas) y los del sistema; la fecha sustituye la selección en un paso; la preferencia se guarda como en N++ | ✅ `tst_edittools`: `dateTimeFormats`, `insertDateTime` |
| Copiar la ruta, el nombre, la carpeta y las listas | ✅ `copyToClipboard` |
| Selección múltiple: todas, con cada variante, la siguiente, saltar y deshacer | ✅ `multiSelect` |
| El Resumen cuenta como N++ (también la `ñ` y los fines de línea `\r\n`) | ✅ `summary` |
| Seguimiento: solo lectura, se vuelve a leer sin preguntar al crecer el archivo y va al final; no con un documento sin guardar | ✅ `monitoring` |
| MD5, SHA-1, SHA-256 (con los vectores de prueba conocidos), línea a línea, de archivos y de la selección | ✅ `hashes` |
| La información de depuración | ✅ `debugInfo` |
| El manual cita cada orden en los dos idiomas | ✅ `tst_manual` |
| Pasan todas las suites, el build no da avisos y clang no tiene nada que decir | ✅ |
