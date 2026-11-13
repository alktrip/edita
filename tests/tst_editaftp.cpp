// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 90: FTP y SFTP (plugins/EditaFTP), un plugin propio de Edita que hace lo que NppFTP
// en Notepad++. Las pruebas no necesitan un servidor: ssh, sftp y curl son programas falsos
// (en Python) que trabajan sobre una carpeta, y comprueban la contraseña como el servidor.

#include "FakePrompter.h"
#include "../plugins/EditaFTP/FtpCore.h"
#include "../plugins/EditaFTP/FtpSession.h"
#include "ui/MainWindow.h"
#include "ui/PluginManager.h"
#include "ui/EditorView.h"

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTreeWidget>

using namespace edita;
using namespace edita::ftp;
using edita::test::FakePrompter;

namespace {

QByteArray readFile(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

bool writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
}

// El servidor falso: $FAKE_REMOTE es la raíz y /home/ana, la carpeta de inicio. Cada programa
// apunta en $FAKE_LOG sus argumentos (para comprobar que la contraseña no va en ellos).
const char* kCommon = R"PY(#!/usr/bin/env python3
import os, sys, shlex, stat, time, subprocess
ROOT = os.environ["FAKE_REMOTE"]
HOME = "/home/ana"
def log(text):
    with open(os.environ["FAKE_LOG"], "a") as f:
        f.write(text + "\n")
log(os.path.basename(sys.argv[0]) + " " + " ".join(sys.argv[1:]))
def local(path, cwd=HOME):
    if not path.startswith("/"):
        path = cwd.rstrip("/") + "/" + path
    return ROOT + os.path.normpath(path)
def longname(full, name):
    st = os.lstat(full)
    kind = "d" if stat.S_ISDIR(st.st_mode) else ("l" if stat.S_ISLNK(st.st_mode) else "-")
    line = "%srw-r--r--    ? ana      ana      %8d Nov 13 10:00 %s" % (kind, st.st_size, name)
    if kind == "l":
        line += " -> " + os.readlink(full)
    return line
def listing(path):
    out = [longname(path, "."), longname(path, "..")] if os.environ.get("FAKE_DOTS") else []
    for name in sorted(os.listdir(path)):
        out.append(longname(os.path.join(path, name), name))
    return out
)PY";

const char* kSsh = R"PY(
args = sys.argv[1:]
if "-O" in args:
    if args[args.index("-O") + 1] == "exit":
        try: os.remove(args[args.index("-S") + 1])
        except OSError: pass
    sys.exit(0)
if "StrictHostKeyChecking=yes" in args and os.environ.get("FAKE_UNKNOWN_HOST"):
    sys.stderr.write("No ED25519 host key is known for server and you have requested strict checking.\nHost key verification failed.\n")
    sys.exit(255)
expected = os.environ.get("FAKE_PASSWORD", "")
if expected:
    given = subprocess.run([os.environ["SSH_ASKPASS"], "ana@server's password: "], capture_output=True, text=True).stdout.rstrip("\n")
    if given != expected:
        sys.stderr.write("ana@server: Permission denied (publickey,password).\n")
        sys.exit(255)
open(args[args.index("-S") + 1], "w").close()
sys.exit(0)
)PY";

const char* kSftp = R"PY(
args = sys.argv[1:]
control = [a for a in args if a.startswith("ControlPath=")]
if not control or not os.path.exists(control[0][len("ControlPath="):]):
    sys.stderr.write("ana@server: Permission denied (publickey,password).\nConnection closed\n")
    sys.exit(255)
cwd = HOME
for raw in sys.stdin.read().splitlines():
    if not raw.strip():
        continue
    print("sftp> " + raw)
    words = shlex.split(raw)
    cmd, rest = words[0], words[1:]
    log("  " + cmd + " " + " | ".join(rest))
    try:
        if cmd == "cd":
            target = os.path.normpath(rest[0] if rest[0].startswith("/") else cwd + "/" + rest[0])
            if not os.path.isdir(ROOT + target):
                raise FileNotFoundError(rest[0])
            cwd = target
        elif cmd == "pwd":
            print("Remote working directory: " + cwd)
        elif cmd == "ls":
            for line in listing(local(cwd)):
                print(line)
        elif cmd == "get":
            with open(local(rest[0], cwd), "rb") as src, open(rest[1], "wb") as dst:
                dst.write(src.read())
        elif cmd == "put":
            with open(rest[0], "rb") as src, open(local(rest[1], cwd), "wb") as dst:
                dst.write(src.read())
        elif cmd == "mkdir":
            os.mkdir(local(rest[0], cwd))
        elif cmd == "rm":
            os.remove(local(rest[0], cwd))
        elif cmd == "rmdir":
            os.rmdir(local(rest[0], cwd))
        elif cmd == "rename":
            os.rename(local(rest[0], cwd), local(rest[1], cwd))
    except OSError:
        sys.stderr.write('File "%s" not found.\n' % rest[0])
        sys.exit(1)
)PY";

const char* kCurl = R"PY(
config = {}
quotes = []
flags = set()
for line in sys.stdin.read().splitlines():
    key, _, value = line.partition("=")
    key, value = key.strip(), value.strip()
    if not value:
        flags.add(key)
        continue
    value = shlex.split(value)[0] if value.startswith('"') else value
    if key == "quote":
        quotes.append(value)
    else:
        config[key] = value
