// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Port de MarkdownViewer++ 0.8.2 (https://github.com/nea/MarkdownViewerPlusPlus,
// Savas Ziplies, MIT) a Edita. El original está escrito en C# (.NET, WinForms) y no
// se puede compilar en Linux, así que este es un port nativo con su comportamiento:
// el menú (MarkdownViewer.cs: CommandMenuInit), el panel acoplable con su barra de
// menús (AbstractRenderer.cs), la exportación a HTML y PDF, el envío a la impresora y
// al portapapeles, las opciones (MarkdownViewerConfiguration.cs) en
// MarkdownViewerPlusPlus.ini y el desplazamiento sincronizado.
//
// Donde el original usa Markdig (CommonMark con extensiones avanzadas) se usa md4c
// (CommonMark con las extensiones de GitHub); HtmlRenderer se sustituye por
// QTextBrowser y PdfSharp por QPdfWriter.

#include <edita_plugin.h>

#include <md4c-html.h>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimer>
#include <QTranslator>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

#include <algorithm>
#include <string>

namespace {

constexpr const char* kPluginName = "MarkdownViewer++";
constexpr const char* kModuleName = "MarkdownViewerPlusPlus.so";
constexpr const char* kVersion = "0.8.2";
constexpr const char* kSection = "MarkdownViewerPlusPlus"; // la sección del .ini (el nombre del ensamblado)

// --- Opciones (MarkdownViewerConfiguration.cs) --------------------------------------------------

struct Options
{
    bool synchronizeScrolling = false;
    QString fileExtensions;    // vacío: todos los archivos
    bool inclNewFiles = true;  // los documentos sin título, si hay extensiones
    QString htmlCssStyle;      // CSS propio, añadido al de serie
    QString pdfOrientation = QStringLiteral("Portrait");
    QString pdfPageSize = QStringLiteral("A4");
    QString margins = QStringLiteral("5,5,5,5"); // izquierdo, superior, derecho, inferior (mm)
    bool pdfOpenExport = false;
    bool htmlOpenExport = false;

    QList<int> marginList() const
    {
        QList<int> values;
        for (const QString& part : margins.split(QLatin1Char(','))) {
            bool ok = false;
            const int value = part.trimmed().toInt(&ok);
            if (!ok)
                return {5, 5, 5, 5}; // GetMargins: 5 mm si no se entiende
            values << value;
        }
        return values.size() == 4 ? values : QList<int>{5, 5, 5, 5};
    }

    // ValidateFileExtension (con la misma comparación por subcadena del original)
    bool validateFile(const QString& extension, bool isNew) const
    {
        if (fileExtensions.isEmpty())
            return true;
        if (extension.isEmpty())
            return inclNewFiles && isNew;
        return fileExtensions.toLower().contains(extension.toLower());
    }
};

// Los tamaños de página de PdfSharp que ofrece el original, con su equivalente en Qt
struct PageSizeName
{
    const char* name;
    QPageSize::PageSizeId id;
};
constexpr PageSizeName kPageSizes[] = {
    {"A0", QPageSize::A0}, {"A1", QPageSize::A1}, {"A2", QPageSize::A2}, {"A3", QPageSize::A3},
    {"A4", QPageSize::A4}, {"A5", QPageSize::A5}, {"B4", QPageSize::B4}, {"B5", QPageSize::B5},
    {"Letter", QPageSize::Letter}, {"Legal", QPageSize::Legal}, {"Ledger", QPageSize::Ledger},
    {"Tabloid", QPageSize::Tabloid}, {"Executive", QPageSize::Executive}, {"Folio", QPageSize::Folio},
};

QPageSize::PageSizeId pageSizeId(const QString& name)
{
    for (const PageSizeName& size : kPageSizes) {
        if (name.compare(QLatin1String(size.name), Qt::CaseInsensitive) == 0)
            return size.id;
    }
    return QPageSize::A4;
}

// --- Estado ---------------------------------------------------------------------------------

class Panel;

struct State
{
    EditaData data{};
    QString iniPath;
    Options options;
    QPointer<Panel> panel;
    bool needsRender = true;
    QString markdown;     // RawText
    QString html;         // ConvertedText (el cuerpo, sin la plantilla)
    QString fileName;     // FileInfo.FileName
    QString fileDirectory;
    bool fileIsNew = false;
};
State g;

intptr_t host(unsigned message, uintptr_t w = 0, intptr_t l = 0)
{
    return g.data.send(g.data.host, message, w, l);
}

void* editor()
{
    int view = 0;
    host(NPPM_GETCURRENTSCINTILLA, 0, reinterpret_cast<intptr_t>(&view));
    return view == 1 ? g.data.subEditor : g.data.mainEditor;
}

intptr_t sci(unsigned message, uintptr_t w = 0, intptr_t l = 0)
{
    return g.data.send(editor(), message, w, l);
}

QString hostString(unsigned message)
{
    std::string buffer(4096, '\0');
    host(message, buffer.size(), reinterpret_cast<intptr_t>(buffer.data()));
    return QString::fromUtf8(buffer.c_str());
}

// El .ini como lo leen y escriben GetPrivateProfileString/WritePrivateProfileString: los
// valores tal cual (QSettings toma «;» como comentario, «\n» como escape y las comas
// como listas, y el CSS propio tiene las tres cosas)
using IniSection = QList<QPair<QString, QString>>;

QMap<QString, IniSection> readIni(const QString& path, QStringList* order)
{
    QMap<QString, IniSection> sections;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return sections;
    QString current;
    for (const QString& raw : QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'))) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char(';')) || line.startsWith(QLatin1Char('#')))
            continue;
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            current = line.mid(1, line.size() - 2).trimmed();
            if (order && !order->contains(current))
                *order << current;
            continue;
        }
        const qsizetype equals = line.indexOf(QLatin1Char('='));
        if (equals > 0 && !current.isEmpty())
            sections[current].append({line.left(equals).trimmed(), line.mid(equals + 1).trimmed()});
    }
    return sections;
}

