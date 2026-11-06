# Fase 67 — Ver y Pestañas como en Notepad++

Fecha: 2026-11-06

## Por qué

Es el primer bloque de [lo que tiene Notepad++ y Edita no](paridad-npp.md): las órdenes de los menús Ver y Archivo que más se notan al venir de N++.

## Cómo lo hace N++

- **Archivo:** el submenú *Close Multiple Documents* (`IDM_FILE_CLOSEALL_BUT_CURRENT`, `_BUT_PINNED`, `_TOLEFT`, `_TORIGHT`, `_UNCHANGED`).
- **Ver:** *Always on Top*, *Toggle Full Screen Mode* (F11), *Post-It* (F12), el submenú *Tab* y *Synchronize Vertical/Horizontal Scrolling*.
- **El submenú *Tab*:**
  - ir a la pestaña 1…9 con Ctrl + el número del **teclado numérico**, a la primera, la última, la siguiente y la anterior;
  - mover la pestaña: al principio, al final, adelante y atrás (Ctrl+Mayús+AvPág/RePág);
  - cinco colores y quitar el color.
- **Fijar** (`IDM_PINTAB`, N++ 8.6): la pestaña pasa a la zona de las fijadas, al principio. Al desfijarla, va al final. En vez del botón de cerrar lleva un alfiler, que la desfija. No se puede arrastrar de una zona a la otra (`TabBar::exchangeTabItemData`). *Close All but Pinned* cierra las demás en las dos vistas.
- **La sesión** guarda `tabPinned` y `tabColourId` en cada `<File>`.
- **Los colores** son los «Tab color N» de `stylers.model.xml`, uno para el modo claro y otro para el oscuro, saturados (`getIndividualTabColor`).
- **El desplazamiento sincronizado** mantiene la diferencia que había entre las dos vistas al activarlo.

## Qué cambió

| Dónde | Qué |
|---|---|
| **Documento** (`Buffer`) | `isPinned`/`setPinned` y `tabColor`/`setTabColor` (−1 o 0…4), con el aviso `BufferChange::Tab` |
| **Sesión** | `tabPinned` y `tabColourId` con sus valores reales (antes se escribían fijos: `no` y `-1`) |
| **Barra de pestañas** (`DocTabBar`) | El color, como una franja arriba de la pestaña (con los tonos de N++ para claro y oscuro). Fijada: un alfiler a la izquierda (`pin.svg`) que la desfija; su botón de cerrar sale de la pestaña (oculto seguiría ocupando el hueco) y vuelve al desfijarla, también si mientras tanto cambió la preferencia de los botones de cerrar |
| **`EditorPane`** | Al fijarla, la pestaña va detrás de las otras fijadas (N++ la pone la primera; así una sesión conserva el orden); al desfijarla, al final. Al soltar una pestaña arrastrada, las fijadas vuelven a quedar primero |
| **Archivo › Cerrar varios documentos** | Todo menos el actual, menos las fijadas, a la izquierda, a la derecha y lo que no tiene cambios |
| **Ver** | Siempre visible; pantalla completa (F11, vuelve a maximizada si lo estaba); Post-It (F12: sin marco, menús, barras ni pestañas; F11 y F12 siguen funcionando con la barra de menús oculta); desplazamiento sincronizado vertical y horizontal; el submenú Pestaña con todo lo anterior y **Fijar la pestaña** |
| **Menú de la pestaña** | Fijar/desfijar y *Aplicar color* |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Fijar y desfijar: posición, alfiler, botón de cerrar (también tras cambiar la preferencia), sin cruzar la frontera al mover ni al arrastrar; cerrar todo menos las fijadas | ✅ `tst_viewtabs`: `pinTabs` |
| Ir a la pestaña N (Ctrl+teclado numérico), primera y última; colores; la sesión recuerda fijadas y colores | ✅ `colorsGoToAndSession` |
| Cerrar lo que no tiene cambios | ✅ `closeUnchanged` |
| Siempre visible, pantalla completa y Post-It (que se quita con F12) | ✅ `windowModes` |
| Las dos vistas se desplazan juntas con su diferencia, y dejan de hacerlo al desactivarlo | ✅ `synchronizedScrolling` |
| El manual cita cada orden en los dos idiomas | ✅ `tst_manual` |
| Pasan todas las suites, el build no da avisos y clang no tiene nada que decir | ✅ |
