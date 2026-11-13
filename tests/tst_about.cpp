// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 91: Ayuda › Acerca de Edita. Como el AboutDlg de Notepad++ (la versión, la
// arquitectura, la compilación y la licencia), más los componentes de terceros con sus
// licencias, si hay una versión nueva, enlaces útiles y copiar la información de la versión.

#include "FakePrompter.h"
#include "AppInfo.h"
#include "core/AboutInfo.h"
#include "ui/MainWindow.h"
#include "ui/NotificationBar.h"

#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSysInfo>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>
#include <QTimer>
#include <QTreeWidget>

#include <map>

using namespace edita;
using edita::test::FakePrompter;
namespace ab = edita::about;

namespace {

QByteArray resource(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

} // namespace

class TestAbout : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;
    QList<QUrl> _opened;

    std::unique_ptr<MainWindow> window()
    {
        auto w = std::make_unique<MainWindow>();
        w->setPrompter(std::make_unique<FakePrompter>());
        _opened.clear();
        w->setUrlOpener([this](const QUrl& url) { _opened << url; return true; });
        QCoreApplication::processEvents();
        return w;
    }

    // Abre Acerca de y ejecuta check con el diálogo abierto (es modal); después lo cierra
    static void withAboutDialog(MainWindow& w, const std::function<void(QDialog*)>& check)
    {
        bool ran = false;
        QTimer::singleShot(0, &w, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (dialog && dialog->objectName() == QLatin1String("aboutBox")) {
                check(dialog);
                ran = true;
            }
            if (dialog)
                dialog->reject();
        });
        w.findChild<QAction*>(QStringLiteral("actionAbout"))->trigger();
        QVERIFY2(ran, "no se abrió el diálogo Acerca de");
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    void init() { qputenv("EDITA_UPDATE_URL", "none"); }

    // --- El núcleo -----------------------------------------------------------------------

    void components()
    {
        const std::vector<ab::Component> list = ab::components();
        std::map<QString, ab::Component> byName;
        for (const ab::Component& c : list) {
            QVERIFY2(!c.version.isEmpty() && !c.license.isEmpty() && c.homepage.isValid() && !c.usedFor.isEmpty(),
                     qPrintable(c.name));
            byName[c.name] = c;
        }
        for (const char* name : {"Qt", "Scintilla", "Lexilla", "Boost.Regex", "uchardet", "pugixml", "libvterm", "md4c", "OpenSSL",
                                 "Notepad++"})
            QVERIFY2(byName.count(QLatin1String(name)), name);
        // Las versiones salen de third_party (y de las bibliotecas con las que se ejecuta)
        QCOMPARE(byName["Qt"].version, QString::fromLatin1(qVersion()));
        QCOMPARE(byName["Scintilla"].version, QStringLiteral("5.6.9"));
        QCOMPARE(byName["Lexilla"].version, QStringLiteral("5.5.5"));
        QCOMPARE(byName["pugixml"].version, QStringLiteral("1.16"));
        QCOMPARE(byName["Boost.Regex"].version, QStringLiteral("1.92"));
        QCOMPARE(byName["libvterm"].version, QStringLiteral("0.3"));
        QCOMPARE(byName["md4c"].version, QStringLiteral("0.5.2"));
        QVERIFY(byName["OpenSSL"].version.front().isDigit());
        QCOMPARE(byName["Scintilla"].license, QStringLiteral("HPND"));
        QCOMPARE(byName["Boost.Regex"].license, QStringLiteral("BSL-1.0"));
        // Los avisos que piden las licencias MIT, BSD y Boost van dentro de Edita
        for (const char* name : {"Scintilla", "Lexilla", "Boost.Regex", "pugixml", "libvterm", "md4c"}) {
            const QString path = byName[QLatin1String(name)].licenseText;
            QVERIFY2(!path.isEmpty() && resource(path).size() > 300, name);
        }
        QVERIFY(resource(byName["Scintilla"].licenseText).contains("Neil Hodgson"));
        QVERIFY(resource(byName["Boost.Regex"].licenseText).contains("Boost Software License - Version 1.0"));
        QVERIFY(resource(QStringLiteral(":/edita/COPYING")).contains("GNU GENERAL PUBLIC LICENSE"));
    }

