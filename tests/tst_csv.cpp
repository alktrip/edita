// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 99: herramientas de CSV. Colores por columna en los .csv y .tsv, alinear y compactar
// las columnas, y ordenar por la columna del cursor (Editar › CSV).

#include "FakePrompter.h"
#include "core/CsvTable.h"
#include "ui/CsvColumns.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QDir>
#include <QFile>
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

} // namespace

class TestCsv : public QObject
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

    // --- El núcleo -----------------------------------------------------------------------

    void detectsTheDelimiter()
    {
        QCOMPARE(csv::detectDelimiter(QStringLiteral("a,b,c\n1,2,3\n")), QLatin1Char(','));
        QCOMPARE(csv::detectDelimiter(QStringLiteral("a;b;c\n1,5;2;3\n")), QLatin1Char(';')); // la coma decimal no confunde
        QCOMPARE(csv::detectDelimiter(QStringLiteral("a\tb\n1\t2\n")), QLatin1Char('\t'));
        QCOMPARE(csv::detectDelimiter(QStringLiteral("a|b|c\n1|2|3\n")), QLatin1Char('|'));
        QCOMPARE(csv::detectDelimiter(QStringLiteral("hola\nmundo\n")), QLatin1Char(','));
    }

    void splitsFields()
    {
        const QString line = QStringLiteral(R"(a,"b,c",,"d ""q""")");
        QCOMPARE(csv::splitLine(line, QLatin1Char(',')),
                 (std::vector<csv::Span>{{0, 1}, {2, 5}, {8, 0}, {9, 9}}));
        QCOMPARE(csv::columnAt(line, QLatin1Char(','), 0), 0);
        QCOMPARE(csv::columnAt(line, QLatin1Char(','), 4), 1); // la coma dentro de las comillas
        QCOMPARE(csv::columnAt(line, QLatin1Char(','), 7), 1); // al final del campo
        QCOMPARE(csv::columnAt(line, QLatin1Char(','), 8), 2);
        QCOMPARE(csv::columnAt(line, QLatin1Char(','), line.size()), 3);
        // Unas comillas sin cerrar llegan hasta el final
        QCOMPARE(csv::splitLine(QStringLiteral(R"(x,"abierta,y)"), QLatin1Char(',')),
                 (std::vector<csv::Span>{{0, 1}, {2, 10}}));
    }

    void parsesRecordsWithNewlinesInQuotes()
    {
        const auto records = csv::parse(QStringLiteral("a,\"línea 1\nlínea 2\",c\r\n1,2,3\r\n"), QLatin1Char(','));
        QCOMPARE(records.size(), size_t{2});
        QCOMPARE(records[0], QStringList({QStringLiteral("a"), QStringLiteral("\"línea 1\nlínea 2\""), QStringLiteral("c")}));
        QCOMPARE(records[1], QStringList({QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3")}));
        QCOMPARE(csv::lineEnding(QStringLiteral("a\r\nb")), QStringLiteral("\r\n"));
        QCOMPARE(csv::lineEnding(QStringLiteral("a\nb")), QStringLiteral("\n"));
    }

    void alignsAndShrinks()
    {
        const QString text = QStringLiteral("id,name,qty\n1,Ana,10\n22,Bob,3\n");
        const QString aligned = csv::align(text, QLatin1Char(','));
        QCOMPARE(aligned, QStringLiteral("id,name,qty\n1 ,Ana ,10\n22,Bob ,3\n"));
        QCOMPARE(csv::shrink(aligned, QLatin1Char(',')), text);
        // Lo de dentro de unas comillas no se toca; los tabuladores, igual
        QCOMPARE(csv::shrink(QStringLiteral("\" a \" , b \n"), QLatin1Char(',')), QStringLiteral("\" a \",b\n"));
        QCOMPARE(csv::align(QStringLiteral("a\tbb\nccc\td"), QLatin1Char('\t')), QStringLiteral("a  \tbb\nccc\td"));
        // Con un salto de línea dentro de un campo, igual que estaba
        const QString multi = QStringLiteral("a,\"x\ny\"\nb,c\n");
        QCOMPARE(csv::align(multi, QLatin1Char(',')), multi);
    }

    void detectsTheHeader()
    {
        auto records = [](const char* text) { return csv::parse(QString::fromUtf8(text), QLatin1Char(',')); };
        QVERIFY(csv::hasHeader(records("name,qty\nb,10\na,9\n")));
        QVERIFY(!csv::hasHeader(records("b,10\na,9\n")));
        QVERIFY(!csv::hasHeader(records("a,b\nc,d\n"))); // todo texto: no se sabe
        QVERIFY(!csv::hasHeader(records("1,2\n3,4\n")));
    }

    void sortsByColumn()
    {
        const QString text = QStringLiteral("name,qty\nb,10\na,9\nC,100\n");
        QCOMPARE(csv::sortByColumn(text, QLatin1Char(','), 1, false), QStringLiteral("name,qty\na,9\nb,10\nC,100\n"));
        QCOMPARE(csv::sortByColumn(text, QLatin1Char(','), 1, true), QStringLiteral("name,qty\nC,100\nb,10\na,9\n"));
        QCOMPARE(csv::sortByColumn(text, QLatin1Char(','), 0, false), QStringLiteral("name,qty\na,9\nb,10\nC,100\n"));
        // Sin encabezado, con \r\n y sin salto al final
        QCOMPARE(csv::sortByColumn(QStringLiteral("3,x\r\n1,y\r\n2,z"), QLatin1Char(','), 0, false), QStringLiteral("1,y\r\n2,z\r\n3,x"));
        // Una columna que falta en alguna fila: vacía, al principio
        QCOMPARE(csv::sortByColumn(QStringLiteral("b,2\na\nc,1\n"), QLatin1Char(','), 1, false), QStringLiteral("a\nc,1\nb,2\n"));
    }

    // --- En el editor --------------------------------------------------------------------

    void colorsAndCommands()
    {
        const QString data = _tmp.filePath(QStringLiteral("datos.csv"));
        writeFile(data, "name,qty\nb,10\na,9\nC,100\n");
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        w.resize(900, 600);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QVERIFY(w.openFile(data) != kInvalidBuffer);
        // El de la vista principal (cada vista tiene el suyo)
        CsvColumns* columns = nullptr;
        for (CsvColumns* c : w.findChildren<CsvColumns*>()) {
            if (c->parent() == w.editor())
                columns = c;
        }
        QVERIFY(columns);
        // Cada columna, de su color
        QTRY_VERIFY(columns->colorAt(0) >= 0);
        QVERIFY(columns->colorAt(5) >= 0);
        QVERIFY(columns->colorAt(0) != columns->colorAt(5));
        QCOMPARE(columns->colorAt(9), columns->colorAt(0)); // «b», otra vez la primera
        // Los colores se pueden apagar
        QAction* colors = w.findChild<QAction*>(QStringLiteral("actionCsvColors"));
        QVERIFY(colors && colors->isCheckable() && colors->isChecked());
        colors->trigger();
        QTRY_COMPARE(columns->colorAt(0), intptr_t{-1});
        colors->trigger();
        QTRY_VERIFY(columns->colorAt(0) >= 0);

        // Ordenar por la columna del cursor (qty), y deshacer
        EditorView* v = w.editor();
        v->sci(SCI_GOTOPOS, static_cast<uptr_t>(v->sci(SCI_POSITIONFROMLINE, 1) + 3));
        w.findChild<QAction*>(QStringLiteral("actionCsvSortAscending"))->trigger();
        QCOMPARE(QString::fromStdString(v->text()), QStringLiteral("name,qty\na,9\nb,10\nC,100\n"));
        w.findChild<QAction*>(QStringLiteral("actionCsvSortDescending"))->trigger();
        QCOMPARE(QString::fromStdString(v->text()), QStringLiteral("name,qty\nC,100\nb,10\na,9\n"));
        // Alinear y compactar; cada una se deshace de una vez
        w.findChild<QAction*>(QStringLiteral("actionCsvAlign"))->trigger();
        QCOMPARE(QString::fromStdString(v->text()), QStringLiteral("name,qty\nC   ,100\nb   ,10\na   ,9\n"));
        v->sci(SCI_UNDO);
        QCOMPARE(QString::fromStdString(v->text()), QStringLiteral("name,qty\nC,100\nb,10\na,9\n"));
        w.findChild<QAction*>(QStringLiteral("actionCsvAlign"))->trigger();
        w.findChild<QAction*>(QStringLiteral("actionCsvShrink"))->trigger();
        QCOMPARE(QString::fromStdString(v->text()), QStringLiteral("name,qty\nC,100\nb,10\na,9\n"));

        // Un archivo que no es CSV: sin colores
        const QString notes = _tmp.filePath(QStringLiteral("notas.txt"));
        writeFile(notes, "uno,dos\n");
        QVERIFY(w.openFile(notes) != kInvalidBuffer);
        QTest::qWait(200);
        QCOMPARE(columns->colorAt(0), intptr_t{-1});
    }
};

QTEST_MAIN(TestCsv)
#include "tst_csv.moc"
