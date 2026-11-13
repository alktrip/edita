// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "FtpSession.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

namespace edita::ftp {

namespace {

QString describe(const Request& r)
{
    switch (r.kind) {
    case Request::List:
        return QCoreApplication::translate("EditaFTP", "List %1").arg(r.remote.isEmpty() ? QStringLiteral("~") : r.remote);
    case Request::Download:
        return QCoreApplication::translate("EditaFTP", "Download %1").arg(r.remote);
    case Request::Upload:
        return QCoreApplication::translate("EditaFTP", "Upload %1").arg(r.remote);
    case Request::MakeDir:
        return QCoreApplication::translate("EditaFTP", "Create folder %1").arg(r.remote);
    case Request::RemoveFile:
    case Request::RemoveDir:
        return QCoreApplication::translate("EditaFTP", "Delete %1").arg(r.remote);
    case Request::Rename:
        return QCoreApplication::translate("EditaFTP", "Rename %1 to %2").arg(r.remote, r.target);
    }
    return QString();
}

QString firstLines(const QString& text)
{
    QStringList lines;
    for (const QString& line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        // Lo que no aporta nada: el eco de sftp y los avisos de ssh sobre known_hosts
        if (line.startsWith(QLatin1String("sftp> ")) || line.startsWith(QLatin1String("Warning: Permanently added")))
            continue;
        lines << line.trimmed();
        if (lines.size() == 3)
            break;
    }
    return lines.join(QLatin1Char(' '));
}

} // namespace

Session::Session()
    : _alive(std::make_shared<bool>(true))
{
}

Session::~Session()
{
    disconnectFromHost();
}

void Session::setProfile(const Profile& profile)
{
    if (!(profile == _profile))
        disconnectFromHost();
    _profile = profile;
}

void Session::log(const QString& text) const
{
    if (_logger)
        _logger(text);
}

SshContext Session::sshContext()
{
    if (!_dir) {
        _dir = std::make_unique<QTemporaryDir>(QDir(QDir::tempPath()).filePath(QStringLiteral("edita-ftp-XXXXXX")));
        QFile script(_dir->filePath(QStringLiteral("askpass")));
        if (script.open(QIODevice::WriteOnly))
            script.write(askPassScript());
        script.close();
        script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    }
    // Sin %C: el plugin comprueba si el socket sigue ahí
    return {_dir->filePath(QStringLiteral("cm")), _dir->filePath(QStringLiteral("askpass"))};
}

void Session::enqueue(Job job)
{
    _queue.push_back(std::move(job));
    if (!_running)
        startNext();
}

void Session::startNext()
{
    if (_running || _queue.empty())
        return;
    Job job = std::move(_queue.front());
    _queue.pop_front();
    auto* process = new QProcess;
    _running = process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const auto& [name, value] : job.invocation.environment)
        env.insert(name, value);
    process->setProcessEnvironment(env);
    // ssh -f se queda en segundo plano con la salida de error abierta: a un archivo, para
    // no esperar a que se cierre
    const bool detaches = job.invocation.program == QLatin1String("ssh") && job.invocation.arguments.contains(QLatin1String("-M"));
    QString errorFile;
    if (detaches) {
        errorFile = sshContext().controlPath + QStringLiteral(".err");
        process->setStandardOutputFile(QProcess::nullDevice());
        process->setStandardErrorFile(errorFile);
    }
    auto finish = [this, alive = std::weak_ptr<bool>(_alive), process, errorFile,
                   done = std::move(job.finished)](int exitCode, const QString& startError) {
        if (alive.expired() || _running != process)
            return;
        QString out = errorFile.isEmpty() ? QString::fromUtf8(process->readAllStandardOutput()) : QString();
        QString err = !startError.isEmpty() || !errorFile.isEmpty() ? startError : QString::fromUtf8(process->readAllStandardError());
        if (!errorFile.isEmpty()) {
            QFile f(errorFile);
            if (f.open(QIODevice::ReadOnly))
                err += QString::fromUtf8(f.readAll());
            f.close();
            f.remove();
        }
        _running = nullptr;
        process->deleteLater();
        done(exitCode, out, err);
        startNext();
    };
    QObject::connect(process, &QProcess::finished, process,
                     [finish, process](int code, QProcess::ExitStatus status) {
                         finish(status == QProcess::NormalExit ? code : -1,
                                status == QProcess::NormalExit ? QString() : process->errorString());
                     });
    QObject::connect(process, &QProcess::errorOccurred, process, [finish, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            finish(-1, QCoreApplication::translate("EditaFTP", "Cannot run %1: %2").arg(process->program(), process->errorString()));
    });
    process->start(job.invocation.program, job.invocation.arguments);
    if (!job.invocation.input.isEmpty())
        process->write(job.invocation.input);
    process->closeWriteChannel();
}

