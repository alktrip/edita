# Fase 80 — Sin avisos de beta y capturas nuevas

Fecha: 2026-11-12

## Por qué

Rumbo a la 1.0, los textos para quien llega al proyecto hablaban de «la beta»: el README («Estado: beta pública»), la guía PROBAR.md («Probar Edita 0.9.0 beta») y el enlace de la plantilla de issues. Las capturas del README y del manual eran de antes de las fases 77-78: les faltaba el menú Herramientas, y el manual no tenía captura del diálogo Buscar.

## Qué se hizo

### Los textos

La siguiente versión todavía puede ser de prueba (1.0.0-rc.1). Por eso los textos nuevos no dicen «beta» ni «estable»: sirven para las versiones de prueba y para la 1.0 sin volver a tocarlos.

| Dónde | Antes | Ahora |
|---|---|---|
| README, estado | «beta pública (0.9.0). Casi todo está hecho; falta que lo use gente real» | Tiene casi todo lo de Notepad++ (con enlace a [paridad-npp.md](paridad-npp.md)); antes de cada versión estable se publica una versión de prueba |
| README, enlaces | «Probar la beta», «Probar la beta y reportar» | «Probar Edita», «Probar y reportar» |
| PROBAR.md | «Probar Edita 0.9.0 beta» / «Testing Edita 0.9.0 beta», «esta versión es una beta» | «Probar Edita» / «Testing Edita» (el ancla en inglés pasa a `#testing-edita`); para qué sirven las versiones de prueba (beta o rc) |
| PROBAR.md | El aviso de `libOpenGL.so.0` de las betas 1-5 | Fuera: está corregido desde la beta 6. Queda cómo reportar una biblioteca que falte |
| Plantilla de issues | «Qué probar en la beta» | «Qué probar» |

La aplicación no tenía avisos de beta: solo muestra la versión (bienvenida, Acerca de), que dice `-beta.N` o `-rc.N` mientras lo sea. El aviso de actualizaciones ya distingue los dos canales ([fase 79](fase79.md)).

Las notas de las versiones publicadas no se tocan: describen lo que fue cada una. Las de las próximas enlazan a `PROBAR.md#testing-edita`.

### Las capturas

- `edita_readme_images` y `edita_manual_images` regeneradas con la aplicación actual: el menú Herramientas y los iconos y el estilo de hoy.
- Nueva `data/manual/<idioma>/images/finddialog.png`: el diálogo Buscar en la pestaña Reemplazar, con las dos casillas de N++ 8.9.9 (resaltar lo que coincide, marcada, y reemplazar sin avanzar). Está en el capítulo «Buscar y reemplazar», en los dos idiomas. `manual_screenshots` desactiva un momento la búsqueda en línea, porque con ella Ctrl+H abre la barra. El campo de reemplazo del diálogo tiene ahora nombre de objeto (`replaceCombo`).

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| README, PROBAR.md y la plantilla de issues no hablan de «la beta» | ✅ `git grep -i beta` solo da la explicación de las versiones de prueba |
| El manual enlaza la captura nueva en los dos idiomas | ✅ `tst_manual` |
| Las capturas se generan sin errores | ✅ |
| Pasan las 66 suites | ✅ |
