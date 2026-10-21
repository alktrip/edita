// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 5: session.xml con el esquema de Notepad++ y config.xml (<FindHistory>).

#include "core/ConfigXml.h"
#include "core/Session.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QByteArray readAll(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

} // namespace

class TestSession : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir _tmp;

private slots:
    void initTestCase() { QVERIFY(_tmp.isValid()); }

    void roundTrip()
    {
        Session s;
        s.activeView = 1;
        s.activeMainIndex = 1;
        s.activeSubIndex = 0;
        SessionFile a;
        a.path = "/tmp/á b.cpp";
        a.languageName = "C++";
        a.encoding = 1252;
        a.userReadOnly = true;
        a.position = {10, 120, 130, 3, 1, 50};
        a.foldedLines = {4, 9};
        SessionFile b;
        b.path = "/tmp/c.py";
        b.languageName = "Python";
        s.mainView = {a, b};
        s.subView = {b};

        const QString path = _tmp.filePath("session.xml");
        QVERIFY(writeSessionFile(path, s));
        Session back;
        QVERIFY(readSessionFile(path, back));
        QCOMPARE(back.activeView, 1);
        QCOMPARE(back.activeMainIndex, 1);
        QCOMPARE(back.mainView.size(), std::size_t{2});
        QCOMPARE(back.subView.size(), std::size_t{1});
        QCOMPARE(back.mainView[0].path, a.path);
        QCOMPARE(back.mainView[0].languageName, QStringLiteral("C++"));
        QCOMPARE(back.mainView[0].encoding, 1252);
        QVERIFY(back.mainView[0].userReadOnly);
        QVERIFY(back.mainView[0].position == a.position);
        QCOMPARE(back.mainView[0].foldedLines, a.foldedLines);

        // Mismo esquema que N++
        const QByteArray xml = readAll(path);
        QVERIFY(xml.contains("<NotepadPlus>"));
        QVERIFY(xml.contains("<Session activeView=\"1\">"));
        QVERIFY(xml.contains("<mainView activeIndex=\"1\">"));
        QVERIFY(xml.contains("firstVisibleLine=\"10\" xOffset=\"3\" scrollWidth=\"50\" startPos=\"120\" endPos=\"130\" selMode=\"1\""));
        QVERIFY(xml.contains("lang=\"C++\" encoding=\"1252\" userReadOnly=\"yes\" filename=\"/tmp/á b.cpp\""));
    }

    void writingKeepsABackupAndCorruptFallsBack()
    {
        const QString path = _tmp.filePath("respaldo.xml");
        Session first;
        first.mainView.push_back({});
        first.mainView[0].path = "/primera";
        QVERIFY(writeSessionFile(path, first));
        Session second;
        second.mainView.push_back({});
        second.mainView[0].path = "/segunda";
        QVERIFY(writeSessionFile(path, second));
        // La anterior queda en .inCaseOfCorruption.bak
        Session backup;
        QVERIFY(readSessionFile(path + QLatin1String(kSessionBackupSuffix), backup));
        QCOMPARE(backup.mainView[0].path, QStringLiteral("/primera"));

        // Sesión dañada: se usa la copia
        writeFile(path, "<NotepadPlus><Session activeView=");
        Session recovered;
        bool usedBackup = false;
        QVERIFY(readSessionWithBackup(path, recovered, &usedBackup));
        QVERIFY(usedBackup);
        QCOMPARE(recovered.mainView[0].path, QStringLiteral("/primera"));

        // Sin sesión (primer arranque) no hay nada que leer, ni copia
        Session none;
        QVERIFY(!readSessionWithBackup(_tmp.filePath("no-existe.xml"), none));
    }

    void readsSessionWrittenByNotepadPlusPlus()
    {
        // Sesión con todos los atributos que escribe N++ 8.x, incluidos los que
        // Edita no usa (mapas, colores de pestaña, marcas…) y una ruta de Windows
        const QString path = _tmp.filePath("npp-session.xml");
        writeFile(path, R"XML(<?xml version="1.0" encoding="UTF-8" ?>
<NotepadPlus>
    <Session activeView="0">
        <mainView activeIndex="1">
            <File firstVisibleLine="42" xOffset="0" scrollWidth="1146" startPos="1520" endPos="1527" selMode="0" offset="0" wrapCount="1" lang="C++" encoding="-1" userReadOnly="no" filename="/home/u/proyecto/main.cpp" backupFilePath="" originalFileLastModifTimestamp="1450318447" originalFileLastModifTimestampHigh="31092378" tabColourId="-1" RTL="no" tabPinned="no" mapFirstVisibleDisplayLine="-1" mapFirstVisibleDocLine="-1" mapLastVisibleDocLine="-1" mapNbLine="-1" mapHigherPos="-1" mapWidth="-1" mapHeight="-1" mapKByteInDoc="512" mapWrapIndentMode="-1" mapIsWrap="no">
                <Mark line="12" />
                <Fold line="3" />
            </File>
            <File firstVisibleLine="0" xOffset="0" scrollWidth="1" startPos="0" endPos="0" selMode="0" offset="0" wrapCount="1" lang="None (Normal Text)" encoding="1252" userReadOnly="yes" filename="C:\Users\u\notas.txt" backupFilePath="" originalFileLastModifTimestamp="0" originalFileLastModifTimestampHigh="0" tabColourId="2" RTL="no" tabPinned="yes" />
        </mainView>
        <subView activeIndex="0">
            <File firstVisibleLine="5" xOffset="0" scrollWidth="1" startPos="7" endPos="7" selMode="0" offset="0" wrapCount="1" lang="Python" encoding="-1" userReadOnly="no" filename="/home/u/proyecto/main.cpp" backupFilePath="" originalFileLastModifTimestamp="0" originalFileLastModifTimestampHigh="0" tabColourId="-1" RTL="no" tabPinned="no" />
        </subView>
    </Session>
</NotepadPlus>
)XML");
        Session s;
        QVERIFY(readSessionFile(path, s));
        QCOMPARE(s.activeMainIndex, 1);
        QCOMPARE(s.mainView.size(), std::size_t{2});
        const SessionFile& f = s.mainView[0];
        QCOMPARE(f.path, QStringLiteral("/home/u/proyecto/main.cpp"));
        QCOMPARE(f.languageName, QStringLiteral("C++"));
        QCOMPARE(f.position.firstVisibleLine, std::intptr_t{42});
        QCOMPARE(f.position.startPos, std::intptr_t{1520});
        QCOMPARE(f.position.endPos, std::intptr_t{1527});
        QCOMPARE(f.position.scrollWidth, std::intptr_t{1146});
        QCOMPARE(f.bookmarks, std::vector<std::size_t>{12});
        QCOMPARE(f.foldedLines, std::vector<std::size_t>{3});
        // Ruta de Windows con barras convertidas (no existirá en Linux y se omitirá al abrir)
        QCOMPARE(s.mainView[1].path, QStringLiteral("C:/Users/u/notas.txt"));
        QCOMPARE(s.mainView[1].encoding, 1252);
        QVERIFY(s.mainView[1].userReadOnly);
        QCOMPARE(s.subView[0].languageName, QStringLiteral("Python"));
    }

    void findHistoryInConfigXml()
    {
        const QString path = _tmp.filePath("config.xml");
        {
            ConfigXml config;
            QVERIFY(config.load(path));
            config.setGuiConfig("stylerTheme", "path", "/x/tema.xml");
            FindHistory h;
            h.finds = {"uno", "dos"};
            h.replaces = {"tres"};
            h.filters = {"*.cpp *.h"};
            h.paths = {"/src"};
            h.matchCase = true;
            h.searchMode = 2;
            h.recursive = false;
            config.setFindHistory(h);
            QVERIFY(config.save());
        }
        ConfigXml again;
        QVERIFY(again.load(path));
        const FindHistory h = again.findHistory();
        QCOMPARE(h.finds, (QStringList{"uno", "dos"}));
        QCOMPARE(h.filters, QStringList{"*.cpp *.h"});
        QVERIFY(h.matchCase);
        QVERIFY(!h.recursive);
        QCOMPARE(h.searchMode, 2);
        QCOMPARE(again.guiConfig("stylerTheme", "path"), QStringLiteral("/x/tema.xml")); // lo demás se conserva
        QVERIFY(readAll(path).contains("fifRecuisive=\"no\"")); // nombre del atributo de N++, tal cual
    }
};

QTEST_GUILESS_MAIN(TestSession)
#include "tst_session.moc"
