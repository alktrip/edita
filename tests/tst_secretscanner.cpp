// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 88: el detector de secretos (plugins/SecretScanner; no existe en Notepad++).
//
// Los secretos de las pruebas son falsos y se arman por partes en tiempo de ejecución: si
// estuvieran enteros en el código, la protección de secretos de GitHub podría rechazar el push.

#include "FakePrompter.h"
#include "../plugins/SecretScanner/SecretRules.h"
#include "ui/EditorView.h"
#include "ui/GitCommitPanel.h"
#include "ui/MainWindow.h"
#include "ui/PluginManager.h"

#include <QAction>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

using namespace edita;
using edita::test::FakePrompter;
namespace sec = edita::secrets;

namespace {

// Caracteres «aleatorios» de longitud n (deterministas, con mayúsculas, minúsculas y cifras)
QByteArray body(int n, const char* alphabet = "q7Lm2Xv9Rt4Kw8ZpB3nY6cJ5hD1sGfVuTa0eWiOk")
{
    const QByteArray a(alphabet);
    QByteArray out;
    for (int i = 0; i < n; ++i)
        out += a[(i * 7 + i / 3) % a.size()];
    return out;
}

QByteArray githubToken() { return QByteArray("gh") + "p_" + body(36); }
QByteArray awsKeyId() { return QByteArray("AK") + "IA" + body(16, "QWERTYUIOP0123456789ASDFGHJKLZXCVBNM"); }

QStringList ruleIds(const std::vector<sec::Finding>& found)
{
    QStringList ids;
    for (const sec::Finding& f : found)
        ids << f.ruleId;
    return ids;
}

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QByteArray git(const QString& dir, const QStringList& args)
{
    QProcess p;
    p.setWorkingDirectory(dir);
    p.start(QStringLiteral("git"), args);
    p.waitForFinished(15000);
    return p.readAllStandardOutput();
}

} // namespace

