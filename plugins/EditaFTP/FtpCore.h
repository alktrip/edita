// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// FTP y SFTP (fase 90): lo que no depende de la interfaz. Los perfiles, la lectura de los
// listados (ls -l de Unix y el formato de MS-DOS de IIS), las rutas y las órdenes que se
// ejecutan: sftp y ssh de OpenSSH para SFTP, curl para FTP, FTPES y FTPS. Así el plugin no
// enlaza ninguna biblioteca de red y usa la configuración de SSH del usuario (~/.ssh/config,
// el agente, known_hosts).
//
// Las contraseñas nunca van en los argumentos (cualquiera los ve en ps): a curl se le pasan
// por su entrada (--config -) y a ssh, por SSH_ASKPASS.

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>
#include <utility>
#include <vector>

namespace edita::ftp {

enum class Protocol {
    Sftp,           // SSH
    Ftp,            // sin cifrar
    FtpExplicitTls, // FTPES: AUTH TLS en el puerto 21
    FtpImplicitTls, // FTPS: TLS desde el principio, puerto 990
};

QString protocolKey(Protocol protocol);                     // "sftp", "ftp", "ftpes", "ftps"
std::optional<Protocol> protocolFromKey(const QString& key);
int defaultPort(Protocol protocol);
bool isFtp(Protocol protocol);

struct Profile
{
    QString name;
    Protocol protocol = Protocol::Sftp;
    QString host;
    int port = 0; // 0: el del protocolo
    QString user;
    QString initialDir;  // vacío: la carpeta de inicio del usuario
    QString keyFile;     // SFTP: la clave privada (vacío: la de ~/.ssh o el agente)
    bool passive = true; // FTP: modo pasivo
    bool verifyCertificate = true; // FTPES y FTPS

    int effectivePort() const { return port > 0 ? port : defaultPort(protocol); }
    bool operator==(const Profile&) const = default;
};

// Los perfiles, en un .ini (sin contraseñas: se piden al conectar)
std::vector<Profile> loadProfiles(const QString& iniPath);
bool saveProfiles(const QString& iniPath, const std::vector<Profile>& profiles);

struct Entry
{
    QString name;
    bool directory = false;
    bool link = false;
    QString linkTarget;
    qint64 size = -1;
    QString modified;    // como lo da el servidor
    QString permissions; // drwxr-xr-x

    bool operator==(const Entry&) const = default;
};

// Un listado de LIST (FTP) o de «ls -la» (sftp). Sin «.» ni «..»; las carpetas primero y
// por nombre. Las líneas que no entiende (total, el eco de sftp…) se saltan.
std::vector<Entry> parseListing(const QString& text);
// La carpeta de «pwd» en la salida de sftp («Remote working directory: /home/ana»)
QString parseWorkingDirectory(const QString& text);

// Rutas remotas, siempre con «/». En FTP pueden ser relativas a la carpeta en la que
// empieza la sesión (la ruta vacía es esa carpeta).
QString joinPath(const QString& dir, const QString& name);
QString parentPath(const QString& path);
QString fileName(const QString& path);
// La copia local de un archivo remoto: <raíz>/<perfil>/<ruta>. No sale de la raíz aunque
// la ruta lleve «..».
QString cachePath(const QString& cacheRoot, const QString& profileName, const QString& remotePath);

// Lo que se pide al servidor
struct Request
{
    enum Kind { List, Download, Upload, MakeDir, RemoveFile, RemoveDir, Rename };
    Kind kind = List;
    QString remote; // la carpeta (List) o el archivo
    QString local;  // Download y Upload
    QString target; // Rename: la ruta nueva
};

// Un programa con sus argumentos, lo que se le escribe en la entrada y las variables de
// entorno que se le añaden
struct Invocation
{
    QString program;
    QStringList arguments;
    QByteArray input;
    QList<std::pair<QString, QString>> environment;
};

// Lo que hace falta para SFTP: el socket de la conexión maestra de ssh (ControlPath) y el
// programa de SSH_ASKPASS, que escribe la contraseña de la variable kSecretVariable
struct SshContext
{
    QString controlPath;
    QString askPass;
};
inline constexpr const char* kSecretVariable = "EDITA_FTP_SECRET";
// El guion de SSH_ASKPASS
QByteArray askPassScript();

// SFTP: abrir la conexión maestra (ssh se autentica una vez; sftp la reutiliza). Con
// acceptNewHostKey, la clave de un servidor nuevo se añade a known_hosts; si no, un
// servidor desconocido es un error (HostKeyUnknown) y se pregunta al usuario.
Invocation sshConnect(const Profile& profile, const SshContext& ssh, const QString& password, bool acceptNewHostKey);
Invocation sshDisconnect(const Profile& profile, const SshContext& ssh);
// La huella de la clave del servidor, para preguntar si se confía en él
Invocation sshKeyScan(const Profile& profile);
Invocation sshFingerprint(const QByteArray& keyScanOutput);

// La orden para una petición: sftp -b - (SFTP) o curl --config - (FTP)
Invocation requestInvocation(const Profile& profile, const SshContext& ssh, const QString& password, const Request& request);

// Los argumentos de sftp. sftp expande como patrón (*, ?, [) el origen de get y de put y el
// de rm: esos van sin comillas, con «\» delante de lo especial (sftpEscape); dentro de
// comillas, la «\» se quedaría en el nombre. Los demás (el destino, rename, mkdir, rmdir,
// cd) no se expanden: entre comillas (sftpQuote).
QString sftpQuote(const QString& text);
QString sftpEscape(const QString& text);
// Las comillas de la configuración de curl
QString curlQuote(const QString& text);
// La URL de una ruta: ftp://host:21/carpeta/ («%2F» al principio si es absoluta)
QString ftpUrl(const Profile& profile, const QString& path, bool directory);

enum class ErrorKind {
    None,
    AuthenticationFailed,
    HostKeyUnknown,
    HostKeyChanged,
    ConnectionLost,  // la conexión maestra de ssh ya no está
    Certificate,     // FTPES/FTPS: el certificado del servidor no es válido
    CannotConnect,
    NotFound,
    Other,
};
// Qué falló, a partir de la salida de error y del código de salida
ErrorKind classifyError(Protocol protocol, int exitCode, const QString& errorOutput);

} // namespace edita::ftp
