# Fase 94 — Los paneles recuerdan su sitio

Fecha: 2026-11-13

## Por qué

Los paneles (lista de documentos, mapa, lista de funciones, terminal, Git…) se pueden mover a cualquier lado, agrupar en pestañas o dejar flotando, pero al volver a abrir Edita volvían a su sitio de fábrica. Solo se recordaba si estaban visibles. El manual ya decía «Edita recuerda dónde estaban».

## Cómo lo hace Notepad++

`DockingManager` guarda en `config.xml`, dentro de `<GUIConfig name="DockingManager">`:

- el ancho y el alto de cada zona (`leftWidth`, `rightWidth`, `topHeight`, `bottomHeight`);
- las ventanas flotantes (`<FloatingWindow>`);
- de cada panel (`<PluginDlg>`), su zona actual, la anterior y si estaba visible;
- la pestaña activa de cada zona (`<ActiveTabs>`).

Al arrancar, los paneles de los plugins vuelven a esa zona cuando el plugin los registra.

## Qué se hizo

- **Al cerrar**, `saveWindowState` guarda el estado de los paneles con `QMainWindow::saveState`, en `<GUIConfig name="EditaDockState" state="…">`, junto a `AppPosition`. Es lo mismo que guarda N++ (zona, tamaño, flotantes y pestañas), en el formato de Qt.
- **Al arrancar**, `restoreWindowGeometry` lo restaura.
  - El estado lleva una versión (`kDockStateVersion`); si cambia, el estado anterior se ignora.
  - Un estado roto también se ignora, y los paneles van a su sitio de fábrica.
- **Los paneles de los plugins** se crean después, cuando el plugin los registra (`NPPM_DMMREGASDCKDLG`). Primero se restaura su sitio con `restoreDockWidget`; si no había nada guardado, van a la zona que pide el plugin. Siguen ocultos hasta `NPPM_DMMSHOW`, como en N++.
- **Visibilidad:** sigue también en `<GUIConfig name="multiInst">`, con los nombres de N++.
- **Manual:** la sección Paneles, en los dos idiomas.

## Pruebas

- **`tst_panels`:**
  - `panelsRememberTheirPlaceAndSize`: el mapa movido abajo, el ancho de la lista de documentos, la lista de funciones flotando con su tamaño, dos paneles en pestañas, y un panel oculto que sigue oculto.
  - `brokenPanelStateIsIgnored`: con un estado roto, los paneles van a su sitio.
- **`tst_hexeditor` (`pluginPanelRemembersItsPlace`):** el panel del editor hexadecimal, movido arriba, vuelve arriba en la sesión siguiente. Esta prueba encontró que llamar a `restoreDockWidget` después de `addDockWidget` no restaura nada: encuentra el panel recién colocado.
