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

#include "core/Encoding.h"
#include "core/Eol.h"
#include "core/LineOperations.h"
#include "core/Macro.h"
#include "core/Preferences.h"
#include "core/Session.h"
#include "doc/Buffer.h"
#include "doc/DocumentManager.h"
#include "search/SearchEngine.h"
#include "syntax/SyntaxManager.h"
#include "ui/CommandPalette.h"
#include "ui/EditCommands.h"
#include "ui/FindReplaceDialog.h"
#include "ui/PluginsAdminDialog.h"
#include "ui/Shortcuts.h"

#include <edita_plugin.h>

#include <QIcon>
#include <QMainWindow>
#include <QPointer>
#include <QSet>

#include <atomic>
#include <functional>
#include <map>
#include <memory>

class QActionGroup;
class QDockWidget;
class QFileSystemWatcher;
class QMenu;
class QSplitter;
class QThread;
class QToolBar;
class QDockWidget;

namespace edita {
struct InstanceRequest;
class DocumentListPanel;
class DocumentMapPanel;
class FileBrowserPanel;
class FunctionListPanel;
class FunctionParsers;
class ColorPreview;
class GitChanges;
class NotificationBar;
class ScrollBarMarks;
}
class QPagedPaintDevice;
class QTimer;
class ScintillaEditBase;

namespace edita {

struct FileSearchRequest;
class DocStatusBar;
class EditorPane;
class FileSearchJob;
class SearchResultsPanel;
class EditorView;
class Prompter;
class AutoCompletion;
class PluginManager;

// Ventana principal. Port de la lógica de Notepad_plus (NppIO.cpp, NppCommands.cpp):
// abrir, guardar, cerrar, codificación y fin de línea. Las preguntas al usuario
// pasan por un Prompter para que los tests las contesten.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // El ícono de Edita (resources/icons, incluido en el binario) en todos sus tamaños
    static QIcon applicationIcon();

    void setPrompter(std::unique_ptr<Prompter> prompter);

    DocumentManager& documents() { return _documents; }
    SyntaxManager& syntax() { return _syntax; }
    // Vista activa (la que tiene el foco) y las dos vistas de la división
    EditorPane* pane() const { return _activePane; }
    EditorPane* mainPane() const { return _mainPane; }
    EditorPane* subPane() const { return _subPane; }
    QSplitter* splitter() const { return _splitter; }
    EditorView* editor() const;
    DocStatusBar* docStatusBar() const { return _statusBar; }
    BufferId currentBuffer() const;

    // --- Archivo -------------------------------------------------------------
    BufferId newDocument();
    // doOpen: si ya está abierto lo activa; un directorio abre sus archivos.
    // Con `askToCreate` (línea de comandos) ofrece crear los que no existen.
    BufferId openFile(const QString& path, bool askToCreate = false);
    void openFiles(const QStringList& paths, bool askToCreate = false);
    void openWithDialog();
    bool saveBuffer(BufferId id);
    bool saveBufferAs(BufferId id, bool isCopy = false);
    // Guardar como sin preguntar el nombre (NPPM_SAVECURRENTFILEAS)
    bool saveBufferTo(BufferId id, const QString& absolutePath, bool isCopy = false);
    bool saveAll();
    bool reloadBuffer(BufferId id);
    bool closeBuffer(BufferId id);
    bool closeAll();
    bool closeAllBut(BufferId keep);
    bool closeAllToLeft(BufferId id);
    bool closeAllToRight(BufferId id);

    // --- Imprimir (Notepad_plus::filePrint, Printer) ------------------------------------
    // Archivo > Imprimir…: diálogo de impresión (con selección y páginas)
    void printWithDialog();
    // Archivo > Imprimir ahora: la impresora por defecto, sin diálogo
    void printNow();
    // Archivo > Exportar a PDF…; devuelve las páginas (0 si se canceló o falló)
    int exportToPdf();
    int exportToPdf(const QString& path, bool selectionOnly = false);
    // Imprime el documento actual en cualquier dispositivo paginado
    int printCurrent(QPagedPaintDevice& device, bool selectionOnly = false, int fromPage = 0, int toPage = 0);

