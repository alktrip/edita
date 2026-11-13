// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ayuda › Acerca de Edita (fase 91). Como el AboutDlg de Notepad++ (versión, arquitectura,
// compilación y licencia), más los componentes de terceros con sus licencias, si hay una
// versión nueva, enlaces útiles, los plugins cargados y copiar la información de la versión.

#pragma once

#include <QDialog>
#include <QStringList>
#include <QUrl>

#include <functional>

class QLabel;

namespace edita {

class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    struct Context
    {
        QString versionLine; // «Edita 1.0.0-rc.3 (x86_64, AppImage)»
        QString build;       // «a5777d0, 2026-11-13» (vacío: sin git)
        QString system;      // el sistema, Qt y la plataforma
        QStringList plugins; // «Hex Editor (1.0.0)»
        QUrl homepage;
        QUrl releaseNotes;
        std::function<void(const QUrl&)> openUrl;
        std::function<void()> openManual;
        std::function<void()> reportProblem;
        std::function<void()> openPluginsAdmin;
    };

    enum class UpdateState { Disabled, Checking, UpToDate, Available, Failed };

    AboutDialog(Context context, QWidget* parent);

    void setUpdateState(UpdateState state, const QString& version = QString(), const QUrl& page = QUrl(),
                        const QString& error = QString());
    QString copyText() const;

private:
    QWidget* aboutPage();
    QWidget* componentsPage();
    QWidget* licensePage();

    Context _context;
    QLabel* _update = nullptr;
    QUrl _downloadPage;
};

} // namespace edita
