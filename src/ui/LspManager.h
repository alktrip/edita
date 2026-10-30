// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Los servidores de lenguaje de los documentos abiertos (no existe en Notepad++). Cada
// documento con archivo y un lenguaje que tenga servidor instalado se abre en el suyo
// (uno por servidor y carpeta de proyecto); el texto se le envía al editar, y lo que
// devuelve se muestra: diagnósticos subrayados y en el panel Problemas, autocompletado
// y la información al dejar el ratón encima.

#pragma once

#include "doc/Buffer.h"
#include "lsp/LspClient.h"

#include <QMap>

#include <functional>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

#include <ScintillaEditBase.h>

namespace edita {

class DocumentManager;

class LspManager : public QObject
{
    Q_OBJECT

public:
    struct Diagnostic
    {
        int line = 0; // posiciones de LSP (carácter según la codificación del servidor)
        int character = 0;
        int endLine = 0;
        int endCharacter = 0;
        int severity = 1; // 1 error, 2 aviso, 3 información, 4 pista
        QString message;
        QString source;
        QString code;
        QJsonObject raw; // tal cual (para pedir las acciones de código que lo arreglan)
    };

    // Un símbolo del proyecto (workspace/symbol)
    struct SymbolHit
    {
        QString name;
        int kind = 0;
        QString container;
        QString path;
        int line = 0;
        int character = 0;
        lsp::PositionEncoding encoding = lsp::PositionEncoding::Utf16;
    };

    struct Location
    {
        QString path;
        int line = 0; // posiciones de LSP
        int character = 0;
        int endLine = 0;
        int endCharacter = 0;
    };
    using LocationsCallback = std::function<void(const std::vector<Location>& locations, lsp::PositionEncoding encoding)>;
    using EditCallback = std::function<void(const QJsonObject& edit, lsp::PositionEncoding encoding, const QString& error)>;

    explicit LspManager(DocumentManager& documents, QObject* parent = nullptr);
    ~LspManager() override;

    // Desactivado con la preferencia o con EDITA_LSP=0 (las pruebas)
    void setEnabled(bool enabled);
    bool isEnabled() const { return _enabled; }
    void setOverrides(const QMap<QString, QString>& overrides);
    void setWorkspaceRoots(const QStringList& roots) { _roots = roots; }
    // La lista sale sola al escribir (desde `fromLength` letras), como la de N++
    void setAutoComplete(bool enabled, int fromLength)
    {
        _autoComplete = enabled;
        _autoCompleteFrom = std::max(1, fromLength);
    }

    // Al abrir un documento, cambiar su lenguaje o su nombre: se abre en su servidor
    void attach(BufferId id);
    void detach(BufferId id);
    bool isAttached(BufferId id) const { return _documents.count(id) != 0; }
    // El texto cambió: se envía en un momento (o ya, antes de una petición)
    void scheduleSync() { _syncTimer.start(); }
    void syncNow();
    void documentSaved(BufferId id);

    lsp::LspClient* clientFor(BufferId id) const;
    // Para la barra de estado: «clangd» o «clangd (iniciando)»…
    QString statusText(BufferId id) const;
    bool hasClients() const { return !_clients.empty(); }

    std::vector<Diagnostic> diagnostics(BufferId id) const;
    const QMap<QString, std::vector<Diagnostic>>& allDiagnostics() const { return _diagnostics; }
    int count(int severity) const;
    // Los diagnósticos del documento como indicadores en la vista que lo muestra
    void paintDiagnostics(ScintillaEditBase& view, BufferId id);
    // Lo que hay en esa posición (para el texto al pasar el ratón)
    std::vector<Diagnostic> diagnosticsAt(ScintillaEditBase& view, BufferId id, sptr_t pos) const;

    // Posiciones: de Scintilla a LSP y al revés, con la codificación del servidor
    QJsonObject lspPosition(ScintillaEditBase& view, lsp::PositionEncoding encoding, sptr_t pos) const;
    sptr_t scintillaPosition(ScintillaEditBase& view, lsp::PositionEncoding encoding, int line, int character) const;
    sptr_t scintillaPosition(ScintillaEditBase& view, lsp::PositionEncoding encoding, const QJsonObject& position) const;

    // Autocompletado: `ch` es el carácter recién escrito (0: Ctrl+Espacio). false si el
    // documento no tiene servidor con autocompletado (lo hace el de N++)
    bool complete(ScintillaEditBase& view, BufferId id, int ch);
    // SCN_AUTOCSELECTION de una lista nuestra: se escribe lo del elemento. true si era nuestra
    bool completionSelected(ScintillaEditBase& view, const QString& text);
    void completionClosed(ScintillaEditBase& view);
    bool isCompletionActive(const ScintillaEditBase& view) const;