    // --- Codificación (menú "Codificación") ------------------------------------
    // "Codificar en UTF-8/UTF-8-BOM/UTF-16…"
    void encodeAs(UniMode mode);
    // "Juegos de caracteres > X": vuelve a leer el archivo con ese codepage
    void reinterpretAs(int codepage);
    // "Convertir a…": cambia cómo se guardará; el texto no cambia
    void convertTo(const TextEncoding& encoding);

    // --- Vista dividida (Notepad_plus::docGotoAnotherEditView…) ------------------------
    void moveToOtherView();
    void cloneToOtherView();
    // F8: pasa el foco a la otra vista, si está en uso
    void switchToOtherView();
    // Divide en horizontal o en vertical
    void toggleSplitOrientation();
    void setActivePane(EditorPane* pane);

    // --- Sesión (session.xml) ------------------------------------------------------------
    QString sessionPath() const;
    // getCurrentOpenedFiles: lo que hay abierto ahora
    Session captureSession();
    // loadSession: abre los archivos de la sesión (los que falten se omiten)
    void restoreSession(const Session& session, QStringList* missing = nullptr);
    // Lee session.xml (o su copia de seguridad si está dañado) y lo restaura
    bool restoreLastSession();
    bool saveSession();
    // -nosession: ni se lee ni se guarda la sesión
    void setSessionEnabled(bool enabled);
    void restoreWindowGeometry();

    // --- Preferencias (config.xml) ----------------------------------------------------
    const Preferences& preferences() const { return _prefs; }
    // Aplica y guarda en config.xml
    void setPreferences(const Preferences& prefs);
    // Configuración > Preferencias…
    void showPreferences();
    // Configuración > Configurador de estilos…
    void showStyleConfigurator();
    // Configuración > Shortcut Mapper… (ShortcutMapper de N++)
    void showShortcutMapper();
    // Todas las órdenes con sus atajos (menú, macros, plugins y Scintilla)
    std::vector<ShortcutEntry> shortcutEntries() const;
    // Cambia los atajos de una orden, los aplica y los guarda en shortcuts.xml
    bool setShortcutKeys(ShortcutEntry::Kind kind, const QString& id, const std::vector<KeyCombo>& keys);

    // --- Archivos recientes (<History> de config.xml) ------------------------------------
    // El más reciente primero; como N++, se añaden al cerrarlos y salen al abrirlos
    const QStringList& recentFiles() const { return _recentFiles; }
    void clearRecentFiles();
    // Ctrl+Mayús+T: vuelve a abrir el último archivo cerrado
    BufferId reopenLastClosed();

    // --- Seguridad de los datos ------------------------------------------------------------
    // NppGUI::isSnapshotMode: copias periódicas y sesión con cambios sin guardar.
    // Requiere recordar la sesión y no haber arrancado con --nosession.
    bool isSnapshotMode() const;
    // Copia de los documentos con cambios nuevos (cada snapshotBackupTiming ms);
    // si cambia alguna ruta de copia también guarda la sesión
    void backupNow();
    // Notepad_plus::checkModifiedDocument: compara con el disco los documentos
    // vigilados (el visible o todos, según "Auto-detection") y pregunta si recargar
    // los que cambiaron fuera o mantener los que se borraron
    void checkModifiedDocuments();
    QFileSystemWatcher* fileWatcher() const { return _fileWatcher; }

