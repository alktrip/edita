// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 22: paneles de N++ (Lista de documentos, Mapa del documento, Lista de
// funciones y Explorador de carpetas).

#include "FakePrompter.h"
#include "core/Session.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/FunctionParser.h"
#include "ui/MainWindow.h"
#include "ui/Panels.h"

#include <QAction>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

using namespace edita;
using edita::test::FakePrompter;

namespace {

const char* kCpp = R"(// int comentada(int x) { return x; }
#include <cstdio>

class Pila
{
public:
    void push(int v) { _v = v; }
    int pop() { return _v; }
private:
    int _v = 0;
};

int suma(int a, int b)
{
    return a + b;
}

/* void enComentario() { } */

static void ayuda(const char* texto)
{
    std::puts(texto);
}
)";

const char* kPython = R"(import os

def primera(a):
    return a

class Coche:
    def arrancar(self):
        pass

    def parar(self):
        pass

def segunda():
    pass
)";

QStringList names(QTreeWidgetItem* item)
{
    QStringList list;
    for (int i = 0; i < item->childCount(); ++i) {
        list << item->child(i)->text(0);
        list << names(item->child(i));
    }
    return list;
}

} // namespace

class TestPanels : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir _tmp;

    QString file(const QString& name, const QByteArray& text)
    {
        const QString path = _tmp.filePath(name);
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        if (f.open(QIODevice::WriteOnly))
            f.write(text);
        return path;
    }

    struct Window
    {
        MainWindow window;
        Window() { window.setPrompter(std::make_unique<FakePrompter>()); }
    };

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    void cleanup()
    {
        QDir(_tmp.filePath("config")).removeRecursively();
    }

    void functionParsersOfNotepadPlusPlus()
    {
        // overrideMap.xml usa los langID de N++: los de Edita son los mismos
        QCOMPARE(static_cast<int>(LangType::Cpp), 3);
        QCOMPARE(static_cast<int>(LangType::Python), 22);
        FunctionParsers parsers;
        ScintillaEditBase view;
        view.send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(kCpp));
        std::vector<FunctionInfo> found;
        QVERIFY(parsers.parse(view, LangType::Cpp, {}, found));
        std::vector<std::pair<std::string, std::string>> got;
        for (const FunctionInfo& f : found)
            got.emplace_back(f.className, f.name);
        // Las de la clase con su clase; las sueltas sin; nada de lo comentado
        QVERIFY(std::count(got.begin(), got.end(), std::pair<std::string, std::string>("Pila", "push")));
        QVERIFY(std::count(got.begin(), got.end(), std::pair<std::string, std::string>("Pila", "pop")));
        QVERIFY(std::count(got.begin(), got.end(), std::pair<std::string, std::string>("", "suma")));
        QVERIFY(std::count(got.begin(), got.end(), std::pair<std::string, std::string>("", "ayuda")));
        for (const auto& [cls, name] : got)
            QVERIFY2(name != "comentada" && name != "enComentario", name.c_str());
        // La posición es la del nombre
        for (const FunctionInfo& f : found) {
            if (f.name == "suma")
                QCOMPARE(std::string_view(kCpp + f.pos, 4), std::string_view("suma"));
        }

        view.send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(kPython));
        found.clear();
        QVERIFY(parsers.parse(view, LangType::Python, {}, found));
        QStringList py;
        for (const FunctionInfo& f : found)
            py << QString::fromStdString(f.className.empty() ? f.name : f.className + "::" + f.name);
        // El analizador de Python de N++ muestra también los parámetros
        QCOMPARE(py, (QStringList{"Coche::arrancar(self)", "Coche::parar(self)", "primera(a)", "segunda()"}));
        // Sin analizador (texto normal)
        QVERIFY(!parsers.parse(view, LangType::Text, {}, found));
    }

    void functionListFollowsTheDocument()
    {
        Window x;
        MainWindow& w = x.window;
        w.openFile(file(QStringLiteral("pila.cpp"), kCpp));
        w.panelDock(QStringLiteral("functionList"))->show();
        QTreeWidget* tree = w.functionListPanel()->tree();
        QTRY_COMPARE(tree->topLevelItemCount(), 1);
        QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("pila.cpp")); // la raíz es el archivo
        const QStringList all = names(tree->topLevelItem(0));
        QVERIFY2(all.contains(QStringLiteral("Pila")) && all.contains(QStringLiteral("push")) && all.contains(QStringLiteral("suma")),
                 qPrintable(all.join(' ')));
        // Un clic lleva al nombre
        QTreeWidgetItem* suma = tree->findItems(QStringLiteral("suma"), Qt::MatchExactly | Qt::MatchRecursive).value(0);
        QVERIFY(suma);
        emit tree->itemClicked(suma, 0);
        const sptr_t caret = w.editor()->send(SCI_GETCURRENTPOS);
        QCOMPARE(std::string_view(kCpp + caret, 4), std::string_view("suma"));
        // Filtro y orden
        w.functionListPanel()->setFilter(QStringLiteral("pu"));
        QVERIFY(!tree->findItems(QStringLiteral("push"), Qt::MatchExactly | Qt::MatchRecursive).value(0)->isHidden());
        QVERIFY(suma->isHidden());
        w.functionListPanel()->setFilter(QString());
        w.functionListPanel()->setSorted(true);
        QTreeWidgetItem* root = tree->topLevelItem(0);
        QStringList top;
        for (int i = 0; i < root->childCount(); ++i)
            top << root->child(i)->text(0);
        QStringList sorted = top;
        std::sort(sorted.begin(), sorted.end(), [](const QString& a, const QString& b) { return a.localeAwareCompare(b) < 0; });
        QCOMPARE(top, sorted);
        // Al escribir una función nueva, se vuelve a analizar
        w.editor()->send(SCI_DOCUMENTEND);
        w.editor()->sends(SCI_REPLACESEL, 0, "\nint nueva(int z)\n{\n    return z;\n}\n");
        QTRY_VERIFY_WITH_TIMEOUT(!tree->findItems(QStringLiteral("nueva"), Qt::MatchExactly | Qt::MatchRecursive).isEmpty(), 5000);
    }

    void documentListShowsBothViews()
    {
        Window x;
        MainWindow& w = x.window;
        w.panelDock(QStringLiteral("documentList"))->show();
        w.openFile(file(QStringLiteral("uno.txt"), "1"));
        w.openFile(file(QStringLiteral("dos.txt"), "2"));
        QTreeWidget* list = w.documentListPanel()->list();
        QTRY_COMPARE(list->topLevelItemCount(), 2); // el "new 1" vacío se cerró al abrir
        QCOMPARE(list->currentItem()->text(0), QStringLiteral("dos.txt"));
        // Con cambios, el punto de "sin guardar"
        w.editor()->sends(SCI_REPLACESEL, 0, "x");
        QTRY_VERIFY(!list->currentItem()->icon(0).isNull());
        // Un clic activa el otro
        emit list->itemClicked(list->topLevelItem(0), 0);
        QCOMPARE(w.documents().buffer(w.currentBuffer())->displayName(), QStringLiteral("uno.txt"));
        // Clic central: cerrar
        emit w.documentListPanel()->closeRequested(w.currentBuffer(), 0);
        QTRY_COMPARE(list->topLevelItemCount(), 1);
    }

    void documentMapFollowsTheEditor()
    {
        QByteArray text;
        for (int i = 1; i <= 2000; ++i)
            text += "línea " + QByteArray::number(i) + "\n";
        Window x;
        MainWindow& w = x.window;
        w.resize(1000, 700);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.openFile(file(QStringLiteral("largo.txt"), text));
        w.panelDock(QStringLiteral("documentMap"))->show();
        DocumentMapPanel* map = w.documentMapPanel();
        // El mismo documento, en miniatura
        QTRY_COMPARE(map->map()->send(SCI_GETDOCPOINTER), w.editor()->send(SCI_GETDOCPOINTER));
        QVERIFY(map->map()->send(SCI_GETZOOM) < 0);
        QTRY_VERIFY(map->viewZone().height() > 0);
        const QRect top = map->viewZone();
        // Clic abajo en el mapa: el editor va hacia el final
        map->scrollEditorTo(map->map()->viewport()->height() - 5);
        QVERIFY(w.editor()->send(SCI_GETFIRSTVISIBLELINE) > 100);
        // El editor se desplaza: la zona también
        w.editor()->send(SCI_SETFIRSTVISIBLELINE, 0);
        QTRY_COMPARE(map->viewZone().top(), top.top());
        w.editor()->send(SCI_SETFIRSTVISIBLELINE, 1000);
        QTRY_VERIFY(map->viewZone().top() != top.top() || map->map()->send(SCI_GETFIRSTVISIBLELINE) > 0);
    }

    void folderAsWorkspace()
    {
        const QString root = _tmp.filePath(QStringLiteral("proyecto"));
        file(QStringLiteral("proyecto/b.txt"), "b");
        file(QStringLiteral("proyecto/a.txt"), "a");
        file(QStringLiteral("proyecto/src/main.cpp"), "int main() {}\n");
        file(QStringLiteral("proyecto/.oculto"), "x");
        Session saved;
        {
            Window x;
            MainWindow& w = x.window;
            QVERIFY(w.addFolderToWorkspace(root));
            QVERIFY(w.panelDock(QStringLiteral("folderAsWorkspace"))->isVisibleTo(&w));
            QVERIFY(!w.addFolderToWorkspace(root)); // no dos veces
            FileBrowserPanel* panel = w.folderPanel();
            QTreeWidgetItem* rootItem = panel->tree()->topLevelItem(0);
            QCOMPARE(rootItem->text(0), QStringLiteral("proyecto"));
            // Carpetas primero, por nombre; sin los ocultos
            QStringList children;
            for (int i = 0; i < rootItem->childCount(); ++i)
                children << rootItem->child(i)->text(0);
            QCOMPARE(children, (QStringList{"src", "a.txt", "b.txt"}));
            // Localizar el archivo actual despliega sus carpetas
            QVERIFY(panel->locate(root + "/src/main.cpp"));
            QCOMPARE(panel->selectedPath(), root + "/src/main.cpp");
            // Doble clic abre
            emit panel->tree()->itemDoubleClicked(panel->tree()->currentItem(), 0);
            QCOMPARE(w.documents().buffer(w.currentBuffer())->filePath(), root + "/src/main.cpp");
            // Va a la sesión (session.xml) con las carpetas desplegadas y la elegida
            saved = w.captureSession();
            QVERIFY(saved.hasFileBrowser);
            QCOMPARE(saved.folderRoots.size(), std::size_t{1});
            QCOMPARE(saved.folderRoots.front().folder, root);
            QVERIFY(saved.folderRoots.front().expanded.contains(root + "/src"));
            QCOMPARE(saved.folderSelected, root + "/src/main.cpp");
        }
        const QString sessionFile = _tmp.filePath(QStringLiteral("session.xml"));
        QVERIFY(writeSessionFile(sessionFile, saved));
        Session read;
        QVERIFY(readSessionFile(sessionFile, read));
        QCOMPARE(read.folderRoots, saved.folderRoots);
        QCOMPARE(read.folderSelected, saved.folderSelected);
        Window y;
        y.window.restoreSession(read);
        QVERIFY(y.window.panelDock(QStringLiteral("folderAsWorkspace"))->isVisibleTo(&y.window));
        QCOMPARE(y.window.folderPanel()->roots(), QStringList{root});
        QCOMPARE(y.window.folderPanel()->selectedPath(), root + "/src/main.cpp");
    }

    void panelsHaveMenuEntriesAndRememberTheirState()
    {
        {
            Window x;
            MainWindow& w = x.window;
            w.show();
            for (const char* name : {"actionDocumentMap", "actionDocumentList", "actionFunctionList", "actionFolderAsWorkspace"}) {
                QAction* action = w.findChild<QAction*>(QString::fromLatin1(name));
                QVERIFY2(action && action->isCheckable() && !action->icon().isNull(), name);
            }
            QAction* functions = w.findChild<QAction*>(QStringLiteral("actionFunctionList"));
            functions->trigger();
            QVERIFY(w.panelDock(QStringLiteral("functionList"))->isVisibleTo(&w));
            // <GUIConfig name="multiInst" fuctionList="yes"> (el nombre de N++)
            QCOMPARE(w.syntax().config().guiConfig(QStringLiteral("multiInst"), QStringLiteral("fuctionList")), QStringLiteral("yes"));
        }
        Window y;
        QVERIFY(y.window.panelDock(QStringLiteral("functionList"))->isVisibleTo(&y.window));
        QVERIFY(y.window.findChild<QAction*>(QStringLiteral("actionFunctionList"))->isChecked());
        QVERIFY(!y.window.panelDock(QStringLiteral("documentMap"))->isVisibleTo(&y.window));
    }
};

QTEST_MAIN(TestPanels)
#include "tst_panels.moc"
