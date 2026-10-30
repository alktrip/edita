// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lsp/LspClient.h"

#include "AppInfo.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLocale>
#include <QMetaMethod>
#include <QPointer>

namespace edita::lsp {

namespace {

constexpr int kMaxLogLines = 2000;

QJsonObject capabilitiesForClient()
{
    // Lo que sabe hacer Edita (y cómo quiere los textos y las posiciones)
    const QJsonArray formats{QStringLiteral("markdown"), QStringLiteral("plaintext")};
    QJsonObject textDocument{
        {"synchronization", QJsonObject{{"didSave", true}, {"dynamicRegistration", false}}},
        {"publishDiagnostics", QJsonObject{{"relatedInformation", false}, {"versionSupport", false},
                                           {"tagSupport", QJsonObject{{"valueSet", QJsonArray{1, 2}}}}}},
        {"completion", QJsonObject{{"contextSupport", true},
                                   {"completionItem", QJsonObject{{"snippetSupport", true},
                                                                  {"documentationFormat", formats},
                                                                  {"deprecatedSupport", true},
                                                                  {"insertReplaceSupport", false}}},
                                   {"completionItemKind", QJsonObject{{"valueSet", [] {
                                        QJsonArray kinds;
                                        for (int i = 1; i <= 25; ++i)
                                            kinds.append(i);
                                        return kinds;
                                    }()}}}}},
        {"hover", QJsonObject{{"contentFormat", formats}}},
        {"signatureHelp", QJsonObject{{"signatureInformation", QJsonObject{{"documentationFormat", formats},
                                                                           {"parameterInformation", QJsonObject{{"labelOffsetSupport", true}}},
                                                                           {"activeParameterSupport", true}}}}},
        {"definition", QJsonObject{{"linkSupport", true}}},
        {"declaration", QJsonObject{{"linkSupport", true}}},
        {"references", QJsonObject{}},
        {"rename", QJsonObject{{"prepareSupport", false}}},
        {"formatting", QJsonObject{}},
        {"rangeFormatting", QJsonObject{}},
        {"codeAction", QJsonObject{{"codeActionLiteralSupport",
                                    QJsonObject{{"codeActionKind", QJsonObject{{"valueSet", QJsonArray{
                                        QStringLiteral(""), QStringLiteral("quickfix"), QStringLiteral("refactor"),
                                        QStringLiteral("refactor.extract"), QStringLiteral("refactor.inline"),
                                        QStringLiteral("refactor.rewrite"), QStringLiteral("source"),
                                        QStringLiteral("source.organizeImports"), QStringLiteral("source.fixAll")}}}}}},
                                   {"isPreferredSupport", true},
                                   {"dataSupport", true},
                                   {"resolveSupport", QJsonObject{{"properties", QJsonArray{QStringLiteral("edit")}}}}}},
        {"inlayHint", QJsonObject{{"dynamicRegistration", false}}},
    };
    return QJsonObject{
        {"general", QJsonObject{{"positionEncodings", QJsonArray{QStringLiteral("utf-8"), QStringLiteral("utf-16")}}}},
        {"textDocument", textDocument},
        {"workspace", QJsonObject{{"workspaceFolders", true}, {"configuration", true}, {"applyEdit", true},
                                  {"workspaceEdit", QJsonObject{{"documentChanges", true}}},
                                  {"symbol", QJsonObject{{"dynamicRegistration", false}}},
                                  {"executeCommand", QJsonObject{{"dynamicRegistration", false}}},
                                  {"inlayHint", QJsonObject{{"refreshSupport", false}}}}},
        {"window", QJsonObject{{"workDoneProgress", false}, {"showMessage", QJsonObject{}}}},
        // clangd antes de LSP 3.17
        {"offsetEncoding", QJsonArray{QStringLiteral("utf-8"), QStringLiteral("utf-16")}},
    };
}

} // namespace

LspClient::LspClient(const QString& program, const QStringList& arguments, const QString& rootPath, QObject* parent)
    : QObject(parent)
    , _program(program)
    , _arguments(arguments)
    , _rootPath(rootPath)
{
    _process.setWorkingDirectory(rootPath);
    connect(&_process, &QProcess::readyReadStandardOutput, this, &LspClient::readOutput);
    connect(&_process, &QProcess::readyReadStandardError, this, [this] {
        for (const QByteArray& line : _process.readAllStandardError().split('\n')) {
            if (!line.trimmed().isEmpty())
                appendLog(QString::fromUtf8(line));
        }
    });
    connect(&_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            appendLog(tr("Could not start %1: %2").arg(_program, _process.errorString()));
            setState(State::Failed);
        }
    });
    connect(&_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        appendLog(tr("The server exited (code %1)").arg(code));
        // Las peticiones que esperaban ya no tendrán respuesta
        auto pending = std::move(_pending);
        _pending.clear();
        for (auto& [id, callback] : pending) {
            if (callback)
                callback(QJsonValue(), QJsonObject{{"code", -32099}, {"message", QStringLiteral("server exited")}});
        }
        setState(_state == State::Stopped ? State::Stopped : State::Failed);
    });
}

