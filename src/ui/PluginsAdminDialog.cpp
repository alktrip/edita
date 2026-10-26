// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
//
// Edita is derived from Notepad++ (Copyright (C) Don HO and Notepad++ team).
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "ui/PluginsAdminDialog.h"

#include "core/PluginInstaller.h"
#include "syntax/DesktopFonts.h"
#include "ui/Prompter.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QFontDatabase>
#include <QProgressBar>
#include <QTextCursor>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace edita {

namespace {

constexpr int kFolderRole = Qt::UserRole;

const PluginInfo* findIn(const std::vector<PluginInfo>& list, const QString& folder)
{
    for (const PluginInfo& info : list) {
        if (info.folderName == folder)
            return &info;
    }
    return nullptr;
}

} // namespace

PluginsAdminDialog::PluginsAdminDialog(PluginsAdminContext context, Prompter& prompter, QWidget* parent)
    : QDialog(parent)
    , _context(std::move(context))
    , _prompter(prompter)
    , _fetch(&PluginsAdminDialog::defaultFetch)
{
    setObjectName(QStringLiteral("pluginsAdmin"));
    setWindowTitle(tr("Plugins Admin"));
    buildUi();
    rescan();
}

bool PluginsAdminDialog::defaultFetch(const QUrl& url, QByteArray& data, QString* error)
{
    if (url.isLocalFile() || url.scheme().isEmpty()) {
        QFile file(url.isLocalFile() ? url.toLocalFile() : url.toString());
        if (!file.open(QIODevice::ReadOnly)) {
            if (error)
                *error = tr("Cannot read %1").arg(file.fileName());
            return false;
        }
        data = file.readAll();
        return true;
    }
    // Lo que en N++ descarga gup.exe (con la misma comprobación de SHA-256 después)
    QNetworkAccessManager network;
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(60000);
    QNetworkReply* reply = network.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const bool ok = reply->error() == QNetworkReply::NoError;
    if (ok)
        data = reply->readAll();
    else if (error)
        *error = reply->errorString();
    reply->deleteLater();
    return ok;
}

void PluginsAdminDialog::buildUi()
{
    // IDD_PLUGINSADMIN_DLG: búsqueda y botones arriba, pestañas, descripción y la versión de la lista
    auto* layout = new QVBoxLayout(this);
    auto* top = new QHBoxLayout;
    auto* searchLabel = new QLabel(tr("&Find what:"), this);
    top->addWidget(searchLabel);
    _search = new QLineEdit(this);
    _search->setObjectName(QStringLiteral("search"));
    searchLabel->setBuddy(_search);
    top->addWidget(_search, 1);
    auto* next = new QPushButton(tr("&Next"), this);
    next->setObjectName(QStringLiteral("searchNext"));
    top->addWidget(next);
    top->addStretch();
    auto button = [this, top](const QString& text, const char* name) {
        auto* b = new QPushButton(text, this);
        b->setObjectName(QString::fromLatin1(name));
        top->addWidget(b);
        return b;
    };
    _install = button(tr("&Install"), "install");
    _update = button(tr("&Update"), "update");
    _deactivate = button(tr("D&isable"), "deactivate");
    _activate = button(tr("&Enable"), "activate");
    _remove = button(tr("&Remove"), "remove");
    layout->addLayout(top);

    _tabs = new QTabWidget(this);
    const QString names[] = {tr("Available"), tr("Updates"), tr("Installed"), tr("Incompatible"), tr("Disabled")};
    const char* objectNames[] = {"available", "updates", "installed", "incompatible", "disabled"};
    for (int i = 0; i < 5; ++i) {
        auto* view = new QTreeWidget(this);
        view->setObjectName(QString::fromLatin1(objectNames[i]));
        view->setHeaderLabels({tr("Plugin"), tr("Version")});
        view->setRootIsDecorated(false);
        view->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        connect(view, &QTreeWidget::currentItemChanged, this, &PluginsAdminDialog::showDescription);
        _views[i] = view;
        _tabs->addTab(view, names[i]);
    }
    connect(_tabs, &QTabWidget::currentChanged, this, [this] {
        updateButtons();
        showDescription();
    });
    layout->addWidget(_tabs, 2);

    _description = new QPlainTextEdit(this);
    _description->setObjectName(QStringLiteral("description"));
    _description->setReadOnly(true);
    layout->addWidget(_description, 1);

    auto* bottom = new QHBoxLayout;
    _status = new QLabel(this);
    _status->setObjectName(QStringLiteral("status"));
    bottom->addWidget(_status, 1);
    _nppList = new QPushButton(tr("&Notepad++ Plugin List…"), this);
    _nppList->setObjectName(QStringLiteral("nppList"));
    bottom->addWidget(_nppList);
    _listVersion = new QLabel(this);
    _listVersion->setObjectName(QStringLiteral("listVersion"));
    bottom->addWidget(_listVersion);
    layout->addLayout(bottom);
    auto* box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::accept);
    layout->addWidget(box);

    connect(next, &QPushButton::clicked, this, [this] { search(_search->text(), true); });
    connect(_search, &QLineEdit::textEdited, this, [this](const QString& text) { search(text, false); });
    connect(_install, &QPushButton::clicked, this, &PluginsAdminDialog::installChecked);
    connect(_update, &QPushButton::clicked, this, &PluginsAdminDialog::updateChecked);
    connect(_remove, &QPushButton::clicked, this, [this] { removeChecked(static_cast<Tab>(_tabs->currentIndex())); });
    connect(_deactivate, &QPushButton::clicked, this, [this] { setCheckedActive(false); });
    connect(_activate, &QPushButton::clicked, this, [this] { setCheckedActive(true); });
    connect(_nppList, &QPushButton::clicked, this, &PluginsAdminDialog::loadNppCatalog);

    _listVersion->setText(tr("Plugin list version: %1").arg(_context.catalog.version));
    resize(720, 520);
}

