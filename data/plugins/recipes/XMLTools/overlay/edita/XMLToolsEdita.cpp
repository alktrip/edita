// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Port de XML Tools (https://github.com/morbac/xmltools, GPL v3) a Edita.
// Las órdenes y sus atajos son los del menú del original (nppMenu.cpp); el
// formateo, la linealización y la ruta actual usan su biblioteca QuickXml sin
// cambios. Donde el original usa MSXML (COM de Windows) se usa libxml2: comprobar
// la sintaxis, validar con DTD o XSD y evaluar XPath; libxslt para XSLT. Los
// diálogos MFC se sustituyen por diálogos de Qt. Las opciones se guardan en
// XMLTools.ini, en la carpeta de configuración de los plugins, como en N++, con
// las mismas claves. Los errores se muestran como anotaciones de Scintilla bajo la
// línea (errorDisplayMode "Annotation", el de serie), en una ventana o en un aviso.

#include <edita_plugin.h>

#include "XmlFormater.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/valid.h>
#include <libxml/xmlschemas.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#ifdef XMLTOOLS_HAVE_XSLT
#include <libxslt/transform.h>
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/xsltutils.h>
#endif

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QFontDatabase>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <map>
#include <stack>
#include <string>
#include <vector>

namespace {

EditaData g_data;
QString g_iniPath;
// XmlToolsConfig (Config.h), con sus valores de serie
bool g_doCheckXML = true;
bool g_doValidation = false;
bool g_doCloseTag = true;
bool g_doAutoXMLType = true;
bool g_doPreventXXE = true;
bool g_doAllowHuge = false;
bool g_doPrettyPrintAllOpenFiles = false;

// struct_xmltoolsoptions: lo que se cambia en Options...
struct Options
{
    QString errorDisplayMode = QStringLiteral("Annotation"); // Annotation / Dialog / Alert
    int maxErrorsNum = 10;
    int maxIndentLevel = 0;
    bool xpathOnStatusbar = true;
    bool dumpAttributeName = false;
    bool printXPathIndex = false;
    QString identityAttributes = QStringLiteral("id;name");
    bool ppAutoclose = true;
    bool ensureConformity = true;
    bool applySpacePreserve = false;
};
Options g_options;

constexpr int kLangXml = 9;   // L_XML
constexpr int kFileNew = 41001; // IDM_FILE_NEW
constexpr int kPrimaryView = 1; // PRIMARY_VIEW
constexpr int kSecondView = 2;  // SECOND_VIEW
// La ruta en la barra de estado se calcula al mover el cursor: en documentos
// enormes, no (lo que tarde QuickXml en recorrerlo se notaría al escribir)
constexpr intptr_t kXPathStatusMaxLength = 4 * 1024 * 1024;

// --- Scintilla -------------------------------------------------------------------------

void* editor()
{
    int view = 0;
    g_data.send(g_data.host, NPPM_GETCURRENTSCINTILLA, 0, reinterpret_cast<intptr_t>(&view));
    return view == 1 ? g_data.subEditor : g_data.mainEditor;
}

intptr_t sci(unsigned message, uintptr_t w = 0, intptr_t l = 0)
{
    return g_data.send(editor(), message, w, l);
}

std::string range(intptr_t start, intptr_t end)
{
    if (end <= start)
        return {};
    std::string text(static_cast<size_t>(end - start) + 1, '\0');
    Sci_TextRangeFull r{{start, end}, text.data()};
    sci(SCI_GETTEXTRANGEFULL, 0, reinterpret_cast<intptr_t>(&r));
    text.resize(static_cast<size_t>(end - start));
    return text;
}

std::string documentText()
{
    return range(0, sci(SCI_GETLENGTH));
}

void setStatus(const QString& text)
{
    const QByteArray utf8 = text.toUtf8();
    g_data.send(g_data.host, NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE, reinterpret_cast<intptr_t>(utf8.constData()));
}

QString currentFile()
{
    std::string buffer(4096, '\0');
    g_data.send(g_data.host, NPPM_GETFULLCURRENTPATH, buffer.size(), reinterpret_cast<intptr_t>(buffer.data()));
    return QString::fromUtf8(buffer.c_str());
}

void info(const QString& text)
{
    QMessageBox::information(QApplication::activeWindow(), QStringLiteral("XML Tools"), text);
}

void error(const QString& text)
{
    QMessageBox::warning(QApplication::activeWindow(), QStringLiteral("XML Tools"), text);
}

intptr_t currentBufferId()
{
    return g_data.send(g_data.host, NPPM_GETCURRENTBUFFERID, 0, 0);
}

int currentLangType()
{
    int lang = 0;
    g_data.send(g_data.host, NPPM_GETCURRENTLANGTYPE, 0, reinterpret_cast<intptr_t>(&lang));
    return lang;
}

// --- Ventana de depuración (Debug.cpp, DebugDlg.cpp) ------------------------------------------

QStringList g_debugLog;
QPointer<QDialog> g_debugDialog;
QPointer<QPlainTextEdit> g_debugText;

void dbgln(const QString& text)
{
    // Lo último, como el original (que también recorta el registro)
    g_debugLog << text;
    while (g_debugLog.size() > 2000)
        g_debugLog.removeFirst();
    if (g_debugText)
        g_debugText->appendPlainText(text);
}

void showDebugDlg()
{
    if (!g_debugDialog) {
        auto* dialog = new QDialog(QApplication::activeWindow());
        dialog->setObjectName(QStringLiteral("XMLToolsDebug"));
        dialog->setWindowTitle(QStringLiteral("XML Tools - Debug window"));
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        auto* layout = new QVBoxLayout(dialog);
        auto* text = new QPlainTextEdit(dialog);
        text->setReadOnly(true);
        text->setLineWrapMode(QPlainTextEdit::NoWrap);
        text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        text->setPlainText(g_debugLog.join(QLatin1Char('\n')));
        layout->addWidget(text, 1);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
        QPushButton* clear = buttons->addButton(QStringLiteral("Clear"), QDialogButtonBox::ResetRole);
        QObject::connect(clear, &QPushButton::clicked, text, [text] {
            g_debugLog.clear();
            text->clear();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
        layout->addWidget(buttons);
        dialog->resize(720, 480);
        g_debugDialog = dialog;
        g_debugText = text;
    }
    g_debugDialog->show();
    g_debugDialog->raise();
    g_debugDialog->activateWindow();
}

// ScintillaDoc::GetWorkText: la selección o, si no hay, todo el documento
struct WorkText
{
    intptr_t start = 0;
    intptr_t end = 0;
    bool selection = false;
    std::string text;
};

WorkText workText()
{
    WorkText w;
    w.start = sci(SCI_GETSELECTIONSTART);
    w.end = sci(SCI_GETSELECTIONEND);
    w.selection = w.end > w.start;
    if (!w.selection) {
        w.start = 0;
        w.end = sci(SCI_GETLENGTH);
    }
    w.text = range(w.start, w.end);
    return w;
}

void replaceWork(const WorkText& w, const std::string& text)
{
    sci(SCI_SETTARGETRANGE, static_cast<uintptr_t>(w.start), w.end);
    sci(SCI_REPLACETARGET, text.size(), reinterpret_cast<intptr_t>(text.data()));
    if (w.selection)
        sci(SCI_SETSEL, static_cast<uintptr_t>(w.start), w.start + static_cast<intptr_t>(text.size()));
}

std::string eolChars()
{
    switch (sci(SCI_GETEOLMODE)) {
    case SC_EOL_CRLF: return "\r\n";
    case SC_EOL_CR: return "\r";
    default: return "\n";
    }
}

std::string indentChars()
{
    if (sci(SCI_GETUSETABS))
        return "\t";
    const intptr_t width = sci(SCI_GETINDENT) > 0 ? sci(SCI_GETINDENT) : sci(SCI_GETTABWIDTH);
    return std::string(static_cast<size_t>(width > 0 ? width : 4), ' ');
}

// --- Varios documentos (nppHelpers.cpp: nppMultiDocumentCommand) -----------------------------

// La orden en el documento actual o, con "Apply to all open files", en todos los
// abiertos de las dos vistas (activándolos uno a uno, como el original). Al terminar
// vuelve al documento en el que se estaba
void forEachDocument(const QString& name, const std::function<void()>& action)
{
    dbgln(QStringLiteral("+ nppMultiDocumentCommand(\"%1\")").arg(name));
    const std::clock_t start = std::clock();
    auto run = [&] {
        if (sci(SCI_GETREADONLY))
            return;
        const std::clock_t docStart = std::clock();
        action();
        dbgln(QStringLiteral("%1 => time taken: %2 ms").arg(currentFile()).arg((std::clock() - docStart) * 1000 / CLOCKS_PER_SEC));
        sci(SCI_SETXOFFSET, 0);
    };
    if (!g_doPrettyPrintAllOpenFiles) {
        run();
    } else {
        int view = 0;
        g_data.send(g_data.host, NPPM_GETCURRENTSCINTILLA, 0, reinterpret_cast<intptr_t>(&view));
        const intptr_t index = g_data.send(g_data.host, NPPM_GETCURRENTDOCINDEX, 0, view);
        const int views[] = {kPrimaryView, kSecondView};
        for (int v = 0; v < 2; ++v) {
            if (g_data.send(g_data.host, NPPM_GETCURRENTDOCINDEX, 0, v) < 0)
                continue;
            const intptr_t count = g_data.send(g_data.host, NPPM_GETNBOPENFILES, 0, views[v]);
            for (intptr_t i = 0; i < count; ++i) {
                if (g_data.send(g_data.host, NPPM_ACTIVATEDOC, static_cast<uintptr_t>(v), i))
                    run();
            }
        }
        if (index >= 0)
            g_data.send(g_data.host, NPPM_ACTIVATEDOC, static_cast<uintptr_t>(view), index);
    }
    dbgln(QStringLiteral("- nppMultiDocumentCommand(\"%1\") => time taken: %2 ms").arg(name).arg((std::clock() - start) * 1000 / CLOCKS_PER_SEC));
}

// --- Tipo XML automático (ToolsEditor.cpp: setAutoXMLType) ----------------------------------

int setAutoXMLType(bool force = false)
{
    int docType = currentLangType();
    if (force || (g_doAutoXMLType && docType != kLangXml)) {
        char head[8] = {};
        sci(SCI_GETTEXT, 7, reinterpret_cast<intptr_t>(head));
        if (std::strlen(head) >= 6 && std::strncmp(head, "<?xml ", 6) == 0) {
            g_data.send(g_data.host, NPPM_SETCURRENTLANGTYPE, 0, kLangXml);
            docType = kLangXml;
        }
    }
    return docType;
}

// --- Formateo (ToolsPrettyPrintFast.cpp, con QuickXml) -----------------------------------------

enum class Format { PrettyPrint, IndentAttributes, IndentOnly, Linearize, Tokenize };

QuickXml::XmlFormaterParamsType formatParams(Format kind)
{
    QuickXml::XmlFormaterParamsType params;
    params.indentChars = indentChars();
    params.eolChars = eolChars();
    params.maxIndentLevel = static_cast<size_t>(std::max(0, g_options.maxIndentLevel));
    params.ensureConformity = g_options.ensureConformity;
    params.autoCloseTags = g_options.ppAutoclose;
    params.indentAttributes = kind == Format::IndentAttributes;
    params.indentOnly = kind == Format::IndentOnly;
    params.applySpacePreserve = g_options.applySpacePreserve;
    return params;
}

void formatDocument(Format kind)
{
    const WorkText w = workText();
    if (w.text.empty())
        return;
    QuickXml::XmlFormater formater(w.text.data(), w.text.size(), formatParams(kind));
    if (kind == Format::Tokenize) {
        // sciDocTokenizeQuickXml: los tokens de QuickXml, en la ventana de depuración
        dbgln(QString::fromStdString(formater.debugTokens("\n", true)));
        return;
    }
    std::stringstream* out = kind == Format::Linearize ? formater.linearize() : formater.prettyPrint();
    const std::string result = out->str();
    if (result != w.text) {
        sci(SCI_BEGINUNDOACTION);
        replaceWork(w, result);
        sci(SCI_ENDUNDOACTION);
    }
}

void format(Format kind)
{
    static const char* const names[] = {"PrettyPrintFast (quickxml)", "PrettyPrintAttrFast (quickxml)",
                                        "PrettyPrintIndentOnlyFast (quickxml)", "LinearizeFast (quickxml)",
                                        "TokenizeFast (quickxml)"};
    forEachDocument(QString::fromLatin1(names[static_cast<int>(kind)]), [kind] { formatDocument(kind); });
    if (kind == Format::Tokenize)
        showDebugDlg();
    else
        setAutoXMLType();
}

// --- Ruta actual (ToolsXPath.cpp: currentXPath) ----------------------------------------------

QString currentXPath(int mode)
{
    const std::string text = documentText();
    QuickXml::XmlFormaterParamsType params = QuickXml::XmlFormater::getDefaultParams();
    if ((mode & XPATH_MODE_KEEPIDATTRIBUTE) && !g_options.identityAttributes.isEmpty()) {
        params.dumpIdAttributesName = g_options.dumpAttributeName;
        for (const QString& attribute : g_options.identityAttributes.split(QLatin1Char(';'), Qt::SkipEmptyParts))
            params.identityAttribues.push_back(attribute.trimmed().toStdString());
    }
    if (g_options.printXPathIndex)
        mode |= XPATH_MODE_WITHNODEINDEX;
    QuickXml::XmlFormater formater(text.data(), text.size(), params);
    return QString::fromStdString(formater.currentPath(static_cast<size_t>(sci(SCI_GETCURRENTPOS)), mode)->str());
}

// printCurrentXPathInStatusbar
void printCurrentXPathInStatusbar()
{
    if (sci(SCI_GETLENGTH) > kXPathStatusMaxLength)
        return;
    const QString path = currentXPath(XPATH_MODE_WITHNAMESPACE | XPATH_MODE_KEEPIDATTRIBUTE);
    setStatus(path.isEmpty() ? QStringLiteral(" ") : path);
}

void showCurrentXPath(bool precise)
{
    const QString path = currentXPath(precise ? (XPATH_MODE_WITHNAMESPACE | XPATH_MODE_KEEPIDATTRIBUTE) : XPATH_MODE_BASIC);
    if (path.isEmpty()) {
        info(QStringLiteral("Current node cannot be resolved."));
        return;
    }
    QGuiApplication::clipboard()->setText(path);
    info(path + QStringLiteral("\n\n(Path has been copied into clipboard)"));
}

// --- libxml2: sintaxis, validación y XPath (en lugar de MSXMLWrapper) -------------------------

struct XmlError
{
    int line = 0;
    int column = 0;
    QString message;
};

struct ErrorCollector
{
    std::vector<XmlError> errors;
};

void collectError(void* userData, const xmlError* e)
{
    if (!e || !userData)
        return;
    auto* collector = static_cast<ErrorCollector*>(userData);
    collector->errors.push_back({e->line, e->int2, QString::fromUtf8(e->message ? e->message : "").trimmed()});
}

// Los mensajes sueltos de libxml2 no van a la consola
void silentGeneric(void*, const char*, ...) {}

void installCollector(ErrorCollector* collector)
{
    xmlSetStructuredErrorFunc(collector, reinterpret_cast<xmlStructuredErrorFunc>(&collectError));
    xmlSetGenericErrorFunc(nullptr, &silentGeneric);
}

void removeCollector()
{
    xmlSetStructuredErrorFunc(nullptr, nullptr);
    xmlSetGenericErrorFunc(nullptr, nullptr);
}

int parseOptions(bool loadDtd)
{
    // doPreventXXE (prohibitDTD/resolveExternals de MSXML): sin red y sin sustituir
    // entidades externas. Sin la protección, se cargan la DTD y las entidades
    int options = XML_PARSE_NONET;
    if (loadDtd)
        options |= XML_PARSE_DTDLOAD | XML_PARSE_DTDATTR;
    if (!g_doPreventXXE)
        options = (options & ~XML_PARSE_NONET) | XML_PARSE_DTDLOAD | XML_PARSE_NOENT;
    // doAllowHuge: sin los límites de tamaño de libxml2 (nodos de más de 10 MB, anidamiento…)
    if (g_doAllowHuge)
        options |= XML_PARSE_HUGE;
    return options;
}

xmlDocPtr parseDocument(const std::string& text, ErrorCollector& collector, bool loadDtd)
{
    installCollector(&collector);
    const QByteArray url = QFile::encodeName(currentFile());
    xmlDocPtr doc = xmlReadMemory(text.data(), static_cast<int>(text.size()), url.isEmpty() ? nullptr : url.constData(),
                                  nullptr, parseOptions(loadDtd));
    removeCollector();
    return doc;
}

// --- Errores (ToolsErrors.cpp) ----------------------------------------------------------------

// Los errores de cada documento (xmlErrors[bufferid]) y el que está resaltado
struct BufferErrors
{
    std::vector<XmlError> errors;
    size_t current = 0;
    void* view = nullptr;
};
std::map<intptr_t, BufferErrors> g_errors;
std::map<void*, int> g_annotationStyles; // el primero de los dos estilos de anotación de cada editor

// Los dos estilos (error y error resaltado) con colores que se leen en temas claros y oscuros
int annotationStyle(void* view)
{
    auto it = g_annotationStyles.find(view);
    if (it == g_annotationStyles.end())
        it = g_annotationStyles.emplace(view, static_cast<int>(g_data.send(view, SCI_ALLOCATEEXTENDEDSTYLES, 2, 0))).first;
    const int style = it->second;
    const intptr_t back = g_data.send(view, SCI_STYLEGETBACK, STYLE_DEFAULT, 0);
    const int luma = static_cast<int>((back & 0xFF) * 3 + ((back >> 8) & 0xFF) * 6 + ((back >> 16) & 0xFF)) / 10;
    const bool dark = luma < 128;
    // Scintilla: 0xBBGGRR
    const intptr_t fore = dark ? 0x9C9CFF : 0x1C1CA0;
    const intptr_t errorBack = dark ? 0x2A2A4A : 0xE6E6FF;
    const intptr_t highlightBack = dark ? 0x30309C : 0x4040D8;
    std::string font(static_cast<size_t>(g_data.send(view, SCI_STYLEGETFONT, STYLE_DEFAULT, 0)) + 1, '\0');
    g_data.send(view, SCI_STYLEGETFONT, STYLE_DEFAULT, reinterpret_cast<intptr_t>(font.data()));
    for (int i = 0; i < 2; ++i) {
        g_data.send(view, SCI_STYLESETFONT, static_cast<uintptr_t>(style + i), reinterpret_cast<intptr_t>(font.c_str()));
        g_data.send(view, SCI_STYLESETSIZE, static_cast<uintptr_t>(style + i), g_data.send(view, SCI_STYLEGETSIZE, STYLE_DEFAULT, 0));
        g_data.send(view, SCI_STYLESETITALIC, static_cast<uintptr_t>(style + i), 1);
    }
    g_data.send(view, SCI_STYLESETFORE, static_cast<uintptr_t>(style), fore);
    g_data.send(view, SCI_STYLESETBACK, static_cast<uintptr_t>(style), errorBack);
    g_data.send(view, SCI_STYLESETFORE, static_cast<uintptr_t>(style + 1), 0xFFFFFF);
    g_data.send(view, SCI_STYLESETBACK, static_cast<uintptr_t>(style + 1), highlightBack);
    g_data.send(view, SCI_ANNOTATIONSETSTYLEOFFSET, static_cast<uintptr_t>(style), 0);
    return style;
}

// clearErrors / clearAnnotations
void clearErrors(intptr_t bufferId)
{
    auto it = g_errors.find(bufferId);
    if (it == g_errors.end())
        return;
    if (it->second.view && bufferId == currentBufferId())
        g_data.send(it->second.view, SCI_ANNOTATIONCLEARALL, 0, 0);
    g_errors.erase(it);
}

// displayXMLError: el texto de cada error, bajo su línea (varios en la misma, juntos)
void annotate(BufferErrors& entry)
{
    void* view = entry.view;
    annotationStyle(view);
    std::map<int, std::string> lines;
    for (const XmlError& e : entry.errors) {
        if (e.line <= 0)
            continue;
        std::string& text = lines[e.line - 1];
        if (!text.empty())
            text += '\n';
        text += QStringLiteral("Line %1, pos %2: %3").arg(e.line).arg(e.column).arg(e.message).toStdString();
    }
    for (const auto& [line, text] : lines) {
        g_data.send(view, SCI_ANNOTATIONSETTEXT, static_cast<uintptr_t>(line), reinterpret_cast<intptr_t>(text.c_str()));
        g_data.send(view, SCI_ANNOTATIONSETSTYLE, static_cast<uintptr_t>(line), 0);
    }
    g_data.send(view, SCI_ANNOTATIONSETVISIBLE, ANNOTATION_BOXED, 0);
}

// highlightCurrentError: va al error y, con anotaciones, lo resalta
void highlightCurrentError()
{
    auto it = g_errors.find(currentBufferId());
    if (it == g_errors.end() || it->second.errors.empty())
        return;
    BufferErrors& entry = it->second;
    const XmlError& err = entry.errors[entry.current];
    setStatus(QStringLiteral("XML Tools: line %1: %2").arg(err.line).arg(err.message));
    if (err.line <= 0)
        return;
    const auto line = static_cast<uintptr_t>(err.line - 1);
    // centerOnPosition: la línea del error, con su anotación, en el centro
    sci(SCI_ENSUREVISIBLEENFORCEPOLICY, line);
    const intptr_t visible = sci(SCI_VISIBLEFROMDOCLINE, line);
    const intptr_t onScreen = sci(SCI_LINESONSCREEN);
    sci(SCI_SETFIRSTVISIBLELINE, static_cast<uintptr_t>(std::max<intptr_t>(0, visible - onScreen / 2)));
    sci(SCI_GOTOPOS, static_cast<uintptr_t>(sci(SCI_POSITIONFROMLINE, line) + std::max(0, err.column - 1)));
    if (g_options.errorDisplayMode != QLatin1String("Annotation"))
        return;
    // El error actual con el estilo resaltado; los demás de su línea, con el normal
    std::string styles;
    size_t offset = 0;
    for (const XmlError& e : entry.errors) {
        if (e.line != err.line)
            continue;
        const std::string text = QStringLiteral("Line %1, pos %2: %3").arg(e.line).arg(e.column).arg(e.message).toStdString();
        if (offset > 0) {
            styles += '\0';
            ++offset;
        }
        styles.append(text.size(), &e == &err ? '\1' : '\0');
        offset += text.size();
    }
    g_data.send(entry.view, SCI_ANNOTATIONSETSTYLES, line, reinterpret_cast<intptr_t>(styles.data()));
}

void highlightError(int which) // 0 primero, -1 anterior, 1 siguiente, 2 último
{
    auto it = g_errors.find(currentBufferId());
    if (it == g_errors.end() || it->second.errors.empty())
        return;
    BufferErrors& entry = it->second;
    switch (which) {
    case 0: entry.current = 0; break;
    case -1: if (entry.current > 0) --entry.current; break;
    case 1: if (entry.current + 1 < entry.errors.size()) ++entry.current; break;
    default: entry.current = entry.errors.size() - 1; break;
    }
    highlightCurrentError();
}

void highlightFirstError() { highlightError(0); }
void highlightPreviousError() { highlightError(-1); }
void highlightNextError() { highlightError(1); }
void highlightLastError() { highlightError(2); }

// La lista de errores, en una ventana que no bloquea (CMessageDlg)
void showErrorsWindow(const QString& what, const std::vector<XmlError>& errors, size_t total)
{
    auto* dialog = new QDialog(QApplication::activeWindow());
    dialog->setWindowTitle(QStringLiteral("XML Tools"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    auto* layout = new QVBoxLayout(dialog);
    auto* text = new QPlainTextEdit(dialog);
    text->setReadOnly(true);
    QStringList lines{what, QString()};
    for (const XmlError& e : errors)
        lines << (e.line > 0 ? QStringLiteral("Line %1, pos %2: %3").arg(e.line).arg(e.column).arg(e.message) : e.message);
    if (total > errors.size())
        lines << (total - errors.size() > 1 ? QStringLiteral("%1 errors follow but are not displayed").arg(total - errors.size())
                                            : QStringLiteral("1 error follows but is not displayed"));
    text->setPlainText(lines.join(QLatin1Char('\n')));
    layout->addWidget(text, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    layout->addWidget(buttons);
    dialog->resize(600, 320);
    dialog->show();
}

// displayXMLErrors: se guardan para First/Previous/Next/Last error y se muestran según
// errorDisplayMode; sin `silent` (comprobación al guardar), las ventanas no se abren
void reportErrors(std::vector<XmlError> errors, const QString& what, bool silent)
{
    const intptr_t bufferId = currentBufferId();
    clearErrors(bufferId);
    const size_t total = errors.size();
    if (g_options.maxErrorsNum >= 0 && errors.size() > static_cast<size_t>(g_options.maxErrorsNum))
        errors.resize(static_cast<size_t>(std::max(1, g_options.maxErrorsNum)));
    BufferErrors& entry = g_errors[bufferId];
    entry.errors = errors;
    entry.view = editor();
    dbgln(what);
    for (const XmlError& e : errors)
        dbgln(QStringLiteral("  line %1, pos %2: %3").arg(e.line).arg(e.column).arg(e.message));
    const bool positioned = errors.front().line > 0;
    if (g_options.errorDisplayMode == QLatin1String("Annotation") && positioned)
        annotate(entry);
    if (g_options.errorDisplayMode != QLatin1String("Alert"))
        highlightFirstError();
    else
        setStatus(QStringLiteral("XML Tools: %1 – line %2: %3").arg(what).arg(errors.front().line).arg(errors.front().message));
    if (silent)
        return;
    if (g_options.errorDisplayMode == QLatin1String("Dialog")) {
        showErrorsWindow(what, errors, total);
    } else if (g_options.errorDisplayMode == QLatin1String("Alert") || !positioned) {
        QStringList lines;
        for (const XmlError& e : errors)
            lines << (e.line > 0 ? QStringLiteral("Line %1, pos %2: %3").arg(e.line).arg(e.column).arg(e.message) : e.message);
        error(what + QStringLiteral(":\n\n") + lines.join(QLatin1Char('\n')));
    }
}

// manualXMLCheck / XMLValidation
bool checkSyntax(bool silent)
{
    ErrorCollector collector;
    xmlDocPtr doc = parseDocument(documentText(), collector, false);
    const bool ok = doc && collector.errors.empty();
    if (doc)
        xmlFreeDoc(doc);
    if (ok) {
        clearErrors(currentBufferId());
        setStatus(QStringLiteral("XML Tools: no error detected"));
        if (!silent)
            info(QStringLiteral("No error detected."));
        return true;
    }
    if (collector.errors.empty())
        collector.errors.push_back({0, 0, QStringLiteral("document is empty or not XML")});
    reportErrors(collector.errors, QStringLiteral("XML Parsing error"), silent);
    return false;
}

// La ubicación del esquema: xsi:noNamespaceSchemaLocation o la segunda parte de xsi:schemaLocation
QString schemaLocation(xmlDocPtr doc)
{
    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root)
        return {};
    static const xmlChar* xsi = reinterpret_cast<const xmlChar*>("http://www.w3.org/2001/XMLSchema-instance");
    if (xmlChar* v = xmlGetNsProp(root, reinterpret_cast<const xmlChar*>("noNamespaceSchemaLocation"), xsi)) {
        const QString location = QString::fromUtf8(reinterpret_cast<const char*>(v)).trimmed();
        xmlFree(v);
        return location;
    }
    if (xmlChar* v = xmlGetNsProp(root, reinterpret_cast<const xmlChar*>("schemaLocation"), xsi)) {
        const QStringList parts = QString::fromUtf8(reinterpret_cast<const char*>(v)).simplified().split(QLatin1Char(' '));
        xmlFree(v);
        return parts.size() >= 2 ? parts[1] : QString();
    }
    return {};
}

QString resolve(const QString& location)
{
    if (location.contains(QStringLiteral("://")) || QDir::isAbsolutePath(location))
        return location;
    const QString file = currentFile();
    return QFileInfo(file).absoluteDir().filePath(location);
}

bool validate(bool silent)
{
    ErrorCollector collector;
    xmlDocPtr doc = parseDocument(documentText(), collector, true);
    if (!doc || !collector.errors.empty()) {
        if (doc)
            xmlFreeDoc(doc);
        if (collector.errors.empty())
            collector.errors.push_back({0, 0, QStringLiteral("document is empty or not XML")});
        reportErrors(collector.errors, QStringLiteral("XML Parsing error"), silent);
        return false;
    }
    bool validated = false;
    bool valid = true;
    installCollector(&collector);
    // DTD interna o externa
    if (doc->intSubset || doc->extSubset) {
        xmlValidCtxtPtr ctxt = xmlNewValidCtxt();
        valid = xmlValidateDocument(ctxt, doc) == 1;
        xmlFreeValidCtxt(ctxt);
        validated = true;
    }
    // XSD indicado en el documento o, si no hay nada, el que elija el usuario (SelectFileDlg)
    QString schema = schemaLocation(doc);
    if (schema.isEmpty() && !validated && !silent) {
        removeCollector();
        schema = QFileDialog::getOpenFileName(QApplication::activeWindow(), QStringLiteral("XML Schema"),
                                              QFileInfo(currentFile()).absolutePath(),
                                              QStringLiteral("XML Schema (*.xsd);;All files (*)"));
        installCollector(&collector);
    }
    if (!schema.isEmpty()) {
        const QByteArray path = QFile::encodeName(resolve(schema));
        xmlSchemaParserCtxtPtr parser = xmlSchemaNewParserCtxt(path.constData());
        xmlSchemaPtr xsd = parser ? xmlSchemaParse(parser) : nullptr;
        if (!xsd) {
            removeCollector();
            if (parser)
                xmlSchemaFreeParserCtxt(parser);
            xmlFreeDoc(doc);
            if (!silent)
                error(QStringLiteral("Unable to load the schema %1").arg(schema));
            return false;
        }
        xmlSchemaValidCtxtPtr ctxt = xmlSchemaNewValidCtxt(xsd);
        valid = xmlSchemaValidateDoc(ctxt, doc) == 0 && valid;
        xmlSchemaFreeValidCtxt(ctxt);
        xmlSchemaFree(xsd);
        xmlSchemaFreeParserCtxt(parser);
        validated = true;
    }
    removeCollector();
    xmlFreeDoc(doc);
    if (!validated) {
        clearErrors(currentBufferId());
        if (!silent)
            info(QStringLiteral("No DTD or schema to validate against."));
        return true;
    }
    if (valid && collector.errors.empty()) {
        clearErrors(currentBufferId());
        setStatus(QStringLiteral("XML Tools: XML is valid"));
        if (!silent)
            info(QStringLiteral("XML is valid."));
        return true;
    }
    if (collector.errors.empty())
        collector.errors.push_back({0, 0, QStringLiteral("document is not valid")});
    reportErrors(collector.errors, QStringLiteral("Validation error"), silent);
    return false;
}

// Evaluate XPath expression (XPathEvalDlg con libxml2)
QString evaluateXPathText(const QString& expression, QString* problem)
{
    ErrorCollector collector;
    xmlDocPtr doc = parseDocument(documentText(), collector, false);
    if (!doc) {
        *problem = collector.errors.empty() ? QStringLiteral("document is not XML") : collector.errors.front().message;
        return {};
    }
    xmlXPathContextPtr ctxt = xmlXPathNewContext(doc);
    // Los espacios de nombres del elemento raíz; el predeterminado como "default"
    if (xmlNodePtr root = xmlDocGetRootElement(doc)) {
        for (xmlNsPtr ns = root->nsDef; ns; ns = ns->next)
            xmlXPathRegisterNs(ctxt, ns->prefix ? ns->prefix : reinterpret_cast<const xmlChar*>("default"), ns->href);
    }
    installCollector(&collector);
    const QByteArray expr = expression.toUtf8();
    xmlXPathObjectPtr result = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(expr.constData()), ctxt);
    removeCollector();
    QStringList lines;
    if (!result) {
        *problem = collector.errors.empty() ? QStringLiteral("invalid XPath expression") : collector.errors.front().message;
    } else if (result->type == XPATH_NODESET) {
        const int count = result->nodesetval ? result->nodesetval->nodeNr : 0;
        lines << QStringLiteral("%1 node(s) found").arg(count);
        for (int i = 0; i < count; ++i) {
            xmlNodePtr node = result->nodesetval->nodeTab[i];
            xmlChar* content = xmlNodeGetContent(node);
            QString value = QString::fromUtf8(reinterpret_cast<const char*>(content ? content : reinterpret_cast<const xmlChar*>(""))).simplified();
            if (content)
                xmlFree(content);
            if (value.size() > 200)
                value = value.left(200) + QStringLiteral("…");
            const QString name = QString::fromUtf8(reinterpret_cast<const char*>(node->name ? node->name : reinterpret_cast<const xmlChar*>("")));
            const QString kind = node->type == XML_ATTRIBUTE_NODE ? QStringLiteral("@") : QString();
            lines << QStringLiteral("[%1] %2%3 (line %4): %5").arg(i + 1).arg(kind, name).arg(xmlGetLineNo(node)).arg(value);
        }
    } else if (result->type == XPATH_BOOLEAN) {
        lines << (result->boolval ? QStringLiteral("true") : QStringLiteral("false"));
    } else if (result->type == XPATH_NUMBER) {
        lines << QString::number(result->floatval);
    } else if (result->type == XPATH_STRING) {
        lines << QString::fromUtf8(reinterpret_cast<const char*>(result->stringval));
    }
    if (result)
        xmlXPathFreeObject(result);
    xmlXPathFreeContext(ctxt);
    xmlFreeDoc(doc);
    return lines.join(QLatin1Char('\n'));
}

void evaluateXPath()
{
    QDialog dialog(QApplication::activeWindow());
    dialog.setWindowTitle(QStringLiteral("XPath expression evaluation"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* row = new QHBoxLayout;
    auto* expression = new QLineEdit(&dialog);
    expression->setPlaceholderText(QStringLiteral("/root/item[@id='1']"));
    auto* evaluate = new QPushButton(QStringLiteral("Evaluate"), &dialog);
    row->addWidget(new QLabel(QStringLiteral("XPath:"), &dialog));
    row->addWidget(expression, 1);
    row->addWidget(evaluate);
    layout->addLayout(row);
    auto* results = new QPlainTextEdit(&dialog);
    results->setReadOnly(true);
    layout->addWidget(results, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    auto run = [&] {
        QString problem;
        const QString text = evaluateXPathText(expression->text(), &problem);
        results->setPlainText(problem.isEmpty() ? text : QStringLiteral("Error: ") + problem);
    };
    QObject::connect(evaluate, &QPushButton::clicked, &dialog, run);
    QObject::connect(expression, &QLineEdit::returnPressed, &dialog, run);
    dialog.resize(640, 420);
    dialog.exec();
}

void xslTransform()
{
#ifdef XMLTOOLS_HAVE_XSLT
    const QString xsl = QFileDialog::getOpenFileName(QApplication::activeWindow(), QStringLiteral("XSL Transformation"),
                                                     QFileInfo(currentFile()).absolutePath(),
                                                     QStringLiteral("XSL (*.xsl *.xslt);;All files (*)"));
    if (xsl.isEmpty())
        return;
    ErrorCollector collector;
    xmlDocPtr doc = parseDocument(documentText(), collector, false);
    if (!doc) {
        if (collector.errors.empty())
            collector.errors.push_back({0, 0, QStringLiteral("document is not XML")});
        reportErrors(collector.errors, QStringLiteral("XML Parsing error"), false);
        return;
    }
    installCollector(&collector);
    xsltStylesheetPtr style = xsltParseStylesheetFile(reinterpret_cast<const xmlChar*>(QFile::encodeName(xsl).constData()));
    xmlDocPtr out = style ? xsltApplyStylesheet(style, doc, nullptr) : nullptr;
    removeCollector();
    std::string result;
    if (out) {
        xmlChar* buffer = nullptr;
        int length = 0;
        xsltSaveResultToString(&buffer, &length, out, style);
        if (buffer) {
            result.assign(reinterpret_cast<const char*>(buffer), static_cast<size_t>(length));
            xmlFree(buffer);
        }
        xmlFreeDoc(out);
    }
    if (style)
        xsltFreeStylesheet(style);
    xmlFreeDoc(doc);
    if (!out) {
        error(QStringLiteral("XSL transformation failed:\n") +
              (collector.errors.empty() ? QString() : collector.errors.front().message));
        return;
    }
    // El resultado, en un documento nuevo (como el original)
    g_data.send(g_data.host, NPPM_MENUCOMMAND, 0, kFileNew);
    sci(SCI_SETTEXT, 0, reinterpret_cast<intptr_t>(result.c_str()));
#else
    error(QStringLiteral("XSL transformation is not available (XML Tools was built without libxslt)."));
#endif
}

// --- Escapar y comentar (ToolsConvert.cpp, ToolsComment.cpp) -----------------------------------

void replaceAll(std::string& str, const char* find, const char* replacement)
{
    const size_t findSize = std::strlen(find);
    const size_t replSize = std::strlen(replacement);
    for (size_t pos = str.find(find); pos != std::string::npos; pos = str.find(find, pos + replSize))
        str.replace(pos, findSize, replacement);
}

bool selectedText(std::string& text, intptr_t& start)
{
    start = sci(SCI_GETSELECTIONSTART);
    const intptr_t end = sci(SCI_GETSELECTIONEND);
    if (end <= start || sci(SCI_GETREADONLY)) {
        if (end <= start)
            error(QStringLiteral("Please select text to transform before you call the function."));
        return false;
    }
    text = range(start, end);
    return true;
}

void replaceSelection(const std::string& text, intptr_t start)
{
    sci(SCI_REPLACESEL, 0, reinterpret_cast<intptr_t>(text.c_str()));
    sci(SCI_SETCURRENTPOS, static_cast<uintptr_t>(start));
    sci(SCI_SETANCHOR, static_cast<uintptr_t>(start + static_cast<intptr_t>(text.size())));
}

void escapeSelection()
{
    std::string str;
    intptr_t start = 0;
    if (!selectedText(str, start))
        return;
    replaceAll(str, "&", "&amp;"); // &amp; el primero
    replaceAll(str, "'", "&apos;");
    replaceAll(str, "\"", "&quot;");
    replaceAll(str, "<", "&lt;");
    replaceAll(str, ">", "&gt;");
    replaceSelection(str, start);
}

void unescapeSelection()
{
    std::string str;
    intptr_t start = 0;
    if (!selectedText(str, start))
        return;
    replaceAll(str, "&apos;", "'");
    replaceAll(str, "&quot;", "\"");
    replaceAll(str, "&lt;", "<");
    replaceAll(str, "&gt;", ">");
    replaceAll(str, "&amp;", "&"); // &amp; el último
    replaceSelection(str, start);
}

// validateSelectionForComment: ¿la selección corta otro comentario?
int validateSelectionForComment(const std::string& str, size_t length)
{
    std::stack<int> check;
    size_t pos = 0;
    int err = 0;
    while (pos <= length && !err && (pos = str.find_first_of("<-*", pos)) != std::string::npos) {
        if (pos > length)
            break;
        if (!str.compare(pos, 4, "<!--"))
            check.push(0);
        if (!str.compare(pos, 3, "-->")) {
            if (check.empty()) {
                err = -3;
                break;
            }
            if (check.top() != 0)
                err = check.top();
            else
                check.pop();
        }
        if (!str.compare(pos, 3, "<![")) {
            const size_t endval = str.find("]**", pos);
            if (endval != std::string::npos)
                check.push(std::atoi(str.substr(pos + 3, endval).c_str()));
        }
        if (!str.compare(pos, 3, "**[")) {
            if (check.empty()) {
                err = -4;
                break;
            }
            const size_t endval = str.find("]>", pos);
            if (endval != std::string::npos && std::atoi(str.substr(pos + 3, endval).c_str()) != check.top())
                err = -2;
            else
                check.pop();
        }
        ++pos;
    }
    if (!check.empty())
        err = -1;
    return err;
}

// Renumera los marcadores de comentarios anidados (<!{n}** y **{n}>) en `delta`;
// el que llega a 0 vuelve a ser `original` (<!-- o -->)
void renumber(std::string& str, const char* open, const char* close, int delta, const char* original)
{
    const bool opening = std::strcmp(open, "<!{") == 0;
    size_t pos = str.size();
    while ((pos = str.rfind(open, pos)) != std::string::npos) {
        const size_t endval = str.find(close, pos);
        if (endval == std::string::npos)
            break;
        const int value = std::atoi(str.substr(pos + 3, endval - pos - 3).c_str()) + delta;
        const size_t len = endval - pos + std::strlen(close);
        if (value >= 1) {
            char tmp[64];
            std::snprintf(tmp, sizeof tmp, opening ? "<!{%d}**" : "**{%d}>", value);
            str.replace(pos, len, tmp);
        } else {
            str.replace(pos, len, original);
        }
        if (pos == 0)
            break;
        --pos;
    }
}

void commentSelection()
{
    std::string str;
    intptr_t start = 0;
    if (!selectedText(str, start))
        return;
    // Los saltos de línea del final quedan fuera del comentario
    while (!str.empty() && (str.back() == '\r' || str.back() == '\n'))
        str.pop_back();
    const intptr_t trimmedEnd = start + static_cast<intptr_t>(str.size());
    if (validateSelectionForComment(str, str.size()) != 0
        && QMessageBox::question(QApplication::activeWindow(), QStringLiteral("XML Tools plugin"),
                                 QStringLiteral("The current selection covers part of another comment.\nUncomment process may be not "
                                                "applicable.\n\nDo you want to continue ?"))
            != QMessageBox::Yes)
        return;
    // Los comentarios que ya hay se convierten en marcadores numerados
    renumber(str, "<!{", "}**", 1, "");
    renumber(str, "**{", "}>", 1, "");
    replaceAll(str, "<!--", "<!{1}**");
    replaceAll(str, "-->", "**{1}>");
    str = "<!--" + str + "-->";
    sci(SCI_SETSEL, static_cast<uintptr_t>(start), trimmedEnd);
    replaceSelection(str, start);
}

void uncommentSelection()
{
    std::string str;
    intptr_t start = 0;
    if (!selectedText(str, start))
        return;
    if (const int err = validateSelectionForComment(str, str.size())) {
        error(QStringLiteral("Unable to uncomment the current selection.\nError code is %1.").arg(err));
        return;
    }
    replaceAll(str, "-->", "");
    replaceAll(str, "<!--", "");
    renumber(str, "<!{", "}**", -1, "<!--");
    renumber(str, "**{", "}>", -1, "-->");
    replaceSelection(str, start);
}

// --- Cierre de etiquetas (ToolsEditor.cpp: closeXMLTag) ---------------------------------------

// Al escribir el '>' de <etiqueta ...>, añade </etiqueta> tras el cursor
void closeXMLTag()
{
    constexpr intptr_t kBuffer = 511;
    constexpr size_t kMaxTagName = 516;
    const intptr_t currentPos = sci(SCI_GETCURRENTPOS);
    const intptr_t startPos = std::max<intptr_t>(0, currentPos - kBuffer);
    const intptr_t size = currentPos - startPos;
    if (size < 3)
        return;
    const std::string buf = range(startPos, currentPos);
    const char before = buf[static_cast<size_t>(size - 2)];
    if (std::strchr("/-?])%", before))
        return;
    std::string insert = "</";
    size_t cur = static_cast<size_t>(size - 2);
    while (cur > 0 && buf[cur] != '<' && buf[cur] != '>')
        --cur;
    if (buf[cur] == '<') {
        ++cur;
        if (std::strchr("/!?[%", buf[cur]))
            return;
        while (cur < buf.size() && buf[cur] != '>' && buf[cur] != ' ' && buf[cur] != '\t' && buf[cur] != '\n' && buf[cur] != '\r') {
            if (insert.size() == kMaxTagName - 1)
                return;
            insert += buf[cur++];
        }
    }
    insert += '>';
    if (insert.size() > 3) {
        sci(SCI_BEGINUNDOACTION);
        sci(SCI_REPLACESEL, 0, reinterpret_cast<intptr_t>(insert.c_str()));
        sci(SCI_SETSEL, static_cast<uintptr_t>(currentPos), currentPos);
        sci(SCI_ENDUNDOACTION);
    }
}

// --- Opciones (OptionsDlg.cpp, Config.cpp) ----------------------------------------------------

void saveOptions()
{
    QSettings ini(g_iniPath, QSettings::IniFormat);
    ini.beginGroup(QStringLiteral("XML Tools"));
    auto flag = [&](const char* key, bool value) { ini.setValue(QLatin1String(key), value ? 1 : 0); };
    flag("doCheckXML", g_doCheckXML);
    flag("doValidation", g_doValidation);
    flag("doCloseTag", g_doCloseTag);
    flag("doAutoXMLType", g_doAutoXMLType);
    flag("doPreventXXE", g_doPreventXXE);
    flag("doAllowHuge", g_doAllowHuge);
    flag("doPrettyPrintAllOpenFiles", g_doPrettyPrintAllOpenFiles);
    ini.setValue(QStringLiteral("errorDisplayMode"), g_options.errorDisplayMode);
    ini.setValue(QStringLiteral("maxErrorsNum"), g_options.maxErrorsNum);
    ini.setValue(QStringLiteral("maxIndentLevel"), g_options.maxIndentLevel);
    flag("xpathOnStatusbar", g_options.xpathOnStatusbar);
    flag("dumpAttributeName", g_options.dumpAttributeName);
    flag("printXPathIndex", g_options.printXPathIndex);
    ini.setValue(QStringLiteral("identityAttributes"), g_options.identityAttributes);
    flag("ppAutoclose", g_options.ppAutoclose);
    flag("ensureConformity", g_options.ensureConformity);
    flag("applySpacePreserve", g_options.applySpacePreserve);
}

void loadOptions()
{
    QSettings ini(g_iniPath, QSettings::IniFormat);
    ini.beginGroup(QStringLiteral("XML Tools"));
    auto flag = [&](const char* key, bool& value) { value = ini.value(QLatin1String(key), value ? 1 : 0).toInt() == 1; };
    flag("doCheckXML", g_doCheckXML);
    flag("doValidation", g_doValidation);
    flag("doCloseTag", g_doCloseTag);
    flag("doAutoXMLType", g_doAutoXMLType);
    flag("doPreventXXE", g_doPreventXXE);
    flag("doAllowHuge", g_doAllowHuge);
    flag("doPrettyPrintAllOpenFiles", g_doPrettyPrintAllOpenFiles);
    const QString mode = ini.value(QStringLiteral("errorDisplayMode"), g_options.errorDisplayMode).toString();
    if (mode == QLatin1String("Annotation") || mode == QLatin1String("Dialog") || mode == QLatin1String("Alert"))
        g_options.errorDisplayMode = mode;
    g_options.maxErrorsNum = ini.value(QStringLiteral("maxErrorsNum"), g_options.maxErrorsNum).toInt();
    g_options.maxIndentLevel = std::max(0, ini.value(QStringLiteral("maxIndentLevel"), g_options.maxIndentLevel).toInt());
    flag("xpathOnStatusbar", g_options.xpathOnStatusbar);
    flag("dumpAttributeName", g_options.dumpAttributeName);
    flag("printXPathIndex", g_options.printXPathIndex);
    g_options.identityAttributes = ini.value(QStringLiteral("identityAttributes"), g_options.identityAttributes).toString();
    flag("ppAutoclose", g_options.ppAutoclose);
    flag("ensureConformity", g_options.ensureConformity);
    flag("applySpacePreserve", g_options.applySpacePreserve);
}

// El original usa una cuadrícula de propiedades (CMFCPropertyGridCtrl); aquí, un formulario
// con las mismas opciones, en sus grupos
void optionsDlg()
{
    QDialog dialog(QApplication::activeWindow());
    dialog.setWindowTitle(QStringLiteral("XML Tools options"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto section = [&](const QString& title) {
        auto* label = new QLabel(QStringLiteral("<b>%1</b>").arg(title), &dialog);
        form->addRow(label);
    };
    auto check = [&](const QString& text, bool value, const QString& tip) {
        auto* box = new QCheckBox(text, &dialog);
        box->setChecked(value);
        box->setToolTip(tip);
        form->addRow(box);
        return box;
    };

    section(QStringLiteral("Errors"));
    auto* displayMode = new QComboBox(&dialog);
    displayMode->addItem(QStringLiteral("Annotation"), QStringLiteral("Annotation"));
    displayMode->addItem(QStringLiteral("Dialog"), QStringLiteral("Dialog"));
    displayMode->addItem(QStringLiteral("Alert"), QStringLiteral("Alert"));
    displayMode->setCurrentIndex(std::max(0, displayMode->findData(g_options.errorDisplayMode)));
    displayMode->setToolTip(QStringLiteral("How the errors of the syntax check and the validation are shown: as annotations below "
                                           "the lines, in a window, or in a message box."));
    form->addRow(QStringLiteral("Error display mode:"), displayMode);
    auto* maxErrors = new QSpinBox(&dialog);
    maxErrors->setRange(-1, 10000);
    maxErrors->setSpecialValueText(QStringLiteral("Unlimited"));
    maxErrors->setValue(g_options.maxErrorsNum);
    maxErrors->setToolTip(QStringLiteral("Maximum number of errors that are shown (-1: all)."));
    form->addRow(QStringLiteral("Maximum number of errors:"), maxErrors);

    section(QStringLiteral("Pretty print"));
    auto* maxIndent = new QSpinBox(&dialog);
    maxIndent->setRange(0, 1000);
    maxIndent->setSpecialValueText(QStringLiteral("Unlimited"));
    maxIndent->setValue(g_options.maxIndentLevel);
    maxIndent->setToolTip(QStringLiteral("Deeper nodes are not indented any more (0: no limit)."));
    form->addRow(QStringLiteral("Maximum indent level:"), maxIndent);
    QCheckBox* autoclose = check(QStringLiteral("Auto-close empty tags (<a></a> → <a/>)"), g_options.ppAutoclose,
                                 QStringLiteral("Tags without content are written as self-closing tags."));
    QCheckBox* conformity = check(QStringLiteral("Ensure conformity"), g_options.ensureConformity,
                                  QStringLiteral("Keep the whitespace that would change the meaning of the text nodes."));
    QCheckBox* spacePreserve = check(QStringLiteral("Apply xml:space=\"preserve\""), g_options.applySpacePreserve,
                                     QStringLiteral("Do not format the content of the nodes with xml:space=\"preserve\"."));

    section(QStringLiteral("XPath"));
    QCheckBox* statusbar = check(QStringLiteral("Show the current XML path in the status bar"), g_options.xpathOnStatusbar,
                                 QStringLiteral("Updated as the caret moves in XML documents."));
    QCheckBox* dumpName = check(QStringLiteral("Show the name of the identity attributes"), g_options.dumpAttributeName,
                                QStringLiteral("item[@id='1'] instead of item['1']."));
    QCheckBox* index = check(QStringLiteral("Show the node index"), g_options.printXPathIndex,
                             QStringLiteral("item[2] for the second item of its parent."));
    auto* identity = new QLineEdit(g_options.identityAttributes, &dialog);
    identity->setToolTip(QStringLiteral("Attributes that identify a node in \"Current XML Path with predicates\", separated by ';'."));
    form->addRow(QStringLiteral("Identity attributes:"), identity);

    layout->addLayout(form);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted)
        return;
    g_options.errorDisplayMode = displayMode->currentData().toString();
    g_options.maxErrorsNum = maxErrors->value();
    g_options.maxIndentLevel = maxIndent->value();
    g_options.ppAutoclose = autoclose->isChecked();
    g_options.ensureConformity = conformity->isChecked();
    g_options.applySpacePreserve = spacePreserve->isChecked();
    g_options.xpathOnStatusbar = statusbar->isChecked();
    g_options.dumpAttributeName = dumpName->isChecked();
    g_options.printXPathIndex = index->isChecked();
    g_options.identityAttributes = identity->text().trimmed();
    saveOptions();
}

// --- Menú (nppMenu.cpp) -------------------------------------------------------------------

void manualCheck() { checkSyntax(false); }
void manualValidation() { validate(false); }
void prettyPrint() { format(Format::PrettyPrint); }
void prettyPrintAttributes() { format(Format::IndentAttributes); }
void prettyPrintIndentOnly() { format(Format::IndentOnly); }
void linearize() { format(Format::Linearize); }
void tokenize() { format(Format::Tokenize); }
void currentPath() { showCurrentXPath(false); }
void currentPathPredicates() { showCurrentXPath(true); }
void about()
{
    info(QStringLiteral("XML Tools %1 for Edita\n\nPort of XML Tools for Notepad++ (morbac/xmltools, GPL v3): "
                        "the original QuickXml library with libxml2 instead of MSXML.\n%2")
             .arg(QString::fromUtf8(EDITA_PLUGIN_VERSION), QString::fromUtf8(EDITA_PLUGIN_HOMEPAGE)));
}

// Las opciones que se marcan en el menú
struct Toggle
{
    bool* value;
    int index; // en g_funcs
};
void toggle(int which);
void toggleCheckXML() { toggle(0); }
void toggleValidation() { toggle(1); }
void toggleCloseTag() { toggle(2); }
void toggleAutoXMLType() { toggle(3); }
void togglePreventXXE() { toggle(4); }
void toggleAllowHuge() { toggle(5); }
void togglePrettyPrintAllFiles() { toggle(6); }

// createShortcut: Ctrl+Alt+Mayús + letra
EditaShortcutKey g_keyM{true, true, true, 'M'};
EditaShortcutKey g_keyB{true, true, true, 'B'};
EditaShortcutKey g_keyA{true, true, true, 'A'};
EditaShortcutKey g_keyL{true, true, true, 'L'};
EditaShortcutKey g_keyP{true, true, true, 'P'};
EditaShortcutKey g_keyC{true, true, true, 'C'};
EditaShortcutKey g_keyR{true, true, true, 'R'};

EditaFuncItem g_funcs[] = {
    {"Enable XML syntax auto-check", toggleCheckXML, 0, true, nullptr},
    {"Check XML syntax now", manualCheck, 0, false, nullptr},
    {"", nullptr, 0, false, nullptr},
    {"Enable auto-validation", toggleValidation, 0, false, nullptr},
    {"Validate now", manualValidation, 0, false, &g_keyM},
    {"First error", highlightFirstError, 0, false, nullptr},
    {"Previous error", highlightPreviousError, 0, false, nullptr},
    {"Next error", highlightNextError, 0, false, nullptr},
    {"Last error", highlightLastError, 0, false, nullptr},
    {"", nullptr, 0, false, nullptr},
    {"Tag auto-close", toggleCloseTag, 0, true, nullptr},
    {"", nullptr, 0, false, nullptr},
    {"Set XML type automatically", toggleAutoXMLType, 0, true, nullptr},
    {"Prevent XXE", togglePreventXXE, 0, true, nullptr},
    {"Allow huge files", toggleAllowHuge, 0, false, nullptr},
    {"", nullptr, 0, false, nullptr},
    {"Pretty print", prettyPrint, 0, false, &g_keyB},
    {"Pretty print - indent attributes", prettyPrintAttributes, 0, false, &g_keyA},
    {"Pretty print - indent only", prettyPrintIndentOnly, 0, false, nullptr},
    {"Linearize", linearize, 0, false, &g_keyL},
    {"Apply to all open files", togglePrettyPrintAllFiles, 0, false, nullptr},
    {"Tokenize (debug)", tokenize, 0, false, nullptr},
    {"", nullptr, 0, false, nullptr},
    {"Current XML Path", currentPath, 0, false, nullptr},
    {"Current XML Path with predicates", currentPathPredicates, 0, false, &g_keyP},
    {"Evaluate XPath expression...", evaluateXPath, 0, false, nullptr},
    {"", nullptr, 0, false, nullptr},
    {"XSL Transformation...", xslTransform, 0, false, nullptr},
    {"", nullptr, 0, false, nullptr},
    {"Escape characters in selection (<> → &&lt;&&gt;)", escapeSelection, 0, false, nullptr},
    {"Unescape characters in selection (&&lt;&&gt; → <>)", unescapeSelection, 0, false, nullptr},
    {"", nullptr, 0, false, nullptr},
    {"Comment selection", commentSelection, 0, false, &g_keyC},
    {"Uncomment selection", uncommentSelection, 0, false, &g_keyR},
    {"", nullptr, 0, false, nullptr},
    {"Options...", optionsDlg, 0, false, nullptr},
    {"Debug window...", showDebugDlg, 0, false, nullptr},
    {"About XML Tools...", about, 0, false, nullptr},
};

const Toggle g_toggles[] = {
    {&g_doCheckXML, 0},  {&g_doValidation, 3},  {&g_doCloseTag, 10},
    {&g_doAutoXMLType, 12}, {&g_doPreventXXE, 13}, {&g_doAllowHuge, 14},
    {&g_doPrettyPrintAllOpenFiles, 20},
};

void toggle(int which)
{
    const Toggle& t = g_toggles[which];
    *t.value = !*t.value;
    g_data.send(g_data.host, NPPM_SETMENUITEMCHECK, static_cast<uintptr_t>(g_funcs[t.index].cmdID), *t.value);
    saveOptions();
}

bool isEditor(void* from)
{
    return from && (from == g_data.mainEditor || from == g_data.subEditor);
}

} // namespace

EDITA_PLUGIN_EXPORT int edita_plugin_api_version(void)
{
    return EDITA_PLUGIN_API_VERSION;
}

EDITA_PLUGIN_EXPORT const char* edita_plugin_version(void)
{
    return EDITA_PLUGIN_VERSION;
}

EDITA_PLUGIN_EXPORT void setInfo(EditaData data)
{
    g_data = data;
    std::string buffer(4096, '\0');
    data.send(data.host, NPPM_GETPLUGINSCONFIGDIR, buffer.size(), reinterpret_cast<intptr_t>(buffer.data()));
    g_iniPath = QDir(QString::fromUtf8(buffer.c_str())).filePath(QStringLiteral("XMLTools.ini"));
    loadOptions();
    for (const Toggle& t : g_toggles)
        g_funcs[t.index].init2Check = *t.value;
}

EDITA_PLUGIN_EXPORT const char* getName(void)
{
    return "XML Tools";
}

EDITA_PLUGIN_EXPORT EditaFuncItem* getFuncsArray(int* count)
{
    *count = static_cast<int>(sizeof g_funcs / sizeof g_funcs[0]);
    return g_funcs;
}

// XMLTools.cpp: beNotified
EDITA_PLUGIN_EXPORT void beNotified(SCNotification* n)
{
    const unsigned code = n->nmhdr.code;
    if (isEditor(n->nmhdr.hwndFrom)) {
        // Solo lo que pasa en el editor activo
        if (n->nmhdr.hwndFrom != editor())
            return;
        if (code == SCN_CHARADDED) {
            // closeXMLTag antes que lo demás
            if (setAutoXMLType() == kLangXml && g_doCloseTag && n->ch == '>')
                closeXMLTag();
        } else if (code == SCN_MODIFIED) {
            // Al cambiar el texto, las anotaciones de errores ya no corresponden
            if ((n->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) && g_errors.count(currentBufferId()))
                clearErrors(currentBufferId());
        } else if (code == SCN_UPDATEUI) {
            if (g_options.xpathOnStatusbar && (n->updated & (SC_UPDATE_SELECTION | SC_UPDATE_CONTENT))
                && setAutoXMLType() == kLangXml)
                printCurrentXPathInStatusbar();
        }
        return;
    }
    if (n->nmhdr.hwndFrom != g_data.host)
        return;
    switch (code) {
    case NPPN_BUFFERACTIVATED:
        setAutoXMLType();
        break;
    case NPPN_FILEBEFORECLOSE:
        g_errors.erase(static_cast<intptr_t>(n->nmhdr.idFrom));
        break;
    case NPPN_FILESAVED:
        // Comprobación automática al guardar un documento XML (sin avisos que bloqueen)
        if (g_data.send(g_data.host, NPPM_GETBUFFERLANGTYPE, n->nmhdr.idFrom, 0) != kLangXml)
            break;
        if (g_doValidation)
            validate(true);
        else if (g_doCheckXML)
            checkSyntax(true);
        break;
    case NPPN_SHUTDOWN:
        if (g_debugDialog)
            delete g_debugDialog.data();
        break;
    default:
        break;
    }
}

EDITA_PLUGIN_EXPORT intptr_t messageProc(unsigned int, uintptr_t, intptr_t)
{
    return 0;
}
