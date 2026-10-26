// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/LspManager.h"

#include "doc/DocumentManager.h"
#include "syntax/SyntaxStyler.h"

#include <ScintillaEditBase.h>

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QMetaMethod>
#include <QPainter>
#include <QScopeGuard>
#include <QTextDocument>
#include <QToolTip>

#include <algorithm>

namespace edita {

namespace {

constexpr qsizetype kMaxDocumentSize = 8 * 1024 * 1024; // más grande: sin servidor
constexpr int kMaxCompletionItems = 300;
constexpr int kKindImageBase = 2000; // imágenes de la lista: 2000 + CompletionItemKind
constexpr char kTypeSeparator = '\x1E';

QString cleanPath(const QString& path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

QByteArrayView lineBytes(ScintillaEditBase& view, sptr_t line)
{
    const sptr_t start = view.send(SCI_POSITIONFROMLINE, static_cast<uptr_t>(line));
    const sptr_t end = view.send(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(line));
    if (end <= start)
        return {};
    const auto* text = reinterpret_cast<const char*>(view.send(SCI_GETRANGEPOINTER, static_cast<uptr_t>(start), end - start));
    return text ? QByteArrayView(text, end - start) : QByteArrayView();
}

QString markdownToHtml(const QString& markdown)
{
    QTextDocument doc;
    doc.setMarkdown(markdown);
    return doc.toHtml();
}

// El contenido de un «hover»: MarkupContent, MarkedString o una lista de ellos, en Markdown
QString hoverMarkdown(const QJsonValue& contents)
{
    if (contents.isString())
        return contents.toString();
    if (contents.isArray()) {
        QStringList parts;
        for (const QJsonValue& part : contents.toArray())
            parts << hoverMarkdown(part);
        parts.removeAll(QString());
        return parts.join(QStringLiteral("\n\n---\n\n"));
    }
    const QJsonObject object = contents.toObject();
    const QString value = object.value(QStringLiteral("value")).toString();
    if (object.contains(QStringLiteral("language")))
        return QStringLiteral("```%1\n%2\n```").arg(object.value(QStringLiteral("language")).toString(), value);
    if (object.value(QStringLiteral("kind")).toString() == QLatin1String("plaintext"))
        return QStringLiteral("```\n%1\n```").arg(value);
    return value;
}

// Letra y color de cada tipo de elemento de la lista (CompletionItemKind)
std::pair<QChar, QColor> kindGlyph(int kind)
{
    switch (kind) {
    case 2: case 3: case 4: return {QLatin1Char('f'), QColor(0x9B, 0x4D, 0xCA)};       // método, función, constructor
    case 5: case 6: case 10: return {QLatin1Char('v'), QColor(0x1E, 0x88, 0xE5)};      // campo, variable, propiedad
    case 7: case 8: case 22: return {QLatin1Char('C'), QColor(0xE8, 0x8B, 0x00)};      // clase, interfaz, struct
    case 9: return {QLatin1Char('M'), QColor(0x6D, 0x8A, 0x9C)};                       // módulo
    case 13: case 20: return {QLatin1Char('E'), QColor(0xD9, 0x7A, 0x00)};             // enum y sus valores
    case 14: return {QLatin1Char('k'), QColor(0x80, 0x80, 0x80)};                      // palabra clave
    case 15: return {QLatin1Char('s'), QColor(0x43, 0xA0, 0x47)};                      // fragmento
    case 21: return {QLatin1Char('c'), QColor(0x00, 0x89, 0x7B)};                      // constante
    case 17: case 19: return {QLatin1Char('F'), QColor(0x8D, 0x6E, 0x63)};             // archivo, carpeta
    case 25: return {QLatin1Char('T'), QColor(0xE8, 0x8B, 0x00)};                      // parámetro de tipo
    default: return {QLatin1Char('t'), QColor(0x90, 0x90, 0x90)};                      // texto y lo demás
    }
}

} // namespace

LspManager::LspManager(DocumentManager& documents, QObject* parent)
    : QObject(parent)
    , _documentsManager(documents)
{
    _enabled = qEnvironmentVariable("EDITA_LSP") != QLatin1String("0");
    _syncTimer.setSingleShot(true);
    _syncTimer.setInterval(300);
    connect(&_syncTimer, &QTimer::timeout, this, &LspManager::syncNow);
}

LspManager::~LspManager()
{
    shutdownAll();
}

void LspManager::setEnabled(bool enabled)
{
    enabled = enabled && qEnvironmentVariable("EDITA_LSP") != QLatin1String("0");
    if (enabled == _enabled)
        return;
    _enabled = enabled;
    if (!enabled) {
        std::vector<BufferId> ids;
        for (const auto& [id, doc] : _documents)
            ids.push_back(id);
        for (const BufferId id : ids)
            detach(id);
        shutdownAll();
        _diagnostics.clear();
        emit diagnosticsChanged();
    }
    emit statusChanged();
}

void LspManager::setOverrides(const QMap<QString, QString>& overrides)
{
    _overrides = overrides;
}

lsp::LspClient* LspManager::clientFor(const lsp::ResolvedServer& server, const QString& root)
{
    const QString key = server.command + QLatin1Char('|') + root;
    if (auto it = _clients.find(key); it != _clients.end())
        return it->second.get(); // también si falló: no se relanza solo (para eso, Reiniciar)
    auto client = std::make_unique<lsp::LspClient>(server.program, server.arguments, root);
    lsp::LspClient* raw = client.get();
    connect(raw, &lsp::LspClient::diagnosticsPublished, this, &LspManager::receiveDiagnostics);
    connect(raw, &lsp::LspClient::stateChanged, this, &LspManager::statusChanged);
    connect(raw, &lsp::LspClient::applyEditRequested, this, [this, raw](const QJsonObject& edit, const std::function<void(bool)>& reply) {
        if (isSignalConnected(QMetaMethod::fromSignal(&LspManager::applyEditRequested)))
            emit applyEditRequested(edit, raw->positionEncoding(), reply);
        else
            reply(false);
    });
    _clients.emplace(key, std::move(client));
    raw->start();
    return raw;
}

void LspManager::attach(BufferId id)
{
    const Buffer* buf = _documentsManager.buffer(id);
    if (!_enabled || !buf || buf->isUntitled()) {
        detach(id);
        return;
    }
    const QString path = cleanPath(buf->filePath());
    const QString languageId = lsp::languageIdFor(buf->language(), buf->userLanguage(), path);
    const lsp::ServerSpec* spec = languageId.isEmpty() ? nullptr : lsp::serverForLanguage(languageId);
    const auto server = spec ? lsp::resolveServer(*spec, _overrides) : std::nullopt;
    if (!server) {
        detach(id);
        return;
    }
    const QString root = lsp::findRoot(path, _roots, spec->rootMarkers);
    lsp::LspClient* client = clientFor(*server, root);
    if (auto it = _documents.find(id); it != _documents.end()) {
        if (it->second.client == client && it->second.path == path && it->second.languageId == languageId)
            return;
        detach(id);
    }
    const QString text = QString::fromStdString(_documentsManager.text(id));
    if (text.size() > kMaxDocumentSize)
        return;
    Document doc;
    doc.path = path;
    doc.uri = lsp::uriFromPath(path);
    doc.languageId = languageId;
    doc.client = client;
    doc.version = 1;
    doc.sentVersion = buf->contentVersion();
    client->notify(QStringLiteral("textDocument/didOpen"),
                   QJsonObject{{"textDocument", QJsonObject{{"uri", doc.uri}, {"languageId", languageId}, {"version", 1}, {"text", text}}}});
    _documents.emplace(id, doc);
    emit statusChanged();
}

void LspManager::detach(BufferId id)
{
    const auto it = _documents.find(id);
    if (it == _documents.end())
        return;
    const Document doc = it->second;
    _documents.erase(it);
    if (doc.client && doc.client->state() == lsp::LspClient::State::Running)
        doc.client->notify(QStringLiteral("textDocument/didClose"), QJsonObject{{"textDocument", QJsonObject{{"uri", doc.uri}}}});
    // Sin documento abierto, sus diagnósticos ya no se actualizan: fuera
    const bool stillOpen = std::any_of(_documents.begin(), _documents.end(), [&](const auto& d) { return d.second.path == doc.path; });
    if (!stillOpen && _diagnostics.remove(doc.path))
        emit diagnosticsChanged();
    emit statusChanged();
}

void LspManager::syncNow()
{
    _syncTimer.stop();
    for (auto& [id, doc] : _documents) {
        const Buffer* buf = _documentsManager.buffer(id);
        if (!buf || buf->contentVersion() == doc.sentVersion || !doc.client || doc.client->syncKind() == 0)
            continue;
        doc.sentVersion = buf->contentVersion();
        ++doc.version;
        // Siempre el texto entero: lo aceptan todos los servidores, también los incrementales
        doc.client->notify(QStringLiteral("textDocument/didChange"),
                           QJsonObject{{"textDocument", QJsonObject{{"uri", doc.uri}, {"version", doc.version}}},
                                       {"contentChanges", QJsonArray{QJsonObject{{"text", QString::fromStdString(_documentsManager.text(id))}}}}});
    }
}

void LspManager::documentSaved(BufferId id)
{
    syncNow();
    if (auto it = _documents.find(id); it != _documents.end() && it->second.client && it->second.client->wantsSave())
        it->second.client->notify(QStringLiteral("textDocument/didSave"), QJsonObject{{"textDocument", QJsonObject{{"uri", it->second.uri}}}});
}

lsp::LspClient* LspManager::clientFor(BufferId id) const
{
    const auto it = _documents.find(id);
    return it == _documents.end() ? nullptr : it->second.client;
}

QString LspManager::statusText(BufferId id) const
{
    const lsp::LspClient* client = clientFor(id);
    if (!client)
        return {};
    switch (client->state()) {
    case lsp::LspClient::State::NotStarted:
    case lsp::LspClient::State::Starting: return tr("%1 (starting)").arg(client->name());
    case lsp::LspClient::State::Failed: return tr("%1 (failed)").arg(client->name());
    case lsp::LspClient::State::Stopped: return tr("%1 (stopped)").arg(client->name());
    case lsp::LspClient::State::Running: break;
    }
    return client->name();
}

// --- Diagnósticos ---------------------------------------------------------------------

void LspManager::receiveDiagnostics(const QString& uri, const QJsonArray& diagnostics)
{
    const QString path = lsp::pathFromUri(uri);
    if (path.isEmpty())
        return;
    std::vector<Diagnostic> list;
    for (const QJsonValue& value : diagnostics) {
        const QJsonObject d = value.toObject();
        const QJsonObject range = d.value(QStringLiteral("range")).toObject();
        const QJsonObject start = range.value(QStringLiteral("start")).toObject();
        const QJsonObject end = range.value(QStringLiteral("end")).toObject();
        Diagnostic diag;
        diag.line = start.value(QStringLiteral("line")).toInt();
        diag.character = start.value(QStringLiteral("character")).toInt();
        diag.endLine = end.value(QStringLiteral("line")).toInt();
        diag.endCharacter = end.value(QStringLiteral("character")).toInt();
        diag.severity = std::clamp(d.value(QStringLiteral("severity")).toInt(1), 1, 4);
        diag.message = d.value(QStringLiteral("message")).toString();
        diag.source = d.value(QStringLiteral("source")).toString();
        const QJsonValue code = d.value(QStringLiteral("code"));
        diag.code = code.isDouble() ? QString::number(code.toInt()) : code.toString();
        list.push_back(diag);
    }
    std::sort(list.begin(), list.end(), [](const Diagnostic& a, const Diagnostic& b) {
        return std::tie(a.line, a.character, a.severity) < std::tie(b.line, b.character, b.severity);
    });
    if (auto* client = qobject_cast<lsp::LspClient*>(sender()))
        _diagnosticEncodings[path] = client->positionEncoding();
    if (list.empty())
        _diagnostics.remove(path);
    else
        _diagnostics[path] = std::move(list);
    emit diagnosticsChanged();
}

std::vector<LspManager::Diagnostic> LspManager::diagnostics(BufferId id) const
{
    const Buffer* buf = _documentsManager.buffer(id);
    if (!buf || buf->isUntitled())
        return {};
    return _diagnostics.value(cleanPath(buf->filePath()));
}

int LspManager::count(int severity) const
{
    int total = 0;
    for (const auto& list : _diagnostics) {
        for (const Diagnostic& d : list)
            total += d.severity == severity ? 1 : 0;
    }
    return total;
}

void LspManager::paintDiagnostics(ScintillaEditBase& view, BufferId id)
{
    // Otros módulos cuentan con su indicador como el actual: se deja como estaba
    const sptr_t previousIndicator = view.send(SCI_GETINDICATORCURRENT);
    const auto restore = qScopeGuard([&] { view.send(SCI_SETINDICATORCURRENT, static_cast<uptr_t>(previousIndicator)); });
    const sptr_t length = view.send(SCI_GETLENGTH);
    for (const int indicator : {kIndicatorDiagError, kIndicatorDiagWarning, kIndicatorDiagInfo, kIndicatorDiagHint}) {
        view.send(SCI_SETINDICATORCURRENT, static_cast<uptr_t>(indicator));
        view.send(SCI_INDICATORCLEARRANGE, 0, length);
    }
    const Buffer* buf = _documentsManager.buffer(id);
    if (!buf || buf->isUntitled())
        return;
    const QString path = cleanPath(buf->filePath());
    const auto list = _diagnostics.value(path);
    const lsp::PositionEncoding encoding = _diagnosticEncodings.value(path, lsp::PositionEncoding::Utf16);
    static constexpr int kIndicators[] = {kIndicatorDiagError, kIndicatorDiagError, kIndicatorDiagWarning, kIndicatorDiagInfo, kIndicatorDiagHint};
    // Los menos graves primero: el error queda encima si se solapan
    std::vector<Diagnostic> ordered(list.begin(), list.end());
    std::stable_sort(ordered.begin(), ordered.end(), [](const Diagnostic& a, const Diagnostic& b) { return a.severity > b.severity; });
    for (const Diagnostic& d : ordered) {
        sptr_t start = scintillaPosition(view, encoding, d.line, d.character);
        sptr_t end = scintillaPosition(view, encoding, d.endLine, d.endCharacter);
        if (end <= start) { // vacío: al menos un carácter (el anterior si está al final)
            if (start >= length && length > 0)
                start = view.send(SCI_POSITIONBEFORE, static_cast<uptr_t>(length));
            end = view.send(SCI_POSITIONAFTER, static_cast<uptr_t>(start));
        }
        if (end <= start)
            continue;
        view.send(SCI_SETINDICATORCURRENT, static_cast<uptr_t>(kIndicators[d.severity]));
        view.send(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(start), end - start);
    }
}

std::vector<LspManager::Diagnostic> LspManager::diagnosticsAt(ScintillaEditBase& view, BufferId id, sptr_t pos) const
{
    std::vector<Diagnostic> found;
    const Buffer* buf = _documentsManager.buffer(id);
    if (!buf || buf->isUntitled())
        return found;
    const QString path = cleanPath(buf->filePath());
    const lsp::PositionEncoding encoding = _diagnosticEncodings.value(path, lsp::PositionEncoding::Utf16);
    for (const Diagnostic& d : _diagnostics.value(path)) {
        const sptr_t start = scintillaPosition(view, encoding, d.line, d.character);
        sptr_t end = scintillaPosition(view, encoding, d.endLine, d.endCharacter);
        if (end <= start)
            end = start + 1;
        if (pos >= start && pos <= end)
            found.push_back(d);
    }
    return found;
}

// --- Posiciones -------------------------------------------------------------------------

QJsonObject LspManager::lspPosition(ScintillaEditBase& view, lsp::PositionEncoding encoding, sptr_t pos) const
{
    const sptr_t line = view.send(SCI_LINEFROMPOSITION, static_cast<uptr_t>(pos));
    const sptr_t column = pos - view.send(SCI_POSITIONFROMLINE, static_cast<uptr_t>(line));
    return QJsonObject{{"line", static_cast<int>(line)},
                       {"character", lsp::lspCharacter(lineBytes(view, line), static_cast<int>(column), encoding)}};
}

sptr_t LspManager::scintillaPosition(ScintillaEditBase& view, lsp::PositionEncoding encoding, int line, int character) const
{
    if (line >= view.send(SCI_GETLINECOUNT))
        return view.send(SCI_GETLENGTH);
    const sptr_t start = view.send(SCI_POSITIONFROMLINE, static_cast<uptr_t>(std::max(0, line)));
    return start + lsp::byteColumn(lineBytes(view, std::max(0, line)), character, encoding);
}

sptr_t LspManager::scintillaPosition(ScintillaEditBase& view, lsp::PositionEncoding encoding, const QJsonObject& position) const
{
    return scintillaPosition(view, encoding, position.value(QStringLiteral("line")).toInt(),
                             position.value(QStringLiteral("character")).toInt());
}

void LspManager::applyTextEdits(ScintillaEditBase& view, lsp::PositionEncoding encoding, const QJsonArray& edits)
{
    struct Edit
    {
        sptr_t start;
        sptr_t end;
        QByteArray text;
    };
    std::vector<Edit> list;
    for (const QJsonValue& value : edits) {
        const QJsonObject edit = value.toObject();
        const QJsonObject range = edit.value(QStringLiteral("range")).toObject();
        list.push_back({scintillaPosition(view, encoding, range.value(QStringLiteral("start")).toObject()),
                        scintillaPosition(view, encoding, range.value(QStringLiteral("end")).toObject()),
                        edit.value(QStringLiteral("newText")).toString().toUtf8()});
    }
    // De atrás hacia delante: cada cambio no mueve los que faltan
    std::stable_sort(list.begin(), list.end(), [](const Edit& a, const Edit& b) { return a.start > b.start; });
    view.send(SCI_BEGINUNDOACTION);
    for (const Edit& e : list) {
        view.send(SCI_SETTARGETRANGE, static_cast<uptr_t>(e.start), std::max(e.start, e.end));
        view.send(SCI_REPLACETARGET, static_cast<uptr_t>(e.text.size()), reinterpret_cast<sptr_t>(e.text.constData()));
    }
    view.send(SCI_ENDUNDOACTION);
}

// --- Autocompletado ---------------------------------------------------------------------

bool LspManager::complete(ScintillaEditBase& view, BufferId id, int ch)
{
    lsp::LspClient* client = clientFor(id);
    if (!client || client->state() == lsp::LspClient::State::Failed || client->state() == lsp::LspClient::State::Stopped)
        return false;
    if (client->state() == lsp::LspClient::State::Running && !client->hasCapability(QStringLiteral("completionProvider")))
        return false;
    const sptr_t caret = view.send(SCI_GETCURRENTPOS);
    QJsonObject context{{"triggerKind", 1}}; // invocado (o escribiendo un identificador)
    if (ch != 0) {
        // ¿Termina en un carácter de disparo («.», «->», «::»…)?
        const sptr_t from = std::max<sptr_t>(0, caret - 3);
        const auto* raw = reinterpret_cast<const char*>(view.send(SCI_GETRANGEPOINTER, static_cast<uptr_t>(from), caret - from));
        const QString before = raw ? QString::fromUtf8(raw, static_cast<qsizetype>(caret - from)) : QString();
        QString trigger;
        for (const QString& t : client->completionTriggers()) {
            if (!t.isEmpty() && before.endsWith(t) && t.size() > trigger.size())
                trigger = t;
        }
        if (!trigger.isEmpty()) {
            context = QJsonObject{{"triggerKind", 2}, {"triggerCharacter", trigger.right(1)}};
        } else {
            const char32_t c = static_cast<char32_t>(ch);
            const bool word = QChar::isLetterOrNumber(c) || c == U'_';
            if (!word || isCompletionActive(view) || !_autoComplete)
                return true; // la lista abierta se filtra sola; lo demás no abre nada
            const sptr_t start = view.send(SCI_WORDSTARTPOSITION, static_cast<uptr_t>(caret), 1);
            if (caret - start < _autoCompleteFrom)
                return true;
        }
    }
    syncNow();
    if (_completionRequest && _completionClient)
        _completionClient->cancel(_completionRequest);
    const auto it = _documents.find(id);
    const QJsonObject params{{"textDocument", QJsonObject{{"uri", it->second.uri}}},
                             {"position", lspPosition(view, client->positionEncoding(), caret)},
                             {"context", context}};
    const int token = ++_completionToken;
    QPointer<ScintillaEditBase> target(&view);
    _completionClient = client;
    _completionRequest = client->request(QStringLiteral("textDocument/completion"), params,
                                         [this, target, client, caret, token](const QJsonValue& result, const QJsonObject& error) {
        if (token != _completionToken || !target)
            return;
        _completionRequest = 0;
        if (error.isEmpty())
            showCompletion(*target, client, result, caret);
    });
    return true;
}

void LspManager::registerKindImages(ScintillaEditBase& view)
{
    const int size = std::max(12, static_cast<int>(view.send(SCI_TEXTHEIGHT, 0)) - 2);
    for (int kind = 1; kind <= 25; ++kind) {
        const auto [letter, color] = kindGlyph(kind);
        QImage image(size, size, QImage::Format_RGBA8888);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawRoundedRect(QRectF(1, 1, size - 2, size - 2), 3, 3);
        QFont font = painter.font();
        font.setBold(true);
        font.setPixelSize(std::max(8, size - 5));
        painter.setFont(font);
        painter.setPen(Qt::white);
        painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, QString(letter));
        painter.end();
        view.send(SCI_RGBAIMAGESETWIDTH, static_cast<uptr_t>(size));
        view.send(SCI_RGBAIMAGESETHEIGHT, static_cast<uptr_t>(size));
        view.send(SCI_REGISTERRGBAIMAGE, static_cast<uptr_t>(kKindImageBase + kind), reinterpret_cast<sptr_t>(image.constBits()));
    }
}

void LspManager::showCompletion(ScintillaEditBase& view, lsp::LspClient* client, const QJsonValue& result, sptr_t requestPos)
{
    const sptr_t caret = view.send(SCI_GETCURRENTPOS);
    if (view.send(SCI_LINEFROMPOSITION, static_cast<uptr_t>(caret)) != view.send(SCI_LINEFROMPOSITION, static_cast<uptr_t>(requestPos)))
        return; // el cursor ya se fue a otra línea
    const QJsonArray items = result.isArray() ? result.toArray() : result.toObject().value(QStringLiteral("items")).toArray();
    const sptr_t wordStart = view.send(SCI_WORDSTARTPOSITION, static_cast<uptr_t>(caret), 1);
    const auto* raw = reinterpret_cast<const char*>(view.send(SCI_GETRANGEPOINTER, static_cast<uptr_t>(wordStart), caret - wordStart));
    const QString prefix = raw ? QString::fromUtf8(raw, static_cast<qsizetype>(caret - wordStart)) : QString();

    struct Entry
    {
        QString sortKey;
        QString display;
        QJsonObject item;
    };
    std::vector<Entry> entries;
    for (const QJsonValue& value : items) {
        const QJsonObject item = value.toObject();
        QString display = item.value(QStringLiteral("label")).toString().trimmed();
        while (display.startsWith(QChar(0x2022))) // «•» de clangd (añadiría un #include)
            display = display.mid(1).trimmed();
        display.remove(QLatin1Char('\n')).remove(QLatin1Char(kTypeSeparator));
        if (display.isEmpty())
            continue;
        const QString filter = item.value(QStringLiteral("filterText")).toString(display);
        if (!prefix.isEmpty() && !filter.startsWith(prefix, Qt::CaseInsensitive) && !display.startsWith(prefix, Qt::CaseInsensitive))
            continue;
        entries.push_back({item.value(QStringLiteral("sortText")).toString(display), display, item});
    }
    std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.sortKey < b.sortKey; });
    _completionItems.clear();
    QStringList list;
    for (const Entry& e : entries) {
        if (_completionItems.contains(e.display))
            continue;
        _completionItems.insert(e.display, {e.item, client});
        const int kind = std::clamp(e.item.value(QStringLiteral("kind")).toInt(1), 1, 25);
        list << e.display + QLatin1Char(kTypeSeparator) + QString::number(kKindImageBase + kind);
        if (list.size() >= kMaxCompletionItems)
            break;
    }
    if (list.isEmpty()) {
        if (isCompletionActive(view))
            view.send(SCI_AUTOCCANCEL);
        return;
    }
    registerKindImages(view);
    view.send(SCI_AUTOCSETSEPARATOR, '\n');
    view.send(SCI_AUTOCSETTYPESEPARATOR, static_cast<uptr_t>(kTypeSeparator));
    view.send(SCI_AUTOCSETIGNORECASE, 1);
    view.send(SCI_AUTOCSETCASEINSENSITIVEBEHAVIOUR, SC_CASEINSENSITIVEBEHAVIOUR_IGNORECASE);
    view.send(SCI_AUTOCSETORDER, SC_ORDER_CUSTOM); // el orden del servidor
    view.send(SCI_AUTOCSETMAXHEIGHT, 12);
    _completionView = &view;
    const QByteArray utf8 = list.join(QLatin1Char('\n')).toUtf8();
    view.send(SCI_AUTOCSHOW, static_cast<uptr_t>(caret - wordStart), reinterpret_cast<sptr_t>(utf8.constData()));
}

