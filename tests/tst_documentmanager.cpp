// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 1: DocumentManager (port de FileManager) sobre documentos de Scintilla.

#include "core/TextFile.h"
#include "doc/DocumentManager.h"

#include <ScintillaEditBase.h>

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

using namespace edita;
using namespace std::string_literals;

namespace {

QString fixture(const QString& relative)
{
    return QStringLiteral(EDITA_FIXTURES_DIR "/") + relative;
}

QByteArray readAll(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// Una vista que muestra un documento del manager, como hará el editor en la
// Fase 2. Al destruirse vuelve a un documento propio.
class View
{
public:
    explicit View(SciDocument doc) { _view.send(SCI_SETDOCPOINTER, 0, doc); }
    ~View() { _view.send(SCI_SETDOCPOINTER, 0, 0); }
    sptr_t send(unsigned int m, uptr_t w = 0, sptr_t l = 0) const { return _view.send(m, w, l); }
    void append(const char* text) const { _view.sends(SCI_APPENDTEXT, std::char_traits<char>::length(text), text); }

private:
    ScintillaEditBase _view;
};

// Máscaras acumuladas de las notificaciones de un buffer
int collectedMask(const QSignalSpy& spy, BufferId id)
{
    int mask = 0;
    for (const QList<QVariant>& args : spy) {
        if (args.at(0).value<BufferId>() == id)
            mask |= args.at(1).toInt();
    }
    return mask;
}

} // namespace

class TestDocumentManager : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir _tmp;

