// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Editor hexadecimal (fase 89): un plugin propio de Edita, escrito desde cero en Qt. Hace lo
// mismo que el plugin HEX-Editor de Notepad++ (ver y editar los bytes de un archivo), pero
// sobre los bytes del archivo en el disco y no sobre el texto del editor, que Edita guarda en
// UTF-8: así sirve también para binarios y para archivos en otras codificaciones.

#include "HexView.h"

#include <edita_plugin.h>

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPointer>
#include <QToolButton>
#include <QTranslator>
#include <QVBoxLayout>

#include <cstring>
#include <string>

using namespace edita::hex;

namespace {

constexpr const char* kPluginName = "Hex Editor";
constexpr const char* kModuleName = "EditaHex.so";
constexpr const char* kVersion = "1.0.0";

enum Command { kPanel, kOpenCurrent, kOpenFile, kSeparator, kAbout, kCommandCount };

class Panel : public QWidget
{
public:
    Panel()
    {
        setObjectName(QStringLiteral("hexEditorPanel"));
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);
        auto* bar = new QHBoxLayout;
        auto button = [this, bar](const QString& text, const char* name, const QString& tip) {
            auto* b = new QToolButton(this);
            b->setText(text);
            b->setToolTip(tip);
            b->setObjectName(QString::fromLatin1(name));
            b->setAutoRaise(true);
            bar->addWidget(b);
            return b;
        };
        QToolButton* open = button(QCoreApplication::translate("EditaHex", "Open…"), "hexOpen",
                                   QCoreApplication::translate("EditaHex", "Open a file in the hex editor"));
        _save = button(QCoreApplication::translate("EditaHex", "Save"), "hexSave",
                       QCoreApplication::translate("EditaHex", "Save the bytes to the file"));
        QToolButton* saveAs = button(QCoreApplication::translate("EditaHex", "Save As…"), "hexSaveAs", QString());
        bar->addSpacing(12);
        _goTo = new QLineEdit(this);
        _goTo->setObjectName(QStringLiteral("hexGoTo"));
        _goTo->setPlaceholderText(QCoreApplication::translate("EditaHex", "Go to offset (0x1F or 31)"));
        _goTo->setMaximumWidth(_goTo->fontMetrics().horizontalAdvance(QLatin1Char('0')) * 26);
        bar->addWidget(_goTo);
        _find = new QLineEdit(this);
        _find->setObjectName(QStringLiteral("hexFind"));
        _find->setPlaceholderText(QCoreApplication::translate("EditaHex", "Find"));
        bar->addWidget(_find, 1);
        _findMode = new QComboBox(this);
        _findMode->setObjectName(QStringLiteral("hexFindMode"));
        _findMode->addItem(QCoreApplication::translate("EditaHex", "Hex"));
        _findMode->addItem(QCoreApplication::translate("EditaHex", "Text"));
        bar->addWidget(_findMode);
        QToolButton* previous = button(QStringLiteral("↑"), "hexFindPrevious", QCoreApplication::translate("EditaHex", "Find previous"));
        QToolButton* next = button(QStringLiteral("↓"), "hexFindNext", QCoreApplication::translate("EditaHex", "Find next"));
        bar->addSpacing(12);
        _perRow = new QComboBox(this);
        _perRow->setObjectName(QStringLiteral("hexBytesPerRow"));
        for (const int n : {8, 16, 24, 32})
            _perRow->addItem(QCoreApplication::translate("EditaHex", "%1 bytes").arg(n), n);
        _perRow->setCurrentIndex(1);
        bar->addWidget(_perRow);
        _mode = button(QString(), "hexInsertMode", QCoreApplication::translate("EditaHex", "Overwrite or insert (Insert key)"));
        layout->addLayout(bar);

        _view = new HexView(this);
        _view->setBuffer(&_buffer);
        layout->addWidget(_view, 1);
        _info = new QLabel(this);
        _info->setObjectName(QStringLiteral("hexInfo"));
        layout->addWidget(_info);
        _values = new QLabel(this);
        _values->setObjectName(QStringLiteral("hexValues"));
        _values->setTextInteractionFlags(Qt::TextSelectableByMouse);
        _values->setWordWrap(true);
        layout->addWidget(_values);

