// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 70: las páginas de Preferencias que faltaban (Barra de herramientas, Lenguaje,
// Delimitadores, Rendimiento, Nube y enlaces) y lo que hacen

#include "FakePrompter.h"
#include "core/OnSelection.h"
#include "core/Paths.h"
#include "core/Preferences.h"
#include "doc/Buffer.h"
#include "syntax/SyntaxManager.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/PreferencesDialog.h"
#include "ui/UrlLinks.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;
using edita::test::FakePrompter;

class TestPreferencePages : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;
    QList<QUrl> _opened;

    std::unique_ptr<MainWindow> window()
    {
        auto w = std::make_unique<MainWindow>();
        w->setPrompter(std::make_unique<FakePrompter>());
        _opened.clear();
        w->setUrlOpener([this](const QUrl& url) {
            _opened << url;
            return true;
        });
        w->resize(900, 600);
        w->show();
        QCoreApplication::processEvents();
        return w;
    }

    static QStringList urls(const QString& text, const QString& schemes = {})
    {
        QStringList out;
        for (const auto& [at, length] : findUrls(text, schemes))
            out << text.mid(at, length);
        return out;
    }

    static void setText(MainWindow& w, const char* text) { w.editor()->sends(SCI_SETTEXT, 0, text); }
    static bool hasLink(MainWindow& w, sptr_t pos) { return w.editor()->sci(SCI_INDICATORVALUEAT, kIndicatorUrl, pos) != 0; }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    // isUrl de N++: esquemas, consulta, fragmento y lo que sobra al final
    void detectUrls()
    {
        // Tras un valor entre comillas, la consulta solo sigue con & + = ; (como en N++)
        QCOMPARE(urls(QStringLiteral("ver https://example.org/a?b=1&c='x y'#top.")),
                 QStringList{QStringLiteral("https://example.org/a?b=1&c='x y'")});
        QCOMPARE(urls(QStringLiteral("ver https://example.org/a?b=1&c=2#top.")),
                 QStringList{QStringLiteral("https://example.org/a?b=1&c=2#top")});
        QCOMPARE(urls(QStringLiteral("(http://a.org/x_(y)) y http://b.org/z), fin")),
                 (QStringList{QStringLiteral("http://a.org/x_(y)"), QStringLiteral("http://b.org/z")}));
        QCOMPARE(urls(QStringLiteral("'https://q.org/' y `ftp://f.org/r`")),
                 (QStringList{QStringLiteral("https://q.org/"), QStringLiteral("ftp://f.org/r")}));
        QCOMPARE(urls(QStringLiteral("mailto:ana@example.org, file:///etc/hosts")),
                 (QStringList{QStringLiteral("mailto:ana@example.org"), QStringLiteral("file:///etc/hosts")}));
        QCOMPARE(urls(QStringLiteral("HTTPS://MAYUS.ORG")), QStringList{QStringLiteral("HTTPS://MAYUS.ORG")});
        // Sin servidor, pegado a una palabra o con un esquema que no está: no
        QCOMPARE(urls(QStringLiteral("http:// xhttp://a.org git://repo.org/x")), QStringList{});
        QCOMPARE(urls(QStringLiteral("git://repo.org/x"), QStringLiteral("svn:// git://")), QStringList{QStringLiteral("git://repo.org/x")});
        QCOMPARE(urls(QStringLiteral("¿ñ? https://ñandú.org/é")), QStringList{QStringLiteral("https://ñandú.org/é")});
    }

    // Las URL de lo que se ve llevan URL_INDIC; doble clic las abre; con el modo 0, nada
    void clickableLinks()
    {
        auto w = window();
        setText(*w, "año: https://example.org/x y nada más");
        QTRY_VERIFY(hasLink(*w, 10));
        QVERIFY(!hasLink(*w, 2));
        QCOMPARE(w->editor()->sci(SCI_INDICGETSTYLE, kIndicatorUrl), static_cast<sptr_t>(INDIC_PLAIN)); // subrayado
        QVERIFY(w->openLinkAt(10));
        QCOMPARE(_opened, QList<QUrl>{QUrl(QStringLiteral("https://example.org/x"))});
        QVERIFY(!w->openLinkAt(2));

        Preferences p = w->preferences();
        p.urlMode = 3; // sin subrayar, con recuadro al pasar el ratón
        w->setPreferences(p);
        QCOMPARE(w->editor()->sci(SCI_INDICGETSTYLE, kIndicatorUrl), static_cast<sptr_t>(INDIC_HIDDEN));
        QCOMPARE(w->editor()->sci(SCI_INDICGETHOVERSTYLE, kIndicatorUrl), static_cast<sptr_t>(INDIC_FULLBOX));
        p.urlMode = 0;
        w->setPreferences(p);
        QVERIFY(!hasLink(*w, 10));
        QVERIFY(!w->openLinkAt(10));
    }

    // Ctrl + doble clic: entre los delimitadores (los de serie, paréntesis; o comillas)
    void delimiters()
    {
        QCOMPARE(delimiterRange("f(a, g(b), c)", 5, '(', ')'), (std::pair<qsizetype, qsizetype>{1, 12}));
        QCOMPARE(delimiterRange("f(a, g(b), c)", 7, '(', ')'), (std::pair<qsizetype, qsizetype>{6, 8}));
        QCOMPARE(delimiterRange("sin nada", 3, '(', ')'), (std::pair<qsizetype, qsizetype>{-1, -1}));
        QCOMPARE(delimiterRange("x = \"a \\\"b\\\" c\";", 8, '"', '"'), (std::pair<qsizetype, qsizetype>{4, 14}));

        auto w = window();
        setText(*w, "uno (dos\ntres) cuatro (cinco)");
        QVERIFY(w->selectDelimitedAt(24));
        QCOMPARE(QString::fromStdString(w->editor()->text().substr(23, 5)), QStringLiteral("cinco"));
        QCOMPARE(w->editor()->sci(SCI_GETSELECTIONSTART), 23);
        QCOMPARE(w->editor()->sci(SCI_GETSELECTIONEND), 28);
        QVERIFY(!w->selectDelimitedAt(6)); // en otra línea: con «Varias líneas»
        Preferences p = w->preferences();
        p.delimiterSeveralLines = true;
        w->setPreferences(p);
        QVERIFY(w->selectDelimitedAt(6));
        QCOMPARE(w->editor()->sci(SCI_GETSELECTIONSTART), 5);
        QCOMPARE(w->editor()->sci(SCI_GETSELECTIONEND), 13);
    }

    // Los caracteres que además son de palabra (para el doble clic, la búsqueda de palabra…)
    void wordCharacters()
    {
        auto w = window();
        setText(*w, "uno-dos ñandú");
        EditorView* v = w->editor();
        QCOMPARE(v->sci(SCI_WORDENDPOSITION, 0, 1), 3);
        QCOMPARE(v->sci(SCI_WORDENDPOSITION, 8, 1), 15); // ñandú: lo que no es ASCII es de palabra
        Preferences p = w->preferences();
        p.wordCharsDefault = false;
        p.wordCharsAdded = QStringLiteral("-");
        w->setPreferences(p);
        QCOMPARE(v->sci(SCI_WORDENDPOSITION, 0, 1), 7);
        // Otro documento también (son del documento, no de la vista)
        w->newDocument();
        setText(*w, "a-b");
        QCOMPARE(w->editor()->sci(SCI_WORDENDPOSITION, 0, 1), 3);
        p.wordCharsDefault = true;
        w->setPreferences(p);
        QCOMPARE(w->editor()->sci(SCI_WORDENDPOSITION, 0, 1), 1);
    }

    // El menú Lenguaje: compacto o no, sin los ocultos
    void languageMenu()
    {
        auto w = window();
        auto findAction = [&w](int lang) { return w->findChild<QAction*>(QStringLiteral("actionLang%1").arg(lang)); };
        QMenu* menu = qobject_cast<QMenu*>(findAction(0)->parent());
        QVERIFY(menu);
        QVERIFY(qobject_cast<QMenu*>(findAction(22)->parent()) != menu); // Python, en «P»
        Preferences p = w->preferences();
        p.langMenuCompact = false;
        p.excludedLanguages = {22};
        w->setPreferences(p);
        QVERIFY(!findAction(22));
        QAction* cpp = findAction(3);
        QVERIFY(cpp);
        QCOMPARE(qobject_cast<QMenu*>(cpp->parent()), menu); // sin letras
        QVERIFY(w->findChild<QMenu*>(QStringLiteral("menuUserLanguages")));
        // Sigue marcando el lenguaje del documento
        w->editor()->sci(SCI_CLEARALL);
        cpp->trigger();
        QVERIFY(cpp->isChecked());
        p.langMenuCompact = true;
        p.excludedLanguages.clear();
        w->setPreferences(p);
        QVERIFY(findAction(22));
        QVERIFY(findAction(3)->isChecked());
    }

    void sqlBackslash()
    {
        auto w = window();
        QVERIFY(w->syntax().options().sqlBackslashEscapes);
        Preferences p = w->preferences();
        p.sqlBackslashEscapes = false;
        w->setPreferences(p);
        QVERIFY(!w->syntax().options().sqlBackslashEscapes);
    }

    // Rendimiento: desde qué tamaño un archivo es grande y qué se apaga
    void largeFiles()
    {
        const QString path = _tmp.filePath(QStringLiteral("grande.c"));
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            QByteArray line = "int f(int a) { return a; } // https://example.org/x\n";
            for (int i = 0; i < 25000; ++i)
                f.write(line); // ~1,3 MB
        }
        auto w = window();
        Preferences p = w->preferences();
        p.largeFileSizeMB = 1;
        p.wrap = true;
        p.urlMode = 2; // las pruebas anteriores lo dejan guardado en config.xml
        w->setPreferences(p);
        QCOMPARE(w->documents().settings().largeFileThreshold, 1024LL * 1024);
        const BufferId id = w->openFile(path);
        QVERIFY(w->documents().buffer(id)->isLargeFile());
        QTRY_VERIFY(!w->preferences().wrap); // «Desactivar el ajuste de línea»
        EditorView* v = w->editor();
        v->sci(SCI_GOTOPOS, 5); // junto a «(»
        v->matchBraces();
        QVERIFY(!hasLink(*w, 35));
        // Con todo permitido, como en un archivo normal
        p = w->preferences();
        p.largeFileBraceMatch = true;
        p.largeFileClickableLink = true;
        w->setPreferences(p);
        QTRY_VERIFY(hasLink(*w, 35));
        // Sin la restricción, ningún archivo es grande
        p.largeFileRestriction = false;
        w->setPreferences(p);
        QCOMPARE(w->documents().settings().largeFileThreshold, std::numeric_limits<std::int64_t>::max());
    }

    // Las páginas del diálogo
    void dialogPages()
    {
        Preferences p;
        p.toolbarVisible = false;
        p.toolbarIcons = ToolbarIcons::Large;
        p.excludedLanguages = {22};
        PreferencesDialog dialog(p, {{0, QStringLiteral("Normal text")}, {3, QStringLiteral("C++")}, {22, QStringLiteral("Python")}});
        QVERIFY(dialog.findChild<QCheckBox*>(QStringLiteral("toolbarHide"))->isChecked());
        QVERIFY(!dialog.findChild<QComboBox*>(QStringLiteral("toolbar"))->isEnabled());
        auto* available = dialog.findChild<QListWidget*>(QStringLiteral("langAvailable"));
        auto* excluded = dialog.findChild<QListWidget*>(QStringLiteral("langExcluded"));
        QCOMPARE(available->count(), 1); // «Normal text» no se puede ocultar
        QCOMPARE(excluded->count(), 1);
        available->item(0)->setSelected(true);
        dialog.findChild<QPushButton*>(QStringLiteral("langHide"))->click();
        QCOMPARE(dialog.preferences().excludedLanguages, (QList<int>{3, 22}));
        // Los caracteres de palabra: sin espacios ni repetidos; vacíos, los de serie
        dialog.findChild<QCheckBox*>(QStringLiteral("wordCharsCustom"))->setChecked(true);
        dialog.findChild<QLineEdit*>(QStringLiteral("wordCharsAdded"))->setText(QStringLiteral("- $-"));
        QCOMPARE(dialog.preferences().wordCharsAdded, QStringLiteral("-$"));
        QVERIFY(!dialog.preferences().wordCharsDefault);
        dialog.findChild<QLineEdit*>(QStringLiteral("wordCharsAdded"))->setText(QString());
        QVERIFY(dialog.preferences().wordCharsDefault);
        // Un delimitador vacío vuelve al de serie
        dialog.findChild<QLineEdit*>(QStringLiteral("leftDelimiter"))->setText(QString());
        QCOMPARE(dialog.preferences().leftDelimiter, QLatin1Char('('));
        // Enlaces: las casillas de N++ y su urlMode
        auto* links = dialog.findChild<QCheckBox*>(QStringLiteral("clickableLinks"));
        auto* noUnderline = dialog.findChild<QCheckBox*>(QStringLiteral("linksNoUnderline"));
        auto* fullBox = dialog.findChild<QCheckBox*>(QStringLiteral("linksFullBox"));
        QCOMPARE(dialog.preferences().urlMode, 2);
        noUnderline->setChecked(true);
        QCOMPARE(dialog.preferences().urlMode, 1);
        fullBox->setChecked(true);
        QCOMPARE(dialog.preferences().urlMode, 3);
        noUnderline->setChecked(false);
        QCOMPARE(dialog.preferences().urlMode, 4);
        links->setChecked(false);
        QCOMPARE(dialog.preferences().urlMode, 0);
        QVERIFY(!fullBox->isEnabled());
        // La nube no es de config.xml
        dialog.setCloudFolder(_tmp.path());
        QCOMPARE(dialog.cloudFolder(), _tmp.path());
        dialog.findChild<QCheckBox*>(QStringLiteral("cloud"))->setChecked(false);
        QCOMPARE(dialog.cloudFolder(), QString());
    }

    // La configuración en otra carpeta (cloud/choice): se copia la de ahora y vale al reiniciar
    void settingsOnCloud()
    {
        QTemporaryDir base;
        QTemporaryDir cloud;
        const QByteArray previous = qgetenv("XDG_CONFIG_HOME");
        qputenv("XDG_CONFIG_HOME", base.path().toUtf8());
        const QString local = base.filePath(QStringLiteral("edita"));
        QDir().mkpath(local + QStringLiteral("/userDefineLangs"));
        for (const char* name : {"config.xml", "shortcuts.xml", "userDefineLangs/Mio.xml"}) {
            QFile f(local + QLatin1Char('/') + QLatin1String(name));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("<x/>");
        }
        QCOMPARE(paths::configDir(), local);
        QString error;
        QVERIFY(!paths::setCloudChoice(cloud.filePath(QStringLiteral("no-existe")), &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(paths::setCloudChoice(cloud.path(), &error));
        QCOMPARE(paths::cloudChoice(), QDir::cleanPath(cloud.path()));
        QVERIFY(QFile::exists(cloud.filePath(QStringLiteral("config.xml"))));
        QVERIFY(QFile::exists(cloud.filePath(QStringLiteral("userDefineLangs/Mio.xml"))));
        QCOMPARE(paths::configDir(), local); // hasta reiniciar
        QCOMPARE(paths::baseConfigDir(), local);
        // «Al reiniciar»: otra vez desde la carpeta base
        qputenv("XDG_CONFIG_HOME", _tmp.path().toUtf8());
        QCOMPARE(paths::configDir(), _tmp.filePath(QStringLiteral("edita")));
        qputenv("XDG_CONFIG_HOME", base.path().toUtf8());
        QCOMPARE(paths::configDir(), QDir::cleanPath(cloud.path()));
        QVERIFY(paths::setCloudChoice(QString()));
        QCOMPARE(paths::cloudChoice(), QString());
        qputenv("XDG_CONFIG_HOME", previous);
    }
};

QTEST_MAIN(TestPreferencePages)
#include "tst_preferencepages.moc"
