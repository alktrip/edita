// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 6: cambios externos, copias "snapshot" de los cambios sin guardar y
// copia de la versión anterior al guardar.

#include "FakePrompter.h"

#include "core/Backup.h"
#include "core/ConfigXml.h"
#include "core/Paths.h"
#include "core/Preferences.h"
#include "core/Session.h"
#include "doc/DocumentManager.h"
#include "ui/DocTabBar.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
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

QByteArray readAll(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// Cambio "de otro programa": contenido nuevo y fecha claramente distinta
void changeOnDisk(const QString& path, const QByteArray& data, int secondsOffset = 10)
{
    const QDateTime before = QFileInfo(path).lastModified();
    writeFile(path, data);
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadWrite));
    QVERIFY(f.setFileTime(before.addSecs(secondsOffset), QFileDevice::FileModificationTime));
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
    const Buffer* buffer(BufferId id) const { return window->documents().buffer(id); }
    const Buffer* current() const { return buffer(window->currentBuffer()); }
    std::string text(BufferId id) const { return window->documents().text(id); }
    void type(const char* text) const { editor()->sends(SCI_REPLACESEL, 0, text); }
    void setPrefs(const std::function<void(Preferences&)>& change) const
    {
        Preferences p = window->preferences();
        change(p);
        window->setPreferences(p);
    }
};

} // namespace