Result Session::resultFor(int exitCode, const QString& out, const QString& err, const Request* request) const
{
    Result r;
    r.ok = exitCode == 0;
    if (!r.ok) {
        r.error = classifyError(_profile.protocol, exitCode, err);
        r.message = firstLines(err);
        if (r.message.isEmpty())
            r.message = QCoreApplication::translate("EditaFTP", "The command failed (exit code %1)").arg(exitCode);
        return r;
    }
    if (request && request->kind == Request::List) {
        r.entries = parseListing(out);
        r.directory = isFtp(_profile.protocol) ? request->remote : parseWorkingDirectory(out);
        if (r.directory.isEmpty())
            r.directory = request->remote;
    }
    return r;
}

void Session::connectToHost(bool acceptNewHostKey, Callback done)
{
    const Request list{Request::List, _profile.initialDir, {}, {}};
    log(QCoreApplication::translate("EditaFTP", "Connecting to %1 (%2)…").arg(_profile.host, protocolKey(_profile.protocol).toUpper()));
    if (isFtp(_profile.protocol)) {
        enqueue({requestInvocation(_profile, {}, _password, list), [this, list, done](int code, const QString& out, const QString& err) {
                     Result r = resultFor(code, out, err, &list);
                     _connected = r.ok;
                     log(r.ok ? QCoreApplication::translate("EditaFTP", "Connected.") : QStringLiteral("! ") + r.message);
                     done(r);
                 }});
        return;
    }
    const SshContext ssh = sshContext();
    if (QFileInfo::exists(ssh.controlPath)) {
        _connected = true;
        runRequest(list, std::move(done), false);
        return;
    }
    enqueue({sshConnect(_profile, ssh, _password, acceptNewHostKey),
             [this, list, done, ssh](int code, const QString& out, const QString& err) {
                 Result r = resultFor(code, out, err, nullptr);
                 if (r.ok && !QFileInfo::exists(ssh.controlPath)) {
                     r.ok = false;
                     r.error = ErrorKind::Other;
                     r.message = QCoreApplication::translate("EditaFTP", "ssh did not open the shared connection");
                 }
                 if (!r.ok) {
                     _connected = false;
                     log(QStringLiteral("! ") + r.message);
                     done(r);
                     return;
                 }
                 _connected = true;
                 log(QCoreApplication::translate("EditaFTP", "Connected."));
                 runRequest(list, done, false);
             }});
}

void Session::disconnectFromHost()
{
    _queue.clear();
    if (_running) {
        QProcess* p = _running;
        _running = nullptr;
        p->kill();
        p->waitForFinished(1000);
        p->deleteLater();
    }
    if (_dir && QFileInfo::exists(sshContext().controlPath)) {
        const Invocation exit = sshDisconnect(_profile, sshContext());
        QProcess p;
        p.start(exit.program, exit.arguments);
        p.waitForFinished(3000);
        QFile::remove(sshContext().controlPath);
    }
    if (_connected)
        log(QCoreApplication::translate("EditaFTP", "Disconnected."));
    _connected = false;
}

void Session::hostFingerprint(std::function<void(const QString&)> done)
{
    enqueue({sshKeyScan(_profile), [this, done](int, const QString& keys, const QString&) {
                 enqueue({sshFingerprint(keys.toUtf8()), [done](int, const QString& out, const QString&) {
                              // «256 SHA256:… servidor (ED25519)», una línea por tipo de clave
                              QString chosen;
                              for (const QString& line : out.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
                                  const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                                  if (parts.size() < 2)
                                      continue;
                                  const QString text = parts.value(1) + QLatin1Char(' ') + parts.last();
                                  if (chosen.isEmpty() || line.contains(QLatin1String("(ED25519)")))
                                      chosen = text;
                              }
                              done(chosen);
                          }});
             }});
}

void Session::run(const Request& request, Callback done)
{
    runRequest(request, std::move(done), true);
}

void Session::runRequest(const Request& request, Callback done, bool mayReconnect)
{
    if (!isFtp(_profile.protocol)) {
        const SshContext ssh = sshContext();
        if (!_connected || !QFileInfo::exists(ssh.controlPath)) {
            // La conexión maestra se cerró: una vez, se vuelve a abrir con la misma contraseña
            if (!_connected || !mayReconnect) {
                Result r;
                r.error = ErrorKind::ConnectionLost;
                r.message = QCoreApplication::translate("EditaFTP", "Not connected");
                done(r);
                return;
            }
            log(QCoreApplication::translate("EditaFTP", "The connection was closed; reconnecting…"));
            enqueue({sshConnect(_profile, ssh, _password, false), [this, request, done, ssh](int code, const QString& out, const QString& err) {
                         Result r = resultFor(code, out, err, nullptr);
                         if (!r.ok || !QFileInfo::exists(ssh.controlPath)) {
                             _connected = false;
                             r.ok = false;
                             if (r.error == ErrorKind::None)
                                 r.error = ErrorKind::ConnectionLost;
                             log(QStringLiteral("! ") + r.message);
                             done(r);
                             return;
                         }
                         runRequest(request, done, false);
                     }});
            return;
        }
    }
    log(describe(request));
    enqueue({requestInvocation(_profile, sshContext(), _password, request),
             [this, request, done](int code, const QString& out, const QString& err) {
                 const Result r = resultFor(code, out, err, &request);
                 if (!r.ok)
                     log(QStringLiteral("! ") + r.message);
                 done(r);
             }});
}

} // namespace edita::ftp
