// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Página de inicio (no existe en Notepad++, que abre siempre un «new 1» vacío): se ve
// en lugar del editor mientras el único documento es uno nuevo y vacío. Tiene
// accesos para empezar (nuevo, abrir, abrir carpeta, paleta de órdenes), los archivos y
// carpetas recientes, la apariencia (claro, oscuro o el del sistema, y el tema) y los
// atajos más útiles. Escribir empieza a escribir en el documento nuevo.

#pragma once

#include <QKeySequence>
#include <QStringList>
#include <QWidget>

#include <vector>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QVBoxLayout;

namespace edita {

class WelcomePage : public QWidget
{
    Q_OBJECT

public:
    enum class Mode { Light = 0, Dark = 1, System = 2 };

    struct Shortcut
    {
        QString text;
        QKeySequence keys;
    };

    explicit WelcomePage(QWidget* parent = nullptr);

    void setVersion(const QString& version);
    void setRecentFiles(const QStringList& paths);
    void setRecentFolders(const QStringList& paths);
    void setThemes(const QStringList& names, int current);
    void setMode(Mode mode);
    void setShortcuts(const std::vector<Shortcut>& shortcuts);
    // Los atajos de las tarjetas (los del Shortcut Mapper)
    void setActionShortcuts(const QKeySequence& newFile, const QKeySequence& open, const QKeySequence& openFolder,
                            const QKeySequence& palette);
    void setShowOnStartup(bool show);
    // Al cambiar entre claro y oscuro: con hoja de estilo, la página no recibe PaletteChange
    void refreshStyle();

    // Para las pruebas
    QStringList recentFileTexts() const;
    QStringList recentFolderTexts() const;
    bool clickCard(const QString& objectName);
    bool clickRecent(const QString& path);

signals:
    void newFileRequested();
    void openFileRequested();
    void openFolderRequested();
    void commandPaletteRequested();
    void recentFileRequested(const QString& path);
    void recentFolderRequested(const QString& path);
    void themeRequested(int index);
    void modeRequested(edita::WelcomePage::Mode mode);
    void showOnStartupChanged(bool show);
    // Se empezó a escribir sobre la página: el texto va al documento
    void typingStarted(const QString& text);
    void dismissRequested();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void applyStyle();
    void fillRecent(QVBoxLayout* layout, const QStringList& paths, bool folders);

    QWidget* _content = nullptr;
    QLabel* _version = nullptr;
    QVBoxLayout* _recentFiles = nullptr;
    QVBoxLayout* _recentFolders = nullptr;
    QWidget* _foldersSection = nullptr;
    QVBoxLayout* _shortcuts = nullptr;
    QComboBox* _themes = nullptr;
    QButtonGroup* _modes = nullptr;
    QCheckBox* _showOnStartup = nullptr;
    QStringList _filePaths;
    QStringList _folderPaths;
    QLabel* _cardKeys[4] = {};
    QKeySequence _cardSequences[4];
    std::vector<std::pair<QLabel*, QKeySequence>> _shortcutLabels;
    QString _capStyle; // el estilo de las teclas («Ctrl», «E»), según el tema
    void renderKeys(QLabel* label, const QKeySequence& keys) const;
    bool _updating = false;
    QString _styleKey; // el modo y los colores con los que se aplicó el estilo
};

} // namespace edita
