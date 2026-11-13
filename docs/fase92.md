# Fase 92 — Plugins compilados para arm64

Fecha: 2026-11-13

## Por qué

Desde la fase 86, Edita se publica para arm64 (Raspberry Pi 4 y 5, portátiles y servidores ARM), pero el catálogo de plugins solo traía binarios de x86_64. En ARM, el administrador de plugins ofrecía compilar cada uno desde su receta. Eso pide `cmake` y un compilador, tarda, y nadie lo había probado en ARM.

Notepad++ no tiene este problema de la misma forma: su lista (`nppPluginList`) trae una entrada por plugin con un `.zip` para x86, otro para x64 y, a veces, otro para ARM64. Cada autor publica los suyos, y en ARM64 hay pocos.

## Qué se hizo

**El catálogo** (edita-plugins):

- **La CI** compila las recetas dos veces, en paralelo: x86_64 en Ubuntu 22.04 y arm64 en Ubuntu 24.04 ARM (el Qt oficial de arm64 pide glibc 2.38, como Edita). Cada una usa el AppImage de Edita de su arquitectura. Un paso aparte junta las dos, decide si algo cambió y firma las dos listas. Las firmas se comprueban con minisign antes de publicar.
- **Una lista por arquitectura, en el mismo release:**
  - `plugins.json` es la de x86_64. Conserva el nombre porque es la que piden las versiones anteriores de Edita.
  - `plugins-linux-aarch64.json` es la de arm64.
  - Los paquetes de arm64 llevan `-linux-aarch64` en el nombre; las recetas (`recipes.zip`) son las mismas.
- **`catalog.py`:** `list --arch`, `rename-packages` (añade la arquitectura al nombre de los paquetes y a su `.sha256`) y `list-name`, con pruebas.
- **Primer catálogo con las dos** (`catalog-2026.11.13.22`): las 23 recetas compilaron y pasaron su prueba en arm64. Son 21 binarios en cada arquitectura; NppExport y Simplify JSON se compilan en el equipo por su licencia, como en x86_64.

**Edita:**

- **`remote_catalog::settings(dir, arch)`:** en una arquitectura que no es x86_64, la lista es `plugins-<arquitectura>.json`, junto a la `plugins.json` de siempre. Eso vale también para un catálogo propio (`EDITA_PLUGIN_CATALOG_URL`).
- **Respaldo:** si la lista de la arquitectura no existe (un catálogo anterior a esta fase), el administrador de plugins usa `plugins.json`, y sus plugins se compilan en el equipo, como hasta ahora.

Las versiones de Edita hasta la 1.0.0-rc.3 siguen pidiendo `plugins.json`. En arm64 siguen compilando los plugins: los binarios llegan con la próxima versión.

## Pruebas

- **`test_catalog.py`:** la lista de arm64 (arquitectura, nombres de paquete y URL, las mismas recetas, sin confundir los paquetes de las dos arquitecturas) y los nombres de las listas.
- **`tst_catalog`:** las URL de cada arquitectura, también con un catálogo propio, y la de respaldo.
- **`tst_pluginsadmin` (`catalogForArm64`):**
  - con solo `plugins.json` publicada, Edita en arm64 usa esa;
  - con `plugins-linux-aarch64.json`, usa la suya.
- **De punta a punta:** las dos listas publicadas se descargaron y se comprobó su firma con la clave del catálogo.
