// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Archivo › Cargar sesión y Guardar sesión (fileLoadSession, fileSaveSession), la
// extensión de las sesiones y --openSession.

#include "FakePrompter.h"
#include "core/Session.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/NotificationBar.h"
#include "ui/Panels.h"
#include "ui/SingleInstance.h"

#include <QAction>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;
using edita::test::FakePrompter;

namespace {

void writeFile(const QString& path, const QByteArray& data = "texto\n")
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QStringList paths(const std::vector<SessionFile>& files)
{
    QStringList out;
    for (const SessionFile& f : files)
        out << QFileInfo(f.path).fileName();
    return out;
}

QStringList openNames(MainWindow& w)
{
    QStringList out;
    for (EditorPane* pane : {w.findChild<EditorPane*>("mainPane"), w.findChild<EditorPane*>("subPane")}) {
        for (const BufferId id : pane->buffers())
            out << w.documents().buffer(id)->displayName();
    }
    return out;
}

} // namespace

class TestNamedSessions : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

    std::unique_ptr<MainWindow> window(FakePrompter** prompter = nullptr)
    {
        auto w = std::make_unique<MainWindow>();
        auto p = std::make_unique<FakePrompter>();
        if (prompter)
            *prompter = p.get();
        w->setPrompter(std::move(p));
        return w;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        for (const char* name : {"a.txt", "b.txt", "c.txt", "project/main.cpp"})
            writeFile(_tmp.filePath(QString::fromLatin1(name)));
    }

    void savesOnlyRealFiles()
    {
        auto w = window();
        w->show();
        w->openFile(_tmp.filePath("a.txt"));
        w->openFile(_tmp.filePath("b.txt"));
        w->findChild<QAction*>(QStringLiteral("actionMoveToOtherView"))->trigger();
        w->newDocument();
        w->editor()->sends(SCI_SETTEXT, 0, "sin guardar");
        w->addFolderToWorkspace(_tmp.filePath("project"));
        const QString file = _tmp.filePath("work.session");
        QVERIFY(w->saveSessionFile(file, true));
        Session s;
        QVERIFY(readSessionFile(file, s));
        QCOMPARE(paths(s.mainView), QStringList{QStringLiteral("a.txt")});
        QCOMPARE(paths(s.subView), QStringList{QStringLiteral("b.txt")});
        QVERIFY(s.mainView.front().backupFilePath.isEmpty());
        QVERIFY(s.hasFileBrowser);
        QCOMPARE(s.folderRoots.size(), size_t(1));
        // Sin las carpetas
        QVERIFY(w->saveSessionFile(file, false));
        QVERIFY(readSessionFile(file, s));
        QVERIFY(!s.hasFileBrowser);
    }

    void loadsAddingToWhatIsOpen()
    {
        Session s;
        SessionFile a;
        a.path = _tmp.filePath("a.txt");
        a.position.firstVisibleLine = 0;
        SessionFile missing;
        missing.path = _tmp.filePath("missing.txt");
        s.mainView = {a, missing};
        SessionFile b;
        b.path = _tmp.filePath("b.txt");
        s.subView = {b};
        s.hasFileBrowser = true;
        s.folderRoots = {{_tmp.filePath("project"), {}}};
        const QString file = _tmp.filePath("load.session");
        QVERIFY(writeSessionFile(file, s));

        auto w = window();
        w->show();
        w->openFile(_tmp.filePath("c.txt"));
        QVERIFY(w->loadSessionFile(file));
        const QStringList names = openNames(*w);
        QVERIFY2(names.contains(QStringLiteral("c.txt")) && names.contains(QStringLiteral("a.txt"))
                     && names.contains(QStringLiteral("b.txt")),
                 qPrintable(names.join(',')));
        QCOMPARE(w->findChild<EditorPane*>("subPane")->count(), 1);
        QCOMPARE(w->folderPanel()->roots(), QStringList{QDir::cleanPath(_tmp.filePath("project"))});
        QVERIFY(w->notificationBar()->currentText().contains(QStringLiteral("missing.txt")));
        // Un archivo que no es una sesión
        FakePrompter* asks = nullptr;
        auto other = window(&asks);
        QVERIFY(!other->loadSessionFile(_tmp.filePath("a.txt")));
        QCOMPARE(asks->warnings.size(), 1);
    }

    void menuAndExtension()
    {
        FakePrompter* asks = nullptr;
        auto w = window(&asks);
        w->show();
        auto prefs = w->preferences();
        prefs.sessionExt = QStringLiteral("edses");
        w->setPreferences(prefs);
        w->openFile(_tmp.filePath("a.txt"));
        // Guardar: la extensión se añade si falta y el filtro la ofrece
        asks->saveNames.enqueue(_tmp.filePath("menu"));
        w->findChild<QAction*>(QStringLiteral("actionSaveSession"))->trigger();
        QVERIFY(asks->lastFilters.contains(QStringLiteral("*.edses")));
        QVERIFY(QFileInfo::exists(_tmp.filePath("menu.edses")));
        // Cargar desde el menú
        auto other = window(&asks);
        other->show();
        asks->openNames.enqueue({_tmp.filePath("menu.edses")});
        other->findChild<QAction*>(QStringLiteral("actionLoadSession"))->trigger();
        QVERIFY(openNames(*other).contains(QStringLiteral("a.txt")));
        // Abrir un archivo con esa extensión carga la sesión en lugar de abrirlo
        auto third = window();
        third->show();
        third->setPreferences(prefs);
        QCOMPARE(third->openFile(_tmp.filePath("menu.edses")), kInvalidBuffer);
        QVERIFY(openNames(*third).contains(QStringLiteral("a.txt")));
        QVERIFY(!openNames(*third).contains(QStringLiteral("menu.edses")));
    }

    void openSessionFromTheCommandLine()
    {
        InstanceRequest request;
        request.session = _tmp.filePath("work.session");
        request.files << _tmp.filePath("c.txt");
        InstanceRequest parsed;
        QVERIFY(InstanceRequest::fromJson(request.toJson(), parsed));
        QCOMPARE(parsed.session, request.session);
        auto w = window();
        w->show();
        w->openFromOtherInstance(parsed, false);
        const QStringList names = openNames(*w);
        QVERIFY2(names.contains(QStringLiteral("a.txt")) && names.contains(QStringLiteral("c.txt")), qPrintable(names.join(',')));
    }
};

QTEST_MAIN(TestNamedSessions)
#include "tst_namedsessions.moc"