    void install()
    {
        auto none = [](const QString&) { return false; };
        auto dpkg = [](const QString& path) { return path == QLatin1String("/var/lib/dpkg/info/edita.list"); };
        QCOMPARE(ab::detectInstall(QStringLiteral("/tmp/.mount_EditaX/usr/bin/edita"), QStringLiteral("/home/ana/Edita.AppImage"), none),
                 ab::Install::AppImage);
        QCOMPARE(ab::detectInstall(QStringLiteral("/opt/edita/usr/bin/edita"), QString(), dpkg), ab::Install::Deb);
        QCOMPARE(ab::detectInstall(QStringLiteral("/opt/edita/usr/bin/edita"), QString(), none), ab::Install::Rpm);
        QCOMPARE(ab::detectInstall(QStringLiteral("/usr/bin/edita"), QString(), none), ab::Install::System);
        QCOMPARE(ab::detectInstall(QStringLiteral("/home/ana/edita/build/edita"), QString(), none), ab::Install::Source);
        QCOMPARE(ab::installName(ab::Install::Deb), QStringLiteral(".deb"));
        QCOMPARE(ab::installName(ab::Install::AppImage), QStringLiteral("AppImage"));
    }

    void texts()
    {
        QCOMPARE(ab::versionLine(QStringLiteral("1.0.0-rc.3"), QStringLiteral("x86_64"), ab::Install::AppImage),
                 QStringLiteral("Edita 1.0.0-rc.3 (x86_64, AppImage)"));
        const QString copied = ab::copyText(QStringLiteral("Edita 1.0.0 (arm64, .deb)"), QStringLiteral("a5777d0, 2026-11-13"),
                                            QStringLiteral("Ubuntu 24.04, Qt 6.8.3, wayland"),
                                            {QStringLiteral("Hex Editor (1.0.0)"), QStringLiteral("FTP (1.0.0)")});
        QCOMPARE(copied, QStringLiteral("Edita 1.0.0 (arm64, .deb)\nBuild: a5777d0, 2026-11-13\nSystem: Ubuntu 24.04, Qt 6.8.3, wayland\n"
                                        "Plugins: Hex Editor (1.0.0), FTP (1.0.0)"));
        QCOMPARE(ab::copyText(QStringLiteral("Edita 1.0.0"), QString(), QStringLiteral("Fedora"), {}),
                 QStringLiteral("Edita 1.0.0\nSystem: Fedora\nPlugins: none"));
        QCOMPARE(ab::releaseNotesUrl(QStringLiteral("https://github.com/alktrip/edita"), QStringLiteral("1.0.0-rc.3")),
                 QUrl(QStringLiteral("https://github.com/alktrip/edita/releases/tag/v1.0.0-rc.3")));
        // Compilado desde git: el commit corto y su fecha
        if (!ab::buildCommit().isEmpty()) {
            QVERIFY(ab::buildCommit().size() >= 7);
            QVERIFY(QDate::fromString(ab::buildDate(), Qt::ISODate).isValid());
        }
    }

    // --- El diálogo ----------------------------------------------------------------------