void loadOptions()
{
    const IniSection section = readIni(g.iniPath, nullptr).value(QLatin1String(kSection));
    QHash<QString, QString> values;
    for (const auto& [key, value] : section)
        values.insert(key, value);
    Options& o = g.options;
    auto flag = [&](const char* key, bool& value) {
        if (values.contains(QLatin1String(key)))
            value = values.value(QLatin1String(key)).toInt() != 0;
    };
    auto text = [&](const char* key, QString& value) {
        if (values.contains(QLatin1String(key)))
            value = values.value(QLatin1String(key));
    };
    flag("synchronizeScrolling", o.synchronizeScrolling);
    text("fileExtensions", o.fileExtensions);
    flag("inclNewFiles", o.inclNewFiles);
    text("htmlCssStyle", o.htmlCssStyle);
    // Como el original, los saltos de línea del CSS se guardan como " \n "
    o.htmlCssStyle.replace(QStringLiteral(" \\n "), QStringLiteral("\n"));
    text("pdfOrientation", o.pdfOrientation);
    text("pdfPageSize", o.pdfPageSize);
    text("margins", o.margins);
    flag("pdfOpenExport", o.pdfOpenExport);
    flag("htmlOpenExport", o.htmlOpenExport);
}

void saveOptions()
{
    // Las demás secciones del archivo se conservan
    QStringList order;
    QMap<QString, IniSection> sections = readIni(g.iniPath, &order);
    const Options& o = g.options;
    QString css = o.htmlCssStyle;
    css.replace(QLatin1Char('\n'), QStringLiteral(" \\n "));
    auto flag = [](bool value) { return value ? QStringLiteral("1") : QStringLiteral("0"); };
    sections[QLatin1String(kSection)] = {
        {QStringLiteral("synchronizeScrolling"), flag(o.synchronizeScrolling)},
        {QStringLiteral("fileExtensions"), o.fileExtensions},
        {QStringLiteral("inclNewFiles"), flag(o.inclNewFiles)},
        {QStringLiteral("htmlCssStyle"), css},
        {QStringLiteral("pdfOrientation"), o.pdfOrientation},
        {QStringLiteral("pdfPageSize"), o.pdfPageSize},
        {QStringLiteral("margins"), o.margins},
        {QStringLiteral("pdfOpenExport"), flag(o.pdfOpenExport)},
        {QStringLiteral("htmlOpenExport"), flag(o.htmlOpenExport)},
    };
    if (!order.contains(QLatin1String(kSection)))
        order << QLatin1String(kSection);
    QString out;
    for (const QString& name : order) {
        out += QLatin1Char('[') + name + QStringLiteral("]\n");
        for (const auto& [key, value] : sections.value(name))
            out += key + QLatin1Char('=') + value + QLatin1Char('\n');
    }
    QDir().mkpath(QFileInfo(g.iniPath).absolutePath());
    QFile file(g.iniPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        file.write(out.toUtf8());
}

// --- Markdown → HTML (md4c) ---------------------------------------------------------------------

void appendHtml(const MD_CHAR* text, MD_SIZE size, void* userData)
{
    static_cast<std::string*>(userData)->append(text, size);
}

// Markdown.ToHtml: CommonMark con tablas, tachado, listas de tareas y enlaces automáticos
QString markdownToHtml(const QString& markdown)
{
    const QByteArray input = markdown.toUtf8();
    std::string out;
    md_html(input.constData(), static_cast<MD_SIZE>(input.size()), &appendHtml, &out,
            MD_DIALECT_GITHUB, 0);
    return QString::fromUtf8(out.data(), static_cast<qsizetype>(out.size()));
}

// El CSS de serie para exportar (el del original solo evita los saltos de página)
QString baseCss()
{
    return QStringLiteral(
        "/* Avoid page breaks inside the most common attributes, especially for exports (i.e. PDF) */\n"
        "td, h1, h2, h3, h4, h5, p, ul, ol, li { page-break-inside: avoid; }\n"
        "body { font-family: sans-serif; line-height: 1.5; color: #1f2328; max-width: 52em; margin: 0 auto; padding: 1em; }\n"
        "h1, h2 { border-bottom: 1px solid #d8dee4; padding-bottom: .3em; }\n"
        "code, pre { font-family: monospace; background: #f6f8fa; border-radius: 4px; }\n"
        "code { padding: .1em .3em; }\n"
        "pre { padding: .8em; overflow: auto; }\n"
        "pre code { padding: 0; }\n"
        "blockquote { margin: 0; padding: 0 1em; color: #59636e; border-left: .25em solid #d1d9e0; }\n"
        "table { border-collapse: collapse; }\n"
        "th, td { border: 1px solid #d1d9e0; padding: .3em .8em; }\n"
        "img { max-width: 100%; }\n"
        "a { color: #0969da; }\n");
}

// getCSS: los @import del CSS propio delante, después el de serie y después el resto
QString css()
{
    static const QRegularExpression imports(QStringLiteral("@import[^;]*;"));
    QString custom = g.options.htmlCssStyle;
    QStringList importLines;
    for (auto it = imports.globalMatch(custom); it.hasNext();)
        importLines << it.next().captured(0);
    custom.remove(imports);
    return importLines.join(QLatin1Char('\n')) + QLatin1Char('\n') + baseCss() + QLatin1Char('\n') + custom.trimmed();
}

QString escapeHtml(const QString& text)
{
    return text.toHtmlEscaped();
}

// BuildHtml: la página completa, para exportar, imprimir y copiar
QString buildHtml(const QString& body, QString title)
{
    if (title.isEmpty())
        title = QLatin1String(kPluginName);
    return QStringLiteral("<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"UTF-8\" />\n"
                          "<meta name=\"author\" content=\"%1\" />\n<title>%2</title>\n"
                          "<style type=\"text/css\">\n%3\n</style>\n</head>\n<body>\n%4\n</body>\n</html>\n")
        .arg(QLatin1String(kPluginName), escapeHtml(title), css(), body);
}

// QTextBrowser no dibuja <input type="checkbox">: las listas de tareas, con ☐ y ☑
QString forTextBrowser(QString html)
{
    static const QRegularExpression checked(QStringLiteral("<input[^>]*type=\"checkbox\"[^>]*checked[^>]*>"));
    static const QRegularExpression unchecked(QStringLiteral("<input[^>]*type=\"checkbox\"[^>]*>"));
    html.replace(checked, QStringLiteral("&#9745; "));
    html.replace(unchecked, QStringLiteral("&#9744; "));
    return html;
}

// El CSS del panel: el subconjunto que entiende QTextBrowser, con los colores de la
// interfaz (tema claro u oscuro)
QString panelCss(const QPalette& palette)
{
    const QColor text = palette.color(QPalette::Text);
    const QColor base = palette.color(QPalette::Base);
    const bool dark = base.lightness() < 128;
    const QString code = (dark ? base.lighter(140) : base.darker(106)).name();
    const QString border = (dark ? base.lighter(180) : base.darker(125)).name();
    const QString quote = QColor::fromRgbF(text.redF() * .6 + base.redF() * .4, text.greenF() * .6 + base.greenF() * .4,
                                           text.blueF() * .6 + base.blueF() * .4)
                              .name();
    return QStringLiteral("code, pre { font-family: monospace; background-color: %1; }\n"
                          "pre { padding: 6px; }\n"
                          "blockquote { color: %2; margin-left: 12px; }\n"
                          "table { border-collapse: collapse; }\n"
                          "th, td { border: 1px solid %3; padding: 3px 8px; }\n"
                          "a { color: %4; }\n")
        .arg(code, quote, border, palette.color(QPalette::Link).name());
}

QString exportBaseName()
{
    QString base = QFileInfo(g.fileName).completeBaseName();
    return base.isEmpty() ? QStringLiteral("markdown") : base;
}

QString exportDirectory()
{
    return g.fileDirectory.isEmpty() || g.fileIsNew ? QDir::homePath() : g.fileDirectory;
}

// --- Exportar (AbstractRenderer.cs) ---------------------------------------------------------------

bool writeHtml(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(buildHtml(g.html, g.fileName).toUtf8());
    return true;
}

// PdfGenerator.GeneratePdf: tamaño de página, orientación y márgenes de las opciones
bool writePdf(const QString& path)
{
    QPdfWriter writer(path);
    if (!writer.setPageSize(QPageSize(pageSizeId(g.options.pdfPageSize))))
        return false;
    writer.setPageOrientation(g.options.pdfOrientation.compare(QLatin1String("Landscape"), Qt::CaseInsensitive) == 0
                                  ? QPageLayout::Landscape
                                  : QPageLayout::Portrait);
    const QList<int> m = g.options.marginList();
    writer.setPageMargins(QMarginsF(m[0], m[1], m[2], m[3]), QPageLayout::Millimeter);
    writer.setTitle(g.fileName);
    writer.setCreator(QLatin1String(kPluginName));
    QTextDocument document;
    document.setBaseUrl(QUrl::fromLocalFile(exportDirectory() + QLatin1Char('/')));
    document.setDefaultStyleSheet(css());
    document.setHtml(forTextBrowser(buildHtml(g.html, g.fileName)));
    document.print(&writer);
    return QFileInfo(path).size() > 0;
}

void openFile(const QString& path)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void exportAsHtml()
{
    const QString path = QFileDialog::getSaveFileName(QApplication::activeWindow(), QCoreApplication::translate("MarkdownViewer", "Export as HTML"),
                                                      QDir(exportDirectory()).filePath(exportBaseName() + QStringLiteral(".html")),
                                                      QCoreApplication::translate("MarkdownViewer", "HTML files (*.html);;All files (*)"));
    if (path.isEmpty())
        return;
    if (!writeHtml(path)) {
        QMessageBox::warning(QApplication::activeWindow(), QLatin1String(kPluginName), QCoreApplication::translate("MarkdownViewer", "Cannot write %1").arg(path));
        return;
    }
    if (g.options.htmlOpenExport)
        openFile(path);
}

void exportAsPdf()
{
    const QString path = QFileDialog::getSaveFileName(QApplication::activeWindow(), QCoreApplication::translate("MarkdownViewer", "Export as PDF"),
                                                      QDir(exportDirectory()).filePath(exportBaseName() + QStringLiteral(".pdf")),
                                                      QCoreApplication::translate("MarkdownViewer", "PDF files (*.pdf);;All files (*)"));
    if (path.isEmpty())
        return;
    if (!writePdf(path)) {
        QMessageBox::warning(QApplication::activeWindow(), QLatin1String(kPluginName), QCoreApplication::translate("MarkdownViewer", "Cannot write %1").arg(path));
        return;
    }
    if (g.options.pdfOpenExport)
        openFile(path);
}

// sendToPrinter: vista previa de impresión
void sendToPrinter()
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(g.fileName);
    QPrintPreviewDialog preview(&printer, QApplication::activeWindow());
    QObject::connect(&preview, &QPrintPreviewDialog::paintRequested, &preview, [](QPrinter* target) {
        QTextDocument document;
        document.setBaseUrl(QUrl::fromLocalFile(exportDirectory() + QLatin1Char('/')));
        document.setDefaultStyleSheet(css());
        document.setHtml(forTextBrowser(buildHtml(g.html, g.fileName)));
        document.print(target);
    });
    preview.exec();
}

