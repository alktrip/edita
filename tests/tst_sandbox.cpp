// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 105: Edita dentro de Flatpak. El puente lanza los programas del sistema fuera de la
// sandbox (aquí, con un lanzador falso que imprime lo que recibe), las carpetas del sistema se
// ven en /run/host y lo que no tiene sentido en Flatpak se desactiva.

#include "core/Preferences.h"
#include "core/Sandbox.h"
#include "core/SpellChecker.h"
#include "ui/PreferencesDialog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QListWidget>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;

namespace {

void writeExecutable(const QString& path, const QByteArray& text)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(text);
    f.close();
    f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
}

} // namespace

class TestSandbox : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    void cleanup() { qunsetenv("EDITA_SANDBOX"); }

    void detectsFlatpak()
    {
        qputenv("EDITA_SANDBOX", "flatpak");
        QVERIFY(sandbox::isFlatpak());
        qputenv("EDITA_SANDBOX", "none");
        QVERIFY(!sandbox::isFlatpak());
        qunsetenv("EDITA_SANDBOX");
        QCOMPARE(sandbox::isFlatpak(), QFileInfo::exists(QStringLiteral("/.flatpak-info")));
    }

    void hostDirectoriesAreUnderRunHost()
    {
        QCOMPARE(sandbox::visibleHostDir(QStringLiteral("/usr/bin")), QStringLiteral("/run/host/usr/bin"));
        QCOMPARE(sandbox::visibleHostDir(QStringLiteral("/usr/local/bin/")), QStringLiteral("/run/host/usr/local/bin"));
        QCOMPARE(sandbox::visibleHostDir(QStringLiteral("/bin")), QStringLiteral("/run/host/bin"));
        QCOMPARE(sandbox::visibleHostDir(QStringLiteral("/sbin")), QStringLiteral("/run/host/sbin"));
        // La carpeta personal y /opt se ven donde están
        QCOMPARE(sandbox::visibleHostDir(QStringLiteral("/home/ana/.local/bin")), QStringLiteral("/home/ana/.local/bin"));
        QCOMPARE(sandbox::visibleHostDir(QStringLiteral("/opt/node/bin")), QStringLiteral("/opt/node/bin"));
        QCOMPARE(sandbox::visibleHostDir(QStringLiteral("/usrlocal")), QStringLiteral("/usrlocal")); // no es /usr
    }

    void bridgeRunsHostPrograms()
    {
        // Dos carpetas del «sistema»: git en las dos (gana la primera, como en el PATH), clangd
        // solo en la segunda; edita no se toca, ni lo que no es ejecutable
        const QString first = _tmp.filePath("host/first");
        const QString second = _tmp.filePath("host/second");
        writeExecutable(first + "/git", "#!/bin/sh\n");
        writeExecutable(second + "/git", "#!/bin/sh\n");
        writeExecutable(second + "/clangd", "#!/bin/sh\n");
        writeExecutable(second + "/edita", "#!/bin/sh\n");
        QFile plain(second + "/LEEME");
        QVERIFY(plain.open(QIODevice::WriteOnly));
        plain.close();
        // El «host-spawn» de la prueba: dice en qué carpeta está y qué recibe
        const QString launcher = _tmp.filePath("launcher.sh");
        writeExecutable(launcher, "#!/bin/sh\necho \"HOST $PWD: $*\"\n");
        const QString bridge = _tmp.filePath("bridge");
        QCOMPARE(sandbox::createBridge(bridge, {first, second, _tmp.filePath("no-existe")}, {launcher},
                                       {QStringLiteral("edita")}),
                 2);
        QVERIFY(QFileInfo(bridge + "/git").isSymLink());
        QVERIFY(QFileInfo::exists(bridge + "/clangd"));
        QVERIFY(!QFileInfo::exists(bridge + "/edita"));
        QVERIFY(!QFileInfo::exists(bridge + "/LEEME"));
        // Por el PATH, como lo haría cualquier QProcess de Edita o de un plugin
        QProcess p;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("PATH"), bridge + QLatin1Char(':') + env.value(QStringLiteral("PATH")));
        p.setProcessEnvironment(env);
        p.setWorkingDirectory(_tmp.path());
        p.start(bridge + "/git", {QStringLiteral("status"), QStringLiteral("con espacios")});
        QVERIFY(p.waitForFinished(10000));
        QCOMPARE(QString::fromUtf8(p.readAllStandardOutput()).trimmed(),
                 QStringLiteral("HOST %1: git status con espacios").arg(QDir(_tmp.path()).canonicalPath()));
        // Se rehace de cero en cada arranque
        QCOMPARE(sandbox::createBridge(bridge, {second}, {launcher}, {}), 3);
        QVERIFY(QFileInfo::exists(bridge + "/edita"));
    }

    void launcherQuotesItsWords()
    {
        const QString launcher = _tmp.filePath("con 'comillas'.sh");
        writeExecutable(launcher, "#!/bin/sh\necho \"[$1] [$2]\"\n");
        const QString host = _tmp.filePath("host/quotes");
        writeExecutable(host + "/tool", "#!/bin/sh\n");
        const QString bridge = _tmp.filePath("bridge-quotes");
        QCOMPARE(sandbox::createBridge(bridge, {host}, {launcher, QStringLiteral("--flag")}, {}), 1);
        QProcess p;
        p.start(bridge + "/tool", {QStringLiteral("x")});
        QVERIFY(p.waitForFinished(10000));
        QCOMPARE(QString::fromUtf8(p.readAllStandardOutput()).trimmed(), QStringLiteral("[--flag] [tool]"));
    }

    void dictionariesOfTheSystem()
    {
        qputenv("EDITA_SANDBOX", "flatpak");
        QVERIFY(SpellChecker::dictionaryDirs({}).contains(QStringLiteral("/run/host/usr/share/hunspell")));
        qputenv("EDITA_SANDBOX", "none");
        QVERIFY(!SpellChecker::dictionaryDirs({}).contains(QStringLiteral("/run/host/usr/share/hunspell")));
    }

    void fileAssociationsAreTheDesktopsInFlatpak()
    {
        qputenv("EDITA_SANDBOX", "flatpak");
        {
            PreferencesDialog dialog(Preferences{}, {});
            auto* groups = dialog.findChild<QListWidget*>(QStringLiteral("assocGroups"));
            QVERIFY(groups);
            QVERIFY(!groups->isEnabled());
            QVERIFY(!dialog.findChild<QListWidget*>(QStringLiteral("assocRegistered"))->isEnabled());
        }
        qputenv("EDITA_SANDBOX", "none");
        PreferencesDialog dialog(Preferences{}, {});
        QVERIFY(dialog.findChild<QListWidget*>(QStringLiteral("assocGroups"))->isEnabled());
    }
};

QTEST_MAIN(TestSandbox)
#include "tst_sandbox.moc"
