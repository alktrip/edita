// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Un servidor de lenguaje (Language Server Protocol 3.17) por su entrada y salida
// estándar: lo lanza, hace el saludo (initialize/initialized), envía peticiones y
// notificaciones (las de antes del saludo esperan) y contesta lo que pide el servidor.

#pragma once

#include "lsp/JsonRpc.h"
#include "lsp/LspServers.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QProcess>

#include <functional>
#include <map>

namespace edita::lsp {

class LspClient : public QObject
{
    Q_OBJECT

public:
    enum class State { NotStarted, Starting, Running, Failed, Stopped };
    // El resultado (o el error, con «code» y «message») de una petición
    using Callback = std::function<void(const QJsonValue& result, const QJsonObject& error)>;

    LspClient(const QString& program, const QStringList& arguments, const QString& rootPath, QObject* parent = nullptr);
    ~LspClient() override;

    void start();
    // shutdown + exit, y si no termina a tiempo, se mata
    void stop(int timeoutMs = 1000);

    State state() const { return _state; }
    QString name() const;
    QString rootPath() const { return _rootPath; }
    QString program() const { return _program; }
    PositionEncoding positionEncoding() const { return _encoding; }
    const QJsonObject& capabilities() const { return _capabilities; }
    bool hasCapability(const QString& provider) const;
    QStringList completionTriggers() const;
    QStringList signatureTriggers() const;
    // TextDocumentSyncKind: 0 nada, 1 todo el texto, 2 por cambios
    int syncKind() const;
    bool wantsSave() const;
    QString log() const { return _log.join(QLatin1Char('\n')); }

    int request(const QString& method, const QJsonValue& params, Callback callback);
    void cancel(int id);
    void notify(const QString& method, const QJsonValue& params);

signals:
    void stateChanged(edita::lsp::LspClient::State state);
    void diagnosticsPublished(const QString& uri, const QJsonArray& diagnostics);
    // workspace/applyEdit: quien lo atienda contesta con reply(true) si lo aplicó
    void applyEditRequested(const QJsonObject& edit, const std::function<void(bool)>& reply);
    void serverMessage(int type, const QString& message);

private:
    void setState(State state);
    void send(const QJsonObject& message);
    void readOutput();
    void handleMessage(const QJsonObject& message);
    void handleServerRequest(const QJsonValue& id, const QString& method, const QJsonValue& params);
    void reply(const QJsonValue& id, const QJsonValue& result);
    void replyError(const QJsonValue& id, int code, const QString& message);
    void appendLog(const QString& line);
    void initialized(const QJsonObject& result);

    QProcess _process;
    QString _program;
    QStringList _arguments;
    QString _rootPath;
    State _state = State::NotStarted;
    JsonRpcStream _stream;
    int _nextId = 1;
    std::map<int, Callback> _pending;
    std::vector<QJsonObject> _queued; // antes de «initialized»
    QJsonObject _capabilities;
    PositionEncoding _encoding = PositionEncoding::Utf16;
    QStringList _log;
};

} // namespace edita::lsp
