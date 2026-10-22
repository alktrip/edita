// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Plugin MarkdownViewer++ (plugins/MarkdownViewerPlusPlus): el port del plugin de N++.

#include "FakePrompter.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/PluginManager.h"

#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QMenuBar>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>

using namespace edita;
using edita::test::FakePrompter;

namespace {

struct Window
{
    std::unique_ptr<MainWindow> window = std::make_unique<MainWindow>();

    Window()
    {
        window->setPrompter(std::make_unique<FakePrompter>());
        window->notifyPluginsReady();
    }
    const PluginManager::Plugin* plugin() const
    {
        for (const auto& p : window->plugins()->plugins()) {
            if (p->name == QLatin1String("MarkdownViewer++"))
                return p.get();
        }
        return nullptr;
    }
    const PluginManager::Command* command(const QString& name) const
    {
        for (const PluginManager::Command& c : plugin()->commands) {
            if (c.name == name)
                return &c;
        }
        return nullptr;
    }
    bool run(const QString& name) const
    {
        const PluginManager::Command* c = command(name);
        return c && window->plugins()->runCommand(c->cmdId);
    }
    EditorView* editor() const { return window->editor(); }
    QTextBrowser* browser() const { return window->findChild<QTextBrowser*>(QStringLiteral("MarkdownViewerBrowser")); }
    // NPPM_MSGTOPLUGIN, como lo mandaría otro plugin o un script
    bool exportTo(long message, const QString& path) const
    {
        const QByteArray file = path.toUtf8();
        EditaCommunicationInfo info{message, "tst.so", const_cast<char*>(file.constData())};
        return window->handlePluginMessage(NPPM_MSGTOPLUGIN, reinterpret_cast<uintptr_t>("MarkdownViewerPlusPlus.so"),
                                           reinterpret_cast<intptr_t>(&info))
            != 0;
    }
};

QByteArray readFile(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

const QByteArray kMarkdown = "# Título\n\nTexto con **negrita** y `código`.\n\n"
                             "| a | b |\n|---|---|\n| 1 | 2 |\n\n- [x] hecho\n- [ ] pendiente\n\n~~tachado~~\n";

} // namespace

class TestMarkdownViewer : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

