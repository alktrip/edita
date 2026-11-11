// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 76: Preferencias › Asociación de archivos (RegExtDlg de N++), con los tipos MIME
// y mimeapps.list de XDG

#include "core/FileAssociation.h"
#include "core/Preferences.h"
#include "ui/PreferencesDialog.h"

#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;
using namespace edita::file_association;

class TestFileAssociation : public QObject
{
    Q_OBJECT

    std::unique_ptr<QTemporaryDir> _tmp;
    QStringList _runs; // las órdenes que se ejecutaron

    Environment env()
    {
        Environment e;
        e.dataHome = _tmp->filePath(QStringLiteral("data"));
        e.configHome = _tmp->filePath(QStringLiteral("config"));
        e.dataDirs = {_tmp->filePath(QStringLiteral("system"))};
        e.appId = QStringLiteral("edita");
        e.launcher = _tmp->filePath(QStringLiteral("Mis programas/Edita.AppImage"));
        e.iconFile = _tmp->filePath(QStringLiteral("edita.png"));
        e.run = [this](const QString& program, const QStringList& args) {
            _runs << program + QLatin1Char(' ') + args.join(QLatin1Char(' '));
            return true;
        };
        return e;
    }

    static QString read(const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return QString::fromUtf8(file.readAll());
    }

    static void write(const QString& path, const QByteArray& text)
    {
        QDir().mkpath(QFileInfo(path).path());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(text);
    }

    static QStringList items(const QListWidget* list)
    {
        QStringList out;
        for (int i = 0; i < list->count(); ++i)
            out << list->item(i)->text();
        return out;
    }

private slots:
    void init()
    {
        _tmp = std::make_unique<QTemporaryDir>();
        QVERIFY(_tmp->isValid());
        _runs.clear();
        write(_tmp->filePath(QStringLiteral("edita.png")), "png");
        write(_tmp->filePath(QStringLiteral("Mis programas/Edita.AppImage")), "#!/bin/sh\n");
    }

    // Los grupos de defExtArray de N++
    void groups()
    {
        const auto& g = extensionGroups();
        QCOMPARE(g.size(), size_t(10));
        QCOMPARE(g.front().name, QStringLiteral("Notepad"));
        QCOMPARE(g.front().extensions, (QStringList{QStringLiteral(".txt"), QStringLiteral(".log")}));
        QCOMPARE(g[2].name, QStringLiteral("c, c++, objc"));
        QVERIFY(g[2].extensions.contains(QStringLiteral(".cpp")));
        QVERIFY(g[5].extensions.contains(QStringLiteral(".py")));
        QCOMPARE(g.back().name, QStringLiteral("customize"));
        QVERIFY(g.back().extensions.isEmpty());
    }

    void normalize()
    {
        QCOMPARE(normalizeExtension(QStringLiteral("txt")), QStringLiteral(".txt"));
        QCOMPARE(normalizeExtension(QStringLiteral(" .TXT ")), QStringLiteral(".txt"));
        QCOMPARE(normalizeExtension(QStringLiteral(".tar.gz")), QStringLiteral(".tar.gz"));
        QCOMPARE(normalizeExtension(QStringLiteral(".c++")), QStringLiteral(".c++"));
        QCOMPARE(normalizeExtension(QStringLiteral(".")), QString());
        QCOMPARE(normalizeExtension(QString()), QString());
        QCOMPARE(normalizeExtension(QStringLiteral("*.txt")), QString());
        QCOMPARE(normalizeExtension(QStringLiteral(".a b")), QString());
        QCOMPARE(normalizeExtension(QStringLiteral("../x")), QString());
    }

    // Comillas de Exec= (especificación de los .desktop): la ruta con espacios va entre
    // comillas, con \ antes de " ` $ y \, y luego cada \ se escribe doble; % se escribe %%
    void execQuoting()
    {
        QCOMPARE(desktopExec(QStringLiteral("/usr/bin/edita")), QStringLiteral("/usr/bin/edita"));
        QCOMPARE(desktopExec(QStringLiteral("/home/a/Mis programas/E.AppImage")),
                 QStringLiteral("\"/home/a/Mis programas/E.AppImage\""));
        QCOMPARE(desktopExec(QStringLiteral("/home/a/$x/E")), QStringLiteral("\"/home/a/\\\\$x/E\""));
        QCOMPARE(desktopExec(QStringLiteral("/home/a/100%/E")), QStringLiteral("/home/a/100%%/E"));
    }