    // --- Lenguaje y estilos ----------------------------------------------------------
    // Menú Lenguaje: fija el lenguaje del documento actual (ya no se redetecta)
    void setLanguage(LangType lang);
    // Lenguaje > Definido por el usuario > nombre
    void setUserLanguage(const QString& name);
    // Lenguaje > Definir tu lenguaje… (UserDefineDialog)
    void showUserLanguageDialog();
    // Importa un userDefineLang de N++; devuelve los lenguajes importados
    QStringList importUserLanguage(const QString& path);
    // Tras cambiar los lenguajes del usuario: redetecta y vuelve a colorear
    void userLanguagesChanged();
    // Vuelve a leer langs.xml y los estilos (Ver > Tema > Recargar)
    void reloadSyntax();
    // Ver > Tema: lo aplica y lo recuerda para el modo actual (claro u oscuro)
    bool setTheme(const QString& path);
    // Modo claro u oscuro (DarkMode de N++): paleta, iconos y el tema de ese modo
    void applyAppearance();
    // La barra de herramientas (los botones de N++, en su orden)
    QToolBar* toolBar() const { return _toolBar; }
    // Paneles de Ver (Lista de documentos, Mapa del documento, Lista de funciones,
    // Explorador de carpetas), como los de N++
    DocumentListPanel* documentListPanel() const { return _docListPanel; }
    DocumentMapPanel* documentMapPanel() const { return _docMapPanel; }
    FunctionListPanel* functionListPanel() const { return _functionListPanel; }
    FileBrowserPanel* folderPanel() const { return _folderPanel; }
    QDockWidget* panelDock(const QString& name) const;
    // Archivo > Abrir carpeta como espacio de trabajo: la añade y muestra el panel
    bool addFolderToWorkspace(const QString& folder);
    // Lo que pide otra instancia (o la línea de comandos): abrir archivos, ir a una
    // línea; con `raise`, traer la ventana al frente (NPPM_INTERNAL_RESTOREFROMMINIMIZED)
    void openFromOtherInstance(const InstanceRequest& request, bool raise);
    // Sin el menú de QMainWindow para ocultar barras: eso va en Preferencias
    QMenu* createPopupMenu() override { return nullptr; }

    // --- Buscar (FindReplaceDlg + Notepad_plus::findInFiles…) -----------------------
    FindReplaceDialog* findDialog();
    SearchResultsPanel* resultsPanel() const { return _results; }
    void showFindDialog(FindReplaceDialog::Tab tab);
    // Texto del último mensaje de estado de la búsqueda (el del diálogo)
    QString lastSearchStatus() const { return _lastSearchStatus; }

    FindResult findNextInCurrent(const SearchOptions& options);
    bool replaceInCurrent(const SearchOptions& options);
    int replaceAllInCurrent(const SearchOptions& options);
    int countInCurrent(const SearchOptions& options);
    int findAllInCurrent(const SearchOptions& options);
    int findAllInOpenDocuments(const SearchOptions& options);
    int replaceAllInOpenDocuments(const SearchOptions& options);
    void findInFiles(const QString& directory, const QString& filters, bool recursive, bool includeHidden,
                     const SearchOptions& options);
    void replaceInFiles(const QString& directory, const QString& filters, bool recursive, bool includeHidden,
                        const SearchOptions& options);
    bool isFileSearchRunning() const;
    void cancelFileSearch();
    // Ctrl+F3 / Ctrl+Mayús+F3: busca la selección o la palabra del cursor
    void findSelectedText(bool backward);
    // Vuelve a un resultado del panel (doble clic, F4)
    void openSearchResult(const QString& path, BufferId buffer, SciPos start, SciPos end);

    // --- Autocompletado (Editar > Autocompletado) ------------------------------------
    AutoCompletion* autoCompletion(const EditorPane* pane) const;

    // --- Macros (menú Macro) -----------------------------------------------------------
    bool isRecordingMacro() const { return _recordingMacro; }
    // Ctrl+Mayús+R: empieza o termina la grabación
    void toggleMacroRecording();
    const Macro& currentMacro() const { return _macro; }
    // Notepad_plus::macroPlayback: todos los pasos se deshacen de una vez
    void playMacro(const Macro& macro);
    // WM_MACRODLGRUNMACRO: `times` veces o, si es negativo, hasta el final del archivo
    void runMacro(const Macro& macro, int times);
    // Guarda la macro grabada en shortcuts.xml con un nombre y un atajo
    bool saveCurrentMacro();
    const std::vector<SavedMacro>& savedMacros() const { return _savedMacros; }
    bool deleteSavedMacro(int index);
    QString shortcutsPath() const;