LspClient::~LspClient()
{
    if (_process.state() != QProcess::NotRunning) {
        _process.disconnect(this);
        _process.kill();
        _process.waitForFinished(500);
    }
}

QString LspClient::name() const
{
    return QFileInfo(_program).fileName();
}

void LspClient::setState(State state)
{
    if (_state == state)
        return;
    _state = state;
    emit stateChanged(state);
}

void LspClient::appendLog(const QString& line)
{
    _log << QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz ")) + line;
    if (_log.size() > kMaxLogLines)
        _log.removeFirst();
}

void LspClient::start()
{
    if (_state != State::NotStarted)
        return;
    setState(State::Starting);
    appendLog(tr("Starting %1 %2 in %3").arg(_program, _arguments.join(QLatin1Char(' ')), _rootPath));
    _process.start(_program, _arguments);
    if (!_process.waitForStarted(5000)) {
        setState(State::Failed);
        return;
    }
    const QString rootUri = uriFromPath(_rootPath);
    const QJsonObject params{
        {"processId", static_cast<qint64>(QCoreApplication::applicationPid())},
        {"clientInfo", QJsonObject{{"name", QStringLiteral("Edita")}, {"version", QString::fromLatin1(kAppVersion)}}},
        {"locale", QLocale().name()},
        {"rootPath", _rootPath},
        {"rootUri", rootUri},
        {"workspaceFolders", QJsonArray{QJsonObject{{"uri", rootUri}, {"name", QFileInfo(_rootPath).fileName()}}}},
        {"capabilities", capabilitiesForClient()},
    };
    // initialize va antes que todo lo que espera en la cola
    const int id = _nextId++;
    _pending[id] = [this](const QJsonValue& result, const QJsonObject& error) {
        if (!error.isEmpty()) {
            appendLog(tr("initialize failed: %1").arg(error.value(QStringLiteral("message")).toString()));
            setState(State::Failed);
            return;
        }
        initialized(result.toObject());
    };
    send(QJsonObject{{"id", id}, {"method", QStringLiteral("initialize")}, {"params", params}});
}

void LspClient::initialized(const QJsonObject& result)
{
    _capabilities = result.value(QStringLiteral("capabilities")).toObject();
    const QString encoding = _capabilities.value(QStringLiteral("positionEncoding")).toString(
        result.value(QStringLiteral("offsetEncoding")).toString());
    _encoding = encoding == QLatin1String("utf-8") ? PositionEncoding::Utf8 : PositionEncoding::Utf16;
    const QJsonObject info = result.value(QStringLiteral("serverInfo")).toObject();
    appendLog(tr("Initialized: %1 %2, positions in %3")
                  .arg(info.value(QStringLiteral("name")).toString(name()), info.value(QStringLiteral("version")).toString(),
                       _encoding == PositionEncoding::Utf8 ? QStringLiteral("UTF-8") : QStringLiteral("UTF-16")));
    send(QJsonObject{{"method", QStringLiteral("initialized")}, {"params", QJsonObject{}}});
    setState(State::Running);
    const auto queued = std::move(_queued);
    _queued.clear();
    for (const QJsonObject& message : queued)
        send(message);
}