QString PluginsAdminDialog::disabledDir() const
{
    return QDir(_context.userPluginsDir).filePath(QStringLiteral("disabled"));
}

void PluginsAdminDialog::rescan()
{
    // Lo que hay en disco (el primero con cada nombre, como al cargar los plugins) y
    // plugins/disabled (initDisabledPluginList)
    std::vector<InstalledPlugin> installed;
    std::vector<InstalledPlugin> disabled;
    QStringList seen;
    const QString userDir = QDir(_context.userPluginsDir).absolutePath();
    auto versionOf = [this](const QString& folder, const QString& path) {
        if (_installedVersions.contains(folder))
            return PluginVersion::parse(_installedVersions.value(folder));
        return PluginVersion::parse(_context.loadedVersions.value(path));
    };
    for (const QString& dir : _context.pluginDirectories) {
        const QDir root(dir);
        for (const QString& name : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            const QString library = root.filePath(name + QLatin1Char('/') + name + QStringLiteral(".so"));
            if (!QFileInfo::exists(library) || seen.contains(name))
                continue;
            seen << name;
            InstalledPlugin plugin;
            plugin.folderName = name;
            plugin.path = library;
            plugin.version = versionOf(name, library);
            plugin.isSystem = !QFileInfo(library).absoluteFilePath().startsWith(userDir + QLatin1Char('/'));
            installed.push_back(plugin);
        }
    }
    const QDir off(disabledDir());
    for (const QString& name : off.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        InstalledPlugin plugin;
        plugin.folderName = name;
        plugin.path = off.filePath(name + QLatin1Char('/') + name + QStringLiteral(".so"));
        plugin.version = versionOf(name, plugin.path);
        disabled.push_back(plugin);
    }
    // Los que fallaron al cargar no cuentan como instalados
    std::vector<PluginInfo> incompatible = _context.loadFailures;
    for (const PluginInfo& failed : _context.loadFailures) {
        installed.erase(std::remove_if(installed.begin(), installed.end(),
                                       [&failed](const InstalledPlugin& p) { return p.path == failed.fullFilePath; }),
                        installed.end());
    }
    incompatible.insert(incompatible.end(), _nppPlugins.begin(), _nppPlugins.end());
    // Las recetas, como si fueran de la lista (si la lista no tiene ese plugin)
    PluginCatalog catalog = _context.catalog;
    for (const PluginRecipe& recipe : _context.recipes) {
        if (!catalog.find(recipe.info.folderName))
            catalog.plugins.push_back(recipe.info);
    }
    _lists = buildPluginLists(catalog, installed, disabled, incompatible, _context.appVersion);
    fillLists();
}

