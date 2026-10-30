// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Las capturas del manual de usuario (data/manual/<idioma>/images/), hechas con la aplicación
// de verdad y sin pantalla: «cmake --build build --target edita_manual_images» las vuelve a
// generar en los dos idiomas. Cada captura es un paso: prepara la ventana, espera a que se
// dibuje y guarda la imagen (con los menús flotantes encima, como se ven).
//
//   manual_screenshots <idioma> <carpeta de salida>

#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/RegexTester.h"
#include "ui/Translations.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QLineEdit>
#include <QPainter>
#include <QTemporaryDir>
#include <QTimer>

#include <functional>
#include <vector>

using namespace edita;

namespace {

const char* const kSource = R"(#include <iostream>
#include <vector>

// Suma los precios de un pedido
struct Item {
    std::string name;
    double price = 0;
    int quantity = 1;
};

double total(const std::vector<Item>& items)
{
    double sum = 0;
    for (const Item& item : items)
        sum += item.price * item.quantity;
    return sum;
}

int main()
{
    std::vector<Item> order{{"pencil", 0.5, 10}, {"notebook", 2.25, 3}};
    std::cout << "Total: " << total(order) << '\n';
    return 0;
}
)";

void write(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(data);
}

// La ventana con lo que flota encima (la paleta, listas): como se ve en la pantalla
QImage grabWithPopups(QWidget* window)
{
    QImage image = window->grab().toImage();
    QPainter painter(&image);
    for (QWidget* top : QApplication::topLevelWidgets()) {
        if (top != window && top->isVisible() && top->geometry().intersects(window->geometry()))
            painter.drawPixmap(top->geometry().topLeft() - window->geometry().topLeft(), top->grab());
    }
    return image;
}

struct Step
{
    std::function<void()> prepare;
    std::function<void()> capture;
};

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    if (argc < 3) {
        qWarning("usage: manual_screenshots <language> <output folder>");
        return 2;
    }
    const QString language = QString::fromLocal8Bit(argv[1]);
    const QDir out(QString::fromLocal8Bit(argv[2]));
    QDir().mkpath(out.path());

    QTemporaryDir tmp;
    qputenv("XDG_CONFIG_HOME", tmp.filePath(QStringLiteral("config")).toUtf8());
    i18n::install(language);

    // Un proyecto pequeño para el explorador
    const QString project = tmp.filePath(QStringLiteral("order"));
    write(project + QStringLiteral("/src/main.cpp"), kSource);
    write(project + QStringLiteral("/src/item.h"), "#pragma once\n");
    write(project + QStringLiteral("/CMakeLists.txt"), "cmake_minimum_required(VERSION 3.20)\nproject(order CXX)\n");
    write(project + QStringLiteral("/README.md"), "# Order\n");

    MainWindow w;
    auto prefs = w.preferences();
    prefs.darkModeFollowSystem = false;
    prefs.darkModeEnabled = false;
    prefs.showWelcome = false;
    w.setPreferences(prefs);
    w.resize(1060, 640);
    w.show();
    for (QDockWidget* dock : w.findChildren<QDockWidget*>())
        dock->hide();
    w.addFolderToWorkspace(project);
    w.openFile(project + QStringLiteral("/src/main.cpp"));

    auto action = [&w](const char* name) { return w.findChild<QAction*>(QString::fromLatin1(name)); };
    auto save = [&out](const QImage& image, const char* name) { image.save(out.filePath(QString::fromLatin1(name))); };
    auto hideDocks = [&w] {
        for (QDockWidget* dock : w.findChildren<QDockWidget*>())
            dock->hide();
    };
    auto modal = [&](const char* actionName, const char* file) {
        // El diálogo es modal: la captura se programa antes de abrirlo
        QTimer::singleShot(1500, &app, [&, file] {
            if (QWidget* dialog = QApplication::activeModalWidget()) {
                save(dialog->grab().toImage(), file);
                dialog->close();
            }
        });
        action(actionName)->trigger();
    };

    const std::vector<Step> steps{
        {[&] { w.editor()->send(SCI_GOTOLINE, 13); }, [&] { save(w.grab().toImage(), "window.png"); }},
        {[&] {
             hideDocks();
             action("actionFind")->trigger();
             // Al abrirse, la barra toma la palabra del cursor: lo buscado, después
             QTimer::singleShot(400, &app, [&] {
                 for (auto* field : w.findChildren<QLineEdit*>(QStringLiteral("findBarFind"))) {
                     if (field->isVisible())
                         field->setText(QStringLiteral("item"));
                 }
             });
         },
         [&] {
             save(w.grab().toImage(), "findbar.png");
             for (auto* field : w.findChildren<QLineEdit*>(QStringLiteral("findBarFind")))
                 field->clear();
             for (QWidget* bar : w.findChildren<QWidget*>(QStringLiteral("findBar")))
                 bar->hide();
             w.editor()->setFocus();
         }},
        {[&] {
             action("actionCommandPalette")->trigger();
             QTimer::singleShot(400, &app, [&] {
                 for (auto* field : w.findChildren<QLineEdit*>(QStringLiteral("commandPaletteInput"))) {
                     if (field->isVisible())
                         field->setText(QStringLiteral(">dupl"));
                 }
             });
         },
         [&] {
             save(grabWithPopups(&w), "palette.png");
             for (QWidget* picker : w.findChildren<QWidget*>(QStringLiteral("commandPalette")))
                 picker->hide();
         }},
        {[&] {
             action("actionRegexTester")->trigger();
             w.regexTesterPanel()->setSample(QStringLiteral("ana@example.com\nluis@test.com\nnot an address\n"));
             w.regexTesterPanel()->setPattern(QStringLiteral("(?<user>\\w+)@(\\w+)\\.com"));
             w.regexTesterPanel()->evaluate();
         },
         [&] {
             save(w.grab().toImage(), "regex.png");
             hideDocks();
         }},
        {[&] {
             // Cambios sin guardar, comparados con lo guardado
             EditorView* view = w.editor();
             view->send(SCI_GOTOLINE, 7);
             view->sends(SCI_REPLACESEL, 0, "    double discount = 0;\n");
             view->send(SCI_GOTOLINE, 15);
             view->send(SCI_LINEDELETE);
             view->sends(SCI_REPLACESEL, 0, "        sum += item.price * item.quantity * (1 - item.discount);\n");
             action("actionCompareSaved")->trigger();
         },
         [&] {
             save(w.grab().toImage(), "compare.png");
             action("actionCompareClear")->trigger();
             while (w.editor()->send(SCI_CANUNDO))
                 w.editor()->send(SCI_UNDO);
         }},
        {[&] { modal("actionStyleConfigurator", "styles.png"); }, [] {}},
        {[&] { modal("actionPreferences", "preferences.png"); }, [] {}},
    };

    // Un paso tras otro, con tiempo para que se dibuje (y para el resaltado, que va con retardo)
    std::function<void(size_t)> run = [&](size_t i) {
        if (i >= steps.size()) {
            app.quit();
            return;
        }
        steps[i].prepare();
        QTimer::singleShot(1200, &app, [&, i] {
            steps[i].capture();
            QTimer::singleShot(300, &app, [&, i] { run(i + 1); });
        });
    };
    QTimer::singleShot(1500, &app, [&] { run(0); });
    return app.exec();
}