    // --- Plugins (menú Plugins) ----------------------------------------------------------
    PluginManager* plugins() const { return _plugins.get(); }
    // Plugins que no se pudieron cargar, con el motivo
    const QStringList& pluginWarnings() const { return _pluginWarnings; }
    // Carpetas donde se buscan: $EDITA_PLUGIN_PATH, ~/.config/edita/plugins y las de la aplicación
    QStringList pluginDirectories() const;
    // Plugins > Administrador de plugins (PluginsAdminDlg): lo que necesita el diálogo
    PluginsAdminContext pluginsAdminContext() const;
    void showPluginsAdmin();
    // Cerrar y volver a abrir Edita (tras instalar o quitar plugins, como N++)
    bool restartApplication();
    // NPPN_READY: al terminar de arrancar (tras restaurar la sesión)
    void notifyPluginsReady();
    // Lo que contesta la ventana a los mensajes NPPM_* de los plugins
    intptr_t handlePluginMessage(unsigned int message, uintptr_t wParam, intptr_t lParam);

    // --- Edición -----------------------------------------------------------------
    // IDM_EDIT_TRIMTRAILING: quita los espacios y tabulaciones del final de las líneas
    // (las seleccionadas o todas)
    void trimTrailingSpaces();
    // IDM_EDIT_SORTLINES_*: si una línea impide el orden numérico, lo dice (SortingError)
    void sortCurrentLines(SortKind kind, bool descending);
    // Editar > Comentar (IDM_EDIT_BLOCK_COMMENT, _SET, BLOCK_UNCOMMENT, STREAM_COMMENT, STREAM_UNCOMMENT)
    enum class CommentKind { Toggle, Comment, Uncomment, Stream, StreamUncomment };
    void commentCurrent(CommentKind kind);
    // Símbolos de comentario del lenguaje del documento (langs.xml o el lenguaje del usuario)
    CommentSymbols commentSymbols(BufferId id) const;
    void convertEol(EolType eol);
    // Línea y columna empezando en 1 (opciones -n y -c de la línea de comandos)
    void goToLine(int line, int column = 1);
    // Paleta de órdenes (">"), «Ir a archivo» ("") e «Ir a símbolo» ("@"); no existen en N++
    void showCommandPalette(const QString& prefix);
    CommandPalette* commandPalette() const { return _palette; }
    // Para las pruebas
    GitChanges* gitChangesOfCurrentView() const;
    ScrollBarMarks* scrollBarMarksOfCurrentView() const;
    ColorPreview* colorPreviewOfCurrentView() const;
    NotificationBar* notificationBar() const { return _notifications; }

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;

    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void createMenus();
    void createToolBar();
    void createPanels();
    void updatePanels();
    // Recuerda los atajos de serie y aplica los de shortcuts.xml (tras crear menús y plugins)
    void applyShortcuts();
    void applyScintillaKeys(int sciCommand, const std::vector<KeyCombo>& before, const std::vector<KeyCombo>& after);
    // El tema (ruta, "" = el de serie) que corresponde a un nombre de DarkMode
    QString themePathFor(const QString& name, bool dark) const;
    QMenu* createEncodingMenu();
    QMenu* createEolMenu();
    QMenu* createLanguageMenu();
    void populateThemeMenu(QMenu* menu);
    void populateUserLanguageMenu();
    // Filtros del diálogo de archivos por lenguaje (setFileOpenSaveDlgFilters)
    QString fileFilters() const;
    QString fileFilterFor(LangType lang) const;
    void afterSave(const QString& path);
    void createSearchMenu();
    void setSearchStatus(const QString& text, FindReplaceDialog::StatusKind kind);
    void selectFoundRange(SciPos start, SciPos end);
    QString scopeText(const SearchOptions& options, bool isEntire) const;
    QString textForFindField() const;
    // Ejecuta `work` con la vista auxiliar mostrando el documento del buffer
    void withSearchView(const Buffer& buffer, const std::function<void(ScintillaEditBase&)>& work);
    void startFileJob(const FileSearchRequest& request, int alreadyReplaced, int alreadyFiles);
    void showWarnings(const QString& title, const QStringList& warnings);
    void showTabContextMenu(BufferId id, const QPoint& globalPos);

