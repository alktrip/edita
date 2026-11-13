// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "FtpCore.h"

#include <QDir>
#include <QRegularExpression>
#include <QSettings>
#include <QUrl>

#include <algorithm>

namespace edita::ftp {

QString protocolKey(Protocol protocol)
{
    switch (protocol) {
    case Protocol::Sftp:
        return QStringLiteral("sftp");
    case Protocol::Ftp:
        return QStringLiteral("ftp");
    case Protocol::FtpExplicitTls:
        return QStringLiteral("ftpes");
    case Protocol::FtpImplicitTls:
        return QStringLiteral("ftps");
    }
    return QString();
}

std::optional<Protocol> protocolFromKey(const QString& key)
{
    for (const Protocol p : {Protocol::Sftp, Protocol::Ftp, Protocol::FtpExplicitTls, Protocol::FtpImplicitTls}) {
        if (protocolKey(p) == key)
            return p;
    }
    return std::nullopt;
}

int defaultPort(Protocol protocol)
{
    switch (protocol) {
    case Protocol::Sftp:
        return 22;
    case Protocol::FtpImplicitTls:
        return 990;
    case Protocol::Ftp:
    case Protocol::FtpExplicitTls:
        break;
    }
    return 21;
}

bool isFtp(Protocol protocol)
{
    return protocol != Protocol::Sftp;
}

// --- Perfiles -------------------------------------------------------------------------------

std::vector<Profile> loadProfiles(const QString& iniPath)
{
    std::vector<Profile> profiles;
    if (!QFileInfo::exists(iniPath))
        return profiles;
    QSettings s(iniPath, QSettings::IniFormat);
    const int count = s.beginReadArray(QStringLiteral("profiles"));
    for (int i = 0; i < count; ++i) {
        s.setArrayIndex(i);
        Profile p;
        p.name = s.value(QStringLiteral("name")).toString();
        p.protocol = protocolFromKey(s.value(QStringLiteral("protocol")).toString()).value_or(Protocol::Sftp);
        p.host = s.value(QStringLiteral("host")).toString();
        p.port = s.value(QStringLiteral("port"), 0).toInt();
        p.user = s.value(QStringLiteral("user")).toString();
        p.initialDir = s.value(QStringLiteral("initialDir")).toString();
        p.keyFile = s.value(QStringLiteral("keyFile")).toString();
        p.passive = s.value(QStringLiteral("passive"), true).toBool();
        p.verifyCertificate = s.value(QStringLiteral("verifyCertificate"), true).toBool();
        if (!p.name.isEmpty())
            profiles.push_back(p);
    }
    s.endArray();
    return profiles;
}

bool saveProfiles(const QString& iniPath, const std::vector<Profile>& profiles)
{
    QDir().mkpath(QFileInfo(iniPath).absolutePath());
    QSettings s(iniPath, QSettings::IniFormat);
    s.remove(QStringLiteral("profiles"));
    s.beginWriteArray(QStringLiteral("profiles"), static_cast<int>(profiles.size()));
    for (int i = 0; i < static_cast<int>(profiles.size()); ++i) {
        const Profile& p = profiles[static_cast<size_t>(i)];
        s.setArrayIndex(i);
        s.setValue(QStringLiteral("name"), p.name);
        s.setValue(QStringLiteral("protocol"), protocolKey(p.protocol));
        s.setValue(QStringLiteral("host"), p.host);
        s.setValue(QStringLiteral("port"), p.port);
        s.setValue(QStringLiteral("user"), p.user);
        s.setValue(QStringLiteral("initialDir"), p.initialDir);
        s.setValue(QStringLiteral("keyFile"), p.keyFile);
        s.setValue(QStringLiteral("passive"), p.passive);
        s.setValue(QStringLiteral("verifyCertificate"), p.verifyCertificate);
    }
    s.endArray();
    s.sync();
    return s.status() == QSettings::NoError;
}

// --- Listados -------------------------------------------------------------------------------

std::vector<Entry> parseListing(const QString& text)
{
    // ls -l: permisos, enlaces, dueño, [grupo], tamaño, fecha (Nov 13 10:00, Mar  3 2024 o
    // 2026-11-13 10:00) y nombre
    static const QRegularExpression unixLine(QStringLiteral(
        R"(^([-dlbcps][-rwxsStTl]{9})[+@.]?\s+(?:\d+|\?)\s+\S+\s+(?:\S+\s+)?(\d+)\s+)"
        R"(([A-Za-z]{3}\s+\d{1,2}\s+(?:\d{1,2}:\d{2}|\d{4})|\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2})\s(.+)$)"));
    // MS-DOS (IIS): 11-13-26  10:00AM  <DIR>  nombre
    static const QRegularExpression dosLine(QStringLiteral(
        R"(^(\d{2}-\d{2}-\d{2,4})\s+(\d{1,2}:\d{2}(?:[AP]M)?)\s+(<DIR>|\d+)\s+(.+)$)"));

    std::vector<Entry> entries;
    for (QString line : text.split(QLatin1Char('\n'))) {
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        Entry e;
        if (const auto m = unixLine.match(line); m.hasMatch()) {
            e.permissions = m.captured(1);
            e.directory = e.permissions.startsWith(QLatin1Char('d'));
            e.link = e.permissions.startsWith(QLatin1Char('l'));
            e.size = m.captured(2).toLongLong();
            e.modified = m.captured(3);
            e.name = m.captured(4);
            if (e.link) {
                const qsizetype arrow = e.name.indexOf(QLatin1String(" -> "));
                if (arrow >= 0) {
                    e.linkTarget = e.name.mid(arrow + 4);
                    e.name.truncate(arrow);
                }
            }
        } else if (const auto d = dosLine.match(line); d.hasMatch()) {
            e.directory = d.captured(3) == QLatin1String("<DIR>");
            e.size = e.directory ? -1 : d.captured(3).toLongLong();
            e.modified = d.captured(1) + QLatin1Char(' ') + d.captured(2);
            e.name = d.captured(4);
        } else {
            continue;
        }
        if (e.name == QLatin1String(".") || e.name == QLatin1String(".."))
            continue;
        entries.push_back(e);
    }
    std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        if (a.directory != b.directory)
            return a.directory;
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });
    return entries;
}