void LspClient::stop(int timeoutMs)
{
    if (_process.state() == QProcess::NotRunning) {
        setState(State::Stopped);
        return;
    }
    const State previous = _state;
    _state = State::Stopped;
    if (previous == State::Running) {
        // shutdown (petición) y exit (notificación); se espera la respuesta lo justo
        bool answered = false;
        const int id = _nextId++;
        _pending[id] = [&answered](const QJsonValue&, const QJsonObject&) { answered = true; };
        send(QJsonObject{{"id", id}, {"method", QStringLiteral("shutdown")}, {"params", QJsonValue()}});
        QElapsedTimer timer;
        timer.start();
        while (!answered && timer.elapsed() < timeoutMs / 2 && _process.state() != QProcess::NotRunning) {
            _process.waitForReadyRead(50);
            readOutput();
        }
        _pending.erase(id);
        send(QJsonObject{{"method", QStringLiteral("exit")}, {"params", QJsonValue()}});
        _process.closeWriteChannel();
    }
    if (!_process.waitForFinished(timeoutMs / 2)) {
        _process.kill();
        _process.waitForFinished(500);
    }
    emit stateChanged(State::Stopped);
}

bool LspClient::hasCapability(const QString& provider) const
{
    const QJsonValue value = _capabilities.value(provider);
    return value.isObject() || (value.isBool() && value.toBool());
}

QStringList LspClient::completionTriggers() const
{
    QStringList triggers;
    for (const QJsonValue& t : _capabilities.value(QStringLiteral("completionProvider")).toObject().value(QStringLiteral("triggerCharacters")).toArray())
        triggers << t.toString();
    return triggers;
}

QStringList LspClient::signatureTriggers() const
{
    QStringList triggers;
    const QJsonObject provider = _capabilities.value(QStringLiteral("signatureHelpProvider")).toObject();
    for (const char* key : {"triggerCharacters", "retriggerCharacters"}) {
        for (const QJsonValue& t : provider.value(QLatin1String(key)).toArray())
            triggers << t.toString();
    }
    return triggers;
}

int LspClient::syncKind() const
{
    const QJsonValue sync = _capabilities.value(QStringLiteral("textDocumentSync"));
    if (sync.isDouble())
        return sync.toInt();
    return sync.toObject().value(QStringLiteral("change")).toInt(0);
}

bool LspClient::wantsSave() const
{
    const QJsonValue save = _capabilities.value(QStringLiteral("textDocumentSync")).toObject().value(QStringLiteral("save"));
    return save.isObject() || (save.isBool() && save.toBool());
}

int LspClient::request(const QString& method, const QJsonValue& params, Callback callback)
{
    const int id = _nextId++;
    _pending[id] = std::move(callback);
    const QJsonObject message{{"id", id}, {"method", method}, {"params", params}};
    if (_state == State::Running)
        send(message);
    else if (_state == State::Starting || _state == State::NotStarted)
        _queued.push_back(message);
    else if (auto it = _pending.find(id); it != _pending.end()) { // sin servidor: error ya
        Callback cb = std::move(it->second);
        _pending.erase(it);
        if (cb)
            cb(QJsonValue(), QJsonObject{{"code", -32099}, {"message", QStringLiteral("server not running")}});
    }
    return id;
}

void LspClient::cancel(int id)
{
    if (_pending.erase(id))
        notify(QStringLiteral("$/cancelRequest"), QJsonObject{{"id", id}});
}

void LspClient::notify(const QString& method, const QJsonValue& params)
{
    const QJsonObject message{{"method", method}, {"params", params}};
    if (_state == State::Running)
        send(message);
    else if (_state == State::Starting || _state == State::NotStarted)
        _queued.push_back(message);
}

