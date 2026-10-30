# Fase 47 — Sesiones con nombre: resultado

Fecha: 2026-10-30

Edita ya restauraba la sesión anterior (fase 5) con el formato de `session.xml` de N++. Faltaba lo
que N++ ofrece en Archivo: guardar y cargar sesiones con nombre.

## Qué se construyó

| Parte | Equivale en N++ a | Qué hace |
|---|---|---|
| Archivo › **Guardar la sesión…** | `IDM_FILE_SAVESESSION`, `fileSaveSession` | Guarda los archivos abiertos de las dos vistas con su posición, lenguaje, codificación, solo lectura y marcadores, y la pestaña activa. Solo los archivos que existen: ni copias del modo snapshot ni documentos sin título (`getCurrentOpenedFiles` sin `includeUntitledDoc`). Con el Explorador de carpetas a la vista, también sus carpetas (`_includeFileBrowser`) |
| Archivo › **Cargar una sesión…** | `IDM_FILE_LOADSESSION`, `fileLoadSession` + `loadSession` | Abre los archivos de la sesión **además** de los abiertos, en su vista, y añade sus carpetas al explorador. Los que no existen se listan en la barra de avisos. Un archivo que no es una sesión da un aviso |
| Extensión de las sesiones | `_definedSessionExt`, `isFileSession` | Preferencias › Copias de seguridad › «Extensión de los archivos de sesión» (`<GUIConfig name="sessionExt">` como en N++). Los diálogos la usan como filtro y extensión por defecto, y abrir un archivo con ella (diálogo, arrastrar, administrador de archivos, línea de órdenes) carga la sesión |
| `--openSession archivo` | `-openSession` | Carga esa sesión. También llega a la instancia que ya está abierta (campo `session` de la petición) |

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| «Guardar Carpeta como espacio de trabajo» al guardar | Casilla en el diálogo, marcada si el panel está abierto | Se guardan si el panel está a la vista | El diálogo de archivos del escritorio no admite casillas |
| Cargar en una instancia nueva (`multiInstOnSession`) | Opción de Multi-instancia | No | Edita no tiene ese modo de instancias |
| Recordar la sesión cargada para guardarla al salir | Con `multiInstOnSession` | No | Ligado al modo anterior |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Guardar: archivos de las dos vistas, sin documentos sin título ni copias; con y sin carpetas | ✅ `tst_namedsessions`: `savesOnlyRealFiles` |
| Cargar: se suman a los abiertos, en su vista, con las carpetas; avisa de los que faltan; rechaza lo que no es una sesión | ✅ `loadsAddingToWhatIsOpen` |
| Menú con la extensión (filtro, extensión añadida); abrir un archivo con la extensión carga la sesión | ✅ `menuAndExtension` |
| `--openSession` por la petición de instancia única | ✅ `openSessionFromTheCommandLine` |
| Manual en los dos idiomas | ✅ `tst_manual` |

Pasan las 47 suites y el build no da ningún warning. En una de las corridas en paralelo,
`tst_pythonscript` se quedó colgado hasta el tiempo límite; solo tarda 1,5 s y pasó en las demás
corridas, así que se anota como intermitente.
