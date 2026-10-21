// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

namespace pythonscript {

Config::Config(QString path, QString machineScripts, QString userScripts)
    : _path(std::move(path))
    , _machineScripts(std::move(machineScripts))
    , _userScripts(std::move(userScripts))
{
    // ConfigFile::initSettings: los valores por defecto de PythonScript
    _settings[QStringLiteral("ADDEXTRALINETOOUTPUT")] = QStringLiteral("0");
    _settings[QStringLiteral("COLORIZEOUTPUT")] = QStringLiteral("-1");
    _settings[QStringLiteral("OPENCONSOLEONERROR")] = QStringLiteral("1");
    _settings[QStringLiteral("DISABLEPOPUPWARNING")] = QStringLiteral("0");
    _settings[QStringLiteral("PREFERINSTALLEDPYTHON")] = QStringLiteral("0");
    _settings[QStringLiteral("STARTUP")] = QStringLiteral("LAZY");
    _settings[QStringLiteral("CUSTOMCONSOLEERRORCOLOR")] = QStringLiteral("-1");
}

bool Config::load()
{
    QFile file(_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    _menuItems.clear();
    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.startsWith(QLatin1String("ITEM/"))) {
            _menuItems << expand(line.mid(5));
        } else if (line.startsWith(QLatin1String("SETTING/"))) {
            const QString rest = line.mid(8);
            const qsizetype slash = rest.indexOf(QLatin1Char('/'));
            if (slash > 0)
                _settings[rest.left(slash)] = rest.mid(slash + 1);
        }
        // TOOLBAR/… (iconos de la barra de herramientas) no existe en Edita y se ignora
    }
    return true;
}

bool Config::save() const
{
    QDir().mkpath(QFileInfo(_path).absolutePath());
    QSaveFile file(_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream out(&file);
    for (const QString& item : _menuItems)
        out << "ITEM/" << shorten(item) << "\n";
    for (auto it = _settings.cbegin(); it != _settings.cend(); ++it)
        out << "SETTING/" << it.key() << "/" << it.value() << "\n";
    out.flush();
    return file.commit();
}

QString Config::expand(const QString& stored) const
{
    // ConfigFile::expandPathIfNeeded: lo que no es absoluto es relativo a los
    // scripts del usuario (las barras invertidas de un .cnf de Windows también valen)
    QString path = stored;
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (QDir::isAbsolutePath(path) && QFileInfo::exists(path))
        return path;
    QString relative = path;
    while (relative.startsWith(QLatin1Char('/')))
        relative.remove(0, 1);
    const QString inUserDir = QDir(_userScripts).filePath(relative);
    return QDir::isAbsolutePath(path) && !QFileInfo::exists(inUserDir) ? path : inUserDir;
}

QString Config::shorten(const QString& path) const
{
    // ConfigFile::shortenPathIfPossible: relativa a los scripts del usuario
    const QString user = QDir(_userScripts).absolutePath() + QLatin1Char('/');
    return path.startsWith(user) ? path.mid(user.size()) : path;
}

} // namespace pythonscript
