// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/RecipeCommands.h"

#include "core/Paths.h"
#include "core/PluginInstaller.h"
#include "core/PluginRecipe.h"
#include "doc/DocumentManager.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/PluginManager.h"
#include "ui/PluginsAdminDialog.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QMessageBox>
#include <QTimer>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QUrl>

#include <optional>

namespace edita {

namespace {

QUrl urlOf(const QString& location, const QString& recipeDir)
{
    const QUrl url(location);
    if (!url.scheme().isEmpty() && url.scheme().size() > 1) // "C:" no es un esquema
        return url;
    // Una ruta local, relativa a la receta
    return QUrl::fromLocalFile(QFileInfo(location).isRelative() ? QDir(recipeDir).filePath(location) : location);
}

bool fetch(const QString& location, const QString& recipeDir, QByteArray& data, QTextStream& out)
{
    const QUrl url = urlOf(location, recipeDir);
    out << QStringLiteral("==> %1\n").arg(url.toDisplayString());
    out.flush();
    QString error;
    if (!PluginsAdminDialog::defaultFetch(url, data, &error)) {
        out << QStringLiteral("Error: %1\n").arg(error);
        return false;
    }
    return true;
}

// Un texto en una línea, con los saltos visibles
QString visible(QString text)
{
    return text.replace(QLatin1Char('\n'), QStringLiteral("\\n")).replace(QLatin1Char('\t'), QStringLiteral("\\t"));
}

// Sin nadie frente a la pantalla, una prueba no puede quedarse esperando un diálogo
// modal (un MessageBox del plugin, uno de sus diálogos): se aceptan y su texto queda en
// la salida
class ModalCloser {
public:
    explicit ModalCloser(QTextStream& out) : _out(out)
    {
        _timer.setInterval(20);
        QObject::connect(&_timer, &QTimer::timeout, [this] { closeModal(); });
        _timer.start();
    }

private:
    void closeModal()
    {
        QWidget* modal = QApplication::activeModalWidget();
        if (!modal)
            return;
        QString text = modal->windowTitle();
        if (auto* box = qobject_cast<QMessageBox*>(modal))
            text += QStringLiteral(": ") + box->text();
        _out << QStringLiteral("    dialog: %1\n").arg(visible(text));
        if (auto* dialog = qobject_cast<QDialog*>(modal))
            dialog->accept();
        else
            modal->close();
    }

