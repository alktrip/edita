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

#include "core/LangType.h"
#include "core/Session.h"
#include "doc/Buffer.h"
#include "ui/DocTabBar.h"

#include <ScintillaEditBase.h>

#include <QPointer>
#include <QWidget>

#include <functional>
#include <vector>

class QFileSystemWatcher;
class QLineEdit;
class QTimer;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace edita {

class FunctionParsers;

// Lista de documentos (VerticalFileSwitcher de N++): los abiertos en las dos vistas,
// con su estado; un clic lo activa y el clic central lo cierra
class DocumentListPanel : public QWidget
{
    Q_OBJECT

public:
    struct Document
    {
        BufferId id = 0;
        QString name;
        QString path;
        DocTabBar::TabState state = DocTabBar::TabState::Saved;
        int view = 0;
    };

    explicit DocumentListPanel(QWidget* parent = nullptr);
    void setDocuments(const std::vector<Document>& documents, BufferId current, int currentView);
    QTreeWidget* list() const { return _list; }

signals:
    void activated(edita::BufferId id, int view);
    void closeRequested(edita::BufferId id, int view);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QTreeWidget* _list = nullptr;
};

// Mapa del documento (DocumentMap de N++): el documento en miniatura (otra vista de
// Scintilla con el mismo documento y zoom -10) con la zona visible marcada; un clic
// o arrastrar lleva el editor a esa parte
class DocumentMapPanel : public QWidget
{
    Q_OBJECT

public:
    using Styler = std::function<void(ScintillaEditBase& view)>;

    explicit DocumentMapPanel(QWidget* parent = nullptr);
    // Sigue a este editor (su documento, sus estilos y su desplazamiento)
    void setEditor(ScintillaEditBase* editor, Styler styler);
    void restyle();
    void sync();

    ScintillaEditBase* map() const { return _map; }
    // La zona visible del editor, en coordenadas del mapa
    QRect viewZone() const;
    // Lo que hace un clic en el mapa a la altura `y`
    void scrollEditorTo(int y);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void configureMap();

    ScintillaEditBase* _map = nullptr;
    QWidget* _zone = nullptr;
    QPointer<ScintillaEditBase> _editor;
    Styler _styler;
    std::vector<QMetaObject::Connection> _connections;
    bool _dragging = false;
};

// Lista de funciones (FunctionListPanel de N++): clases y funciones del documento
// según functionList/*.xml; filtro, orden alfabético y recargar
class FunctionListPanel : public QWidget
{
    Q_OBJECT

public:
    explicit FunctionListPanel(FunctionParsers& parsers, QWidget* parent = nullptr);

    void setSource(ScintillaEditBase* editor, LangType lang, const QString& userLanguage, const QString& title);
    void reload();
    void setSorted(bool sorted);
    void setFilter(const QString& text);
    QTreeWidget* tree() const { return _tree; }

signals:
    void goToPosition(sptr_t position);

private:
    void applyFilter();

    FunctionParsers& _parsers;
    ScintillaEditBase* _scanner = nullptr; // vista oculta con el mismo documento
    QPointer<ScintillaEditBase> _editor;
    QMetaObject::Connection _modified;
    LangType _lang = LangType::Text;
    QString _userLanguage;
    QString _title;
    QTreeWidget* _tree = nullptr;
    QLineEdit* _filter = nullptr;
    QToolButton* _sort = nullptr;
    QTimer* _refresh = nullptr;
};

// Explorador de carpetas (FileBrowser de N++, "Folder as Workspace"): una o varias
// carpetas raíz; doble clic abre el archivo
class FileBrowserPanel : public QWidget
{
    Q_OBJECT

public:
    explicit FileBrowserPanel(QWidget* parent = nullptr);

    bool addRoot(const QString& folder);
    void removeRoot(const QString& folder);
    QStringList roots() const;
    // session.xml: carpetas, las desplegadas y la seleccionada
    void setState(const std::vector<Session::FolderRoot>& roots, const QString& selected);
    std::vector<Session::FolderRoot> state() const;
    QString selectedPath() const;
    // Selecciona el archivo (desplegando sus carpetas) si está en alguna raíz
    bool locate(const QString& filePath);
    QTreeWidget* tree() const { return _tree; }
    // Al cambiar la preferencia de los iconos por tipo de archivo
    void refreshIcons();

signals:
    void openFile(const QString& path);
    void findInFolder(const QString& folder);
    void openTerminal(const QString& folder);
    void addFolderRequested();
    void locateRequested();

private:
    QTreeWidgetItem* makeItem(const QString& path, bool isDir);
    void populate(QTreeWidgetItem* item);
    void showMenu(const QPoint& pos);
    QTreeWidgetItem* rootFor(const QString& path) const;

    QTreeWidget* _tree = nullptr;
    QFileSystemWatcher* _watcher = nullptr;
};

} // namespace edita
