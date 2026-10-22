# Fase 26 — MarkdownViewer++: resultado

Fecha: 2026-10-21

Objetivo: poder usar [MarkdownViewer++](https://nea.github.io/MarkdownViewerPlusPlus/) 0.8.2 (nea, MIT), que en el administrador de plugins salía como incompatible. El original está escrito en **C# (.NET Framework, WinForms)** y usa Markdig, HtmlRenderer, PdfSharp y Outlook. No se puede cargar en Linux ni compilar con una receta (fase 15), que solo sirve para plugins en C/C++. Por eso es un **port nativo** que se incluye con Edita, como Python Script. Reproduce el comportamiento del original a partir de su código (`MarkdownViewer.cs`, `MarkdownViewerConfiguration.cs`, `Forms/AbstractRenderer.cs`, `Forms/MarkdownViewerRenderer.cs`, las opciones de `Forms/OptionsPanel*.cs`).

## Qué se construyó

| Parte | Original | En Edita |
|---|---|---|
| Menú Plugins | `CommandMenuInit`: MarkdownViewer++ (Ctrl+Mayús+M), Synchronize scrolling, Options, About | Igual, con los mismos nombres en inglés y los separadores |
| Panel | Un panel acoplable con la vista previa, que se marca en el menú | Igual. Un panel a la derecha (`NPPM_DMMREGASDCKDLG`) con un `QTextBrowser`. Al cerrarlo con su X, el menú se desmarca |
| Markdown | Markdig con `UseAdvancedExtensions` | **md4c** 0.5.2 (CommonMark con las extensiones de GitHub: tablas, tachado, listas de tareas, enlaces automáticos), en `third_party/md4c`. Es el analizador que usa Qt |
| Actualización | Con `SCN_UPDATEUI` después de un cambio (`SCN_MODIFIED`) y al cambiar de documento (`NPPN_BUFFERACTIVATED`) | Igual, con una pausa de 150 ms para no volver a dibujar con cada tecla. Conserva la posición del panel |
| Desplazamiento sincronizado | La proporción desplazada del editor, aplicada al panel | Igual (primera línea visible ÷ líneas que se pueden desplazar) |
| Barra de menús del panel | Export as (HTML, PDF), Send (HTML e-mail, Text e-mail, Printer, HTML to Clipboard), View (Refresh), ? (Options, About) | Igual, salvo el correo HTML (ver divergencias) |
| Exportar a HTML | `BuildHtml`: página completa con el CSS | Igual: `<!DOCTYPE html>`, título = nombre del archivo, CSS (los `@import` del CSS propio, el CSS de serie y el resto del propio, como `getCSS`) |
| Exportar a PDF | PdfSharp con tamaño de página, orientación y márgenes en mm | `QPdfWriter` + `QTextDocument` con las mismas opciones (A0–A5, B4, B5, Letter, Legal, Ledger, Tabloid, Executive, Folio) |
| Imprimir | Vista previa de impresión de Internet Explorer | `QPrintPreviewDialog` |
| Portapapeles | HTML completo y, como texto, el HTML convertido | Igual |
| Extensiones | `ValidateFileExtension`: vacío = todos; sin extensión, solo los nuevos si `inclNewFiles`; si no, el aviso con las extensiones configuradas | Igual, con el mismo aviso |
| Opciones | Diálogo con General, HTML y PDF | Igual, en pestañas: extensiones e incluir los nuevos; CSS propio y abrir el HTML exportado; orientación, tamaño, márgenes y abrir el PDF exportado |
| Configuración | `MarkdownViewerPlusPlus.ini` en la carpeta de configuración de los plugins, sección `[MarkdownViewerPlusPlus]`, una clave por campo; los saltos de línea del CSS como ` \n ` | Igual, con las mismas claves y el mismo formato. Se lee y escribe como `GetPrivateProfileString`, con los valores tal cual: `QSettings` no sirve porque toma `;` como comentario, `\n` como escape y las comas como listas |
| Traducción | — | El panel y los diálogos en español (`translations/markdownviewer_es.ts`); el menú en inglés, como el original |

Para otros plugins y scripts, `NPPM_MSGTOPLUGIN` a `MarkdownViewerPlusPlus.so` exporta el documento actual sin diálogos: `internalMsg` 1 a HTML y 2 a PDF, con `info` = ruta (UTF-8).

En el **administrador de plugins**, MarkdownViewer++ aparece como instalado (está en la lista de Edita). Al consultar la lista de Notepad++, ya no se muestran como incompatibles los plugins que Edita ofrece, ya sea como port en su lista o con una receta. Antes, MarkdownViewer++ y XML Tools salían ahí con el aviso de «DLL de Windows».

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Correo | «as HTML E-mail» y «as Text E-mail» con Outlook | Solo «as Text E-mail», con el cliente de correo del sistema (`mailto:` con el Markdown) | No hay Outlook; `mailto:` no admite HTML |
| Vista del panel | HtmlRenderer (HTML y CSS 2) | `QTextBrowser`, con el subconjunto de HTML y CSS de Qt. Las casillas de las listas de tareas se dibujan como ☐/☑. Los colores siguen al tema claro u oscuro | El CSS propio se aplica completo en la exportación; en el panel, lo que entienda Qt |
| Imágenes SVG remotas | Las descarga para los «badges» | Solo imágenes locales (rutas relativas al documento) | Sin red en la vista previa |
| Enlaces | — | Los anclas (`#…`) desplazan el panel; los enlaces a archivos locales se abren en Edita; los demás, en el navegador | — |
| Icono en la barra de herramientas | `NPPM_ADDTOOLBARICON` | No | Edita aún no tiene esa API para plugins |
| CSS de serie | Solo evita saltos de página dentro de párrafos, tablas y listas | Esa regla y un estilo legible (tipografía, tablas, código, citas) para exportar | El original se apoyaba en los estilos de HtmlRenderer |

## Problemas encontrados

- **Dos md4c en el mismo proceso:** Qt Gui trae su propia copia de md4c, con otra versión y otros valores de las opciones. Con los símbolos visibles, `md_html` del plugin llamaba a `md_parse` de Qt, y el tachado salía como `<x-equation>`. md4c se compila con los símbolos ocultos.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| El plugin carga con el menú del original (nombres, separadores, Ctrl+Mayús+M) | ✅ `tst_markdownviewer`: `menuOfTheOriginal` |
| La orden abre el panel con encabezados, negrita, tablas, tareas y tachado; al escribir se actualiza; otra vez la orden lo oculta; cerrarlo con la X desmarca el menú | ✅ `panelRendersTheDocument` |
| Con extensiones configuradas, un `.txt` muestra el aviso y un `.md` se muestra | ✅ `fileExtensionsAreFiltered` |
| Exportar a HTML: página completa con el CSS en el orden de `getCSS`; a PDF: tamaño y orientación de las opciones (Carta apaisada = 792 × 612 pt) | ✅ `exportsHtmlAndPdf` |
| El desplazamiento sincronizado se guarda en el `.ini` y el panel sigue al editor | ✅ `synchronizeScrollingIsSavedAndFollowsTheEditor` |
| Los plugins de la lista de N++ que Edita ofrece no salen como incompatibles | ✅ `tst_pluginsadmin`: `notepadPlusPlusPluginsThatEditaOffersAreNotIncompatible` |

Se revisaron capturas del panel en modo claro y oscuro. Pasan las 31 suites y el build no da ningún warning.