    void dialog()
    {
        auto w = window();
        withAboutDialog(*w, [this](QDialog* d) {
            auto* tabs = d->findChild<QTabWidget*>(QStringLiteral("aboutTabs"));
            QVERIFY(tabs);
            QCOMPARE(tabs->count(), 3);
            // La versión, con la arquitectura y la forma de instalación
            auto* version = d->findChild<QLabel*>(QStringLiteral("aboutVersion"));
            QVERIFY(version->text().contains(QString::fromUtf8(kAppVersion)));
            QVERIFY(version->text().contains(QSysInfo::currentCpuArchitecture()));
            // Buscar versiones nuevas está desactivado en las pruebas
            QVERIFY(d->findChild<QLabel*>(QStringLiteral("aboutUpdate"))->text().contains(QLatin1String("disabled")));

            // Los enlaces: las notas de esta versión
            auto* links = d->findChild<QLabel*>(QStringLiteral("aboutLinks"));
            QVERIFY(links->text().contains(QLatin1String("Release Notes")));
            emit links->linkActivated(QStringLiteral("notes"));
            QCOMPARE(_opened.value(0), ab::releaseNotesUrl(QString::fromUtf8(kHomepageUrl), QString::fromUtf8(kAppVersion)));

            // Copiar: la versión y el sistema
            QApplication::clipboard()->clear();
            d->findChild<QPushButton*>(QStringLiteral("aboutCopy"))->click();
            const QString copied = QApplication::clipboard()->text();
            QVERIFY(copied.startsWith(QStringLiteral("Edita %1").arg(QString::fromUtf8(kAppVersion))));
            QVERIFY(copied.contains(QLatin1String("System: ")));
            QVERIFY(copied.contains(QLatin1String("Plugins: ")));

            // Componentes: elegir uno muestra su licencia
            auto* list = d->findChild<QTreeWidget*>(QStringLiteral("aboutComponents"));
            QVERIFY(list->topLevelItemCount() >= 10);
            QTreeWidgetItem* scintilla = list->findItems(QStringLiteral("Scintilla"), Qt::MatchExactly).value(0);
            QVERIFY(scintilla);
            QCOMPARE(scintilla->text(1), QStringLiteral("5.6.9"));
            list->setCurrentItem(scintilla);
            QVERIFY(d->findChild<QPlainTextEdit*>(QStringLiteral("aboutComponentLicense"))->toPlainText().contains(QLatin1String("Neil Hodgson")));
            QTreeWidgetItem* qt = list->findItems(QStringLiteral("Qt"), Qt::MatchExactly).value(0);
            list->setCurrentItem(qt);
            QVERIFY(d->findChild<QPlainTextEdit*>(QStringLiteral("aboutComponentLicense"))->toPlainText().contains(QLatin1String("LGPL-3.0")));

            // Licencia: el texto de la GPL
            QVERIFY(d->findChild<QPlainTextEdit*>(QStringLiteral("aboutLicense"))->toPlainText().contains(QLatin1String("GNU GENERAL PUBLIC LICENSE")));
        });
    }

    // Con la búsqueda de versiones activada, el diálogo dice si hay una nueva (sin la barra
    // de avisos)
    void newVersionInTheDialog()
    {
        const QString json = _tmp.filePath(QStringLiteral("latest.json"));
        QFile fixture(QStringLiteral(EDITA_FIXTURES_DIR "/update/latest.json"));
        QVERIFY(fixture.open(QIODevice::ReadOnly));
        writeFile(json, fixture.readAll());
        qputenv("EDITA_UPDATE_URL", json.toUtf8());
        auto w = window();
        withAboutDialog(*w, [this, &w](QDialog* d) {
            auto* status = d->findChild<QLabel*>(QStringLiteral("aboutUpdate"));
            QTRY_VERIFY2(status->text().contains(QLatin1String("Edita 99.0.0 is available")), qPrintable(status->text()));
            emit status->linkActivated(QStringLiteral("download"));
            QCOMPARE(_opened.value(0), QUrl(QStringLiteral("https://github.com/alktrip/edita/releases/tag/v99.0.0")));
            QVERIFY(w->notificationBar()->currentText().isEmpty());
        });
        // La segunda vez ya lo sabe: no vuelve a preguntar
        qputenv("EDITA_UPDATE_URL", "/no/existe.json");
        withAboutDialog(*w, [](QDialog* d) {
            QVERIFY(d->findChild<QLabel*>(QStringLiteral("aboutUpdate"))->text().contains(QLatin1String("99.0.0")));
        });

        // Al día
        QFile again(QStringLiteral(EDITA_FIXTURES_DIR "/update/latest.json"));
        QVERIFY(again.open(QIODevice::ReadOnly));
        writeFile(json, again.readAll().replace("99.0.0", "0.0.1"));
        qputenv("EDITA_UPDATE_URL", json.toUtf8());
        auto fresh = window();
        withAboutDialog(*fresh, [](QDialog* d) {
            auto* status = d->findChild<QLabel*>(QStringLiteral("aboutUpdate"));
            QTRY_VERIFY2(status->text().contains(QLatin1String("latest version")), qPrintable(status->text()));
        });
    }
};

QTEST_MAIN(TestAbout)
#include "tst_about.moc"
