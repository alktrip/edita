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
| `.dmg` generado en GitHub Actions | ✅ `Edita-0.0.1-macos-universal.dmg` (37 MB), en https://github.com/alktrip/edita/actions |
| Pruebas en macOS 14 (Qt 6.8.3, Apple Clang 15) | ✅ 35 de 37 suites en la sexta ejecución; las dos que faltaban se corrigieron después (ver abajo) |

## Lo que salió en las primeras ejecuciones en macOS

Se necesitaron seis ejecuciones. Cada una avanzó un poco más y dejó algo que desde Linux no se veía:

| Problema | Arreglo |
|---|---|
| La libc++ de Apple no tiene `from_chars`/`to_chars` para decimales antes de macOS 13.3 | pugixml sin `PUGIXML_CHARCONV_FLOAT` en macOS (parche 0006); el orden numérico de líneas lee con un flujo en el locale «C» si falta `__cpp_lib_to_chars` |
| Sin libzip (el binario es universal), dos cosas de libzip quedaban fuera de su `#ifdef` | Dentro del `#ifdef`; se comprobó compilando sin libzip |
| En macOS `iconv` es una biblioteca aparte | `find_package(Iconv)` |
| Las pruebas no compilaban con Qt 6.8 (las macros `QTRY_*` convierten un tiempo de 64 bits a `int`) ni con GCC 12 (falso positivo de `-Wrestrict`) | Con Qt < 6.12 las pruebas se compilan sin las advertencias de conversión, y con GCC 12 sin `-Wrestrict`. Ahora también se compilan y pasan con Qt 6.8 y GCC 12 en el chroot de Ubuntu del AppImage |
| `codesign` rechazaba los restos del build en los plugins (`.lupdate`) | Del plugin solo se copian el `.so`, `lib/` y `scripts/` |
| **CP874**: la `iconv` de macOS da caracteres a 0xDB–0xDE y 0xFC–0xFF, que Windows no define, y algunos repiten otros (0xDB «้» = 0xE9): un texto tailandés se habría guardado con otros bytes | Esos bytes se tratan como no definidos, como en Windows. Se compararon byte a byte todas las codificaciones de un byte entre Linux y macOS: solo cambia además MAC-CYRILLIC 0xFF («¤»/«€»), que no pierde nada al guardar |
| La libc++ de macOS no da nombre a la locale del entorno, y se tomaba por un fallo (la regex funcionaba igual) | Se devuelve un nombre |
| Sin pantalla, la fuente fija de Qt en macOS («Monospace») no existe y salía una proporcional (el espacio virtual del editor de columnas salía el doble de ancho) | En macOS, Menlo, Monaco o Courier New si la de Qt no es monoespaciada |
| `XDG_RUNTIME_DIR` no lo usa Qt en macOS | La instancia única lo respeta si está definido (como la configuración con `XDG_CONFIG_HOME`) |
| Pruebas que suponían Linux: QSettings nativos de Qt, `/var` = `/private/var`, rutas largas que se partían, bash 3.2 sigue la lista tras Ctrl+C | Se omite en macOS, se aceptan las dos rutas, terminal más ancha, `sleep 30` sin nada detrás |

Limitaciones: la app no está notarizada, así que la primera vez hay que abrirla con clic derecho › Abrir (o en Ajustes › Privacidad y seguridad › «Abrir igualmente»). Por ahora no incluye Python Script. El instalador de paquetes `.zip` del administrador de plugins necesita libzip, que el runner no tiene.
