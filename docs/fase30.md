# Fase 30 — Página de inicio: resultado

Fecha: 2026-10-21

Objetivo: una página de inicio moderna en lugar del «new 1» vacío con el que arranca Notepad++. **No existe en N++.**

## Qué se construyó

| Parte | Qué hace |
|---|---|
| `ui/WelcomePage` | Una página centrada (hasta 980 px de ancho, con desplazamiento) con tarjetas de esquinas redondeadas que se resaltan al pasar el ratón. **Encabezado**: el logo, «Edita», el lema y la versión. **Para empezar**: cuatro tarjetas (Nuevo archivo, Abrir archivo, Abrir carpeta como espacio de trabajo y Paleta de órdenes) con un icono en el color de acento y su atajo dibujado como teclas. **Archivos recientes** (hasta 8, con su icono por tipo y la carpeta, con `~` para la carpeta personal) y **carpetas recientes**. **Apariencia**: un selector Claro / Oscuro / Sistema y la lista de temas. **Atajos útiles**: Ir a archivo, Ir a símbolo, Buscar, Buscar en archivos, Reemplazar, Editor de columnas, Comentar y Duplicar línea, con las teclas que tengan asignadas en el Shortcut Mapper. **Pie**: «Mostrar esta página cuando no hay documentos». Los colores salen de la paleta de la interfaz (claro u oscuro) y del verde de Edita; se recalculan al cambiar de modo |
| Cuándo se ve | Mientras el único documento abierto es uno nuevo, sin cambios y vacío, y la vista secundaria está cerrada: al arrancar sin sesión, al cerrar todo (N++ deja un «new» vacío)… La página sustituye al editor (un `QStackedWidget` en el centro de la ventana); las órdenes y los atajos siguen funcionando. También en **Ayuda › Página de inicio** |
| Cómo se sale | **Escribiendo**: la primera tecla va al documento nuevo, y Intro solo pasa al editor. También con Esc, «Nuevo archivo» (se queda en el documento vacío, sin crear otro), abriendo un archivo (de la página, el menú, arrastrándolo o desde otra instancia) o creando otro documento. Una vez descartada, no vuelve para ese documento |
| Carpetas recientes | Las que se abren como espacio de trabajo (hasta 10), en `config.xml`: `<GUIConfig name="EditaRecentFolders" paths="…">`. Abrir una desde la página la añade al Explorador de carpetas |
| Preferencias | General › «Mostrar la página de inicio cuando no hay documentos» (`EditaWelcomePage`, activada de serie) |
| Icono | `command-palette.svg`, para la tarjeta de la paleta, con el estilo de los demás iconos de la interfaz |

## Problemas encontrados

- `setStyleSheet` manda un `PaletteChange`, y la página se volvía a estilizar al recibirlo: una recursión sin fin que hacía fallar todas las pruebas con ventana. Ahora el estilo solo se recalcula si cambian el modo o los colores de la interfaz.
- Los archivos cerrados con «Cerrar todo» pasan a recientes después de que la página se rellena. Ahora la página se actualiza también cuando cambia la lista de recientes.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Se ve al arrancar; escribir la quita y el texto va al documento; vuelve al cerrar todo; abrir un archivo la quita; Ayuda › Página de inicio la muestra y Esc la cierra | ✅ `tst_welcome`: `shownWhileTheOnlyDocumentIsNewAndEmpty` |
| Archivos y carpetas recientes; un reciente lo abre; «Nuevo archivo» no crea otro documento; la tarjeta de la paleta la abre con «>» | ✅ `cardsAndRecentItems` |
| Claro / Oscuro / Sistema cambian las preferencias; el tema se aplica al modo actual; sin «Mostrar al iniciar» no aparece | ✅ `appearanceFromThePage` |

Se revisaron capturas en claro y en oscuro. Pasan las 35 suites y el build no da ningún warning.