log("  config " + " ".join(sorted(k for k in list(config) + list(flags) if k != "user")) + " " + config.get("url", ""))
expected = os.environ.get("FAKE_PASSWORD", "")
if expected and config.get("user", "") != "ana:" + expected:
    sys.stderr.write("curl: (67) Access denied: 530\n")
    sys.exit(67)
from urllib.parse import unquote
url = config["url"]
path = url.split("/", 3)[3] if url.count("/") >= 3 else ""
path = unquote(path)
full = local(path)
for q in quotes:
    verb, _, arg = q.partition(" ")
    target = local(arg)
    if verb == "MKD": os.mkdir(target)
    elif verb == "RMD": os.rmdir(target)
    elif verb == "DELE": os.remove(target)
    elif verb == "RNFR": renaming = target
    elif verb == "RNTO": os.rename(renaming, target)
if "upload-file" in config:
    if url.endswith("/"):
        full = os.path.join(full, os.path.basename(config["upload-file"]))
    with open(config["upload-file"], "rb") as src, open(full, "wb") as dst:
        dst.write(src.read())
elif "output" in config:
    if not os.path.exists(full):
        sys.stderr.write("curl: (78) The file does not exist\n")
        sys.exit(78)
    if os.path.isfile(full):
        with open(full, "rb") as src, open(config["output"], "wb") as dst:
            dst.write(src.read())
elif url.endswith("/"):
    if not os.path.isdir(full):
        sys.stderr.write("curl: (9) Server denied you to change to the given directory\n")
        sys.exit(9)
    if os.environ.get("FAKE_DOS"):
        for name in sorted(os.listdir(full)):
            p = os.path.join(full, name)
            print("11-13-26  10:00AM  %s %s" % ("     <DIR>         " if os.path.isdir(p) else "%18d" % os.path.getsize(p), name))
    else:
        for line in listing(full):
            print(line)
)PY";

const char* kKeyScan = R"PY(
print("server ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIFakeKeyForTests")
)PY";

const char* kKeyGen = R"PY(
data = sys.stdin.read()
print("256 SHA256:FakeFingerprintForTests server (ED25519)" if "ssh-ed25519" in data else "")
)PY";

} // namespace

