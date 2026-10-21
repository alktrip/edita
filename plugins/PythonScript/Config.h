// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

namespace pythonscript {

// PythonScriptStartup.cnf, con el formato de ConfigFile.cpp de PythonScript:
//   ITEM/<script>             → entrada del menú del plugin
//   SETTING/<nombre>/<valor>  → STARTUP (LAZY | ATSTARTUP), OPENCONSOLEONERROR…
// Las rutas de los scripts del usuario se guardan relativas a su carpeta.
class Config
{
public:
    Config(QString path, QString machineScripts, QString userScripts);

    bool load();
    bool save() const;

    const QString& path() const { return _path; }
    const QString& machineScriptsDir() const { return _machineScripts; }
    const QString& userScriptsDir() const { return _userScripts; }

    QString setting(const QString& name) const { return _settings.value(name); }
    void setSetting(const QString& name, const QString& value) { _settings[name] = value; }
    bool startsAtStartup() const { return setting(QStringLiteral("STARTUP")) == QLatin1String("ATSTARTUP"); }

    // Scripts del menú (rutas absolutas)
    const QStringList& menuItems() const { return _menuItems; }
    void setMenuItems(const QStringList& items) { _menuItems = items; }

private:
    QString expand(const QString& stored) const;
    QString shorten(const QString& path) const;

    QString _path;
    QString _machineScripts;
    QString _userScripts;
    QMap<QString, QString> _settings;
    QStringList _menuItems;
};

} // namespace pythonscript
