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


#include "core/Preferences.h"

#include "core/ConfigXml.h"

#include <QFileInfo>
#include <QStringList>

#include <algorithm>

namespace edita {

namespace {

QString yesNo(bool value)
{
    return value ? QStringLiteral("yes") : QStringLiteral("no");
}

// Mismos textos que NppParameters (Parameters.cpp, "Auto-detection")
struct DetectionName
{
    const char* text;
    int value;
};

constexpr DetectionName kDetectionNames[] = {
    {"yes", FileAutoDetection::CurrentFile},
    {"auto", FileAutoDetection::CurrentFile | FileAutoDetection::AutoUpdate},
    {"Update2End", FileAutoDetection::CurrentFile | FileAutoDetection::GoToEnd},
    {"autoUpdate2End", FileAutoDetection::CurrentFile | FileAutoDetection::AutoUpdate | FileAutoDetection::GoToEnd},
    {"yesOld", FileAutoDetection::AllFiles},
    {"autoOld", FileAutoDetection::AllFiles | FileAutoDetection::AutoUpdate},
    {"Update2EndOld", FileAutoDetection::AllFiles | FileAutoDetection::GoToEnd},
    {"autoUpdate2EndOld", FileAutoDetection::AllFiles | FileAutoDetection::AutoUpdate | FileAutoDetection::GoToEnd},
};

} // namespace

int fileAutoDetectionFromText(const QString& text)
{
    for (const DetectionName& name : kDetectionNames) {
        if (text == QLatin1String(name.text))
            return name.value;
    }
    return FileAutoDetection::Disabled; // "no" o desconocido, como N++
}

QString fileAutoDetectionToText(int value)
{
    for (const DetectionName& name : kDetectionNames) {
        if (value == name.value)
            return QString::fromLatin1(name.text);
    }
    return QStringLiteral("no");
}

namespace {

// Lectura y escritura de <GUIConfig> con los valores de N++ ("yes"/"no", "show"/"hide")
class GuiReader
{
public:
    explicit GuiReader(const ConfigXml& config) : _config(config) {}

    void boolText(const char* name, bool& value) const
    {
        const QString text = _config.guiConfigText(QString::fromLatin1(name));
        if (!text.isEmpty())
            value = text == QLatin1String("yes") || text == QLatin1String("show");
    }
    void boolAttr(const char* name, const char* attr, bool& value) const
    {
        const QString text = get(name, attr);
        if (!text.isEmpty())
            value = text == QLatin1String("yes") || text == QLatin1String("show");
    }
    void intAttr(const char* name, const char* attr, int& value, int min, int max) const
    {
        bool ok = false;
        const int v = get(name, attr).toInt(&ok);
        if (ok && v >= min && v <= max)
            value = v;
    }
    template<typename Enum>
    void enumAttr(const char* name, const char* attr, Enum& value, int min, int max) const
    {
        int v = static_cast<int>(value);
        intAttr(name, attr, v, min, max);
        value = static_cast<Enum>(v);
    }
    QString get(const char* name, const char* attr) const
    {
        return _config.guiConfig(QString::fromLatin1(name), QString::fromLatin1(attr));
    }
    QString text(const char* name) const { return _config.guiConfigText(QString::fromLatin1(name)); }

private:
    const ConfigXml& _config;
};

class GuiWriter
{
public:
    explicit GuiWriter(ConfigXml& config) : _config(config) {}

