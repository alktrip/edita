# Fase 107 — La página del proyecto

Fecha: 2026-11-28

## Por qué

Para dar a conocer Edita hace falta un sitio al que enlazar desde los anuncios, las listas de aplicaciones y los artículos: que en un minuto diga qué es, cómo se ve y cómo instalarla. El README de GitHub está pensado para quien ya llegó al código; la página está pensada para quien todavía no conoce Edita.

## Qué se hizo

- **`site/`**: la página en inglés (`index.html`) y en español (`es/index.html`), en HTML y CSS sin dependencias:
  - fuentes del sistema;
  - modo claro y oscuro según el escritorio;
  - pensada para el móvil, con un margen lateral de 16 px.

  Contenido:
  - qué es Edita y para quién;
  - lo de Notepad++ y lo moderno;
  - capturas;
  - los plugins y el catálogo firmado;
  - la descarga por distribución con sus requisitos;
  - preguntas frecuentes. La primera aclara que Edita es un proyecto independiente, sin relación con Notepad++.
- **`packaging/site/assemble.sh`**: arma la página con las capturas del repositorio (las de `docs/images/en` en inglés y las de `docs/images` en español) y el icono, sin copiarlas dos veces en git.
- **`.github/workflows/pages.yml`**: publica la página en GitHub Pages cuando cambia `site/`, cuando cambian las capturas o a mano.

## Pruebas

La página armada se revisó con Chrome sin ventana a 1280 y a 390 píxeles de ancho, en los dos idiomas. Se corrigió el margen lateral de la portada en el móvil, que el `padding` de `.hero` anulaba.