bool LspManager::isCompletionActive(const ScintillaEditBase& view) const
{
    return _completionView == &view && view.send(SCI_AUTOCACTIVE) != 0;
}

void LspManager::completionClosed(ScintillaEditBase& view)
{
    if (_completionView == &view) {
        _completionView = nullptr;
        _completionItems.clear();
    }
}

bool LspManager::completionSelected(ScintillaEditBase& view, const QString& text)
{
    if (_completionView != &view)
        return false;
    const auto it = _completionItems.constFind(text);
    if (it == _completionItems.constEnd())
        return false;
    const CompletionItem item = *it;
    view.send(SCI_AUTOCCANCEL); // lo escribimos nosotros (textEdit, fragmentos, #include…)
    completionClosed(view);
    insertCompletion(view, item);
    return true;
}

void LspManager::insertCompletion(ScintillaEditBase& view, const CompletionItem& entry)
{
    const QJsonObject& item = entry.item;
    const lsp::PositionEncoding encoding = entry.client ? entry.client->positionEncoding() : lsp::PositionEncoding::Utf16;
    const sptr_t caret = view.send(SCI_GETCURRENTPOS);
    sptr_t start = view.send(SCI_WORDSTARTPOSITION, static_cast<uptr_t>(caret), 1);
    sptr_t end = caret;
    QString text = item.value(QStringLiteral("insertText")).toString(item.value(QStringLiteral("label")).toString().trimmed());
    const QJsonObject textEdit = item.value(QStringLiteral("textEdit")).toObject();
    if (!textEdit.isEmpty()) {
        // TextEdit o InsertReplaceEdit; lo escrito después de pedir la lista también se sustituye
        const QJsonObject range = textEdit.value(textEdit.contains(QStringLiteral("range")) ? QStringLiteral("range") : QStringLiteral("insert")).toObject();
        start = scintillaPosition(view, encoding, range.value(QStringLiteral("start")).toObject());
        end = std::max(caret, scintillaPosition(view, encoding, range.value(QStringLiteral("end")).toObject()));
        text = textEdit.value(QStringLiteral("newText")).toString();
    }
    int selectionStart = static_cast<int>(text.size());
    int selectionEnd = selectionStart;
    if (item.value(QStringLiteral("insertTextFormat")).toInt() == 2) {
        const lsp::ExpandedSnippet snippet = lsp::expandSnippet(text);
        text = snippet.text;
        selectionStart = snippet.selectionStart;
        selectionEnd = snippet.selectionEnd;
    }
    const QByteArray bytes = text.toUtf8();
    const auto selStartBytes = static_cast<sptr_t>(text.left(selectionStart).toUtf8().size());
    const auto selEndBytes = static_cast<sptr_t>(text.left(selectionEnd).toUtf8().size());

    // Los cambios adicionales (p. ej. el #include que añade clangd), en posiciones de ahora
    struct Edit
    {
        sptr_t start;
        sptr_t end;
        QByteArray text;
        bool main;
    };
    std::vector<Edit> edits{{start, end, bytes, true}};
    for (const QJsonValue& value : item.value(QStringLiteral("additionalTextEdits")).toArray()) {
        const QJsonObject edit = value.toObject();
        const QJsonObject range = edit.value(QStringLiteral("range")).toObject();
        edits.push_back({scintillaPosition(view, encoding, range.value(QStringLiteral("start")).toObject()),
                         scintillaPosition(view, encoding, range.value(QStringLiteral("end")).toObject()),
                         edit.value(QStringLiteral("newText")).toString().toUtf8(), false});
    }
    sptr_t shift = 0; // lo que se mueve el principio del cambio principal
    for (const Edit& e : edits) {
        if (!e.main && e.start < start)
            shift += static_cast<sptr_t>(e.text.size()) - (e.end - e.start);
    }
    std::stable_sort(edits.begin(), edits.end(), [](const Edit& a, const Edit& b) { return a.start > b.start; });
    view.send(SCI_BEGINUNDOACTION);
    for (const Edit& e : edits) {
        view.send(SCI_SETTARGETRANGE, static_cast<uptr_t>(e.start), std::max(e.start, e.end));
        view.send(SCI_REPLACETARGET, static_cast<uptr_t>(e.text.size()), reinterpret_cast<sptr_t>(e.text.constData()));
    }
    view.send(SCI_SETSEL, static_cast<uptr_t>(start + shift + selStartBytes), start + shift + selEndBytes);
    view.send(SCI_ENDUNDOACTION);
    view.send(SCI_SCROLLCARET);
}