class TestDataSafety : public QObject
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
        QCOMPARE(paths::configDir(), configDir());
    }

    // Cada test empieza con las preferencias por defecto y sin sesión ni copias
    void cleanup()
    {
        QFile::remove(QDir(configDir()).filePath("config.xml"));
        QFile::remove(QDir(configDir()).filePath("session.xml"));
        QDir(snapshotBackupDir()).removeRecursively();
        QDir(_tmp.filePath("files")).removeRecursively();
    }

    // --- Núcleo ---------------------------------------------------------------------

    void preferencesUseNotepadPlusPlusNames()
    {
        QCOMPARE(fileAutoDetectionFromText("yes"), int(FileAutoDetection::CurrentFile));
        QCOMPARE(fileAutoDetectionFromText("autoUpdate2EndOld"),
                 FileAutoDetection::AllFiles | FileAutoDetection::AutoUpdate | FileAutoDetection::GoToEnd);
        QCOMPARE(fileAutoDetectionFromText("no"), int(FileAutoDetection::Disabled));
        QCOMPARE(fileAutoDetectionToText(FileAutoDetection::CurrentFile | FileAutoDetection::AutoUpdate), QStringLiteral("auto"));

        const QString path = _tmp.filePath("prefs.xml");
        writeFile(path, R"XML(<?xml version="1.0" encoding="UTF-8" ?>
<NotepadPlus>
    <GUIConfigs>
        <GUIConfig name="Auto-detection">Update2EndOld</GUIConfig>
        <GUIConfig name="RememberLastSession">no</GUIConfig>
        <GUIConfig name="Backup" action="2" useCustumDir="yes" dir="/copias" isSnapshotMode="no" snapshotBackupTiming="15000" />
    </GUIConfigs>
</NotepadPlus>
)XML");
        ConfigXml config;
        QVERIFY(config.load(path));
        Preferences p = loadPreferences(config);
        QCOMPARE(p.fileAutoDetection, FileAutoDetection::AllFiles | FileAutoDetection::GoToEnd);
        QVERIFY(!p.rememberLastSession);
        QVERIFY(!p.snapshotMode);
        QCOMPARE(p.snapshotBackupTiming, 15000);
        QCOMPARE(p.backupOnSave, BackupOnSave::Verbose);
        QVERIFY(p.useCustomBackupDir);
        QCOMPARE(p.backupDir, QStringLiteral("/copias"));

        p.fileAutoDetection = FileAutoDetection::Disabled;
        p.snapshotMode = true;
        storePreferences(config, p);
        QVERIFY(config.save());
        const QByteArray xml = readAll(path);
        QVERIFY(xml.contains("<GUIConfig name=\"Auto-detection\">no</GUIConfig>"));
        QVERIFY(xml.contains("isSnapshotMode=\"yes\""));
        QVERIFY(xml.contains("useCustumDir=\"yes\""));

        // Sin config.xml: los valores por defecto de N++
        const Preferences defaults = loadPreferences(ConfigXml());
        QCOMPARE(defaults.fileAutoDetection, int(FileAutoDetection::CurrentFile));
        QVERIFY(defaults.snapshotMode && defaults.rememberLastSession);
        QCOMPARE(defaults.snapshotBackupTiming, 7000);
    }

    void backupNamesAndFileTime()
    {
        const QDateTime when(QDate(2026, 10, 20), QTime(9, 5, 7));
        QCOMPARE(snapshotBackupPath("/b", "new 1", when), QStringLiteral("/b/new 1@2026-10-20_090507"));

        Preferences p;
        QVERIFY(saveBackupPath("/d/a.txt", p, when).isEmpty());
        p.backupOnSave = BackupOnSave::Simple;
        QCOMPARE(saveBackupPath("/d/a.txt", p, when), QStringLiteral("/d/a.txt.bak"));
        p.backupOnSave = BackupOnSave::Verbose;
        QCOMPARE(saveBackupPath("/d/a.txt", p, when), QStringLiteral("/d/nppBackup/a.txt.2026-10-20_090507.bak"));
        p.useCustomBackupDir = true;
        p.backupDir = "/copias";
        QCOMPARE(saveBackupPath("/d/a.txt", p, when), QStringLiteral("/copias/nppBackup/a.txt.2026-10-20_090507.bak"));

        // FILETIME como lo escribe N++ (originalFileLastModifTimestamp/High)
        const QDateTime t = QDateTime::fromMSecsSinceEpoch(1760950000123);
        const FileTimeParts parts = toFileTime(t);
        QCOMPARE(fromFileTime(parts), t);
        // 1970-01-01 = 116444736000000000 en FILETIME
        const FileTimeParts epoch = toFileTime(QDateTime::fromMSecsSinceEpoch(0));
        QCOMPARE(static_cast<quint32>(epoch.high), 0x019DB1DEu);
        QCOMPARE(static_cast<quint32>(epoch.low), 0xD53E8000u);
        QVERIFY(!fromFileTime({}).isValid());
    }

    void sessionKeepsBackupFields()
    {
        Session s;
        SessionFile f;
        f.path = "new 2";
        f.backupFilePath = "/c/backup/new 2@2026-10-20_090507";
        f.originalLastModified = QDateTime::fromMSecsSinceEpoch(1760950000123);
        s.mainView.push_back(f);
        const QString path = _tmp.filePath("s.xml");
        QVERIFY(writeSessionFile(path, s));
        Session back;
        QVERIFY(readSessionFile(path, back));
        QCOMPARE(back.mainView[0].backupFilePath, f.backupFilePath);
        QCOMPARE(back.mainView[0].originalLastModified, f.originalLastModified);
        QVERIFY(readAll(path).contains("backupFilePath=\"/c/backup/new 2@2026-10-20_090507\""));
    }

    // --- Cambios externos --------------------------------------------------------

    void externalChangeAsksToReload()
    {
        writeFile(file("a.txt"), "uno\ndos\ntres\n");
        Window x;
        const BufferId id = x.w().openFile(file("a.txt"));
        x.editor()->sci(SCI_GOTOPOS, 5);

        // Sin cambios no se pregunta nada
        x.w().checkModifiedDocuments();
        QVERIFY(x.prompter->asked.isEmpty());

        changeOnDisk(file("a.txt"), "uno\nDOS\ntres\ncuatro\n");
        x.prompter->confirmations.enqueue(true);
        x.w().checkModifiedDocuments();
        QCOMPARE(x.prompter->asked, QStringList{"confirm:Reload"});
        QCOMPARE(x.text(id), "uno\nDOS\ntres\ncuatro\n"s);
        QVERIFY(!x.buffer(id)->isDirty());
        QCOMPARE(x.editor()->sci(SCI_GETCURRENTPOS), sptr_t{5}); // el cursor se conserva

        // Ya está al día: no se vuelve a preguntar
        x.w().checkModifiedDocuments();
        QCOMPARE(x.prompter->asked.size(), 1);

        // "No": se queda el texto del editor, que ya no coincide con el disco → modificado
        changeOnDisk(file("a.txt"), "otro\n", 20);
        x.prompter->confirmations.enqueue(false);
        x.w().checkModifiedDocuments();
        QCOMPARE(x.text(id), "uno\nDOS\ntres\ncuatro\n"s);
        QVERIFY(x.buffer(id)->isDirty());
        QCOMPARE(x.buffer(id)->status(), DocStatus::Modified);
        // ni deshaciendo vuelve a estar "sin cambios"
        x.editor()->sci(SCI_UNDO);
        QVERIFY(x.editor()->sci(SCI_GETMODIFY) != 0);
        x.w().checkModifiedDocuments();
        QCOMPARE(x.prompter->asked.size(), 2);
    }

    void autoUpdateReloadsSilentlyAndGoesToEnd()
    {
        writeFile(file("log.txt"), "a\n");
        Window x;
        x.setPrefs([](Preferences& p) {
            p.fileAutoDetection = FileAutoDetection::CurrentFile | FileAutoDetection::AutoUpdate | FileAutoDetection::GoToEnd;
        });
        const BufferId id = x.w().openFile(file("log.txt"));
        changeOnDisk(file("log.txt"), "a\nb\nc\n");
        x.w().checkModifiedDocuments();
        QVERIFY(x.prompter->asked.isEmpty());
        QCOMPARE(x.text(id), "a\nb\nc\n"s);
        QCOMPARE(x.editor()->sci(SCI_GETCURRENTPOS), sptr_t{6}); // al final, como tail -f

        // Con cambios sin guardar sí se pregunta, aunque esté en "actualizar sin preguntar"
        x.type("mío");
        changeOnDisk(file("log.txt"), "x\n", 20);
        x.prompter->confirmations.enqueue(false);
        x.w().checkModifiedDocuments();
        QCOMPARE(x.prompter->asked, QStringList{"confirm:Reload"});
    }

    void deletedFileCanBeKeptOrClosed()
    {
        writeFile(file("k.txt"), "guardar\n");
        writeFile(file("c.txt"), "cerrar\n");
        Window x;
        const BufferId keep = x.w().openFile(file("k.txt"));
        QFile::remove(file("k.txt"));
        x.prompter->confirmations.enqueue(true); // mantener
        x.w().checkModifiedDocuments();
        QCOMPARE(x.prompter->asked, QStringList{"confirm:Keep non existing file"});
        QVERIFY(x.buffer(keep));
        QVERIFY(x.buffer(keep)->isDirty());
        QCOMPARE(x.buffer(keep)->status(), DocStatus::Deleted);
        x.w().checkModifiedDocuments(); // no se pregunta otra vez
        QCOMPARE(x.prompter->asked.size(), 1);

        // Si reaparece, se ofrece recargarlo
        writeFile(file("k.txt"), "de vuelta\n");
        x.prompter->confirmations.enqueue(true);
        x.w().checkModifiedDocuments();
        QCOMPARE(x.prompter->asked.last(), QStringLiteral("confirm:Reload"));
        QCOMPARE(x.text(keep), "de vuelta\n"s);
        QVERIFY(!x.buffer(keep)->isDirty());

        // Cerrar: sin preguntar si se guarda
        const BufferId gone = x.w().openFile(file("c.txt"));
        QFile::remove(file("c.txt"));
        x.prompter->confirmations.enqueue(false);
        x.w().checkModifiedDocuments();
        QVERIFY(!x.buffer(gone));
        QVERIFY(!x.w().mainPane()->contains(gone));
        QCOMPARE(x.w().currentBuffer(), keep);
    }

    void currentFileModeChecksOnActivation()
    {
        writeFile(file("p.txt"), "p\n");
        writeFile(file("q.txt"), "q\n");
        Window x;
        const BufferId p = x.w().openFile(file("p.txt"));
        const BufferId q = x.w().openFile(file("q.txt"));
        QCOMPARE(x.w().currentBuffer(), q);
        changeOnDisk(file("p.txt"), "p2\n");
        // Por defecto ("yes") solo se mira el documento visible
        x.w().checkModifiedDocuments();
        QVERIFY(x.prompter->asked.isEmpty());
        x.w().mainPane()->activateBuffer(p);
        x.prompter->confirmations.enqueue(true);
        x.w().checkModifiedDocuments();
        QCOMPARE(x.prompter->asked, QStringList{"confirm:Reload"});
        QCOMPARE(x.text(p), "p2\n"s);

        // Con "todos los archivos" también los que no se ven
        x.setPrefs([](Preferences& prefs) { prefs.fileAutoDetection = FileAutoDetection::AllFiles; });
        changeOnDisk(file("q.txt"), "q2\n");
        x.prompter->confirmations.enqueue(true);
        x.w().checkModifiedDocuments();
        QCOMPARE(x.text(q), "q2\n"s);

        // Desactivada: nada
        x.setPrefs([](Preferences& prefs) { prefs.fileAutoDetection = FileAutoDetection::Disabled; });
        changeOnDisk(file("q.txt"), "q3\n", 30);
        x.w().checkModifiedDocuments();
        QCOMPARE(x.prompter->asked.size(), 2);
    }

    void ownSaveIsNotAnExternalChange()
    {
        writeFile(file("s.txt"), "s\n");
        Window x;
        const BufferId id = x.w().openFile(file("s.txt"));
        QVERIFY(x.w().fileWatcher()->files().contains(file("s.txt")));
        x.type("más ");
        QVERIFY(x.w().saveBuffer(id));
        QCoreApplication::processEvents();
        x.w().checkModifiedDocuments();
        QVERIFY(x.prompter->asked.isEmpty());
        // QSaveFile sustituye el archivo: se sigue vigilando
        QVERIFY(x.w().fileWatcher()->files().contains(file("s.txt")));
    }

    void watcherDetectsChangesWhileActive()
    {
        writeFile(file("w.txt"), "w\n");
        Window x;
        x.w().show();
        x.w().activateWindow();
        if (!QTest::qWaitForWindowActive(&x.w()))
            QSKIP("La plataforma no activa ventanas");
        const BufferId id = x.w().openFile(file("w.txt"));
        QCoreApplication::processEvents();
        x.prompter->confirmations.enqueue(true);
        changeOnDisk(file("w.txt"), "w2\n");
        QTRY_COMPARE(x.prompter->asked, QStringList{"confirm:Reload"});
        QCOMPARE(x.text(id), "w2\n"s);
    }

    // --- Snapshot ------------------------------------------------------------------

    void snapshotBackupsOnlyWhatChanged()
    {
        writeFile(file("b.txt"), "original\n");
        Window x;
        QVERIFY(x.w().isSnapshotMode());
        const BufferId untitled = x.w().currentBuffer();
        x.type("borrador ñ");
        const BufferId fileId = x.w().openFile(file("b.txt"));
        x.w().backupNow();
        // Solo el documento con cambios
        const QString backup = x.buffer(untitled)->backupPath();
        QVERIFY(QFileInfo(backup).fileName().startsWith(QStringLiteral("new 1@")));
        QCOMPARE(QFileInfo(backup).absolutePath(), snapshotBackupDir());
        QCOMPARE(readAll(backup), QByteArray("borrador ñ"));
        QVERIFY(x.buffer(fileId)->backupPath().isEmpty());
        QVERIFY(!x.buffer(untitled)->needsBackup());
        // La sesión se guarda en cuanto aparece una copia nueva
        Session s;
        QVERIFY(readSessionFile(x.w().sessionPath(), s));
        QCOMPARE(s.mainView.size(), std::size_t{2});
        QCOMPARE(s.mainView[0].path, QStringLiteral("new 1"));
        QCOMPARE(s.mainView[0].backupFilePath, backup);

        // Sin cambios nuevos no se reescribe
        QFile::remove(backup);
        writeFile(backup, "marca");
        x.w().backupNow();
        QCOMPARE(readAll(backup), QByteArray("marca"));
        x.w().mainPane()->activateBuffer(untitled);
        x.editor()->sci(SCI_DOCUMENTEND);
        x.type("!");
        x.w().backupNow();
        QCOMPARE(readAll(backup), QByteArray("borrador ñ!"));

        // Editar con la vista de búsqueda (reemplazar en todos los abiertos) también cuenta
        x.w().mainPane()->activateBuffer(fileId);
        SearchOptions o;
        o.pattern = "original";
        o.replacement = "cambiado";
        QCOMPARE(x.w().replaceAllInOpenDocuments(o), 1);
        x.w().backupNow();
        QCOMPARE(readAll(x.buffer(fileId)->backupPath()), QByteArray("cambiado\n"));

        // Guardar borra la copia; deshacer hasta el original también
        const QString fileBackup = x.buffer(fileId)->backupPath();
        QVERIFY(x.w().saveBuffer(fileId));
        QVERIFY(!QFileInfo::exists(fileBackup));
        QVERIFY(x.buffer(fileId)->backupPath().isEmpty());
        x.w().mainPane()->activateBuffer(untitled);
        while (x.editor()->sci(SCI_CANUNDO))
            x.editor()->sci(SCI_UNDO);
        QVERIFY(!x.buffer(untitled)->isDirty());
        x.w().backupNow();
        QVERIFY(!QFileInfo::exists(backup));

        // Cerrar una pestaña con cambios (descartándolos) borra su copia
        x.type("otra vez");
        x.w().backupNow();
        const QString again = x.buffer(untitled)->backupPath();
        QVERIFY(QFileInfo::exists(again));
        x.prompter->saveChoices.enqueue(Prompter::SaveChoice::Discard);
        QVERIFY(x.w().closeBuffer(untitled));
        QVERIFY(!QFileInfo::exists(again));
    }

    void snapshotRestoresUnsavedWorkAcrossRestarts()
    {
        writeFile(file("r.cpp"), "int a;\n");
        writeFile(file("cp.txt"), "caf\xe9\n"); // Windows-1252
        QString untitledBackup;
        {
            Window x;
            x.type("idea sin título");
            const BufferId code = x.w().openFile(file("r.cpp"));
            x.editor()->sci(SCI_DOCUMENTEND);
            x.type("int b;\n");
            const BufferId cp = x.w().openFile(file("cp.txt"));
            x.w().reinterpretAs(1252);
            QCOMPARE(x.text(cp), "café\n"s);
            x.editor()->sci(SCI_DOCUMENTEND);
            x.type("€ y ☃"); // ☃ no cabe en 1252: la copia no debe perderlo
            QVERIFY(x.buffer(code)->isDirty());
            // Salir no pregunta nada: los cambios quedan en las copias
            QVERIFY(x.w().close());
            QVERIFY(x.prompter->asked.isEmpty());
            QCOMPARE(readAll(file("r.cpp")), QByteArray("int a;\n")); // el archivo no se toca
        }
        Session s;
        QVERIFY(readSessionFile(QDir(configDir()).filePath("session.xml"), s));
        QCOMPARE(s.mainView.size(), std::size_t{3});
        untitledBackup = s.mainView[0].backupFilePath;
        QVERIFY(QFileInfo::exists(untitledBackup));
        QVERIFY(s.mainView[1].originalLastModified.isValid());

        Window y;
        QVERIFY(y.w().restoreLastSession());
        const std::vector<BufferId> ids = y.w().mainPane()->buffers();
        QCOMPARE(ids.size(), std::size_t{3});
        const Buffer* untitled = y.buffer(ids[0]);
        QVERIFY(untitled->isUntitled());
        QCOMPARE(untitled->displayName(), QStringLiteral("new 1"));
        QVERIFY(untitled->isDirty());
        QCOMPARE(y.text(ids[0]), "idea sin título"s);
        const Buffer* code = y.buffer(ids[1]);
        QCOMPARE(code->filePath(), file("r.cpp"));
        QVERIFY(code->isDirty());
        QCOMPARE(code->language(), LangType::Cpp);
        QCOMPARE(y.text(ids[1]), "int a;\nint b;\n"s);
        const Buffer* cp = y.buffer(ids[2]);
        QCOMPARE(cp->encoding().codepage, 1252);
        QCOMPARE(y.text(ids[2]), "café\n€ y ☃"s);
        QCOMPARE(y.w().mainPane()->tabBar()->tabText(0), QStringLiteral("new 1"));
        // Un documento nuevo no repite el nombre
        const BufferId fresh = y.w().newDocument();
        QCOMPARE(y.buffer(fresh)->displayName(), QStringLiteral("new 2"));

        // Restaurado con cambios: no se ofrece recargar si el archivo no cambió
        y.w().mainPane()->activateBuffer(ids[1]);
        y.w().checkModifiedDocuments();
        QVERIFY(y.prompter->asked.isEmpty());
        // Deshacer no lo deja "sin cambios": el disco tiene otra cosa
        y.editor()->sci(SCI_UNDO);
        QVERIFY(y.buffer(ids[1])->isDirty());

        // Guardar escribe el archivo y borra la copia
        const QString codeBackup = code->backupPath();
        QVERIFY(y.w().saveBuffer(ids[1]));
        QCOMPARE(readAll(file("r.cpp")), QByteArray("int a;\nint b;\n"));
        QVERIFY(!QFileInfo::exists(codeBackup));
    }

    void fileChangedWhileClosedIsDetectedAfterRestore()
    {
        writeFile(file("t.txt"), "v1\n");
        {
            Window x;
            x.w().openFile(file("t.txt"));
            x.editor()->sci(SCI_DOCUMENTEND);
            x.type("mío\n");
            QVERIFY(x.w().close());
        }
        changeOnDisk(file("t.txt"), "v2 de otro programa\n");
        Window y;
        QVERIFY(y.w().restoreLastSession());
        const BufferId id = y.w().currentBuffer();
        QCOMPARE(y.text(id), "v1\nmío\n"s);
        y.prompter->confirmations.enqueue(false); // quedarse con lo de la copia
        y.w().checkModifiedDocuments();
        QCOMPARE(y.prompter->asked, QStringList{"confirm:Reload"});
        QCOMPARE(y.text(id), "v1\nmío\n"s);
    }

    void withoutSnapshotExitAsksAndKeepsNoBackups()
    {
        Window x;
        x.setPrefs([](Preferences& p) { p.snapshotMode = false; });
        QVERIFY(!x.w().isSnapshotMode());
        x.type("algo");
        x.w().backupNow();
        QVERIFY(x.current()->backupPath().isEmpty());
        x.prompter->saveChoices.enqueue(Prompter::SaveChoice::Discard);
        QVERIFY(x.w().close());
        QCOMPARE(x.prompter->asked.size(), 1);

        // --nosession también lo desactiva (NppGUI::isSnapshotMode)
        Window y;
        QVERIFY(!y.w().isSnapshotMode()); // config.xml sigue con snapshot desactivado
        y.setPrefs([](Preferences& p) { p.snapshotMode = true; });
        QVERIFY(y.w().isSnapshotMode());
        y.w().setSessionEnabled(false);
        QVERIFY(!y.w().isSnapshotMode());
    }

    void notRememberingTheSessionSkipsIt()
    {
        writeFile(file("n.txt"), "n\n");
        {
            Window x;
            x.setPrefs([](Preferences& p) { p.rememberLastSession = false; });
            QVERIFY(!x.w().isSnapshotMode()); // snapshot necesita la sesión
            x.w().openFile(file("n.txt"));
            QVERIFY(x.w().close());
        }
        QVERIFY(!QFileInfo::exists(QDir(configDir()).filePath("session.xml")));
        Window y;
        QVERIFY(!y.w().restoreLastSession());
    }

    void deletedBackupMakesExitAsk()
    {
        Window x;
        x.type("frágil");
        x.w().backupNow();
        QFile::remove(x.current()->backupPath()); // alguien borró la copia
        // backupNow vuelve a escribirla al salir; si no se puede, se pregunta
        QFile::remove(snapshotBackupDir());
        QDir(snapshotBackupDir()).removeRecursively();
        writeFile(_tmp.filePath("bloqueo"), "x");
        QVERIFY(QFile::rename(_tmp.filePath("bloqueo"), snapshotBackupDir())); // un archivo donde va la carpeta
        x.prompter->saveChoices.enqueue(Prompter::SaveChoice::Discard);
        QVERIFY(x.w().close());
        QCOMPARE(x.prompter->asked, QStringList{"save:new 1"});
        QFile::remove(snapshotBackupDir());
    }

    // --- Copia al guardar --------------------------------------------------------

    void backupOnSaveKeepsThePreviousVersion()
    {
        writeFile(file("doc.txt"), "versión 1\n");
        Window x;
        x.setPrefs([](Preferences& p) { p.backupOnSave = BackupOnSave::Simple; });
        const BufferId id = x.w().openFile(file("doc.txt"));
        x.editor()->sci(SCI_DOCUMENTEND);
        x.type("versión 2\n");
        QVERIFY(x.w().saveBuffer(id));
        QCOMPARE(readAll(file("doc.txt.bak")), QByteArray("versión 1\n"));
        QCOMPARE(readAll(file("doc.txt")), QByteArray("versión 1\nversión 2\n"));

        x.setPrefs([this](Preferences& p) {
            p.backupOnSave = BackupOnSave::Verbose;
            p.useCustomBackupDir = true;
            p.backupDir = _tmp.filePath("files/copias");
        });
        x.type("versión 3\n");
        QVERIFY(x.w().saveBuffer(id));
        const QStringList copies = QDir(_tmp.filePath("files/copias/nppBackup")).entryList(QDir::Files);
        QCOMPARE(copies.size(), 1);
        QVERIFY(copies[0].startsWith(QStringLiteral("doc.txt.")) && copies[0].endsWith(QStringLiteral(".bak")));
        QCOMPARE(readAll(_tmp.filePath("files/copias/nppBackup/") + copies[0]), QByteArray("versión 1\nversión 2\n"));

        // Si no se puede copiar se pregunta; "no" cancela el guardado
        x.setPrefs([this](Preferences& p) {
            p.backupOnSave = BackupOnSave::Simple;
            p.backupDir = _tmp.filePath("files/no-es-carpeta");
        });
        writeFile(_tmp.filePath("files/no-es-carpeta"), "archivo");
        x.type("versión 4\n");
        x.prompter->confirmations.enqueue(false);
        QVERIFY(!x.w().saveBuffer(id));
        QCOMPARE(x.prompter->asked, QStringList{"confirm:Backup"});
        QVERIFY(x.buffer(id)->isDirty());
    }
};

QTEST_MAIN(TestDataSafety)
#include "tst_datasafety.moc"
