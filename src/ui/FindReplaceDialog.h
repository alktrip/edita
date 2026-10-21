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

#include "search/SearchOptions.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QTabBar;
class QWidget;

namespace edita {

// Diálogo de búsqueda no modal, con las pestañas Buscar / Reemplazar / Buscar en
// archivos que comparten campos y opciones, como FindReplaceDlg de N++. Solo
// recoge lo que pide el usuario; la ventana principal hace el trabajo.
class FindReplaceDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Tab { Find = 0, Replace = 1, FindInFiles = 2 };
    // Colores de los mensajes de estado ("Find status" de stylers.xml)
    enum class StatusKind { Message, NotFound, EndReached };

    explicit FindReplaceDialog(QWidget* parent = nullptr);

    // Muestra la pestaña; si `text` no está vacío lo pone como texto a buscar
    void showTab(Tab tab, const QString& text = {});
    Tab currentTab() const;

    SearchOptions options() const;
    void setOptions(const SearchOptions& options);
    QString directory() const;
    void setDirectory(const QString& dir);
    QString filters() const;
    void setFilters(const QString& filters);
    bool recursive() const;
    bool includeHidden() const;
    bool followCurrentDocument() const;

    void setStatus(const QString& text, StatusKind kind = StatusKind::Message);
    QString statusText() const;
    // Mientras se busca en archivos: botones desactivados y "Cancelar" visible
    void setFileSearchRunning(bool running);

    // Guarda los textos actuales en el historial de las listas
    void rememberCurrentTexts();
    QStringList findHistory() const;
    QStringList replaceHistory() const;
    QStringList filterHistory() const;
    QStringList directoryHistory() const;
    void setFindInFilesOptions(bool recursive, bool hidden, bool followCurrentDocument);
    // Searching "monospacedFontFindDlg": fuente monoespaciada en los campos de texto
    void setMonospacedFont(bool on);
    void setHistories(const QStringList& find, const QStringList& replace, const QStringList& filters,
                      const QStringList& directories);

signals:
    void findNextRequested(bool backward);
    void countRequested();
    void findAllInCurrentRequested();
    void findAllInOpenRequested();
    void replaceRequested();
    void replaceAllRequested();
    void replaceAllInOpenRequested();
    void findInFilesRequested();
    void replaceInFilesRequested();
    void cancelFileSearchRequested();

private:
    void updateForTab();
    void updateModeDependentOptions();
    QPushButton* makeButton(const QString& text, const char* name);

    QTabBar* _tabs;
    QComboBox* _find;
    QComboBox* _replace;
    QComboBox* _filters;
    QComboBox* _directory;
    QLabel* _replaceLabel;
    QLabel* _filtersLabel;
    QLabel* _directoryLabel;
    QPushButton* _browse;
    QCheckBox* _wholeWord;
    QCheckBox* _matchCase;
    QCheckBox* _wrapAround;
    QCheckBox* _backward;
    QCheckBox* _inSelection;
    QCheckBox* _recursive;
    QCheckBox* _hidden;
    QCheckBox* _followDoc;
    QRadioButton* _normal;
    QRadioButton* _extended;
    QRadioButton* _regex;
    QCheckBox* _dotNewline;
    QLabel* _status;

    QPushButton* _findNext;
    QPushButton* _count;
    QPushButton* _findAllCurrent;
    QPushButton* _findAllOpen;
    QPushButton* _replaceOne;
    QPushButton* _replaceAll;
    QPushButton* _replaceAllOpen;
    QPushButton* _findInFiles;
    QPushButton* _replaceInFiles;
    QPushButton* _cancel;
    bool _running = false;
};

} // namespace edita
