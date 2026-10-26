# Fase 38 — Versión para macOS: resultado

Fecha: 2026-10-26

Edita se compila también para macOS, como un `.dmg` universal (Apple Silicon e Intel) que genera GitHub Actions. No hace falta tener un Mac para producirlo.

## Qué se construyó

| Parte | Qué hace |
|---|---|
| **GitHub Actions** (`.github/workflows/macos.yml`) | Se ejecuta al subir a `main`, en cada pull request, con etiquetas `v*` o a mano («Run workflow»). En macOS 14 instala Qt 6.8.3 (la misma versión que el AppImage; para macOS es universal) y Ninja, compila, pasa las pruebas y genera el `.dmg`, que queda como artefacto «Edita-macos». Con una etiqueta `v*` se publica además en Releases. El registro de las pruebas se sube aparte («tests-macos») |
| **Empaquetado** (`packaging/macos/build-dmg.sh`) | Compila para `arm64;x86_64` con macOS 12 como mínimo y arma `Edita.app`: los datos en `Contents/Resources/edita`, los plugins en `Contents/PlugIns/edita/plugins`, el SDK de plugins y el icono `.icns` (hecho con `sips` e `iconutil` desde el PNG). `macdeployqt` mete Qt, también para los plugins. Firma ad hoc (con `EDITA_SIGN` se puede usar una identidad real, y entonces con «hardened runtime» para notarizar) y crea el `.dmg` con un acceso a Aplicaciones. Si alguna prueba falla, avisa y empaqueta igual |
| **Info.plist** (`packaging/macos/Info.plist.in`) | Nombre, versión, identificador `io.github.edita`, alta resolución y modo oscuro. Declara los tipos de documento (texto, código fuente, scripts, XML, JSON, YAML, Markdown, CSV, registros y, con rango «None», cualquier archivo) para que Finder ofrezca «Abrir con Edita» |
| CMake | En macOS el ejecutable es el paquete `Edita.app` (`MACOSX_BUNDLE`). `libutil` solo se enlaza en Linux. Nueva opción `EDITA_PYTHONSCRIPT` (desactivada en macOS: incrustar el Python del sistema en el paquete queda para después) |
| **Terminal** | `forkpty` sale de `<util.h>`; sin `execvpe`, se fija `environ` y se usa `execvp`; la carpeta de la shell se lee con `proc_pidinfo`, porque no hay `/proc`. En macOS, Qt llama «Control» a ⌘: para la shell cuenta la tecla Control (Control+C interrumpe), ⌘C y ⌘V copian y pegan, y los demás atajos con ⌘ siguen siendo de la ventana |
| Rutas | Los datos también se buscan en `../Resources/edita` y los plugins en `../PlugIns/edita/plugins`, junto al ejecutable del paquete. `$XDG_CONFIG_HOME`, si está definido, manda en cualquier sistema (en macOS Qt usaría `~/Library/Preferences`, y las pruebas lo necesitan para no tocar la configuración real) |
| **Finder y el Dock** | Abren los archivos con `QFileOpenEvent`, no por la línea de órdenes. Los que llegan antes de crear la ventana se guardan y se abren después, y los siguientes van a la ventana abierta |
| Clang | Se compiló todo con Clang 23 y `-Werror`, como hará el de Apple. Salieron dos avisos que GCC no da: una variable que se escribía y no se leía (`DesktopFonts`) y una captura de `this` sin usar. Las 40 suites pasan con Clang |

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Compila con Clang sin warnings y pasan las pruebas | ✅ (en Linux, Clang 23) |
| Con GCC no cambia nada: 41 suites | ✅ |
| El flujo de trabajo, el `Info.plist` y el script son válidos (YAML, XML, `bash -n`) | ✅ |
| `.dmg` generado en GitHub Actions | ⏳ cuando el repositorio esté en GitHub (no se puede comprobar aquí) |

Limitaciones: la app no está notarizada, así que la primera vez hay que abrirla con clic derecho › Abrir (o en Ajustes › Privacidad y seguridad › «Abrir igualmente»). Por ahora no incluye Python Script. El instalador de paquetes `.zip` del administrador de plugins necesita libzip, que el runner no tiene.
