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

#include <QList>
#include <QString>

namespace edita {

class ConfigXml;

// Vigilancia de cambios externos (enum de N++ en Parameters.h: cdDisabled…cdGo2end)
namespace FileAutoDetection {
enum : int
{
    Disabled = 0x00,
    AllFiles = 0x01,     // cdEnabled4AllFiles: se comprueban todos los documentos
    AutoUpdate = 0x02,   // recargar sin preguntar si no hay cambios sin guardar
    GoToEnd = 0x04,      // tras recargar, ir al final (como tail -f)
    CurrentFile = 0x08,  // cdEnabled4CurrentFile: solo el documento visible (por defecto)
};
}

// Copia del archivo anterior al guardar (enum BackupFeature de N++)
enum class BackupOnSave : int
{
    None = 0,    // bak_none
    Simple = 1,  // bak_simple: "archivo.bak" junto al archivo
    Verbose = 2, // bak_verbose: "nppBackup/archivo.AAAA-MM-DD_HHMMSS.bak"
};

// Estilo de los marcadores de plegado (folderStyle de N++), en el orden de su menú
enum class FolderStyle : int
{
    Simple = 1,
    Arrow = 2,
    Circle = 3,
    Box = 4,
    None = 5,
};

// Resaltado de la línea del cursor (lineHiliteMode)
enum class CurrentLineMode : int
{
    None = 0,
    Background = 1,
    Frame = 2,
};

// Sangría de las líneas partidas por el ajuste (lineWrapMethod)
enum class WrapIndent : int
{
    Fixed = 0,   // "default"
    Aligned = 1, // "aligned": como la primera parte de la línea
    Indent = 2,  // "indent": un nivel más
};

// Directorio de los diálogos Abrir/Guardar (OpenSaveDirSetting)
enum class OpenSaveDir : int
{
    FollowCurrent = 0,
    LastUsed = 1,
    Custom = 2,
};

// Instancias (MultiInstSetting de N++): una sola, una nueva para cada sesión, o varias
enum class MultiInst : int
{
    Mono = 0,      // monoInst: los archivos se abren en la ventana que ya está
    OnSession = 1, // multiInstOnSession (Edita no abre sesiones desde la línea de comandos: como Mono)
    Always = 2,    // multiInst
};

// Tamaño de los iconos de la barra de herramientas (<GUIConfig name="ToolBar">small|standard|large</GUIConfig>)
enum class ToolbarIcons : int
{
    Small = 0,    // 16 px
    Standard = 1, // 20 px
    Large = 2,    // 24 px
};

// Sangría automática al pulsar Intro (autoIndentMode)
enum class AutoIndent : int
{
    None = 0,
    Basic = 1, // copia la sangría de la línea anterior
    CLike = 2, // además, un nivel más tras "{" y uno menos al escribir "}"
};

// Preferencias del usuario guardadas en config.xml con los nombres de N++
// (NppGUI y ScintillaViewParams en Parameters.h). Los valores por defecto son
// los de N++, salvo el fin de línea de los documentos nuevos (LF en Linux).
struct Preferences
{
    // --- General (StatusBar, TabBar) ---------------------------------------------
    bool statusBarShow = true;
    bool tabBarHide = false;
    bool tabCloseButton = true;
    bool tabDoubleClickCloses = false;
    bool quitOnEmpty = false; // cerrar la última pestaña cierra la ventana
    // Idioma de la interfaz ("es", "en"…; vacío = el del sistema). En N++ es el
    // nativeLang.xml elegido en Localization; aquí <GUIConfig name="EditaLanguage" value="…"/>
    QString language;
    // Diálogos de archivos de Qt en lugar de los del escritorio (que siguen el idioma
    // del escritorio, no el de Edita): <GUIConfig name="EditaQtFileDialog">yes</GUIConfig>
    bool qtFileDialog = false;
    // Cambios respecto al último commit de Git en un margen y marcas en la barra de
    // desplazamiento (no existen en N++): <GUIConfig name="EditaGitMargin">yes</GUIConfig>,
    // <GUIConfig name="EditaScrollBarMarks">yes</GUIConfig>
    bool gitMargin = true;
    bool scrollBarMarks = true;
    // Mejoras visuales (no existen en N++): el número de la línea actual resaltado, la
    // guía de sangría del bloque actual, la vista previa de colores y los iconos por tipo
    // de archivo (EditaCurrentLineNumber, EditaActiveIndentGuide, EditaColorPreview,
    // EditaFileTypeIcons)
    bool currentLineNumber = true;
    bool activeIndentGuide = true;
    bool colorPreview = true;
    bool fileTypeIcons = true;
    // Paréntesis por colores según el nivel (EditaRainbowBrackets)
    bool rainbowBrackets = true;
    // Página de inicio mientras el único documento es uno nuevo y vacío (EditaWelcomePage)
    bool showWelcome = true;
    // <GUIConfig name="multiInst" setting="0" …/>
    MultiInst multiInstSetting = MultiInst::Mono;
    // Modo oscuro de N++: <GUIConfig name="DarkMode" enable="no" enableWindowsMode="yes"
    // darkThemeName="" lightThemeName=""/>. Con enableWindowsMode sigue al sistema (en N++
    // es "seguir a Windows"; en Edita viene activado). Cada modo tiene su tema de colores:
    // el nombre del archivo en themes/; vacío = el de Edita para ese modo (Edita Light.xml
    // o Edita Dark.xml); "stylers.xml" = el de serie de N++
    bool darkModeEnabled = false;
    bool darkModeFollowSystem = true;
    QString darkThemeName;
    QString lightThemeName;
    // Barra de herramientas: <GUIConfig name="ToolBar" visible="yes">standard</GUIConfig>
    bool toolbarVisible = true;
    ToolbarIcons toolbarIcons = ToolbarIcons::Standard;

