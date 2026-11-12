# Fase 81 — Pulido de la interfaz

Fecha: 2026-11-12

## Por qué

Antes de la 1.0, un repaso de lo que se ve y se toca: textos, atajos, letras de acceso de los menús y el aspecto de los diálogos. Lo que se puede comprobar solo queda en una prueba nueva (`tst_uipolish`), para que no vuelva a pasar.

## Qué se hizo

### F12: dos órdenes con el mismo atajo

La fase 37 dio F12 a **Ir a la definición** y la 67 se lo dio a **Post-It**, como en N++. Con dos órdenes en el mismo atajo, Qt no ejecuta ninguna (`activatedAmbiguously`). Por eso F12 no hacía nada. Y quien entraba en Post-It por el menú no podía salir: Post-It esconde el menú y solo se sale con F12.

| N++ | Edita |
|---|---|
| F12: Post-It (`IDM_VIEW_POSTIT`, 44009) | F12 sigue siendo Post-It, como en N++: los `shortcuts.xml` de N++ lo esperan así |
| — | **Ir a la definición** pasa a **Alt+F12**. Mayús+F12 (Buscar referencias) no cambia |

### Letras de acceso de los menús

Había unas 40 letras repetidas en los menús en inglés y unas 45 en español: Alt+letra no llevaba a una orden, sino que pasaba de una a otra. Casi todas venían de letras que Edita añadió a órdenes que en N++ no tienen ninguna.

- **Inglés:** las letras que coinciden con N++ se quedan. Si N++ usa otra letra para el mismo texto, se toma esa (`Find in Fi&les…`, `&Document Map`). Las órdenes propias de Edita pasan a una letra libre de su texto, preferiblemente al principio de una palabra.
- **Español:** sin repeticiones evitables. Las órdenes más usadas conservan su letra: **&Reemplazar…** y **Guardar &como…**.
- En los menús con más órdenes que letras (Ver, Editar, Buscar) alguna se repite. Es preferible a quitarla: Qt pasa de una orden a otra con cada pulsación. Lo mismo para tres pares que vienen de N++ (`Load Sess&ion…` / `Save Sess&ion…`, `&Paste` / `Character &Panel`, `&Clone to Other View` / `Syn&chronize Horizontal Scrolling`).

La regla de la prueba: en una repetición, ninguna de las órdenes debe tener otra letra libre en su texto.

### Diálogos

| Dónde | Antes | Ahora |
|---|---|---|
| Ejecutar una macro varias veces | «1 veces» / «1 times» | Plural de Qt (`tr(" time(s)", nullptr, n)`): «1 vez», «2 veces»; «1 time», «2 times» |
| Diálogo Buscar, «Modo de búsqueda» | El recuadro se estiraba a la altura de las casillas de la izquierda. En Marcar, con dos casillas más, las opciones quedaban muy separadas | El recuadro tiene su altura y el espacio sobrante queda debajo |
| Acerca de | Sin la dirección del proyecto | Con el enlace a la página del proyecto |

Se revisaron también con capturas Acerca de, Editor de columnas, Ir a, Buscar caracteres en un rango, Resumen, Información de depuración, Asignación de atajos, Administrador de plugins y las cuatro pestañas del diálogo Buscar, en español. Fuera de lo de la tabla, no hubo más que corregir.

### La prueba nueva, `tst_uipolish`

- `menuMnemonicsAreUnique` (inglés y español): sin letras repetidas evitables.
- `shortcutsAreUnique`: un atajo, una orden.
- `f12TogglesPostIt`: F12 de verdad (pulsado, no la orden), entrar y salir de Post-It.
- `aboutShowsTheProjectPage`.
- `menuTextsFollowTheRules` (inglés y español): «…» y no «...», sin espacios de más, sin «:» al final.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| F12 entra y sale de Post-It; Ir a la definición en Alt+F12 | ✅ `tst_uipolish`: `f12TogglesPostIt` (falla con el código de antes) |
| Ningún atajo repetido entre las órdenes | ✅ `shortcutsAreUnique` |
| Sin letras de acceso repetidas evitables, en inglés y en español | ✅ `menuMnemonicsAreUnique` |
| El mismo número de letras de acceso en cada traducción, y las traducciones completas | ✅ `tst_i18n` |
| «1 vez» / «2 veces» | ✅ capturas; `edita_en.ts` y `edita_es.ts` con sus plurales |
| Pasan las 67 suites, sin avisos con GCC 16 y clang | ✅ |