// sendToClipboard: el HTML completo y, como texto, el HTML convertido
void sendToClipboard()
{
    auto* mime = new QMimeData;
    mime->setHtml(buildHtml(g.html, g.fileName));
    mime->setText(g.html);
    QGuiApplication::clipboard()->setMimeData(mime);
}

// sendAsTextMail: el original abre un correo de Outlook; aquí, el cliente de correo
// del sistema (mailto:) con el texto Markdown
void sendAsTextMail()
{
    QUrl url(QStringLiteral("mailto:"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("subject"), g.fileName);
    query.addQueryItem(QStringLiteral("body"), g.markdown);
    url.setQuery(query);
    QDesktopServices::openUrl(url);
}

// --- El panel (MarkdownViewerRenderer.cs) ---------------------------------------------------------

void renderMarkdown();
void updateViewer(bool updateScrollBar, bool updateRenderer);
void updateEditorInformation();
void optionsCommand();
void aboutCommand();
void setPanelChecked(bool checked);

class Panel : public QWidget
{
public:
    Panel()
    {
        setObjectName(QStringLiteral("MarkdownViewerPanel"));
        setWindowTitle(QLatin1String(kPluginName));
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        // markdownViewerMenuStrip
        auto* menu = new QMenuBar(this);
        menu->setNativeMenuBar(false);
        QMenu* exportAs = menu->addMenu(QCoreApplication::translate("MarkdownViewer", "&Export as"));
        exportAs->addAction(QCoreApplication::translate("MarkdownViewer", "&HTML"), &exportAsHtml);
        exportAs->addAction(QCoreApplication::translate("MarkdownViewer", "&PDF"), &exportAsPdf);
        QMenu* send = menu->addMenu(QCoreApplication::translate("MarkdownViewer", "&Send"));
        send->addAction(QCoreApplication::translate("MarkdownViewer", "as &Text E-mail"), &sendAsTextMail);
        send->addAction(QCoreApplication::translate("MarkdownViewer", "to &Printer"), &sendToPrinter);
        send->addAction(QCoreApplication::translate("MarkdownViewer", "HTML to &Clipboard"), &sendToClipboard);
        QMenu* view = menu->addMenu(QCoreApplication::translate("MarkdownViewer", "&View"));
        view->addAction(QCoreApplication::translate("MarkdownViewer", "&Refresh"), [] { updateViewer(false, true); });
        QMenu* help = menu->addMenu(QStringLiteral("&?"));
        help->addAction(QCoreApplication::translate("MarkdownViewer", "&Options"), &optionsCommand);
        help->addAction(QCoreApplication::translate("MarkdownViewer", "&About"), &aboutCommand);
        layout->addWidget(menu);

        _browser = new QTextBrowser(this);
        _browser->setObjectName(QStringLiteral("MarkdownViewerBrowser"));
        _browser->setOpenLinks(false);
        _browser->setFrameShape(QFrame::NoFrame);
        _browser->document()->setDocumentMargin(10);
        QObject::connect(_browser, &QTextBrowser::anchorClicked, _browser, [this](const QUrl& url) { openLink(url); });
        layout->addWidget(_browser, 1);

        _timer.setSingleShot(true);
        _timer.setInterval(150);
        QObject::connect(&_timer, &QTimer::timeout, _browser, [] { renderMarkdown(); });
    }

    QTextBrowser* browser() const { return _browser; }
    void scheduleRender() { _timer.start(); }

    void setHtml(const QString& html)
    {
        // Mantiene la posición al volver a dibujar mientras se escribe
        QScrollBar* bar = _browser->verticalScrollBar();
        const double ratio = bar->maximum() > 0 ? double(bar->value()) / bar->maximum() : 0.0;
        _browser->document()->setDefaultStyleSheet(panelCss(_browser->palette()));
        _browser->document()->setBaseUrl(QUrl::fromLocalFile(g.fileDirectory + QLatin1Char('/')));
        _browser->setHtml(forTextBrowser(html));
        bar->setValue(int(ratio * bar->maximum()));
    }

    // ScrollByRatioVertically
    void scrollByRatio(double ratio)
    {
        QScrollBar* bar = _browser->verticalScrollBar();
        bar->setValue(int(std::clamp(ratio, 0.0, 1.0) * bar->maximum()));
    }

protected:
    void showEvent(QShowEvent* event) override
    {
        QWidget::showEvent(event);
        setPanelChecked(true);
    }
    void hideEvent(QHideEvent* event) override
    {
        QWidget::hideEvent(event);
        // Cerrado con la X del panel: el menú deja de estar marcado
        if (!window()->isHidden())
            setPanelChecked(false);
    }

private:
    void openLink(const QUrl& url)
    {
        if (url.scheme().isEmpty() && url.path().isEmpty() && !url.fragment().isEmpty()) {
            _browser->scrollToAnchor(url.fragment());
            return;
        }
        const QUrl resolved = QUrl::fromLocalFile(g.fileDirectory + QLatin1Char('/')).resolved(url);
        if (resolved.isLocalFile() && QFileInfo(resolved.toLocalFile()).isFile()) {
            // Un enlace a otro archivo: se abre en Edita
            const QByteArray path = resolved.toLocalFile().toUtf8();
            host(NPPM_DOOPEN, 0, reinterpret_cast<intptr_t>(path.constData()));
            return;
        }
        QDesktopServices::openUrl(resolved);
    }

    QTextBrowser* _browser = nullptr;
    QTimer _timer;
};

extern EditaFuncItem g_funcs[];
constexpr int kToggleIndex = 0;
constexpr int kSyncIndex = 2;

void setPanelChecked(bool checked)
{
    host(NPPM_SETMENUITEMCHECK, static_cast<uintptr_t>(g_funcs[kToggleIndex].cmdID), checked);
}

// Visible en su ventana (aunque la ventana aún no se haya mostrado)
bool panelVisible()
{
    return g.panel && g.panel->isVisibleTo(g.panel->window());
}

// UpdateEditorInformation
void updateEditorInformation()
{
    const QString full = hostString(NPPM_GETFULLCURRENTPATH);
    // Un documento sin título no tiene carpeta («new 1»)
    g.fileIsNew = !full.contains(QLatin1Char('/'));
    g.fileName = g.fileIsNew ? full : QFileInfo(full).fileName();
    g.fileDirectory = g.fileIsNew ? QString() : QFileInfo(full).absolutePath();
}

// UpdateMarkdownViewer
void renderMarkdown()
{
    if (!g.panel)
        return;
    updateEditorInformation();
    const QString extension = g.fileIsNew ? QString() : QFileInfo(g.fileName).suffix();
    if (!g.options.validateFile(extension, g.fileIsNew)) {
        g.panel->setHtml(QCoreApplication::translate("MarkdownViewer", "<p>Your configuration settings do not include the currently selected file extension.<br />"
                            "The rendered file extensions are <b>'%1'</b>.<br />The current file is <i>'%2'</i>.</p>")
                             .arg(escapeHtml(g.options.fileExtensions), escapeHtml(g.fileName)));
        return;
    }
    const intptr_t length = sci(SCI_GETLENGTH);
    std::string text(static_cast<size_t>(length) + 1, '\0');
    sci(SCI_GETTEXT, text.size(), reinterpret_cast<intptr_t>(text.data()));
    text.resize(static_cast<size_t>(length));
    g.markdown = QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
    g.html = markdownToHtml(g.markdown);
    g.needsRender = false;
    g.panel->setHtml(g.html);
}

// UpdateScrollBar: la proporción desplazada del editor, en el panel
void updateScrollBar()
{
    if (!g.panel)
        return;
    const intptr_t first = sci(SCI_GETFIRSTVISIBLELINE);
    const intptr_t onScreen = sci(SCI_LINESONSCREEN);
    const intptr_t total = sci(SCI_VISIBLEFROMDOCLINE, static_cast<uintptr_t>(sci(SCI_GETLINECOUNT)));
    const intptr_t range = total - onScreen;
    g.panel->scrollByRatio(range > 0 ? double(first) / double(range) : 0.0);
}

void updateViewer(bool updateScrollBar_, bool updateRenderer)
{
    if (!g.panel)
        return;
    g.needsRender = g.needsRender || updateRenderer;
    if (g.needsRender)
        g.panel->scheduleRender();
    if (g.options.synchronizeScrolling && updateScrollBar_)
        updateScrollBar();
}

// --- Órdenes del menú (MarkdownViewer.cs) ---------------------------------------------------------

void markdownViewerCommand()
{
    if (!g.panel) {
        g.panel = new Panel;
        EditaDockData dock{};
        dock.hClient = g.panel.data();
        dock.pszName = kPluginName;
        dock.dlgID = 0;
        dock.uMask = DWS_DF_CONT_RIGHT;
        dock.pszModuleName = kModuleName;
        host(NPPM_DMMREGASDCKDLG, 0, reinterpret_cast<intptr_t>(&dock));
    }
    const bool show = !panelVisible();
    host(show ? NPPM_DMMSHOW : NPPM_DMMHIDE, 0, reinterpret_cast<intptr_t>(g.panel.data()));
    setPanelChecked(show);
    if (show) {
        g.needsRender = true;
        renderMarkdown();
        if (g.options.synchronizeScrolling)
            updateScrollBar();
        sci(SCI_GRABFOCUS);
    }
}

void synchronizeScrollingCommand()
{
    g.options.synchronizeScrolling = !g.options.synchronizeScrolling;
    host(NPPM_SETMENUITEMCHECK, static_cast<uintptr_t>(g_funcs[kSyncIndex].cmdID), g.options.synchronizeScrolling);
    saveOptions();
    if (g.options.synchronizeScrolling)
        updateScrollBar();
}

// MarkdownViewerOptions: General, HTML y PDF, como el original
void optionsCommand()
{
    QDialog dialog(QApplication::activeWindow());
    dialog.setWindowTitle(QCoreApplication::translate("MarkdownViewer", "MarkdownViewer++ Options"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* tabs = new QTabWidget(&dialog);
    layout->addWidget(tabs);

    // General (OptionsPanelGeneral)
    auto* general = new QWidget;
    auto* generalLayout = new QVBoxLayout(general);
    auto* files = new QGroupBox(QCoreApplication::translate("MarkdownViewer", "Files"), general);
    auto* filesForm = new QFormLayout(files);
    auto* extensions = new QLineEdit(g.options.fileExtensions, files);
    extensions->setPlaceholderText(QCoreApplication::translate("MarkdownViewer", "md,markdown,txt (empty: all files)"));
    extensions->setToolTip(QCoreApplication::translate("MarkdownViewer", "Only files with these extensions are rendered. Leave it empty to render all files."));
    filesForm->addRow(QCoreApplication::translate("MarkdownViewer", "File extensions"), extensions);
    auto* newFiles = new QCheckBox(QCoreApplication::translate("MarkdownViewer", "Include new files?"), files);
    newFiles->setChecked(g.options.inclNewFiles);
    newFiles->setToolTip(QCoreApplication::translate("MarkdownViewer", "Also render new documents that have not been saved yet."));
    filesForm->addRow(newFiles);
    generalLayout->addWidget(files);
    generalLayout->addStretch(1);
    tabs->addTab(general, QCoreApplication::translate("MarkdownViewer", "General"));

    // HTML (OptionsPanelHTML)
    auto* html = new QWidget;
    auto* htmlLayout = new QVBoxLayout(html);
    auto* styles = new QGroupBox(QCoreApplication::translate("MarkdownViewer", "Styles"), html);
    auto* stylesLayout = new QVBoxLayout(styles);
    stylesLayout->addWidget(new QLabel(QCoreApplication::translate("MarkdownViewer", "Custom CSS"), styles));
    auto* cssEdit = new QPlainTextEdit(g.options.htmlCssStyle, styles);
    cssEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    cssEdit->setToolTip(QCoreApplication::translate("MarkdownViewer", "Added after the default style of the exported HTML and PDF files."));
    stylesLayout->addWidget(cssEdit);
    htmlLayout->addWidget(styles, 1);
    auto* htmlExport = new QGroupBox(QCoreApplication::translate("MarkdownViewer", "Export"), html);
    auto* htmlExportLayout = new QVBoxLayout(htmlExport);
    auto* openHtml = new QCheckBox(QCoreApplication::translate("MarkdownViewer", "Open HTML after export"), htmlExport);
    openHtml->setChecked(g.options.htmlOpenExport);
    htmlExportLayout->addWidget(openHtml);
    htmlLayout->addWidget(htmlExport);
    tabs->addTab(html, QCoreApplication::translate("MarkdownViewer", "HTML"));

    // PDF (OptionsPanelPDF)
    auto* pdf = new QWidget;
    auto* pdfLayout = new QVBoxLayout(pdf);
    auto* pdfExport = new QGroupBox(QCoreApplication::translate("MarkdownViewer", "Export"), pdf);
    auto* pdfForm = new QFormLayout(pdfExport);
    auto* orientation = new QComboBox(pdfExport);
    orientation->addItem(QCoreApplication::translate("MarkdownViewer", "Portrait"), QStringLiteral("Portrait"));
    orientation->addItem(QCoreApplication::translate("MarkdownViewer", "Landscape"), QStringLiteral("Landscape"));
    orientation->setCurrentIndex(std::max(0, orientation->findData(g.options.pdfOrientation)));
    pdfForm->addRow(QCoreApplication::translate("MarkdownViewer", "Orientation"), orientation);
    auto* pageSize = new QComboBox(pdfExport);
    for (const PageSizeName& size : kPageSizes)
        pageSize->addItem(QLatin1String(size.name));
    pageSize->setCurrentIndex(std::max(0, pageSize->findText(g.options.pdfPageSize, Qt::MatchFixedString)));
    pdfForm->addRow(QCoreApplication::translate("MarkdownViewer", "Page size"), pageSize);
    const QList<int> current = g.options.marginList();
    auto* marginsRow = new QWidget(pdfExport);
    auto* marginsLayout = new QHBoxLayout(marginsRow);
    marginsLayout->setContentsMargins(0, 0, 0, 0);
    QSpinBox* margins[4];
    const QString names[] = {QCoreApplication::translate("MarkdownViewer", "Left"), QCoreApplication::translate("MarkdownViewer", "Top"), QCoreApplication::translate("MarkdownViewer", "Right"), QCoreApplication::translate("MarkdownViewer", "Bottom")};
    for (int i = 0; i < 4; ++i) {
        margins[i] = new QSpinBox(marginsRow);
        margins[i]->setRange(0, 100);
        margins[i]->setSuffix(QStringLiteral(" mm"));
        margins[i]->setValue(current[i]);
        margins[i]->setToolTip(names[i]);
        marginsLayout->addWidget(margins[i]);
    }
    pdfForm->addRow(QCoreApplication::translate("MarkdownViewer", "Margins"), marginsRow);
    auto* openPdf = new QCheckBox(QCoreApplication::translate("MarkdownViewer", "Open PDF after export"), pdfExport);
    openPdf->setChecked(g.options.pdfOpenExport);
    pdfForm->addRow(openPdf);
    pdfLayout->addWidget(pdfExport);
    pdfLayout->addStretch(1);
    tabs->addTab(pdf, QCoreApplication::translate("MarkdownViewer", "PDF"));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.resize(520, 420);
    if (dialog.exec() != QDialog::Accepted)
        return;
    g.options.fileExtensions = extensions->text().trimmed();
    g.options.inclNewFiles = newFiles->isChecked();
    g.options.htmlCssStyle = cssEdit->toPlainText();
    g.options.htmlOpenExport = openHtml->isChecked();
    g.options.pdfOrientation = orientation->currentData().toString();
    g.options.pdfPageSize = pageSize->currentText();
    g.options.margins = QStringLiteral("%1,%2,%3,%4").arg(margins[0]->value()).arg(margins[1]->value()).arg(margins[2]->value()).arg(margins[3]->value());
    g.options.pdfOpenExport = openPdf->isChecked();
    saveOptions();
    // Lo que haya cambiado, a la vista
    updateViewer(true, true);
}

void aboutCommand()
{
    QMessageBox::about(QApplication::activeWindow(), QCoreApplication::translate("MarkdownViewer", "About MarkdownViewer++"),
                       QCoreApplication::translate("MarkdownViewer", "<b>MarkdownViewer++ %1</b> for Edita<br><br>"
                          "Renders Markdown (CommonMark with GitHub extensions) in a docked panel and exports it as HTML or PDF.<br><br>"
                          "Native port of MarkdownViewer++ for Notepad++ by Savas Ziplies (MIT): "
                          "<a href=\"https://nea.github.io/MarkdownViewerPlusPlus/\">nea.github.io/MarkdownViewerPlusPlus</a><br>"
                          "Markdown parser: md4c by Martin Mitáš (MIT).")
                           .arg(QLatin1String(kVersion)));
}

EditaShortcutKey g_toggleKey{true, false, true, 'M'}; // Ctrl+Mayús+M, como el original

EditaFuncItem g_funcs[] = {
    {"MarkdownViewer++", markdownViewerCommand, 0, false, &g_toggleKey},
    {"", nullptr, 0, false, nullptr},
    {"Synchronize scrolling (Editor -> Viewer)", synchronizeScrollingCommand, 0, false, nullptr},
    {"", nullptr, 0, false, nullptr},
    {"Options", optionsCommand, 0, false, nullptr},
    {"About", aboutCommand, 0, false, nullptr},
};

// NPPM_MSGTOPLUGIN (desde otros plugins o scripts): exportar sin diálogos
constexpr long kMsgExportHtml = 1; // info: const char* ruta
constexpr long kMsgExportPdf = 2;  // info: const char* ruta

} // namespace

EDITA_PLUGIN_EXPORT int edita_plugin_api_version(void)
{
    return EDITA_PLUGIN_API_VERSION;
}

EDITA_PLUGIN_EXPORT const char* edita_plugin_version(void)
{
    return kVersion;
}

EDITA_PLUGIN_EXPORT void setInfo(EditaData data)
{
    // Los diálogos y el panel en el idioma de Edita; el menú, en inglés como el original
    static QTranslator translator;
    if (translator.isEmpty() && translator.load(QLocale(), QStringLiteral("markdownviewer"), QStringLiteral("_"),
                                                QStringLiteral(":/markdownviewer/i18n")))
        QCoreApplication::installTranslator(&translator);
    g.data = data;
    g.panel = nullptr; // otra ventana: su panel se crea al abrirlo
    g.needsRender = true;
    g.iniPath = QDir(hostString(NPPM_GETPLUGINSCONFIGDIR)).filePath(QStringLiteral("MarkdownViewerPlusPlus.ini"));
    g.options = Options{};
    loadOptions();
    g_funcs[kSyncIndex].init2Check = g.options.synchronizeScrolling;
}

EDITA_PLUGIN_EXPORT const char* getName(void)
{
    return kPluginName;
}

EDITA_PLUGIN_EXPORT EditaFuncItem* getFuncsArray(int* count)
{
    *count = static_cast<int>(sizeof g_funcs / sizeof g_funcs[0]);
    return g_funcs;
}

// OnNotification
EDITA_PLUGIN_EXPORT void beNotified(SCNotification* n)
{
    if (!panelVisible())
        return;
    const unsigned code = n->nmhdr.code;
    void* from = n->nmhdr.hwndFrom;
    if (from == g.data.host) {
        if (code == NPPN_BUFFERACTIVATED)
            updateViewer(true, true);
        else if (code == NPPN_SHUTDOWN)
            saveOptions();
        return;
    }
    if (from != editor())
        return;
    if (code == SCN_UPDATEUI) {
        updateViewer((n->updated & SC_UPDATE_V_SCROLL) != 0, false);
    } else if (code == SCN_MODIFIED && (n->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT))) {
        g.needsRender = true;
    }
}

EDITA_PLUGIN_EXPORT intptr_t messageProc(unsigned int message, uintptr_t, intptr_t lParam)
{
    if (message != NPPM_MSGTOPLUGIN || !lParam)
        return 0;
    const auto* info = reinterpret_cast<const EditaCommunicationInfo*>(lParam);
    if (info->internalMsg == kMsgExportHtml || info->internalMsg == kMsgExportPdf) {
        // El documento actual, aunque el panel esté cerrado
        updateEditorInformation();
        const intptr_t length = sci(SCI_GETLENGTH);
        std::string text(static_cast<size_t>(length) + 1, '\0');
        sci(SCI_GETTEXT, text.size(), reinterpret_cast<intptr_t>(text.data()));
        text.resize(static_cast<size_t>(length));
        g.markdown = QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
        g.html = markdownToHtml(g.markdown);
        const QString path = QString::fromUtf8(static_cast<const char*>(info->info));
        return (info->internalMsg == kMsgExportHtml ? writeHtml(path) : writePdf(path)) ? 1 : 0;
    }
    return 0;
}