QString parseWorkingDirectory(const QString& text)
{
    static const QString prefix = QStringLiteral("Remote working directory: ");
    for (const QString& line : text.split(QLatin1Char('\n'))) {
        if (line.startsWith(prefix))
            return line.mid(prefix.size()).trimmed();
    }
    return QString();
}

// --- Rutas ----------------------------------------------------------------------------------

QString joinPath(const QString& dir, const QString& name)
{
    if (dir.isEmpty())
        return name;
    return dir.endsWith(QLatin1Char('/')) ? dir + name : dir + QLatin1Char('/') + name;
}

QString parentPath(const QString& path)
{
    const qsizetype slash = path.lastIndexOf(QLatin1Char('/'));
    if (slash < 0)
        return QString();
    if (slash == 0)
        return QStringLiteral("/");
    return path.left(slash);
}

QString fileName(const QString& path)
{
    return path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);
}

QString cachePath(const QString& cacheRoot, const QString& profileName, const QString& remotePath)
{
    QString profile = profileName;
    profile.replace(QLatin1Char('/'), QLatin1Char('_'));
    if (profile.isEmpty() || profile == QLatin1String(".") || profile == QLatin1String(".."))
        profile = QStringLiteral("_");
    QStringList parts{profile};
    if (!remotePath.startsWith(QLatin1Char('/')))
        parts << QStringLiteral("~");
    // «..» se descarta, como si la ruta fuera absoluta: nunca se sale de la raíz
    QStringList segments;
    for (const QString& s : remotePath.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        if (s == QLatin1String(".") || s == QLatin1String(".."))
            continue;
        segments << s;
    }
    parts += segments;
    return QDir::cleanPath(cacheRoot + QLatin1Char('/') + parts.join(QLatin1Char('/')));
}