void LspClient::send(const QJsonObject& message)
{
    if (_process.state() != QProcess::Running)
        return;
    _process.write(JsonRpcStream::encode(message));
}

void LspClient::readOutput()
{
    _stream.feed(_process.readAllStandardOutput());
    while (auto message = _stream.next())
        handleMessage(*message);
}

void LspClient::handleMessage(const QJsonObject& message)
{
    const QJsonValue id = message.value(QStringLiteral("id"));
    const QString method = message.value(QStringLiteral("method")).toString();
    if (!method.isEmpty()) {
        const QJsonValue params = message.value(QStringLiteral("params"));
        if (!id.isUndefined()) {
            handleServerRequest(id, method, params);
            return;
        }
        if (method == QLatin1String("textDocument/publishDiagnostics")) {
            const QJsonObject p = params.toObject();
            emit diagnosticsPublished(p.value(QStringLiteral("uri")).toString(), p.value(QStringLiteral("diagnostics")).toArray());
        } else if (method == QLatin1String("window/logMessage") || method == QLatin1String("window/showMessage")) {
            const QJsonObject p = params.toObject();
            appendLog(p.value(QStringLiteral("message")).toString());
            if (method == QLatin1String("window/showMessage"))
                emit serverMessage(p.value(QStringLiteral("type")).toInt(), p.value(QStringLiteral("message")).toString());
        }
        return;
    }
    // Una respuesta
    const auto it = _pending.find(id.toInt(-1));
    if (it == _pending.end())
        return;
    Callback callback = std::move(it->second);
    _pending.erase(it);
    const QJsonObject error = message.value(QStringLiteral("error")).toObject();
    if (!error.isEmpty())
        appendLog(tr("Error %1: %2").arg(error.value(QStringLiteral("code")).toInt()).arg(error.value(QStringLiteral("message")).toString()));
    if (callback)
        callback(message.value(QStringLiteral("result")), error);
}

void LspClient::handleServerRequest(const QJsonValue& id, const QString& method, const QJsonValue& params)
{
    if (method == QLatin1String("workspace/configuration")) {
        // Sin configuración propia: null para cada elemento pedido
        QJsonArray result;
        for (int i = 0; i < params.toObject().value(QStringLiteral("items")).toArray().size(); ++i)
            result.append(QJsonValue());
        reply(id, result);
    } else if (method == QLatin1String("workspace/workspaceFolders")) {
        reply(id, QJsonArray{QJsonObject{{"uri", uriFromPath(_rootPath)}, {"name", QFileInfo(_rootPath).fileName()}}});
    } else if (method == QLatin1String("client/registerCapability") || method == QLatin1String("client/unregisterCapability")
               || method == QLatin1String("window/workDoneProgress/create") || method == QLatin1String("window/showMessageRequest")) {
        reply(id, QJsonValue());
    } else if (method == QLatin1String("workspace/applyEdit")) {
        if (!isSignalConnected(QMetaMethod::fromSignal(&LspClient::applyEditRequested))) {
            reply(id, QJsonObject{{"applied", false}});
            return;
        }
        const QJsonValue requestId = id;
        QPointer<LspClient> self(this);
        emit applyEditRequested(params.toObject().value(QStringLiteral("edit")).toObject(), [self, requestId](bool applied) {
            if (self)
                self->reply(requestId, QJsonObject{{"applied", applied}});
        });
    } else {
        replyError(id, -32601, QStringLiteral("Method not found: ") + method);
    }
}

void LspClient::reply(const QJsonValue& id, const QJsonValue& result)
{
    send(QJsonObject{{"id", id}, {"result", result}});
}

void LspClient::replyError(const QJsonValue& id, int code, const QString& message)
{
    send(QJsonObject{{"id", id}, {"error", QJsonObject{{"code", code}, {"message", message}}}});
}

} // namespace edita::lsp
