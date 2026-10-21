// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
//
// Edita is derived from Notepad++ (Copyright (C) Don HO and Notepad++ team).
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "ui/SingleInstance.h"

#include "AppInfo.h"

#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>

namespace edita {

namespace {

// Respuesta de la instancia que recibe: la petición llegó entera
const QByteArray kAck = QByteArrayLiteral("ok\n");

} // namespace

QByteArray InstanceRequest::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("files"), QJsonArray::fromStringList(files));
    o.insert(QStringLiteral("line"), line);
    o.insert(QStringLiteral("column"), column);
    return QJsonDocument(o).toJson(QJsonDocument::Compact) + '\n';
}

bool InstanceRequest::fromJson(const QByteArray& json, InstanceRequest& request)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json.trimmed());
    if (!doc.isObject())
        return false;
    const QJsonObject o = doc.object();
    request = InstanceRequest{};
    for (const QJsonValue& file : o.value(QStringLiteral("files")).toArray()) {
        if (file.isString())
            request.files << file.toString();
    }
    request.line = o.value(QStringLiteral("line")).toInt(-1);
    request.column = o.value(QStringLiteral("column")).toInt(-1);
    return true;
}

InstanceServer::InstanceServer(QObject* parent)
    : QObject(parent)
{
}

InstanceServer::~InstanceServer() = default;

QString InstanceServer::socketName(const QString& configDir)
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (dir.isEmpty())
        dir = QDir::tempPath();
    const QByteArray hash = QCryptographicHash::hash(QDir(configDir).absolutePath().toUtf8(), QCryptographicHash::Sha1).toHex().left(12);
    return QDir(dir).filePath(QStringLiteral("%1-%2.sock").arg(QString::fromUtf8(kAppId), QString::fromLatin1(hash)));
}

bool InstanceServer::listen(const QString& name)
{
    if (!_server) {
        _server = new QLocalServer(this);
        _server->setSocketOptions(QLocalServer::UserAccessOption);
        connect(_server, &QLocalServer::newConnection, this, &InstanceServer::accept);
    }
    // Antes de escuchar: Qt sustituye el archivo del socket aunque otra instancia lo
    // esté usando. Si nadie responde, es de una instancia que terminó mal: se quita
    if (isRunning(name))
        return false;
    QLocalServer::removeServer(name);
    return _server->listen(name);
}

bool InstanceServer::isListening() const
{
    return _server && _server->isListening();
}

void InstanceServer::accept()
{
    while (QLocalSocket* socket = _server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
            if (!socket->canReadLine())
                return;
            InstanceRequest request;
            if (InstanceRequest::fromJson(socket->readLine(), request)) {
                socket->write(kAck);
                socket->flush();
                emit requestReceived(request);
            }
            socket->disconnectFromServer();
        });
    }
}

bool InstanceServer::send(const QString& name, const InstanceRequest& request, int timeoutMs)
{
    QLocalSocket socket;
    socket.connectToServer(name);
    if (!socket.waitForConnected(timeoutMs))
        return false;
    socket.write(request.toJson());
    if (!socket.waitForBytesWritten(timeoutMs))
        return false;
    while (!socket.canReadLine()) {
        if (!socket.waitForReadyRead(timeoutMs))
            return false;
    }
    return socket.readLine() == kAck;
}

bool InstanceServer::isRunning(const QString& name, int timeoutMs)
{
    QLocalSocket socket;
    socket.connectToServer(name);
    const bool connected = socket.waitForConnected(timeoutMs);
    socket.abort();
    return connected;
}

} // namespace edita