// --- Órdenes --------------------------------------------------------------------------------

QString sftpQuote(const QString& text)
{
    QString out = text;
    out.replace(QLatin1Char('"'), QLatin1String("\\\""));
    return QLatin1Char('"') + out + QLatin1Char('"');
}

QString sftpEscape(const QString& text)
{
    static const QString special = QStringLiteral(" \t\"'\\*?[]#");
    QString out;
    for (const QChar c : text) {
        if (special.contains(c))
            out += QLatin1Char('\\');
        out += c;
    }
    return out;
}

QString curlQuote(const QString& text)
{
    QString out = QStringLiteral("\"");
    for (const QChar c : text) {
        if (c == QLatin1Char('"') || c == QLatin1Char('\\'))
            out += QLatin1Char('\\') + c;
        else if (c == QLatin1Char('\n'))
            out += QStringLiteral("\\n");
        else if (c == QLatin1Char('\r'))
            out += QStringLiteral("\\r");
        else if (c == QLatin1Char('\t'))
            out += QStringLiteral("\\t");
        else
            out += c;
    }
    return out + QLatin1Char('"');
}

QString ftpUrl(const Profile& profile, const QString& path, bool directory)
{
    QString host = profile.host;
    if (host.contains(QLatin1Char(':')) && !host.startsWith(QLatin1Char('[')))
        host = QLatin1Char('[') + host + QLatin1Char(']');
    QString url = (profile.protocol == Protocol::FtpImplicitTls ? QStringLiteral("ftps://") : QStringLiteral("ftp://")) + host
                + QLatin1Char(':') + QString::number(profile.effectivePort()) + QLatin1Char('/');
    QStringList segments;
    for (const QString& s : path.split(QLatin1Char('/'), Qt::SkipEmptyParts))
        segments << QString::fromLatin1(QUrl::toPercentEncoding(s));
    // Una ruta absoluta empieza por %2F: la primera «/» de la URL solo separa el servidor
    if (path.startsWith(QLatin1Char('/')))
        url += QStringLiteral("%2F");
    url += segments.join(QLatin1Char('/'));
    if (directory && !segments.isEmpty())
        url += QLatin1Char('/');
    return url;
}

QByteArray askPassScript()
{
    return QByteArrayLiteral("#!/bin/sh\n"
                             "# SSH_ASKPASS de Edita (FTP): escribe la contraseña de la sesión\n"
                             "[ -n \"$") + kSecretVariable + QByteArrayLiteral("\" ] || exit 1\n"
                             "printf '%s\\n' \"$") + kSecretVariable + QByteArrayLiteral("\"\n");
}

namespace {

QString sshDestination(const Profile& profile)
{
    return profile.user.isEmpty() ? profile.host : profile.user + QLatin1Char('@') + profile.host;
}

QStringList sshCommonOptions(const Profile& profile, const QString& portFlag)
{
    QStringList args{QStringLiteral("-o"), QStringLiteral("ConnectTimeout=20"), portFlag, QString::number(profile.effectivePort())};
    if (!profile.keyFile.isEmpty()) {
        QString key = profile.keyFile;
        if (key.startsWith(QLatin1String("~/")))
            key = QDir::homePath() + key.mid(1);
        args << QStringLiteral("-i") << key;
    }
    return args;
}

} // namespace

Invocation sshConnect(const Profile& profile, const SshContext& ssh, const QString& password, bool acceptNewHostKey)
{
    Invocation inv;
    inv.program = QStringLiteral("ssh");
    inv.arguments << QStringLiteral("-M") << QStringLiteral("-S") << ssh.controlPath << QStringLiteral("-f") << QStringLiteral("-N")
                  << QStringLiteral("-o") << QStringLiteral("ControlPersist=30m") << QStringLiteral("-o")
                  << (acceptNewHostKey ? QStringLiteral("StrictHostKeyChecking=accept-new") : QStringLiteral("StrictHostKeyChecking=yes"))
                  << QStringLiteral("-o") << QStringLiteral("NumberOfPasswordPrompts=1");
    inv.arguments << sshCommonOptions(profile, QStringLiteral("-p")) << sshDestination(profile);
    inv.environment << std::pair{QStringLiteral("SSH_ASKPASS"), ssh.askPass}
                    << std::pair{QStringLiteral("SSH_ASKPASS_REQUIRE"), QStringLiteral("force")}
                    << std::pair{QString::fromLatin1(kSecretVariable), password};
    return inv;
}

