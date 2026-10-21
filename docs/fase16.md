# Fase 16 — Internacionalización: resultado

Fecha: 2026-10-20 · Entorno: el mismo que en la fase 15, más `qt6-tools` (lupdate y lrelease) para compilar

Objetivo: que toda la interfaz salga en un solo idioma. Antes, los textos de Edita estaban fijos en español y algunos diálogos (los de Qt y los del escritorio) salían en inglés. Como en Notepad++, el código queda en **inglés** y cada idioma es una traducción. La primera, **el español, está completa**.

## Qué se construyó

| Parte | Qué hace |
|---|---|
| Textos del código | Los 906 textos de la interfaz están en inglés y pasan por el sistema de traducción de Qt. Se usó la terminología del `english.xml` de N++ («Find in Files», «Line Operations», «Plugins Admin»…). Hay `tr()` en las clases y `QCoreApplication::translate("Contexto", …)` en el código sin `QObject` (`core/`, `syntax/`). Se tradujeron también los textos que no pasaban por `tr()`: mensajes del instalador y del catálogo de plugins, la línea de comandos, «nuevo 1», el «1 de 3» de la ayuda de parámetros, y los nombres largos de lenguaje de la barra de estado y del diálogo Abrir («Archivo de texto normal», «Código fuente en C++»…) |
| Plurales | Los mensajes que armaban el plural a mano («1 coincidencia» / «3 coincidencias», un verbo suelto en el administrador de plugins) ahora usan `%n`/`%Ln` de Qt, para que cada idioma ponga sus formas. `edita_en.ts` solo tiene las formas del inglés |
| `translations/edita_es.ts` | La traducción al español: 907 textos (incluido el nombre del idioma, «Español»), todos terminados, con los mismos `%1`/`%n` y los mismos atajos (`&`) que el original. Se creó a partir de los textos en español que había, así que en español la interfaz dice lo mismo que antes |
| CMake | `qt_add_lupdate`/`qt_add_lrelease`: las `.qm` se compilan con el programa y van dentro del binario (`:/edita/i18n`). `cmake --build build --target edita_lupdate` actualiza los `.ts` con los textos del código. Requiere Qt 6.7 o posterior |
| `ui/Translations` | Lista los idiomas que hay en el binario (el nombre de cada uno lo pone su traducción). Elige el idioma: el de la preferencia o, si está vacía, el primero de los del sistema que tenga traducción (`es-MX` → español); si ninguno la tiene, inglés. Instala la traducción de Edita y la de Qt (`qtbase_es.qm`, para los botones y diálogos estándar) y fija el idioma en `QLocale`, de donde lo leen los plugins. `main` lo hace antes de crear cualquier texto |
| Preferencias > General | **Idioma**: «Idioma del sistema», English o Español (`<GUIConfig name="EditaLanguage" value="es"/>`). Al cambiarlo, Edita ofrece reiniciarse, como N++ con su `nativeLang.xml`. **Usar los diálogos de archivos de Edita**: los diálogos del escritorio (KDE, GNOME) salen en el idioma del escritorio, no en el de Edita. Con esta opción se usan los de Qt, que siguen el idioma de Edita (`Qt::AA_DontUseNativeDialogs`, que también aplica a los plugins) |
| Plugins | `NPPM_GETNATIVELANGFILENAME`, como en N++: devuelve `spanish.xml` en español y 0 en inglés. El plugin de ejemplo lo usa para poner sus menús y mensajes en el idioma de Edita. **Python Script** traduce sus diálogos con su propia `pythonscript_es.ts` (dentro del `.so`), según `QLocale`. Sus menús quedan en inglés, como en N++, porque los scripts llaman a sus órdenes por el nombre (`notepad.runPluginCommand('Python Script', 'Show Console')`). En el port de XML Tools se pasaron a inglés unos textos que habían quedado en español; el original no tiene traducciones |

