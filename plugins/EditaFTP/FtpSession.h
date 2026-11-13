// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Una sesión con un servidor (fase 90): ejecuta las órdenes de FtpCore de una en una, sin
// bloquear la interfaz, y avisa con funciones (el plugin se compila sin moc).

#pragma once

#include "FtpCore.h"

#include <QString>

#include <deque>
#include <functional>
#include <memory>
#include <vector>

class QProcess;
class QTemporaryDir;

namespace edita::ftp {

struct Result
{
    bool ok = false;
    ErrorKind error = ErrorKind::None;
    QString message;          // el error, como lo da el programa
    QString directory;        // List: la carpeta (en SFTP, la ruta absoluta)
    std::vector<Entry> entries;
};

class Session
{
public:
    using Callback = std::function<void(const Result&)>;
    using Logger = std::function<void(const QString&)>;

    Session();
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    void setProfile(const Profile& profile);
    const Profile& profile() const { return _profile; }
    void setPassword(const QString& password) { _password = password; }
    bool hasPassword() const { return !_password.isEmpty(); }
    void setLogger(Logger logger) { _logger = std::move(logger); }

    bool isConnected() const { return _connected; }
    bool isBusy() const { return _running != nullptr || !_queue.empty(); }

    // SFTP: abre la conexión maestra; FTP: lista la carpeta de inicio. En los dos casos, el
    // resultado trae la carpeta de inicio y su contenido.
    void connectToHost(bool acceptNewHostKey, Callback done);
    // Cierra la conexión (en SFTP, la maestra) y descarta lo pendiente
    void disconnectFromHost();
    // La huella de la clave del servidor SFTP («SHA256:… (ED25519)»)
    void hostFingerprint(std::function<void(const QString&)> done);
    // Pone una petición en la cola
    void run(const Request& request, Callback done);

private:
    struct Job
    {
        Invocation invocation;
        std::function<void(int exitCode, const QString& out, const QString& err)> finished;
    };
    void enqueue(Job job);
    void startNext();
    void log(const QString& text) const;
    SshContext sshContext();
    void runRequest(const Request& request, Callback done, bool mayReconnect);
    Result resultFor(int exitCode, const QString& out, const QString& err, const Request* request) const;

    Profile _profile;
    QString _password;
    bool _connected = false;
    Logger _logger;
    std::unique_ptr<QTemporaryDir> _dir; // ControlPath y el guion de SSH_ASKPASS
    std::deque<Job> _queue;
    QProcess* _running = nullptr;
    std::shared_ptr<bool> _alive;
};

} // namespace edita::ftp