    // --- Edición (ScintillaPrimaryView, Caret) -----------------------------------------
    CurrentLineMode currentLineMode = CurrentLineMode::Background;
    int currentLineFrameWidth = 1; // 1-6 px
    int caretWidth = 1;            // 0 = bloque (N++: 0-3)
    int caretBlinkRate = 600;      // ms; 0 = sin parpadeo
    bool multiSelection = true;
    bool virtualSpace = false;
    bool scrollBeyondLastLine = true;
    bool lineCopyCutWithoutSelection = true;
    bool smoothFont = false;
    bool whiteSpaceShow = false;
    bool eolShow = false;
    bool indentGuideLine = true;
    bool wrap = false;
    WrapIndent wrapIndent = WrapIndent::Aligned;
    bool wrapSymbolShow = false;

    // --- Márgenes ------------------------------------------------------------------
    bool lineNumberMargin = true;
    bool lineNumberDynamicWidth = true;
    FolderStyle folderStyle = FolderStyle::Box;
    QList<int> edgeColumns;   // edgeMultiColumnPos: vacío = sin línea de margen
    bool edgeBackground = false; // isEdgeBgMode: colorear lo que pasa de la columna
    bool bookmarkMargin = true;  // bookMarkMargin: margen de los marcadores
    int changeHistory = 1;       // isChangeHistoryEnabled: 0 no, 1 margen, 2 indicadores, 3 los dos

    // --- Documento nuevo (NewDocDefaultSettings) y directorio (openSaveDir) ----------------
    int newDocEol = 2;           // format: 0 Windows, 1 Mac, 2 Unix
    int newDocUnicodeMode = 4;   // encoding: UniMode (4 = UTF-8 sin BOM)
    int newDocCodepage = -1;     // codepage si encoding es ANSI (0)
    int newDocLanguage = 0;      // lang: LangType (0 = texto normal)
    OpenSaveDir openSaveDir = OpenSaveDir::FollowCurrent;
    QString defaultDirPath;

    // --- Archivos recientes (<History>) ---------------------------------------------------
    int recentFilesMax = 10;       // nbMaxFile (0-30)
    bool recentFilesInSubMenu = false;
    int recentFilesCustomLength = -1; // -1 = ruta completa; 0 = solo el nombre; n = n caracteres

    // --- Tabulación y sangría (TabSetting, MaintainIndent) -------------------------------------
    int tabSize = 4;
    bool tabReplacedBySpace = false;
    bool backspaceUnindent = false;
    AutoIndent autoIndent = AutoIndent::Basic;

    // --- Autocompletado (auto-completion, auto-insert) -------------------------------------
    int autocStatus = 3;          // autoCAction: 0 no, 1 funciones (API), 2 palabras, 3 ambas
    int autocFromLen = 1;         // triggerFromNbChar: 1-9 caracteres
    bool autocIgnoreNumbers = true;
    bool autocBrief = false;      // la lista solo con las palabras que empiezan igual
    bool funcParams = true;       // ayuda de parámetros al escribir "("
    bool insertParentheses = false;
    bool insertBrackets = false;
    bool insertCurlyBrackets = false;
    bool insertQuotes = false;
    bool insertDoubleQuotes = false;
    bool insertHtmlXmlTag = false;