## Divergencias

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Formato de las traducciones | `nativeLang.xml` por idioma, con los textos por identificador de menú y de diálogo | `.ts`/`.qm` de Qt por idioma, con los textos por contexto y texto original; van dentro del binario | Es el sistema de Qt: plurales, herramientas (Qt Linguist) y traducciones de Qt para los diálogos estándar |
| Elegir el idioma | Preferencias > General > Localización; se aplica en el acto | Preferencias > General > Idioma; se aplica al reiniciar (lo ofrece) | Los menús y diálogos se construyen una vez; rehacerlos en caliente no compensa |
| Idioma por defecto | Inglés, salvo que el instalador elija otro | El del sistema si hay traducción; si no, inglés | En Linux el idioma del sistema está en el entorno (`LANG`, `LANGUAGE`) |
| Diálogos de archivos | Los de Windows, en el idioma del sistema | Los del escritorio por defecto, o los de Qt (opción) en el idioma de Edita | El diálogo del escritorio lo dibuja el escritorio; ninguna aplicación puede cambiar su idioma |
| Menús de los plugins | En el idioma de cada plugin | Igual. MIME Tools, XML Tools y los menús de Python Script quedan en inglés | Los textos son de cada autor. Además, los scripts usan los nombres de los menús |

## Para traducir Edita a otro idioma

1. Agregar `translations/edita_<código>.ts` a `EDITA_TS_FILES` en `src/CMakeLists.txt`.
2. Ejecutar `cmake --build build --target edita_lupdate`, que crea el archivo con todos los textos.
3. Traducirlo con Qt Linguist (`linguist translations/edita_<código>.ts`). El texto «English» del contexto `i18n` es el nombre del idioma en ese idioma.
4. Volver a compilar. El idioma aparece solo en Preferencias, y se elige solo si es el del sistema.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Las traducciones están completas y bien formadas: ningún texto sin terminar ni vacío, los mismos `%1`/`%n` y el mismo número de atajos (`&`) que el inglés. Vale para Edita, los plurales del inglés y Python Script | ✅ `tst_i18n`: `translationFilesAreComplete` |
| Idiomas disponibles (English y Español, con su nombre). El idioma se elige por la preferencia, por el sistema (`es-MX`, `es_ES`) o cae a inglés. Al instalarlo se cambian las traducciones y `QLocale`. Los plurales del inglés («1 hit», «3 hits») funcionan | ✅ `languageSelection` |
| La preferencia se guarda en `config.xml` y está en el diálogo. Cambiar el idioma ofrece reiniciar y no cambiarlo no pregunta nada. La opción de los diálogos de Qt activa y desactiva `AA_DontUseNativeDialogs` | ✅ `preferencePersistsAndAsksToRestart` |
| En inglés no queda nada en español: menús, Preferencias, Buscar y lenguajes de usuario (más de 300 textos) | ✅ `englishInterfaceHasNoSpanish` |
| En español está todo traducido: se comparan los menús y los diálogos de Preferencias, Buscar, Lenguajes de usuario y Administrador de plugins en los dos idiomas (más de 500 textos), y no queda ningún texto igual que tendría que cambiar. Además: título «nuevo 1 - Edita», «Lín:», «Archivo de texto normal», los plurales del español y los botones estándar de Qt («Cancelar») | ✅ `spanishInterfaceIsComplete` |
| Los plugins saben el idioma (`NPPM_GETNATIVELANGFILENAME`): el plugin de ejemplo sale en español o en inglés | ✅ `pluginsGetTheLanguage` |

Además:

- Las 23 suites anteriores se pasaron a inglés, que es el idioma base. Los textos que comparan ahora son los ingleses («new 1», «Find: Can't find the text…»). `notepad.runMenuCommand` usa el texto del menú en el idioma activo, como en N++.
- En el camino se corrigió un fallo que ya existía: los nombres de los juegos de caracteres y de los estilos de los lenguajes de usuario estaban marcados con un contexto distinto del que se usaba al traducir, así que nunca se habrían traducido.

Tests: `tst_i18n` tiene 10 casos (contando `initTestCase` y `cleanupTestCase`). Pasan las 24 suites y el build no da ningún warning.