    QTextStream& _out;
    QTimer _timer;
};

} // namespace

int runRecipeCommandLine(const QStringList& arguments, QTextStream& out)
{
    auto value = [&](const QString& option) {
        const qsizetype at = arguments.indexOf(option);
        return at >= 0 && at + 1 < arguments.size() ? arguments[at + 1] : QString();
    };
    if (arguments.contains(QStringLiteral("--build-recipe"))) {
        const QString recipe = value(QStringLiteral("--build-recipe"));
        const QString output = value(QStringLiteral("--output"));
        if (recipe.isEmpty() || output.isEmpty()) {
            out << QStringLiteral("Usage: edita --build-recipe recipe.json --output folder\n");
            return 2;
        }
        return buildRecipeCommand(recipe, output, out);
    }
    if (arguments.contains(QStringLiteral("--check-plugin"))) {
        const QString package = value(QStringLiteral("--check-plugin"));
        const QString recipe = value(QStringLiteral("--recipe"));
        if (package.isEmpty() || recipe.isEmpty()) {
            out << QStringLiteral("Usage: edita --check-plugin package.zip --recipe recipe.json\n");
            return 2;
        }
        return checkPluginCommand(package, recipe, out);
    }
    return -1;
}

int buildRecipeCommand(const QString& recipePath, const QString& outputFolder, QTextStream& out)
{
    // CMake y el compilador corren en otra carpeta: las rutas, absolutas
    const QString outputDir = QFileInfo(outputFolder).absoluteFilePath();
    PluginRecipe recipe;
    QString error;
    if (!PluginRecipe::parse(recipePath, recipe, &error)) {
        out << QStringLiteral("Error: %1\n").arg(error);
        return 1;
    }
    const PluginSdk sdk = findPluginSdk();
    if (!sdk.isValid()) {
        out << QStringLiteral("Error: the Edita plugin SDK was not found\n");
        return 1;
    }
    QByteArray source;
    if (!fetch(recipe.sourceUrl, recipe.directory(), source, out))
        return 1;
    std::vector<QByteArray> extras;
    for (const RecipeSource& extra : recipe.extraSources) {
        QByteArray data;
        if (!fetch(extra.url, recipe.directory(), data, out))
            return 1;
        extras.push_back(data);
    }

    QDir().mkpath(outputDir);
    const QString workDir = QDir(outputDir).filePath(QStringLiteral("work/") + recipe.info.folderName);
    RecipeBuilder builder(recipe, sdk, workDir);
    QEventLoop loop;
    bool ok = false;
    QObject::connect(&builder, &RecipeBuilder::output, &builder, [&out](const QString& text) {
        out << text;
        out.flush();
    });
    QObject::connect(&builder, &RecipeBuilder::finished, &loop, [&](bool success, const QString&) {
        ok = success;
        loop.quit();
    });
    builder.start(source, extras);
    if (builder.isRunning())
        loop.exec();
    if (!ok)
        return 1;

    // <carpeta>-<versión>.zip y su SHA-256, como en la lista de plugins
    const QString package = QDir(outputDir).filePath(QFileInfo(builder.packagePath()).fileName());
    QFile::remove(package);
    if (!QFile::copy(builder.packagePath(), package)) {
        out << QStringLiteral("Error: cannot write %1\n").arg(package);
        return 1;
    }
    QFile sha(package + QStringLiteral(".sha256"));
    if (sha.open(QIODevice::WriteOnly | QIODevice::Truncate))
        sha.write(builder.packageSha256().toUtf8() + "  " + QFileInfo(package).fileName().toUtf8() + '\n');
    out << QStringLiteral("==> %1\n    SHA-256 %2\n").arg(package, builder.packageSha256());
    return 0;
}

int checkPluginCommand(const QString& packagePath, const QString& recipePath, QTextStream& out)
{
    PluginRecipe recipe;
    QString error;
    if (!PluginRecipe::parse(recipePath, recipe, &error)) {
        out << QStringLiteral("Error: %1\n").arg(error);
        return 1;
    }
    QFile package(packagePath);
    if (!package.open(QIODevice::ReadOnly)) {
        out << QStringLiteral("Error: cannot read %1\n").arg(packagePath);
        return 1;
    }
    const QByteArray zip = package.readAll();

    // Un Edita aislado: la configuración en una carpeta temporal, solo con este plugin
    QTemporaryDir home;
    struct RestoreConfigHome {
        bool wasSet = qEnvironmentVariableIsSet("XDG_CONFIG_HOME");
        QByteArray value = qgetenv("XDG_CONFIG_HOME");
        ~RestoreConfigHome() { wasSet ? qputenv("XDG_CONFIG_HOME", value) : qunsetenv("XDG_CONFIG_HOME"); }
    } restore;
    qputenv("XDG_CONFIG_HOME", QDir(home.path()).filePath(QStringLiteral("config")).toUtf8());
    const QString userPlugins = QDir(paths::configDir()).filePath(QStringLiteral("plugins"));
    if (!plugin_installer::installPackage(zip, recipe.info.folderName, plugin_installer::sha256Hex(zip), userPlugins, false)) {
        out << QStringLiteral("Error: the package cannot be installed as %1\n").arg(recipe.info.folderName);
        return 1;
    }

    // Los diálogos de la carga (un aviso en setInfo…) también se cierran solos
    std::optional<ModalCloser> loading(std::in_place, out);
    MainWindow window;
    // Una ventana de verdad (sin pantalla): con su tamaño, Scintilla sabe cuántas líneas se
    // ven, y hay plugins que solo trabajan sobre ellas
    window.resize(1000, 700);
    window.show();
    QCoreApplication::processEvents();
    window.notifyPluginsReady();
    loading.reset();
    const PluginManager::Plugin* plugin = nullptr;
    const QString file = recipe.info.folderName + QLatin1Char('/') + recipe.info.folderName + QStringLiteral(".so");
    for (const auto& p : window.plugins()->plugins()) {
        if (p->path.endsWith(file))
            plugin = p.get();
    }
    if (!plugin) {
        out << QStringLiteral("FAIL: the plugin does not load\n");
        for (const QString& warning : window.pluginWarnings())
            out << QStringLiteral("    %1\n").arg(warning);
        return 1;
    }
    out << QStringLiteral("Plugin: %1 %2\n").arg(plugin->name, plugin->version);
    int failures = 0;
    auto findCommand = [&](const QString& name) -> const PluginManager::Command* {
        for (const PluginManager::Command& c : plugin->commands) {
            if (c.name == name)
                return &c;
        }
        return nullptr;
    };
    QStringList names;
    for (const PluginManager::Command& c : plugin->commands) {
        if (c.function)
            names << c.name;
    }
    out << QStringLiteral("Commands: %1\n").arg(names.join(QStringLiteral(" | ")));
    for (const QString& name : recipe.checkCommands) {
        const bool found = findCommand(name);
        failures += found ? 0 : 1;
        out << (found ? QStringLiteral("PASS") : QStringLiteral("FAIL")) << QStringLiteral(": command \"%1\"\n").arg(name);
    }

    for (const RecipeCheck& check : recipe.checks) {
        const PluginManager::Command* command = findCommand(check.command);
        if (!command) {
            ++failures;
            out << QStringLiteral("FAIL: \"%1\": there is no such command\n").arg(check.command);
            continue;
        }
        window.newDocument();
        EditorView* editor = window.editor();
        const QByteArray text = check.text.toUtf8();
        editor->sends(SCI_SETTEXT, 0, text.constData());
        if (check.selectAll)
            editor->sci(SCI_SELECTALL);
        else
            editor->sci(SCI_GOTOPOS, 0);
        {
            ModalCloser closer(out);
            for (const QString& name : check.before) {
                if (const PluginManager::Command* first = findCommand(name)) {
                    window.plugins()->runCommand(first->cmdId);
                    QCoreApplication::processEvents();
                } else {
                    ++failures;
                    out << QStringLiteral("FAIL: \"%1\": there is no such command\n").arg(name);
                }
            }
            window.plugins()->runCommand(command->cmdId);
            QCoreApplication::processEvents();
        }
        bool passed = true;
        QString detail;
        if (check.hasExpect) {
            const QString result = QString::fromStdString(window.documents().text(window.currentBuffer()));
            passed = result == check.expect;
            if (!passed)
                detail = QStringLiteral("\n    expected: %1\n    got:      %2").arg(visible(check.expect), visible(result));
        }
        if (!check.expectFile.isEmpty()) {
            const Buffer* buffer = window.documents().buffer(window.currentBuffer());
            const QString path = buffer ? buffer->filePath() : QString();
            passed = passed && QFileInfo(path).absoluteFilePath() == QFileInfo(check.expectFile).absoluteFilePath();
            if (!passed)
                detail += QStringLiteral("\n    expected file: %1\n    current:       %2").arg(check.expectFile, path);
        }
        failures += passed ? 0 : 1;
        out << (passed ? QStringLiteral("PASS") : QStringLiteral("FAIL")) << QStringLiteral(": \"%1\"%2\n").arg(check.command, detail);
    }
    out << (failures ? QStringLiteral("%1 check(s) failed\n").arg(failures) : QStringLiteral("All checks passed\n"));
    return failures ? 1 : 0;
}

} // namespace edita
