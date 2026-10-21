// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 9: imprimir y exportar a PDF (Printer::doPrint de N++ sobre QPagedPaintDevice).

#include "FakePrompter.h"

#include "core/ConfigXml.h"
#include "core/Preferences.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/Printer.h"

#include <QDir>
#include <QFile>
#include <QPdfWriter>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;
using edita::test::FakePrompter;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QByteArray readAll(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// Páginas de un PDF escrito por Qt: los objetos "/Type /Page" (no "/Pages")
int pdfPages(const QString& path)
{
    static const QRegularExpression page(QStringLiteral("/Type\\s*/Page(?!s)"));
    return static_cast<int>(QString::fromLatin1(readAll(path)).count(page));
}

// Texto del PDF con pdftotext (poppler), si está instalado
QString pdfText(const QString& path)
{
    const QString tool = QStandardPaths::findExecutable(QStringLiteral("pdftotext"));
    if (tool.isEmpty())
        return {};
    QProcess process;
    process.start(tool, {QStringLiteral("-layout"), path, QStringLiteral("-")});
    process.waitForFinished(20000);
    return QString::fromUtf8(process.readAllStandardOutput());
}

QByteArray manyLines(int n)
{
    QByteArray out;
    for (int i = 1; i <= n; ++i)
        out += "línea número " + QByteArray::number(i) + "\n";
    return out;
}

struct Window
{
    std::unique_ptr<MainWindow> window = std::make_unique<MainWindow>();
    FakePrompter* prompter;

    Window()
    {
        auto fake = std::make_unique<FakePrompter>();
        prompter = fake.get();
        window->setPrompter(std::move(fake));
    }
    MainWindow& w() const { return *window; }
    EditorView* editor() const { return window->editor(); }
    void setPrefs(const std::function<void(Preferences&)>& change) const
    {
        Preferences p = window->preferences();
        change(p);
        window->setPreferences(p);
    }
};

} // namespace

