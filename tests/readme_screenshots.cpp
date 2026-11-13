// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Las capturas del README (docs/images/), hechas con la aplicación de verdad y sin pantalla:
// «cmake --build build --target edita_readme_images» las vuelve a generar (al doble de
// resolución, QT_SCALE_FACTOR=2). Como manual_screenshots, cada captura es un paso que prepara
// la ventana, espera a que se dibuje y guarda la imagen.
//
// La del administrador de plugins usa el catálogo de EDITA_PLUGIN_CATALOG_URL (la lista
// publicada y su firma, descargadas antes), y la de Markdown, el plugin de build/plugins.
//
//   readme_screenshots <idioma> <carpeta de salida>

#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/Terminal.h"
#include "ui/TerminalPanel.h"
#include "ui/PluginManager.h"
#include "ui/Translations.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QLineEdit>
#include <QPainter>
#include <QProcess>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>

#include <cstring>
#include <functional>
#include <vector>

using namespace edita;

namespace {

const char* const kSource = R"(#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

// @COMMENT@
struct Item {
    std::string name;
    double price = 0;
    int quantity = 1;
};

class Order
{
public:
    void add(Item item) { _items.push_back(std::move(item)); }

    double total(double discount = 0) const
    {
        double sum = 0;
        for (const Item& item : _items)
            sum += item.price * item.quantity;
        return sum * (1 - discount);
    }

    const Item* mostExpensive() const
    {
        auto it = std::max_element(_items.begin(), _items.end(),
            [](const Item& a, const Item& b) { return a.price < b.price; });
        return it == _items.end() ? nullptr : &*it;
    }

private:
    std::vector<Item> _items;
};

int main()
{
    Order order;
    order.add({"pencil", 0.5, 10});
    order.add({"notebook", 2.25, 3});
    std::cout << "Total: " << order.total(0.1) << '\n';
    return 0;
}
)";

const char* const kReadme = R"(# Order

A small example project for **Edita**.

## Build

```sh
cmake -S . -B build
cmake --build build
```

## Features

- Orders with any number of items
- Totals with a *discount*
- The most expensive item

| Item     | Price | Quantity |
|----------|------:|---------:|
| pencil   |  0.50 |       10 |
| notebook |  2.25 |        3 |

> Edita shows this preview while you write.
)";

void write(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(data);
}

// La ventana con lo que flota encima (la paleta): como se ve en la pantalla
QImage grabWithPopups(QWidget* window)
{
    QImage image = window->grab().toImage();
    QPainter painter(&image);
    for (QWidget* top : QApplication::topLevelWidgets()) {
        if (top != window && top->isVisible() && top->geometry().intersects(window->geometry())) {
            const QPoint at = top->geometry().topLeft() - window->geometry().topLeft();
            painter.drawPixmap(at * window->devicePixelRatio() / image.devicePixelRatio(), top->grab());
        }
    }
    return image;
}

struct Step
{
    std::function<void()> prepare;
    std::function<void()> capture;
    int delay = 1500; // lo que se espera entre preparar y capturar
};