void PluginsAdminDialog::fillLists()
{
    const std::vector<PluginInfo>* sources[] = {&_lists.available, &_lists.updates, &_lists.installed, &_lists.incompatible,
                                                &_lists.disabled};
    for (int i = 0; i < 5; ++i) {
        QTreeWidget* view = _views[i];
        view->clear();
        for (const PluginInfo& info : *sources[i]) {
            auto* item = new QTreeWidgetItem(view, {info.displayName, info.version.toString()});
            item->setData(0, kFolderRole, info.folderName);
            item->setCheckState(0, Qt::Unchecked);
            if (info.isSystem)
                item->setToolTip(0, tr("Installed with Edita: %1").arg(info.fullFilePath));
            if (!info.recipePath.isEmpty() && (i == Available || i == Updates)) {
                item->setText(1, info.version.toString() + tr(" (from source)"));
                item->setToolTip(0, tr("Built on your computer from its source code"));
            }
        }
        _tabs->setTabText(i, _tabs->tabText(i).section(QStringLiteral(" ("), 0, 0)
                                 + (view->topLevelItemCount() ? QStringLiteral(" (%1)").arg(view->topLevelItemCount()) : QString()));
    }
    if (!_context.catalogError.isEmpty())
        _status->setText(tr("Could not read the plugin list: %1").arg(_context.catalogError));
    else if (_changed)
        _status->setText(tr("Changes will take effect when Edita restarts."));
    updateButtons();
    showDescription();
}

void PluginsAdminDialog::updateButtons()
{
    // switchDialog: cada pestaña con sus botones
    const int tab = _tabs->currentIndex();
    _install->setVisible(tab == Available);
    _update->setVisible(tab == Updates);
    _deactivate->setVisible(tab == Installed);
    _activate->setVisible(tab == Disabled);
    _remove->setVisible(tab == Installed || tab == Incompatible || tab == Disabled);
}

QTreeWidget* PluginsAdminDialog::list(Tab tab) const
{
    return _views[tab];
}

void PluginsAdminDialog::showDescription()
{
    const int tab = _tabs->currentIndex();
    QTreeWidgetItem* item = _views[tab]->currentItem();
    const std::vector<PluginInfo>* sources[] = {&_lists.available, &_lists.updates, &_lists.installed, &_lists.incompatible,
                                                &_lists.disabled};
    const PluginInfo* info = item ? findIn(*sources[tab], item->data(0, kFolderRole).toString()) : nullptr;
    _description->setPlainText(info ? info->describe() : QString());
}

QString PluginsAdminDialog::descriptionText() const
{
    return _description->toPlainText();
}

bool PluginsAdminDialog::setChecked(Tab tab, const QString& folderName, bool isChecked)
{
    for (int i = 0; i < _views[tab]->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = _views[tab]->topLevelItem(i);
        if (item->data(0, kFolderRole).toString() == folderName) {
            item->setCheckState(0, isChecked ? Qt::Checked : Qt::Unchecked);
            return true;
        }
    }
    return false;
}

std::vector<PluginInfo> PluginsAdminDialog::checked(Tab tab) const
{
    const std::vector<PluginInfo>* sources[] = {&_lists.available, &_lists.updates, &_lists.installed, &_lists.incompatible,
                                                &_lists.disabled};
    std::vector<PluginInfo> out;
    for (int i = 0; i < _views[tab]->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = _views[tab]->topLevelItem(i);
        if (item->checkState(0) != Qt::Checked)
            continue;
        if (const PluginInfo* info = findIn(*sources[tab], item->data(0, kFolderRole).toString()))
            out.push_back(*info);
    }
    return out;
}

bool PluginsAdminDialog::confirmChanges(const QString& what)
{
    // N++: "If you click YES, you will quit Notepad++ to continue the operations…"
    return _prompter.confirm(this, tr("Plugins Admin"),
                             what + QLatin1Char(' ') + tr("Changes will take effect when Edita restarts.\nContinue?"));
}

QString PluginsAdminDialog::resolveRepository(const QString& repository) const
{
    const QUrl url(repository);
    if (url.isRelative() && !repository.startsWith(QLatin1Char('/')))
        return QFileInfo(_context.catalog.sourcePath).absoluteDir().filePath(repository);
    return repository;
}

