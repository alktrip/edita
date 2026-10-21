# Fase 1 — Núcleo de documentos: resultado

Fecha: 2026-10-19 · Entorno: el mismo que en la Fase 0 (Arch/CachyOS, GCC 16.2.1, Qt 6.12.0)

## Qué se construyó

| Módulo | Equivale en N++ a | Qué hace |
|---|---|---|
| `core/Encoding` | `EncodingMapper.cpp`, `UniMode`, `Notepad_plus::setUniModeText` | Tabla de los 46 codepages del menú de N++ (alias, nombre iconv, nombre visible), `TextEncoding` = par `_encoding`/`_unicodeMode` |
| `core/EncodingDetection` | `Utf8_16_Read::determineEncoding`, `FileManager::detectCodepage`, `resolveLoadedEncoding` | BOM → uchardet → UTF-16 LE sin BOM / UTF-8 / ASCII → codepage de respaldo |
| `core/Codec` | `Utf8_16_Read/Write`, `WcharMbcsConvertor::encode` | Conversión por bloques a UTF-8 y de vuelta, con caracteres partidos entre bloques |
| `core/Eol` | `getEOLFormatForm`, `SCI_CONVERTEOLS` | Detección (gana el primer fin de línea) y conversión |
| `core/TextFile` | `FileManager::loadFileData` / `saveBuffer` (la parte de E/S) | Lectura en bloques de 128 KiB + 4 (como N++) y guardado atómico |
| `core/Paths` | `NppParameters::_userPath` / `_nppPath` | Rutas XDG (`~/.config/edita`, `/usr/share/edita`) |
| `doc/Buffer`, `doc/DocumentManager` | `Buffer`, `FileManager` | Documentos de Scintilla independientes de las vistas, creados y rellenados con una vista auxiliar invisible; notificaciones con la misma máscara `BufferChange*` |
| `third_party/uchardet` | `PowerEditor/src/uchardet` | La copia de N++, para detectar exactamente igual |

## Decisiones y divergencias respecto a N++

Comportamiento de referencia: el modo de N++ **"UTF-8 como codepage del sistema"**, que es lo que equivale a Linux. Scintilla siempre trabaja en UTF-8 y los archivos ANSI se convierten.

| Tema | Notepad++ | Edita | Motivo |
|---|---|---|---|
| Detección de encoding | uchardet (copia antigua de Mozilla) + `EncodingMapper` | **Igual**: misma copia, mismos alias, TIS-620 descartado | Fidelidad. Incluye sus rarezas (ver abajo) |
| Codepage de respaldo para 8 bits | ANSI del sistema (`GetACP`, p. ej. 1252) | Configurable, **Windows-1252** por defecto | Linux no tiene codepage ANSI |
| EOL por defecto (archivo sin saltos de línea / nuevo) | El del sistema: CRLF | **LF** (configurable) | Convención de Linux |
| Codepages de un byte | `MultiByteToWideChar`, sin componer, conserva bytes no definidos | **Tabla de 256 entradas**, construida con iconv byte a byte | glibc compone en CP1255/CP1258 (letra + signo → precompuesto) y rechaza bytes no definidos: con iconv directo, abrir y guardar texto vietnamita/hebreo lo modificaría. Con la tabla, **todos los bytes de los 40 codepages de un byte se conservan** (test) |
| Bytes no definidos en codepages de un byte | 0x80-0x9F → C1; resto → uso privado (tabla de Windows) | 0x80-0x9F → C1 (igual); resto → **U+F700 + byte** | La tabla de uso privado de Windows no está documentada; el resultado práctico es el mismo: el archivo no cambia |
| Caracteres que no existen en el codepage destino al guardar | "Best fit" de Windows (ā → a) o `?` | **`?`**, y se cuentan en `SaveResult::unmappableChars` | Para poder avisar al usuario en la Fase 2 en vez de perder datos en silencio |
| Multibyte (932, 936, 949, 950, EUC-KR) | Codepages de Windows | iconv con las variantes de Microsoft (CP932, CP936, CP949, CP950) | Mismo repertorio que Windows |
| OEM 720 (árabe DOS) | Disponible | **No disponible**: glibc no lo trae. Abrir o guardar en él devuelve `UnsupportedEncoding` | Se puede añadir una tabla propia si hace falta |
| Guardado | Sobrescribe el archivo en el sitio | **Atómico** (`QSaveFile`): temporal + renombrado; conserva permisos y escribe a través de enlaces simbólicos; si el directorio no admite escritura pero el archivo sí, escribe directamente | Un fallo (disco lleno, corte de luz) nunca deja el archivo a medias. Contrapartida: rompe enlaces duros y no conserva el propietario si lo guarda otro usuario |
| CR al final de un bloque y LF al principio del siguiente | Lo toma como CR (Mac) | CRLF | Error de N++ en el límite de bloque (raro) |
| UTF-8 cortado al final del primer bloque | Puede tomar el archivo por ANSI | Lo tolera si el bloque no es el archivo entero | Mismo motivo |
| `IsTextUnicode` (UTF-16 LE sin BOM) | API estadística de Win32 | Aproximación: ≥ 60 % de bytes altos a cero y sin caracteres NUL | No existe en Linux; solo se llega aquí si uchardet no decide |
| Identificador de documento | `Buffer*` | Número que no se reutiliza | Un id viejo no puede apuntar a otro documento |
| Nombre de los documentos sin título | "new 1" | "nuevo 1" (configurable) | Interfaz en español |