    QString ini() const { return _tmp.filePath("config/edita/plugins/config/MarkdownViewerPlusPlus.ini"); }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        QVERIFY2(QFileInfo::exists(EDITA_PLUGIN_BUILD_DIR "/MarkdownViewerPlusPlus/MarkdownViewerPlusPlus.so"), "falta el plugin");
        qputenv("EDITA_PLUGIN_PATH", EDITA_PLUGIN_BUILD_DIR);
    }

    void cleanup()
    {
        QFile::remove(ini());
    }

    void menuOfTheOriginal()
    {
        Window w;
        QVERIFY2(w.plugin(), qPrintable(w.window->pluginWarnings().join('\n')));
        QCOMPARE(w.plugin()->version, QStringLiteral("0.8.2"));
        // CommandMenuInit: el panel (Ctrl+Mayús+M), sincronizar, opciones y acerca de
        QStringList names;
        for (const PluginManager::Command& c : w.plugin()->commands)
            names << (c.function ? c.name : QStringLiteral("---"));
        QCOMPARE(names, (QStringList{"MarkdownViewer++", "---", "Synchronize scrolling (Editor -> Viewer)", "---", "Options", "About"}));
        QCOMPARE(w.command("MarkdownViewer++")->shortcut, QKeySequence(QStringLiteral("Ctrl+Shift+M")));
        QVERIFY(!w.command("Synchronize scrolling (Editor -> Viewer)")->checked);
    }

    void panelRendersTheDocument()
    {
        Window w;
        w.window->show();
        QVERIFY(QTest::qWaitForWindowExposed(w.window.get()));
        w.editor()->sends(SCI_SETTEXT, 0, kMarkdown.constData());
        QVERIFY(!w.browser());
        QVERIFY(w.run("MarkdownViewer++"));
        QTextBrowser* browser = w.browser();
        QVERIFY(browser);
        auto* dock = qobject_cast<QDockWidget*>(browser->parentWidget()->parentWidget());
        QVERIFY(dock);
        QVERIFY(dock->isVisible());
        QCOMPARE(dock->windowTitle(), QStringLiteral("MarkdownViewer++"));
        QVERIFY(w.command("MarkdownViewer++")->checked);
        // La barra de menús del panel (AbstractRenderer)
        auto* menu = browser->parentWidget()->findChild<QMenuBar*>();
        QVERIFY(menu);
        QCOMPARE(menu->actions().size(), 4);
        // Encabezado, negrita, tabla, lista de tareas (☑/☐) y tachado
        const QString text = browser->toPlainText();
        QVERIFY2(text.startsWith(QStringLiteral("Título")), qPrintable(text));
        QVERIFY(text.contains(QStringLiteral("negrita")));
        QVERIFY(text.contains(QStringLiteral("☑ hecho")));
        QVERIFY(text.contains(QStringLiteral("☐ pendiente")));
        QVERIFY(browser->toHtml().contains(QStringLiteral("font-weight:700")));
        QVERIFY(browser->document()->toMarkdown().contains(QStringLiteral("|1|2|")) || browser->toHtml().contains(QStringLiteral("<table")));

        // Al escribir, se vuelve a dibujar (tras una pausa breve)
        w.editor()->sends(SCI_APPENDTEXT, 9, "\n## Nuevo");
        QTRY_VERIFY(browser->toPlainText().contains(QStringLiteral("Nuevo")));

        // Otra vez la orden: se oculta y el menú se desmarca
        QVERIFY(w.run("MarkdownViewer++"));
        QVERIFY(!dock->isVisible());
        QVERIFY(!w.command("MarkdownViewer++")->checked);
        // Mostrar y cerrar con la X del panel: también se desmarca
        QVERIFY(w.run("MarkdownViewer++"));
        QVERIFY(w.command("MarkdownViewer++")->checked);
        dock->close();
        QVERIFY(!w.command("MarkdownViewer++")->checked);
    }

    void fileExtensionsAreFiltered()
    {
        // ValidateFileExtension: solo las extensiones configuradas; los nuevos, con inclNewFiles
        writeFile(ini(), "[MarkdownViewerPlusPlus]\nfileExtensions=md,markdown\ninclNewFiles=0\n");
        const QString path = _tmp.filePath("files/notas.txt");
        writeFile(path, "# No es Markdown");
        Window w;
        QVERIFY(w.window->openFile(path) != kInvalidBuffer);
        QVERIFY(w.run("MarkdownViewer++"));
        QVERIFY2(w.browser()->toPlainText().contains(QStringLiteral("do not include the currently selected file extension")),
                 qPrintable(w.browser()->toPlainText()));
        QVERIFY(w.browser()->toPlainText().contains(QStringLiteral("notas.txt")));
        const QString md = _tmp.filePath("files/notas.md");
        writeFile(md, "# Sí es Markdown");
        QVERIFY(w.window->openFile(md) != kInvalidBuffer);
        QTRY_VERIFY(w.browser()->toPlainText().startsWith(QStringLiteral("Sí es Markdown")));
    }

    void exportsHtmlAndPdf()
    {
        // Página 1/2 de márgenes y CSS propio
        writeFile(ini(), "[MarkdownViewerPlusPlus]\nhtmlCssStyle=@import url(x.css); \\n body { color: red; }\npdfPageSize=Letter\n"
                         "pdfOrientation=Landscape\nmargins=10,10,10,10\n");
        const QString md = _tmp.filePath("files/doc.md");
        writeFile(md, kMarkdown);
        Window w;
        QVERIFY(w.window->openFile(md) != kInvalidBuffer);
        const QString html = _tmp.filePath("out/doc.html");
        QDir().mkpath(_tmp.filePath("out"));
        QVERIFY(w.exportTo(1, html));
        const QByteArray page = readFile(html);
        QVERIFY(page.startsWith("<!DOCTYPE html>"));
        QVERIFY(page.contains("<title>doc.md</title>"));
        QVERIFY(page.contains("<h1>Título</h1>"));
        QVERIFY(page.contains("<strong>negrita</strong>"));
        QVERIFY(page.contains("<table>"));
        QVERIFY2(page.contains("<del>tachado</del>"), page.constData());
        QVERIFY(page.contains("type=\"checkbox\""));
        // getCSS: los @import primero, después el CSS de serie y después el propio
        const qsizetype import = page.indexOf("@import url(x.css);");
        const qsizetype base = page.indexOf("page-break-inside: avoid");
        const qsizetype custom = page.indexOf("body { color: red; }");
        QVERIFY(import >= 0 && base > import && custom > base);

        const QString pdf = _tmp.filePath("out/doc.pdf");
        QVERIFY(w.exportTo(2, pdf));
        const QByteArray bytes = readFile(pdf);
        QVERIFY(bytes.startsWith("%PDF"));
        // Carta apaisada: 792 × 612 puntos
        const QRegularExpressionMatch box =
            QRegularExpression(QStringLiteral("/MediaBox\\s*\\[\\s*0\\s+0\\s+([0-9.]+)\\s+([0-9.]+)")).match(QString::fromLatin1(bytes));
        QVERIFY(box.hasMatch());
        QCOMPARE(qRound(box.captured(1).toDouble()), 792);
        QCOMPARE(qRound(box.captured(2).toDouble()), 612);
    }

    void synchronizeScrollingIsSavedAndFollowsTheEditor()
    {
        Window w;
        QByteArray text;
        for (int i = 0; i < 400; ++i)
            text += "Línea " + QByteArray::number(i) + "\n\n";
        w.editor()->sends(SCI_SETTEXT, 0, text.constData());
        QVERIFY(w.run("Synchronize scrolling (Editor -> Viewer)"));
        QVERIFY(w.command("Synchronize scrolling (Editor -> Viewer)")->checked);
        QVERIFY(readFile(ini()).contains("[MarkdownViewerPlusPlus]\nsynchronizeScrolling=1\n"));
        w.window->resize(900, 600);
        w.window->show();
        QVERIFY(QTest::qWaitForWindowExposed(w.window.get()));
        QVERIFY(w.run("MarkdownViewer++"));
        QScrollBar* bar = w.browser()->verticalScrollBar();
        QTRY_VERIFY(bar->maximum() > 0);
        // El editor al final: el panel también
        w.editor()->sci(SCI_DOCUMENTEND);
        w.editor()->sci(SCI_SCROLLCARET);
        QTRY_VERIFY(bar->value() > bar->maximum() * 9 / 10);
        w.editor()->sci(SCI_DOCUMENTSTART);
        QTRY_COMPARE(bar->value(), 0);
        // Otra ventana lee la opción guardada
        Window again;
        QVERIFY(again.command("Synchronize scrolling (Editor -> Viewer)")->checked);
    }
};

QTEST_MAIN(TestMarkdownViewer)
#include "tst_markdownviewer.moc"
