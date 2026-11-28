# Security policy

*Español más abajo.*

## Reporting a vulnerability

Please **do not open a public issue** for a security problem. Report it privately on GitHub: [Security › Report a vulnerability](https://github.com/alktrip/edita/security/advisories/new). Only the maintainers can see the report.

Include what you can: the Edita version (**Help › About Edita › Copy Version Info**), how it is installed (AppImage, `.deb`, `.rpm`, Flatpak, built from source), the steps to reproduce it and what an attacker could do with it.

What to expect:

- an acknowledgement within 7 days;
- an assessment, and a plan or a question, within 14 days;
- a fix in a new release, with the problem described in its notes once users have had a chance to update. You will be credited, unless you prefer not to be.

## Supported versions

Security fixes go into the latest release of the current series (1.x). Test versions (betas and release candidates) are not supported once a newer version is out.

## What is in scope

- **Edita itself**: opening, saving and parsing files (encodings, `langs.xml`, `stylers.xml`, sessions, macros, themes, user defined languages), the command line, the single-instance socket and the crash reporter.
- **The plugin catalog**: the signature check of the plugin list (Ed25519, key id `F1C3EC604B709A70`), the SHA-256 check of each package against that signed list, the installation of packages from Plugins Admin and the anti-rollback check.
- **Network features**: the update check, the catalog download and the FTP and SFTP plugin (how it handles passwords and host keys).
- **Running external programs**: language servers, external formatters, project tasks, the Run menu, the terminal and, in Flatpak, the bridge that runs host programs.
- **The secret scanner**: a way to make it miss a secret it should find, or to leak what it found.
- **Bundled plugins**: Python Script, MarkdownViewer++ (for example, script execution from a previewed document), the hex editor.

## Out of scope

- Problems in Notepad++ itself, or in a Notepad++ plugin's original code that also affect Notepad++: please report them to that project. If the Linux port in Edita's catalog is affected, tell us too.
- Problems in third-party components (Qt, Scintilla, Lexilla, libvterm, md4c, pugixml…) that are not caused by how Edita uses them: please report them upstream, and tell us if Edita needs an update.
- Running a program or a script that you chose to run (a task, a macro, a Python script, a language server) doing what that program does.

---

# Política de seguridad

## Reportar una vulnerabilidad

Por favor, **no abras un issue público** para un problema de seguridad. Repórtalo en privado en GitHub: [Security › Report a vulnerability](https://github.com/alktrip/edita/security/advisories/new). Solo quienes mantienen Edita pueden ver el reporte.

Incluye lo que puedas: la versión de Edita (**Ayuda › Acerca de Edita › Copiar la información de la versión**), cómo está instalada (AppImage, `.deb`, `.rpm`, Flatpak, compilada), los pasos para reproducirlo y lo que podría hacer alguien con él.

Qué esperar:

- una respuesta en 7 días como mucho;
- una evaluación, y un plan o una pregunta, en 14 días;
- la corrección en una versión nueva, con el problema descrito en sus notas cuando los usuarios hayan podido actualizar. Te mencionaremos, salvo que prefieras que no.

## Versiones con soporte

Las correcciones de seguridad van en la última versión de la serie actual (1.x). Las versiones de prueba (betas y candidatas) no tienen soporte una vez que sale una más nueva.

## Qué cubre

- **Edita**: abrir, guardar y leer archivos (codificaciones, `langs.xml`, `stylers.xml`, sesiones, macros, temas, lenguajes definidos por el usuario), la línea de órdenes, el socket de instancia única y el reporte de fallos.
- **El catálogo de plugins**: la comprobación de la firma de la lista de plugins (Ed25519, clave `F1C3EC604B709A70`), la del SHA-256 de cada paquete con esa lista firmada, la instalación desde el administrador de plugins y la protección contra versiones anteriores.
- **Lo que usa la red**: el aviso de versiones, la descarga del catálogo y el plugin de FTP y SFTP (cómo trata las contraseñas y las claves de los servidores).
- **Ejecutar programas externos**: servidores de lenguaje, formateadores, tareas del proyecto, el menú Ejecutar, la terminal y, en Flatpak, el puente que ejecuta programas del sistema.
- **El detector de secretos**: una forma de que no vea un secreto que debería encontrar, o de que filtre lo que encontró.
- **Los plugins incluidos**: Python Script, MarkdownViewer++ (por ejemplo, ejecutar código desde un documento en la vista previa) y el editor hexadecimal.

## Qué no cubre

- Problemas de Notepad++ o del código original de un plugin de Notepad++ que también afecten a Notepad++: repórtalos a ese proyecto. Si afecta a la versión para Linux del catálogo de Edita, avísanos también.
- Problemas de componentes de terceros (Qt, Scintilla, Lexilla, libvterm, md4c, pugixml…) que no se deban a cómo los usa Edita: repórtalos en su proyecto y avísanos si Edita necesita actualizarlo.
- Que un programa o script que elegiste ejecutar (una tarea, una macro, un script de Python, un servidor de lenguaje) haga lo que ese programa hace.
