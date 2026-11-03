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

#include "core/PluginCatalog.h"
#include "core/PluginRecipe.h"
#include "core/RemoteCatalog.h"

#include <QDialog>
#include <QMap>
#include <QUrl>

#include <functional>
#include <vector>

class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QNetworkReply;
class QPlainTextEdit;
class QPushButton;
class QTabWidget;
class QTreeWidget;

namespace edita {

class Prompter;

// Lo que el administrador necesita saber de Edita
struct PluginsAdminContext
{
    PluginCatalog catalog;             // la que se muestra: la de Edita y la del catálogo
    QString catalogError;              // si no se pudo leer la lista
    // El catálogo de Edita (alktrip/edita-plugins): su lista firmada (core/RemoteCatalog.h)
    PluginCatalog bundledCatalog;      // la lista que trae Edita (editaPluginList.json)
    remote_catalog::Settings catalogSettings;
    QString remoteVersion;             // la última lista del catálogo aceptada (vacío: ninguna)
    QString catalogNote;               // por qué sus binarios no sirven en este equipo
    QStringList pluginDirectories;     // donde se buscan (el primero que tenga un nombre gana)
    QString userPluginsDir;            // donde se instala (~/.config/edita/plugins)
    QMap<QString, QString> loadedVersions; // ruta del .so cargado → edita_plugin_version()
    std::vector<PluginInfo> loadFailures;  // los que no se pudieron cargar
    PluginVersion appVersion;
    QUrl nppCatalogUrl;                // la lista oficial de Notepad++ (para consultarla)
    // Plugins que se compilan en el equipo desde su código (recetas)
    std::vector<PluginRecipe> recipes;
    PluginSdk sdk;
    QString buildRoot;                 // donde se compilan (~/.cache/edita/plugin-builds)
    QString localPackagesDir;          // los paquetes compilados (~/.config/edita/plugins/local)
};

// PluginsAdminDlg de N++ (WinControls/PluginsAdmin): pestañas Disponibles,
// Actualizaciones, Instalados, Incompatibles y Desactivados, con casillas, búsqueda
// por nombre y descripción, y los botones Instalar, Actualizar, Quitar, Desactivar y
// Activar. N++ sale y deja el trabajo a gup.exe; aquí se hace en el acto y los
// cambios valen al reiniciar Edita (que lo ofrece al cerrar el diálogo).
class PluginsAdminDialog : public QDialog
{
    Q_OBJECT

public:
    enum Tab { Available = 0, Updates = 1, Installed = 2, Incompatible = 3, Disabled = 4 };

    // Descarga (o lee) un paquete o una lista. La de serie usa QNetworkAccessManager
    // para http(s) y lee los archivos locales.
    using Fetcher = std::function<bool(const QUrl& url, QByteArray& data, QString* error)>;

    PluginsAdminDialog(PluginsAdminContext context, Prompter& prompter, QWidget* parent = nullptr);

    void setFetcher(Fetcher fetcher) { _fetch = std::move(fetcher); }
    static bool defaultFetch(const QUrl& url, QByteArray& data, QString* error);

    // La lista del catálogo de Edita, sin esperar: la que se aceptó la última vez, o la que
    // trae Edita, y la nueva se descarga aparte. Si es válida (firma, versión), se guarda y
    // las pestañas se actualizan. Al abrir el diálogo se pide sola.
    void refreshCatalog();
    bool isRefreshingCatalog() const { return _catalogReply != nullptr; }

    // La lista de Edita con la del catálogo, adaptada a este equipo (los binarios que no
    // sirven, a compilar con su receta)
    static void useRemoteCatalog(PluginsAdminContext& context, PluginCatalog remote);

    // ¿Se instaló, actualizó, quitó, activó o desactivó algo?
    bool hasChanges() const { return _changed; }

    QTreeWidget* list(Tab tab) const;
    const PluginLists& lists() const { return _lists; }
    // Marca o desmarca la casilla de un plugin (por su carpeta)
    bool setChecked(Tab tab, const QString& folderName, bool checked = true);

    // Las operaciones con los marcados de la pestaña correspondiente
    bool installChecked();
    bool updateChecked();
    bool removeChecked(Tab tab);
    bool setCheckedActive(bool activate); // Activar (Desactivados) o Desactivar (Instalados)

    // searchInPlugins: desde la selección, en los nombres y luego en las descripciones
    bool search(const QString& text, bool next = false);
    // Lista oficial de N++: sus plugins son DLL de Windows → Incompatibles
    bool loadNppCatalog();

    QString descriptionText() const;
    // El registro de la última compilación desde el código
    const QString& lastBuildLog() const { return _lastBuildLog; }

signals:
    // Terminó refreshCatalog(): con la lista nueva (ok) o sin ella (error)
    void catalogRefreshed(bool ok, const QString& error);

private:
    void buildUi();
    void rescan();
    void fillLists();
    void showDescription();
    void updateButtons();
    QString resolveRepository(const QString& repository) const;
    QString disabledDir() const;
    std::vector<PluginInfo> checked(Tab tab) const;
    bool confirmChanges(const QString& what); // "Se van a instalar 2 plugins." + aviso del reinicio
    // Obtiene el paquete de un plugin: lo descarga o, si tiene receta, lo compila
    bool obtainPackage(const PluginInfo& info, QByteArray& zip, QString& expectedId, QString* error);
    bool buildFromRecipe(const PluginRecipe& recipe, QByteArray& zip, QString* error);
    // Un plugin del catálogo sin binario para este equipo: su receta de recipes.zip
    bool buildFromCatalogRecipe(const PluginInfo& info, QByteArray& zip, QString* error);
    void catalogDownloaded(const QByteArray& json, const QByteArray& signature);
    void catalogFailed(const QString& error);
    void showListVersion();
    const PluginRecipe* recipeFor(const PluginInfo& info) const;

    PluginsAdminContext _context;
    Prompter& _prompter;
    Fetcher _fetch;
    PluginLists _lists;
    std::vector<PluginInfo> _nppPlugins; // de la lista de N++, si se ha consultado
    QMap<QString, QString> _installedVersions; // carpeta → versión instalada en esta sesión
    bool _changed = false;
    QString _lastBuildLog;
    QNetworkAccessManager* _network = nullptr;
    QNetworkReply* _catalogReply = nullptr;

    QTabWidget* _tabs = nullptr;
    QTreeWidget* _views[5] = {};
    QLineEdit* _search = nullptr;
    QPushButton* _install = nullptr;
    QPushButton* _update = nullptr;
    QPushButton* _remove = nullptr;
    QPushButton* _deactivate = nullptr;
    QPushButton* _activate = nullptr;
    QPushButton* _nppList = nullptr;
    QPlainTextEdit* _description = nullptr;
    QLabel* _listVersion = nullptr;
    QLabel* _status = nullptr;
};

} // namespace edita
