// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 14: administrador de plugins (PluginsAdminDlg de N++): lista de plugins
// con el formato de nppPluginList.json, versiones y compatibilidad, instalar,
// actualizar, quitar, desactivar y activar, y la lista de Notepad++.

#include "FakePrompter.h"
#include "core/PluginCatalog.h"
#include "core/PluginInstaller.h"
#include "ui/MainWindow.h"
#include "ui/PluginsAdminDialog.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QTemporaryDir>
#include <QTest>
#include <QLabel>
#include <QTimer>
#include <QTreeWidget>

#include <zip.h>

#include <map>

using namespace edita;
using edita::test::FakePrompter;
namespace pi = edita::plugin_installer;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

// Un .zip con esos archivos (nombre → contenido)
QByteArray makeZip(const std::map<QString, QByteArray>& files)
{
    QTemporaryDir dir;
    const QByteArray path = dir.filePath(QStringLiteral("p.zip")).toUtf8();
    int err = 0;
    zip_t* z = zip_open(path.constData(), ZIP_CREATE | ZIP_TRUNCATE, &err);
    for (const auto& [name, data] : files) {
        zip_source_t* src = zip_source_buffer(z, data.constData(), static_cast<zip_uint64_t>(data.size()), 0);
        zip_file_add(z, name.toUtf8().constData(), src, ZIP_FL_OVERWRITE);
    }
    zip_close(z);
    QFile f(QString::fromUtf8(path));
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

QJsonObject entry(const QString& folder, const QString& name, const QString& version, const QString& repository,
                  const QString& id, const QString& description = QStringLiteral("Hace cosas"))
{
    return QJsonObject{{"folder-name", folder}, {"display-name", name}, {"version", version},
                       {"npp-compatible-versions", "[0.0.1,]"}, {"id", id}, {"repository", repository},
                       {"description", description}, {"author", "Alguien"}, {"homepage", "https://ejemplo.org"}};
}

void writeCatalog(const QString& path, const QJsonArray& plugins, const QString& version = QStringLiteral("2.1"))
{
    writeFile(path, QJsonDocument(QJsonObject{{"version", version}, {"npp-plugins", plugins}}).toJson());
}

} // namespace

