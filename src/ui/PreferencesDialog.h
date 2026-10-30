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

#include "core/Preferences.h"

#include <QDialog>

#include <utility>
#include <vector>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QSpinBox;
class QStackedWidget;
class QTableWidget;

namespace edita {

// Diálogo de preferencias (PreferenceDlg de N++): una lista de categorías a la
// izquierda y su página a la derecha. Solo edita una copia de las preferencias;
// la ventana principal las aplica y las guarda al aceptar o al pulsar "Aplicar".
class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    // `languages`: (LangType, nombre) para el lenguaje de los documentos nuevos
    PreferencesDialog(const Preferences& prefs, std::vector<std::pair<int, QString>> languages,
                      QWidget* parent = nullptr);

    Preferences preferences() const;
    void setPreferences(const Preferences& prefs);

    // Muestra la página con ese nombre de objeto ("general", "backup"…)
    void showPage(const QString& name);
    QStringList pageNames() const;

signals:
    void applyRequested(const edita::Preferences& prefs);

private:
    QWidget* addPage(const QString& name, const QString& title);
    void buildGeneral();
    void buildEditing();
    void buildMargins();
    void buildNewDocument();
    void buildRecentFiles();
    void buildIndentation();
    void buildAutoCompletion();
    void buildHighlighting();
    void buildSearching();
    void buildPrinting();
    void buildBackup();
    void buildMisc();
    void buildLanguageServers();
    void updateEnabledState();

    QListWidget* _categories;
    QStackedWidget* _pages;
    std::vector<std::pair<int, QString>> _languages;
    Preferences _base; // lo que el diálogo no edita se conserva tal cual

