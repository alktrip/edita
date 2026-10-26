// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Servidores de lenguaje (no existe en N++): protocolo, posiciones, fragmentos, el
// cliente con un servidor de mentira (fixtures/fake_lsp.py) y, si está, clangd.

#include "FakePrompter.h"
#include "lsp/JsonRpc.h"
#include "lsp/LspClient.h"
#include "lsp/LspServers.h"
#include "syntax/SyntaxStyler.h"
#include "ui/DocStatusBar.h"
#include "ui/EditorView.h"
#include "ui/LspManager.h"
#include "ui/MainWindow.h"
#include "ui/ProblemsPanel.h"
#include "ui/SearchResultsPanel.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

using namespace edita;
using edita::test::FakePrompter;

namespace {

QString fakeServer()
{
    return QStringLiteral("python3 -u " EDITA_FIXTURES_DIR "/fake_lsp.py");
}

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

} // namespace

class TestLsp : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        qputenv("EDITA_LSP", "1"); // CTest lo apaga para las demás pruebas
        if (QStandardPaths::findExecutable("python3").isEmpty())
            QSKIP("Sin python3 no hay servidor de mentira");
    }

    void jsonRpc()
    {
        lsp::JsonRpcStream stream;
        const QByteArray one = lsp::JsonRpcStream::encode(QJsonObject{{"id", 1}, {"result", "ñ"}});
        const QByteArray two = "content-length: 17\r\nContent-Type: x\r\n\r\n{\"method\":\"ping\"}";
        const QByteArray all = one + two;
        // Llega a trozos: nada hasta que hay un mensaje entero
        stream.feed(all.left(10));
        QVERIFY(!stream.next());
        stream.feed(all.mid(10, one.size() - 15));
        QVERIFY(!stream.next());
        stream.feed(all.mid(one.size() - 5));
        auto first = stream.next();
        QVERIFY(first);
        QCOMPARE(first->value("result").toString(), QStringLiteral("ñ"));
        QCOMPARE(first->value("jsonrpc").toString(), QStringLiteral("2.0"));
        auto second = stream.next();
        QVERIFY(second);
        QCOMPARE(second->value("method").toString(), QStringLiteral("ping"));
        QVERIFY(!stream.next());
    }

    void positions()
    {
        // «ñ» son 2 bytes y 1 unidad UTF-16; «😀», 4 bytes y 2 unidades
        const QByteArray line = QStringLiteral("ñandú 😀 x").toUtf8();
        const int x = static_cast<int>(line.indexOf('x'));
        QCOMPARE(lsp::lspCharacter(line, x, lsp::PositionEncoding::Utf16), 9);
        QCOMPARE(lsp::lspCharacter(line, x, lsp::PositionEncoding::Utf8), x);
        QCOMPARE(lsp::byteColumn(line, 9, lsp::PositionEncoding::Utf16), x);
        QCOMPARE(lsp::byteColumn(line, 99, lsp::PositionEncoding::Utf16), static_cast<int>(line.size()));
        QCOMPARE(lsp::byteColumn(line, x, lsp::PositionEncoding::Utf8), x);
        const QString path = QStringLiteral("/tmp/carpeta con espacio/ñ.cpp");
        QCOMPARE(lsp::uriFromPath(path), QStringLiteral("file:///tmp/carpeta%20con%20espacio/%C3%B1.cpp"));
        QCOMPARE(lsp::pathFromUri(lsp::uriFromPath(path)), path);
    }

    void snippets()
    {
        auto check = [](const QString& snippet, const QString& text, int start, int end) {
            const lsp::ExpandedSnippet e = lsp::expandSnippet(snippet);
            QCOMPARE(e.text, text);
            QCOMPARE(e.selectionStart, start);
            QCOMPARE(e.selectionEnd, end);
        };
        check("push_back(${1:const T &x})", "push_back(const T &x)", 10, 20);
        check("${2:b}, ${1:a}$0", "b, a", 3, 4);
        check("foo($0)", "foo()", 4, 4);
        check("\\$5 ${1|uno,dos|} ${TM_FILENAME:def}", "$5 uno def", 3, 6);
        check("if ${1:${2:x} > 0} {\n\t$0\n}", "if x > 0 {\n\t\n}", 3, 8);
        check("plain", "plain", 5, 5);
    }

    void serverSelection()
    {
        QCOMPARE(lsp::languageIdFor(LangType::Cpp, {}, "a.cpp"), QStringLiteral("cpp"));
        QCOMPARE(lsp::languageIdFor(LangType::TypeScript, {}, "a.tsx"), QStringLiteral("typescriptreact"));
        QCOMPARE(lsp::languageIdFor(LangType::User, "Markdown (preinstalled)", "a.md"), QStringLiteral("markdown"));
        QCOMPARE(lsp::languageIdFor(LangType::Text, {}, "a.txt"), QString());
        const lsp::ServerSpec* python = lsp::serverForLanguage("python");
        QVERIFY(python);
        // La orden de la preferencia manda; «off» lo apaga; una que no existe, nada
        auto custom = lsp::resolveServer(*python, {{"python", fakeServer()}});
        QVERIFY(custom);
        QVERIFY(custom->program.endsWith("python3"));
        QCOMPARE(custom->arguments.last(), QStringLiteral(EDITA_FIXTURES_DIR "/fake_lsp.py"));
        QVERIFY(!lsp::resolveServer(*python, {{"python", "off"}}));
        QVERIFY(!lsp::resolveServer(*python, {{"python", "no-existe-este-servidor --stdio"}}));
        const auto overrides = lsp::parseOverrides("python=pylsp\n  clangd = clangd --log=error \nmal");
        QCOMPARE(overrides.value("clangd"), QStringLiteral("clangd --log=error"));
        QCOMPARE(lsp::parseOverrides(lsp::formatOverrides(overrides)), overrides);
        // La raíz: la carpeta de trabajo; si no, la del marcador; si no, la del archivo
        const QString root = _tmp.filePath("raiz");
        writeFile(root + "/proyecto/Cargo.toml", "");
        writeFile(root + "/proyecto/src/main.rs", "");
        const QStringList markers = lsp::serverForLanguage("rust")->rootMarkers;
        QCOMPARE(lsp::findRoot(root + "/proyecto/src/main.rs", {}, markers), QDir::cleanPath(root + "/proyecto"));
        QCOMPARE(lsp::findRoot(root + "/proyecto/src/main.rs", {root}, markers), QDir::cleanPath(root));
        QCOMPARE(lsp::findRoot(root + "/suelto/x.rs", {}, {"no-hay"}), QDir::cleanPath(root + "/suelto"));
    }

    void client()
    {
        auto server = lsp::resolveServer(*lsp::serverForLanguage("python"), {{"python", fakeServer()}});
        QVERIFY(server);
        lsp::LspClient client(server->program, server->arguments, _tmp.path());
        QSignalSpy diagnostics(&client, &lsp::LspClient::diagnosticsPublished);
        client.start();
        // Lo de antes del saludo espera en la cola
        const QString uri = lsp::uriFromPath(_tmp.filePath("a.py"));
        client.notify("textDocument/didOpen", QJsonObject{{"textDocument", QJsonObject{{"uri", uri}, {"languageId", "python"}, {"version", 1}, {"text", "x = error\n"}}}});
        QTRY_COMPARE(client.state(), lsp::LspClient::State::Running);
        QCOMPARE(client.positionEncoding(), lsp::PositionEncoding::Utf16);
        QCOMPARE(client.completionTriggers(), QStringList{"."});
        QVERIFY(client.hasCapability("hoverProvider"));
        QVERIFY(client.hasCapability("definitionProvider"));
        QVERIFY(!client.hasCapability("codeActionProvider"));
        QCOMPARE(client.syncKind(), 1);
        QTRY_COMPARE(diagnostics.count(), 1);
        QCOMPARE(diagnostics.first().at(0).toString(), uri);
        QCOMPARE(diagnostics.first().at(1).toJsonArray().first().toObject().value("message").toString(), QStringLiteral("found error"));
        // Una petición con respuesta y otra que el servidor no conoce
        QJsonValue hover;
        QJsonObject unknownError;
        client.request("textDocument/hover", QJsonObject{{"textDocument", QJsonObject{{"uri", uri}}}, {"position", QJsonObject{{"line", 0}, {"character", 4}}}},
                       [&](const QJsonValue& result, const QJsonObject&) { hover = result; });
        client.request("textDocument/nada", QJsonObject{}, [&](const QJsonValue&, const QJsonObject& error) { unknownError = error; });
        QTRY_VERIFY(!hover.isUndefined() && !unknownError.isEmpty());
        QCOMPARE(hover.toObject().value("contents").toObject().value("value").toString(), QStringLiteral("**fake** hover at 0:4"));
        QCOMPARE(unknownError.value("code").toInt(), -32601);
        QVERIFY(client.log().contains("fake server ready"));
        client.stop();
        QCOMPARE(client.state(), lsp::LspClient::State::Stopped);
    }

    void inTheEditor()
    {
        const QString dir = _tmp.filePath("py");
        const QString file = dir + "/main.py";
        writeFile(file, QStringLiteral("ñandú error aquí\nprint(1)\nwarn x\n").toUtf8());
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        Preferences p = w.preferences();
        p.lspServers = "python=" + fakeServer();
        w.setPreferences(p);
        w.resize(900, 600);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QVERIFY(w.openFile(file) != kInvalidBuffer);
        EditorView* v = w.editor();
        LspManager* lsp = w.lsp();
        QTRY_VERIFY(lsp->isAttached(w.currentBuffer()));
        QTRY_COMPARE(lsp->diagnostics(w.currentBuffer()).size(), std::size_t{2});

        // Subrayados en su sitio: «error» empieza en el carácter 6 (UTF-16), el byte 8
        QTRY_VERIFY(v->sci(SCI_INDICATORVALUEAT, kIndicatorDiagError, 8) != 0);
        QCOMPARE(v->sci(SCI_INDICATORVALUEAT, kIndicatorDiagError, 7), sptr_t{0});
        QVERIFY(v->sci(SCI_INDICATORVALUEAT, kIndicatorDiagError, 12) != 0);
        QCOMPARE(v->sci(SCI_INDICATORVALUEAT, kIndicatorDiagError, 13), sptr_t{0});
        const sptr_t warn = v->sci(SCI_POSITIONFROMLINE, 2);
        QVERIFY(v->sci(SCI_INDICATORVALUEAT, kIndicatorDiagWarning, warn) != 0);
        // En la barra de estado y en Problemas; elegir uno lleva a él
        auto* status = w.findChild<DocStatusBar*>();
        QCOMPARE(status->problemsText(), QStringLiteral("⨯ 1   ⚠ 1"));
        QVERIFY(status->field(DocStatusBar::Field::Problems)->toolTip().contains("python3"));
        QTreeWidget* tree = w.problemsPanel()->tree();
        QCOMPARE(tree->topLevelItemCount(), 1);
        QCOMPARE(tree->topLevelItem(0)->childCount(), 2);
        QVERIFY(tree->topLevelItem(0)->child(0)->text(0).startsWith("found error"));
        QCOMPARE(tree->topLevelItem(0)->child(1)->text(1), QStringLiteral("Ln 3, Col 1"));
        v->sci(SCI_GOTOPOS, 0);
        emit tree->itemActivated(tree->topLevelItem(0)->child(0), 0);
        QCOMPARE(v->sci(SCI_GETCURRENTPOS), sptr_t{8});
        // Al pasar el ratón: el diagnóstico y lo que diga el servidor
        lsp->hover(*v, w.currentBuffer(), 9, QPoint(10, 10));
        QTRY_VERIFY2(lsp->lastHoverText().contains("**fake** hover at 0:7"), qPrintable(lsp->lastHoverText()));
        QVERIFY(lsp->lastHoverText().startsWith("found error"));

        // Al editar se vuelve a enviar el texto: sin «error», sin el diagnóstico
        v->sci(SCI_SETSEL, 8, 13);
        v->sends(SCI_REPLACESEL, 0, "bien");
        QTRY_COMPARE(lsp->count(1), 0);
        QCOMPARE(lsp->count(2), 1);
        QTRY_COMPARE(v->sci(SCI_INDICATORVALUEAT, kIndicatorDiagError, 8), sptr_t{0});
        QTRY_COMPARE(status->problemsText(), QStringLiteral("⨯ 0   ⚠ 1"));

        // Autocompletado al escribir: la lista del servidor, con el fragmento expandido
        v->setFocus();
        const sptr_t end = v->sci(SCI_GETLENGTH);
        v->sci(SCI_GOTOPOS, static_cast<uptr_t>(end));
        QTest::keyClicks(v, "al");
        QTRY_VERIFY(v->sci(SCI_AUTOCACTIVE) != 0);
        QVERIFY(lsp->isCompletionActive(*v));
        v->sci(SCI_AUTOCCOMPLETE); // elige «alpha»
        QCOMPARE(QByteArray::fromStdString(w.documents().text(w.currentBuffer())).mid(end), QByteArray("alpha(x)")); // (en bytes)
        const sptr_t selStart = v->sci(SCI_GETSELECTIONSTART);
        QCOMPARE(v->sci(SCI_GETSELECTIONEND) - selStart, sptr_t{1}); // la «x», seleccionada
        QCOMPARE(selStart, end + 6);
        // Tras «.», lo que se pide con ese carácter
        v->sci(SCI_GOTOPOS, static_cast<uptr_t>(v->sci(SCI_GETLENGTH)));
        QTest::keyClick(v, Qt::Key_Return);
        QTest::keyClicks(v, "obj.");
        QTRY_VERIFY(v->sci(SCI_AUTOCACTIVE) != 0);
        v->sci(SCI_AUTOCCOMPLETE);
        QVERIFY(QString::fromStdString(w.documents().text(w.currentBuffer())).endsWith("obj.member"));
        // Ctrl+Espacio, con textEdit y un cambio adicional al principio (el import)
        QTest::keyClick(v, Qt::Key_Return);
        QTest::keyClicks(v, "Ga");
        QTRY_VERIFY(v->sci(SCI_AUTOCACTIVE) != 0); // (al escribir ya pide la lista)
        v->sci(SCI_AUTOCCANCEL);
        w.findChild<QAction*>("actionCompleteFunction")->trigger();
        QTRY_VERIFY(v->sci(SCI_AUTOCACTIVE) != 0);
        v->sci(SCI_AUTOCSELECT, 0, reinterpret_cast<sptr_t>("Gamma"));
        v->sci(SCI_AUTOCCOMPLETE);
        const QString text = QString::fromStdString(w.documents().text(w.currentBuffer()));
        QVERIFY2(text.startsWith("import gamma\n") && text.endsWith("\nGamma()"), qPrintable(text));
        QCOMPARE(v->sci(SCI_GETCURRENTPOS), v->sci(SCI_GETLENGTH));
        // Deshacer: todo de una vez
        v->sci(SCI_UNDO);
        QVERIFY(QString::fromStdString(w.documents().text(w.currentBuffer())).endsWith("\nGa"));

        // Guardar avisa al servidor (que contesta con una pista)
        w.findChild<QAction*>("actionSave")->trigger();
        QTRY_COMPARE(lsp->count(4), 1);
        // El registro del servidor, en un documento nuevo
        w.findChild<QAction*>("actionLspLog")->trigger();
        QVERIFY(QString::fromStdString(w.documents().text(w.currentBuffer())).contains("fake server ready"));
        // Desactivado en las preferencias: fuera diagnósticos y campo
        p = w.preferences();
        p.lspEnabled = false;
        w.setPreferences(p);
        QCOMPARE(lsp->count(2), 0);
        QVERIFY(status->problemsText().isEmpty());
        QCOMPARE(tree->topLevelItemCount(), 0);
    }

    void navigationAndEdits()
    {
        const QString dir = _tmp.filePath("nav");
        writeFile(dir + "/lib.py", "def helper(n):\n    return n\n");
        writeFile(dir + "/main.py", "from lib import helper\nhelper(1)\nx=helper(2)\n");
        MainWindow w;
        auto prompter = std::make_unique<FakePrompter>();
        FakePrompter* asks = prompter.get();
        w.setPrompter(std::move(prompter));
        Preferences p = w.preferences();
        p.lspEnabled = true;
        p.lspServers = "python=" + fakeServer();
        w.setPreferences(p);
        w.resize(900, 600);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QVERIFY(w.addFolderToWorkspace(dir));
        const BufferId main = w.openFile(dir + "/main.py");
        LspManager* lsp = w.lsp();
        QTRY_VERIFY(lsp->supports(main, "definitionProvider"));
        EditorView* v = w.editor();
        auto text = [&](BufferId id) { return QString::fromStdString(w.documents().text(id)); };

        // F12: a la definición, en otro archivo (que se abre)
        v->sci(SCI_GOTOPOS, static_cast<uptr_t>(v->sci(SCI_POSITIONFROMLINE, 1) + 2));
        w.findChild<QAction*>("actionGoToDefinition")->trigger();
        QTRY_VERIFY(QFileInfo(w.documents().buffer(w.currentBuffer())->filePath()).fileName() == "lib.py");
        const BufferId lib = w.currentBuffer();
        QCOMPARE(w.editor()->sci(SCI_GETCURRENTPOS), sptr_t{4}); // «def |helper»

        // Mayús+F12: las referencias en el panel de resultados, por archivo
        w.findChild<QAction*>("actionFindReferences")->trigger();
        ScintillaEditBase* results = w.resultsPanel()->view();
        auto resultsText = [&] {
            const sptr_t length = results->send(SCI_GETLENGTH);
            QByteArray t(static_cast<qsizetype>(length) + 1, '\0');
            results->send(SCI_GETTEXT, static_cast<uptr_t>(length + 1), reinterpret_cast<sptr_t>(t.data()));
            return QString::fromUtf8(t.left(length));
        };
        QTRY_VERIFY2(resultsText().contains("References to helper"), qPrintable(resultsText()));
        QVERIFY(resultsText().contains("(4 hits)") || resultsText().contains("4 hits"));
        QVERIFY(resultsText().contains("x=helper(2)"));
        QVERIFY(resultsText().contains("lib.py"));

        // Renombrar: los dos archivos (sin cambiar de pestaña), deshacer de una vez por archivo
        w.editor()->sci(SCI_GOTOPOS, 5);
        asks->texts.enqueue("ayudante");
        w.findChild<QAction*>("actionRenameSymbol")->trigger();
        QTRY_VERIFY(text(main).contains("x=ayudante(2)"));
        QCOMPARE(text(main), QStringLiteral("from lib import ayudante\nayudante(1)\nx=ayudante(2)\n"));
        QCOMPARE(text(lib), QStringLiteral("def ayudante(n):\n    return n\n"));
        QCOMPARE(w.currentBuffer(), lib);
        QVERIFY(asks->asked.contains("Rename Symbol"));

        // Dar formato (varios cambios, de atrás hacia delante)
        w.openFile(dir + "/main.py");
        w.editor()->sci(SCI_GOTOPOS, static_cast<uptr_t>(w.editor()->sci(SCI_POSITIONFROMLINE, 2)));
        w.findChild<QAction*>("actionFormatDocument")->trigger();
        QTRY_VERIFY(text(main).contains("x = ayudante(2)"));
        w.editor()->sci(SCI_UNDO);
        QVERIFY(text(main).contains("x=ayudante(2)"));

        // Ayuda de parámetros al escribir «(» y «,»; con «)» se cierra
        v = w.editor();
        v->setFocus();
        v->sci(SCI_GOTOPOS, static_cast<uptr_t>(v->sci(SCI_GETLENGTH)));
        QTest::keyClicks(v, "ayudante(");
        QTRY_VERIFY(v->sci(SCI_CALLTIPACTIVE) != 0);
        QVERIFY2(lsp->lastSignatureText().startsWith("(1/2) ayudante(a, b)\nDoes things."), qPrintable(lsp->lastSignatureText()));
        QCOMPARE(lsp->lastSignatureHighlight(), (std::pair<int, int>{15, 16})); // la «a»
        QTest::keyClicks(v, "1,");
        QTRY_COMPARE(lsp->lastSignatureHighlight(), (std::pair<int, int>{18, 19})); // la «b»
        if (v->sci(SCI_AUTOCACTIVE))
            v->sci(SCI_AUTOCCANCEL);
        QTest::keyClicks(v, "2)");
        QTRY_COMPARE(v->sci(SCI_CALLTIPACTIVE), sptr_t{0});

        // Un WorkspaceEdit con «changes» (como los de workspace/applyEdit)
        const QJsonObject edit{{"changes", QJsonObject{{lsp::uriFromPath(dir + "/lib.py"),
            QJsonArray{QJsonObject{{"range", QJsonObject{{"start", QJsonObject{{"line", 1}, {"character", 11}}},
                                                         {"end", QJsonObject{{"line", 1}, {"character", 12}}}}},
                                   {"newText", "n * 2"}}}}}}};
        QCOMPARE(w.applyWorkspaceEdit(edit, lsp::PositionEncoding::Utf16), 1);
        QCOMPARE(text(lib), QStringLiteral("def ayudante(n):\n    return n * 2\n"));
    }

    void clangd()
    {
        if (QStandardPaths::findExecutable("clangd").isEmpty())
            QSKIP("clangd no está instalado");
        const QString file = _tmp.filePath("cpp/main.cpp");
        writeFile(file, "int doble(int n) { return n * 2; }\nint main() {\n    return doble(x);\n}\n");
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        Preferences p = w.preferences(); // (la prueba anterior lo dejó apagado)
        p.lspEnabled = true;
        p.lspServers.clear();
        w.setPreferences(p);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QVERIFY(w.openFile(file) != kInvalidBuffer);
        LspManager* lsp = w.lsp();
        QTRY_VERIFY_WITH_TIMEOUT(!lsp->diagnostics(w.currentBuffer()).empty(), 30000);
        const auto diags = lsp->diagnostics(w.currentBuffer());
        QVERIFY2(diags.front().message.contains("'x'"), qPrintable(diags.front().message));
        QCOMPARE(diags.front().line, 2);
        QCOMPARE(lsp->clientFor(w.currentBuffer())->positionEncoding(), lsp::PositionEncoding::Utf8);
        // Información de «doble» al pasar el ratón
        lsp->hover(*w.editor(), w.currentBuffer(), w.editor()->sci(SCI_POSITIONFROMLINE, 2) + 12, QPoint(0, 0));
        QTRY_VERIFY_WITH_TIMEOUT(lsp->lastHoverText().contains("doble"), 15000);
        // F12 sobre «doble»: a la línea 1
        EditorView* v = w.editor();
        v->sci(SCI_GOTOPOS, static_cast<uptr_t>(v->sci(SCI_POSITIONFROMLINE, 2) + 12));
        w.findChild<QAction*>("actionGoToDefinition")->trigger();
        QTRY_COMPARE_WITH_TIMEOUT(v->sci(SCI_GETCURRENTPOS), sptr_t{4}, 15000);
        // Formato con clang-format (el de clangd)
        w.findChild<QAction*>("actionFormatDocument")->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(QString::fromStdString(w.documents().text(w.currentBuffer())).contains("int main() { return doble(x); }"), 15000); // (estilo LLVM: funciones cortas en una línea)
    }
};

QTEST_MAIN(TestLsp)
#include "tst_lsp.moc"
