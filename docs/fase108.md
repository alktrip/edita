# Fase 108 — Marca, documentación y confianza

Fecha: 2026-11-28

## Por qué

Una revisión externa del proyecto antes de la 1.0 señaló problemas que no son de código, pero que importan para que Edita se pueda presentar al público:

- la relación con Notepad++ no estaba aclarada en ningún sitio visible;
- algunas frases prometían más de lo que hay;
- el README en español estaba desactualizado;
- la comparación con otros editores dejaba fuera a los que hacen lo mismo que Edita;
- no había un criterio medible para publicar la 1.0, ni una forma de reportar vulnerabilidades, ni una explicación de cómo se desarrolla Edita con IA.

El autor de Notepad++ dejó clara su postura en 2026, con un aviso sobre un sitio que se presentaba como un Notepad++ oficial para Mac: los ports y forks son bienvenidos, pero no pueden contar con su aval. Edita deriva de Notepad++ bajo la GPL v3 y tiene que decirlo así, sin dar a entender que es Notepad++.

## Qué se hizo

- **No afiliación**, con el mismo texto en el README (en inglés y español), el diálogo Acerca de, los metadatos de AppStream y la página web: «Edita es un proyecto independiente, derivado de Notepad++ bajo la GPL v3 de GNU. No está afiliado a Don Ho ni al proyecto Notepad++, ni cuenta con su aval».
- **Exactitud y peso de la marca:**
  - El eslogan pasa de «The Notepad++ experience» a «A Notepad++-style editor, built for Linux». La portada de la página, de «el flujo de trabajo de Notepad++» a «un editor al estilo de Notepad++».
  - Los plugins: «plugins de Notepad++ portados desde su código fuente», en vez de «ejecuta plugins de Notepad++».
  - El resumen de AppStream: «inspirado en Notepad++».
  - Se quita la palabra clave «Notepad++»; queda «notepad».
  - La descripción del repositorio en GitHub, igual.
- **El alcance**, sin contradicción: «más que un editor de texto, menos que un IDE». Dice qué tiene (LSP, Git, terminal, tareas) y dónde está el límite: sin un proyecto que configurar, sin depurador, sin un sistema de compilación propio y sin más refactorización que la del servidor de lenguaje.
- **La comparación** incluye a **Nextpad++** (port de Notepad++ con GTK 4, también en macOS e iOS, con su catálogo de plugins portados), **Notepad Next** (reimplementación con Qt para los tres sistemas) y **CudaText** (editor independiente y rápido con complementos en Python), cada uno con su punto fuerte. Se revisó con las páginas de cada proyecto. Los datos que no se pudieron confirmar no se afirman.
- **El README en español**, rehecho como traducción del inglés. Usa las capturas de la interfaz en español (`docs/images/`); las de `docs/images/en/` son las de la interfaz en inglés.
- **Las versiones** en el README ya no llevan un número fijo (enlazan a la última) y el número de pruebas dice «más de 80».
- **El proceso**, en el ROADMAP:
  - funciones congeladas hasta la 1.0;
  - un criterio de salida medible: 14 días sin errores graves en la última candidata, al menos dos personas ajenas al proyecto que hayan recorrido `PROBAR.md`, y la CI en verde.

  `PROBAR.md` explica cómo entregar el resultado, con una plantilla de issue nueva (`test-report.yml`) y su etiqueta.
- **Confianza:**
  - Una sección en `CONTRIBUTING.md` sobre cómo se desarrolla Edita con un asistente de IA (Claude, como dicen los `Co-Authored-By` de los commits) y qué comprobaciones no dependen de la atención de nadie: pruebas primero, tres compiladores, sanitizers, paquetes instalados en la CI, un documento por fase y revisiones del código.
  - `SECURITY.md`, en inglés y español: cómo reportar en privado (se activó el reporte privado de vulnerabilidades del repositorio), qué esperar y qué cubre.
- **Flatpak documentado**: dónde vive el manifiesto y cómo llegará a Flathub, en `docs/empaquetado.md`.

## Pruebas

- `tst_about`: la línea de no afiliación en el diálogo Acerca de.
- `tst_metainfo`: el metainfo sigue siendo válido con `appstreamcli validate --strict`.