class TestEditaFtp : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;
    QString _remote;
    QString _log;

    QString remote(const QString& path) const { return _remote + path; }

    Profile sftpProfile() const
    {
        Profile p;
        p.name = QStringLiteral("Servidor");
        p.protocol = Protocol::Sftp;
        p.host = QStringLiteral("server");
        p.user = QStringLiteral("ana");
        return p;
    }
    Profile ftpProfile() const
    {
        Profile p = sftpProfile();
        p.name = QStringLiteral("FTP");
        p.protocol = Protocol::Ftp;
        return p;
    }

    // Espera al resultado de una petición
    static Result wait(ftp::Session& s, const std::function<void(ftp::Session::Callback)>& start)
    {
        Result result;
        bool done = false;
        start([&](const Result& r) {
            result = r;
            done = true;
        });
        for (int i = 0; i < 1000 && !done; ++i)
            QTest::qWait(10);
        if (!done)
            result.message = QStringLiteral("timeout");
        Q_UNUSED(s);
        return result;
    }
    static Result connect(ftp::Session& s, bool acceptNew = false)
    {
        return wait(s, [&](ftp::Session::Callback cb) { s.connectToHost(acceptNew, cb); });
    }
    static Result run(ftp::Session& s, const Request& r)
    {
        return wait(s, [&](ftp::Session::Callback cb) { s.run(r, cb); });
    }
    static QStringList names(const std::vector<Entry>& entries)
    {
        QStringList out;
        for (const Entry& e : entries)
            out << e.name + (e.directory ? QStringLiteral("/") : QString());
        return out;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        qputenv("XDG_CACHE_HOME", _tmp.filePath("cache").toUtf8());
        qputenv("EDITA_UPDATE_URL", "none");
        QVERIFY2(QFileInfo::exists(EDITA_PLUGIN_BUILD_DIR "/EditaFTP/EditaFTP.so"), "falta el plugin");
        qputenv("EDITA_PLUGIN_PATH", EDITA_PLUGIN_BUILD_DIR);
        // Los programas falsos, antes que los del sistema
        const QString bin = _tmp.filePath(QStringLiteral("bin"));
        QDir().mkpath(bin);
        const std::pair<const char*, const char*> tools[] = {
            {"ssh", kSsh}, {"sftp", kSftp}, {"curl", kCurl}, {"ssh-keyscan", kKeyScan}, {"ssh-keygen", kKeyGen}};
        for (const auto& [name, body] : tools) {
            const QString path = QDir(bin).filePath(QLatin1String(name));
            QVERIFY(writeFile(path, QByteArray(kCommon) + body));
            QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        }
        qputenv("PATH", (bin + QLatin1Char(':') + qEnvironmentVariable("PATH")).toUtf8());
        _remote = _tmp.filePath(QStringLiteral("remote"));
        _log = _tmp.filePath(QStringLiteral("log.txt"));
        qputenv("FAKE_REMOTE", _remote.toUtf8());
        qputenv("FAKE_LOG", _log.toUtf8());
    }

    void init()
    {
        QDir(_remote).removeRecursively();
        QVERIFY(writeFile(remote(QStringLiteral("/home/ana/index.html")), "<h1>Hola</h1>\n"));
        QVERIFY(writeFile(remote(QStringLiteral("/home/ana/con espacio.txt")), "a\n"));
        QVERIFY(QDir().mkpath(remote(QStringLiteral("/home/ana/www"))));
        QVERIFY(writeFile(remote(QStringLiteral("/home/ana/www/style.css")), "body {}\n"));
        QFile::remove(_log);
        qunsetenv("FAKE_PASSWORD");
        qunsetenv("FAKE_UNKNOWN_HOST");
        qunsetenv("FAKE_DOS");
        qunsetenv("FAKE_DOTS");
    }

    // --- Perfiles, listados y rutas --------------------------------------------------------

    void protocols()
    {
        for (const Protocol p : {Protocol::Sftp, Protocol::Ftp, Protocol::FtpExplicitTls, Protocol::FtpImplicitTls})
            QCOMPARE(protocolFromKey(protocolKey(p)), std::optional<Protocol>(p));
        QVERIFY(!protocolFromKey(QStringLiteral("gopher")));
        QCOMPARE(defaultPort(Protocol::Sftp), 22);
        QCOMPARE(defaultPort(Protocol::Ftp), 21);
        QCOMPARE(defaultPort(Protocol::FtpExplicitTls), 21);
        QCOMPARE(defaultPort(Protocol::FtpImplicitTls), 990);
        Profile p = sftpProfile();
        QCOMPARE(p.effectivePort(), 22);
        p.port = 2222;
        QCOMPARE(p.effectivePort(), 2222);
    }

    void profilesRoundTrip()
    {
        Profile a = sftpProfile();
        a.name = QStringLiteral("Mi servidor/producción = 1");
        a.port = 2222;
        a.initialDir = QStringLiteral("/var/www");
        a.keyFile = QStringLiteral("~/.ssh/id_ed25519");
        Profile b = ftpProfile();
        b.protocol = Protocol::FtpExplicitTls;
        b.passive = false;
        b.verifyCertificate = false;
        const QString ini = _tmp.filePath(QStringLiteral("profiles.ini"));
        QVERIFY(saveProfiles(ini, {a, b}));
        const std::vector<Profile> loaded = loadProfiles(ini);
        QCOMPARE(loaded.size(), size_t{2});
        QVERIFY(loaded[0] == a);
        QVERIFY(loaded[1] == b);
        QVERIFY(!readFile(ini).contains("password"));
        QVERIFY(loadProfiles(_tmp.filePath(QStringLiteral("no-existe.ini"))).empty());
    }

    void unixListing()
    {
        const QString text = QStringLiteral(
            "sftp> ls -la\n"
            "total 24\n"
            "drwxr-xr-x    5 ana      ana          4096 Nov 13 10:00 .\n"
            "drwxr-xr-x    3 root     root         4096 Jan  2  2025 ..\n"
            "-rw-r--r--    1 ana      ana           123 Nov 13 10:00 zeta.txt\n"
            "-rw-r--r--    1 ana      ana             7 Nov 13 10:00 con  dos espacios.txt\n"
            "drwxr-xr-x    2 ana      ana          4096 Nov 13 10:00 www\n"
            "lrwxrwxrwx    1 ana      ana             9 Nov 13 10:00 enlace -> www/index\n"
            "-rw-r--r-- 1 ftp 512 Mar  3 2024 sin-grupo.bin\n"
            "-rw-r--r--+ 1 ana staff 1 2026-11-13 10:00 acl.txt\n"
            "-rw-r--r--    ? ana      ana             5 Nov 13 08:32 bajo-sftp.txt\n" // el sftp de OpenSSH no da los enlaces
            "Ábrete sésamo\n");
        std::vector<Entry> entries = parseListing(text);
        QCOMPARE(names(entries), QStringList({QStringLiteral("www/"), QStringLiteral("acl.txt"), QStringLiteral("bajo-sftp.txt"),
                                              QStringLiteral("con  dos espacios.txt"), QStringLiteral("enlace"), QStringLiteral("sin-grupo.bin"),
                                              QStringLiteral("zeta.txt")}));
        entries.erase(entries.begin() + 2); // bajo-sftp.txt: las posiciones de abajo, sin él
        QCOMPARE(entries[5].size, qint64{123});
        QCOMPARE(entries[5].modified, QStringLiteral("Nov 13 10:00"));
        QCOMPARE(entries[5].permissions, QStringLiteral("-rw-r--r--"));
        QVERIFY(entries[3].link);
        QCOMPARE(entries[3].linkTarget, QStringLiteral("www/index"));
        QCOMPARE(entries[4].size, qint64{512});
        QCOMPARE(entries[4].modified, QStringLiteral("Mar  3 2024"));
        QCOMPARE(entries[1].modified, QStringLiteral("2026-11-13 10:00"));
    }

    void dosListing()
    {
        const std::vector<Entry> entries = parseListing(QStringLiteral(
            "11-13-26  10:00AM       <DIR>          Carpeta Uno\r\n"
            "01-02-2024  09:15PM                 1234 informe.pdf\r\n"));
        QCOMPARE(names(entries), QStringList({QStringLiteral("Carpeta Uno/"), QStringLiteral("informe.pdf")}));
        QCOMPARE(entries[1].size, qint64{1234});
        QCOMPARE(entries[1].modified, QStringLiteral("01-02-2024 09:15PM"));
    }

    void workingDirectory()
    {
        QCOMPARE(parseWorkingDirectory(QStringLiteral("sftp> pwd\nRemote working directory: /home/ana mía\n")),
                 QStringLiteral("/home/ana mía"));
        QCOMPARE(parseWorkingDirectory(QStringLiteral("nada")), QString());
    }

    void paths()
    {
        QCOMPARE(joinPath(QString(), QStringLiteral("a")), QStringLiteral("a"));
        QCOMPARE(joinPath(QStringLiteral("/"), QStringLiteral("a")), QStringLiteral("/a"));
        QCOMPARE(joinPath(QStringLiteral("/x/"), QStringLiteral("a")), QStringLiteral("/x/a"));
        QCOMPARE(joinPath(QStringLiteral("x"), QStringLiteral("a")), QStringLiteral("x/a"));
        QCOMPARE(parentPath(QStringLiteral("/x/a")), QStringLiteral("/x"));
        QCOMPARE(parentPath(QStringLiteral("/a")), QStringLiteral("/"));
        QCOMPARE(parentPath(QStringLiteral("a")), QString());
        QCOMPARE(parentPath(QStringLiteral("a/b")), QStringLiteral("a"));
        QCOMPARE(fileName(QStringLiteral("/x/a.txt")), QStringLiteral("a.txt"));
        QCOMPARE(fileName(QStringLiteral("a.txt")), QStringLiteral("a.txt"));

        const QString root = QStringLiteral("/cache/ftp");
        QCOMPARE(cachePath(root, QStringLiteral("Web"), QStringLiteral("/var/www/index.html")),
                 QStringLiteral("/cache/ftp/Web/var/www/index.html"));
        QCOMPARE(cachePath(root, QStringLiteral("Web"), QStringLiteral("www/a.css")), QStringLiteral("/cache/ftp/Web/~/www/a.css"));
        // Ni el perfil ni la ruta salen de la raíz
        QCOMPARE(cachePath(root, QStringLiteral("../x"), QStringLiteral("/../../etc/passwd")),
                 QStringLiteral("/cache/ftp/.._x/etc/passwd"));
        QCOMPARE(cachePath(root, QStringLiteral("a/b"), QStringLiteral("x/../../y")), QStringLiteral("/cache/ftp/a_b/~/x/y"));
    }

    void quoting()
    {
        // Probados con el sftp de OpenSSH 10
        QCOMPARE(sftpQuote(QStringLiteral("a \"b\" *?[1] 'c'.txt")), QStringLiteral("\"a \\\"b\\\" *?[1] 'c'.txt\""));
        QCOMPARE(sftpEscape(QStringLiteral("/a b/\"q\" 'r' #x*?[1]\\ñ.txt")),
                 QStringLiteral("/a\\ b/\\\"q\\\"\\ \\'r\\'\\ \\#x\\*\\?\\[1\\]\\\\ñ.txt"));
        QCOMPARE(curlQuote(QStringLiteral("p\"w\\x\ny")), QStringLiteral("\"p\\\"w\\\\x\\ny\""));
    }

    void urls()
    {
        Profile p = ftpProfile();
        QCOMPARE(ftpUrl(p, QString(), true), QStringLiteral("ftp://server:21/"));
        QCOMPARE(ftpUrl(p, QStringLiteral("www/mi página.html"), false), QStringLiteral("ftp://server:21/www/mi%20p%C3%A1gina.html"));
        QCOMPARE(ftpUrl(p, QStringLiteral("/var/www"), true), QStringLiteral("ftp://server:21/%2Fvar/www/"));
        QCOMPARE(ftpUrl(p, QStringLiteral("a#b?c"), false), QStringLiteral("ftp://server:21/a%23b%3Fc"));
        p.protocol = Protocol::FtpImplicitTls;
        QCOMPARE(ftpUrl(p, QString(), true), QStringLiteral("ftps://server:990/"));
        p.host = QStringLiteral("::1");
        QCOMPARE(ftpUrl(p, QString(), true), QStringLiteral("ftps://[::1]:990/"));
    }

    void sftpInvocations()
    {
        Profile p = sftpProfile();
        p.port = 2222;
        p.keyFile = QStringLiteral("/home/ana/.ssh/llave");
        const SshContext ssh{QStringLiteral("/run/x/cm"), QStringLiteral("/run/x/askpass")};

        const Invocation connect = sshConnect(p, ssh, QStringLiteral("s3cret"), false);
        QCOMPARE(connect.program, QStringLiteral("ssh"));
        for (const char* arg : {"-M", "-f", "-N", "StrictHostKeyChecking=yes", "ControlPersist=30m", "/run/x/cm", "2222",
                                "/home/ana/.ssh/llave", "ana@server"})
            QVERIFY2(connect.arguments.contains(QLatin1String(arg)), arg);
        QVERIFY(!connect.arguments.join(QLatin1Char(' ')).contains(QLatin1String("s3cret")));
        QVERIFY(connect.environment.contains({QStringLiteral("SSH_ASKPASS"), QStringLiteral("/run/x/askpass")}));
        QVERIFY(connect.environment.contains({QStringLiteral("SSH_ASKPASS_REQUIRE"), QStringLiteral("force")}));
        QVERIFY(connect.environment.contains({QLatin1String(kSecretVariable), QStringLiteral("s3cret")}));
        QVERIFY(sshConnect(p, ssh, QString(), true).arguments.contains(QLatin1String("StrictHostKeyChecking=accept-new")));

        Request list{Request::List, QStringLiteral("/var/www"), {}, {}};
        const Invocation ls = requestInvocation(p, ssh, QStringLiteral("s3cret"), list);
        QCOMPARE(ls.program, QStringLiteral("sftp"));
        for (const char* arg : {"-b", "-", "ControlPath=/run/x/cm", "ControlMaster=no", "BatchMode=yes", "2222", "ana@server"})
            QVERIFY2(ls.arguments.contains(QLatin1String(arg)), arg);
        QCOMPARE(QString::fromUtf8(ls.input), QStringLiteral("cd \"/var/www\"\npwd\nls -la\n"));
        QVERIFY(!ls.arguments.join(QLatin1Char(' ')).contains(QLatin1String("s3cret")));
        QVERIFY(!ls.input.contains("s3cret"));
        // La carpeta de inicio: sin cd
        QCOMPARE(QString::fromUtf8(requestInvocation(p, ssh, {}, Request{Request::List, {}, {}, {}}).input), QStringLiteral("pwd\nls -la\n"));

        auto input = [&](const Request& r) { return QString::fromUtf8(requestInvocation(p, ssh, {}, r).input); };
        QCOMPARE(input({Request::Download, QStringLiteral("/a b.txt"), QStringLiteral("/tmp/a b.txt"), {}}),
                 QStringLiteral("get /a\\ b.txt \"/tmp/a b.txt\"\n"));
        QCOMPARE(input({Request::Upload, QStringLiteral("/a.txt"), QStringLiteral("/tmp/a.txt"), {}}), QStringLiteral("put /tmp/a.txt \"/a.txt\"\n"));
        QCOMPARE(input({Request::MakeDir, QStringLiteral("/n"), {}, {}}), QStringLiteral("mkdir \"/n\"\n"));
        QCOMPARE(input({Request::RemoveFile, QStringLiteral("/n.txt"), {}, {}}), QStringLiteral("rm /n.txt\n"));
        QCOMPARE(input({Request::RemoveDir, QStringLiteral("/n"), {}, {}}), QStringLiteral("rmdir \"/n\"\n"));
        QCOMPARE(input({Request::Rename, QStringLiteral("/a"), {}, QStringLiteral("/b")}), QStringLiteral("rename \"/a\" \"/b\"\n"));

        const Invocation exit = sshDisconnect(p, ssh);
        QCOMPARE(exit.program, QStringLiteral("ssh"));
        QVERIFY(exit.arguments.contains(QLatin1String("-O")));
        QVERIFY(exit.arguments.contains(QLatin1String("exit")));
        QCOMPARE(sshKeyScan(p).program, QStringLiteral("ssh-keyscan"));
        QVERIFY(sshKeyScan(p).arguments.contains(QLatin1String("2222")));
        QCOMPARE(sshFingerprint("x").input, QByteArray("x"));
    }

    void curlInvocations()
    {
        Profile p = ftpProfile();
        const QString password = QStringLiteral("p\"w");
        const SshContext none;
        auto config = [&](const Request& r) {
            const Invocation inv = requestInvocation(p, none, password, r);
            [&] {
                QCOMPARE(inv.program, QStringLiteral("curl"));
                QCOMPARE(inv.arguments, QStringList({QStringLiteral("--silent"), QStringLiteral("--show-error"),
                                                     QStringLiteral("--config"), QStringLiteral("-")}));
            }();
            return QString::fromUtf8(inv.input);
        };
        QString c = config({Request::List, QStringLiteral("www"), {}, {}});
        QVERIFY(c.contains(QLatin1String("url = \"ftp://server:21/www/\"\n")));
        QVERIFY(c.contains(QLatin1String("user = \"ana:p\\\"w\"\n")));
        QVERIFY(c.contains(QLatin1String("ftp-pasv\n")));
        QVERIFY(!c.contains(QLatin1String("ssl-reqd")));
        c = config({Request::Download, QStringLiteral("a.txt"), QStringLiteral("/tmp/a.txt"), {}});
        QVERIFY(c.contains(QLatin1String("url = \"ftp://server:21/a.txt\"\n")));
        QVERIFY(c.contains(QLatin1String("output = \"/tmp/a.txt\"\n")));
        c = config({Request::Upload, QStringLiteral("www/a.txt"), QStringLiteral("/tmp/a.txt"), {}});
        QVERIFY(c.contains(QLatin1String("upload-file = \"/tmp/a.txt\"\n")));
        QVERIFY(c.contains(QLatin1String("url = \"ftp://server:21/www/a.txt\"\n")));
        c = config({Request::MakeDir, QStringLiteral("www/nueva"), {}, {}});
        QVERIFY(c.contains(QLatin1String("quote = \"MKD www/nueva\"\n")));
        QVERIFY(c.contains(QLatin1String("output = \"/dev/null\"\n")));
        c = config({Request::Rename, QStringLiteral("a"), {}, QStringLiteral("b")});
        QVERIFY(c.indexOf(QLatin1String("quote = \"RNFR a\"")) < c.indexOf(QLatin1String("quote = \"RNTO b\"")));
        QVERIFY(config({Request::RemoveFile, QStringLiteral("a"), {}, {}}).contains(QLatin1String("quote = \"DELE a\"")));
        QVERIFY(config({Request::RemoveDir, QStringLiteral("a"), {}, {}}).contains(QLatin1String("quote = \"RMD a\"")));

        p.protocol = Protocol::FtpExplicitTls;
        p.passive = false;
        p.verifyCertificate = false;
        c = config({Request::List, {}, {}, {}});
        QVERIFY(c.contains(QLatin1String("ssl-reqd\n")));
        QVERIFY(c.contains(QLatin1String("insecure\n")));
        QVERIFY(c.contains(QLatin1String("ftp-port = \"-\"\n")));
        // Sin usuario: anónimo
        p.user.clear();
        QVERIFY(!config({Request::List, {}, {}, {}}).contains(QLatin1String("user =")));
    }

    void errors()
    {
        QCOMPARE(classifyError(Protocol::Sftp, 255, QStringLiteral("ana@server: Permission denied (publickey,password).")),
                 ErrorKind::AuthenticationFailed);
        QCOMPARE(classifyError(Protocol::Sftp, 255, QStringLiteral("No ED25519 host key is known for server and you have requested strict checking.\nHost key verification failed.")),
                 ErrorKind::HostKeyUnknown);
        QCOMPARE(classifyError(Protocol::Sftp, 255, QStringLiteral("@@@ WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED! @@@\nHost key verification failed.")),
                 ErrorKind::HostKeyChanged);
        QCOMPARE(classifyError(Protocol::Sftp, 255, QStringLiteral("ssh: connect to host server port 22: Connection refused")),
                 ErrorKind::CannotConnect);
        QCOMPARE(classifyError(Protocol::Sftp, 1, QStringLiteral("File \"/x\" not found.")), ErrorKind::NotFound);
        QCOMPARE(classifyError(Protocol::Ftp, 67, QStringLiteral("curl: (67) Access denied: 530")), ErrorKind::AuthenticationFailed);
        QCOMPARE(classifyError(Protocol::FtpExplicitTls, 60, QStringLiteral("curl: (60) SSL certificate problem")), ErrorKind::Certificate);
        QCOMPARE(classifyError(Protocol::Ftp, 7, QStringLiteral("curl: (7) Failed to connect")), ErrorKind::CannotConnect);
        QCOMPARE(classifyError(Protocol::Ftp, 78, QStringLiteral("curl: (78) RETR response: 550")), ErrorKind::NotFound);
        QCOMPARE(classifyError(Protocol::Ftp, 0, QString()), ErrorKind::None);
        QCOMPARE(classifyError(Protocol::Ftp, 3, QStringLiteral("curl: (3) URL malformed")), ErrorKind::Other);
        QVERIFY(askPassScript().startsWith("#!/bin/sh"));
        QVERIFY(askPassScript().contains(kSecretVariable));
    }

    // Los .qm del repositorio son los que se compilan de los .ts (la receta del catálogo usa
    // los del repositorio: el SDK no trae lrelease)
    void translationsAreCurrent()
    {
        const QByteArray built = readFile(QStringLiteral(EDITA_PLUGIN_BUILD_DIR "/EditaFTP/translations/editaftp_es.qm"));
        QVERIFY(!built.isEmpty());
        QVERIFY2(built == readFile(QStringLiteral(EDITA_SOURCE_DIR "/plugins/EditaFTP/translations/editaftp_es.qm")),
                 "plugins/EditaFTP/translations/editaftp_es.qm no está al día: cópialo de la compilación");
    }

    // --- La sesión, con los programas falsos -----------------------------------------------

    void sftpSession()
    {
        qputenv("FAKE_PASSWORD", "s3cret");
        qputenv("FAKE_DOTS", "1");
        ftp::Session s;
        QStringList log;
        s.setLogger([&](const QString& line) { log << line; });
        s.setProfile(sftpProfile());
        s.setPassword(QStringLiteral("s3cret"));
        Result r = connect(s);
        QVERIFY2(r.ok, qPrintable(r.message));
        QVERIFY(s.isConnected());
        QCOMPARE(r.directory, QStringLiteral("/home/ana"));
        QCOMPARE(names(r.entries), QStringList({QStringLiteral("www/"), QStringLiteral("con espacio.txt"), QStringLiteral("index.html")}));

        r = run(s, {Request::List, QStringLiteral("/home/ana/www"), {}, {}});
        QVERIFY2(r.ok, qPrintable(r.message));
        QCOMPARE(names(r.entries), QStringList({QStringLiteral("style.css")}));

        const QString local = _tmp.filePath(QStringLiteral("down/con espacio.txt"));
        QDir().mkpath(QFileInfo(local).absolutePath());
        r = run(s, {Request::Download, QStringLiteral("/home/ana/con espacio.txt"), local, {}});
        QVERIFY2(r.ok, qPrintable(r.message));
        QCOMPARE(readFile(local), QByteArray("a\n"));
        QVERIFY(writeFile(local, "cambiado\n"));
        r = run(s, {Request::Upload, QStringLiteral("/home/ana/con espacio.txt"), local, {}});
        QVERIFY2(r.ok, qPrintable(r.message));
        QCOMPARE(readFile(remote(QStringLiteral("/home/ana/con espacio.txt"))), QByteArray("cambiado\n"));

        QVERIFY(run(s, {Request::MakeDir, QStringLiteral("/home/ana/nueva"), {}, {}}).ok);
        QVERIFY(QFileInfo(remote(QStringLiteral("/home/ana/nueva"))).isDir());
        QVERIFY(run(s, {Request::Rename, QStringLiteral("/home/ana/nueva"), {}, QStringLiteral("/home/ana/otra")}).ok);
        QVERIFY(run(s, {Request::RemoveDir, QStringLiteral("/home/ana/otra"), {}, {}}).ok);
        QVERIFY(!QFileInfo::exists(remote(QStringLiteral("/home/ana/otra"))));
        QVERIFY(run(s, {Request::RemoveFile, QStringLiteral("/home/ana/index.html"), {}, {}}).ok);
        QVERIFY(!QFileInfo::exists(remote(QStringLiteral("/home/ana/index.html"))));
        r = run(s, {Request::RemoveFile, QStringLiteral("/home/ana/no-existe"), {}, {}});
        QVERIFY(!r.ok);
        QCOMPARE(r.error, ErrorKind::NotFound);

        // Varias peticiones seguidas: se ejecutan en orden, de una en una
        std::vector<int> order;
        for (int i = 0; i < 3; ++i)
            s.run({Request::List, QStringLiteral("/home/ana"), {}, {}}, [&order, i](const Result&) { order.push_back(i); });
        QTRY_COMPARE(order.size(), size_t{3});
        QCOMPARE(order, std::vector<int>({0, 1, 2}));

        // La contraseña no aparece en ningún argumento ni en el registro
        QVERIFY(!readFile(_log).contains("s3cret"));
        QVERIFY(!log.join(QLatin1Char('\n')).contains(QLatin1String("s3cret")));
        QVERIFY(!log.isEmpty());

        s.disconnectFromHost();
        QVERIFY(!s.isConnected());
        QVERIFY(readFile(_log).contains("-O exit"));
    }

    void sftpWrongPassword()
    {
        qputenv("FAKE_PASSWORD", "s3cret");
        ftp::Session s;
        s.setProfile(sftpProfile());
        s.setPassword(QStringLiteral("otra"));
        Result r = connect(s);
        QVERIFY(!r.ok);
        QCOMPARE(r.error, ErrorKind::AuthenticationFailed);
        QVERIFY(!s.isConnected());
        // Sin contraseña (solo claves o el agente) también
        s.setPassword(QString());
        r = connect(s);
        QCOMPARE(r.error, ErrorKind::AuthenticationFailed);
    }

    void sftpUnknownHost()
    {
        qputenv("FAKE_UNKNOWN_HOST", "1");
        ftp::Session s;
        s.setProfile(sftpProfile());
        Result r = connect(s);
        QCOMPARE(r.error, ErrorKind::HostKeyUnknown);
        QString fingerprint;
        s.hostFingerprint([&](const QString& f) { fingerprint = f; });
        QTRY_COMPARE(fingerprint, QStringLiteral("SHA256:FakeFingerprintForTests (ED25519)"));
        r = connect(s, true);
        QVERIFY2(r.ok, qPrintable(r.message));
    }

    void sftpReconnects()
    {
        qputenv("FAKE_PASSWORD", "s3cret");
        ftp::Session s;
        QStringList log;
        s.setLogger([&](const QString& line) { log << line; });
        s.setProfile(sftpProfile());
        s.setPassword(QStringLiteral("s3cret"));
        QVERIFY(connect(s).ok);
        // La conexión maestra se cerró (ControlPersist, o el servidor la cortó)
        const QString control = QString::fromUtf8(readFile(_log)).section(QStringLiteral(" -S "), 1).section(QLatin1Char(' '), 0, 0);
        QVERIFY2(QFile::remove(control), qPrintable(control));
        const Result r = run(s, {Request::List, QStringLiteral("/home/ana/www"), {}, {}});
        QVERIFY2(r.ok, qPrintable(r.message));
        QCOMPARE(names(r.entries), QStringList({QStringLiteral("style.css")}));
    }

    void ftpSession()
    {
        qputenv("FAKE_PASSWORD", "p\"w ñ");
        ftp::Session s;
        s.setProfile(ftpProfile());
        s.setPassword(QStringLiteral("p\"w ñ"));
        Result r = connect(s);
        QVERIFY2(r.ok, qPrintable(r.message));
        QCOMPARE(r.directory, QString());
        QCOMPARE(names(r.entries), QStringList({QStringLiteral("www/"), QStringLiteral("con espacio.txt"), QStringLiteral("index.html")}));
        QVERIFY(run(s, {Request::List, QStringLiteral("/home/ana/www"), {}, {}}).ok); // absoluta

        const QString local = _tmp.filePath(QStringLiteral("down/style.css"));
        QDir().mkpath(QFileInfo(local).absolutePath());
        r = run(s, {Request::Download, QStringLiteral("www/style.css"), local, {}});
        QVERIFY2(r.ok, qPrintable(r.message));
        QCOMPARE(readFile(local), QByteArray("body {}\n"));
        QVERIFY(writeFile(local, "p {}\n"));
        QVERIFY(run(s, {Request::Upload, QStringLiteral("www/style.css"), local, {}}).ok);
        QCOMPARE(readFile(remote(QStringLiteral("/home/ana/www/style.css"))), QByteArray("p {}\n"));
        QVERIFY(run(s, {Request::MakeDir, QStringLiteral("www/img"), {}, {}}).ok);
        QVERIFY(run(s, {Request::Rename, QStringLiteral("www/img"), {}, QStringLiteral("www/imagenes")}).ok);
        QVERIFY(QFileInfo(remote(QStringLiteral("/home/ana/www/imagenes"))).isDir());
        QVERIFY(run(s, {Request::RemoveDir, QStringLiteral("www/imagenes"), {}, {}}).ok);
        QVERIFY(run(s, {Request::RemoveFile, QStringLiteral("www/style.css"), {}, {}}).ok);
        QVERIFY(!QFileInfo::exists(remote(QStringLiteral("/home/ana/www/style.css"))));
        QVERIFY(!readFile(_log).contains("p\"w"));

        qputenv("FAKE_DOS", "1");
        r = run(s, {Request::List, QString(), {}, {}});
        QVERIFY2(r.ok, qPrintable(r.message));
        QCOMPARE(names(r.entries), QStringList({QStringLiteral("www/"), QStringLiteral("con espacio.txt"), QStringLiteral("index.html")}));

        s.setPassword(QStringLiteral("mala"));
        r = run(s, {Request::List, QString(), {}, {}});
        QCOMPARE(r.error, ErrorKind::AuthenticationFailed);
    }

    // --- El plugin en Edita ----------------------------------------------------------------

    void pluginOpensEditsAndUploads()
    {
        QVERIFY(saveProfiles(QDir(qEnvironmentVariable("XDG_CONFIG_HOME")).filePath(QStringLiteral("edita/plugins/config/EditaFTP.ini")),
                             {sftpProfile()}));
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        w.notifyPluginsReady();
        const PluginManager::Plugin* plugin = nullptr;
        for (const auto& p : w.plugins()->plugins()) {
            if (p->name == QLatin1String("FTP"))
                plugin = p.get();
        }
        QVERIFY(plugin);
        QVERIFY(plugin->commands.size() >= 5);
        QVERIFY(w.plugins()->runCommand(plugin->commands.at(0).cmdId)); // el panel
        auto* profiles = w.findChild<QComboBox*>(QStringLiteral("ftpProfile"));
        QVERIFY(profiles);
        QCOMPARE(profiles->count(), 1);
        QCOMPARE(profiles->currentText(), QStringLiteral("Servidor"));
        auto* tree = w.findChild<QTreeWidget*>(QStringLiteral("ftpTree"));
        auto* connectButton = w.findChild<QToolButton*>(QStringLiteral("ftpConnect"));
        QVERIFY(tree && connectButton);
        connectButton->click();
        // La raíz es la carpeta de inicio, con su contenido
        QTRY_COMPARE(tree->topLevelItemCount(), 1);
        QTreeWidgetItem* root = tree->topLevelItem(0);
        QCOMPARE(root->text(0), QStringLiteral("/home/ana"));
        QTRY_COMPARE(root->childCount(), 3);
        QCOMPARE(root->child(0)->text(0), QStringLiteral("www"));
        // Abrir una carpeta la lista
        root->child(0)->setExpanded(true);
        QTRY_COMPARE(root->child(0)->childCount(), 1);
        QCOMPARE(root->child(0)->child(0)->text(0), QStringLiteral("style.css"));

        // Doble clic en un archivo: se descarga y se abre en Edita
        QTreeWidgetItem* index = root->child(2);
        QCOMPARE(index->text(0), QStringLiteral("index.html"));
        emit tree->itemDoubleClicked(index, 0);
        const QString cached = QDir(qEnvironmentVariable("XDG_CACHE_HOME")).filePath(QStringLiteral("edita/ftp/Servidor/home/ana/index.html"));
        QTRY_VERIFY(w.documents().findByPath(cached) != kInvalidBuffer);
        QCOMPARE(QString::fromStdString(w.editor()->text()), QStringLiteral("<h1>Hola</h1>\n"));

        // Al guardar, se sube
        w.editor()->setText("<h1>Adiós</h1>\n");
        QVERIFY(w.saveBuffer(w.currentBuffer()));
        QTRY_COMPARE(readFile(remote(QStringLiteral("/home/ana/index.html"))), QByteArray("<h1>Adiós</h1>\n"));
        auto* log = w.findChild<QPlainTextEdit*>(QStringLiteral("ftpLog"));
        QVERIFY(log);
        QTRY_VERIFY(log->toPlainText().contains(QLatin1String("index.html")));

        connectButton->click(); // desconectar
        QTRY_COMPARE(tree->topLevelItemCount(), 0);
    }
};

QTEST_MAIN(TestEditaFtp)
#include "tst_editaftp.moc"