    // --- Impresión (Print) -------------------------------------------------------------------
    bool printLineNumber = true;
    bool printFormFeedPageBreak = false;   // un salto de página (\f) empieza página nueva
    int printColourMode = 3;               // printOption: SC_PRINT_NORMAL (0) … SC_PRINT_COLOURONWHITE (3)
    QString printHeaderLeft, printHeaderMiddle, printHeaderRight;
    QString printFooterLeft, printFooterMiddle, printFooterRight;
    QString printHeaderFontName, printFooterFontName; // vacío = Arial (o la que la sustituya)
    int printHeaderFontStyle = 0, printFooterFontStyle = 0; // 1 negrita, 2 cursiva
    int printHeaderFontSize = 0, printFooterFontSize = 0;   // 0 = 9 puntos
    int printMarginLeft = 0, printMarginTop = 0, printMarginRight = 0, printMarginBottom = 0; // mm

    // --- Resaltado (SmartHighLight) -----------------------------------------------------------
    bool smartHighlight = true;
    bool smartHighlightMatchCase = false;
    bool smartHighlightWholeWord = true;
    bool smartHighlightUseFindSettings = false;
    bool smartHighlightOtherView = false;
    bool tagsMatchHighlight = true;   // TagsMatchHighLight: etiqueta XML/HTML pareja
    bool tagAttributesHighlight = true; // TagAttrHighLight
    bool markAllMatchCase = false;    // MarkAll: estilos de token
    bool markAllWholeWord = true;

    // --- Búsqueda (Searching) -----------------------------------------------------------------
    bool fillFindFieldWithSelected = true;
    bool confirmReplaceInAllOpenDocs = true;
    bool monospacedFontFindDlg = false;

    // --- Copias de seguridad (Backup, RememberLastSession) ------------------------------------
    // <GUIConfig name="RememberLastSession">yes</GUIConfig>
    bool rememberLastSession = true;
    // <GUIConfig name="Backup" action="0" useCustumDir="no" dir="" isSnapshotMode="yes" snapshotBackupTiming="7000" />
    bool snapshotMode = true;
    int snapshotBackupTiming = 7000; // ms
    BackupOnSave backupOnSave = BackupOnSave::None;
    bool useCustomBackupDir = false;
    QString backupDir;

    // --- Varios (Auto-detection, DetectEncoding) ---------------------------------------------
    // <GUIConfig name="Auto-detection">yes</GUIConfig>
    int fileAutoDetection = FileAutoDetection::CurrentFile;
    bool detectEncoding = true;
    // Juego de caracteres de los archivos de 8 bits sin detectar (en Windows, el
    // ANSI del sistema). Propio de Edita: <GUIConfig name="EditaFallbackCodepage">
    int fallbackCodepage = 1252;
    // Los temas de N++ piden fuentes de Windows (Courier New, Consolas, Lucida Console):
    // se usa en su lugar la monoespaciada del sistema. Propio de Edita:
    // <GUIConfig name="EditaSystemMonospaceFont">yes</GUIConfig>
    bool systemMonospaceFont = true;

    bool detectsChanges() const { return fileAutoDetection != FileAutoDetection::Disabled; }
    bool detectsAllFiles() const { return (fileAutoDetection & FileAutoDetection::AllFiles) != 0; }
    bool autoUpdates() const { return (fileAutoDetection & FileAutoDetection::AutoUpdate) != 0; }
    bool goesToEnd() const { return (fileAutoDetection & FileAutoDetection::GoToEnd) != 0; }

    bool operator==(const Preferences&) const = default;
};

// NppParameters::feedGUIParameters / createXmlTreeFromGUIParams para estas entradas.
// Lo que falte en el archivo se queda con el valor por defecto.
Preferences loadPreferences(const ConfigXml& config);
void storePreferences(ConfigXml& config, const Preferences& prefs);

// edgeMultiColumnPos: "80 120" ↔ {80, 120} (ordenadas y sin repetir)
QList<int> parseEdgeColumns(const QString& text);
QString edgeColumnsToText(const QList<int>& columns);

// Texto de "Auto-detection" de N++ ("yes", "auto", "Update2End", "yesOld"…) y al revés
int fileAutoDetectionFromText(const QString& text);
QString fileAutoDetectionToText(int value);

} // namespace edita