    // Pone un documento en una vista (loadBufferIntoView): si la única pestaña
    // es un "nuevo" sin cambios, ocupa su lugar
    void showLoadedBuffer(BufferId id, EditorPane* pane = nullptr);
    // Quita el documento de las dos vistas y lo libera, sin preguntar
    void discardBuffer(BufferId id, bool keepBackup = false);
    // Quita el documento de una vista; lo libera si la otra no lo muestra
    void removeFromPane(BufferId id, EditorPane* pane);
    // Libera el documento (y lo pasa a "recientes")
    void releaseDocument(BufferId id, bool keepBackup);
    // Pregunta si guardar un documento modificado. false = cancelar
    bool maybeSave(BufferId id, bool offerAll, bool& saveRest, bool& discardRest);
    // `onlyPane`: cerrar solo en esa vista (los clones de la otra siguen abiertos)
    bool closeBuffers(const std::vector<BufferId>& ids, bool exiting, EditorPane* onlyPane = nullptr);
    void connectPane(EditorPane* pane);
    EditorPane* otherPane(const EditorPane* pane) const;
    EditorPane* paneContaining(BufferId id) const;
    // Documentos de las dos vistas, sin repetir (primero la principal)
    std::vector<BufferId> allBuffers() const;
    // showView/hideView: una vista sin pestañas se oculta (si la otra tiene)
    void updatePaneVisibility();
    void transferToOtherView(bool clone);
    void saveWindowState();
    // Avisa si el texto no cabe en la codificación del documento. false = cancelar
    bool confirmEncodable(BufferId id);
    // Antes de reinterpretar: guardar cambios y aceptar perder el deshacer
    bool prepareReinterpret(BufferId id);
    QString suggestedDirectory() const;
    // Copia de la versión anterior antes de guardar (Backup "action"). false = cancelar
    bool backupBeforeSave(const QString& path);
    // Vuelve a leer un archivo que cambió fuera conservando el cursor de cada vista
    bool reloadFromDisk(BufferId id);
    void handleFileState(BufferId id, DocumentManager::FileState state);
    // Documentos que se comprueban con la detección "solo el documento actual"
    std::vector<BufferId> visibleBuffers() const;
    void syncWatchedFiles();
    void scheduleFileCheck();
    void updateBackupTimer();
    // Aplica _prefs a las vistas, los estilos, los documentos nuevos y los menús
    void applyPreferences();
    void addRecentFile(const QString& path);
    void removeRecentFile(const QString& path);
    void rebuildRecentFilesMenu();
    QString recentFileLabel(const QString& path) const;
    void toggleViewOption(bool Preferences::* option, bool on);
    // Ejecuta una orden de menú grabable en macros (IDM_* de N++)
    void runMenuCommand(int idm, const std::function<void()>& command);
    QAction* actionForCommand(int idm) const;
    void createMacroMenu();
    void loadPlugins();
    void notifyPlugins(unsigned int code, BufferId id);
    void rebuildMacroMenu();
    void refreshMacroActions();
    void showRunMacroDialog();
    void playMacroSteps(const Macro& macro, std::vector<SciDocument>& undoDocs);
    void endMacroUndo(const std::vector<SciDocument>& undoDocs);
    void copyOrCut(bool cut);

    void refreshCurrentDocument();
    void refreshWindowTitle();
    void refreshMenuChecks();

    DocumentManager _documents;
    SyntaxManager _syntax;
    std::unique_ptr<Prompter> _prompter;
    QSplitter* _splitter = nullptr;
    EditorPane* _mainPane = nullptr;
    EditorPane* _subPane = nullptr;
    EditorPane* _activePane = nullptr;
    bool _sessionEnabled = true;
    Preferences _prefs;
    QFileSystemWatcher* _fileWatcher = nullptr;
    QTimer* _backupTimer = nullptr;
    bool _checkingFiles = false;
    bool _fileCheckPending = false;
    bool _lockRecentFiles = false;
    DocStatusBar* _statusBar = nullptr;
    QString _lastDirectory;

    FindReplaceDialog* _findDialog = nullptr;
    SearchResultsPanel* _results = nullptr;
    std::unique_ptr<ScintillaEditBase> _searchView; // _invisibleEditView de N++
    SciDocument _searchViewDefaultDoc = 0;
    SearchOptions _lastOptions;
    std::unique_ptr<FileSearchJob> _fileJob;
    QString _lastSearchStatus;