    // El tipo de texto que conoce el sistema; si no es de texto o no tiene, uno propio
    void mimeTypes()
    {
        FileAssociations fa(env());
        bool own = true;
        QCOMPARE(fa.mimeTypeFor(QStringLiteral(".txt"), &own), QStringLiteral("text/plain"));
        QVERIFY(!own);
        QCOMPARE(fa.mimeTypeFor(QStringLiteral(".cpp"), &own), QStringLiteral("text/x-c++src"));
        QVERIFY(!own);
        QCOMPARE(fa.mimeTypeFor(QStringLiteral(".zzqedita"), &own), QStringLiteral("application/x-edita-zzqedita"));
        QVERIFY(own);
        // Una imagen no es texto: el tipo propio se queda con la extensión
        QCOMPARE(fa.mimeTypeFor(QStringLiteral(".png"), &own), QStringLiteral("application/x-edita-png"));
        QVERIFY(own);
    }

    // Asociar pone a Edita por omisión y guarda el programa de antes; quitarla lo devuelve
    // y deja lo demás de mimeapps.list como estaba
    void addAndRemove()
    {
        const Environment e = env();
        write(e.configHome + QStringLiteral("/mimeapps.list"),
              "# mío\n[Default Applications]\ntext/plain=gedit.desktop\nimage/png=eog.desktop\n\n"
              "[Added Associations]\ntext/plain=gedit.desktop;\n");
        {
            FileAssociations fa(e);
            QString error;
            QVERIFY2(fa.add(QStringLiteral("TXT"), &error), qPrintable(error));
            QVERIFY(fa.isRegistered(QStringLiteral(".txt")));
            QCOMPARE(fa.registered().size(), 1);
            QCOMPARE(fa.registered().front().previous, QStringLiteral("gedit.desktop"));
            const QString list = read(fa.mimeAppsFile());
            QVERIFY(list.startsWith(QStringLiteral("# mío\n")));
            QVERIFY(list.contains(QStringLiteral("text/plain=edita.desktop\n")));
            QVERIFY(list.contains(QStringLiteral("image/png=eog.desktop\n")));
            QVERIFY(list.contains(QStringLiteral("text/plain=edita.desktop;gedit.desktop;\n")));
            // Asociar otra vez no cambia nada
            QVERIFY(fa.add(QStringLiteral(".txt")));
            QCOMPARE(fa.registered().size(), 1);
        }
        FileAssociations fa(e); // se lee del registro
        QVERIFY(fa.isRegistered(QStringLiteral(".txt")));
        QVERIFY(fa.remove(QStringLiteral(".txt")));
        QVERIFY(!fa.isRegistered(QStringLiteral(".txt")));
        const QString list = read(fa.mimeAppsFile());
        QVERIFY(list.contains(QStringLiteral("text/plain=gedit.desktop\n")));
        QVERIFY(list.contains(QStringLiteral("image/png=eog.desktop\n")));
        QVERIFY(list.contains(QStringLiteral("text/plain=gedit.desktop;\n")));
        QVERIFY(!list.contains(QStringLiteral("edita.desktop")));
        QVERIFY(!fa.remove(QStringLiteral(".txt"))); // ya no estaba
    }

    // Sin mimeapps.list: se crea, y al quitar no queda la línea
    void noPreviousDefault()
    {
        FileAssociations fa(env());
        QVERIFY(fa.add(QStringLiteral(".py")));
        QCOMPARE(fa.registered().front().mimeType, QStringLiteral("text/x-python"));
        QVERIFY(read(fa.mimeAppsFile()).contains(QStringLiteral("[Default Applications]\ntext/x-python=edita.desktop\n")));
        QVERIFY(fa.remove(QStringLiteral(".py")));
        QVERIFY(!read(fa.mimeAppsFile()).contains(QStringLiteral("text/x-python")));
    }

    // .cpp y .cc son el mismo tipo: quitar una no quita la asociación de la otra
    void sharedMimeType()
    {
        const Environment e = env();
        write(e.configHome + QStringLiteral("/mimeapps.list"), "[Default Applications]\ntext/x-c++src=kate.desktop\n");
        FileAssociations fa(e);
        QVERIFY(fa.add(QStringLiteral(".cpp")));
        QVERIFY(fa.add(QStringLiteral(".cc")));
        QCOMPARE(fa.registered().at(1).previous, QStringLiteral("kate.desktop"));
        QVERIFY(fa.remove(QStringLiteral(".cpp")));
        QVERIFY(read(fa.mimeAppsFile()).contains(QStringLiteral("text/x-c++src=edita.desktop\n")));
        QVERIFY(fa.remove(QStringLiteral(".cc")));
        QVERIFY(read(fa.mimeAppsFile()).contains(QStringLiteral("text/x-c++src=kate.desktop\n")));
    }

