// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 100: herramientas de JSON y YAML (Editar › JSON/YAML): validar, formatear, compactar,
// convertir entre los dos y la ruta del cursor.

#include "FakePrompter.h"
#include "core/StructuredData.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/NotificationBar.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;
using edita::test::FakePrompter;
using data::Node;

namespace {

void writeFile(const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(bytes);
}

Node json(const QString& text)
{
    data::Error e;
    auto n = data::parseJson(text, &e);
    if (!n)
        qWarning("JSON: %d:%d %s", e.line, e.column, qPrintable(e.message));
    return n.value_or(Node{});
}

Node yaml(const QString& text)
{
    data::Error e;
    auto n = data::parseYaml(text, &e);
    if (!n)
        qWarning("YAML: %d:%d %s", e.line, e.column, qPrintable(e.message));
    return n.value_or(Node{});
}

// Para comparar: el árbol como JSON compacto (sin espacios)
QString compact(const Node& n)
{
    return data::formatJson(n, 0);
}

} // namespace

class TestStructured : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        qputenv("EDITA_UPDATE_URL", "none");
    }

    // --- JSON ----------------------------------------------------------------------------

    void jsonKeepsOrderAndNumbers()
    {
        const QString text = QStringLiteral("{\"z\": 1, \"a\": [1.50, 2e3, -0, true, null], \"s\": \"caf\\u00e9 \\\"x\\\" ñ\", \"o\": {}}");
        const Node n = json(text);
        QCOMPARE(n.kind, Node::Kind::Object);
        QCOMPARE(n.members.front().first, QStringLiteral("z")); // en el orden del texto
        QCOMPARE(compact(n), QStringLiteral("{\"z\":1,\"a\":[1.50,2e3,-0,true,null],\"s\":\"caf\\u00e9 \\\"x\\\" ñ\",\"o\":{}}"));
        QCOMPARE(n.members[2].second.scalar, QStringLiteral("café \"x\" ñ"));
        QCOMPARE(data::formatJson(n, 2), QStringLiteral(
            "{\n  \"z\": 1,\n  \"a\": [\n    1.50,\n    2e3,\n    -0,\n    true,\n    null\n  ],\n"
            "  \"s\": \"caf\\u00e9 \\\"x\\\" ñ\",\n  \"o\": {}\n}\n"));
        QCOMPARE(data::formatJson(json(QStringLiteral("[]")), 4, true), QStringLiteral("[]\n"));
        QCOMPARE(data::formatJson(json(QStringLiteral("{\"a\":[1]}")), 1, true), QStringLiteral("{\n\t\"a\": [\n\t\t1\n\t]\n}\n"));
    }

    void jsonErrorsHaveLineAndColumn()
    {
        struct Case { const char* text; int line; int column; };
        const Case cases[] = {
            {"{\n  \"a\": 1,\n  \"b\" 2\n}", 3, 7},  // falta «:»
            {"[1, 2,]", 1, 7},                       // coma de más
            {"{\"a\": tru}", 1, 7},                  // literal mal escrito
            {"\"abierta", 1, 1},                     // cadena sin cerrar
            {"{\"a\": 1} x", 1, 10},                 // algo después
            {"", 1, 1},                              // vacío
            {"[01]", 1, 2},                          // cero a la izquierda
        };
        for (const Case& c : cases) {
            data::Error e;
            QVERIFY2(!data::parseJson(QString::fromUtf8(c.text), &e), c.text);
            QVERIFY2(e.line == c.line && e.column == c.column,
                     qPrintable(QStringLiteral("%1 → %2:%3 %4").arg(QString::fromUtf8(c.text)).arg(e.line).arg(e.column).arg(e.message)));
            QVERIFY(!e.message.isEmpty());
        }
    }

    // Lo mismo que QJsonDocument para un JSON real (salvo el orden de las claves)
    void jsonAgreesWithQt()
    {
        QFile f(QStringLiteral(EDITA_FIXTURES_DIR "/update/latest.json"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray bytes = f.readAll();
        const Node n = json(QString::fromUtf8(bytes));
        QCOMPARE(QJsonDocument::fromJson(data::formatJson(n, 2).toUtf8()), QJsonDocument::fromJson(bytes));
    }

    // --- YAML ----------------------------------------------------------------------------

    void yamlBlocksAndScalars()
    {
        const QString text = QStringLiteral(
            "# Docker Compose\n"
            "version: \"3.8\"\n"
            "services:\n"
            "  web:\n"
            "    image: nginx:1.27   # comentario\n"
            "    ports:\n"
            "      - \"80:80\"\n"
            "      - 443\n"
            "    enabled: true\n"
            "    replicas: 2\n"
            "    ratio: 0.5\n"
            "    nothing: ~\n"
            "    empty:\n"
            "    env: {DEBUG: 'no', LEVEL: \"a\\tb\"}\n"
            "    tags: [a, b c, 'd, e']\n"
            "list:\n"
            "- name: uno\n"
            "  value: 1\n"
            "- name: dos\n"
            "  value: 2\n"
            "script: |\n"
            "  echo uno\n"
            "  echo dos\n"
            "folded: >-\n"
            "  una\n"
            "  línea\n"
            "url: http://x.org/a#b\n");
        QCOMPARE(compact(yaml(text)), QStringLiteral(
            "{\"version\":\"3.8\",\"services\":{\"web\":{\"image\":\"nginx:1.27\",\"ports\":[\"80:80\",443],\"enabled\":true,\"replicas\":2,"
            "\"ratio\":0.5,\"nothing\":null,\"empty\":null,\"env\":{\"DEBUG\":\"no\",\"LEVEL\":\"a\\tb\"},\"tags\":[\"a\",\"b c\",\"d, e\"]}},"
            "\"list\":[{\"name\":\"uno\",\"value\":1},{\"name\":\"dos\",\"value\":2}],\"script\":\"echo uno\\necho dos\\n\",\"folded\":\"una línea\","
            "\"url\":\"http://x.org/a#b\"}"));
    }

    void yamlAnchorsTagsAndDocumentMarkers()
    {
        const QString text = QStringLiteral(
            "---\n"
            "base: &base\n"
            "  a: 1\n"
            "copy: *base\n"
            "str: !!str 123\n"
            "nested:\n"
            "  - - x\n"
            "    - y\n"
            "  - {}\n"
            "...\n");
        QCOMPARE(compact(yaml(text)), QStringLiteral("{\"base\":{\"a\":1},\"copy\":{\"a\":1},\"str\":\"123\",\"nested\":[[\"x\",\"y\"],{}]}"));
        QCOMPARE(compact(yaml(QStringLiteral("- a\n- b\n"))), QStringLiteral("[\"a\",\"b\"]"));
        QCOMPARE(compact(yaml(QStringLiteral("solo texto\n"))), QStringLiteral("\"solo texto\""));
    }

    void yamlErrorsHaveLineAndColumn()
    {
        struct Case { const char* text; int line; };
        const Case cases[] = {
            {"a: 1\n b: 2\n", 2},             // sangría que no corresponde
            {"a: \"abierta\nb: 1\n", 1},      // comillas sin cerrar
            {"a:\n\t- 1\n", 2},               // tabulador en la sangría
            {"a: [1, 2\nb: 1\n", 1},          // corchete sin cerrar
            {"a: *nadie\n", 1},               // alias sin ancla
            {"a: 1\na: 2\n", 2},              // clave repetida
        };
        for (const Case& c : cases) {
            data::Error e;
            QVERIFY2(!data::parseYaml(QString::fromUtf8(c.text), &e), c.text);
            QVERIFY2(e.line == c.line, qPrintable(QStringLiteral("%1 → %2:%3 %4").arg(QString::fromUtf8(c.text)).arg(e.line).arg(e.column).arg(e.message)));
            QVERIFY(e.column >= 1 && !e.message.isEmpty());
        }
    }

    // De JSON a YAML y de vuelta: el mismo árbol
    void roundTrips()
    {
        const QString text = QStringLiteral(
            "{\"name\":\"Edita\",\"version\":\"1.0\",\"n\":3,\"f\":-1.5e2,\"ok\":false,\"none\":null,\"empty\":\"\",\"list\":[1,\"two\",[],{}],"
            "\"obj\":{\"key: colon\":\"value # hash\",\"multi\":\"line1\\nline2\",\"spaces\":\"  lead\",\"num-like\":\"123\",\"bool-like\":\"yes\"}}");
        const Node original = json(text);
        const QString y = data::toYaml(original);
        const Node back = yaml(y);
        QVERIFY2(compact(back) == compact(original), qPrintable(y));
        QVERIFY(y.contains(QLatin1String("name: Edita\n")));
        QVERIFY(y.contains(QLatin1String("version: \"1.0\"\n"))); // parece número: entre comillas
        QVERIFY(y.contains(QLatin1String("list:\n  - 1\n  - two\n  - []\n  - {}\n")));
    }

    void pathAtPosition()
    {
        const QString text = QStringLiteral("{\"services\": {\"web\": {\"ports\": [80, 443]}}, \"a b\": 1}");
        const Node n = json(text);
        QCOMPARE(data::pathAt(n, text.indexOf(QLatin1String("443"))), QStringLiteral("$.services.web.ports[1]"));
        QCOMPARE(data::pathAt(n, text.indexOf(QLatin1String("\"web\""))), QStringLiteral("$.services.web"));
        QCOMPARE(data::pathAt(n, text.indexOf(QLatin1String("1}"))), QStringLiteral("$['a b']"));
        QCOMPARE(data::pathAt(n, 0), QStringLiteral("$"));
        const QString y = QStringLiteral("services:\n  web:\n    ports:\n      - 80\n      - 443\n");
        QCOMPARE(data::pathAt(yaml(y), y.indexOf(QLatin1String("443"))), QStringLiteral("$.services.web.ports[1]"));
        QCOMPARE(data::pathAt(yaml(y), y.indexOf(QLatin1String("web"))), QStringLiteral("$.services.web"));
    }

    // --- En el editor --------------------------------------------------------------------

    void editorCommands()
    {
        const QString path = _tmp.filePath(QStringLiteral("config.json"));
        writeFile(path, "{\"b\":[1,2],\"a\":{\"x\":true}}");
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        QVERIFY(w.openFile(path) != kInvalidBuffer);
        EditorView* v = w.editor();
        auto text = [v] { return QString::fromStdString(v->text()); };
        // Formatear, con la sangría del documento (tabuladores, como en N++), y deshacer
        w.findChild<QAction*>(QStringLiteral("actionJsonFormat"))->trigger();
        QCOMPARE(text(), QStringLiteral("{\n\t\"b\": [\n\t\t1,\n\t\t2\n\t],\n\t\"a\": {\n\t\t\"x\": true\n\t}\n}\n"));
        w.findChild<QAction*>(QStringLiteral("actionJsonCompact"))->trigger();
        QCOMPARE(text(), QStringLiteral("{\"b\":[1,2],\"a\":{\"x\":true}}"));
        v->sci(SCI_UNDO);
        QVERIFY(text().startsWith(QLatin1String("{\n")));
        // La ruta del cursor, copiada
        v->sci(SCI_GOTOPOS, static_cast<uptr_t>(text().indexOf(QLatin1String("true"))));
        w.findChild<QAction*>(QStringLiteral("actionDataCopyPath"))->trigger();
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("$.a.x"));
        // Validar: bien; con un error, el cursor va a él (un aviso a la vez: el anterior, fuera)
        w.notificationBar()->dismiss();
        w.findChild<QAction*>(QStringLiteral("actionDataValidate"))->trigger();
        QVERIFY2(w.notificationBar()->currentText().contains(QLatin1String("valid JSON")), qPrintable(w.notificationBar()->currentText()));
        v->setText("{\n  \"a\": 1,\n  \"b\" 2\n}");
        w.notificationBar()->dismiss();
        w.findChild<QAction*>(QStringLiteral("actionDataValidate"))->trigger();
        QVERIFY(w.notificationBar()->currentText().contains(QLatin1String("Line 3")));
        QCOMPARE(v->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(v->sci(SCI_GETCURRENTPOS))), sptr_t{2});
        // Convertir a YAML: en un documento nuevo
        v->setText("{\"a\": [1, 2], \"b\": \"x\"}");
        const size_t before = w.documents().count();
        w.findChild<QAction*>(QStringLiteral("actionJsonToYaml"))->trigger();
        QCOMPARE(w.documents().count(), before + 1);
        QCOMPARE(text(), QStringLiteral("a:\n  - 1\n  - 2\nb: x\n"));
        QCOMPARE(w.documents().buffer(w.currentBuffer())->language(), LangType::Yaml);
        // Y de vuelta a JSON
        w.findChild<QAction*>(QStringLiteral("actionYamlToJson"))->trigger();
        QCOMPARE(text(), QStringLiteral("{\n\t\"a\": [\n\t\t1,\n\t\t2\n\t],\n\t\"b\": \"x\"\n}\n"));
        QCOMPARE(w.documents().buffer(w.currentBuffer())->language(), LangType::Json);
    }
};

QTEST_MAIN(TestStructured)
#include "tst_structured.moc"