    QActionGroup* _encodingGroup = nullptr;
    std::map<int, QAction*> _codepageActions;     // codepage → acción
    std::map<int, QAction*> _unicodeModeActions;  // UniMode → acción
    QActionGroup* _eolGroup = nullptr;
    std::map<int, QAction*> _eolActions;          // EolType → acción
    QActionGroup* _languageGroup = nullptr;
    std::map<int, QAction*> _languageActions;     // LangType → acción
    QMenu* _userLanguageMenu = nullptr;
    QAction* _readOnlyAction = nullptr;
    QAction* _wrapAction = nullptr;
    std::vector<std::pair<bool Preferences::*, QAction*>> _viewOptionActions; // Ver > Mostrar símbolo
    QAction* _showAllAction = nullptr;
    QMenu* _fileMenu = nullptr;
    QAction* _recentAnchor = nullptr;       // separador delante de "Salir"
    std::vector<QAction*> _recentEntries;  // lo que hay que quitar al reconstruir
    QStringList _recentFiles;

    std::unique_ptr<PluginManager> _plugins;
    QStringList _pluginWarnings;
    QMenu* _pluginsMenu = nullptr;
    QToolBar* _toolBar = nullptr;
    std::unique_ptr<FunctionParsers> _functionParsers;
    CommandPalette* _palette = nullptr;
    // Cambios de Git en el margen y marcas en la barra de desplazamiento, de cada vista
    QPointer<GitChanges> _git[2];
    QPointer<ScrollBarMarks> _scrollMarks[2];
    QPointer<ColorPreview> _colorPreview[2];
    NotificationBar* _notifications = nullptr;
    GitChanges* gitChanges(const EditorPane* pane) const;
    void updateGitDocument(EditorPane* pane);
    void goToGitChange(bool forward);
    QThread* _paletteScan = nullptr;
    std::shared_ptr<std::atomic_bool> _paletteScanCancel;
    QSet<QString> _paletteFiles; // las rutas que ya están en «Ir a archivo»
    void fillPalette(CommandPalette::Mode mode);
    std::vector<CommandPalette::Item> paletteCommands() const;
    std::vector<CommandPalette::Item> paletteSymbols() const;
    void startPaletteFileScan();
    void stopPaletteFileScan();
    DocumentListPanel* _docListPanel = nullptr;
    DocumentMapPanel* _docMapPanel = nullptr;
    FunctionListPanel* _functionListPanel = nullptr;
    FileBrowserPanel* _folderPanel = nullptr;
    std::vector<QPointer<QDockWidget>> _panelDocks; // al cerrar se destruyen antes que la ventana
    ShortcutsConfig _shortcutConfig;
    // Paneles de los plugins (NPPM_DMMREGASDCKDLG): se destruyen antes de descargar los plugins
    struct PluginDock
    {
        QDockWidget* dock = nullptr;
        QWidget* client = nullptr;
        QString module;
        QString name;
    };
    std::vector<PluginDock> _pluginDocks;
    PluginDock* pluginDock(const QWidget* client);
    intptr_t registerPluginDock(const EditaDockData& dockData);
    // Recursos que reparte NPPM_ALLOCATE* (rangos de N++: ID_PLUGINS_CMD_DYNAMIC, MARKER_PLUGINS, INDICATOR_PLUGINS)
    int _nextDynamicCommand = 23000;
    int _nextMarker = 1;
    int _nextIndicator = 9;

    std::unique_ptr<AutoCompletion> _mainAuto;
    std::unique_ptr<AutoCompletion> _subAuto;

    bool _recordingMacro = false;
    bool _playingMacro = false;
    bool _lastStepWasCr = false;
    int _suppressRecording = 0; // > 0: lo que llega de Scintilla no se graba
    Macro _macro;
    std::vector<SavedMacro> _savedMacros;
    QMenu* _macroMenu = nullptr;
    QAction* _recordAction = nullptr;
    QAction* _playAction = nullptr;
    QAction* _saveMacroAction = nullptr;
    QAction* _runMacroAction = nullptr;
    QAction* _savedMacrosAnchor = nullptr;
    std::vector<QAction*> _savedMacroActions;
};

} // namespace edita