    void text(const char* name, const QString& value) { _config.setGuiConfigText(QString::fromLatin1(name), value); }
    void attr(const char* name, const char* attr, const QString& value)
    {
        _config.setGuiConfig(QString::fromLatin1(name), QString::fromLatin1(attr), value);
    }
    void attr(const char* name, const char* a, bool value) { attr(name, a, yesNo(value)); }
    void attr(const char* name, const char* a, int value) { attr(name, a, QString::number(value)); }
    void showHide(const char* name, const char* a, bool value)
    {
        attr(name, a, value ? QStringLiteral("show") : QStringLiteral("hide"));
    }

private:
    ConfigXml& _config;
};

constexpr const char* kFolderStyleNames[] = {"simple", "arrow", "circle", "box", "none"}; // FolderStyle 1..5
constexpr const char* kWrapIndentNames[] = {"default", "aligned", "indent"};
constexpr const char* kView = "ScintillaPrimaryView";

} // namespace

Preferences loadPreferences(const ConfigXml& config)
{
    Preferences p;
    const GuiReader r(config);

    // General
    r.boolText("StatusBar", p.statusBarShow);
    r.boolAttr("TabBar", "hide", p.tabBarHide);
    r.boolAttr("TabBar", "closeButton", p.tabCloseButton);
    r.boolAttr("TabBar", "doubleClick2Close", p.tabDoubleClickCloses);
    r.boolAttr("TabBar", "quitOnEmpty", p.quitOnEmpty);

    // Edición y márgenes
    r.enumAttr(kView, "currentLineIndicator", p.currentLineMode, 0, 2);
    r.intAttr(kView, "currentLineFrameWidth", p.currentLineFrameWidth, 1, 6);
    r.intAttr("Caret", "width", p.caretWidth, 0, 3);
    r.intAttr("Caret", "blinkRate", p.caretBlinkRate, 0, 10000);
    r.boolAttr(kView, "multiSelection", p.multiSelection);
    r.boolAttr(kView, "virtualSpace", p.virtualSpace);
    r.boolAttr(kView, "scrollBeyondLastLine", p.scrollBeyondLastLine);
    r.boolAttr(kView, "lineCopyCutWithoutSelection", p.lineCopyCutWithoutSelection);
    r.boolAttr(kView, "smoothFont", p.smoothFont);
    r.boolAttr(kView, "whiteSpaceShow", p.whiteSpaceShow);
    r.boolAttr(kView, "eolShow", p.eolShow);
    r.boolAttr(kView, "indentGuideLine", p.indentGuideLine);
    r.boolAttr(kView, "Wrap", p.wrap);
    r.boolAttr(kView, "wrapSymbolShow", p.wrapSymbolShow);
    const QString wrapMethod = r.get(kView, "lineWrapMethod");
    for (int i = 0; i < 3; ++i) {
        if (wrapMethod == QLatin1String(kWrapIndentNames[i]))
            p.wrapIndent = static_cast<WrapIndent>(i);
    }
    r.boolAttr(kView, "lineNumberMargin", p.lineNumberMargin);
    r.boolAttr(kView, "lineNumberDynamicWidth", p.lineNumberDynamicWidth);
    const QString folder = r.get(kView, "folderMarkStyle");
    for (int i = 0; i < 5; ++i) {
        if (folder == QLatin1String(kFolderStyleNames[i]))
            p.folderStyle = static_cast<FolderStyle>(i + 1);
    }
    const QString edge = r.get(kView, "edgeMultiColumnPos");
    if (!edge.isEmpty() || !r.get(kView, "isEdgeBgMode").isEmpty())
        p.edgeColumns = parseEdgeColumns(edge);
    r.boolAttr(kView, "isEdgeBgMode", p.edgeBackground);
    r.boolAttr(kView, "bookMarkMargin", p.bookmarkMargin);
    if (r.get(kView, "isChangeHistoryEnabled") == QLatin1String("yes"))
        p.changeHistory = 1; // formato antiguo
    else
        r.intAttr(kView, "isChangeHistoryEnabled", p.changeHistory, 0, 3);

    // Documento nuevo y directorio
    r.intAttr("NewDocDefaultSettings", "format", p.newDocEol, 0, 2);
    r.intAttr("NewDocDefaultSettings", "encoding", p.newDocUnicodeMode, 0, 7);
    r.intAttr("NewDocDefaultSettings", "codepage", p.newDocCodepage, -1, 100000);
    r.intAttr("NewDocDefaultSettings", "lang", p.newDocLanguage, 0, 200);
    r.enumAttr("openSaveDir", "value", p.openSaveDir, 0, 2);
    p.defaultDirPath = r.get("openSaveDir", "defaultDirPath");

    // Archivos recientes
    const RecentFilesConfig recent = config.recentFiles();
    p.recentFilesMax = recent.max;
    p.recentFilesInSubMenu = recent.inSubMenu;
    p.recentFilesCustomLength = recent.customLength;

    // Tabulación y sangría
    r.intAttr("TabSetting", "size", p.tabSize, 1, 64);
    r.boolAttr("TabSetting", "replaceBySpace", p.tabReplacedBySpace);
    r.boolAttr("TabSetting", "backspaceUnindent", p.backspaceUnindent);
    QString indent = r.text("MaintainIndent");
    if (indent.isEmpty())
        indent = r.text("MaitainIndent"); // errata que N++ sigue aceptando
    if (indent == QLatin1String("yes"))
        p.autoIndent = AutoIndent::Basic;
    else if (indent == QLatin1String("no"))
        p.autoIndent = AutoIndent::None;
    else if (bool ok = false; !indent.isEmpty()) {
        const int mode = indent.toInt(&ok);
        if (ok && mode >= 0 && mode <= 2)
            p.autoIndent = static_cast<AutoIndent>(mode);
    }

    // Autocompletado
    r.intAttr("auto-completion", "autoCAction", p.autocStatus, 0, 3);
    r.intAttr("auto-completion", "triggerFromNbChar", p.autocFromLen, 1, 9);
    r.boolAttr("auto-completion", "autoCIgnoreNumbers", p.autocIgnoreNumbers);
    r.boolAttr("auto-completion", "autoCBrief", p.autocBrief);
    r.boolAttr("auto-completion", "funcParams", p.funcParams);
    r.boolAttr("auto-insert", "parentheses", p.insertParentheses);
    r.boolAttr("auto-insert", "brackets", p.insertBrackets);
    r.boolAttr("auto-insert", "curlyBrackets", p.insertCurlyBrackets);
    r.boolAttr("auto-insert", "quotes", p.insertQuotes);
    r.boolAttr("auto-insert", "doubleQuotes", p.insertDoubleQuotes);
    r.boolAttr("auto-insert", "htmlXmlTag", p.insertHtmlXmlTag);

    // Impresión
    r.boolAttr("Print", "lineNumber", p.printLineNumber);
    r.boolAttr("Print", "formFeedPageBreak", p.printFormFeedPageBreak);
    r.intAttr("Print", "printOption", p.printColourMode, 0, 3);
    p.printHeaderLeft = r.get("Print", "headerLeft");
    p.printHeaderMiddle = r.get("Print", "headerMiddle");
    p.printHeaderRight = r.get("Print", "headerRight");
    p.printFooterLeft = r.get("Print", "footerLeft");
    p.printFooterMiddle = r.get("Print", "footerMiddle");
    p.printFooterRight = r.get("Print", "footerRight");
    p.printHeaderFontName = r.get("Print", "headerFontName");
    p.printFooterFontName = r.get("Print", "footerFontName");
    r.intAttr("Print", "headerFontStyle", p.printHeaderFontStyle, 0, 3);
    r.intAttr("Print", "footerFontStyle", p.printFooterFontStyle, 0, 3);
    r.intAttr("Print", "headerFontSize", p.printHeaderFontSize, 0, 72);
    r.intAttr("Print", "footerFontSize", p.printFooterFontSize, 0, 72);
    r.intAttr("Print", "margeLeft", p.printMarginLeft, 0, 200);
    r.intAttr("Print", "margeTop", p.printMarginTop, 0, 200);
    r.intAttr("Print", "margeRight", p.printMarginRight, 0, 200);
    r.intAttr("Print", "margeBottom", p.printMarginBottom, 0, 200);

    // Resaltado
    r.boolText("SmartHighLight", p.smartHighlight);
    r.boolAttr("SmartHighLight", "matchCase", p.smartHighlightMatchCase);
    r.boolAttr("SmartHighLight", "wholeWordOnly", p.smartHighlightWholeWord);
    r.boolAttr("SmartHighLight", "useFindSettings", p.smartHighlightUseFindSettings);
    r.boolAttr("SmartHighLight", "onAnotherView", p.smartHighlightOtherView);
    r.boolText("TagsMatchHighLight", p.tagsMatchHighlight);
    r.boolAttr("TagsMatchHighLight", "TagAttrHighLight", p.tagAttributesHighlight);
    r.boolAttr("MarkAll", "matchCase", p.markAllMatchCase);
    r.boolAttr("MarkAll", "wholeWordOnly", p.markAllWholeWord);

    // Búsqueda
    r.boolAttr("Searching", "fillFindFieldWithSelected", p.fillFindFieldWithSelected);
    r.boolAttr("Searching", "confirmReplaceInAllOpenDocs", p.confirmReplaceInAllOpenDocs);
    r.boolAttr("Searching", "monospacedFontFindDlg", p.monospacedFontFindDlg);

    // Copias de seguridad
    r.boolText("RememberLastSession", p.rememberLastSession);
    r.boolAttr("Backup", "isSnapshotMode", p.snapshotMode);
    r.intAttr("Backup", "snapshotBackupTiming", p.snapshotBackupTiming, 1, 24 * 3600 * 1000);
    r.enumAttr("Backup", "action", p.backupOnSave, 0, 2);
    r.boolAttr("Backup", "useCustumDir", p.useCustomBackupDir); // sic, como N++
    p.backupDir = r.get("Backup", "dir");

    // Varios
    const QString detection = r.text("Auto-detection");
    if (!detection.isEmpty())
        p.fileAutoDetection = fileAutoDetectionFromText(detection);
    r.boolText("DetectEncoding", p.detectEncoding);
    r.intAttr("EditaFallbackCodepage", "codepage", p.fallbackCodepage, 1, 100000);
    p.language = r.get("EditaLanguage", "value");
    r.enumAttr("multiInst", "setting", p.multiInstSetting, 0, 2);
    r.boolAttr("DarkMode", "enable", p.darkModeEnabled);
    r.boolAttr("DarkMode", "enableWindowsMode", p.darkModeFollowSystem);
    p.darkThemeName = r.get("DarkMode", "darkThemeName");
    // Hasta la fase 22, Edita guardaba el tema oscuro de serie de N++ aunque nadie lo
    // hubiera elegido: ahora el de serie es el de Edita (themesVersion marca lo nuevo)
    if (r.get("DarkMode", "themesVersion").isEmpty() && p.darkThemeName == QLatin1String("DarkModeDefault.xml"))
        p.darkThemeName.clear();
    if (config.guiConfig(QStringLiteral("DarkMode"), QStringLiteral("enable")).isEmpty()) {
        // Sin <DarkMode> (config.xml de antes): el tema claro es el que se eligió en el menú
        const QString theme = r.get("stylerTheme", "path");
        if (!theme.isEmpty())
            p.lightThemeName = QFileInfo(theme).fileName();
    } else {
        p.lightThemeName = r.get("DarkMode", "lightThemeName");
    }
    r.boolAttr("ToolBar", "visible", p.toolbarVisible);
    const QString icons = r.text("ToolBar");
    if (icons.startsWith(QLatin1String("small")))
        p.toolbarIcons = ToolbarIcons::Small;
    else if (icons.startsWith(QLatin1String("large")))
        p.toolbarIcons = ToolbarIcons::Large;
    else if (!icons.isEmpty())
        p.toolbarIcons = ToolbarIcons::Standard;
    r.boolText("EditaSystemMonospaceFont", p.systemMonospaceFont);
    r.boolText("EditaQtFileDialog", p.qtFileDialog);
    return p;
}

void storePreferences(ConfigXml& config, const Preferences& p)
{
    GuiWriter w(config);

    w.text("StatusBar", p.statusBarShow ? QStringLiteral("show") : QStringLiteral("hide"));
    w.attr("TabBar", "hide", p.tabBarHide);
    w.attr("TabBar", "closeButton", p.tabCloseButton);
    w.attr("TabBar", "doubleClick2Close", p.tabDoubleClickCloses);
    w.attr("TabBar", "quitOnEmpty", p.quitOnEmpty);

    w.showHide(kView, "lineNumberMargin", p.lineNumberMargin);
    w.attr(kView, "lineNumberDynamicWidth", p.lineNumberDynamicWidth);
    w.showHide(kView, "indentGuideLine", p.indentGuideLine);
    w.attr(kView, "folderMarkStyle", QString::fromLatin1(kFolderStyleNames[static_cast<int>(p.folderStyle) - 1]));
    w.attr(kView, "lineWrapMethod", QString::fromLatin1(kWrapIndentNames[static_cast<int>(p.wrapIndent)]));
    w.attr(kView, "currentLineIndicator", static_cast<int>(p.currentLineMode));
    w.attr(kView, "currentLineFrameWidth", p.currentLineFrameWidth);
    w.attr(kView, "virtualSpace", p.virtualSpace);
    w.attr(kView, "scrollBeyondLastLine", p.scrollBeyondLastLine);
    w.showHide(kView, "wrapSymbolShow", p.wrapSymbolShow);
    w.attr(kView, "Wrap", p.wrap);
    w.attr(kView, "isEdgeBgMode", p.edgeBackground);
    w.attr(kView, "edgeMultiColumnPos", edgeColumnsToText(p.edgeColumns));
    w.showHide(kView, "bookMarkMargin", p.bookmarkMargin);
    w.attr(kView, "isChangeHistoryEnabled", p.changeHistory);
    w.showHide(kView, "whiteSpaceShow", p.whiteSpaceShow);
    w.showHide(kView, "eolShow", p.eolShow);
    w.attr(kView, "smoothFont", p.smoothFont);
    w.attr(kView, "lineCopyCutWithoutSelection", p.lineCopyCutWithoutSelection);
    w.attr(kView, "multiSelection", p.multiSelection);
    w.attr("Caret", "width", p.caretWidth);
    w.attr("Caret", "blinkRate", p.caretBlinkRate);

    w.attr("NewDocDefaultSettings", "format", p.newDocEol);
    w.attr("NewDocDefaultSettings", "encoding", p.newDocUnicodeMode);
    w.attr("NewDocDefaultSettings", "codepage", p.newDocCodepage);
    w.attr("NewDocDefaultSettings", "lang", p.newDocLanguage);
    w.attr("openSaveDir", "value", static_cast<int>(p.openSaveDir));
    w.attr("openSaveDir", "defaultDirPath", p.defaultDirPath);

    RecentFilesConfig recent = config.recentFiles();
    recent.max = p.recentFilesMax;
    recent.inSubMenu = p.recentFilesInSubMenu;
    recent.customLength = p.recentFilesCustomLength;
    config.setRecentFiles(recent);

    w.attr("TabSetting", "replaceBySpace", p.tabReplacedBySpace);
    w.attr("TabSetting", "size", p.tabSize);
    w.attr("TabSetting", "backspaceUnindent", p.backspaceUnindent);
    w.text("MaintainIndent", QString::number(static_cast<int>(p.autoIndent)));

    w.attr("auto-completion", "autoCAction", p.autocStatus);
    w.attr("auto-completion", "triggerFromNbChar", p.autocFromLen);
    w.attr("auto-completion", "autoCIgnoreNumbers", p.autocIgnoreNumbers);
    w.attr("auto-completion", "autoCBrief", p.autocBrief);
    w.attr("auto-completion", "funcParams", p.funcParams);
    w.attr("auto-insert", "parentheses", p.insertParentheses);
    w.attr("auto-insert", "brackets", p.insertBrackets);
    w.attr("auto-insert", "curlyBrackets", p.insertCurlyBrackets);
    w.attr("auto-insert", "quotes", p.insertQuotes);
    w.attr("auto-insert", "doubleQuotes", p.insertDoubleQuotes);
    w.attr("auto-insert", "htmlXmlTag", p.insertHtmlXmlTag);

    w.attr("Print", "lineNumber", p.printLineNumber);
    w.attr("Print", "formFeedPageBreak", p.printFormFeedPageBreak);
    w.attr("Print", "printOption", p.printColourMode);
    w.attr("Print", "headerLeft", p.printHeaderLeft);
    w.attr("Print", "headerMiddle", p.printHeaderMiddle);
    w.attr("Print", "headerRight", p.printHeaderRight);
    w.attr("Print", "footerLeft", p.printFooterLeft);
    w.attr("Print", "footerMiddle", p.printFooterMiddle);
    w.attr("Print", "footerRight", p.printFooterRight);
    w.attr("Print", "headerFontName", p.printHeaderFontName);
    w.attr("Print", "headerFontStyle", p.printHeaderFontStyle);
    w.attr("Print", "headerFontSize", p.printHeaderFontSize);
    w.attr("Print", "footerFontName", p.printFooterFontName);
    w.attr("Print", "footerFontStyle", p.printFooterFontStyle);
    w.attr("Print", "footerFontSize", p.printFooterFontSize);
    w.attr("Print", "margeLeft", p.printMarginLeft);
    w.attr("Print", "margeTop", p.printMarginTop);
    w.attr("Print", "margeRight", p.printMarginRight);
    w.attr("Print", "margeBottom", p.printMarginBottom);

    w.text("SmartHighLight", yesNo(p.smartHighlight));
    w.attr("SmartHighLight", "matchCase", p.smartHighlightMatchCase);
    w.attr("SmartHighLight", "wholeWordOnly", p.smartHighlightWholeWord);
    w.attr("SmartHighLight", "useFindSettings", p.smartHighlightUseFindSettings);
    w.attr("SmartHighLight", "onAnotherView", p.smartHighlightOtherView);
    w.text("TagsMatchHighLight", yesNo(p.tagsMatchHighlight));
    w.attr("TagsMatchHighLight", "TagAttrHighLight", p.tagAttributesHighlight);
    w.attr("MarkAll", "matchCase", p.markAllMatchCase);
    w.attr("MarkAll", "wholeWordOnly", p.markAllWholeWord);

    w.attr("Searching", "monospacedFontFindDlg", p.monospacedFontFindDlg);
    w.attr("Searching", "fillFindFieldWithSelected", p.fillFindFieldWithSelected);
    w.attr("Searching", "confirmReplaceInAllOpenDocs", p.confirmReplaceInAllOpenDocs);

    w.text("RememberLastSession", yesNo(p.rememberLastSession));
    w.attr("Backup", "action", static_cast<int>(p.backupOnSave));
    w.attr("Backup", "useCustumDir", p.useCustomBackupDir);
    w.attr("Backup", "dir", p.backupDir);
    w.attr("Backup", "isSnapshotMode", p.snapshotMode);
    w.attr("Backup", "snapshotBackupTiming", p.snapshotBackupTiming);

    w.text("Auto-detection", fileAutoDetectionToText(p.fileAutoDetection));
    w.text("DetectEncoding", yesNo(p.detectEncoding));
    w.attr("EditaFallbackCodepage", "codepage", p.fallbackCodepage);
    w.attr("EditaLanguage", "value", p.language);
    w.attr("multiInst", "setting", static_cast<int>(p.multiInstSetting));
    w.attr("DarkMode", "enable", p.darkModeEnabled);
    w.attr("DarkMode", "enableWindowsMode", p.darkModeFollowSystem);
    w.attr("DarkMode", "darkThemeName", p.darkThemeName);
    w.attr("DarkMode", "lightThemeName", p.lightThemeName);
    w.attr("DarkMode", "themesVersion", QStringLiteral("2"));
    w.attr("ToolBar", "visible", p.toolbarVisible);
    static const char* const kToolbarIcons[] = {"small", "standard", "large"};
    w.text("ToolBar", QString::fromLatin1(kToolbarIcons[static_cast<int>(p.toolbarIcons)]));
    w.text("EditaSystemMonospaceFont", yesNo(p.systemMonospaceFont));
    w.text("EditaQtFileDialog", yesNo(p.qtFileDialog));
}

QList<int> parseEdgeColumns(const QString& text)
{
    QList<int> columns;
    for (const QString& part : text.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        bool ok = false;
        const int column = part.toInt(&ok);
        if (ok && column > 0 && !columns.contains(column))
            columns << column;
    }
    std::sort(columns.begin(), columns.end());
    return columns;
}

QString edgeColumnsToText(const QList<int>& columns)
{
    QStringList parts;
    for (const int column : columns)
        parts << QString::number(column);
    return parts.join(QLatin1Char(' '));
}

} // namespace edita