        connect(open, &QToolButton::clicked, this, [this] { openDialog(); });
        connect(_save, &QToolButton::clicked, this, [this] { save(_buffer.path()); });
        connect(saveAs, &QToolButton::clicked, this, [this] {
            const QString path = QFileDialog::getSaveFileName(this, QCoreApplication::translate("EditaHex", "Save As"), _buffer.path());
            if (!path.isEmpty())
                save(path);
        });
        connect(_goTo, &QLineEdit::returnPressed, this, [this] { goTo(_goTo->text()); });
        connect(_find, &QLineEdit::returnPressed, this, [this] { find(true); });
        connect(next, &QToolButton::clicked, this, [this] { find(true); });
        connect(previous, &QToolButton::clicked, this, [this] { find(false); });
        connect(_perRow, &QComboBox::currentIndexChanged, this, [this] { _view->setBytesPerRow(_perRow->currentData().toInt()); });
        connect(_mode, &QToolButton::clicked, this, [this] { _view->setOverwrite(!_view->overwrite()); });
        connect(_view, &HexView::overwriteChanged, this, [this] { updateInfo(); });
        connect(_view, &HexView::cursorChanged, this, [this] { updateInfo(); });
        connect(_view, &HexView::contentChanged, this, [this] { updateInfo(); });
        updateInfo();
    }

    HexBuffer& buffer() { return _buffer; }
    HexView* view() const { return _view; }

