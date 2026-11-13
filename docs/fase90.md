# Fase 90 — FTP y SFTP

Fecha: 2026-11-13

## Por qué

NppFTP es uno de los plugins más descargados de Notepad++ y el último de la lista de los más populares que Edita no tenía. Editar archivos en un servidor es algo que se hace todos los días, y en Linux más.

Portarlo no tenía sentido:

- **Tamaño:** son 40 000 líneas.
- **Dependencias de Windows:** usa sockets de Windows (57 funciones `WSA*`), hilos, arrastrar y soltar con OLE, y libssh y OpenSSL enlazadas.
- **Licencia:** su biblioteca FTP (Ultimate TCP/IP) tiene la licencia de The Code Project, que no es compatible con la GPL. El catálogo no podría publicar el binario, y cada usuario tendría que compilarlo con libssh-dev y libssl-dev.

Escribirlo desde cero en Qt es menos trabajo, y el plugin sí puede ir en el catálogo con su binario.

## Cómo lo hace Notepad++

NppFTP 0.30.23 (`ashkulz/NppFTP`) es un panel acoplable con:

- una barra (conectar, descargar, subir, actualizar, abortar, ajustes);
- el árbol de carpetas remotas;
- una ventana de mensajes;
- una cola de transferencias.

Cada perfil guarda servidor, puerto, usuario, contraseña (cifrada con una contraseña maestra), seguridad (FTP, FTPES, FTPS, SFTP), modo pasivo o activo, carpeta inicial y la forma de autenticarse en SFTP (contraseña, clave o interactiva). Doble clic abre un archivo desde una caché local y al guardarlo se sube.

| NppFTP (N++) | FTP y SFTP de Edita |
|---|---|
| libssh, OpenSSL y Ultimate TCP/IP dentro de la DLL | Los programas del sistema: `ssh`/`sftp` (OpenSSH) y `curl`. El plugin no enlaza nada de red |
| Sus propias claves y su propio `known_hosts` | Los del usuario: `~/.ssh/config`, las claves, el agente y `~/.ssh/known_hosts` |
| Contraseñas guardadas, cifradas con una contraseña maestra | No se guardan: se piden al conectar y se recuerdan mientras Edita está abierto |
| SFTP, FTP, FTPES, FTPS; pasivo o activo | Igual |
| Abrir con doble clic desde la caché y subir al guardar | Igual (`~/.cache/edita/ftp/<perfil>/`) |
| Subir, descargar, crear y borrar carpetas, cambiar nombres, borrar | Igual |
| Cola de transferencias con progreso y abortar; arrastrar y soltar; permisos (chmod) | No, por ahora. Las operaciones van en cola, de una en una, sin bloquear la interfaz |

## Qué se hizo

`plugins/EditaFTP`, el primer plugin propio que **no va en los paquetes de Edita**. Lo publica el catálogo de plugins.

- **`FtpCore`**, sin interfaz:
  - **Perfiles** en `EditaFTP.ini`, sin contraseñas.
  - **Listados:** el `ls -l` de Unix (con o sin grupo, fechas `Nov 13 10:00`, `Mar  3 2024` o ISO, enlaces, el `?` de los enlaces del `sftp` de OpenSSH) y el formato MS-DOS de IIS.
  - **Rutas y caché:** las rutas remotas y la ruta de la copia local, que nunca sale de la caché aunque la ruta lleve `..`.
  - **Órdenes para cada petición:**
    - **SFTP:** `ssh -M` abre una conexión maestra (ControlMaster) que se autentica una vez. Después, cada operación es un `sftp -b -` sobre ella. La contraseña o la frase de paso llegan por `SSH_ASKPASS` (un guion que lee una variable de entorno de ese proceso).
    - **FTP:** `curl --config -`, con la configuración, y la contraseña, por la entrada estándar.
    - En ningún caso la contraseña va en los argumentos, que cualquiera ve con `ps`.
  - **Errores:** qué falló, según la salida de error y el código de salida.
- **`FtpSession`:**
  - Ejecuta las órdenes en cola con `QProcess`, sin bloquear.
  - Si la conexión maestra se cerró (ControlPersist de 30 minutos), vuelve a conectar una vez con la misma contraseña.
  - Al desconectar, cierra la conexión maestra (`ssh -O exit`).
