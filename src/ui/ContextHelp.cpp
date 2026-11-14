// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ContextHelp.h"

#include <QWidget>

namespace edita::context_help {

const std::vector<std::pair<const char*, const char*>>& table()
{
    static const std::vector<std::pair<const char*, const char*>> entries = {
        // Diálogos
        {"AboutDialog", "actionAbout"},
        {"ColumnEditorDialog", "actionColumnEditor"},
        {"CommandPalette", "actionCommandPalette"},
        {"FindBar", "actionFind"},
        {"FindCharsDialog", "actionFindCharsInRange"},
        {"FindReplaceDialog", "actionFind"},
        {"HashDialog", "actionMd5Text"},
        {"PluginsAdminDialog", "actionPluginsAdmin"},
        {"PreferencesDialog", "actionPreferences"},
        {"RunDialog", "actionRun"},
        {"ShortcutMapperDialog", "actionShortcutMapper"},
        {"StyleConfiguratorDialog", "actionStyleConfigurator"},
        {"UserLanguageDialog", "actionDefineLanguage"},
        {"WindowsDialog", "actionWindowsList"},
        // Paneles
        {"CharacterPanel", "actionCharPanel"},
        {"ClipboardHistoryPanel", "actionClipboardHistory"},
        {"DocumentListPanel", "actionDocumentList"},
        {"DocumentMapPanel", "actionDocumentMap"},
        {"FileBrowserPanel", "actionFolderAsWorkspace"},
        {"FunctionListPanel", "actionFunctionList"},
        {"GitCommitPanel", "actionGitCommit"},
        {"LocalHistoryPanel", "actionLocalHistory"},
        {"ProblemsPanel", "actionProblems"},
        {"RegexTesterPanel", "actionRegexTester"},
        {"SearchResultsPanel", "actionFindInFiles"},
        {"TaskOutputPanel", "actionTaskOutput"},
        {"TerminalPanel", "actionTerminal"},
        {"TodoPanel", "actionTodo"},
        {"WelcomePage", "actionWelcomePage"},
    };
    return entries;
}

QString commandFor(const QWidget* widget)
{
    for (const QWidget* w = widget; w; w = w->parentWidget()) {
        // Los paneles de los plugins: el capítulo de los plugins
        if (w->objectName().startsWith(QLatin1String("pluginDock_")))
            return QStringLiteral("actionPluginsAdmin");
        QString name = QString::fromLatin1(w->metaObject()->className());
        name = name.mid(name.lastIndexOf(QLatin1Char(':')) + 1); // sin «edita::»
        for (const auto& [context, command] : table()) {
            if (name == QLatin1String(context) || w->objectName() == QLatin1String(context))
                return QString::fromLatin1(command);
        }
    }
    return QString();
}

} // namespace edita::context_help