class TestPrinting : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir _tmp;
    QString file(const QString& name) const { return _tmp.filePath(QStringLiteral("files/") + name); }
    QString configDir() const { return _tmp.filePath(QStringLiteral("config/edita")); }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    void init() { QDir().mkpath(_tmp.filePath("files")); }

    void cleanup()
    {
        QFile::remove(QDir(configDir()).filePath("config.xml"));
        QDir(_tmp.filePath("files")).removeRecursively();
    }

    void headerVariables()
    {
        const QDateTime now(QDate(2026, 10, 20), QTime(9, 5));
        QCOMPARE(expandPrintVariables("$(FILE_NAME) | $(NAME_PART)$(EXT_PART) | $(CURRENT_DIRECTORY)", "/d/sub/doc.tar.gz", now, 1),
                 QStringLiteral("doc.tar.gz | doc.tar.gz | /d/sub"));
        QCOMPARE(expandPrintVariables("-$(CURRENT_PRINTING_PAGE)- $(FULL_CURRENT_PATH)", "/a/b.cpp", now, 7),
                 QStringLiteral("-7- /a/b.cpp"));
        QCOMPARE(expandPrintVariables("$(SHORT_DATE)", "", now, 1), QLocale().toString(now.date(), QLocale::ShortFormat));
        QCOMPARE(expandPrintVariables("$(TIME)", "", now, 1), QLocale().toString(now.time(), QLocale::ShortFormat));
        // Documento sin archivo: el nombre de la pestaña, sin directorio
        QCOMPARE(expandPrintVariables("[$(CURRENT_DIRECTORY)]$(FILE_NAME)", "new 1", now, 1), QStringLiteral("[]new 1"));
    }

    void pagination()
    {
        writeFile(file("largo.txt"), manyLines(300));
        Window x;
        x.w().openFile(file("largo.txt"));
        const int all = x.w().exportToPdf(file("todo.pdf"));
        QVERIFY2(all > 3, qPrintable(QString::number(all)));
        QCOMPARE(pdfPages(file("todo.pdf")), all);
        QVERIFY(readAll(file("todo.pdf")).startsWith("%PDF"));

        // Márgenes mayores → más páginas
        x.setPrefs([](Preferences& p) { p.printMarginTop = p.printMarginBottom = 60; });
        QVERIFY(x.w().exportToPdf(file("margenes.pdf")) > all);

        // Solo la selección
        x.editor()->sci(SCI_SETSEL, 0, 40);
        QCOMPARE(x.w().exportToPdf(file("seleccion.pdf"), true), 1);

        // Un intervalo de páginas
        QPdfWriter writer(file("intervalo.pdf"));
        writer.setResolution(300);
        QCOMPARE(x.w().printCurrent(writer, false, 2, 3), 2);
    }

    void formFeedStartsANewPage()
    {
        writeFile(file("ff.txt"), "primera\n\fsegunda\n\ftercera\n");
        Window x;
        x.w().openFile(file("ff.txt"));
        QCOMPARE(x.w().exportToPdf(file("sin.pdf")), 1);
        x.setPrefs([](Preferences& p) { p.printFormFeedPageBreak = true; });
        QCOMPARE(x.w().exportToPdf(file("con.pdf")), 3);
        const QString text = pdfText(file("con.pdf"));
        if (!text.isEmpty()) {
            const QStringList pages = text.split(QLatin1Char('\f'));
            QVERIFY(pages[0].contains(QStringLiteral("primera")) && !pages[0].contains(QStringLiteral("segunda")));
            QVERIFY(pages[1].contains(QStringLiteral("segunda")));
            QVERIFY(pages[2].contains(QStringLiteral("tercera")));
        }
    }

    void headerFooterAndLineNumbers()
    {
        writeFile(file("cabecera.cpp"), "int main()\n{\n    return 0;\n}\n");
        Window x;
        x.w().openFile(file("cabecera.cpp"));
        x.setPrefs([](Preferences& p) {
            p.printHeaderLeft = "$(FILE_NAME)";
            p.printHeaderRight = "ESQUINA";
            p.printFooterMiddle = "- $(CURRENT_PRINTING_PAGE) -";
            p.printLineNumber = false;
        });
        const sptr_t margin = x.editor()->sci(SCI_GETMARGINWIDTHN, 0);
        QCOMPARE(x.w().exportToPdf(file("cabecera.pdf")), 1);
        // El margen de números de la vista se restaura tras imprimir sin números
        QCOMPARE(x.editor()->sci(SCI_GETMARGINWIDTHN, 0), margin);
        const QString text = pdfText(file("cabecera.pdf"));
        if (text.isEmpty())
            QSKIP("pdftotext no está instalado");
        QVERIFY(text.contains(QStringLiteral("cabecera.cpp")));
        QVERIFY(text.contains(QStringLiteral("ESQUINA")));
        QVERIFY(QRegularExpression(QStringLiteral("-\\s*1\\s*-")).match(text).hasMatch()); // pdftotext junta los espacios
        QVERIFY(text.contains(QStringLiteral("return 0;")));
        QVERIFY(!QRegularExpression(QStringLiteral("^\\s*3\\s+return")).match(text).hasMatch());

        // Con números de línea
        x.setPrefs([](Preferences& p) { p.printLineNumber = true; });
        x.w().exportToPdf(file("numeros.pdf"));
        QVERIFY(QRegularExpression(QStringLiteral("(?m)^\\s*3\\s+return 0;")).match(pdfText(file("numeros.pdf"))).hasMatch());
    }

    void emptyDocumentGivesOnePage()
    {
        Window x;
        QCOMPARE(x.w().exportToPdf(file("vacio.pdf")), 1);
        QCOMPARE(pdfPages(file("vacio.pdf")), 1);
    }

    void exportFromTheMenuAsksForAName()
    {
        writeFile(file("menu.txt"), "hola\n");
        Window x;
        x.w().openFile(file("menu.txt"));
        x.prompter->saveNames.enqueue(file("salida")); // sin extensión: se añade .pdf
        QCOMPARE(x.w().exportToPdf(), 1);
        QCOMPARE(x.prompter->asked.last(), QStringLiteral("saveAs:%1").arg(file("menu.pdf")));
        QVERIFY(QFileInfo::exists(file("salida.pdf")));
        // Cancelar no escribe nada
        x.prompter->saveNames.enqueue(QString());
        QCOMPARE(x.w().exportToPdf(), 0);
    }

    void printSettingsUseNotepadPlusPlusNames()
    {
        Window x;
        x.setPrefs([](Preferences& p) {
            p.printHeaderLeft = "$(FULL_CURRENT_PATH)";
            p.printFooterMiddle = "-$(CURRENT_PRINTING_PAGE)-";
            p.printColourMode = 2;
            p.printMarginLeft = 20;
        });
        const QByteArray xml = readAll(QDir(configDir()).filePath("config.xml"));
        QVERIFY(xml.contains("<GUIConfig name=\"Print\" lineNumber=\"yes\" formFeedPageBreak=\"no\" printOption=\"2\" "
                             "headerLeft=\"$(FULL_CURRENT_PATH)\""));
        QVERIFY(xml.contains("footerMiddle=\"-$(CURRENT_PRINTING_PAGE)-\""));
        QVERIFY(xml.contains("margeLeft=\"20\""));
    }
};

QTEST_MAIN(TestPrinting)
#include "tst_printing.moc"
