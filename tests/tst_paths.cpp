// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/Paths.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;

class TestPaths : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir _tmp;

private slots:
    void initTestCase()
    {
        // Antes de que QStandardPaths lea el entorno
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        qputenv("EDITA_DATA_DIR", _tmp.filePath("data").toUtf8());
    }

    void configDirFollowsXdg()
    {
        QCOMPARE(paths::configDir(), _tmp.filePath("config/edita"));
        QVERIFY(!QDir(paths::configDir()).exists());
        QVERIFY(paths::ensureConfigDir());
        QVERIFY(QDir(paths::configDir()).exists());
    }

    void dataFilesAreSearchedInOrder()
    {
        QCOMPARE(paths::dataDirs().value(0), _tmp.filePath("data"));
        QVERIFY(paths::findDataFile("solo-en-el-temporal.xml").isEmpty());
        QVERIFY(QDir().mkpath(_tmp.filePath("data")));
        QFile f(_tmp.filePath("data/solo-en-el-temporal.xml"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        QCOMPARE(paths::findDataFile("solo-en-el-temporal.xml"), _tmp.filePath("data/solo-en-el-temporal.xml"));
        // EDITA_DATA_DIR tiene prioridad sobre los datos de la aplicación
        QFile model(_tmp.filePath("data/langs.model.xml"));
        QVERIFY(model.open(QIODevice::WriteOnly));
        model.close();
        QCOMPARE(paths::findDataFile("langs.model.xml"), _tmp.filePath("data/langs.model.xml"));
    }
};

QTEST_GUILESS_MAIN(TestPaths)
#include "tst_paths.moc"