    // El tipo propio: el paquete de shared-mime-info y update-mime-database
    void ownMimeType()
    {
        FileAssociations fa(env());
        QVERIFY(fa.add(QStringLiteral(".zzqedita")));
        QVERIFY(fa.registered().front().ownMimeType);
        const QString package = read(fa.mimePackageFile());
        QVERIFY(package.contains(QStringLiteral("<mime-type type=\"application/x-edita-zzqedita\">")));
        QVERIFY(package.contains(QStringLiteral("<glob pattern=\"*.zzqedita\" weight=\"60\"/>")));
        QVERIFY(package.contains(QStringLiteral("<sub-class-of type=\"text/plain\"/>")));
        QVERIFY(read(fa.mimeAppsFile()).contains(QStringLiteral("application/x-edita-zzqedita=edita.desktop\n")));
        QVERIFY(_runs.contains(QStringLiteral("update-mime-database ") + fa.environment().dataHome + QStringLiteral("/mime")));
        _runs.clear();
        QVERIFY(fa.remove(QStringLiteral(".zzqedita")));
        QVERIFY(!QFile::exists(fa.mimePackageFile()));
        QVERIFY(_runs.contains(QStringLiteral("update-mime-database ") + fa.environment().dataHome + QStringLiteral("/mime")));
    }

    // Sin update-mime-database el tipo propio no funcionaría: se avisa y no se asocia
    void ownMimeTypeWithoutTool()
    {
        Environment e = env();
        e.run = [](const QString& program, const QStringList&) { return program != QLatin1String("update-mime-database"); };
        FileAssociations fa(e);
        QString error;
        QVERIFY(!fa.add(QStringLiteral(".zzqedita"), &error));
        QVERIFY(error.contains(QStringLiteral("update-mime-database")));
        QVERIFY(!fa.isRegistered(QStringLiteral(".zzqedita")));
        QVERIFY(!QFile::exists(fa.mimePackageFile()));
        QVERIFY(fa.add(QStringLiteral(".txt"))); // los de texto no lo necesitan
    }

    // Sin Edita instalado (AppImage): su propio edita.desktop, con la ruta del AppImage
    void ownDesktopFile()
    {
        FileAssociations fa(env());
        QVERIFY(!fa.installedDesktopFile());
        QVERIFY(fa.add(QStringLiteral(".txt")));
        const QString desktop = read(fa.desktopFile());
        QVERIFY(desktop.startsWith(QStringLiteral("[Desktop Entry]\n")));
        QVERIFY(desktop.contains(QStringLiteral("\nExec=\"") + fa.environment().launcher + QStringLiteral("\" %F\n")));
        QVERIFY(desktop.contains(QStringLiteral("\nIcon=edita\n")));
        QVERIFY(desktop.contains(QStringLiteral("text/plain;")));
        QVERIFY(desktop.contains(QStringLiteral("\nX-Edita-Generated=true\n")));
        QCOMPARE(read(fa.environment().dataHome + QStringLiteral("/icons/hicolor/256x256/apps/edita.png")), QStringLiteral("png"));
        QVERIFY(_runs.contains(QStringLiteral("update-desktop-database ") + fa.environment().dataHome + QStringLiteral("/applications")));
        // Asociar otro tipo lo añade al MimeType= del propio
        QVERIFY(fa.add(QStringLiteral(".zzqedita")));
        QVERIFY(read(fa.desktopFile()).contains(QStringLiteral("application/x-edita-zzqedita;")));
    }

    // Instalado (paquete .deb o AUR): se usa el suyo
    void installedDesktop()
    {
        const Environment e = env();
        write(e.dataDirs.front() + QStringLiteral("/applications/edita.desktop"), "[Desktop Entry]\n");
        FileAssociations fa(e);
        QVERIFY(fa.installedDesktopFile());
        QVERIFY(fa.add(QStringLiteral(".txt")));
        QVERIFY(!QFile::exists(fa.desktopFile()));
    }

