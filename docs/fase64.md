# Fase 64 — Avisar de una versión nueva

Fecha: 2026-11-05

## Por qué

Con las betas publicadas, quien prueba Edita tiene que ir a GitHub para saber si hay una versión más nueva. Así el feedback llega de versiones viejas, con errores que ya están corregidos.

## Cómo lo hace N++

N++ trae un actualizador aparte (WinGUP, `updater\gup.exe`) y lo lanza al arrancar (`launchUpdater`, en `winmain.cpp`). Lo hace si toca: cada `intervalDays` días (15). Al lanzarlo fija la próxima fecha (`nextUpdateDate` = hoy + 15), salga bien o no. Lo guarda en `config.xml` así: `<GUIConfig name="noUpdate" autoUpdateMode="1" intervalDays="15" nextUpdateDate="AAAAMMDD">`, donde `autoUpdateMode` vale 0 (desactivado), 1 (al arrancar) o 2 (al salir). La preferencia está en Preferencias › Varios («Auto-updater»), y se oculta si no hay actualizador. La orden es **? › Update Notepad++** (`IDM_UPDATE_NPP`). WinGUP pregunta al sitio de N++, descarga el instalador y lo ejecuta.

## Qué hace Edita

Edita no se instala solo: un AppImage no tiene instalador, y un paquete de una distribución se actualiza con su gestor de paquetes. Pregunta a GitHub por la última versión publicada (`GET https://api.github.com/repos/alktrip/edita/releases/latest`, sin más datos que `User-Agent: Edita/<versión>`). Si es más nueva, lo dice en la barra de avisos con dos botones: **Página de descarga** y **Omitir esta versión**.

| Dónde | Qué |
|---|---|
| **`core/UpdateCheck`** | Comparar versiones (SemVer con etapas: `0.9.0-beta.2` < `0.9.0-beta.10` < `0.9.0` < `0.9.1`; la «v» de la etiqueta se ignora). Leer la respuesta de GitHub (`tag_name`, `html_url` y el AppImage de `assets`). El estado en `noUpdate`: `nextUpdateDate` y `skippedVersion`. Cuándo toca y la próxima fecha, como `launchUpdater`. La dirección sale de `HOMEPAGE_URL`, o de `EDITA_UPDATE_URL`: una ruta local para las pruebas, o `none` para desactivarlo |
| **Preferencias › Varios** | «Buscar una versión nueva de Edita al arrancar» y «Cada: N días» (`autoUpdateMode` e `intervalDays` del mismo `<GUIConfig>`). Por defecto, 15 días como N++, y 1 en una beta: quien prueba necesita la última. `autoUpdateMode="2"` (al salir, de N++) cuenta como al arrancar |
| **Ayuda › Buscar actualizaciones…** | Comprueba aunque no toque y aunque se haya omitido esa versión. Dice también si no hay nada nuevo, o por qué no pudo comprobarlo. Al arrancar, en cambio, sin red no avisa |
| **Al arrancar** | Tres segundos después de abrir la ventana, si toca, sin bloquear nada |
| **`EDITA_UPDATE_CHECK`** | Opción de CMake, activada por defecto. Una distribución que empaquete Edita la desactiva: no hay orden ni búsqueda al arrancar, y la preferencia aparece desactivada, como N++ la oculta sin actualizador |
| **Pruebas** | `EDITA_UPDATE_URL=none` en todas las suites, como ya se hacía con el catálogo de plugins: ninguna habla con GitHub |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Las versiones con etapa se comparan como SemVer | ✅ `tst_updatecheck`: `comparesVersions` |
| La respuesta de GitHub se entiende, y una que no lo es se rechaza | ✅ `parsesTheLatestRelease` (y comprobado contra la API real: `v0.9.0-beta.2`) |
| Comprueba al arrancar solo si toca, y entonces fija la próxima fecha en `config.xml` | ✅ `startupCheckWhenDue`, `scheduleAndSettings` |
| Una versión nueva se avisa, la página se abre y la omitida no se vuelve a avisar al arrancar (una más nueva, sí) | ✅ `manualCheckFindsANewVersion` |
| Sin novedades o sin red: a mano lo dice, al arrancar calla | ✅ `upToDateAndErrors` |
| El manual cita la orden en los dos idiomas | ✅ `tst_manual` |
| Pasan todas las suites, el build no da avisos y clang no tiene nada que decir | ✅ |