**Rarezas de N++ que se conservan** (cada una tiene su test):
- uchardet devuelve `KOI8-R`, pero la tabla de N++ tiene `koi8_r`, así que un archivo KOI8-R acaba en el codepage de respaldo. Se puede abrir bien forzando la codificación.
- Un texto ruso corto lleno de letras raras (un pangrama) se detecta como `x-mac-cyrillic`. Un texto normal se detecta como `windows-1251`.
- ISO-8859-15 se detecta como Windows-1252, así que `€` (0xA4) se muestra como `¤`. El archivo no cambia al guardar, y forzando ISO 8859-15 se lee bien.
- `EUC-JP` no está en la tabla de N++.

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Fixtures UTF-8, UTF-8 BOM, UTF-16 LE/BE BOM, Windows-1252, ISO-8859-15, Shift_JIS, GB2312/GB18030, ASCII y vacío: la detección coincide con la tabla y cargar + guardar da un archivo **idéntico byte a byte** | ✅ `tst_textfile::detectAndRoundTrip`: 20 fixtures, además de Windows-1251, KOI8-R, Big5, EUC-KR, ISO-8859-7, UTF-16 LE sin BOM, UTF-8 inválido, bytes altos y NUL. Se repite a través de documentos de Scintilla en `tst_documentmanager::everyFixtureRoundTripsThroughScintilla` |
| CRLF, LF, CR y mixto: el EOL detectado sigue la regla del primero que aparece, y se conserva al guardar | ✅ `tst_textfile::eolDetection` (con bloques de 1 y 4 bytes) |
| Convertir EOL y encoding da la salida esperada | ✅ `convertEncodingAndEolOnSave`, `convertEolChangesTextAndSavedBytes`, `convertEncodingOnSave` |
| 100 MB cargan en menos de 2 s (release) | ✅ **296 ms** en UTF-8 (guardado: 63 ms); Windows-1252: 624 ms (guardado: 297 ms) — `tst_perf` |
| Ruta inexistente o sin permisos → error, sin excepción ni cierre | ✅ `missingFile`, `directoryIsNotAFile`, `unreadableFile`, `saveIntoMissingDirectoryFails`, `unsupportedCodepage`, `loadErrorsDoNotCreateBuffers` |

Comprobaciones adicionales:
- Caracteres partidos entre bloques en todos los codecs, con bloques de 1, 2, 3, 5, 7 y 4096 bytes.
- Ida y vuelta de los 256 bytes en los 40 codepages de un byte.
- Composición de CP1255/CP1258.
- Permisos conservados y escritura a través de enlaces simbólicos.
- Sin historial de deshacer tras cargar, y conversión de EOL que se puede deshacer.
- Numeración "nuevo N" (el número libre más bajo).
- Recarga con otra codificación sobre el mismo documento de Scintilla.
- Archivos grandes sin estilos (`SC_DOCUMENTOPTION_STYLES_NONE`).
- Un documento sigue vivo mientras una vista lo muestre.

Tests: `tst_encoding` 59, `tst_textfile` 44 (también con `LC_ALL=C`), `tst_paths` 4, `tst_documentmanager` 42, `tst_perf` 4, más los de la Fase 0: `tst_engine` 20 y `tst_mainwindow` 5. Todos en verde, y el build desde cero no da ningún warning.

## Fixtures

`tests/fixtures/generate.py` genera los archivos con los codecs de Python (independientes de glibc). Los archivos generados se versionan, y `encoding/reference/*.txt` guarda el texto esperado en UTF-8.

## Pendiente para fases posteriores

- Vigilancia de cambios externos (`DocStatus::Modified`/`Deleted` existen pero nadie los activa): fuera del MVP.
- Detección del lenguaje por contenido (`detectLanguageFromTextBeginning`): Fase 3.
- Aviso al usuario de caracteres no representables al guardar (`unmappableChars`): Fase 2.