    // Al cambiar de AppImage, el .desktop propio pasa al nuevo si el viejo ya no está
    void refreshLauncher()
    {
        Environment e = env();
        {
            FileAssociations fa(e);
            QVERIFY(fa.add(QStringLiteral(".txt")));
            QVERIFY(!fa.refreshLauncher()); // apunta al de ahora
        }
        const QString old = e.launcher;
        e.launcher = _tmp->filePath(QStringLiteral("Edita-nuevo.AppImage"));
        {
            FileAssociations fa(e);
            QVERIFY(!fa.refreshLauncher()); // el viejo sigue ahí: no se toca
            QVERIFY(read(fa.desktopFile()).contains(old));
        }
        QVERIFY(QFile::remove(old));
        FileAssociations fa(e);
        QVERIFY(fa.refreshLauncher());
        QVERIFY(read(fa.desktopFile()).contains(QStringLiteral("Exec=") + e.launcher + QStringLiteral(" %F")));
        // Uno escrito a mano (sin X-Edita-Generated) no se toca
        write(fa.desktopFile(), "[Desktop Entry]\nExec=/no/existe %F\n");
        QVERIFY(!fa.refreshLauncher());
    }

    // La página de Preferencias, como la de N++: grupos, extensiones, → y ←, la lista de
    // las asociadas y el campo de «customize»
    void preferencesPage()
    {
        PreferencesDialog dialog(Preferences{}, {});
        QVERIFY(dialog.pageNames().contains(QStringLiteral("fileAssociation")));
        dialog.setFileAssociations(std::make_unique<FileAssociations>(env()));
        dialog.showPage(QStringLiteral("fileAssociation"));
        auto* groups = dialog.findChild<QListWidget*>(QStringLiteral("assocGroups"));
        auto* exts = dialog.findChild<QListWidget*>(QStringLiteral("assocExtensions"));
        auto* registered = dialog.findChild<QListWidget*>(QStringLiteral("assocRegistered"));
        auto* custom = dialog.findChild<QLineEdit*>(QStringLiteral("assocCustom"));
        auto* add = dialog.findChild<QPushButton*>(QStringLiteral("assocAdd"));
        auto* remove = dialog.findChild<QPushButton*>(QStringLiteral("assocRemove"));
        QVERIFY(groups && exts && registered && custom && add && remove);
        QCOMPARE(groups->count(), 10);
        QVERIFY(!add->isEnabled());
        QVERIFY(!remove->isEnabled());

        groups->setCurrentRow(2); // c, c++, objc
        QVERIFY(items(exts).contains(QStringLiteral(".cpp")));
        exts->setCurrentRow(int(items(exts).indexOf(QStringLiteral(".cpp"))));
        QVERIFY(add->isEnabled());
        add->click();
        QCOMPARE(items(registered), QStringList{QStringLiteral(".cpp")});
        QVERIFY(!items(exts).contains(QStringLiteral(".cpp")));
        QVERIFY(registered->item(0)->toolTip().contains(QStringLiteral("text/x-c++src")));
        QVERIFY(read(env().configHome + QStringLiteral("/mimeapps.list")).contains(QStringLiteral("text/x-c++src=edita.desktop")));

        // Cambiar de grupo y volver: la asociada ya no sale entre las del grupo
        groups->setCurrentRow(0);
        QCOMPARE(items(exts), (QStringList{QStringLiteral(".txt"), QStringLiteral(".log")}));
        groups->setCurrentRow(2);
        QVERIFY(!items(exts).contains(QStringLiteral(".cpp")));

        // ← la quita y vuelve a la lista del grupo
        registered->setCurrentRow(0);
        QVERIFY(remove->isEnabled());
        remove->click();
        QVERIFY(items(registered).isEmpty());
        QVERIFY(items(exts).contains(QStringLiteral(".cpp")));

        // Doble clic también pasa de una lista a la otra
        exts->setCurrentRow(0);
        emit exts->itemDoubleClicked(exts->item(0));
        QCOMPARE(registered->count(), 1);
        registered->setCurrentRow(0);
        emit registered->itemDoubleClicked(registered->item(0));
        QCOMPARE(registered->count(), 0);

        // customize: se escribe la extensión; el punto se pone solo
        groups->setCurrentRow(9);
        QVERIFY(custom->isVisibleTo(&dialog));
        QVERIFY(!exts->isVisibleTo(&dialog));
        QTest::keyClicks(custom, QStringLiteral("zzq"));
        QCOMPARE(custom->text(), QStringLiteral(".zzq"));
        QVERIFY(add->isEnabled());
        add->click();
        QCOMPARE(items(registered), QStringList{QStringLiteral(".zzq")});
        QVERIFY(custom->text().isEmpty());
        // Una que no vale no se asocia
        custom->setText(QStringLiteral(".a b"));
        add->click();
        QCOMPARE(registered->count(), 1);
    }
};

QTEST_MAIN(TestFileAssociation)
#include "tst_fileassociation.moc"
