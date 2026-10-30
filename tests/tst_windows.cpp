// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Menú Ventana (WindowsMenu, WindowsDlg de N++): ordenar las pestañas, la lista de
// documentos del menú y el diálogo Ventanas.

#include "FakePrompter.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/WindowsDialog.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

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

QStringList tabNames(MainWindow& w)
{
    QStringList names;
    auto* pane = w.findChild<EditorPane*>("mainPane");
    for (const BufferId id : pane->buffers())
        names << w.documents().buffer(id)->displayName();
    return names;
}

} // namespace

class TestWindows : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

    std::unique_ptr<MainWindow> window()
    {
        auto w = std::make_unique<MainWindow>();
        w->setPrompter(std::make_unique<FakePrompter>());
        w->openFile(_tmp.filePath("b/zeta.py"));
        w->openFile(_tmp.filePath("a/alfa.txt"));
        w->openFile(_tmp.filePath("c/Medio.c"));
        return w;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        writeFile(_tmp.filePath("b/zeta.py"), "print(1)\n");                       // 9 bytes
        writeFile(_tmp.filePath("a/alfa.txt"), "texto mucho más largo que los otros\n");
        writeFile(_tmp.filePath("c/Medio.c"), "int x;\nint y;\n");                  // 14 bytes
    }

    void sortsTheTabs()
    {
        auto w = window();
        QCOMPARE(tabNames(*w), (QStringList{"zeta.py", "alfa.txt", "Medio.c"}));
        w->findChild<QAction*>(QStringLiteral("actionSortTabsNameAsc"))->trigger();
        QCOMPARE(tabNames(*w), (QStringList{"alfa.txt", "Medio.c", "zeta.py"})); // sin distinguir mayúsculas
        w->findChild<QAction*>(QStringLiteral("actionSortTabsNameDesc"))->trigger();
        QCOMPARE(tabNames(*w), (QStringList{"zeta.py", "Medio.c", "alfa.txt"}));
        w->sortTabs(MainWindow::TabSort::Size, true);
        QCOMPARE(tabNames(*w), (QStringList{"zeta.py", "Medio.c", "alfa.txt"}));
        w->sortTabs(MainWindow::TabSort::Path, true);
        QCOMPARE(tabNames(*w), (QStringList{"alfa.txt", "zeta.py", "Medio.c"}));
        w->sortTabs(MainWindow::TabSort::Path, false);
        QCOMPARE(tabNames(*w), (QStringList{"Medio.c", "zeta.py", "alfa.txt"}));
        // El documento activo sigue siéndolo
        const BufferId current = w->currentBuffer();
        w->sortTabs(MainWindow::TabSort::Type, true);
        QCOMPARE(w->currentBuffer(), current);
    }

    void listsTheDocumentsInTheMenu()
    {
        auto w = window();
        w->editor()->sends(SCI_REPLACESEL, 0, "cambio ");
        auto* menu = w->findChild<QMenu*>(QStringLiteral("menuWindow"));
        QVERIFY(menu);
        emit menu->aboutToShow();
        QAction* first = w->findChild<QAction*>(QStringLiteral("actionWindowDocument0"));
        QAction* third = w->findChild<QAction*>(QStringLiteral("actionWindowDocument2"));
        QVERIFY(first && third);
        QCOMPARE(first->text(), QStringLiteral("&1: zeta.py"));
        QCOMPARE(third->text(), QStringLiteral("&3: Medio.c*"));
        QVERIFY(third->isChecked() && !first->isChecked());
        first->trigger();
        QCOMPARE(w->documents().buffer(w->currentBuffer())->displayName(), QStringLiteral("zeta.py"));
        // Al cerrar uno, la lista cambia
        w->closeBuffer(w->currentBuffer());
        emit menu->aboutToShow();
        QVERIFY(!w->findChild<QAction*>(QStringLiteral("actionWindowDocument2")));
    }

    void windowsDialog()
    {
        auto w = window();
        w->editor()->sends(SCI_REPLACESEL, 0, "// cambio\n");
        w->findChild<QAction*>(QStringLiteral("actionWindowsList"))->trigger();
        WindowsDialog* dialog = w->windowsDialog();
        QVERIFY(dialog && dialog->isVisible());
        QTreeWidget* list = dialog->list();
        QCOMPARE(list->topLevelItemCount(), 3);
        QCOMPARE(list->topLevelItem(2)->text(0), QStringLiteral("Medio.c*"));
        QCOMPARE(list->topLevelItem(2)->text(1), QDir::cleanPath(_tmp.filePath("c/Medio.c")));
        QVERIFY(!list->topLevelItem(2)->text(2).isEmpty());
        // Ordenar por tamaño (columna 3) y pasar ese orden a las pestañas
        list->sortByColumn(3, Qt::DescendingOrder);
        dialog->findChild<QPushButton*>(QStringLiteral("windowsSort"))->click();
        QCOMPARE(tabNames(*w), (QStringList{"alfa.txt", "Medio.c", "zeta.py"}));
        // Guardar el elegido
        list->clearSelection();
        for (int i = 0; i < list->topLevelItemCount(); ++i)
            list->topLevelItem(i)->setSelected(list->topLevelItem(i)->text(0).startsWith(QLatin1String("Medio")));
        dialog->findChild<QPushButton*>(QStringLiteral("windowsSave"))->click();
        QFile saved(_tmp.filePath("c/Medio.c"));
        QVERIFY(saved.open(QIODevice::ReadOnly));
        QVERIFY(saved.readAll().startsWith("// cambio"));
        // Cerrar dos a la vez
        for (int i = 0; i < list->topLevelItemCount(); ++i)
            list->topLevelItem(i)->setSelected(!list->topLevelItem(i)->text(0).startsWith(QLatin1String("alfa")));
        QCOMPARE(dialog->selected().size(), size_t(2));
        dialog->findChild<QPushButton*>(QStringLiteral("windowsClose"))->click();
        QCOMPARE(tabNames(*w), QStringList{"alfa.txt"});
        QCOMPARE(list->topLevelItemCount(), 1);
        // Activar
        w->openFile(_tmp.filePath("b/zeta.py"));
        w->showWindowsDialog();
        list->clearSelection();
        list->topLevelItem(0)->setSelected(true); // alfa.txt
        dialog->findChild<QPushButton*>(QStringLiteral("windowsActivate"))->click();
        QCOMPARE(w->documents().buffer(w->currentBuffer())->displayName(), QStringLiteral("alfa.txt"));
        QVERIFY(!dialog->isVisible());
    }
};

QTEST_MAIN(TestWindows)
#include "tst_windows.moc"
