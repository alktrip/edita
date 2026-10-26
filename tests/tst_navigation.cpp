// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ruta de navegación y encabezado fijo (no existen en N++).

#include "FakePrompter.h"
#include "ui/Breadcrumbs.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/QuickPicker.h"
#include "ui/StickyScroll.h"

#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QMouseEvent>
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

// Una clase con dos métodos y una función larga
QByteArray cppSource()
{
    QByteArray text = "class Pila\n{\npublic:\n    void push(int v)\n    {\n        _v = v;\n    }\n"
                      "    int pop()\n    {\n        return _v;\n    }\nprivate:\n    int _v = 0;\n};\n\n"
                      "int larga(int n)\n{\n    int total = 0;\n    for (int i = 0; i < n; ++i) {\n";
    for (int i = 0; i < 60; ++i)
        text += "        total += i * " + QByteArray::number(i) + ";\n";
    text += "    }\n    return total;\n}\n";
    return text;
}

int lineOf(const QByteArray& text, const char* needle)
{
    return static_cast<int>(text.left(text.indexOf(needle)).count('\n'));
}

} // namespace

class TestNavigation : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    void breadcrumbsFollowTheCaret()
    {
        const QString root = _tmp.filePath("proyecto");
        const QByteArray source = cppSource();
        writeFile(root + "/src/pila.cpp", source);
        writeFile(root + "/src/otro.cpp", "int x;\n");
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        QVERIFY(w.addFolderToWorkspace(root));
        QVERIFY(w.openFile(root + "/src/pila.cpp") != kInvalidBuffer);
        Breadcrumbs* crumbs = w.breadcrumbsOfCurrentView();
        QVERIFY(crumbs);
        // Desde la carpeta de trabajo, el archivo y, dentro de un método, la clase y el método
        w.goToLine(lineOf(source, "return _v;") + 1);
        crumbs->setCaretLine(lineOf(source, "return _v;"));
        QVERIFY2(crumbs->segments() == (QStringList{"proyecto", "src", "pila.cpp", "Pila", "pop"}), qPrintable(crumbs->segments().join(QStringLiteral(" | "))));
        crumbs->setCaretLine(lineOf(source, "total += i * 30"));
        QCOMPARE(crumbs->segments(), (QStringList{"proyecto", "src", "pila.cpp", "larga"}));
        crumbs->setCaretLine(lineOf(source, "\n\nint larga") + 1); // entre los dos: ninguno
        QCOMPARE(crumbs->segments(), (QStringList{"proyecto", "src", "pila.cpp"}));

        // El archivo: la lista de símbolos (con buscador); elegir uno va a su línea
        QVERIFY(crumbs->clickSegment(2));
        QuickPicker* picker = crumbs->picker();
        QVERIFY(picker && picker->isVisible());
        QVERIFY(picker->visibleTexts().contains(QStringLiteral("push")));
        picker->input()->setText(QStringLiteral("larg"));
        QVERIFY(picker->activate(QStringLiteral("larga")));
        QCOMPARE(w.editor()->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(w.editor()->sci(SCI_GETCURRENTPOS))),
                 sptr_t{lineOf(source, "int larga")});
        // Una carpeta: sus archivos; elegir uno lo abre
        QVERIFY(crumbs->clickSegment(1));
        QVERIFY(picker->visibleTexts().contains(QStringLiteral("otro.cpp")));
        QVERIFY(picker->activate(QStringLiteral("otro.cpp")));
        QCOMPARE(QFileInfo(w.documents().buffer(w.currentBuffer())->filePath()).fileName(), QStringLiteral("otro.cpp"));

        // Sin analizador de funciones (JSON): los bloques plegables, con su primera línea
        writeFile(root + "/datos.json", "{\n  \"a\": {\n    \"b\": [\n      1\n    ]\n  }\n}\n");
        QVERIFY(w.openFile(root + "/datos.json") != kInvalidBuffer);
        crumbs->setCaretLine(3);
        QCOMPARE(crumbs->segments(), (QStringList{"proyecto", "datos.json", "{", "\"a\": {", "\"b\": ["}));
        // Desactivada
        Preferences p = w.preferences();
        p.breadcrumbs = false;
        w.setPreferences(p);
        QVERIFY(crumbs->isHidden());
    }

    void stickyScrollKeepsTheHeaders()
    {
        const QByteArray source = cppSource();
        const QString file = _tmp.filePath("sticky/pila.cpp");
        writeFile(file, source);
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        w.resize(900, 500);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QVERIFY(w.openFile(file) != kInvalidBuffer);
        EditorView* v = w.editor();
        v->sci(SCI_COLOURISE, 0, -1);
        StickyScroll* sticky = w.stickyScrollOfCurrentView();
        QVERIFY(sticky);
        // Arriba del todo: nada fijo
        v->sci(SCI_SETFIRSTVISIBLELINE, 0);
        sticky->refresh();
        QVERIFY(sticky->lines().empty());
        QVERIFY(sticky->isHidden());
        // Dentro del for de la función larga: la función y el for, en ese orden. La llave de
        // la función va en su propia línea: se ve la firma («int larga(int n)»)
        const int forLine = lineOf(source, "    for (int i");
        v->sci(SCI_SETFIRSTVISIBLELINE, static_cast<uptr_t>(forLine + 20));
        sticky->refresh();
        QCOMPARE(sticky->lines(), (std::vector<int>{lineOf(source, "int larga"), forLine}));
        QVERIFY(!sticky->isHidden());
        // Un clic en la primera lleva a ella
        QTest::mouseClick(sticky, Qt::LeftButton, Qt::NoModifier, QPoint(80, 2));
        QCOMPARE(v->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(v->sci(SCI_GETCURRENTPOS))), sptr_t{lineOf(source, "int larga")});
        // Desactivado
        v->sci(SCI_SETFIRSTVISIBLELINE, static_cast<uptr_t>(forLine + 20));
        Preferences p = w.preferences();
        p.stickyScroll = false;
        w.setPreferences(p);
        QVERIFY(sticky->isHidden());
    }
};

QTEST_MAIN(TestNavigation)
#include "tst_navigation.moc"