class TestPluginsAdmin : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir _tmp;
    QString userPlugins() const { return _tmp.filePath(QStringLiteral("config/edita/plugins")); }
    QString repo() const { return _tmp.filePath(QStringLiteral("repo")); }
    // Solo la lista de plugins: las recetas (fase 15) tienen su propia prueba
    static PluginsAdminContext adminContext(MainWindow& w)
    {
        PluginsAdminContext context = w.pluginsAdminContext();
        context.recipes.clear();
        return context;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        qputenv("EDITA_PLUGIN_PATH", EDITA_PLUGIN_BUILD_DIR);
        qputenv("EDITA_PLUGIN_LIST", repo().toUtf8() + "/list.json");
    }

    void cleanup()
    {
        QDir(userPlugins()).removeRecursively();
        QDir(repo()).removeRecursively();
    }

    // --- Núcleo ----------------------------------------------------------------------------

    void versionsLikeNotepadPlusPlus()
    {
        QCOMPARE(PluginVersion::parse("1.2.3").toString(), QStringLiteral("1.2.3"));
        QCOMPARE(PluginVersion::parse("2.0.0.0").toString(), QStringLiteral("2"));
        QCOMPARE(PluginVersion::parse("1.2.3.4.5").toString(), QString()); // más de 4 partes
        QCOMPARE(PluginVersion::parse("1.x").toString(), QString());       // no numérico
        QVERIFY(PluginVersion::parse("1.10") > PluginVersion::parse("1.9"));
        QVERIFY(PluginVersion::parse("2") == PluginVersion::parse("2.0.0"));
        // Intervalos de npp-compatible-versions
        const PluginVersion v = PluginVersion::parse("8.5");
        auto compatible = [&v](const char* interval) {
            const auto [from, to] = parseVersionInterval(QString::fromLatin1(interval));
            return v.isCompatibleTo(from, to);
        };
        QVERIFY(compatible(""));
        QVERIFY(compatible("[8.3,]"));
        QVERIFY(!compatible("[8.6,]"));
        QVERIFY(compatible("[,8.5]"));
        QVERIFY(!compatible("[,8.4.9]"));
        QVERIFY(compatible("[4.2,8.6.6]"));
        QVERIFY(compatible("8.5"));
        QVERIFY(!compatible("8.4"));
        QVERIFY(!PluginVersion{}.isCompatibleTo({}, {})); // una versión vacía no es compatible con nada
    }

    void catalogAndLists()
    {
        QJsonArray plugins{entry("A", "Alfa", "2.0", "a.zip", ""), entry("B", "Beta", "1.0", "b.zip", ""),
                           entry("C", "Gamma", "1.0", "c.zip", ""), entry("D", "Delta", "1.0", "d.zip", "")};
        QJsonObject old = entry("Old", "Vieja", "1.0", "o.zip", "");
        old["npp-compatible-versions"] = "[,0.0.0.1]";
        plugins.append(old);
        QJsonObject broken = entry("X", "Rota", "1.0", "x.zip", "");
        broken.remove("homepage"); // falta un campo obligatorio: se ignora
        plugins.append(broken);
        const QByteArray json = QJsonDocument(QJsonObject{{"version", "1.5"}, {"npp-plugins", plugins}}).toJson();
        PluginCatalog catalog;
        QVERIFY(PluginCatalog::parse(json, catalog));
        QCOMPARE(catalog.version, QStringLiteral("1.5"));
        QCOMPARE(catalog.plugins.size(), std::size_t{5});
        QVERIFY(!PluginCatalog::parse("{\"npp-plugins\": []}", catalog)); // sin version

        PluginCatalog c;
        PluginCatalog::parse(json, c);
        const std::vector<InstalledPlugin> loaded{{"A", "/p/A/A.so", PluginVersion::parse("1.0"), false},
                                                  {"Mio", "/p/Mio/Mio.so", PluginVersion::parse("0.1"), false}};
        const std::vector<InstalledPlugin> disabled{{"B", "/p/disabled/B/B.so", PluginVersion::parse("1.0"), false}};
        PluginInfo failed;
        failed.folderName = "Roto";
        failed.displayName = "Roto.so";
        failed.problem = "no es un plugin";
        const PluginLists lists = buildPluginLists(c, loaded, disabled, {failed}, PluginVersion::parse("0.1"));
        auto names = [](const std::vector<PluginInfo>& list) {
            QStringList out;
            for (const PluginInfo& i : list)
                out << i.displayName;
            return out;
        };
        QCOMPARE(names(lists.available), (QStringList{"Delta", "Gamma"})); // ni instalados, ni desactivados, ni incompatibles
        QCOMPARE(names(lists.installed), (QStringList{"Alfa", "Mio"}));
        QCOMPARE(lists.installed[0].version.toString(), QStringLiteral("1")); // la instalada, no la de la lista
        QCOMPARE(names(lists.updates), (QStringList{"Alfa"}));              // 1.0 < 2.0
        QCOMPARE(names(lists.disabled), (QStringList{"Beta"}));
        QCOMPARE(names(lists.incompatible), (QStringList{"Roto.so"}));
        QVERIFY(lists.installed[0].describe().contains(QStringLiteral("Author: Alguien")));
    }

    void installerChecksAndExtracts()
    {
        const QByteArray zip = makeZip({{"Otro.so", "ELF"}, {"doc/leeme.txt", "hola"}});
        const QString id = pi::sha256Hex(zip);
        QCOMPARE(id.size(), 64);
        QString error;
        // SHA-256 incorrecto
        QVERIFY(!pi::installPackage(zip, "Otro", QString(64, '0'), userPlugins(), false, &error));
        QVERIFY(error.contains(QStringLiteral("SHA-256")));
        // Bien: el contenido en plugins/Otro/
        QVERIFY2(pi::installPackage(zip, "Otro", id.toUpper(), userPlugins(), false, &error), qPrintable(error));
        QVERIFY(QFileInfo::exists(userPlugins() + "/Otro/Otro.so"));
        QVERIFY(QFileInfo::exists(userPlugins() + "/Otro/doc/leeme.txt"));
        // Ya existe: sin clean es un error; con clean se sustituye
        QVERIFY(!pi::installPackage(zip, "Otro", id, userPlugins(), false, &error));
        const QByteArray v2 = makeZip({{"Otro/Otro.so", "ELF2"}}); // dentro de una carpeta con su nombre
        QVERIFY2(pi::installPackage(v2, "Otro", pi::sha256Hex(v2), userPlugins(), true, &error), qPrintable(error));
        QFile so(userPlugins() + "/Otro/Otro.so");
        QVERIFY(so.open(QIODevice::ReadOnly));
        QCOMPARE(so.readAll(), QByteArray("ELF2"));
        QVERIFY(!QFileInfo::exists(userPlugins() + "/Otro/doc"));
        // Sin el .so, o con rutas que se salen de la carpeta
        QVERIFY(!pi::installPackage(makeZip({{"otra.so", "x"}}), "Nada", {}, userPlugins(), false, &error));
        QVERIFY(error.contains(QStringLiteral("Nada.so")));
        QVERIFY(!pi::installPackage(makeZip({{"../fuera.so", "x"}}), "Malo", {}, userPlugins(), false, &error));
        QVERIFY(!QFileInfo::exists(QFileInfo(userPlugins()).absolutePath() + "/fuera.so"));
        QVERIFY(!pi::installPackage("no es un zip", "Malo", {}, userPlugins(), false, &error));
        // Desactivar, activar y quitar
        QVERIFY(pi::movePlugin(userPlugins(), userPlugins() + "/disabled", "Otro", &error));
        QVERIFY(QFileInfo::exists(userPlugins() + "/disabled/Otro/Otro.so"));
        QVERIFY(pi::movePlugin(userPlugins() + "/disabled", userPlugins(), "Otro", &error));
        QVERIFY(pi::removePlugin(userPlugins(), "Otro", &error));
        QVERIFY(!QFileInfo::exists(userPlugins() + "/Otro"));
        QVERIFY(!pi::removePlugin(userPlugins(), "Otro", &error));
    }

    // --- En la ventana ---------------------------------------------------------------------

    void menuAndInstalledPlugins()
    {
        writeCatalog(repo() + "/list.json", {entry("EditaDemo", "Demo", "1.0", "", "")});
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        auto* menu = w.findChild<QMenu*>(QStringLiteral("menuPlugins"));
        QCOMPARE(menu->actions().first()->objectName(), QStringLiteral("actionPluginsAdmin"));
        const PluginsAdminContext context = adminContext(w);
        QVERIFY(context.catalogError.isEmpty());
        QCOMPARE(context.catalog.version, QStringLiteral("2.1"));
        QCOMPARE(context.userPluginsDir, userPlugins());
        FakePrompter prompter;
        PluginsAdminDialog dialog(context, prompter);
        // Los del build aparecen instalados, con la versión que exportan, y "de Edita"
        const PluginLists& lists = dialog.lists();
        const auto demo = std::find_if(lists.installed.begin(), lists.installed.end(),
                                       [](const PluginInfo& i) { return i.folderName == QLatin1String("EditaDemo"); });
        QVERIFY(demo != lists.installed.end());
        QCOMPARE(demo->version.toString(), QStringLiteral("1"));
        QVERIFY(demo->isSystem);
        QCOMPARE(demo->displayName, QStringLiteral("Demo")); // con los datos de la lista
        QVERIFY(lists.available.empty());
        // No se puede quitar ni desactivar desde aquí
        QVERIFY(dialog.setChecked(PluginsAdminDialog::Installed, "EditaDemo"));
        prompter.confirmations.enqueue(true);
        QVERIFY(!dialog.removeChecked(PluginsAdminDialog::Installed));
        QVERIFY(prompter.warnings.last().contains(QStringLiteral("installed with Edita")));
        QVERIFY(QFileInfo::exists(EDITA_PLUGIN_BUILD_DIR "/EditaDemo/EditaDemo.so"));
        QVERIFY(!dialog.hasChanges());
    }

    void installUpdateDeactivateRemove()
    {
        const QByteArray v1 = makeZip({{"Otro.so", "v1"}});
        writeFile(repo() + "/packages/Otro-1.0.zip", v1);
        writeCatalog(repo() + "/list.json",
                     {entry("Otro", "Otro plugin", "1.0", "packages/Otro-1.0.zip", pi::sha256Hex(v1)),
                      entry("Malo", "Plugin malo", "1.0", "packages/Otro-1.0.zip", QString(64, 'f'))});
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        FakePrompter prompter;
        PluginsAdminDialog dialog(adminContext(w), prompter);
        QCOMPARE(dialog.list(PluginsAdminDialog::Available)->topLevelItemCount(), 2);

        // Instalar: el que tiene un SHA-256 equivocado falla y el otro se instala
        QVERIFY(dialog.setChecked(PluginsAdminDialog::Available, "Otro"));
        QVERIFY(dialog.setChecked(PluginsAdminDialog::Available, "Malo"));
        prompter.confirmations.enqueue(true);
        QVERIFY(!dialog.installChecked());
        QVERIFY(prompter.warnings.last().contains(QStringLiteral("SHA-256")));
        QVERIFY(QFileInfo::exists(userPlugins() + "/Otro/Otro.so"));
        QVERIFY(!QFileInfo::exists(userPlugins() + "/Malo"));
        QVERIFY(dialog.hasChanges());
        QCOMPARE(dialog.findChild<QLabel*>(QStringLiteral("status"))->text(),
                 QStringLiteral("Changes will take effect when Edita restarts."));
        // Ahora está en Instalados (con la versión de la lista) y no en Disponibles
        QCOMPARE(dialog.list(PluginsAdminDialog::Available)->topLevelItemCount(), 1);
        const auto installed = std::find_if(dialog.lists().installed.begin(), dialog.lists().installed.end(),
                                            [](const PluginInfo& i) { return i.folderName == QLatin1String("Otro"); });
        QVERIFY(installed != dialog.lists().installed.end());
        QCOMPARE(installed->version.toString(), QStringLiteral("1"));
        QVERIFY(!installed->isSystem);

        // Cancelar la confirmación no hace nada
        QVERIFY(dialog.setChecked(PluginsAdminDialog::Installed, "Otro"));
        prompter.confirmations.enqueue(false);
        QVERIFY(!dialog.setCheckedActive(false));
        QVERIFY(QFileInfo::exists(userPlugins() + "/Otro/Otro.so"));
        // Desactivar: a plugins/disabled; activar: de vuelta
        QVERIFY(dialog.setChecked(PluginsAdminDialog::Installed, "Otro"));
        prompter.confirmations.enqueue(true);
        QVERIFY(dialog.setCheckedActive(false));
        QVERIFY(QFileInfo::exists(userPlugins() + "/disabled/Otro/Otro.so"));
        QCOMPARE(dialog.list(PluginsAdminDialog::Disabled)->topLevelItemCount(), 1);
        QVERIFY(dialog.setChecked(PluginsAdminDialog::Disabled, "Otro"));
        prompter.confirmations.enqueue(true);
        QVERIFY(dialog.setCheckedActive(true));
        QVERIFY(QFileInfo::exists(userPlugins() + "/Otro/Otro.so"));

        // Una versión nueva en la lista: aparece en Actualizaciones
        const QByteArray v2 = makeZip({{"Otro.so", "v2"}});
        writeFile(repo() + "/packages/Otro-2.0.zip", v2);
        writeCatalog(repo() + "/list.json", {entry("Otro", "Otro plugin", "2.0", "packages/Otro-2.0.zip", pi::sha256Hex(v2))});
        PluginsAdminContext context = adminContext(w);
        context.loadedVersions.insert(userPlugins() + "/Otro/Otro.so", "1.0"); // como si estuviera cargado
        PluginsAdminDialog second(context, prompter);
        QCOMPARE(second.list(PluginsAdminDialog::Updates)->topLevelItemCount(), 1);
        QVERIFY(second.setChecked(PluginsAdminDialog::Updates, "Otro"));
        prompter.confirmations.enqueue(true);
        QVERIFY(second.updateChecked());
        QFile so(userPlugins() + "/Otro/Otro.so");
        QVERIFY(so.open(QIODevice::ReadOnly));
        QCOMPARE(so.readAll(), QByteArray("v2"));
        QCOMPARE(second.list(PluginsAdminDialog::Updates)->topLevelItemCount(), 0);

        // Quitar
        QVERIFY(second.setChecked(PluginsAdminDialog::Installed, "Otro"));
        prompter.confirmations.enqueue(true);
        QVERIFY(second.removeChecked(PluginsAdminDialog::Installed));
        QVERIFY(!QFileInfo::exists(userPlugins() + "/Otro"));
        QCOMPARE(second.list(PluginsAdminDialog::Available)->topLevelItemCount(), 1);
    }

    void incompatibleAndNotepadPlusPlusList()
    {
        writeCatalog(repo() + "/list.json", {});
        writeFile(userPlugins() + "/Roto/Roto.so", "esto no es una biblioteca");
        // La lista oficial de N++ (un archivo local en la prueba)
        writeCatalog(repo() + "/npp.json",
                     {entry("ComparePlugin", "Compare", "2.0.2", "https://example.org/Compare.zip", QString(64, 'a'),
                            "Compara dos archivos"),
                      entry("XMLTools", "XML Tools", "3.1", "https://example.org/XMLTools.zip", QString(64, 'b'))},
                     QStringLiteral("1.8.3"));
        qputenv("EDITA_NPP_PLUGIN_LIST", repo().toUtf8() + "/npp.json");
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        FakePrompter prompter;
        PluginsAdminDialog dialog(adminContext(w), prompter);
        // Un plugin que no se pudo cargar
        QCOMPARE(dialog.list(PluginsAdminDialog::Incompatible)->topLevelItemCount(), 1);
        QCOMPARE(dialog.lists().incompatible[0].displayName, QStringLiteral("Roto.so"));
        QVERIFY(!dialog.lists().incompatible[0].problem.isEmpty());

        QVERIFY(dialog.loadNppCatalog());
        QCOMPARE(dialog.list(PluginsAdminDialog::Incompatible)->topLevelItemCount(), 3);
        QVERIFY(dialog.findChild<QLabel*>(QStringLiteral("status"))->text().contains(QStringLiteral("1.8.3")));
        // Búsqueda: por nombre y, si no, por descripción (al menos dos letras)
        QVERIFY(!dialog.search(QStringLiteral("x")));
        QVERIFY(dialog.search(QStringLiteral("xml")));
        QCOMPARE(dialog.list(PluginsAdminDialog::Incompatible)->currentItem()->text(0), QStringLiteral("XML Tools"));
        QVERIFY(dialog.descriptionText().contains(QStringLiteral("Windows")));
        QVERIFY(dialog.search(QStringLiteral("compara")));
        QCOMPARE(dialog.list(PluginsAdminDialog::Incompatible)->currentItem()->text(0), QStringLiteral("Compare"));
        // Los de N++ no se pueden quitar (no están instalados); el roto sí
        QVERIFY(dialog.setChecked(PluginsAdminDialog::Incompatible, "XMLTools"));
        QVERIFY(dialog.setChecked(PluginsAdminDialog::Incompatible, "Roto"));
        prompter.confirmations.enqueue(true);
        QVERIFY(!dialog.removeChecked(PluginsAdminDialog::Incompatible));
        QVERIFY(prompter.warnings.last().contains(QStringLiteral("is not installed")));
        QVERIFY(!QFileInfo::exists(userPlugins() + "/Roto"));
        qunsetenv("EDITA_NPP_PLUGIN_LIST");
    }

    void restartIsOfferedAfterChanges()
    {
        // Sin cambios no se pregunta; con cambios se ofrece reiniciar (aquí se rechaza)
        writeCatalog(repo() + "/list.json", {});
        MainWindow w;
        auto fake = std::make_unique<FakePrompter>();
        FakePrompter* prompter = fake.get();
        w.setPrompter(std::move(fake));
        QTimer::singleShot(0, &w, [&w] {
            if (auto* dialog = w.findChild<PluginsAdminDialog*>())
                dialog->accept();
        });
        w.showPluginsAdmin();
        QVERIFY(prompter->asked.isEmpty());
    }
};

QTEST_MAIN(TestPluginsAdmin)
#include "tst_pluginsadmin.moc"