// --- Información al pasar el ratón ------------------------------------------------------

void LspManager::hover(ScintillaEditBase& view, BufferId id, sptr_t pos, const QPoint& globalPos)
{
    const int token = ++_hoverToken;
    QStringList diagnosticHtml;
    QStringList diagnosticText;
    static const char* const kSymbols[] = {"", "⨯", "⚠", "ⓘ", "…"};
    static const char* const kColors[] = {"", "#e51400", "#bf8803", "#1a85ff", "#808080"};
    for (const Diagnostic& d : diagnosticsAt(view, id, pos)) {
        QString source = d.source;
        if (!d.code.isEmpty())
            source += QStringLiteral("(%1)").arg(d.code);
        diagnosticHtml << QStringLiteral("<p><b style=\"color:%1\">%2</b> %3 <span style=\"color:gray\">%4</span></p>")
                              .arg(QLatin1String(kColors[d.severity]), QString::fromUtf8(kSymbols[d.severity]),
                                   d.message.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>")), source.toHtmlEscaped());
        diagnosticText << d.message;
    }
    QPointer<ScintillaEditBase> target(&view);
    auto show = [this, target, globalPos, diagnosticHtml, diagnosticText](const QString& markdown) {
        if (!target)
            return;
        QString html = diagnosticHtml.join(QString());
        if (!markdown.trimmed().isEmpty()) {
            if (!html.isEmpty())
                html += QStringLiteral("<hr>");
            html += markdownToHtml(markdown.left(6000));
        }
        _lastHoverText = (diagnosticText + QStringList{markdown}).join(QLatin1Char('\n')).trimmed();
        if (!html.isEmpty())
            QToolTip::showText(globalPos, html, target);
    };
    lsp::LspClient* client = clientFor(id);
    if (!client || client->state() != lsp::LspClient::State::Running || !client->hasCapability(QStringLiteral("hoverProvider"))) {
        show(QString());
        return;
    }
    syncNow();
    const QJsonObject params{{"textDocument", QJsonObject{{"uri", _documents.at(id).uri}}},
                             {"position", lspPosition(view, client->positionEncoding(), pos)}};
    client->request(QStringLiteral("textDocument/hover"), params, [this, token, show](const QJsonValue& result, const QJsonObject&) {
        if (token != _hoverToken)
            return; // el ratón ya se movió
        show(result.isObject() ? hoverMarkdown(result.toObject().value(QStringLiteral("contents"))) : QString());
    });
}

void LspManager::hideHover()
{
    ++_hoverToken;
    QToolTip::hideText();
}

// --- Navegación y cambios --------------------------------------------------------------

bool LspManager::supports(BufferId id, const QString& provider) const
{
    const lsp::LspClient* client = clientFor(id);
    return client && client->state() == lsp::LspClient::State::Running && client->hasCapability(provider);
}

lsp::PositionEncoding LspManager::encodingForPath(const QString& path) const
{
    const QString clean = cleanPath(path);
    if (_diagnosticEncodings.contains(clean))
        return _diagnosticEncodings.value(clean);
    for (const auto& [id, doc] : _documents) {
        if (doc.path == clean && doc.client)
            return doc.client->positionEncoding();
    }
    return lsp::PositionEncoding::Utf16;
}

std::vector<LspManager::Location> LspManager::parseLocations(const QJsonValue& result)
{
    // Location, Location[] o LocationLink[] (de este, el nombre: targetSelectionRange)
    std::vector<Location> locations;
    const QJsonArray array = result.isArray() ? result.toArray() : (result.isObject() ? QJsonArray{result} : QJsonArray());
    for (const QJsonValue& value : array) {
        const QJsonObject object = value.toObject();
        const bool link = object.contains(QStringLiteral("targetUri"));
        const QString uri = object.value(link ? QStringLiteral("targetUri") : QStringLiteral("uri")).toString();
        QJsonObject range = object.value(link ? QStringLiteral("targetSelectionRange") : QStringLiteral("range")).toObject();
        if (range.isEmpty())
            range = object.value(QStringLiteral("targetRange")).toObject();
        const QString path = lsp::pathFromUri(uri);
        if (path.isEmpty())
            continue;
        const QJsonObject start = range.value(QStringLiteral("start")).toObject();
        const QJsonObject end = range.value(QStringLiteral("end")).toObject();
        locations.push_back({path, start.value(QStringLiteral("line")).toInt(), start.value(QStringLiteral("character")).toInt(),
                             end.value(QStringLiteral("line")).toInt(), end.value(QStringLiteral("character")).toInt()});
    }
    return locations;
}

bool LspManager::findLocations(ScintillaEditBase& view, BufferId id, sptr_t pos, const QString& method, LocationsCallback done)
{
    const QString provider = method == QLatin1String("textDocument/references") ? QStringLiteral("referencesProvider")
                                                                                 : QStringLiteral("definitionProvider");
    if (!supports(id, provider))
        return false;
    lsp::LspClient* client = clientFor(id);
    syncNow();
    QJsonObject params{{"textDocument", QJsonObject{{"uri", _documents.at(id).uri}}},
                       {"position", lspPosition(view, client->positionEncoding(), pos)}};
    if (provider == QLatin1String("referencesProvider"))
        params.insert(QStringLiteral("context"), QJsonObject{{"includeDeclaration", true}});
    const lsp::PositionEncoding encoding = client->positionEncoding();
    client->request(method, params, [done = std::move(done), encoding](const QJsonValue& result, const QJsonObject&) {
        done(parseLocations(result), encoding);
    });
    return true;
}

bool LspManager::rename(ScintillaEditBase& view, BufferId id, sptr_t pos, const QString& newName, EditCallback done)
{
    if (!supports(id, QStringLiteral("renameProvider")))
        return false;
    lsp::LspClient* client = clientFor(id);
    syncNow();
    const QJsonObject params{{"textDocument", QJsonObject{{"uri", _documents.at(id).uri}}},
                             {"position", lspPosition(view, client->positionEncoding(), pos)},
                             {"newName", newName}};
    const lsp::PositionEncoding encoding = client->positionEncoding();
    client->request(QStringLiteral("textDocument/rename"), params,
                    [done = std::move(done), encoding](const QJsonValue& result, const QJsonObject& error) {
        done(result.toObject(), encoding, error.value(QStringLiteral("message")).toString());
    });
    return true;
}

bool LspManager::format(ScintillaEditBase& view, BufferId id)
{
    const sptr_t selStart = view.send(SCI_GETSELECTIONSTART);
    const sptr_t selEnd = view.send(SCI_GETSELECTIONEND);
    const bool range = selEnd > selStart && supports(id, QStringLiteral("documentRangeFormattingProvider"));
    if (!range && !supports(id, QStringLiteral("documentFormattingProvider")))
        return false;
    lsp::LspClient* client = clientFor(id);
    syncNow();
    const lsp::PositionEncoding encoding = client->positionEncoding();
    QJsonObject params{{"textDocument", QJsonObject{{"uri", _documents.at(id).uri}}},
                       {"options", QJsonObject{{"tabSize", static_cast<int>(view.send(SCI_GETTABWIDTH))},
                                               {"insertSpaces", view.send(SCI_GETUSETABS) == 0},
                                               {"trimTrailingWhitespace", true}}}};
    if (range)
        params.insert(QStringLiteral("range"), QJsonObject{{"start", lspPosition(view, encoding, selStart)}, {"end", lspPosition(view, encoding, selEnd)}});
    // La respuesta llega después: solo si la vista sigue con el mismo documento
    QPointer<ScintillaEditBase> target(&view);
    const sptr_t document = view.send(SCI_GETDOCPOINTER);
    client->request(range ? QStringLiteral("textDocument/rangeFormatting") : QStringLiteral("textDocument/formatting"), params,
                    [this, target, document, encoding](const QJsonValue& result, const QJsonObject&) {
        if (!target || target->send(SCI_GETDOCPOINTER) != document || !result.isArray() || result.toArray().isEmpty())
            return;
        // El cursor, en la misma línea y columna (lo más parecido)
        const sptr_t caret = target->send(SCI_GETCURRENTPOS);
        const sptr_t line = target->send(SCI_LINEFROMPOSITION, static_cast<uptr_t>(caret));
        const sptr_t column = target->send(SCI_GETCOLUMN, static_cast<uptr_t>(caret));
        applyTextEdits(*target, encoding, result.toArray());
        target->send(SCI_GOTOPOS, static_cast<uptr_t>(target->send(SCI_FINDCOLUMN, static_cast<uptr_t>(line), column)));
    });
    return true;
}

bool LspManager::signatureHelp(ScintillaEditBase& view, BufferId id, int ch)
{
    if (!supports(id, QStringLiteral("signatureHelpProvider")))
        return false;
    lsp::LspClient* client = clientFor(id);
    const bool active = _signatureView == &view && view.send(SCI_CALLTIPACTIVE) != 0;
    QJsonObject context{{"triggerKind", 1}, {"isRetrigger", active}};
    if (ch != 0) {
        const auto c = static_cast<char32_t>(ch);
        const QString typed = QString::fromUcs4(&c, 1);
        if (typed == QLatin1String(")") && active) {
            view.send(SCI_CALLTIPCANCEL);
            return true;
        }
        if (!client->signatureTriggers().contains(typed))
            return false;
        context = QJsonObject{{"triggerKind", 2}, {"triggerCharacter", typed}, {"isRetrigger", active}};
    }
    syncNow();
    const int token = ++_signatureToken;
    const sptr_t caret = view.send(SCI_GETCURRENTPOS);
    const QJsonObject params{{"textDocument", QJsonObject{{"uri", _documents.at(id).uri}}},
                             {"position", lspPosition(view, client->positionEncoding(), caret)},
                             {"context", context}};
    QPointer<ScintillaEditBase> target(&view);
    client->request(QStringLiteral("textDocument/signatureHelp"), params, [this, target, token, caret](const QJsonValue& result, const QJsonObject&) {
        if (token != _signatureToken || !target)
            return;
        const QJsonArray signatures = result.toObject().value(QStringLiteral("signatures")).toArray();
        if (signatures.isEmpty()) {
            if (_signatureView == target && target->send(SCI_CALLTIPACTIVE))
                target->send(SCI_CALLTIPCANCEL);
            return;
        }
        const QJsonObject help = result.toObject();
        const int activeSignature = std::clamp(help.value(QStringLiteral("activeSignature")).toInt(0), 0, static_cast<int>(signatures.size()) - 1);
        const QJsonObject signature = signatures.at(activeSignature).toObject();
        const QString label = signature.value(QStringLiteral("label")).toString();
        const int activeParameter = signature.value(QStringLiteral("activeParameter")).toInt(help.value(QStringLiteral("activeParameter")).toInt(0));
        // «(1/2) » si hay varias sobrecargas
        const QString prefix = signatures.size() > 1 ? QStringLiteral("(%1/%2) ").arg(activeSignature + 1).arg(signatures.size()) : QString();
        QString text = prefix + label;
        int highlightStart = 0;
        int highlightEnd = 0;
        const QJsonArray parameters = signature.value(QStringLiteral("parameters")).toArray();
        if (activeParameter >= 0 && activeParameter < parameters.size()) {
            const QJsonValue parameter = parameters.at(activeParameter).toObject().value(QStringLiteral("label"));
            int from = -1;
            int to = -1;
            if (parameter.isArray()) { // [inicio, fin] en UTF-16 dentro de la etiqueta
                from = parameter.toArray().at(0).toInt();
                to = parameter.toArray().at(1).toInt();
            } else {
                const qsizetype open = label.indexOf(QLatin1Char('('));
                from = static_cast<int>(label.indexOf(parameter.toString(), std::max<qsizetype>(0, open)));
                to = from < 0 ? -1 : from + static_cast<int>(parameter.toString().size());
            }
            if (from >= 0 && to > from) {
                highlightStart = static_cast<int>(text.left(prefix.size() + from).toUtf8().size());
                highlightEnd = static_cast<int>(text.left(prefix.size() + to).toUtf8().size());
            }
        }
        const QJsonValue documentation = signature.value(QStringLiteral("documentation"));
        QString doc = documentation.isObject() ? documentation.toObject().value(QStringLiteral("value")).toString() : documentation.toString();
        doc = doc.section(QStringLiteral("\n\n"), 0, 0).trimmed();
        if (doc.size() > 300)
            doc = doc.left(299) + QStringLiteral("…");
        if (!doc.isEmpty())
            text += QLatin1Char('\n') + doc;
        const QByteArray utf8 = text.toUtf8();
        target->send(SCI_CALLTIPSHOW, static_cast<uptr_t>(caret), reinterpret_cast<sptr_t>(utf8.constData()));
        target->send(SCI_CALLTIPSETHLT, static_cast<uptr_t>(highlightStart), highlightEnd);
        _signatureView = target;
        _lastSignatureText = text;
        _lastSignatureHighlight = {highlightStart, highlightEnd};
    });
    return true;
}

// --- Servidores -------------------------------------------------------------------------

void LspManager::restart(BufferId id)
{
    lsp::LspClient* client = clientFor(id);
    if (!client)
        return;
    std::vector<BufferId> users;
    for (const auto& [bufferId, doc] : _documents) {
        if (doc.client == client)
            users.push_back(bufferId);
    }
    for (const BufferId user : users)
        detach(user);
    for (auto it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->second.get() == client) {
            it->second->stop(1000);
            _clients.erase(it);
            break;
        }
    }
    for (const BufferId user : users)
        attach(user);
}

QString LspManager::log(BufferId id) const
{
    const lsp::LspClient* client = clientFor(id);
    return client ? client->log() : QString();
}

void LspManager::shutdownAll()
{
    for (auto& [key, client] : _clients) {
        client->disconnect(this);
        client->stop(800);
    }
    _clients.clear();
    _documents.clear();
}

} // namespace edita
