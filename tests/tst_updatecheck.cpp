// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 64: avisar de una versión nueva (el actualizador de N++, gup.exe, que aquí solo avisa)

#include "AppInfo.h"
#include "FakePrompter.h"
#include "core/ConfigXml.h"
#include "core/Preferences.h"
#include "core/UpdateCheck.h"
#include "syntax/SyntaxManager.h"
#include "ui/MainWindow.h"
#include "ui/NotificationBar.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;
using edita::test::FakePrompter;
namespace uc = edita::update_check;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QByteArray release(const QString& tag)
{
    QFile f(QStringLiteral(EDITA_FIXTURES_DIR "/update/latest.json"));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll().replace("v99.0.0", tag.toUtf8()).replace("99.0.0", tag.mid(1).toUtf8());
}

QPushButton* button(MainWindow& w, const QString& text)
{
    for (QPushButton* b : w.notificationBar()->findChildren<QPushButton*>()) {
        if (b->text() == text)
            return b;
    }
    return nullptr;
}

} // namespace

class TestUpdateCheck : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;
    FakePrompter* _prompter = nullptr;
    QList<QUrl> _opened;

    QString responsePath() const { return _tmp.filePath("latest.json"); }

    std::unique_ptr<MainWindow> window()
    {
        auto w = std::make_unique<MainWindow>();
        auto prompter = std::make_unique<FakePrompter>();
        _prompter = prompter.get();
        w->setPrompter(std::move(prompter));
        _opened.clear();
        w->setUrlOpener([this](const QUrl& url) { _opened << url; return true; });
        QCoreApplication::processEvents();
        return w;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    void init()
    {
        QDir(_tmp.filePath("config")).removeRecursively();
        qputenv("EDITA_UPDATE_URL", responsePath().toUtf8());
    }

    // SemVer con etapas: la final va después de sus betas, y beta.10 después de beta.2
    void comparesVersions()
    {
        QVERIFY(uc::compareVersions("0.9.0-beta.2", "0.9.0-beta.10") < 0);
        QVERIFY(uc::compareVersions("0.9.0-beta.2", "0.9.0") < 0);
        QVERIFY(uc::compareVersions("0.9.0", "0.9.1-beta.1") < 0);
        QVERIFY(uc::compareVersions("0.9.0-alpha.3", "0.9.0-beta.1") < 0);
        QVERIFY(uc::compareVersions("0.9.0-rc.1", "0.9.0-beta.5") > 0);
        QVERIFY(uc::compareVersions("0.9.0-beta.11", "1.0.0-rc.1") < 0); // de la última beta a la rc
        QVERIFY(uc::compareVersions("1.0.0-rc.1", "1.0.0") < 0);
        QVERIFY(uc::compareVersions("1.0.0", "0.10.0") > 0);
        QVERIFY(uc::compareVersions("0.10.0", "0.9.9") > 0); // números, no texto
        QCOMPARE(uc::compareVersions("v0.9.0-beta.2", "0.9.0-beta.2"), 0);
        QVERIFY(uc::compareVersions("0.9.0-beta", "0.9.0-beta.1") < 0);
        QVERIFY(uc::compareVersions("basura", "0.0.1") < 0);
        QVERIFY(!uc::isValidVersion("1.0"));
        QVERIFY(uc::isValidVersion(QString::fromUtf8(kAppVersion)));
    }

    // La respuesta de GitHub (releases/latest)
    void parsesTheLatestRelease()
    {
        uc::Release r;
        QVERIFY(uc::parseLatestRelease(release("v1.2.3-beta.4"), r));
        QCOMPARE(r.version, QStringLiteral("1.2.3-beta.4"));
        QCOMPARE(r.page, QUrl("https://github.com/alktrip/edita/releases/tag/v1.2.3-beta.4"));
        QCOMPARE(r.appImage.fileName(), QStringLiteral("Edita-1.2.3-beta.4-x86_64.AppImage"));
        QString error;
        QVERIFY(!uc::parseLatestRelease("{\"message\": \"Not Found\"}", r, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!uc::parseLatestRelease("no es json", r));
    }

    // Fase 79: la lista de versiones (releases?per_page=30). Una versión estable solo avisa de
    // versiones estables; una beta, también de las betas. Nunca de un borrador ni de «continuous»
    void choosesTheNewestRelease()
    {
        auto entry = [](const char* tag, bool prerelease = false, bool draft = false) {
            return QStringLiteral("{\"tag_name\": \"%1\", \"html_url\": \"https://github.com/alktrip/edita/releases/tag/%1\", "
                                  "\"draft\": %2, \"prerelease\": %3, \"assets\": [{\"name\": \"Edita-%4-x86_64.AppImage\", "
                                  "\"browser_download_url\": \"https://example.org/Edita-%4-x86_64.AppImage\"}]}")
                .arg(QString::fromLatin1(tag), draft ? "true" : "false", prerelease ? "true" : "false",
                     QString::fromLatin1(tag).mid(1));
        };
        const QByteArray list = QStringLiteral("[%1]")
                                    .arg(QStringList{entry("continuous", true), entry("v1.1.0-beta.1"),
                                                     entry("v1.0.0"), entry("v1.0.1"), entry("v2.0.0", false, true),
                                                     entry("v1.0.5", true), entry("v0.9.0-beta.11")}
                                             .join(QLatin1Char(',')))
                                    .toUtf8();
        uc::Release r;
        QVERIFY(uc::parseReleases(list, false, r));
        QCOMPARE(r.version, QStringLiteral("1.0.1"));
        QCOMPARE(r.page, QUrl("https://github.com/alktrip/edita/releases/tag/v1.0.1"));
        QCOMPARE(r.appImage.fileName(), QStringLiteral("Edita-1.0.1-x86_64.AppImage"));
        QVERIFY(uc::parseReleases(list, true, r));
        QCOMPARE(r.version, QStringLiteral("1.1.0-beta.1"));

        // Solo hay betas y Edita es estable: nada que avisar (versión vacía, «al día»)
        const QByteArray betas = QStringLiteral("[%1]").arg(entry("v0.9.0-beta.11")).toUtf8();
        QVERIFY(uc::parseReleases(betas, false, r));
        QVERIFY(r.version.isEmpty());
        QVERIFY(uc::compareVersions(r.version, QStringLiteral("1.0.0")) < 0);
        QVERIFY(uc::parseReleases("[]", false, r));
        QVERIFY(r.version.isEmpty());

        // Un solo objeto (releases/latest o EDITA_UPDATE_URL) sigue sirviendo, con el mismo filtro
        QVERIFY(uc::parseReleases(release("v99.0.0"), false, r));
        QCOMPARE(r.version, QStringLiteral("99.0.0"));
        QVERIFY(uc::parseReleases(release("v98.1.0-beta.1"), false, r));
        QVERIFY(r.version.isEmpty());
        QVERIFY(uc::parseReleases(release("v98.1.0-beta.1"), true, r));
        QCOMPARE(r.version, QStringLiteral("98.1.0-beta.1"));

        QString error;
        QVERIFY(!uc::parseReleases("{\"message\": \"Not Found\"}", false, r, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!uc::parseReleases("no es json", true, r));
    }

    // Quien pasa de una beta (cada día) a una estable (cada 15 días) o al revés no se queda
    // con el intervalo de la otra; si lo había cambiado, se respeta
    void intervalFollowsTheChannel()
    {
        QCOMPARE(defaultUpdateIntervalDays(true), 1);
        QCOMPARE(defaultUpdateIntervalDays(false), 15);
        QCOMPARE(migratedUpdateInterval(1, true, false), 15);
        QCOMPARE(migratedUpdateInterval(15, false, true), 1);
        QCOMPARE(migratedUpdateInterval(7, true, false), 7);
        QCOMPARE(migratedUpdateInterval(1, true, true), 1);
        QCOMPARE(migratedUpdateInterval(15, false, false), 15);

        // Se guarda quién lo escribió. Sin el atributo (las betas 1-11) es una beta
        ConfigXml config;
        Preferences p;
        storePreferences(config, p);
        QCOMPARE(config.guiConfig("noUpdate", "editaPrerelease"), kAppPrerelease ? QStringLiteral("yes") : QStringLiteral("no"));
        config.setGuiConfig("noUpdate", "editaPrerelease", QString());
        config.setGuiConfig("noUpdate", "intervalDays", "1");
        QCOMPARE(loadPreferences(config).updateIntervalDays, defaultUpdateIntervalDays(kAppPrerelease));
        config.setGuiConfig("noUpdate", "editaPrerelease", "no");
        config.setGuiConfig("noUpdate", "intervalDays", "15");
        QCOMPARE(loadPreferences(config).updateIntervalDays, defaultUpdateIntervalDays(kAppPrerelease));
        config.setGuiConfig("noUpdate", "intervalDays", "3");
        QCOMPARE(loadPreferences(config).updateIntervalDays, 3);
    }

    // Cuándo toca: al arrancar, cada intervalDays días (como N++), y lo que se guarda
    void scheduleAndSettings()
    {
        const QDate today(2026, 11, 5);
        uc::State state;
        QVERIFY(uc::isDue(true, state, today));
        QVERIFY(!uc::isDue(false, state, today));
        uc::scheduleNext(state, 15, today);
        QCOMPARE(state.nextCheck, QDate(2026, 11, 20));
        QVERIFY(!uc::isDue(true, state, today.addDays(14)));
        QVERIFY(uc::isDue(true, state, today.addDays(15)));

        ConfigXml config;
        state.skippedVersion = QStringLiteral("1.0.0");
        uc::writeState(config, state);
        QCOMPARE(config.guiConfig("noUpdate", "nextUpdateDate"), QStringLiteral("20261120"));
        const uc::State back = uc::readState(config);
        QCOMPARE(back.nextCheck, state.nextCheck);
        QCOMPARE(back.skippedVersion, QStringLiteral("1.0.0"));

        // Las preferencias, en el mismo <GUIConfig> que N++: autoUpdateMode e intervalDays
        Preferences p;
        QCOMPARE(p.updateIntervalDays, kAppPrerelease ? 1 : 15);
        p.updateOnStartup = false;
        p.updateIntervalDays = 7;
        storePreferences(config, p);
        QCOMPARE(config.guiConfig("noUpdate", "autoUpdateMode"), QStringLiteral("0"));
        QCOMPARE(config.guiConfig("noUpdate", "intervalDays"), QStringLiteral("7"));
        const Preferences read = loadPreferences(config);
        QVERIFY(!read.updateOnStartup);
        QCOMPARE(read.updateIntervalDays, 7);
        config.setGuiConfig("noUpdate", "autoUpdateMode", "2"); // N++: «al salir» también comprueba
        QVERIFY(loadPreferences(config).updateOnStartup);

        // De dónde: la API de GitHub del repositorio de Edita, o EDITA_UPDATE_URL
        qunsetenv("EDITA_UPDATE_URL");
        QCOMPARE(uc::releaseUrl("https://github.com/alktrip/edita"),
                 QUrl("https://api.github.com/repos/alktrip/edita/releases?per_page=30"));
        QVERIFY(uc::releaseUrl("https://example.org/edita").isEmpty());
        qputenv("EDITA_UPDATE_URL", "none");
        QVERIFY(uc::releaseUrl("https://github.com/alktrip/edita").isEmpty());
        qputenv("EDITA_UPDATE_URL", "/tmp/x.json");
        QCOMPARE(uc::releaseUrl("https://github.com/alktrip/edita"), QUrl::fromLocalFile("/tmp/x.json"));
    }

    // Ayuda › Buscar actualizaciones: hay una nueva → el aviso, con la página y «Omitir»
    void manualCheckFindsANewVersion()
    {
        writeFile(responsePath(), release("v99.0.0"));
        auto w = window();
        QAction* check = w->findChild<QAction*>(QStringLiteral("actionCheckForUpdates"));
        QVERIFY(check);
        check->trigger();
        QTRY_VERIFY(!w->isCheckingForUpdates());
        QVERIFY2(w->notificationBar()->currentText().contains(QStringLiteral("Edita 99.0.0 is available")),
                 qPrintable(w->notificationBar()->currentText()));
        QVERIFY(w->notificationBar()->currentText().contains(QString::fromUtf8(kAppVersion)));
        QPushButton* page = button(*w, QStringLiteral("Download Page"));
        QVERIFY(page);
        page->click();
        QCOMPARE(_opened, QList<QUrl>{QUrl("https://github.com/alktrip/edita/releases/tag/v99.0.0")});

        // «Omitir esta versión»: al arrancar ya no se avisa de ella; a mano, sí
        w->checkForUpdates(true);
        QTRY_VERIFY(!w->isCheckingForUpdates());
        button(*w, QStringLiteral("Skip This Version"))->click();
        QCOMPARE(uc::readState(w->syntax().config()).skippedVersion, QStringLiteral("99.0.0"));
        w->checkForUpdates(false);
        QTRY_VERIFY(!w->isCheckingForUpdates());
        QVERIFY(w->notificationBar()->currentText().isEmpty());
        w->checkForUpdates(true);
        QTRY_VERIFY(!w->isCheckingForUpdates());
        QVERIFY(w->notificationBar()->currentText().contains(QStringLiteral("99.0.0")));
        // Una más nueva que la omitida, sí
        writeFile(responsePath(), release("v99.0.1"));
        w->notificationBar()->dismiss();
        w->checkForUpdates(false);
        QTRY_VERIFY(!w->isCheckingForUpdates());
        QVERIFY(w->notificationBar()->currentText().contains(QStringLiteral("99.0.1")));
    }

    // A mano, sin novedades o sin conexión, lo dice; al arrancar, calla
    void upToDateAndErrors()
    {
        writeFile(responsePath(), release("v0.0.1"));
        auto w = window();
        w->checkForUpdates(false);
        QTRY_VERIFY(!w->isCheckingForUpdates());
        QVERIFY(w->notificationBar()->currentText().isEmpty());
        w->checkForUpdates(true);
        QTRY_VERIFY(!w->isCheckingForUpdates());
        QVERIFY(w->notificationBar()->currentText().contains(QStringLiteral("up to date")));
        // La misma versión que esta tampoco es nueva
        writeFile(responsePath(), release(QStringLiteral("v") + QString::fromUtf8(kAppVersion)));
        w->notificationBar()->dismiss();
        w->checkForUpdates(false);
        QTRY_VERIFY(!w->isCheckingForUpdates());
        QVERIFY(w->notificationBar()->currentText().isEmpty());

        QFile::remove(responsePath());
        w->checkForUpdates(false);
        QTRY_VERIFY(!w->isCheckingForUpdates());
        QVERIFY(_prompter->warnings.isEmpty());
        w->checkForUpdates(true);
        QTRY_VERIFY(!w->isCheckingForUpdates());
        QCOMPARE(_prompter->warnings.size(), 1);
        QVERIFY(_prompter->warnings[0].contains(QStringLiteral("Could not check")));
        // Desactivado (EDITA_UPDATE_URL=none, o un paquete de una distribución)
        qputenv("EDITA_UPDATE_URL", "none");
        w->checkForUpdates(true);
        QVERIFY(!w->isCheckingForUpdates());
        QVERIFY(_prompter->warnings.last().contains(QStringLiteral("disabled")));
    }

    // Al arrancar: solo si toca, y entonces fija la próxima fecha
    void startupCheckWhenDue()
    {
        writeFile(responsePath(), release("v99.0.0"));
        auto w = window();
        const QDate today = QDate::currentDate();
        w->checkForUpdatesIfDue(today);
        QTRY_VERIFY(!w->isCheckingForUpdates());
        QVERIFY(w->notificationBar()->currentText().contains(QStringLiteral("99.0.0")));
        const int interval = w->preferences().updateIntervalDays;
        QCOMPARE(uc::readState(w->syntax().config()).nextCheck, today.addDays(interval));
        // Guardado en config.xml
        QFile config(w->syntax().configPath());
        QVERIFY(config.open(QIODevice::ReadOnly));
        QVERIFY(config.readAll().contains("name=\"noUpdate\""));
        // Hasta esa fecha, no
        w->notificationBar()->dismiss();
        w->checkForUpdatesIfDue(today.addDays(interval - 1));
        QVERIFY(!w->isCheckingForUpdates());
        // Ni si el usuario lo desactivó
        Preferences p = w->preferences();
        p.updateOnStartup = false;
        w->setPreferences(p);
        w->checkForUpdatesIfDue(today.addDays(interval + 30));
        QVERIFY(!w->isCheckingForUpdates());
    }
};

QTEST_MAIN(TestUpdateCheck)
#include "tst_updatecheck.moc"
