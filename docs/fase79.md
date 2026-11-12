# Fase 79 — El aviso de actualizaciones de la versión estable

Fecha: 2026-11-12

## Por qué

La fase 64 añadió el aviso de versiones nuevas. Pedía a GitHub `releases/latest`, que da la última versión publicada. Las betas se publican como versiones normales, no como «prerelease», para que GitHub las muestre como la última. Así, una 1.0 avisaría de la 1.1.0-beta.1 en cuanto saliera, y quien usa la versión estable no quiere eso.

Además, las betas comprueban cada día y las estables cada 15 días (`updateIntervalDays`). Pero el intervalo se guarda en `config.xml`: quien pasara de la beta 11 a la 1.0 se quedaría comprobando cada día.

N++ hace lo mismo que Edita hace ahora: su actualizador (gup) solo ofrece versiones estables, y las versiones candidatas se anuncian aparte.

## Qué se hizo

| Antes | Ahora |
|---|---|
| `GET /repos/alktrip/edita/releases/latest` | `GET /repos/alktrip/edita/releases?per_page=30`, la lista de las 30 más recientes |
| La versión que diera GitHub | `update_check::parseReleases(json, includePrereleases, …)` elige la más nueva. Una versión estable de Edita (`kAppPrerelease` falso) descarta las betas: las que tienen etapa en el número (`1.1.0-beta.1`) y las marcadas `prerelease`. Nunca cuentan los borradores ni las etiquetas que no son versiones (`continuous`). Una beta sigue avisando también de las betas, que es lo que necesitan los testers |
| — | Si no queda ninguna, la versión queda vacía y Edita dice que está al día. Pasa, por ejemplo, con una estable cuando solo hay betas más nuevas |
| `EDITA_UPDATE_URL` con un solo objeto | Sigue sirviendo (un objeto cuenta como lista de uno), con el mismo filtro |
| `intervalDays` de la beta (1) se quedaba en la estable | `<GUIConfig name="noUpdate" editaPrerelease="yes/no">` guarda si lo escribió una beta. Sin el atributo, lo escribió una de las betas 1-11. `migratedUpdateInterval`: al cambiar de beta a estable o al revés, si el intervalo era el de omisión del otro canal (1 o 15), pasa al de este. Si el usuario lo había cambiado (7, por ejemplo), se respeta |

El manual (Introducción, menú Ayuda) dice, en los dos idiomas, que una versión estable solo avisa de versiones estables.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| De una lista con «continuous», un borrador, betas y versiones marcadas prerelease, una estable elige la estable más nueva, y una beta la más nueva de todas | ✅ `tst_updatecheck`: `choosesTheNewestRelease` |
| Solo betas y Edita estable: nada que avisar | ✅ `choosesTheNewestRelease` |
| Un solo objeto se sigue leyendo y se filtra igual; las respuestas que no son versiones dan error | ✅ `choosesTheNewestRelease` |
| La URL es la lista de versiones | ✅ `scheduleAndSettings` |
| El intervalo pasa de 1 a 15 al cambiar de beta a estable (y al revés), salvo que el usuario lo cambiara; `editaPrerelease` se guarda | ✅ `intervalFollowsTheChannel` |
| Pasan las 66 suites | ✅ |