    QString copyToTmp(const QString& fixturePath, const QString& name)
    {
        const QString dst = _tmp.filePath(name);
        QFile::remove(dst);
        if (!QFile::copy(fixture(fixturePath), dst))
            return {};
        QFile::setPermissions(dst, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        return dst;
    }

private slots:
    void initTestCase() { QVERIFY(_tmp.isValid()); }

    // Criterio de la Fase 1, a través de documentos de Scintilla reales
    void everyFixtureRoundTripsThroughScintilla_data()
    {
        QTest::addColumn<QString>("file");
        const QDir dir(fixture("encoding"));
        for (const QString& name : dir.entryList(QDir::Files))
            QTest::newRow(qPrintable(name)) << name;
        for (const QString& name : QDir(fixture("eol")).entryList(QDir::Files))
            QTest::newRow(qPrintable("eol/" + name)) << "../eol/" + name;
    }

    void everyFixtureRoundTripsThroughScintilla()
    {
        QFETCH(QString, file);
        const QString path = fixture("encoding/" + file);
        DocumentManager manager;
        QString error;
        const BufferId id = manager.loadFile(path, -1, &error);
        QVERIFY2(id != kInvalidBuffer, qPrintable(error));

        std::string expected;
        const LoadResult direct = loadTextFile(path, LoadOptions{}, expected);
        QCOMPARE(manager.text(id), expected);
        QVERIFY(manager.buffer(id)->encoding() == direct.encoding);
        QCOMPARE(manager.buffer(id)->eol(), direct.eol);

        const QString out = _tmp.filePath("dm-" + QFileInfo(file).fileName());
        QVERIFY(manager.saveBuffer(id, out).ok());
        QCOMPARE(readAll(out), readAll(path));
    }

    void loadedDocumentState()
    {
        DocumentManager manager;
        const BufferId id = manager.loadFile(fixture("eol/crlf.txt"));
        QVERIFY(id != kInvalidBuffer);
        const Buffer* buf = manager.buffer(id);
        QCOMPARE(buf->status(), DocStatus::Regular);
        QCOMPARE(buf->displayName(), QStringLiteral("crlf.txt"));
        QCOMPARE(buf->filePath(), QDir::cleanPath(fixture("eol/crlf.txt")));
        QVERIFY(!buf->isDirty());
        QVERIFY(!buf->isLargeFile());

        const View view(buf->document());
        QCOMPARE(view.send(SCI_GETCODEPAGE), sptr_t{SC_CP_UTF8});
        QCOMPARE(view.send(SCI_GETEOLMODE), sptr_t{SC_EOL_CRLF});
        QCOMPARE(view.send(SCI_CANUNDO), sptr_t{0});   // cargar no se deshace
        QCOMPARE(view.send(SCI_GETMODIFY), sptr_t{0}); // punto de guardado
    }

    void untitledNumbering()
    {
        DocumentManager manager;
        const BufferId a = manager.newEmptyDocument();
        const BufferId b = manager.newEmptyDocument();
        QCOMPARE(manager.buffer(a)->displayName(), QStringLiteral("new 1"));
        QCOMPARE(manager.buffer(b)->displayName(), QStringLiteral("new 2"));
        QVERIFY(manager.buffer(a)->isUntitled());
        QVERIFY(manager.buffer(a)->encoding() == TextEncoding::utf8());

        manager.closeBuffer(a);
        const BufferId c = manager.newEmptyDocument();
        QCOMPARE(manager.buffer(c)->displayName(), QStringLiteral("new 1")); // el número libre más bajo
        QVERIFY(c != a);                                                    // los id no se reutilizan
    }

    void newDocumentUsesDefaultEol()
    {
        DocumentManager manager;
        manager.settings().load.defaultEol = EolType::Windows;
        const BufferId id = manager.newEmptyDocument();
        QCOMPARE(manager.buffer(id)->eol(), EolType::Windows);
        const View view(manager.buffer(id)->document());
        QCOMPARE(view.send(SCI_GETEOLMODE), sptr_t{SC_EOL_CRLF});
        QCOMPARE(view.send(SCI_GETCODEPAGE), sptr_t{SC_CP_UTF8});
    }

    void editAndSaveAs()
    {
        DocumentManager manager;
        const BufferId id = manager.newEmptyDocument();
        {
            const View view(manager.buffer(id)->document());
            view.append("hola ñandú\n");
        }
        manager.syncDirtyState(id);
        QVERIFY(manager.buffer(id)->isDirty());

        QSignalSpy spy(&manager, &DocumentManager::bufferChanged);
        const QString out = _tmp.filePath("guardado como.txt");
        QVERIFY(manager.saveBuffer(id, out).ok());

        const Buffer* buf = manager.buffer(id);
        QVERIFY(!buf->isDirty());
        QVERIFY(!buf->isUntitled());
        QCOMPARE(buf->filePath(), out);
        QCOMPARE(buf->displayName(), QStringLiteral("guardado como.txt"));
        QCOMPARE(manager.findByPath(out), id);
        QCOMPARE(readAll(out), QByteArray("hola ñandú\n"));
        const int mask = collectedMask(spy, id);
        QVERIFY(mask & BufferChange::Filename);
        QVERIFY(mask & BufferChange::Dirty);
        QVERIFY(mask & BufferChange::Status);

        const View view(buf->document());
        QCOMPARE(view.send(SCI_GETMODIFY), sptr_t{0});
    }

    void saveCopyKeepsPathAndDirtyState()
    {
        DocumentManager manager;
        const QString path = copyToTmp("eol/lf.txt", "original.txt");
        const BufferId id = manager.loadFile(path);
        {
            const View view(manager.buffer(id)->document());
            view.append("cuatro\n");
        }
        manager.syncDirtyState(id);

        const QString copy = _tmp.filePath("copia.txt");
        QVERIFY(manager.saveBuffer(id, copy, /*isCopy*/ true).ok());
        QCOMPARE(manager.buffer(id)->filePath(), path);
        QVERIFY(manager.buffer(id)->isDirty());
        QCOMPARE(readAll(copy), QByteArray("uno\ndos\ntres\ncuatro\n"));
        QCOMPARE(readAll(path), QByteArray("uno\ndos\ntres\n"));
    }

    void convertEolChangesTextAndSavedBytes()
    {
        DocumentManager manager;
        const BufferId id = manager.loadFile(copyToTmp("eol/crlf.txt", "a-convertir.txt"));
        QSignalSpy spy(&manager, &DocumentManager::bufferChanged);

        manager.convertEol(id, EolType::Unix);
        QCOMPARE(manager.text(id), "uno\ndos\ntres\n"s);
        QCOMPARE(manager.buffer(id)->eol(), EolType::Unix);
        QVERIFY(manager.buffer(id)->isDirty());
        QVERIFY(collectedMask(spy, id) & BufferChange::Format);

        const View view(manager.buffer(id)->document());
        QCOMPARE(view.send(SCI_GETEOLMODE), sptr_t{SC_EOL_LF});

        QVERIFY(manager.saveBuffer(id, manager.buffer(id)->filePath()).ok());
        QCOMPARE(readAll(manager.buffer(id)->filePath()), QByteArray("uno\ndos\ntres\n"));

        // La conversión se puede deshacer, como en N++
        view.send(SCI_UNDO);
        QCOMPARE(manager.text(id), "uno\r\ndos\r\ntres\r\n"s);
    }

    void convertEncodingOnSave()
    {
        DocumentManager manager;
        const BufferId id = manager.loadFile(copyToTmp("encoding/windows-1252.txt", "a-utf8.txt"));
        QVERIFY(manager.buffer(id)->encoding() == TextEncoding::fromCodepage(1252));
        QSignalSpy spy(&manager, &DocumentManager::bufferChanged);

        manager.buffer(id)->setEncoding(TextEncoding::unicode(UniMode::Utf8Bom));
        QVERIFY(manager.buffer(id)->isDirty());
        QVERIFY(collectedMask(spy, id) & BufferChange::Unicode);

        QVERIFY(manager.saveBuffer(id, manager.buffer(id)->filePath()).ok());
        QCOMPARE(readAll(manager.buffer(id)->filePath()),
                 "\xEF\xBB\xBF" + readAll(fixture("encoding/reference/spanish-euro.txt")));
    }

    void reloadWithAnotherEncoding()
    {
        DocumentManager manager;
        const BufferId id = manager.loadFile(fixture("encoding/iso-8859-15.txt"));
        QVERIFY(manager.buffer(id)->encoding() == TextEncoding::fromCodepage(1252));
        const SciDocument docBefore = manager.buffer(id)->document();

        QVERIFY(manager.reloadBuffer(id, 28605));
        QVERIFY(manager.buffer(id)->encoding() == TextEncoding::fromCodepage(28605));
        QCOMPARE(QByteArray::fromStdString(manager.text(id)), readAll(fixture("encoding/reference/spanish-euro.txt")));
        // Mismo documento de Scintilla: las vistas que lo muestran no se enteran del cambio de puntero
        QCOMPARE(manager.buffer(id)->document(), docBefore);
        QVERIFY(!manager.buffer(id)->isDirty());
    }

    void loadErrorsDoNotCreateBuffers()
    {
        DocumentManager manager;
        QString error;
        QCOMPARE(manager.loadFile(_tmp.filePath("no-existe.txt"), -1, &error), kInvalidBuffer);
        QVERIFY(!error.isEmpty());
        QCOMPARE(manager.loadFile(_tmp.path(), -1, &error), kInvalidBuffer);
        QCOMPARE(manager.count(), std::size_t{0});

        const BufferId untitled = manager.newEmptyDocument();
        QVERIFY(!manager.reloadBuffer(untitled, -1, &error));
    }

    void readOnlyFile()
    {
        DocumentManager manager;
        const QString path = copyToTmp("eol/lf.txt", "solo-lectura.txt");
        QVERIFY(QFile::setPermissions(path, QFileDevice::ReadOwner));
        const BufferId id = manager.loadFile(path);
        if (geteuid() != 0)
            QVERIFY(manager.buffer(id)->isFileReadOnly());
        QVERIFY(!manager.buffer(id)->isUserReadOnly());
        manager.buffer(id)->setUserReadOnly(true);
        QVERIFY(manager.buffer(id)->isReadOnly());
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }

    void largeFilesHaveNoStyles()
    {
        DocumentManager manager;
        manager.settings().largeFileThreshold = 16; // ascii.txt tiene más
        const BufferId id = manager.loadFile(fixture("encoding/ascii.txt"));
        QVERIFY(manager.buffer(id)->isLargeFile());
        const View view(manager.buffer(id)->document());
        QVERIFY(view.send(SCI_GETDOCUMENTOPTIONS) & SC_DOCUMENTOPTION_STYLES_NONE);
    }

    void closeReleasesBuffer()
    {
        DocumentManager manager;
        const BufferId id = manager.loadFile(fixture("eol/lf.txt"));
        QSignalSpy closed(&manager, &DocumentManager::bufferClosed);
        manager.closeBuffer(id);
        QCOMPARE(manager.count(), std::size_t{0});
        QVERIFY(manager.buffer(id) == nullptr);
        QCOMPARE(closed.count(), 1);
        manager.closeBuffer(id); // cerrar dos veces no hace nada
    }

    void documentOutlivesManagerWhileAViewShowsIt()
    {
        // El recuento de referencias de Scintilla mantiene vivo el documento
        auto manager = std::make_unique<DocumentManager>();
        const BufferId id = manager->loadFile(fixture("eol/lf.txt"));
        const View view(manager->buffer(id)->document());
        manager.reset();
        QCOMPARE(view.send(SCI_GETLENGTH), sptr_t{13});
    }
};

QTEST_MAIN(TestDocumentManager)
#include "tst_documentmanager.moc"
