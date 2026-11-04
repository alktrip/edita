# Fase 62 — Beta pública

Fecha: 2026-11-04

## Por qué

Edita ya hace casi todo lo que hace Notepad++, y 17 plugins de N++ llegan por el catálogo. Lo que falta no se ve con pruebas automáticas: cómo se usa en equipos, escritorios y archivos reales. Los testers son los usuarios que prueben la beta y reporten lo que encuentren. Para que lo hagan hace falta:

- una versión con número, porque el release `continuous` cambia con cada push;
- una forma fácil de reportar;
- una guía de qué probar.

## Qué cambió

| Dónde | Qué |
|---|---|
| **Versión** | `project(VERSION 0.9.0)` y `EDITA_VERSION_SUFFIX "-beta.1"` (CMake no admite la etapa en `VERSION`). `kAppVersion` (Acerca de, página de inicio, LSP, nombre del AppImage) es la completa, `0.9.0-beta.1`; `kAppVersionNumber`, solo los números, es la que ven los plugins (`NPPM_GETNPPVERSION`) y la de las comparaciones con `npp-compatible-versions`. El catálogo pide `[0.0.1,]`, así que sigue valiendo. `HOMEPAGE_URL` ya es el repositorio de Edita, no el de Notepad++ |
| **Ayuda › Reportar un problema…** | Abre `issues/new?template=bug.yml&version=…&system=…`. GitHub rellena cada campo del formulario con el parámetro de su `id`. El sistema (`core/IssueReport`): distribución, arquitectura, núcleo, escritorio, Wayland o X11, Qt y si es el AppImage. Si no se puede abrir el navegador, un aviso muestra la dirección |
| **Plantillas** (`.github/ISSUE_TEMPLATE`) | `bug.yml` (Edita) y `plugin.yml` (el plugin y su versión, y dónde falla: instalar, cargar, una orden, un diálogo), en español y en inglés; `config.yml` enlaza la guía |
| **`PROBAR.md`** | Para los testers, en los dos idiomas: instalar el AppImage, reportar, qué probar, y qué probar en cada plugin del catálogo |
| **Release** | `linux.yml` publica la etiqueta con su nombre, las notas de `packaging/release-notes/<etiqueta>.md` y como la última versión. Una beta va como versión normal, no como prerelease: la 0.0.1 era la «última», y una prerelease no lo puede ser |
| **Manual** | La orden, en «Obtener ayuda» y en «Informar de un problema», en los dos idiomas |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| La versión es `0.9.0-beta.1` y los plugins ven `0.9.0` | ✅ `tst_welcome`: `versionAndReportAProblem` |
| El formulario se abre con la versión y el sistema, y sus campos existen en la plantilla | ✅ `versionAndReportAProblem` |
| Sin navegador, el usuario ve la dirección | ✅ `versionAndReportAProblem` |
| El manual cita la orden en los dos idiomas | ✅ `tst_manual` |
| La etiqueta `v0.9.0-beta.1` publica el AppImage `Edita-0.9.0-beta.1-x86_64.AppImage` con sus notas | ✅ CI de Linux |
| Pasan todas las suites y el build no da avisos | ✅ |
