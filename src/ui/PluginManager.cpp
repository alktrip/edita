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


#include "ui/PluginManager.h"

#include "core/Macro.h"

#include <ScintillaEditBase.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibrary>

#include <algorithm>
#include <cstring>

namespace edita {

namespace {

// data.send es una función de C sin contexto: apunta al gestor activo
PluginManager* g_current = nullptr;

} // namespace

PluginManager::PluginManager(void* host, ScintillaEditBase* mainEditor, ScintillaEditBase* subEditor,
                             HostHandler handler, QObject* parent)
    : QObject(parent)
    , _host(host)
    , _mainEditor(mainEditor)
    , _subEditor(subEditor)
    , _handler(std::move(handler))
{
    g_current = this;
}

PluginManager::~PluginManager()
{
    if (g_current == this)
        g_current = nullptr;
    // Los editores sin padre que crearon los plugins
    for (const QPointer<ScintillaEditBase>& editor : _extraEditors) {
        if (editor && !editor->parent())
            delete editor.data();
    }
    // Las bibliotecas se descargan al destruir los QLibrary (unload explícito no:
    // un plugin puede haber dejado hilos o atexit registrados)
}

ScintillaEditBase* PluginManager::editorFor(void* target) const
{
    if (target == _mainEditor)
        return _mainEditor;
    if (target == _subEditor)
        return _subEditor;
    for (const QPointer<ScintillaEditBase>& editor : _extraEditors) {
        if (editor && editor.data() == target)
            return editor.data();
    }
    return nullptr;
}

ScintillaEditBase* PluginManager::createEditor(QWidget* parent)
{
    // ScintillaEditView::init sin documento propio: UTF-8, como las vistas
    auto* editor = new ScintillaEditBase(parent);
    editor->send(SCI_SETCODEPAGE, SC_CP_UTF8);
    _extraEditors.erase(std::remove_if(_extraEditors.begin(), _extraEditors.end(),
                                       [](const QPointer<ScintillaEditBase>& e) { return e.isNull(); }),
                        _extraEditors.end());
    _extraEditors.emplace_back(editor);
    return editor;
}

bool PluginManager::destroyEditor(void* target)
{
    for (auto it = _extraEditors.begin(); it != _extraEditors.end(); ++it) {
        if (*it && it->data() == target) {
            delete it->data();
            _extraEditors.erase(it);
            return true;
        }
    }
    return false;
}

intptr_t PluginManager::send(void* target, unsigned int message, uintptr_t wParam, intptr_t lParam)
{
    PluginManager* self = g_current;
    if (!self || !target)
        return 0;
    if (target == self->_host)
        return self->_handler ? self->_handler(message, wParam, lParam) : 0;
    if (ScintillaEditBase* editor = self->editorFor(target))
        return editor->send(message, wParam, lParam);
    return 0;
}

QStringList PluginManager::loadFrom(const QStringList& directories)
{
    QStringList warnings;
    for (const QString& directory : directories) {
        const QDir dir(directory);
        for (const QString& name : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            const QString path = dir.filePath(name + QLatin1Char('/') + name + QStringLiteral(".so"));
            if (!QFileInfo::exists(path))
                continue;
            QString error;
            if (!loadPlugin(path, &error)) {
                warnings << QStringLiteral("%1: %2").arg(path, error);
                _failures.push_back({path, error});
            }
        }
    }
    return warnings;
}

bool PluginManager::loadPlugin(const QString& path, QString* error)
{
    auto library = std::make_unique<QLibrary>(path);
    if (!library->load()) {
        *error = library->errorString();
        return false;
    }
    using ApiVersionFn = int (*)();
    using SetInfoFn = void (*)(EditaData);
    using GetNameFn = const char* (*)();
    using GetFuncsFn = EditaFuncItem* (*)(int*);
    const auto apiVersion = reinterpret_cast<ApiVersionFn>(library->resolve("edita_plugin_api_version"));
    const auto setInfo = reinterpret_cast<SetInfoFn>(library->resolve("setInfo"));
    const auto getName = reinterpret_cast<GetNameFn>(library->resolve("getName"));
    const auto getFuncs = reinterpret_cast<GetFuncsFn>(library->resolve("getFuncsArray"));
    if (!apiVersion || !setInfo || !getName || !getFuncs) {
        *error = tr("is not an Edita plugin (exported functions are missing)");
        return false;
    }
    if (apiVersion() != EDITA_PLUGIN_API_VERSION) {
        *error = tr("is made for API version %1 (Edita uses %2)").arg(apiVersion()).arg(EDITA_PLUGIN_API_VERSION);
        return false;
    }
    const QString name = QString::fromUtf8(getName() ? getName() : "");
    if (name.isEmpty()) {
        *error = tr("getName() returns an empty name");
        return false;
    }
    for (const auto& plugin : _plugins) {
        if (plugin->name == name) {
            *error = tr("a plugin named \"%1\" is already loaded").arg(name);
            return false;
        }
    }

    auto plugin = std::make_unique<Plugin>();
    plugin->name = name;
    plugin->path = path;
    using VersionFn = const char* (*)();
    if (const auto version = reinterpret_cast<VersionFn>(library->resolve("edita_plugin_version")))
        plugin->version = QString::fromUtf8(version() ? version() : "");
    plugin->beNotified = reinterpret_cast<void (*)(SCNotification*)>(library->resolve("beNotified"));
    plugin->messageProc = reinterpret_cast<intptr_t (*)(unsigned int, uintptr_t, intptr_t)>(library->resolve("messageProc"));

    // Como PluginsManager::loadPluginFromPath: setInfo antes de pedir las órdenes
    EditaData data{_host, _mainEditor, _subEditor, &PluginManager::send};
    setInfo(data);
    int count = 0;
    EditaFuncItem* items = getFuncs(&count);
    for (int i = 0; items && i < count; ++i) {
        EditaFuncItem& item = items[i];
        item.cmdID = _nextCommandId++; // el plugin lo usa con NPPM_SETMENUITEMCHECK
        Command command;
        command.cmdId = item.cmdID;
        command.name = QString::fromUtf8(item.itemName, static_cast<qsizetype>(strnlen(item.itemName, EDITA_MENU_ITEM_SIZE)));
        command.function = item.pFunc;
        command.checked = item.init2Check;
        if (item.pShKey && item.pShKey->key) {
            if (const int key = qtKeyFromVirtualKey(item.pShKey->key)) {
                Qt::KeyboardModifiers modifiers;
                modifiers.setFlag(Qt::ControlModifier, item.pShKey->isCtrl);
                modifiers.setFlag(Qt::AltModifier, item.pShKey->isAlt);
                modifiers.setFlag(Qt::ShiftModifier, item.pShKey->isShift);
                command.shortcut = QKeySequence(QKeyCombination(modifiers, static_cast<Qt::Key>(key)));
            }
        }
        plugin->commands.push_back(command);
    }
    plugin->library = std::move(library);
    _plugins.push_back(std::move(plugin));
    return true;
}

const PluginManager::Command* PluginManager::command(int cmdId) const
{
    for (const auto& plugin : _plugins) {
        for (const Command& command : plugin->commands) {
            if (command.cmdId == cmdId)
                return &command;
        }
    }
    return nullptr;
}

bool PluginManager::runCommand(int cmdId)
{
    const Command* cmd = command(cmdId);
    if (!cmd || !cmd->function)
        return false;
    cmd->function();
    return true;
}

bool PluginManager::setChecked(int cmdId, bool checked)
{
    for (const auto& plugin : _plugins) {
        for (Command& command : plugin->commands) {
            if (command.cmdId == cmdId) {
                command.checked = checked;
                emit checkChanged(cmdId, checked);
                return true;
            }
        }
    }
    return false;
}

void PluginManager::notify(SCNotification& notification)
{
    for (const auto& plugin : _plugins) {
        if (plugin->beNotified)
            plugin->beNotified(&notification);
    }
}

void PluginManager::notifyHost(unsigned int code, uintptr_t bufferId)
{
    if (_plugins.empty())
        return;
    SCNotification notification{};
    notification.nmhdr.hwndFrom = _host;
    notification.nmhdr.idFrom = bufferId;
    notification.nmhdr.code = code;
    notify(notification);
}

bool PluginManager::sendToPlugin(const QString& module, unsigned int message, uintptr_t wParam, intptr_t lParam)
{
    for (const auto& plugin : _plugins) {
        if (QFileInfo(plugin->path).fileName() != module)
            continue;
        if (plugin->messageProc)
            plugin->messageProc(message, wParam, lParam);
        return true;
    }
    return false;
}

QString PluginManager::nameOfModule(const QString& module) const
{
    for (const auto& plugin : _plugins) {
        if (QFileInfo(plugin->path).fileName() == module)
            return plugin->name;
    }
    return {};
}

} // namespace edita