    // General
    QCheckBox* _statusBar = nullptr;
    QCheckBox* _tabBarHide = nullptr;
    QCheckBox* _tabCloseButton = nullptr;
    QCheckBox* _tabDoubleClick = nullptr;
    QCheckBox* _quitOnEmpty = nullptr;
    QComboBox* _language = nullptr;
    QComboBox* _appearance = nullptr;
    QComboBox* _multiInst = nullptr;
    QComboBox* _toolbar = nullptr;
    QCheckBox* _systemMonospace = nullptr;
    QCheckBox* _qtFileDialog = nullptr;
    // Edición
    QComboBox* _currentLine = nullptr;
    QSpinBox* _frameWidth = nullptr;
    QComboBox* _caretWidth = nullptr;
    QSpinBox* _blinkRate = nullptr;
    QCheckBox* _multiSelection = nullptr;
    QCheckBox* _virtualSpace = nullptr;
    QCheckBox* _scrollBeyond = nullptr;
    QCheckBox* _lineCopyCut = nullptr;
    QCheckBox* _smoothFont = nullptr;
    QCheckBox* _whiteSpace = nullptr;
    QCheckBox* _eol = nullptr;
    QCheckBox* _indentGuides = nullptr;
    QCheckBox* _wrap = nullptr;
    QComboBox* _wrapIndent = nullptr;
    QCheckBox* _wrapSymbol = nullptr;
    // Márgenes
    QCheckBox* _lineNumbers = nullptr;
    QCheckBox* _dynamicWidth = nullptr;
    QComboBox* _folderStyle = nullptr;
    QLineEdit* _edgeColumns = nullptr;
    QCheckBox* _edgeBackground = nullptr;
    QCheckBox* _bookmarkMargin = nullptr;
    QCheckBox* _gitMargin = nullptr;
    QCheckBox* _gitExplorer = nullptr;
    QCheckBox* _snippetsTab = nullptr;
    QCheckBox* _gitBlameInline = nullptr;
    QCheckBox* _currentLineNumber = nullptr;
    QCheckBox* _activeIndentGuide = nullptr;
    QCheckBox* _colorPreview = nullptr;
    QCheckBox* _fileTypeIcons = nullptr;
    QCheckBox* _showWelcome = nullptr;
    QCheckBox* _rainbowBrackets = nullptr;
    QCheckBox* _breadcrumbs = nullptr;
    QCheckBox* _stickyScroll = nullptr;
    QCheckBox* _inlineFind = nullptr;
    QLineEdit* _terminalShell = nullptr;
    QCheckBox* _lspEnabled = nullptr;
    QTableWidget* _lspServers = nullptr;
    QCheckBox* _scrollBarMarks = nullptr;
    QComboBox* _changeHistory = nullptr;
    // Documento nuevo y directorio
    QComboBox* _newEol = nullptr;
    QComboBox* _newEncoding = nullptr;
    QComboBox* _newCodepage = nullptr;
    QComboBox* _newLanguage = nullptr;
    QComboBox* _openSaveDir = nullptr;
    QLineEdit* _defaultDir = nullptr;
    // Archivos recientes
    QSpinBox* _recentMax = nullptr;
    QCheckBox* _recentSubMenu = nullptr;
    QComboBox* _recentLengthMode = nullptr;
    QSpinBox* _recentLength = nullptr;
    // Tabulación
    QSpinBox* _tabSize = nullptr;
    QCheckBox* _tabSpaces = nullptr;
    QCheckBox* _backspaceUnindent = nullptr;
    QComboBox* _autoIndent = nullptr;
    // Autocompletado
    QComboBox* _autocStatus = nullptr;
    QSpinBox* _autocFromLen = nullptr;
    QCheckBox* _autocIgnoreNumbers = nullptr;
    QCheckBox* _autocBrief = nullptr;
    QCheckBox* _funcParams = nullptr;
    QCheckBox* _insertParentheses = nullptr;
    QCheckBox* _insertBrackets = nullptr;
    QCheckBox* _insertCurly = nullptr;
    QCheckBox* _insertQuotes = nullptr;
    QCheckBox* _insertDoubleQuotes = nullptr;
    QCheckBox* _insertTag = nullptr;
    // Resaltado
    QCheckBox* _smart = nullptr;
    QCheckBox* _smartCase = nullptr;
    QCheckBox* _smartWord = nullptr;
    QCheckBox* _smartFind = nullptr;
    QCheckBox* _smartOther = nullptr;
    QCheckBox* _tagsMatch = nullptr;
    QCheckBox* _tagAttributes = nullptr;
    QCheckBox* _markAllCase = nullptr;
    QCheckBox* _markAllWord = nullptr;
    // Búsqueda
    QCheckBox* _fillFind = nullptr;
    QCheckBox* _confirmReplaceAll = nullptr;
    QCheckBox* _monoFind = nullptr;
    // Impresión
    QCheckBox* _printLineNumber = nullptr;
    QCheckBox* _printFormFeed = nullptr;
    QComboBox* _printColour = nullptr;
    QSpinBox* _printMargins[4] = {}; // izquierda, arriba, derecha, abajo
    QLineEdit* _printHeader[3] = {}; // izquierda, centro, derecha
    QLineEdit* _printFooter[3] = {};
    QComboBox* _printHeaderFont = nullptr;
    QComboBox* _printFooterFont = nullptr;
    QSpinBox* _printHeaderSize = nullptr;
    QSpinBox* _printFooterSize = nullptr;
    QComboBox* _printHeaderStyle = nullptr;
    QComboBox* _printFooterStyle = nullptr;
    // Copias de seguridad
    QCheckBox* _remember = nullptr;
    QCheckBox* _snapshot = nullptr;
    QSpinBox* _snapshotSeconds = nullptr;
    QComboBox* _backupOnSave = nullptr;
    QCheckBox* _customBackupDir = nullptr;
    QLineEdit* _backupDir = nullptr;
    // Varios
    QCheckBox* _detect = nullptr;
    QCheckBox* _detectAll = nullptr;
    QCheckBox* _autoUpdate = nullptr;
    QCheckBox* _goToEnd = nullptr;
    QCheckBox* _detectEncoding = nullptr;
    QComboBox* _fallbackCodepage = nullptr;
};

} // namespace edita