class TestSecretScanner : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;
    FakePrompter* _prompter = nullptr;

    std::unique_ptr<MainWindow> window()
    {
        auto w = std::make_unique<MainWindow>();
        auto fake = std::make_unique<FakePrompter>();
        _prompter = fake.get();
        w->setPrompter(std::move(fake));
        w->notifyPluginsReady();
        return w;
    }
    static const PluginManager::Plugin* plugin(const MainWindow& w)
    {
        for (const auto& p : w.plugins()->plugins()) {
            if (p->name == QLatin1String("Secret Scanner"))
                return p.get();
        }
        return nullptr;
    }
    // Las órdenes por su posición: los nombres van en el idioma de la interfaz
    static bool run(MainWindow& w, int index)
    {
        const PluginManager::Plugin* p = plugin(w);
        return p && index < static_cast<int>(p->commands.size()) && w.plugins()->runCommand(p->commands[static_cast<size_t>(index)].cmdId);
    }
    static QTreeWidget* tree(MainWindow& w) { return w.findChild<QTreeWidget*>(QStringLiteral("secretScannerTree")); }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        qputenv("EDITA_UPDATE_URL", "none");
        QVERIFY2(QFileInfo::exists(EDITA_PLUGIN_BUILD_DIR "/SecretScanner/SecretScanner.so"), "falta el plugin");
        qputenv("EDITA_PLUGIN_PATH", EDITA_PLUGIN_BUILD_DIR);
        qputenv("GIT_CONFIG_GLOBAL", _tmp.filePath("gitconfig").toUtf8());
        qputenv("GIT_CONFIG_NOSYSTEM", "1");
        for (const char* var : {"GIT_AUTHOR_NAME", "GIT_COMMITTER_NAME"})
            qputenv(var, "Prueba");
        for (const char* var : {"GIT_AUTHOR_EMAIL", "GIT_COMMITTER_EMAIL"})
            qputenv(var, "prueba@example.com");
    }

    // --- El motor ------------------------------------------------------------------------

    void detectsKnownFormats_data()
    {
        QTest::addColumn<QByteArray>("text");
        QTest::addColumn<QString>("rule");
        QTest::addColumn<QString>("fileName");
        QTest::newRow("github") << "token: " + githubToken() << "github-token" << QString();
        QTest::newRow("aws id") << "key = " + awsKeyId() << "aws-access-key-id" << QString();
        QTest::newRow("aws secret") << QByteArray("aws_secret_access_key = ") + body(40) << "aws-secret-access-key" << QString();
        QTest::newRow("private key") << QByteArray("-----BEGIN ") + "RSA PRIVATE" + " KEY-----" << "private-key" << QString();
        QTest::newRow("gitlab") << QByteArray("gl") + "pat-" + body(20) << "gitlab-token" << QString();
        QTest::newRow("slack") << QByteArray("xo") + "xb-" + "1234567890-" + body(24) << "slack-token" << QString();
        QTest::newRow("anthropic") << QByteArray("sk-") + "ant-api03-" + body(90) << "anthropic-key" << QString();
        QTest::newRow("openai") << QByteArray("sk-") + "proj-" + body(48) << "openai-key" << QString();
        QTest::newRow("stripe") << QByteArray("sk_") + "live_" + body(24) << "stripe-secret-key" << QString();
        QTest::newRow("google") << QByteArray("AI") + "za" + body(35) << "google-api-key" << QString();
        QTest::newRow("npm") << QByteArray("np") + "m_" + body(36) << "npm-token" << QString();
        QTest::newRow("jwt") << QByteArray("ey") + "J" + body(20) + ".ey" + "J" + body(30) + "." + body(43) << "jwt" << QString();
        QTest::newRow("url password") << QByteArray("postgres://admin:") + body(14) + "@db.example.com/app" << "url-password" << QString();
        QTest::newRow("generic") << QByteArray("api_key = \"") + body(24) + "\"" << "generic-secret" << QString();
        QTest::newRow("generic js") << QByteArray("const authToken = '") + body(32) + "';" << "generic-secret" << QString();
        QTest::newRow(".env") << QByteArray("DB_PASSWORD=") + body(16) << "env-secret" << QStringLiteral(".env");
        QTest::newRow(".env.local") << QByteArray("export STRIPE_SECRET=") + body(20) << "env-secret" << QStringLiteral(".env.local");
    }

    void detectsKnownFormats()
    {
        QFETCH(QByteArray, text);
        QFETCH(QString, rule);
        QFETCH(QString, fileName);
        const auto found = sec::scan(text, fileName);
        QCOMPARE(ruleIds(found), QStringList{rule});
    }

    // Lo que parece un secreto y no lo es
    void ignoresPlaceholders_data()
    {
        QTest::addColumn<QByteArray>("text");
        QTest::addColumn<QString>("fileName");
        QTest::newRow("changeme") << QByteArray("password = \"changeme123\"") << QString();
        QTest::newRow("env var") << QByteArray("password = \"${DB_PASSWORD}\"") << QString();
        QTest::newRow("angle") << QByteArray("api_key = \"<your-api-key>\"") << QString();
        QTest::newRow("repeated") << QByteArray("token = \"aaaaaaaaaaaaaaaa\"") << QString();
        QTest::newRow("low entropy") << QByteArray("secret = \"abababab\"") << QString();
        QTest::newRow("code, not a literal") << QByteArray("password = os.environ[\"DB_PASSWORD\"]") << QString();
        QTest::newRow("url placeholder") << QByteArray("postgres://user:password@localhost/db") << QString();
        QTest::newRow("unquoted outside .env") << QByteArray("DB_PASSWORD=") + body(16) << QStringLiteral("config.txt");
        QTest::newRow("too short") << QByteArray("token = \"Ab3$\"") << QString();
        QTest::newRow("allow marker") << "x = " + githubToken() + "  # edita:allow-secret" << QString();
        QTest::newRow("gitleaks marker") << "x = " + githubToken() + "  // gitleaks:allow" << QString();
    }

    void ignoresPlaceholders()
    {
        QFETCH(QByteArray, text);
        QFETCH(QString, fileName);
        QVERIFY2(sec::scan(text, fileName).empty(), text.constData());
    }

    // Posiciones en bytes UTF-8 (las de Scintilla), línea y columna
    void positions()
    {
        const QByteArray token = githubToken();
        const QByteArray text = "línea uno\r\nñandú = " + token + "\n";
        const auto found = sec::scan(text);
        QCOMPARE(found.size(), std::size_t{1});
        const sec::Finding& f = found.front();
        QCOMPARE(f.line, 1);
        QCOMPARE(f.column, 8); // «ñandú = »: 8 caracteres
        QCOMPARE(text.mid(f.start, f.length), token);
        QCOMPARE(f.secret, QString::fromLatin1(token));
        QCOMPARE(f.fingerprint, sec::fingerprint(QStringLiteral("github-token"), f.secret));
    }

    void maskAndEntropy()
    {
        QCOMPARE(sec::mask(QStringLiteral("abcdefghijkl")), QStringLiteral("abcd…kl"));
        QCOMPARE(sec::mask(QStringLiteral("short")), QStringLiteral("*****"));
        QVERIFY(sec::entropy(QStringLiteral("aaaa")) < 0.01);
        QVERIFY(sec::entropy(QString::fromLatin1(body(32))) > 3.5);
        QVERIFY(sec::looksLikePlaceholder(QStringLiteral("<token>")));
        QVERIFY(!sec::looksLikePlaceholder(QString::fromLatin1(body(20))));
        QVERIFY(sec::rules().size() >= 20);
    }

    // Dos reglas sobre el mismo texto: solo la más concreta
    void oneFindingPerSecret()
    {
        const QByteArray text = QByteArray("ANTHROPIC_API_KEY=\"sk-") + "ant-api03-" + body(90) + "\"";
        QCOMPARE(ruleIds(sec::scan(text)), QStringList{QStringLiteral("anthropic-key")});
    }

    // --- El plugin en Edita ------------------------------------------------------------------

    void marksAndListsTheSecrets()
    {
        const QString path = _tmp.filePath(QStringLiteral("doc/config.py"));
        writeFile(path, "import os\n\nGITHUB = \"" + githubToken() + "\"\nNAME = \"edita\"\n");
        auto w = window();
        QVERIFY(plugin(*w));
        QVERIFY(w->openFile(path) != kInvalidBuffer);
        QVERIFY(run(*w, 1)); // Analizar el documento actual
        QTreeWidget* t = tree(*w);
        QVERIFY(t);
        QCOMPARE(t->topLevelItemCount(), 1);
        QVERIFY(t->topLevelItem(0)->text(0).contains(QLatin1String("3")));
        QVERIFY(t->topLevelItem(0)->text(2).startsWith(QLatin1String("ghp_")));
        QVERIFY(!t->topLevelItem(0)->text(2).contains(QString::fromLatin1(body(36))));
        // La marca en el texto
        const std::string text = w->editor()->text();
        const auto at = static_cast<uptr_t>(text.find("ghp_"));
        QVERIFY(w->editor()->sci(SCI_INDICATORALLONFOR, at + 5) != 0);
        QCOMPARE(w->editor()->sci(SCI_INDICATORALLONFOR, 2), sptr_t{0});
        // Doble clic: el secreto seleccionado
        emit t->itemActivated(t->topLevelItem(0), 0);
        QCOMPARE(static_cast<uptr_t>(w->editor()->sci(SCI_GETSELECTIONSTART)), at);
    }

    // Mientras se escribe (con una pausa) y sin la orden
    void marksWhileTyping()
    {
        const QString path = _tmp.filePath(QStringLiteral("doc/typing.txt"));
        writeFile(path, "nada\n");
        auto w = window();
        QVERIFY(w->openFile(path) != kInvalidBuffer);
        w->editor()->sci(SCI_DOCUMENTEND);
        w->insertText(QString::fromLatin1("clave = " + awsKeyId()));
        const auto at = static_cast<uptr_t>(w->editor()->text().find("AKIA"));
        QTRY_VERIFY(w->editor()->sci(SCI_INDICATORALLONFOR, at + 1) != 0);
    }

    void ignoreIsRemembered()
    {
        const QString path = _tmp.filePath(QStringLiteral("doc/ignore.txt"));
        const QByteArray token = QByteArray("gl") + "pat-" + body(24);
        writeFile(path, "a = " + token + "\n");
        {
            auto w = window();
            QVERIFY(w->openFile(path) != kInvalidBuffer);
            QVERIFY(run(*w, 1));
            QCOMPARE(tree(*w)->topLevelItemCount(), 1);
            w->findChild<QPushButton*>(QStringLiteral("secretScannerIgnore"))->click();
            QCOMPARE(tree(*w)->topLevelItemCount(), 0);
        }
        auto w = window();
        QVERIFY(w->openFile(path) != kInvalidBuffer);
        QVERIFY(run(*w, 1));
        QCOMPARE(tree(*w)->topLevelItemCount(), 0);
    }

    void scansTheProject()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("git")).isEmpty())
            QSKIP("sin git");
        const QString dir = _tmp.filePath(QStringLiteral("project"));
        writeFile(dir + QStringLiteral("/src/app.js"), "const key = '" + QByteArray("AI") + "za" + body(35) + "';\n");
        writeFile(dir + QStringLiteral("/.env"), "SECRET_TOKEN=" + body(18) + "\n");
        writeFile(dir + QStringLiteral("/README.md"), "# nothing here\n");
        writeFile(dir + QStringLiteral("/node_modules/x.js"), "ignored by .gitignore " + githubToken() + "\n");
        writeFile(dir + QStringLiteral("/.gitignore"), "node_modules/\n");
        git(dir, {QStringLiteral("init"), QStringLiteral("-q")});
        auto w = window();
        QVERIFY(w->openFile(dir + QStringLiteral("/README.md")) != kInvalidBuffer);
        QVERIFY(run(*w, 2)); // Analizar el proyecto
        QTreeWidget* t = tree(*w);
        QCOMPARE(t->topLevelItemCount(), 2);
        QStringList where;
        for (int i = 0; i < t->topLevelItemCount(); ++i)
            where << t->topLevelItem(i)->text(0);
        QVERIFY(where.join(QLatin1Char(' ')).contains(QLatin1String("app.js:1")));
        QVERIFY(where.join(QLatin1Char(' ')).contains(QLatin1String(".env:1")));
        // Doble clic: abre el archivo con el secreto seleccionado
        emit t->itemActivated(t->topLevelItem(static_cast<int>(where.indexOf(QRegularExpression(QStringLiteral(".*app\\.js.*"))))), 0);
        QTRY_VERIFY(w->documents().buffer(w->currentBuffer())->filePath().endsWith(QLatin1String("app.js")));
        QVERIFY(w->editor()->sci(SCI_GETSELECTIONEND) > w->editor()->sci(SCI_GETSELECTIONSTART));
    }

    // Antes del commit del panel Git: avisa, y no confirma si se responde que no
    void warnsBeforeCommit()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("git")).isEmpty())
            QSKIP("sin git");
        const QString dir = _tmp.filePath(QStringLiteral("repo"));
        writeFile(dir + QStringLiteral("/README.md"), "hola\n");
        git(dir, {QStringLiteral("init"), QStringLiteral("-q")});
        git(dir, {QStringLiteral("add"), QStringLiteral(".")});
        git(dir, {QStringLiteral("commit"), QStringLiteral("-q"), QStringLiteral("-m"), QStringLiteral("primero")});
        writeFile(dir + QStringLiteral("/settings.py"), "TOKEN = '" + githubToken() + "'\n");
        git(dir, {QStringLiteral("add"), QStringLiteral("settings.py")});
        auto w = window();
        QVERIFY(w->openFile(dir + QStringLiteral("/README.md")) != kInvalidBuffer);
        w->findChild<QAction*>(QStringLiteral("actionGitCommit"))->trigger();
        GitCommitPanel* panel = w->gitCommitPanel();
        QTRY_COMPARE(panel->tree()->topLevelItem(0)->childCount(), 1);
        panel->message()->setPlainText(QStringLiteral("Con un secreto"));
        auto* commit = panel->findChild<QPushButton*>(QStringLiteral("gitCommit"));
        // No: no hay commit
        _prompter->confirmations.enqueue(false);
        commit->click();
        QCOMPARE(_prompter->asked.last(), QStringLiteral("confirm:Commit"));
        QCOMPARE(git(dir, {QStringLiteral("rev-list"), QStringLiteral("--count"), QStringLiteral("HEAD")}).trimmed(), QByteArray("1"));
        // Sí: se confirma igual
        _prompter->confirmations.enqueue(true);
        commit->click();
        QCOMPARE(git(dir, {QStringLiteral("rev-list"), QStringLiteral("--count"), QStringLiteral("HEAD")}).trimmed(), QByteArray("2"));
        // Sin secretos: ni pregunta
        writeFile(dir + QStringLiteral("/notes.txt"), "nada que ver\n");
        git(dir, {QStringLiteral("add"), QStringLiteral("notes.txt")});
        w->refreshGitCommitPanel();
        panel->message()->setPlainText(QStringLiteral("Sin secretos"));
        const int asked = static_cast<int>(_prompter->asked.size());
        commit->click();
        QCOMPARE(static_cast<int>(_prompter->asked.size()), asked);
        QCOMPARE(git(dir, {QStringLiteral("rev-list"), QStringLiteral("--count"), QStringLiteral("HEAD")}).trimmed(), QByteArray("3"));
    }
};

QTEST_MAIN(TestSecretScanner)
#include "tst_secretscanner.moc"