    // Información al dejar el ratón: diagnósticos y lo que diga el servidor
    void hover(ScintillaEditBase& view, BufferId id, sptr_t pos, const QPoint& globalPos);
    void hideHover();
    // Para las pruebas: el último texto mostrado
    QString lastHoverText() const { return _lastHoverText; }

    // Aplica cambios de texto de LSP (TextEdit[]) en la vista, de una vez para deshacer
    void applyTextEdits(ScintillaEditBase& view, lsp::PositionEncoding encoding, const QJsonArray& edits);

    // ¿El servidor del documento sabe hacer esto? («definitionProvider», «renameProvider»…)
    bool supports(BufferId id, const QString& provider) const;
    // textDocument/definition o textDocument/references en esa posición. false si no hay servidor
    bool findLocations(ScintillaEditBase& view, BufferId id, sptr_t pos, const QString& method, LocationsCallback done);
    static std::vector<Location> parseLocations(const QJsonValue& result);
    bool rename(ScintillaEditBase& view, BufferId id, sptr_t pos, const QString& newName, EditCallback done);
    // Da formato al documento (o a la selección) con los cambios del servidor
    bool format(ScintillaEditBase& view, BufferId id);
    // Ayuda de parámetros: tras «(» o «,» (o `ch` 0, a petición); con «)» se cierra
    bool signatureHelp(ScintillaEditBase& view, BufferId id, int ch);
    // Para las pruebas: el último texto de la ayuda y lo resaltado (en bytes)
    QString lastSignatureText() const { return _lastSignatureText; }
    std::pair<int, int> lastSignatureHighlight() const { return _lastSignatureHighlight; }
    // Acciones de código (arreglos, refactorizaciones…) para [start, end); false si no hay servidor
    bool codeActions(ScintillaEditBase& view, BufferId id, sptr_t start, sptr_t end,
                     std::function<void(const QJsonArray& actions, lsp::LspClient* client)> done);
    // Ejecuta una acción (o un Command): su edit, su orden en el servidor o, si no trae
    // nada, lo que diga codeAction/resolve
    void runCodeAction(lsp::LspClient* client, const QJsonObject& action);
    // Símbolos de todo el proyecto que contienen `query`
    bool workspaceSymbols(BufferId id, const QString& query, std::function<void(const std::vector<SymbolHit>& hits)> done);
    // Las pistas (tipos y nombres de parámetros) de esas líneas, ya como texto por línea
    bool inlayHints(ScintillaEditBase& view, BufferId id, int firstLine, int lastLine,
                    std::function<void(const QMap<int, QString>& hints)> done);

    // La codificación de las posiciones de los diagnósticos o del servidor de un archivo
    lsp::PositionEncoding encodingForPath(const QString& path) const;

    void restart(BufferId id);
    QString log(BufferId id) const;
    void shutdownAll();

signals:
    void diagnosticsChanged();
    void statusChanged();
    // workspace/applyEdit del servidor: quien lo atienda contesta reply(aplicado)
    void applyEditRequested(const QJsonObject& edit, edita::lsp::PositionEncoding encoding, const std::function<void(bool)>& reply);

private:
    struct Document
    {
        QString path;
        QString uri;
        QString languageId;
        lsp::LspClient* client = nullptr;
        int version = 0;
        std::uint64_t sentVersion = 0;
    };
    struct CompletionItem
    {
        QJsonObject item;
        lsp::LspClient* client = nullptr;
    };

    lsp::LspClient* clientFor(const lsp::ResolvedServer& server, const QString& root);
    void receiveDiagnostics(const QString& uri, const QJsonArray& diagnostics);
    void showCompletion(ScintillaEditBase& view, lsp::LspClient* client, const QJsonValue& result, sptr_t requestPos);
    void insertCompletion(ScintillaEditBase& view, const CompletionItem& item);
    void registerKindImages(ScintillaEditBase& view);

    DocumentManager& _documentsManager;
    bool _enabled = true;
    QMap<QString, QString> _overrides;
    QStringList _roots;
    std::map<BufferId, Document> _documents;
    std::map<QString, std::unique_ptr<lsp::LspClient>> _clients; // «orden|raíz»
    QMap<QString, std::vector<Diagnostic>> _diagnostics;         // por ruta
    QMap<QString, lsp::PositionEncoding> _diagnosticEncodings;
    QTimer _syncTimer;
    // La lista de autocompletado que se está mostrando
    QPointer<ScintillaEditBase> _completionView;
    QMap<QString, CompletionItem> _completionItems;
    int _completionRequest = 0;
    int _completionToken = 0;
    bool _autoComplete = true;
    int _autoCompleteFrom = 1;
    lsp::LspClient* _completionClient = nullptr;
    int _hoverToken = 0;
    int _signatureToken = 0;
    QPointer<ScintillaEditBase> _signatureView;
    QString _lastSignatureText;
    std::pair<int, int> _lastSignatureHighlight{0, 0};
    QString _lastHoverText;
};

} // namespace edita