    // Si hay cambios sin guardar, pregunta antes de abrir otro archivo
    bool mayDiscard()
    {
        if (!_buffer.isModified())
            return true;
        const auto answer = QMessageBox::question(
            this, QLatin1String(kPluginName),
            QCoreApplication::translate("EditaHex", "Save the changes to %1?").arg(QFileInfo(_buffer.path()).fileName()),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (answer == QMessageBox::Save)
            return save(_buffer.path());
        return answer == QMessageBox::Discard;
    }

    bool open(const QString& path)
    {
        if (!mayDiscard())
            return false;
        QString error;
        if (QFileInfo(path).size() > HexBuffer::kMaxBytes) {
            QMessageBox::warning(this, QLatin1String(kPluginName),
                                 QCoreApplication::translate("EditaHex", "%1 is larger than %2 MB.")
                                     .arg(QFileInfo(path).fileName())
                                     .arg(HexBuffer::kMaxBytes / (1024 * 1024)));
            return false;
        }
        if (!_buffer.load(path, &error)) {
            QMessageBox::warning(this, QLatin1String(kPluginName),
                                 QCoreApplication::translate("EditaHex", "Cannot read %1:\n%2").arg(path, error));
            return false;
        }
        _view->setBuffer(&_buffer);
        updateInfo();
        return true;
    }

    bool save(const QString& path)
    {
        if (path.isEmpty())
            return false;
        QString error;
        if (!_buffer.save(path, &error)) {
            QMessageBox::warning(this, QLatin1String(kPluginName),
                                 QCoreApplication::translate("EditaHex", "Cannot write %1:\n%2").arg(path, error));
            return false;
        }
        updateInfo();
        return true;
    }

    void openDialog()
    {
        const QString path = QFileDialog::getOpenFileName(this, QCoreApplication::translate("EditaHex", "Open in Hex Editor"),
                                                          QFileInfo(_buffer.path()).absolutePath());
        if (!path.isEmpty())
            open(path);
    }

    void goTo(const QString& text)
    {
        QString t = text.trimmed();
        bool ok = false;
        qlonglong offset = 0;
        if (t.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
            offset = t.mid(2).toLongLong(&ok, 16);
        else if (t.endsWith(QLatin1Char('h'), Qt::CaseInsensitive))
            offset = t.chopped(1).toLongLong(&ok, 16);
        else
            offset = t.toLongLong(&ok, 10);
        if (!ok || offset < 0 || offset > _buffer.size()) {
            _info->setText(QCoreApplication::translate("EditaHex", "Offset out of range: %1").arg(t));
            return;
        }
        _view->setPosition(offset);
        _view->setFocus();
    }

    // Busca desde el cursor (hacia delante, después de la selección actual)
    bool find(bool forward)
    {
        const QString text = _find->text();
        if (text.isEmpty())
            return false;
        QByteArray pattern;
        if (_findMode->currentIndex() == 0) {
            const auto hex = parseHex(text);
            if (!hex) {
                _info->setText(QCoreApplication::translate("EditaHex", "Not valid hexadecimal: %1").arg(text));
                return false;
            }
            pattern = *hex;
        } else {
            pattern = text.toUtf8();
        }
        const auto [from, to] = _view->selection();
        const auto found = _buffer.find(pattern, forward ? (to > from ? from + 1 : _view->position()) : from, forward);
        if (!found) {
            _info->setText(QCoreApplication::translate("EditaHex", "Not found: %1").arg(text));
            return false;
        }
        _view->select(*found, pattern.size());
        return true;
    }

    // El carácter para el inspector: los de control, como U+0002 (no se ven)
    static QString character(const QString& text)
    {
        if (text.isEmpty())
            return QStringLiteral("—");
        const uint code = text.toUcs4().value(0);
        const QString point = QStringLiteral("U+") + QString::number(code, 16).toUpper().rightJustified(4, QLatin1Char('0'));
        if (QChar::category(code) == QChar::Other_Control || QChar::category(code) == QChar::Other_Format)
            return point;
        return QStringLiteral("«%1» %2").arg(text, point);
    }

    void updateInfo()
    {
        _mode->setText(_view->overwrite() ? QCoreApplication::translate("EditaHex", "OVR") : QCoreApplication::translate("EditaHex", "INS"));
        _save->setEnabled(_buffer.isModified() && !_buffer.path().isEmpty());
        const qsizetype pos = _view->position();
        const auto [from, to] = _view->selection();
        QString info = _buffer.path().isEmpty() ? QCoreApplication::translate("EditaHex", "No file")
                                                : QFileInfo(_buffer.path()).fileName() + (_buffer.isModified() ? QStringLiteral(" •") : QString());
        info += QStringLiteral("   ") + QCoreApplication::translate("EditaHex", "%n byte(s)", nullptr, static_cast<int>(_buffer.size()));
        info += QStringLiteral("   ") + QCoreApplication::translate("EditaHex", "Offset: 0x%1 (%2)").arg(pos, 0, 16).arg(pos);
        if (to > from)
            info += QStringLiteral("   ") + QCoreApplication::translate("EditaHex", "Selected: %n byte(s)", nullptr, static_cast<int>(to - from));
        _info->setText(info);
        const Values v = valuesAt(_buffer.data(), pos);
        auto num = [](const auto& value) { return value ? QString::number(*value) : QStringLiteral("—"); };
        _values->setText(QStringLiteral("int8 %1 · uint8 %2 · int16 %3/%4 · int32 %5/%6 · int64 %7 · float %8 · double %9 · UTF-8 %10")
                             .arg(num(v.i8), num(v.u8), num(v.i16le), num(v.i16be), num(v.i32le), num(v.i32be), num(v.i64le),
                                  num(v.f32le), num(v.f64le))
                             .arg(character(v.utf8)));
        _values->setToolTip(QCoreApplication::translate("EditaHex", "Values at the cursor; with two numbers, little endian / big endian"));
    }

private:
    HexBuffer _buffer;
    HexView* _view = nullptr;
    QToolButton* _save = nullptr;
    QToolButton* _mode = nullptr;
    QLineEdit* _goTo = nullptr;
    QLineEdit* _find = nullptr;
    QComboBox* _findMode = nullptr;
    QComboBox* _perRow = nullptr;
    QLabel* _info = nullptr;
    QLabel* _values = nullptr;
};

struct State
{
    EditaData data{};
    QPointer<Panel> panel;
    std::string names[kCommandCount];
} g;

EditaFuncItem g_funcs[kCommandCount];
EditaShortcutKey g_panelKey{true, true, true, 'H'}; // Ctrl+Alt+Mayús+H, como «View in HEX» de N++

intptr_t host(unsigned message, uintptr_t w = 0, intptr_t l = 0)
{
    return g.data.send(g.data.host, message, w, l);
}

QString currentPath()
{
    std::string buffer(4096, '\0');
    host(NPPM_GETFULLCURRENTPATH, buffer.size(), reinterpret_cast<intptr_t>(buffer.data()));
    const QString path = QString::fromUtf8(buffer.c_str());
    return QFileInfo(path).isFile() ? path : QString();
}

bool panelVisible()
{
    return g.panel && g.panel->isVisible();
}

void showPanel(bool show)
{
    if (!g.panel) {
        g.panel = new Panel;
        EditaDockData dock{};
        dock.hClient = g.panel.data();
        dock.pszName = kPluginName;
        dock.dlgID = kPanel;
        dock.uMask = DWS_DF_CONT_BOTTOM;
        dock.pszModuleName = kModuleName;
        host(NPPM_DMMREGASDCKDLG, 0, reinterpret_cast<intptr_t>(&dock));
    }
    host(show ? NPPM_DMMSHOW : NPPM_DMMHIDE, 0, reinterpret_cast<intptr_t>(g.panel.data()));
    host(NPPM_SETMENUITEMCHECK, static_cast<uintptr_t>(g_funcs[kPanel].cmdID), show);
    if (show)
        g.panel->view()->setFocus();
}

void openCurrent()
{
    const QString path = currentPath();
    if (path.isEmpty()) {
        QMessageBox::information(QApplication::activeWindow(), QLatin1String(kPluginName),
                                 QCoreApplication::translate("EditaHex", "The current document has not been saved to a file. Save it first, or use Open File."));
        return;
    }
    showPanel(true);
    g.panel->open(path);
}

void panelCommand()
{
    const bool show = !panelVisible();
    // Al abrirlo por primera vez, con el archivo actual
    if (show && (!g.panel || g.panel->buffer().path().isEmpty()) && !currentPath().isEmpty()) {
        openCurrent();
        return;
    }
    showPanel(show);
}

void openFileCommand()
{
    showPanel(true);
    g.panel->openDialog();
}

void aboutCommand()
{
    QMessageBox::about(QApplication::activeWindow(), QLatin1String(kPluginName),
                       QCoreApplication::translate("EditaHex", "<b>Hex Editor %1</b><br><br>Shows and edits the bytes of a file: offset, hexadecimal and ASCII, "
                          "overwrite or insert, undo, find bytes or text, go to an offset and a value inspector.<br><br>"
                          "It works on the file on disk, so it also opens binary files.<br><br>Part of Edita. GPL-3.0-or-later.")
                           .arg(QLatin1String(kVersion)));
}

void setName(Command index, const QString& text)
{
    g.names[index] = text.toUtf8().toStdString();
    std::strncpy(g_funcs[index].itemName, g.names[index].c_str(), EDITA_MENU_ITEM_SIZE - 1);
    g_funcs[index].itemName[EDITA_MENU_ITEM_SIZE - 1] = '\0';
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
    if (translator.isEmpty() && translator.load(QLocale(), QStringLiteral("editahex"), QStringLiteral("_"), QStringLiteral(":/editahex/i18n")))
        QCoreApplication::installTranslator(&translator);
    g.data = data;
    g.panel = nullptr;
    std::memset(g_funcs, 0, sizeof g_funcs);
    setName(kPanel, QCoreApplication::translate("EditaHex", "Hex Editor"));
    setName(kOpenCurrent, QCoreApplication::translate("EditaHex", "Open Current File in Hex Editor"));
    setName(kOpenFile, QCoreApplication::translate("EditaHex", "Open File in Hex Editor…"));
    setName(kAbout, QCoreApplication::translate("EditaHex", "About Hex Editor…"));
    g_funcs[kPanel].pFunc = panelCommand;
    g_funcs[kPanel].pShKey = &g_panelKey;
    g_funcs[kOpenCurrent].pFunc = openCurrent;
    g_funcs[kOpenFile].pFunc = openFileCommand;
    g_funcs[kAbout].pFunc = aboutCommand;
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
    // Al cerrar Edita, los cambios sin guardar del editor hexadecimal
    if (n->nmhdr.hwndFrom == g.data.host && n->nmhdr.code == NPPN_BEFORESHUTDOWN && g.panel)
        g.panel->mayDiscard();
}

EDITA_PLUGIN_EXPORT intptr_t messageProc(unsigned int, uintptr_t, intptr_t)
{
    return 0;
}
