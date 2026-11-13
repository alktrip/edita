// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Detector de secretos (fase 88; no existe en Notepad++). Un plugin de Edita que marca en el
// documento las claves de API, tokens, claves privadas y contraseñas escritas a mano, las
// lista en un panel, revisa todo el proyecto y avisa antes de un commit desde el panel Git
// si lo que se va a confirmar contiene alguna (edita_plugin_before_commit).
// Las reglas están en SecretRules.cpp.

#include "SecretRules.h"

#include <edita_plugin.h>

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QSet>
#include <QTimer>
#include <QTranslator>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <cstring>
#include <string>

using namespace edita::secrets;

namespace {

constexpr const char* kPluginName = "Secret Scanner";
constexpr const char* kModuleName = "SecretScanner.so";
constexpr const char* kVersion = "1.0.0";
constexpr qint64 kMaxFileBytes = 2 * 1024 * 1024;
constexpr int kMaxProjectFiles = 20000;

// La descripción de una regla en el idioma de la interfaz
QString describe(const QString& ruleId, const QString& fallback)
{
    static const QHash<QString, const char*> kNames{
        {QStringLiteral("private-key"), QT_TRANSLATE_NOOP("SecretScanner", "Private key")},
        {QStringLiteral("aws-access-key-id"), QT_TRANSLATE_NOOP("SecretScanner", "AWS access key ID")},
        {QStringLiteral("aws-secret-access-key"), QT_TRANSLATE_NOOP("SecretScanner", "AWS secret access key")},
        {QStringLiteral("github-token"), QT_TRANSLATE_NOOP("SecretScanner", "GitHub token")},
        {QStringLiteral("github-fine-grained-token"), QT_TRANSLATE_NOOP("SecretScanner", "GitHub fine-grained token")},
        {QStringLiteral("gitlab-token"), QT_TRANSLATE_NOOP("SecretScanner", "GitLab token")},
        {QStringLiteral("slack-webhook"), QT_TRANSLATE_NOOP("SecretScanner", "Slack webhook URL")},
        {QStringLiteral("slack-token"), QT_TRANSLATE_NOOP("SecretScanner", "Slack token")},
        {QStringLiteral("discord-webhook"), QT_TRANSLATE_NOOP("SecretScanner", "Discord webhook URL")},
        {QStringLiteral("anthropic-key"), QT_TRANSLATE_NOOP("SecretScanner", "Anthropic API key")},
        {QStringLiteral("openai-key"), QT_TRANSLATE_NOOP("SecretScanner", "OpenAI API key")},
        {QStringLiteral("stripe-secret-key"), QT_TRANSLATE_NOOP("SecretScanner", "Stripe secret key")},
        {QStringLiteral("google-api-key"), QT_TRANSLATE_NOOP("SecretScanner", "Google API key")},
        {QStringLiteral("huggingface-token"), QT_TRANSLATE_NOOP("SecretScanner", "Hugging Face token")},
        {QStringLiteral("npm-token"), QT_TRANSLATE_NOOP("SecretScanner", "npm token")},
        {QStringLiteral("pypi-token"), QT_TRANSLATE_NOOP("SecretScanner", "PyPI token")},
        {QStringLiteral("sendgrid-key"), QT_TRANSLATE_NOOP("SecretScanner", "SendGrid API key")},
        {QStringLiteral("digitalocean-token"), QT_TRANSLATE_NOOP("SecretScanner", "DigitalOcean token")},
        {QStringLiteral("telegram-bot-token"), QT_TRANSLATE_NOOP("SecretScanner", "Telegram bot token")},
        {QStringLiteral("azure-storage-key"), QT_TRANSLATE_NOOP("SecretScanner", "Azure storage account key")},
        {QStringLiteral("jwt"), QT_TRANSLATE_NOOP("SecretScanner", "JSON Web Token")},
        {QStringLiteral("url-password"), QT_TRANSLATE_NOOP("SecretScanner", "Password in a URL")},
        {QStringLiteral("env-secret"), QT_TRANSLATE_NOOP("SecretScanner", "Secret in a .env file")},
        {QStringLiteral("generic-secret"), QT_TRANSLATE_NOOP("SecretScanner", "Hard-coded secret")},
    };
    const auto it = kNames.constFind(ruleId);
    return it == kNames.constEnd() ? fallback : QCoreApplication::translate("SecretScanner", *it);
}

enum Command { kPanel, kScanDocument, kScanProject, kSeparator1, kIgnore, kLive, kSeparator2, kAbout, kCommandCount };

struct Located
{
    QString path; // vacío: el documento actual
    Finding finding;
};

struct State
{
    EditaData data{};
    int indicator = -1;
    bool live = true;
    QSet<QString> ignored; // huellas
    QString iniPath;
    QPointer<QWidget> panel;
    QLabel* label = nullptr;
    QTreeWidget* tree = nullptr;
    std::vector<Located> shown;
    QTimer* debounce = nullptr;
    std::string names[kCommandCount];
} g;

EditaFuncItem g_funcs[kCommandCount];

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

// --- Configuración: SecretScanner.ini en la carpeta de configuración de los plugins ---------

void loadSettings()
{
    QFile file(g.iniPath);
    if (!file.open(QIODevice::ReadOnly))
        return;
    for (const QByteArray& raw : file.readAll().split('\n')) {
        const QString line = QString::fromUtf8(raw).trimmed();
        if (line.startsWith(QLatin1String("live=")))
            g.live = line.mid(5) != QLatin1String("0");
        else if (line.startsWith(QLatin1String("ignore=")))
            g.ignored.insert(line.mid(7));
    }
}

void saveSettings()
{
    QDir().mkpath(QFileInfo(g.iniPath).absolutePath());
    QFile file(g.iniPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    QByteArray out = "[SecretScanner]\nlive=" + QByteArray(g.live ? "1" : "0") + "\n";
    for (const QString& f : std::as_const(g.ignored))
        out += "ignore=" + f.toUtf8() + "\n";
    file.write(out);
}

std::vector<Finding> notIgnored(std::vector<Finding> found)
{
    std::erase_if(found, [](const Finding& f) { return g.ignored.contains(f.fingerprint); });
    return found;
}

// --- El documento actual -------------------------------------------------------------------

QByteArray documentText()
{
    const intptr_t length = sci(SCI_GETLENGTH);
    if (length <= 0 || length > kMaxFileBytes)
        return {};
    const auto* data = reinterpret_cast<const char*>(sci(SCI_GETCHARACTERPOINTER));
    return data ? QByteArray(data, static_cast<qsizetype>(length)) : QByteArray();
}

void styleIndicator()
{
    if (g.indicator < 0)
        return;
    const auto ind = static_cast<uintptr_t>(g.indicator);
    sci(SCI_INDICSETSTYLE, ind, INDIC_SQUIGGLEPIXMAP);
    sci(SCI_INDICSETFORE, ind, 0x2020E0); // rojo (BGR)
    sci(SCI_INDICSETUNDER, ind, 1);
}

void clearMarks()
{
    if (g.indicator < 0)
        return;
    sci(SCI_SETINDICATORCURRENT, static_cast<uintptr_t>(g.indicator));
    sci(SCI_INDICATORCLEARRANGE, 0, sci(SCI_GETLENGTH));
}

void fillPanel(const std::vector<Located>& items, const QString& summary);

std::vector<Finding> scanDocument(bool showInPanel)
{
    const QString name = QFileInfo(hostString(NPPM_GETFULLCURRENTPATH)).fileName();
    std::vector<Finding> found = notIgnored(scan(documentText(), name));
    styleIndicator();
    clearMarks();
    if (g.indicator >= 0) {
        for (const Finding& f : found)
            sci(SCI_INDICATORFILLRANGE, static_cast<uintptr_t>(f.start), f.length);
    }
    if (showInPanel && g.panel) {
        std::vector<Located> items;
        for (const Finding& f : found)
            items.push_back({QString(), f});
        fillPanel(items, found.empty() ? QCoreApplication::translate("SecretScanner", "No secrets found in %1.").arg(name)
                                       : QCoreApplication::translate("SecretScanner", "%1: %n possible secret(s)", nullptr, static_cast<int>(found.size())).arg(name));
    }
    return found;
}

// --- El panel ------------------------------------------------------------------------------

void goTo(const Located& item)
{
    if (!item.path.isEmpty()) {
        const QByteArray path = item.path.toUtf8();
        host(NPPM_DOOPEN, 0, reinterpret_cast<intptr_t>(path.constData()));
        // Abierto: sus posiciones se calculan con el texto del editor
        const std::vector<Finding> found = scanDocument(false);
        for (const Finding& f : found) {
            if (f.fingerprint == item.finding.fingerprint && f.line == item.finding.line) {
                sci(SCI_SETSEL, static_cast<uintptr_t>(f.start), f.start + f.length);
                sci(SCI_SCROLLCARET);
                break;
            }
        }
    } else {
        sci(SCI_SETSEL, static_cast<uintptr_t>(item.finding.start), item.finding.start + item.finding.length);
        sci(SCI_SCROLLCARET);
    }
    sci(SCI_GRABFOCUS);
}

void ensurePanel()
{
    if (g.panel)
        return;
    auto* panel = new QWidget;
    panel->setObjectName(QStringLiteral("secretScannerPanel"));
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(4, 4, 4, 4);
    g.label = new QLabel(panel);
    g.label->setObjectName(QStringLiteral("secretScannerLabel"));
    g.label->setWordWrap(true);
    layout->addWidget(g.label);
    g.tree = new QTreeWidget(panel);
    g.tree->setObjectName(QStringLiteral("secretScannerTree"));
    g.tree->setRootIsDecorated(false);
    g.tree->setUniformRowHeights(true);
    g.tree->setHeaderLabels({QCoreApplication::translate("SecretScanner", "Where"), QCoreApplication::translate("SecretScanner", "Type"), QCoreApplication::translate("SecretScanner", "Value")});
    g.tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    g.tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    layout->addWidget(g.tree, 1);
    auto* buttons = new QHBoxLayout;
    auto* ignore = new QPushButton(QCoreApplication::translate("SecretScanner", "&Ignore"), panel);
    ignore->setObjectName(QStringLiteral("secretScannerIgnore"));
    ignore->setToolTip(QCoreApplication::translate("SecretScanner", "Do not report this value again (in any file)"));
    auto* rescan = new QPushButton(QCoreApplication::translate("SecretScanner", "&Scan Again"), panel);
    rescan->setObjectName(QStringLiteral("secretScannerRescan"));
    buttons->addWidget(ignore);
    buttons->addStretch(1);
    buttons->addWidget(rescan);
    layout->addLayout(buttons);
    QObject::connect(g.tree, &QTreeWidget::itemActivated, panel, [](QTreeWidgetItem* item) {
        const int index = item ? item->data(0, Qt::UserRole).toInt() : -1;
        if (index >= 0 && index < static_cast<int>(g.shown.size()))
            goTo(g.shown[static_cast<size_t>(index)]);
    });
    QObject::connect(ignore, &QPushButton::clicked, panel, [] {
        const QTreeWidgetItem* item = g.tree->currentItem();
        const int index = item ? item->data(0, Qt::UserRole).toInt() : -1;
        if (index < 0 || index >= static_cast<int>(g.shown.size()))
            return;
        g.ignored.insert(g.shown[static_cast<size_t>(index)].finding.fingerprint);
        saveSettings();
        scanDocument(true);
    });
    QObject::connect(rescan, &QPushButton::clicked, panel, [] { scanDocument(true); });
    g.panel = panel;
    EditaDockData dock{};
    dock.hClient = panel;
    dock.pszName = kPluginName;
    dock.dlgID = kPanel;
    dock.uMask = DWS_DF_CONT_BOTTOM;
    dock.pszModuleName = kModuleName;
    host(NPPM_DMMREGASDCKDLG, 0, reinterpret_cast<intptr_t>(&dock));
}

void fillPanel(const std::vector<Located>& items, const QString& summary)
{
    if (!g.panel)
        return;
    g.shown = items;
    g.label->setText(summary);
    g.tree->clear();
    for (size_t i = 0; i < items.size(); ++i) {
        const Located& l = items[i];
        const QString where = l.path.isEmpty() ? QCoreApplication::translate("SecretScanner", "Line %1").arg(l.finding.line + 1)
                                               : QStringLiteral("%1:%2").arg(QDir::toNativeSeparators(l.path)).arg(l.finding.line + 1);
        auto* item = new QTreeWidgetItem(g.tree, {where, describe(l.finding.ruleId, l.finding.description), mask(l.finding.secret)});
        item->setData(0, Qt::UserRole, static_cast<int>(i));
        item->setToolTip(0, where);
    }
    if (!items.empty())
        g.tree->setCurrentItem(g.tree->topLevelItem(0));
}

bool panelVisible()
{
    return g.panel && g.panel->isVisible();
}

void showPanel(bool show)
{
    ensurePanel();
    host(show ? NPPM_DMMSHOW : NPPM_DMMHIDE, 0, reinterpret_cast<intptr_t>(g.panel.data()));
    host(NPPM_SETMENUITEMCHECK, static_cast<uintptr_t>(g_funcs[kPanel].cmdID), show);
}

// --- Todo el proyecto ----------------------------------------------------------------------

// La raíz del repositorio de Git del documento, o su carpeta
QString projectRoot(bool* isGit)
{
    const QString dir = hostString(NPPM_GETCURRENTDIRECTORY);
    *isGit = false;
    if (dir.isEmpty())
        return {};
    QProcess git;
    git.setWorkingDirectory(dir);
    git.start(QStringLiteral("git"), {QStringLiteral("rev-parse"), QStringLiteral("--show-toplevel")});
    if (git.waitForFinished(10000) && git.exitStatus() == QProcess::NormalExit && git.exitCode() == 0) {
        *isGit = true;
        return QString::fromUtf8(git.readAllStandardOutput()).trimmed();
    }
    return dir;
}

QStringList projectFiles(const QString& root, bool isGit)
{
    QStringList files;
    if (isGit) {
        // Lo que sigue Git y lo nuevo que no ignora: lo que podría acabar en un commit
        QProcess git;
        git.setWorkingDirectory(root);
        git.start(QStringLiteral("git"), {QStringLiteral("ls-files"), QStringLiteral("-z"), QStringLiteral("--cached"),
                                          QStringLiteral("--others"), QStringLiteral("--exclude-standard")});
        if (git.waitForFinished(60000)) {
            for (const QByteArray& f : git.readAllStandardOutput().split('\0')) {
                if (!f.isEmpty())
                    files << QDir(root).filePath(QString::fromUtf8(f));
            }
        }
        return files;
    }
    static const QSet<QString> kSkip{QStringLiteral(".git"), QStringLiteral("node_modules"), QStringLiteral("build"),
                                     QStringLiteral("dist"), QStringLiteral("target"), QStringLiteral(".venv")};
    QDirIterator it(root, QDir::Files | QDir::Hidden | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext() && files.size() < kMaxProjectFiles) {
        const QString path = it.next();
        const QStringList parts = QDir(root).relativeFilePath(path).split(QLatin1Char('/'));
        if (std::none_of(parts.begin(), parts.end(), [](const QString& p) { return kSkip.contains(p); }))
            files << path;
    }
    return files;
}

std::vector<Located> scanFiles(const QStringList& files)
{
    std::vector<Located> found;
    for (const QString& path : files) {
        QFile file(path);
        if (file.size() > kMaxFileBytes || !file.open(QIODevice::ReadOnly))
            continue;
        const QByteArray bytes = file.readAll();
        if (bytes.left(8192).contains('\0'))
            continue; // binario
        for (Finding& f : notIgnored(scan(bytes, QFileInfo(path).fileName())))
            found.push_back({path, std::move(f)});
    }
    return found;
}

void scanProject()
{
    bool isGit = false;
    const QString root = projectRoot(&isGit);
    if (root.isEmpty()) {
        QMessageBox::information(QApplication::activeWindow(), QLatin1String(kPluginName),
                                 QCoreApplication::translate("SecretScanner", "Save the document first: the project is the folder (or Git repository) of the current file."));
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QStringList files = projectFiles(root, isGit);
    const std::vector<Located> found = scanFiles(files);
    QApplication::restoreOverrideCursor();
    showPanel(true);
    fillPanel(found, found.empty()
                         ? QCoreApplication::translate("SecretScanner", "No secrets found in %n file(s) of %1.", nullptr,
                                                       static_cast<int>(files.size()))
                               .arg(QDir::toNativeSeparators(root))
                         : QCoreApplication::translate("SecretScanner", "%n possible secret(s) in %1", nullptr,
                                                       static_cast<int>(found.size()))
                               .arg(QDir::toNativeSeparators(root)));
}

// --- Órdenes del menú ----------------------------------------------------------------------

void panelCommand()
{
    const bool show = !panelVisible();
    showPanel(show);
    if (show)
        scanDocument(true);
}

void scanDocumentCommand()
{
    showPanel(true);
    scanDocument(true);
}

void ignoreAtCursorCommand()
{
    const intptr_t caret = sci(SCI_GETCURRENTPOS);
    for (const Finding& f : scanDocument(false)) {
        if (caret >= f.start && caret <= f.start + f.length) {
            g.ignored.insert(f.fingerprint);
            saveSettings();
            scanDocument(panelVisible());
            return;
        }
    }
    QMessageBox::information(QApplication::activeWindow(), QLatin1String(kPluginName),
                             QCoreApplication::translate("SecretScanner", "The cursor is not on a detected secret."));
}

void liveCommand()
{
    g.live = !g.live;
    host(NPPM_SETMENUITEMCHECK, static_cast<uintptr_t>(g_funcs[kLive].cmdID), g.live);
    saveSettings();
    if (g.live)
        scanDocument(panelVisible());
    else
        clearMarks();
}

void aboutCommand()
{
    QMessageBox::about(QApplication::activeWindow(), QLatin1String(kPluginName),
                       QCoreApplication::translate("SecretScanner", "<b>Secret Scanner %1</b><br><br>"
                          "Marks API keys, tokens, private keys and passwords written in your files, "
                          "and warns before a commit from the Git panel if the staged changes contain one.<br><br>"
                          "%2 rules. To allow a value, add <code>edita:allow-secret</code> to its line, "
                          "or ignore it from the panel.<br><br>Part of Edita. GPL-3.0-or-later.")
                           .arg(QLatin1String(kVersion))
                           .arg(rules().size()));
}

void setName(Command index, const QString& text)
{
    g.names[index] = text.toUtf8().toStdString();
    std::strncpy(g_funcs[index].itemName, g.names[index].c_str(), EDITA_MENU_ITEM_SIZE - 1);
    g_funcs[index].itemName[EDITA_MENU_ITEM_SIZE - 1] = '\0';
}

void setupCommands()
{
    std::memset(g_funcs, 0, sizeof g_funcs);
    setName(kPanel, QCoreApplication::translate("SecretScanner", "Secrets Panel"));
    setName(kScanDocument, QCoreApplication::translate("SecretScanner", "Scan Current Document"));
    setName(kScanProject, QCoreApplication::translate("SecretScanner", "Scan Project"));
    setName(kIgnore, QCoreApplication::translate("SecretScanner", "Ignore Secret at Cursor"));
    setName(kLive, QCoreApplication::translate("SecretScanner", "Mark Secrets While Typing"));
    setName(kAbout, QCoreApplication::translate("SecretScanner", "About Secret Scanner…"));
    g_funcs[kPanel].pFunc = panelCommand;
    g_funcs[kScanDocument].pFunc = scanDocumentCommand;
    g_funcs[kScanProject].pFunc = scanProject;
    g_funcs[kIgnore].pFunc = ignoreAtCursorCommand;
    g_funcs[kLive].pFunc = liveCommand;
    g_funcs[kLive].init2Check = g.live;
    g_funcs[kAbout].pFunc = aboutCommand;
}

// --- Antes de un commit ----------------------------------------------------------------------

// Las líneas añadidas en lo que está en el índice (git diff --cached), con su archivo y línea
std::vector<Located> scanStaged(const QString& repository)
{
    QProcess git;
    git.setWorkingDirectory(repository);
    git.start(QStringLiteral("git"), {QStringLiteral("diff"), QStringLiteral("--cached"), QStringLiteral("-U0"),
                                      QStringLiteral("--no-color"), QStringLiteral("--no-ext-diff")});
    std::vector<Located> found;
    if (!git.waitForFinished(60000) || git.exitCode() != 0)
        return found;
    QString file;
    int line = 0;
    for (const QByteArray& raw : git.readAllStandardOutput().split('\n')) {
        if (raw.startsWith("+++ ")) {
            file = raw.startsWith("+++ b/") ? QString::fromUtf8(raw.mid(6)) : QString();
        } else if (raw.startsWith("@@")) {
            // @@ -a,b +c,d @@
            const qsizetype plus = raw.indexOf('+');
            line = plus > 0 ? raw.mid(plus + 1).split(',').first().split(' ').first().toInt() : 0;
        } else if (raw.startsWith('+') && !file.isEmpty()) {
            for (Finding& f : notIgnored(scan(raw.mid(1), QFileInfo(file).fileName()))) {
                f.line = line - 1;
                found.push_back({file, std::move(f)});
            }
            ++line;
        }
    }
    return found;
}

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
    static QTranslator translator;
    if (translator.isEmpty() && translator.load(QLocale(), QStringLiteral("secretscanner"), QStringLiteral("_"),
                                                QStringLiteral(":/secretscanner/i18n")))
        QCoreApplication::installTranslator(&translator);
    g.data = data;
    g.panel = nullptr;
    g.shown.clear();
    g.iniPath = QDir(hostString(NPPM_GETPLUGINSCONFIGDIR)).filePath(QStringLiteral("SecretScanner.ini"));
    g.ignored.clear();
    g.live = true;
    loadSettings();
    if (!g.debounce) {
        g.debounce = new QTimer;
        g.debounce->setSingleShot(true);
        g.debounce->setInterval(400);
        QObject::connect(g.debounce, &QTimer::timeout, [] { scanDocument(panelVisible()); });
    }
    setupCommands();
}

EDITA_PLUGIN_EXPORT const char* getName(void)
{
    return kPluginName;
}

EDITA_PLUGIN_EXPORT EditaFuncItem* getFuncsArray(int* count)
{
    *count = kCommandCount;
    return g_funcs;
}

EDITA_PLUGIN_EXPORT void beNotified(SCNotification* n)
{
    const unsigned code = n->nmhdr.code;
    if (n->nmhdr.hwndFrom == g.data.host) {
        if (code == NPPN_READY) {
            int first = -1;
            if (host(NPPM_ALLOCATEINDICATOR, 1, reinterpret_cast<intptr_t>(&first)) && first >= 0)
                g.indicator = first;
            if (g.live)
                scanDocument(false);
        } else if (code == NPPN_BUFFERACTIVATED || code == NPPN_FILEOPENED) {
            if (g.live || panelVisible())
                g.debounce->start(0);
        } else if (code == NPPN_SHUTDOWN) {
            saveSettings();
        }
        return;
    }
    if (n->nmhdr.hwndFrom != editor())
        return;
    if (code == SCN_MODIFIED && (n->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) && g.live)
        g.debounce->start();
}

EDITA_PLUGIN_EXPORT intptr_t messageProc(unsigned int, uintptr_t, intptr_t)
{
    return 0;
}

EDITA_PLUGIN_EXPORT int edita_plugin_before_commit(EditaCommitCheck* check)
{
    if (!check || !check->repository)
        return 0;
    const std::vector<Located> found = scanStaged(QString::fromUtf8(check->repository));
    if (found.empty())
        return 0;
    QString text = QCoreApplication::translate("SecretScanner", "Secret Scanner found %n possible secret(s) in the staged changes:", nullptr,
                                              static_cast<int>(found.size()));
    for (size_t i = 0; i < found.size() && i < 8; ++i) {
        const Finding& f = found[i].finding;
        text += QStringLiteral("\n  %1:%2  %3  %4").arg(found[i].path).arg(f.line + 1).arg(describe(f.ruleId, f.description), mask(f.secret));
    }
    if (found.size() > 8)
        text += QStringLiteral("\n  …");
    text += QLatin1Char('\n') + QCoreApplication::translate("SecretScanner", "If a value is not a secret, add “edita:allow-secret” to its line or ignore it from the Secrets panel.");
    const QByteArray utf8 = text.toUtf8();
    if (check->reason && check->reasonSize > 0) {
        const size_t n = std::min<size_t>(static_cast<size_t>(utf8.size()), check->reasonSize - 1);
        std::memcpy(check->reason, utf8.constData(), n);
        check->reason[n] = '\0';
    }
    return 1;
}
