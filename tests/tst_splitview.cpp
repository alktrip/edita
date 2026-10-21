// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 5: vista dividida (mover, clonar, ocultar la vista vacía) y restauración
// de la sesión al volver a abrir.

#include "FakePrompter.h"

#include "core/Session.h"
#include "doc/DocumentManager.h"
#include "ui/DocTabBar.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QSplitter>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;
using edita::test::FakePrompter;
using namespace std::string_literals;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QByteArray manyLines(int n)
{
    QByteArray out;
    for (int i = 1; i <= n; ++i)
        out += "línea " + QByteArray::number(i) + "\n";
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
    EditorPane* main() const { return window->mainPane(); }
    EditorPane* sub() const { return window->subPane(); }
};

} // namespace

class TestSplitView : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir _tmp;
    QString file(const QString& name) const { return _tmp.filePath(name); }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        writeFile(file("a.txt"), manyLines(300));
        writeFile(file("b.cpp"), "int main() {\n    return 0;\n}\n");
        writeFile(file("c.py"), "print('hola')\n");
        writeFile(file("d.txt"), manyLines(50));
    }

    void startsWithOnlyTheMainView()
    {
        Window x;
        QCOMPARE(x.sub()->count(), 0);
        QVERIFY(x.sub()->isHidden());
        QCOMPARE(x.w().pane(), x.main());
    }

    void moveToOtherViewAndBack()
    {
        Window x;
        const BufferId a = x.w().openFile(file("a.txt"));
        const BufferId b = x.w().openFile(file("b.cpp"));
        x.w().moveToOtherView();
        QVERIFY(x.sub()->contains(b));
        QVERIFY(!x.main()->contains(b));
        QVERIFY(!x.sub()->isHidden());
        QCOMPARE(x.w().pane(), x.sub());
        QCOMPARE(x.w().currentBuffer(), b);

        // Mover el último documento de la secundaria la oculta (hideView)
        x.w().moveToOtherView();
        QCOMPARE(x.sub()->count(), 0);
        QVERIFY(x.sub()->isHidden());
        QCOMPARE(x.w().pane(), x.main());
        QCOMPARE(x.main()->buffers(), (std::vector<BufferId>{a, b}));

        // Con la otra vista vacía, mover el único documento no hace nada
        x.w().closeAllBut(a);
        x.w().moveToOtherView();
        QCOMPARE(x.main()->count(), 1);
        QCOMPARE(x.sub()->count(), 0);
    }

    void cloneSharesTheDocumentWithIndependentCarets()
    {
        Window x;
        const BufferId a = x.w().openFile(file("d.txt"));
        x.main()->editor()->sci(SCI_GOTOPOS, 30);
        x.w().cloneToOtherView();
        QVERIFY(x.main()->contains(a));
        QVERIFY(x.sub()->contains(a));
        // El clon empieza donde estaba la vista original
        QCOMPARE(x.sub()->editor()->sci(SCI_GETCURRENTPOS), sptr_t{30});

        // Editar en una vista se ve al instante en la otra (mismo documento)
        x.sub()->editor()->sci(SCI_GOTOPOS, 0);
        x.sub()->editor()->sends(SCI_REPLACESEL, 0, "NUEVO ");
        QCOMPARE(x.main()->editor()->text().substr(0, 6), "NUEVO "s);
        // Cada vista con su cursor
        QCOMPARE(x.sub()->editor()->sci(SCI_GETCURRENTPOS), sptr_t{6});
        QVERIFY(x.main()->editor()->sci(SCI_GETCURRENTPOS) != 6);
        QVERIFY(x.w().documents().buffer(a)->isDirty());

        // Cerrar el clon no pregunta ni libera el documento
        x.w().setActivePane(x.sub());
        QVERIFY(x.w().closeBuffer(a));
        QVERIFY(x.prompter->asked.isEmpty());
        QVERIFY(x.w().documents().buffer(a) != nullptr);
        QCOMPARE(x.sub()->count(), 0);
        QVERIFY(x.sub()->isHidden());
        // El último sí pregunta
        x.prompter->saveChoices.enqueue(Prompter::SaveChoice::Discard);
        QVERIFY(x.w().closeBuffer(a));
        QCOMPARE(x.prompter->asked.size(), 1);
        QVERIFY(x.w().documents().buffer(a) == nullptr);
    }

    void closingTheLastDocumentOfTheMainViewHidesIt()
    {
        Window x;
        const BufferId a = x.w().openFile(file("a.txt"));
        x.w().openFile(file("b.cpp"));
        x.w().moveToOtherView();
        x.w().setActivePane(x.main());
        QVERIFY(x.w().closeBuffer(a));
        QVERIFY(x.main()->isHidden());
        QCOMPARE(x.w().pane(), x.sub());
        // Cerrar todo deja un "new 1" en la principal
        QVERIFY(x.w().closeAll());
        QCOMPARE(x.main()->count(), 1);
        QVERIFY(!x.main()->isHidden());
        QVERIFY(x.sub()->isHidden());
    }

    void tabBarsHaveHeightWhenSplitBeforeShowing()
    {
        // Regresión: clonar antes de mostrar la ventana dejaba la barra de
        // pestañas de la vista principal con altura 0
        Window x;
        x.w().openFile(file("a.txt"));
        x.w().openFile(file("b.cpp"));
        x.w().cloneToOtherView();
        x.w().show();
        QVERIFY(QTest::qWaitForWindowExposed(&x.w()));
        QTRY_VERIFY(x.main()->tabBar()->height() > 0);
        QTRY_VERIFY(x.sub()->tabBar()->height() > 0);
    }

    void switchAndRotate()
    {
        Window x;
        x.w().openFile(file("a.txt"));
        x.w().openFile(file("b.cpp"));
        x.w().moveToOtherView();
        x.w().switchToOtherView();
        QCOMPARE(x.w().pane(), x.main());
        x.w().findChild<QAction*>("actionSwitchView")->trigger(); // F8
        QCOMPARE(x.w().pane(), x.sub());
        QCOMPARE(x.w().splitter()->orientation(), Qt::Horizontal);
        x.w().toggleSplitOrientation();
        QCOMPARE(x.w().splitter()->orientation(), Qt::Vertical);
    }

    void openingAFileShownInTheOtherViewActivatesIt()
    {
        Window x;
        x.w().openFile(file("a.txt"));
        const BufferId b = x.w().openFile(file("b.cpp"));
        x.w().moveToOtherView();
        x.w().setActivePane(x.main());
        QCOMPARE(x.w().openFile(file("b.cpp")), b);
        QCOMPARE(x.w().pane(), x.sub());
        QCOMPARE(x.main()->count(), 1); // no se duplica
    }

    // --- Criterio: la sesión se restaura idéntica ---------------------------------

    void sessionRoundTripRestoresIdenticalState()
    {
        Session saved;
        {
            Window x;
            x.w().resize(800, 400);
            x.w().show();
            QVERIFY(QTest::qWaitForWindowExposed(&x.w()));
            const BufferId a = x.w().openFile(file("a.txt"));
            x.w().goToLine(200, 3);
            x.main()->editor()->sci(SCI_SETSEL, 1500, 1510);
            const BufferId b = x.w().openFile(file("b.cpp"));
            x.main()->editor()->sci(SCI_SETSEL, 4, 8); // "main"
            // Lenguaje forzado a mano en un .py: Bash
            const BufferId c = x.w().openFile(file("c.py"));
            x.w().setLanguage(LangType::Bash);
            x.w().moveToOtherView(); // c a la secundaria
            x.sub()->editor()->sci(SCI_GOTOPOS, 6);
            // Clon de a en la secundaria con otra posición
            x.w().setActivePane(x.main());
            x.main()->activateBuffer(a);
            x.w().cloneToOtherView();
            x.sub()->editor()->sci(SCI_GOTOPOS, 20);
            // Activa: b en la principal y la principal como vista activa
            x.w().setActivePane(x.main());
            x.main()->activateBuffer(b);
            QVERIFY(c != a);

            saved = x.w().captureSession();
            QVERIFY(x.w().saveSession());
            QVERIFY(QFileInfo::exists(x.w().sessionPath()));
        }

        // Comprobación del contenido capturado
        QCOMPARE(saved.mainView.size(), std::size_t{2});
        QCOMPARE(saved.subView.size(), std::size_t{2});
        QCOMPARE(saved.activeView, 0);
        QCOMPARE(saved.activeMainIndex, 1);
        QCOMPARE(saved.subView[0].languageName, QStringLiteral("Shell"));

        // Ventana nueva: restaurar desde session.xml
        Window y;
        y.w().resize(800, 400);
        y.w().show();
        QVERIFY(QTest::qWaitForWindowExposed(&y.w()));
        QVERIFY(y.w().restoreLastSession());
        const Session restored = y.w().captureSession();

        QCOMPARE(restored.activeView, saved.activeView);
        QCOMPARE(restored.activeMainIndex, saved.activeMainIndex);
        QCOMPARE(restored.activeSubIndex, saved.activeSubIndex);
        for (int k = 0; k < 2; ++k) {
            const auto& before = k == 0 ? saved.mainView : saved.subView;
            const auto& after = k == 0 ? restored.mainView : restored.subView;
            QCOMPARE(after.size(), before.size());
            for (std::size_t i = 0; i < before.size(); ++i) {
                QCOMPARE(after[i].path, before[i].path);
                QCOMPARE(after[i].languageName, before[i].languageName);
                QCOMPARE(after[i].encoding, before[i].encoding);
                QCOMPARE(after[i].position.startPos, before[i].position.startPos);
                QCOMPARE(after[i].position.endPos, before[i].position.endPos);
                QCOMPARE(after[i].position.firstVisibleLine, before[i].position.firstVisibleLine);
            }
        }
        // El "new 1" inicial desapareció y el clon es el mismo documento en las dos vistas
        QCOMPARE(y.main()->count(), 2);
        QVERIFY(!y.sub()->isHidden());
        const BufferId a = y.w().documents().findByPath(file("a.txt"));
        QVERIFY(y.main()->contains(a) && y.sub()->contains(a));
        QCOMPARE(y.w().documents().buffer(y.w().documents().findByPath(file("c.py")))->language(), LangType::Bash);
        QCOMPARE(y.w().pane(), y.main());
        QCOMPARE(y.w().currentBuffer(), y.w().documents().findByPath(file("b.cpp")));
        // Los cursores restaurados son los de la vista activa al mostrarse
        QCOMPARE(y.main()->editor()->sci(SCI_GETSELECTIONSTART), sptr_t{4});
        QCOMPARE(y.main()->editor()->sci(SCI_GETSELECTIONEND), sptr_t{8});
    }

    void sessionIsWrittenOnCloseAndRestoredOnStart()
    {
        {
            Window x;
            x.w().openFile(file("b.cpp"));
            QVERIFY(x.w().close()); // nada sin guardar: no pregunta
        }
        Window y;
        QVERIFY(y.w().restoreLastSession());
        QCOMPARE(y.w().documents().buffer(y.w().currentBuffer())->filePath(), file("b.cpp"));
    }

    void missingFilesAreSkippedAndEncodingIsForced()
    {
        writeFile(file("latin.txt"), "caf\xE9\n");
        Session s;
        SessionFile gone;
        gone.path = file("ya-no-existe.txt");
        SessionFile latin;
        latin.path = file("latin.txt");
        latin.encoding = 28591; // ISO-8859-1 forzado en la sesión
        latin.languageName = "None (Normal Text)";
        SessionFile windows;
        windows.path = "C:/Users/u/notas.txt";
        s.mainView = {gone, latin, windows};
        Window x;
        QStringList missing;
        x.w().restoreSession(s, &missing);
        QCOMPARE(missing.size(), 2);
        QVERIFY(x.prompter->warnings.isEmpty());
        QCOMPARE(x.main()->count(), 1);
        const Buffer* buf = x.w().documents().buffer(x.w().currentBuffer());
        QVERIFY(buf->encoding() == TextEncoding::fromCodepage(28591));
        QCOMPARE(x.w().documents().text(buf->id()), "café\n"s);
    }

    void corruptSessionUsesTheBackup()
    {
        Window x;
        x.w().openFile(file("d.txt"));
        QVERIFY(x.w().saveSession()); // sesión válida…
        x.w().openFile(file("b.cpp"));
        QVERIFY(x.w().saveSession()); // …que pasa a ser la copia
        writeFile(x.w().sessionPath(), "<NotepadPlus><Sess"); // y la actual se daña
        Window y;
        QVERIFY(y.w().restoreLastSession());
        QCOMPARE(y.prompter->warnings.size(), 1);
        QCOMPARE(y.main()->count(), 1);
        QCOMPARE(y.w().documents().buffer(y.w().currentBuffer())->filePath(), file("d.txt"));
    }

    void noSessionMode()
    {
        QFile::remove(_tmp.filePath("config/edita/session.xml"));
        Window x;
        x.w().setSessionEnabled(false);
        x.w().openFile(file("a.txt"));
        QVERIFY(x.w().close());
        QVERIFY(!QFileInfo::exists(_tmp.filePath("config/edita/session.xml")));
        QVERIFY(!x.w().restoreLastSession());
    }

    void findHistoryAndWindowSizeSurviveRestart()
    {
        {
            Window x;
            x.w().resize(900, 650);
            FindReplaceDialog* d = x.w().findDialog();
            d->showTab(FindReplaceDialog::Tab::Find, QStringLiteral("palabra buscada"));
            d->rememberCurrentTexts();
            QVERIFY(x.w().close());
        }
        Window y;
        y.w().restoreWindowGeometry();
        QCOMPARE(y.w().size(), QSize(900, 650));
        QCOMPARE(y.w().findDialog()->findHistory().value(0), QStringLiteral("palabra buscada"));
    }
};

QTEST_MAIN(TestSplitView)
#include "tst_splitview.moc"
