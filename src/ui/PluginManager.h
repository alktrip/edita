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


#pragma once

#include <edita_plugin.h>

#include <QKeySequence>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <vector>

class QLibrary;
class ScintillaEditBase;

namespace edita {

// Carga y gestiona los plugins (PluginsManager de N++, MISC/PluginsManager/).
// Los plugins hablan con el editor con data.send(target, mensaje, w, l): con
// target = host, los NPPM_* los contesta la ventana (HostHandler); con target =
// una de las dos vistas, el mensaje SCI_* va a esa vista de Scintilla.
class PluginManager : public QObject
{
    Q_OBJECT

public:
    using HostHandler = std::function<intptr_t(unsigned int message, uintptr_t wParam, intptr_t lParam)>;

    // Primer identificador de orden de plugin (ID_PLUGINS_CMD de N++)
    static constexpr int kFirstCommandId = 22000;

    struct Command
    {
        int cmdId = 0;
        QString name;
        EditaPluginCommand function = nullptr; // nullptr = separador
        bool checked = false;
        QKeySequence shortcut;
    };

    struct Plugin
    {
        QString name;     // el de getName()
        QString path;
        QString version;  // edita_plugin_version() si lo exporta (el recurso de versión de una DLL)
        std::unique_ptr<QLibrary> library;
        void (*beNotified)(SCNotification*) = nullptr;
        intptr_t (*messageProc)(unsigned int, uintptr_t, intptr_t) = nullptr;
        std::vector<Command> commands;
    };

    PluginManager(void* host, ScintillaEditBase* mainEditor, ScintillaEditBase* subEditor, HostHandler handler,
                  QObject* parent = nullptr);
    ~PluginManager() override;

    // PluginsManager::loadPlugins: <carpeta>/<Nombre>/<Nombre>.so en cada carpeta.
    // Un plugin con un nombre ya cargado se ignora (gana la primera carpeta).
    // Devuelve los avisos (plugins que no se pudieron cargar y por qué).
    QStringList loadFrom(const QStringList& directories);
    // Los que no se pudieron cargar (la pestaña Incompatibles del administrador)
    struct Failure
    {
        QString path;
        QString reason;
    };
    const std::vector<Failure>& failures() const { return _failures; }
    const std::vector<std::unique_ptr<Plugin>>& plugins() const { return _plugins; }

    // Orden de un plugin por su cmdID; nullptr si no es de ningún plugin
    const Command* command(int cmdId) const;
    bool runCommand(int cmdId);
    // NPPM_SETMENUITEMCHECK
    bool setChecked(int cmdId, bool checked);

    // PluginsManager::notify: a todos los plugins que tengan beNotified
    void notify(SCNotification& notification);
    // Atajo para NPPN_*: hwndFrom = host, idFrom = bufferID
    void notifyHost(unsigned int code, uintptr_t bufferId);

    // NPPM_MSGTOPLUGIN: messageProc del plugin cuyo archivo es `module` ("PythonScript.so").
    // false si no hay ninguno con ese nombre.
    bool sendToPlugin(const QString& module, unsigned int message, uintptr_t wParam, intptr_t lParam);
    // Nombre del submenú de un plugin por su archivo (vacío si no está cargado)
    QString nameOfModule(const QString& module) const;

    // NPPM_CREATESCINTILLAHANDLE / NPPM_DESTROYSCINTILLAHANDLE: editores de Scintilla
    // para los plugins (la consola de PythonScript, notepad.createScintilla()). Se
    // les habla con data.send(editor, SCI_*, …) como a las dos vistas.
    ScintillaEditBase* createEditor(QWidget* parent);
    bool destroyEditor(void* editor);

    void* host() const { return _host; }
    ScintillaEditBase* editorFor(void* target) const;

signals:
    void checkChanged(int cmdId, bool checked);

private:
    static intptr_t send(void* target, unsigned int message, uintptr_t wParam, intptr_t lParam);
    bool loadPlugin(const QString& path, QString* error);

    void* _host;
    ScintillaEditBase* _mainEditor;
    ScintillaEditBase* _subEditor;
    HostHandler _handler;
    std::vector<std::unique_ptr<Plugin>> _plugins;
    std::vector<QPointer<ScintillaEditBase>> _extraEditors;
    std::vector<Failure> _failures;
    int _nextCommandId = kFirstCommandId;
};

} // namespace edita
