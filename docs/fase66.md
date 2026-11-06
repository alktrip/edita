# Fase 66 — Reporte de fallos

Fecha: 2026-11-06

## Por qué

Con las betas, un fallo que cierra Edita de golpe es lo más importante de reportar y lo más difícil: hasta ahora había que abrirlo desde una terminal y copiar lo que escribía. Casi nadie lo hace, y sin la traza no se sabe dónde falló.

## Qué hace

1. **Al arrancar** (`main.cpp`), Edita instala manejadores para SIGSEGV, SIGABRT, SIGFPE, SIGILL y SIGBUS (`core/CrashReport`), con una pila aparte para poder escribir aunque el fallo sea un desbordamiento de pila.
2. **Si falla**, el manejador escribe `~/.config/edita/crash/last-crash.txt` con la versión, la señal, la hora y la traza (`backtrace_symbols_fd`). Solo usa llamadas seguras en una señal: `open`, `write`, `backtrace`; la ruta y la cabecera se preparan al instalarlo. Después deja que el proceso termine como lo haría sin él (`SA_RESETHAND` y la misma señal otra vez), con el mismo código de salida y, si el sistema lo guarda, su volcado de memoria.
3. **Al volver a abrirlo**, la barra de avisos dice que Edita se cerró por un error y ofrece:
   - **Reportar…**: el formulario de GitHub (`bug.yml`) con la versión, el sistema, la señal en «Qué pasó» y la traza en «Más información». La traza va recortada a 4000 caracteres, para que la dirección quepa en una URL. El usuario la revisa y añade qué estaba haciendo antes de enviarla;
   - **Copiar detalles**, al portapapeles;
   - **Descartar**.

   Cualquiera de las tres borra el reporte. Si se cierra el aviso sin elegir, vuelve a salir la próxima vez.

Si Edita termina por otra causa (`kill -9`, falta de memoria, un apagón), no hay reporte y no se pregunta nada.

**La traza.** Cada marco va con su módulo y su desplazamiento (`edita(+0x49ac77)`), y los nombres de C++ de las bibliotecas, legibles (`abi::__cxa_demangle`). El AppImage se compila con `RelWithDebInfo` y no se le quitan los símbolos (`NO_STRIP=1`), así que, con el AppImage de esa versión, `addr2line -e usr/bin/edita -f -C <desplazamiento>` da la función, el archivo y la línea. No se exportan los símbolos del ejecutable (`-rdynamic`) para tener nombres en la traza: taparían los de los plugins, como el `CreateLexer` de un plugin de lexer.

**`formUrl`** acepta más campos del formulario y codifica cada valor entero: `QUrlQuery` dejaba el `+` sin codificar, y GitHub lo lee como un espacio (las trazas están llenas de `+0x…`).

## Criterios de aceptación

| Criterio | Resultado |
|---|---|
| Un proceso que falla de verdad (puntero nulo, `abort`, desbordamiento de pila) termina como sin manejador y deja la versión, la señal y la traza | ✅ `tst_crashreport`: `writesTheReportWhenItCrashes` (con `crash_helper`) |
| Al volver a abrir Edita: el aviso, y Reportar (formulario completo, URL de menos de 8 KB), Copiar y Descartar, que borran el reporte | ✅ `offersTheReportOnTheNextStart` |
| Los nombres de C++, legibles; el `+` llega a GitHub como `+` | ✅ `readableNames`, `formUrlEncodesEverything` |
| Con el Edita real, `kill -SEGV` deja el reporte, el proceso sale con 139 y `addr2line` traduce la traza a archivo y línea | ✅ (a mano) |
| Pasan todas las suites, el build no da avisos y clang no tiene nada que decir | ✅ |