bool PluginsAdminDialog::installChecked()
{
    const std::vector<PluginInfo> plugins = checked(Available);
    if (plugins.empty() || !confirmChanges(tr("%n plugin(s) will be installed.", nullptr, static_cast<int>(plugins.size()))))
        return false;
    QStringList errors;
    for (const PluginInfo& info : plugins) {
        QByteArray zip;
        QString id;
        QString error;
        if (!obtainPackage(info, zip, id, &error)
            || !plugin_installer::installPackage(zip, info.folderName, id, _context.userPluginsDir, false, &error)) {
            errors << QStringLiteral("%1: %2").arg(info.displayName, error);
            continue;
        }
        _installedVersions[info.folderName] = info.version.toString();
        _changed = true;
    }
    if (!errors.isEmpty())
        _prompter.warn(this, tr("Install plugins"), errors.join(QLatin1Char('\n')));
    rescan();
    return errors.isEmpty();
}

bool PluginsAdminDialog::updateChecked()
{
    const std::vector<PluginInfo> plugins = checked(Updates);
    if (plugins.empty() || !confirmChanges(tr("%n plugin(s) will be updated.", nullptr, static_cast<int>(plugins.size()))))
        return false;
    QStringList errors;
    for (const PluginInfo& info : plugins) {
        QByteArray zip;
        QString id;
        QString error;
        // -unzipTo -clean: la versión nueva va siempre a la carpeta del usuario (si el
        // plugin venía con Edita, la del usuario tiene prioridad al cargar)
        if (!obtainPackage(info, zip, id, &error)
            || !plugin_installer::installPackage(zip, info.folderName, id, _context.userPluginsDir, true, &error)) {
            errors << QStringLiteral("%1: %2").arg(info.displayName, error);
            continue;
        }
        _installedVersions[info.folderName] = info.version.toString();
        _changed = true;
    }
    if (!errors.isEmpty())
        _prompter.warn(this, tr("Update plugins"), errors.join(QLatin1Char('\n')));
    rescan();
    return errors.isEmpty();
}

const PluginRecipe* PluginsAdminDialog::recipeFor(const PluginInfo& info) const
{
    for (const PluginRecipe& recipe : _context.recipes) {
        if (recipe.path == info.recipePath)
            return &recipe;
    }
    return nullptr;
}

bool PluginsAdminDialog::obtainPackage(const PluginInfo& info, QByteArray& zip, QString& expectedId, QString* error)
{
    if (const PluginRecipe* recipe = recipeFor(info)) {
        expectedId.clear(); // lo acaba de compilar Edita
        return buildFromRecipe(*recipe, zip, error);
    }
    if (info.repository.isEmpty()) {
        *error = tr("ships with Edita (there is no package to download)");
        return false;
    }
    expectedId = info.id;
    const QString source = resolveRepository(info.repository);
    const QUrl url = QUrl(source).scheme().isEmpty() ? QUrl::fromLocalFile(source) : QUrl(source);
    return _fetch(url, zip, error);
}