Invocation sshDisconnect(const Profile& profile, const SshContext& ssh)
{
    Invocation inv;
    inv.program = QStringLiteral("ssh");
    inv.arguments << QStringLiteral("-S") << ssh.controlPath << QStringLiteral("-O") << QStringLiteral("exit")
                  << QStringLiteral("-p") << QString::number(profile.effectivePort()) << sshDestination(profile);
    return inv;
}

Invocation sshKeyScan(const Profile& profile)
{
    Invocation inv;
    inv.program = QStringLiteral("ssh-keyscan");
    inv.arguments << QStringLiteral("-T") << QStringLiteral("10") << QStringLiteral("-p") << QString::number(profile.effectivePort())
                  << profile.host;
    return inv;
}

Invocation sshFingerprint(const QByteArray& keyScanOutput)
{
    Invocation inv;
    inv.program = QStringLiteral("ssh-keygen");
    inv.arguments << QStringLiteral("-l") << QStringLiteral("-f") << QStringLiteral("-");
    inv.input = keyScanOutput;
    return inv;
}

namespace {

Invocation sftpInvocation(const Profile& profile, const SshContext& ssh, const Request& request)
{
    Invocation inv;
    inv.program = QStringLiteral("sftp");
    inv.arguments << QStringLiteral("-b") << QStringLiteral("-") << QStringLiteral("-o")
                  << QStringLiteral("ControlPath=") + ssh.controlPath << QStringLiteral("-o") << QStringLiteral("ControlMaster=no")
                  << QStringLiteral("-o") << QStringLiteral("BatchMode=yes");
    inv.arguments << sshCommonOptions(profile, QStringLiteral("-P")) << sshDestination(profile);
    QString script;
    switch (request.kind) {
    case Request::List:
        if (!request.remote.isEmpty())
            script = QStringLiteral("cd %1\n").arg(sftpQuote(request.remote));
        script += QStringLiteral("pwd\nls -la\n");
        break;
    case Request::Download:
        script = QStringLiteral("get %1 %2\n").arg(sftpEscape(request.remote), sftpQuote(request.local));
        break;
    case Request::Upload:
        script = QStringLiteral("put %1 %2\n").arg(sftpEscape(request.local), sftpQuote(request.remote));
        break;
    case Request::MakeDir:
        script = QStringLiteral("mkdir %1\n").arg(sftpQuote(request.remote));
        break;
    case Request::RemoveFile:
        script = QStringLiteral("rm %1\n").arg(sftpEscape(request.remote));
        break;
    case Request::RemoveDir:
        script = QStringLiteral("rmdir %1\n").arg(sftpQuote(request.remote));
        break;
    case Request::Rename:
        script = QStringLiteral("rename %1 %2\n").arg(sftpQuote(request.remote), sftpQuote(request.target));
        break;
    }
    inv.input = script.toUtf8();
    return inv;
}

Invocation curlInvocation(const Profile& profile, const QString& password, const Request& request)
{
    Invocation inv;
    inv.program = QStringLiteral("curl");
    inv.arguments << QStringLiteral("--silent") << QStringLiteral("--show-error") << QStringLiteral("--config") << QStringLiteral("-");
    QStringList lines;
    auto set = [&lines](const QString& key, const QString& value) { lines << key + QStringLiteral(" = ") + curlQuote(value); };
    switch (request.kind) {
    case Request::List:
        set(QStringLiteral("url"), ftpUrl(profile, request.remote, true));
        break;
    case Request::Download:
        set(QStringLiteral("url"), ftpUrl(profile, request.remote, false));
        set(QStringLiteral("output"), request.local);
        break;
    case Request::Upload:
        set(QStringLiteral("url"), ftpUrl(profile, request.remote, false));
        set(QStringLiteral("upload-file"), request.local);
        break;
    case Request::MakeDir:
    case Request::RemoveFile:
    case Request::RemoveDir:
    case Request::Rename: {
        // Las órdenes de FTP van después de entrar; el listado de la carpeta de inicio que
        // hace curl después no interesa
        set(QStringLiteral("url"), ftpUrl(profile, QString(), true));
        set(QStringLiteral("output"), QStringLiteral("/dev/null"));
        if (request.kind == Request::MakeDir)
            set(QStringLiteral("quote"), QStringLiteral("MKD ") + request.remote);
        else if (request.kind == Request::RemoveFile)
            set(QStringLiteral("quote"), QStringLiteral("DELE ") + request.remote);
        else if (request.kind == Request::RemoveDir)
            set(QStringLiteral("quote"), QStringLiteral("RMD ") + request.remote);
        else {
            set(QStringLiteral("quote"), QStringLiteral("RNFR ") + request.remote);
            set(QStringLiteral("quote"), QStringLiteral("RNTO ") + request.target);
        }
        break;
    }
    }
    if (!profile.user.isEmpty())
        set(QStringLiteral("user"), profile.user + QLatin1Char(':') + password);
    if (profile.passive)
        lines << QStringLiteral("ftp-pasv");
    else
        set(QStringLiteral("ftp-port"), QStringLiteral("-"));
    if (profile.protocol == Protocol::FtpExplicitTls)
        lines << QStringLiteral("ssl-reqd");
    if (profile.protocol != Protocol::Ftp && !profile.verifyCertificate)
        lines << QStringLiteral("insecure");
    lines << QStringLiteral("connect-timeout = 20");
    inv.input = (lines.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8();
    return inv;
}

} // namespace

Invocation requestInvocation(const Profile& profile, const SshContext& ssh, const QString& password, const Request& request)
{
    return isFtp(profile.protocol) ? curlInvocation(profile, password, request) : sftpInvocation(profile, ssh, request);
}

ErrorKind classifyError(Protocol protocol, int exitCode, const QString& errorOutput)
{
    if (exitCode == 0)
        return ErrorKind::None;
    if (isFtp(protocol)) {
        switch (exitCode) {
        case 67:
            return ErrorKind::AuthenticationFailed;
        case 35:
        case 51:
        case 58:
        case 60:
        case 83:
        case 90:
        case 91:
            return ErrorKind::Certificate;
        case 5:
        case 6:
        case 7:
        case 28:
            return ErrorKind::CannotConnect;
        case 9:
        case 19:
        case 78:
            return ErrorKind::NotFound;
        default:
            return ErrorKind::Other;
        }
    }
    if (errorOutput.contains(QLatin1String("REMOTE HOST IDENTIFICATION HAS CHANGED")))
        return ErrorKind::HostKeyChanged;
    if (errorOutput.contains(QLatin1String("Host key verification failed")))
        return ErrorKind::HostKeyUnknown;
    if (errorOutput.contains(QLatin1String("Permission denied")) || errorOutput.contains(QLatin1String("Too many authentication failures")))
        return ErrorKind::AuthenticationFailed;
    if (errorOutput.contains(QLatin1String("not found")) || errorOutput.contains(QLatin1String("No such file")))
        return ErrorKind::NotFound;
    if (errorOutput.contains(QLatin1String("ssh: ")) || errorOutput.contains(QLatin1String("Connection closed"))
        || errorOutput.contains(QLatin1String("Could not resolve")) || errorOutput.contains(QLatin1String("timed out")))
        return ErrorKind::CannotConnect;
    return ErrorKind::Other;
}

} // namespace edita::ftp