// git en `dir`, con un autor genérico y sin la configuración del usuario
void git(const QString& dir, const QStringList& args)
{
    QProcess p;
    p.setWorkingDirectory(dir);
    p.start(QStringLiteral("git"), args);
    p.waitForFinished(15000);
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    if (argc < 3) {
        qWarning("usage: readme_screenshots <language> <output folder>");
        return 2;
    }
    const QString language = QString::fromLocal8Bit(argv[1]);
    const QDir out(QString::fromLocal8Bit(argv[2]));
    QDir().mkpath(out.path());

    QTemporaryDir tmp;
    qputenv("XDG_CONFIG_HOME", tmp.filePath(QStringLiteral("config")).toUtf8());
    i18n::install(language);
    // Nada del usuario en las capturas: git sin su configuración y una shell sin la suya
    qputenv("GIT_CONFIG_GLOBAL", "/dev/null");
    qputenv("GIT_CONFIG_NOSYSTEM", "1");
    for (const char* var : {"GIT_AUTHOR_NAME", "GIT_COMMITTER_NAME"})
        qputenv(var, "Edita");
    for (const char* var : {"GIT_AUTHOR_EMAIL", "GIT_COMMITTER_EMAIL"})
        qputenv(var, "edita@example.invalid");
    qputenv("SHELL", "/bin/sh");
    qputenv("ENV", "");
    qputenv("PS1", "$ ");
    qputenv("GIT_PAGER", "cat");
    if (language.startsWith(QLatin1String("en")))
        qputenv("LC_ALL", "C.UTF-8"); // los mensajes de la shell, en inglés

    const QString project = tmp.filePath(QStringLiteral("order"));
    // Los textos del ejemplo, en el idioma de la captura
    const bool english = language.startsWith(QLatin1String("en"));
    QByteArray source(kSource);
    source.replace("@COMMENT@", english ? "An order: its items and the total with a discount"
                                        : "Un pedido: sus artículos y el total con descuento");
    write(project + QStringLiteral("/src/order.cpp"), source);
    write(project + QStringLiteral("/src/order.h"), "#pragma once\n");
    write(project + QStringLiteral("/tests/test_order.cpp"), english ? "// tests\n" : "// pruebas\n");
    write(project + QStringLiteral("/CMakeLists.txt"), "cmake_minimum_required(VERSION 3.20)\nproject(order CXX)\n");
    write(project + QStringLiteral("/README.md"), kReadme);
    write(project + QStringLiteral("/Makefile"), "build:\n\tg++ -std=c++20 -Wall -c src/order.cpp -o /dev/null\n");
    git(project, {QStringLiteral("init"), QStringLiteral("-q"), QStringLiteral("-b"), QStringLiteral("main")});
    git(project, {QStringLiteral("add"), QStringLiteral(".")});
    git(project, {QStringLiteral("commit"), QStringLiteral("-q"), QStringLiteral("-m"),
                  english ? QStringLiteral("First version of the order") : QStringLiteral("Primera versión del pedido")});

    MainWindow w;
    auto prefs = w.preferences();
    prefs.darkModeFollowSystem = false;
    prefs.darkModeEnabled = false;
    prefs.showWelcome = false;
    prefs.spellCheck = false; // sin subrayados del diccionario del equipo en los comentarios
    w.setPreferences(prefs);
    w.resize(1280, 760);
    w.show();
    for (QDockWidget* dock : w.findChildren<QDockWidget*>())
        dock->hide();
    w.addFolderToWorkspace(project);
    w.openFile(project + QStringLiteral("/README.md"));
    w.openFile(project + QStringLiteral("/src/order.cpp"));

    auto action = [&w](const char* name) { return w.findChild<QAction*>(QString::fromLatin1(name)); };
    auto save = [&out](const QImage& image, const char* name) { image.save(out.filePath(QString::fromLatin1(name))); };
    auto hideDocks = [&w] {
        for (QDockWidget* dock : w.findChildren<QDockWidget*>())
            dock->hide();
    };
    auto setDark = [&w](bool dark) {
        auto p = w.preferences();
        p.darkModeEnabled = dark;
        w.setPreferences(p);
    };
    auto showWorkspace = [&] {
        hideDocks();
        action("actionFolderAsWorkspace")->setChecked(false);
        action("actionFolderAsWorkspace")->trigger();
    };
    auto runPlugin = [&w](const QString& name) {
        for (const auto& p : w.plugins()->plugins()) {
            for (const PluginManager::Command& c : p->commands) {
                if (c.name == name)
                    w.plugins()->runCommand(c.cmdId);
            }
        }
    };

    // order.cpp como en el commit, guardado
    auto restoreSource = [&] {
        w.openFile(project + QStringLiteral("/src/order.cpp"));
        w.editor()->setText(std::string(source.constData(), static_cast<std::size_t>(source.size())));
        w.saveBuffer(w.currentBuffer());
    };
    auto replaceInSource = [&](const char* from, const char* to) {
        std::string text = w.editor()->text();
        if (const auto at = text.find(from); at != std::string::npos)
            text.replace(at, std::strlen(from), to);
        w.editor()->setText(text);
        w.saveBuffer(w.currentBuffer());
    };

    const std::vector<Step> steps{
        // 1. La ventana, en claro: el proyecto, el código y el mapa del documento
        {[&] {
             showWorkspace();
             action("actionDocumentMap")->trigger();
             w.editor()->send(SCI_GOTOLINE, 20);
         },
         [&] { save(w.grab().toImage(), "edita-light.png"); }},
        // 2. Lo mismo en oscuro, con la lista de funciones
        {[&] {
             hideDocks();
             showWorkspace();
             action("actionFunctionList")->trigger();
             setDark(true);
         },
         [&] { save(w.grab().toImage(), "edita-dark.png"); }},
        // 3. La paleta de órdenes (Ctrl+Mayús+A)
        {[&] {
             hideDocks();
             action("actionCommandPalette")->trigger();
             QTimer::singleShot(400, &app, [&] {
                 for (auto* field : w.findChildren<QLineEdit*>(QStringLiteral("commandPaletteInput"))) {
                     if (field->isVisible())
                         field->setText(english ? QStringLiteral(">comment") : QStringLiteral(">coment"));
                 }
             });
         },
         [&] {
             save(grabWithPopups(&w), "edita-palette.png");
             for (QWidget* picker : w.findChildren<QWidget*>(QStringLiteral("commandPalette")))
                 picker->hide();
         }},
        // 4. La vista previa de Markdown (el plugin MarkdownViewer++)
        {[&] {
             hideDocks();
             setDark(false);
             w.openFile(project + QStringLiteral("/README.md"));
             runPlugin(QStringLiteral("MarkdownViewer++"));
         },
         [&] {
             save(w.grab().toImage(), "edita-markdown.png");
             runPlugin(QStringLiteral("MarkdownViewer++")); // cierra la vista previa
             hideDocks();
             w.openFile(project + QStringLiteral("/src/order.cpp"));
         }},
        // 5. Comparar con lo guardado
        {[&] {
             EditorView* view = w.editor();
             view->send(SCI_GOTOLINE, 22);
             view->send(SCI_LINEDELETE);
             view->sends(SCI_REPLACESEL, 0, english ? "            sum += item.price * item.quantity * 1.16; // with VAT\n"
                                                  : "            sum += item.price * item.quantity * 1.16; // con IVA\n");
             view->send(SCI_GOTOLINE, 10);
             view->sends(SCI_REPLACESEL, 0, "    std::string sku;\n");
             action("actionCompareSaved")->trigger();
         },
         [&] {
             save(w.grab().toImage(), "edita-compare.png");
             action("actionCompareClear")->trigger();
             while (w.editor()->send(SCI_CANUNDO))
                 w.editor()->send(SCI_UNDO);
         }},
        // 6. Git: cambios en el margen y el panel Ver › Git
        {[&] {
             hideDocks();
             w.statusBar()->clearMessage();
             w.openFile(project + QStringLiteral("/src/order.cpp"));
             replaceInSource("    int quantity = 1;\n", "    int quantity = 1;\n    std::string sku;\n");
             replaceInSource("return sum * (1 - discount);", "return std::max(0.0, sum * (1 - discount));");
             action("actionGitCommit")->trigger();
             w.editor()->send(SCI_GOTOLINE, 23);
         },
         [&] {
             save(w.grab().toImage(), "edita-git.png");
             hideDocks();
             restoreSource();
         },
         2500},
        // 7. Una tarea del proyecto (make build) con un error del compilador
        {[&] {
             hideDocks();
             replaceInSource("sum += item.price * item.quantity;", "sum += item.price * item.qty;");
             w.runTask({QStringLiteral("make build"), QStringLiteral("make build"), project, QStringLiteral("Makefile")});
         },
         [&] {
             save(w.grab().toImage(), "edita-tasks.png");
             hideDocks();
             restoreSource();
         },
         6000},
        // 8. La terminal integrada
        {[&] {
             hideDocks();
             w.openTerminal(project);
             w.statusBar()->clearMessage();
             // Una orden cada vez, cuando la anterior ya terminó
             const QStringList commands{QStringLiteral("git log --oneline"), QStringLiteral("ls src tests"),
                                        QStringLiteral("cat Makefile")};
             for (int i = 0; i < commands.size(); ++i) {
                 QTimer::singleShot(1500 + 600 * i, &app, [&w, command = commands[i]] {
                     if (auto* panel = w.findChild<TerminalPanel*>(); panel && panel->currentTerminal())
                         panel->currentTerminal()->sendText(command + QLatin1Char('\n'));
                 });
             }
         },
         [&] {
             save(w.grab().toImage(), "edita-terminal.png");
             hideDocks();
         },
         4500},
        // 9. El administrador de plugins con el catálogo (modal: la captura va antes)
        {[&] {
             hideDocks();
             QTimer::singleShot(3500, &app, [&] {
                 if (QWidget* dialog = QApplication::activeModalWidget()) {
                     dialog->resize(1100, 680);
                     // Con un plugin elegido: su descripción, licencia y autor abajo
                     if (auto* list = dialog->findChild<QTreeWidget*>(QStringLiteral("available"))) {
                         const auto found = list->findItems(QStringLiteral("XML Tools"), Qt::MatchExactly);
                         if (!found.isEmpty())
                             list->setCurrentItem(found.first());
                     }
                     QApplication::processEvents();
                     save(dialog->grab().toImage(), "edita-plugins.png");
                     dialog->close();
                 }
             });
             action("actionPluginsAdmin")->trigger();
         },
         [] {}},
    };

    std::function<void(size_t)> run = [&](size_t i) {
        if (i >= steps.size()) {
            app.quit();
            return;
        }
        steps[i].prepare();
        QTimer::singleShot(steps[i].delay, &app, [&, i] {
            steps[i].capture();
            QTimer::singleShot(300, &app, [&, i] { run(i + 1); });
        });
    };
    QTimer::singleShot(1500, &app, [&] { run(0); });
    return app.exec();
}