bool PluginsAdminDialog::buildFromRecipe(const PluginRecipe& recipe, QByteArray& zip, QString* error)
{
    const PluginInfo& info = recipe.info;
    // Un paquete ya compilado de esa versión (plugins/local) no se vuelve a compilar, si se
    // compiló con el mismo Qt (<paquete>.build): el AppImage y Edita instalado usan Qts distintos
    const QString cached = QDir(_context.localPackagesDir)
                               .filePath(QStringLiteral("%1-%2.zip").arg(info.folderName, info.version.toString()));
    const QString stamp = cached + QStringLiteral(".build");
    const QString buildTag = recipeBuildTag(_context.sdk);
    QFile stampFile(stamp);
    const bool sameBuild = stampFile.open(QIODevice::ReadOnly) && QString::fromUtf8(stampFile.readAll()).trimmed() == buildTag;
    if (QFile file(cached); sameBuild && file.open(QIODevice::ReadOnly)) {
        zip = file.readAll();
        _lastBuildLog = tr("Package already built: %1\n").arg(cached);
        return true;
    }
    // Lo necesario, antes de descargar nada
    QStringList requirements = recipe.requires_;
    requirements << QStringLiteral("cmake") << QStringLiteral("c++");
    if (!recipe.patches.isEmpty())
        requirements << QStringLiteral("patch");
    if (const QStringList missing = missingRequirements(requirements); !missing.isEmpty()) {
        *error = tr("to build it, the system is missing: %1").arg(missing.join(QStringLiteral(", ")));
        return false;
    }
    if (!_context.sdk.isValid()) {
        *error = tr("the Edita plugin SDK was not found");
        return false;
    }
    QByteArray source;
    if (!_fetch(QUrl(recipe.sourceUrl).scheme().isEmpty() ? QUrl::fromLocalFile(recipe.sourceUrl) : QUrl(recipe.sourceUrl),
                source, error))
        return false;

    // La compilación, con su registro a la vista y la posibilidad de cancelarla
    QDialog progress(this);
    progress.setObjectName(QStringLiteral("recipeBuild"));
    progress.setWindowTitle(tr("Building %1").arg(info.displayName));
    auto* layout = new QVBoxLayout(&progress);
    layout->addWidget(new QLabel(tr("Building %1 %2 from its source code…").arg(info.displayName, info.version.toString()), &progress));
    auto* log = new QPlainTextEdit(&progress);
    log->setObjectName(QStringLiteral("buildLog"));
    log->setReadOnly(true);
    log->setFont(desktopfonts::fixedFont());
    layout->addWidget(log, 1);
    auto* bar = new QProgressBar(&progress);
    bar->setRange(0, 0);
    layout->addWidget(bar);
    auto* cancel = new QPushButton(tr("Cancel"), &progress);
    cancel->setObjectName(QStringLiteral("cancelBuild"));
    layout->addWidget(cancel, 0, Qt::AlignRight);
    progress.resize(720, 420);

    RecipeBuilder builder(recipe, _context.sdk, QDir(_context.buildRoot).filePath(info.folderName));
    QEventLoop loop;
    bool ok = false;
    QString buildError;
    connect(&builder, &RecipeBuilder::output, log, [log](const QString& text) {
        log->moveCursor(QTextCursor::End);
        log->insertPlainText(text);
        log->ensureCursorVisible();
    });
    connect(&builder, &RecipeBuilder::finished, &loop, [&](bool success, const QString& message) {
        ok = success;
        buildError = message;
        loop.quit();
    });
    connect(cancel, &QPushButton::clicked, &builder, &RecipeBuilder::cancel);
    connect(&progress, &QDialog::rejected, &builder, &RecipeBuilder::cancel);
    progress.show();
    builder.start(source);
    if (builder.isRunning())
        loop.exec();
    _lastBuildLog = builder.log();
    progress.hide();
    if (!ok) {
        *error = tr("could not be built: %1").arg(buildError);
        return false;
    }
    // El paquete queda en plugins/local para reinstalarlo sin compilar
    QDir().mkpath(_context.localPackagesDir);
    QFile::remove(cached);
    QFile::copy(builder.packagePath(), cached);
    if (QFile out(stamp); out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        out.write(buildTag.toUtf8() + '\n');
    QFile file(builder.packagePath());
    if (!file.open(QIODevice::ReadOnly)) {
        *error = tr("cannot read the built package");
        return false;
    }
    zip = file.readAll();
    return true;
}

bool PluginsAdminDialog::removeChecked(Tab tab)
{
    if (tab != Installed && tab != Incompatible && tab != Disabled)
        return false;
    const std::vector<PluginInfo> plugins = checked(tab);
    if (plugins.empty() || !confirmChanges(tr("%n plugin(s) will be removed.", nullptr, static_cast<int>(plugins.size()))))
        return false;
    QStringList errors;
    for (const PluginInfo& info : plugins) {
        if (info.isSystem) {
            errors << tr("%1: it was installed with Edita (%2) and cannot be removed from here").arg(info.displayName, info.fullFilePath);
            continue;
        }
        // Un incompatible de la lista de N++ no está en disco
        if (tab == Incompatible && info.fullFilePath.isEmpty()) {
            errors << tr("%1: it is not installed").arg(info.displayName);
            continue;
        }
        // La carpeta que contiene la del plugin (<raíz>/<carpeta>/<carpeta>.so)
        const QString root = tab == Disabled ? disabledDir()
                           : tab == Incompatible ? QFileInfo(QFileInfo(info.fullFilePath).absolutePath()).absolutePath()
                                                 : _context.userPluginsDir;
        QString error;
        if (!plugin_installer::removePlugin(root, info.folderName, &error)) {
            errors << QStringLiteral("%1: %2").arg(info.displayName, error);
            continue;
        }
        _changed = true;
    }
    if (!errors.isEmpty())
        _prompter.warn(this, tr("Remove plugins"), errors.join(QLatin1Char('\n')));
    rescan();
    return errors.isEmpty();
}

bool PluginsAdminDialog::setCheckedActive(bool activate)
{
    const std::vector<PluginInfo> plugins = checked(activate ? Disabled : Installed);
    const int count = static_cast<int>(plugins.size());
    if (plugins.empty()
        || !confirmChanges(activate ? tr("%n plugin(s) will be enabled.", nullptr, count)
                                    : tr("%n plugin(s) will be disabled.", nullptr, count)))
        return false;
    QStringList errors;
    for (const PluginInfo& info : plugins) {
        if (info.isSystem) {
            errors << tr("%1: it was installed with Edita and cannot be disabled from here").arg(info.displayName);
            continue;
        }
        QString error;
        const QString from = activate ? disabledDir() : _context.userPluginsDir;
        const QString to = activate ? _context.userPluginsDir : disabledDir();
        if (!plugin_installer::movePlugin(from, to, info.folderName, &error)) {
            errors << QStringLiteral("%1: %2").arg(info.displayName, error);
            continue;
        }
        _changed = true;
    }
    if (!errors.isEmpty())
        _prompter.warn(this, activate ? tr("Enable plugins") : tr("Disable plugins"), errors.join(QLatin1Char('\n')));
    rescan();
    return errors.isEmpty();
}

bool PluginsAdminDialog::search(const QString& text, bool next)
{
    // searchInPlugins: al menos dos letras; primero los nombres y luego las descripciones
    if (text.size() < 2)
        return false;
    const int tab = _tabs->currentIndex();
    QTreeWidget* view = _views[tab];
    const std::vector<PluginInfo>* sources[] = {&_lists.available, &_lists.updates, &_lists.installed, &_lists.incompatible,
                                                &_lists.disabled};
    const int count = view->topLevelItemCount();
    const int current = view->currentItem() ? view->indexOfTopLevelItem(view->currentItem()) : -1;
    for (const bool inDescription : {false, true}) {
        for (int step = 0; step < count; ++step) {
            const int start = next ? current + 1 : std::max(current, 0);
            const int i = (start + step) % count;
            QTreeWidgetItem* item = view->topLevelItem(i);
            const PluginInfo* info = findIn(*sources[tab], item->data(0, kFolderRole).toString());
            const QString haystack = inDescription ? (info ? info->description : QString()) : item->text(0);
            if (haystack.contains(text, Qt::CaseInsensitive)) {
                view->setCurrentItem(item);
                return true;
            }
        }
    }
    return false;
}

bool PluginsAdminDialog::loadNppCatalog()
{
    if (!_context.nppCatalogUrl.isLocalFile()
        && !_prompter.confirm(this, tr("Notepad++ plugin list"),
                              tr("The official Notepad++ plugin list will be downloaded from\n%1\n\nIts plugins are "
                                 "Windows DLLs: they will appear under Incompatible. Continue?")
                                  .arg(_context.nppCatalogUrl.toString())))
        return false;
    QByteArray json;
    QString error;
    PluginCatalog npp;
    if (!_fetch(_context.nppCatalogUrl, json, &error) || !PluginCatalog::parse(json, npp, &error)) {
        _prompter.warn(this, tr("Notepad++ plugin list"), tr("Could not read the list: %1").arg(error));
        return false;
    }
    _nppPlugins.clear();
    for (PluginInfo info : npp.plugins) {
        // Los que Edita ya ofrece (un port en su lista o una receta) no son incompatibles
        const bool offered = _context.catalog.find(info.folderName)
            || std::any_of(_context.recipes.begin(), _context.recipes.end(), [&info](const PluginRecipe& recipe) {
                   return recipe.info.folderName.compare(info.folderName, Qt::CaseInsensitive) == 0;
               });
        if (offered)
            continue;
        info.problem = tr("Notepad++ plugin for Windows (DLL); it cannot be loaded on Linux. If it is open source, "
                          "it can be ported with the Edita SDK (see docs/plugins.md).");
        info.repository.clear(); // no se instala
        _nppPlugins.push_back(std::move(info));
    }
    _nppList->setEnabled(false);
    rescan();
    _tabs->setCurrentIndex(Incompatible);
    _status->setText(tr("Notepad++ list %1: %n plugin(s) (incompatible)", nullptr, static_cast<int>(_nppPlugins.size()))
                         .arg(npp.version));
    return true;
}

} // namespace edita
