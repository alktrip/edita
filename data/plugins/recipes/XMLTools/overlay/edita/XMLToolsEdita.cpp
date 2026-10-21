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
// XMLTools.ini, en la carpeta de configuración de los plugins, como en N++.

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
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

#include <cstdlib>
#include <cstring>
#include <stack>
#include <string>
#include <vector>

namespace {

EditaData g_data;
QString g_iniPath;
bool g_doCheckXML = true;     // XmlToolsConfig: doCheckXML
bool g_doValidation = false;  // doValidation
bool g_doPreventXXE = true;   // doPreventXXE
constexpr int kLangXml = 9;   // L_XML
constexpr int kFileNew = 41001; // IDM_FILE_NEW

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

// --- Formateo (ToolsPrettyPrintFast.cpp, con QuickXml) -----------------------------------------

enum class Format { PrettyPrint, IndentAttributes, IndentOnly, Linearize };

void format(Format kind)
{
    if (sci(SCI_GETREADONLY))
        return;
    const WorkText w = workText();
    if (w.text.empty())
        return;
    QuickXml::XmlFormaterParamsType params;
    params.indentChars = indentChars();
    params.eolChars = eolChars();
    params.maxIndentLevel = 0; // xmltoolsoptions.maxIndentLevel
    params.ensureConformity = true;
    params.autoCloseTags = true; // ppAutoclose
    params.indentAttributes = kind == Format::IndentAttributes;
    params.indentOnly = kind == Format::IndentOnly;
    params.applySpacePreserve = false;
    QuickXml::XmlFormater formater(w.text.data(), w.text.size(), params);
    std::stringstream* out = kind == Format::Linearize ? formater.linearize() : formater.prettyPrint();
    const std::string result = out->str();
    if (result != w.text) {
        sci(SCI_BEGINUNDOACTION);
        replaceWork(w, result);
        sci(SCI_ENDUNDOACTION);
    }
}

// --- Ruta actual (ToolsXPath.cpp: currentXPath) ----------------------------------------------

QString currentXPath(bool precise)
{
    const std::string text = documentText();
    QuickXml::XmlFormaterParamsType params = QuickXml::XmlFormater::getDefaultParams();
    int mode = precise ? (XPATH_MODE_WITHNAMESPACE | XPATH_MODE_KEEPIDATTRIBUTE) : XPATH_MODE_BASIC;
    if (precise) {
        params.identityAttribues = {"id", "name"}; // identityAttributes = "id;name"
        params.dumpIdAttributesName = true;
    }
    QuickXml::XmlFormater formater(text.data(), text.size(), params);
    return QString::fromStdString(formater.currentPath(static_cast<size_t>(sci(SCI_GETCURRENTPOS)), mode)->str());
}

void showCurrentXPath(bool precise)
{
    const QString path = currentXPath(precise);
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
    // doPreventXXE: sin red y sin sustituir entidades externas
    int options = XML_PARSE_NONET;
    if (loadDtd)
        options |= XML_PARSE_DTDLOAD | XML_PARSE_DTDATTR;
    if (!g_doPreventXXE)
        options &= ~XML_PARSE_NONET;
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

// Report de errores: el primero se selecciona; todos en la barra de estado y en un aviso
void reportErrors(const std::vector<XmlError>& errors, const QString& what, bool silent)
{
    const XmlError& first = errors.front();
    if (first.line > 0) {
        const intptr_t pos = sci(SCI_POSITIONFROMLINE, static_cast<uintptr_t>(first.line - 1));
        sci(SCI_ENSUREVISIBLEENFORCEPOLICY, static_cast<uintptr_t>(first.line - 1));
        sci(SCI_GOTOPOS, static_cast<uintptr_t>(pos + std::max(0, first.column - 1)));
    }
    setStatus(QStringLiteral("XML Tools: %1 – line %2: %3").arg(what).arg(first.line).arg(first.message));
    if (silent)
        return;
    QStringList lines;
    for (size_t i = 0; i < errors.size() && i < 20; ++i)
        lines << QStringLiteral("Line %1, column %2: %3").arg(errors[i].line).arg(errors[i].column).arg(errors[i].message);
    if (errors.size() > 20)
        lines << QStringLiteral("… (%1 errores en total)").arg(errors.size());
    error(what + QStringLiteral(":\n\n") + lines.join(QLatin1Char('\n')));
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
        if (!silent)
            info(QStringLiteral("No DTD or schema to validate against."));
        return true;
    }
    if (valid && collector.errors.empty()) {
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
        reportErrors(collector.errors.empty() ? std::vector<XmlError>{{0, 0, QStringLiteral("document is not XML")}} : collector.errors,
                     QStringLiteral("XML Parsing error"), false);
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

// --- Menú (nppMenu.cpp) -------------------------------------------------------------------

void toggleCheckXML();
void toggleValidation();
void manualCheck() { checkSyntax(false); }
void manualValidation() { validate(false); }
void prettyPrint() { format(Format::PrettyPrint); }
void prettyPrintAttributes() { format(Format::IndentAttributes); }
void prettyPrintIndentOnly() { format(Format::IndentOnly); }
void linearize() { format(Format::Linearize); }
void currentPath() { showCurrentXPath(false); }
void currentPathPredicates() { showCurrentXPath(true); }
void about()
{
    info(QStringLiteral("XML Tools %1 for Edita\n\nPort of XML Tools for Notepad++ (morbac/xmltools, GPL v3): "
                        "the original QuickXml library with libxml2 instead of MSXML.\n%2")
             .arg(QString::fromUtf8(EDITA_PLUGIN_VERSION), QString::fromUtf8(EDITA_PLUGIN_HOMEPAGE)));
}

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
    {"", nullptr, 0, false, nullptr},
    {"Pretty print", prettyPrint, 0, false, &g_keyB},
    {"Pretty print - indent attributes", prettyPrintAttributes, 0, false, &g_keyA},
    {"Pretty print - indent only", prettyPrintIndentOnly, 0, false, nullptr},
    {"Linearize", linearize, 0, false, &g_keyL},
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
    {"About XML Tools...", about, 0, false, nullptr},
};
constexpr int kCheckIndex = 0;
constexpr int kValidationIndex = 3;

void saveOptions()
{
    QSettings ini(g_iniPath, QSettings::IniFormat);
    ini.beginGroup(QStringLiteral("XML Tools"));
    ini.setValue(QStringLiteral("doCheckXML"), g_doCheckXML ? 1 : 0);
    ini.setValue(QStringLiteral("doValidation"), g_doValidation ? 1 : 0);
    ini.setValue(QStringLiteral("doPreventXXE"), g_doPreventXXE ? 1 : 0);
}

void toggleCheckXML()
{
    g_doCheckXML = !g_doCheckXML;
    g_data.send(g_data.host, NPPM_SETMENUITEMCHECK, static_cast<uintptr_t>(g_funcs[kCheckIndex].cmdID), g_doCheckXML);
    saveOptions();
}

void toggleValidation()
{
    g_doValidation = !g_doValidation;
    g_data.send(g_data.host, NPPM_SETMENUITEMCHECK, static_cast<uintptr_t>(g_funcs[kValidationIndex].cmdID), g_doValidation);
    saveOptions();
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
    QSettings ini(g_iniPath, QSettings::IniFormat);
    ini.beginGroup(QStringLiteral("XML Tools"));
    g_doCheckXML = ini.value(QStringLiteral("doCheckXML"), 1).toInt() == 1;
    g_doValidation = ini.value(QStringLiteral("doValidation"), 0).toInt() == 1;
    g_doPreventXXE = ini.value(QStringLiteral("doPreventXXE"), 1).toInt() == 1;
    g_funcs[kCheckIndex].init2Check = g_doCheckXML;
    g_funcs[kValidationIndex].init2Check = g_doValidation;
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

EDITA_PLUGIN_EXPORT void beNotified(SCNotification* n)
{
    // Comprobación automática al guardar un documento XML (sin avisos que bloqueen)
    if (n->nmhdr.hwndFrom != g_data.host || n->nmhdr.code != NPPN_FILESAVED)
        return;
    if (g_data.send(g_data.host, NPPM_GETBUFFERLANGTYPE, n->nmhdr.idFrom, 0) != kLangXml)
        return;
    if (g_doValidation)
        validate(true);
    else if (g_doCheckXML)
        checkSyntax(true);
}

EDITA_PLUGIN_EXPORT intptr_t messageProc(unsigned int, uintptr_t, intptr_t)
{
    return 0;
}
