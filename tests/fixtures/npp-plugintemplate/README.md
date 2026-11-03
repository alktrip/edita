# Plantilla de plugins de Notepad++ (para las pruebas)

Archivos sin modificar de [npp-plugins/plugintemplate](https://github.com/npp-plugins/plugintemplate), commit `abd8c2394936e934753db08ab1684aae1dcf7fd4` (2026-10-13). Copyright (C) Don HO y colaboradores; GPL v3 o posterior, como dicen sus cabeceras.

`tst_recipes` compila con ellos dos plugins con un panel acoplable:

- `builderCompilesATemplateDockingPlugin` (fase 52): las clases `Window`, `StaticDialog` y `DockingDlgInterface` de la plantilla, sin cambios, sobre la capa de compatibilidad de Edita;
- `builderCompilesTheTemplateGoToLineDialog` (fase 53): el panel «Go To Line #» de la plantilla (`GoToLineDlg.cpp` y su diálogo de `goLine.rc`), sin cambios, con los recursos convertidos por `edita_rc`.
