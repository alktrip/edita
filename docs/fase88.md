# Fase 88 — Detector de secretos

Fecha: 2026-11-13

## Por qué

Subir por error una clave de API o una contraseña a un repositorio es uno de los fallos de seguridad más comunes y más caros: los bots buscan claves en GitHub a los pocos minutos de cada push. Ningún plugin de Notepad++ lo cubre; se buscaron en la lista oficial de 228 plugins las palabras «secret», «credential», «leak» y similares. Edita tiene un panel Git propio (fase 85), así que puede avisar justo antes del commit.

## Qué se hizo

**Un plugin propio, incluido con Edita** (`plugins/SecretScanner`), como MarkdownViewer++. Se puede desactivar desde el administrador de plugins.

- **El motor** (`SecretRules.{h,cpp}`) no depende de la interfaz y las pruebas lo compilan aparte.
  - **24 reglas**:
    - **Por formato:** tokens de GitHub, GitLab, Slack, npm y PyPI; claves de AWS (ID y secreta), Google, OpenAI, Anthropic, Stripe, SendGrid, DigitalOcean, Telegram y Azure; webhooks de Slack y Discord; claves privadas y JWT.
    - **Generales:** contraseña en una URL, `password|secret|token|api_key… = "…"` y, solo en archivos `.env`, `…SECRET…=valor`.
  - **Las generales piden** que el valor parezca aleatorio (entropía de Shannon de 3 bits por carácter o más) y que no sea un ejemplo: `changeme`, `${VAR}`, `<…>`, un solo carácter repetido, `os.environ`…
  - **Si dos reglas encuentran lo mismo, gana la más concreta:** `sk-ant-…` es de Anthropic, no de OpenAI.
  - **Se salta** una línea con `edita:allow-secret`, `gitleaks:allow` o `pragma: allowlist secret`, y las líneas de más de 8 KB (archivos minificados).
  - **Devuelve** posiciones en bytes UTF-8 para Scintilla, línea, columna, el valor y una huella (SHA-256 de la regla y el valor) para ignorarlo.
- **El plugin:**
  - Marca los secretos con un indicador propio (`NPPM_ALLOCATEINDICATOR`, subrayado rojo). Analiza al cambiar de documento y, con una pausa de 400 ms, mientras se escribe.
  - Su **panel** (abajo) muestra dónde, el tipo y el valor enmascarado (`ghp_…tB`). Doble clic selecciona el secreto; **Ignorar** guarda la huella.
  - **Analizar el proyecto** recorre `git ls-files --cached --others --exclude-standard`, o la carpeta fuera de Git, sin binarios ni archivos de más de 2 MB.
  - Los valores ignorados y el interruptor del análisis mientras se escribe van en `SecretScanner.ini`.
  - El menú, el panel y los avisos están en inglés y en español, con su `.ts` dentro del `.so`.
- **Antes de un commit:**
  - La API de plugins suma una función opcional, `edita_plugin_before_commit(EditaCommitCheck*)`, documentada en `edita_plugin.h`. Su estructura queda congelada en `tst_pluginapi`.
  - Antes de `git commit`, el panel Git la llama en cada plugin que la exporta. Si alguno devuelve distinto de 0, muestra su texto y pregunta si hacer el commit de todos modos.
  - Es una función y no un mensaje de `messageProc` porque los plugins de N++ devuelven `TRUE` a cualquier mensaje.
  - El detector revisa las líneas añadidas de `git diff --cached -U0`.
- **Documentación:** el manual (Plugins › Detector de secretos, en los dos idiomas), `docs/plugins.md` y los README.

## Pruebas

`tst_secretscanner` (38):

- **Las reglas, una por una:** 17 casos positivos, incluido `.env`.
- **11 casos que no deben dar aviso:** ejemplos, variables, poca entropía, código sin literal, marcadores…
- **Posiciones en UTF-8**, enmascarado, entropía y la preferencia por la regla más concreta.
- **El plugin en Edita:**
  - la marca en el texto y la lista del panel, con el doble clic;
  - la marca mientras se escribe;
  - ignorar, y que siga ignorado en otra ventana;
  - analizar un proyecto con `.gitignore`;
  - el aviso al hacer commit: «no» deja el repositorio igual, «sí» hace el commit, y sin secretos ni pregunta.

Los secretos de las pruebas son falsos y se arman por partes en tiempo de ejecución. Si el texto estuviera entero en el código, la protección de secretos de GitHub podría rechazar el push.