- **El panel:**
  - Perfil, conectar o desconectar, y un diálogo de perfiles.
  - Árbol con carga perezosa; los enlaces se tratan como carpetas si se pueden listar.
  - Botones y menú contextual para actualizar, subir el documento actual, descargar en…, carpeta nueva, cambiar nombre y eliminar.
  - Un registro de cada operación.
- **Conexiones:**
  - Ante un **servidor desconocido**, muestra la huella (`ssh-keyscan` y `ssh-keygen -l`) y pregunta.
  - Si **la clave cambió**, no conecta y explica `ssh-keygen -R`.
  - Si **falta la contraseña**, la pide y reintenta.
  - Si **el certificado no es válido** (FTPES o FTPS), lo explica.
- **Al guardar** una copia remota (`NPPN_FILESAVED`), la sube. Si no hay conexión, avisa.

### Por qué así

- **Sin moc.** El SDK de las recetas trae las cabeceras de Qt, pero no `moc`, `rcc` ni `lrelease`. Por eso el plugin usa conexiones a funciones (`QObject::connect` con lambdas) y avisa con `std::function`. Las traducciones se cargan de `translations/editaftp_es.qm`, junto al `.so`. El `.qm` está en el repositorio, y `tst_editaftp` comprueba que es el que sale del `.ts`.
- **Escape de `sftp`.** `sftp` expande como patrón el origen de `get` y de `put`, y el de `rm`. Se probó con OpenSSH 10: entre comillas, la `\` se queda en el nombre. Por eso esos argumentos van sin comillas, con `\` delante de lo especial. Los demás (el destino, `rename`, `mkdir`, `rmdir`, `cd`) van entre comillas.
- **Fuera de los paquetes.** `edita_add_plugin(… NO_INSTALL)` lo compila en el árbol para las pruebas, pero no lo instala. `plugins/EditaFTP/sdk/CMakeLists.txt` lo compila con el SDK (`edita_native_plugin`). La receta de edita-plugins (`recipes/EditaFTP`) descarga el código de Edita de un commit y compila esa carpeta.

## Pruebas

`tst_editaftp` (20). `ssh`, `sftp`, `curl`, `ssh-keyscan` y `ssh-keygen` son programas falsos en Python. Trabajan sobre una carpeta, comprueban la contraseña como un servidor y anotan sus argumentos.

- **El núcleo:**
  - protocolos y puertos;
  - perfiles (ida y vuelta, sin contraseñas);
  - listados de Unix, de `sftp` y de MS-DOS;
  - `pwd`, rutas y caché;
  - las comillas de `sftp` y de `curl`, y las URL (`%2F` para rutas absolutas, IPv6);
  - las órdenes de SFTP y de FTP (y que la contraseña no está en los argumentos);
  - la clasificación de errores;
  - que los `.qm` estén al día.
- **La sesión:**
  - **SFTP:** conectar con contraseña (por `SSH_ASKPASS`), listar, descargar, subir, crear, renombrar y borrar; peticiones en orden; contraseña incorrecta; servidor desconocido con su huella; reconexión si la conexión maestra desaparece.
  - **FTP:** lo mismo, con una contraseña con comillas y `ñ`, y un listado MS-DOS.
- **El plugin en Edita:**
  - Conectar con el perfil y ver el árbol, y abrir una carpeta.
  - Doble clic en un archivo: se abre su copia.
  - Editar y guardar: el archivo del servidor cambia, y el registro lo anota.
  - Desconectar.

**De punta a punta, a mano**, con servidores reales en el equipo:

- **SFTP:** un `sshd` de OpenSSH 10.6 en el puerto 2222, sin permisos de administrador, con una clave con frase de paso y un `known_hosts` vacío. Se probaron:
  - el servidor desconocido con su huella;
  - el rechazo sin frase de paso y la conexión con ella;
  - un archivo `a*b [1] ñ.txt` en una carpeta con espacio: subir, listar, descargar y renombrar;
  - borrar una carpeta no vacía (error) y vacía;
  - que no queda ningún `ssh -M` al desconectar.
- **FTP y FTPES:** pyftpdlib 2.2, con una contraseña con comillas y `ñ` y un certificado propio. Las mismas operaciones. Con verificación, el certificado propio da el código 60 de `curl`, y una contraseña incorrecta, el 67.
