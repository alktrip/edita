// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Plugin "Python Script" para Edita: Python integrado con la misma API que el
// plugin PythonScript de Notepad++ (objetos notepad, editor, editor1, editor2 y
// console del módulo Npp). Este archivo es la parte nativa:
//   - las funciones que exporta un plugin (setInfo, getFuncsArray, beNotified…)
//     y su menú (New Script, Show Console, Scripts, Configuration…);
//   - la inicialización de CPython y el módulo _npp, el puente de bajo nivel con
//     data.send() sobre el que está escrito el paquete Npp (lib/Npp);
//   - la consola, un panel acoplable con un editor de Scintilla del host
//     (NPPM_CREATESCINTILLAHANDLE + NPPM_DMMREGASDCKDLG, como en N++).
// Como en PythonScript, los scripts se ejecutan en un hilo propio (y los callbacks
// asíncronos en otro): lo que piden al editor o a la ventana se pasa al hilo de
// la interfaz. startup.py y PYSCRF_SYNC se ejecutan en el hilo de la interfaz.

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "Config.h"
#include "NppPythonScript.h"

#include <edita_plugin.h>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QThread>
#include <QTimer>
#include <QTranslator>
#include <QUrl>
#include <QVBoxLayout>

#include <dlfcn.h>

#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <vector>

using pythonscript::Config;

namespace {

constexpr const char* kPluginName = "Python Script";
constexpr const char* kModuleName = "PythonScript.so";
constexpr int kLangPython = 22; // L_PYTHON

// IDOK… de Win32: lo que devuelve notepad.messageBox()
enum MessageBoxResult { IdOk = 1, IdCancel = 2, IdAbort = 3, IdRetry = 4, IdIgnore = 5, IdYes = 6, IdNo = 7, IdTryAgain = 10, IdContinue = 11 };

class ConsoleInput;

struct State
{
    EditaData data{};
    QString pluginDir;       // carpeta del .so (scripts y lib de la máquina)
    QString userBase;        // <plugins/config>/PythonScript (scripts y lib del usuario)
    std::unique_ptr<Config> config;

    QPointer<QWidget> console;
    void* consoleEditor = nullptr; // ScintillaEditBase* creado por el host
    QPointer<ConsoleInput> consoleInput;
    QPointer<QLabel> consolePrompt;
    bool dockRegistered = false;

    bool pythonReady = false;
    bool pythonFailed = false;
    PyObject* npp = nullptr; // paquete Npp
    QString previousScript;
    std::set<unsigned> scintillaCodes; // SCN_* con callbacks en Python
    std::set<unsigned> nppCodes;       // NPPN_* con callbacks en Python

    QPointer<QMenu> pluginMenu;
    std::vector<QPointer<QAction>> dynamicActions;
};

State g;

QString fromUtf8(const char* text)
{
    return QString::fromUtf8(text ? text : "");
}

// --- Hilos ------------------------------------------------------------------------------

bool onMainThread()
{
    return !qApp || QThread::currentThread() == qApp->thread();
}

// Ejecuta `work` en el hilo de la interfaz. Desde otro hilo se suelta el GIL
// mientras tanto: el hilo de la interfaz puede necesitarlo para los callbacks.
template<typename F>
auto inMainThread(F work) -> decltype(work())
{
    if (onMainThread())
        return work();
    using R = decltype(work());
    if constexpr (std::is_void_v<R>) {
        Py_BEGIN_ALLOW_THREADS
        QMetaObject::invokeMethod(qApp, work, Qt::BlockingQueuedConnection);
        Py_END_ALLOW_THREADS
    } else {
        R result{};
        Py_BEGIN_ALLOW_THREADS
        QMetaObject::invokeMethod(qApp, [&] { result = work(); }, Qt::BlockingQueuedConnection);
        Py_END_ALLOW_THREADS
        return result;
    }
}

intptr_t host(unsigned message, uintptr_t w = 0, intptr_t l = 0)
{
    return inMainThread([=] { return g.data.send(g.data.host, message, w, l); });
}

// Destino de un mensaje SCI_*: -1 la vista activa, 0 la principal, 1 la secundaria,
// cualquier otro valor es el puntero de un editor (la consola, createScintilla)
void* sciTarget(long long target)
{
    if (target == -1) {
        int view = 0;
        g.data.send(g.data.host, NPPM_GETCURRENTSCINTILLA, 0, reinterpret_cast<intptr_t>(&view));
        return view == 1 ? g.data.subEditor : g.data.mainEditor;
    }
    if (target == 0)
        return g.data.mainEditor;
    if (target == 1)
        return g.data.subEditor;
    return reinterpret_cast<void*>(target);
}

intptr_t sci(long long target, unsigned message, uintptr_t w, intptr_t l)
{
    return inMainThread([=] { return g.data.send(sciTarget(target), message, w, l); });
}

// --- Consola ------------------------------------------------------------------------------

void ensureConsole();
void consoleWrite(const QString& text, bool error);
void consolePush(const QString& line);

// Línea de entrada con historial (flechas arriba y abajo), como ConsoleDialog
class ConsoleInput : public QLineEdit
{
public:
    using QLineEdit::QLineEdit;
    QStringList history;
    qsizetype position = 0;

protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
            if (history.isEmpty())
                return;
            position += event->key() == Qt::Key_Up ? -1 : 1;
            position = std::clamp<qsizetype>(position, 0, history.size());
            setText(position < history.size() ? history[position] : QString());
            return;
        }
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            const QString line = text();
            if (!line.trimmed().isEmpty() && (history.isEmpty() || history.last() != line))
                history << line;
            position = history.size();
            clear();
            consolePush(line);
            return;
        }
        QLineEdit::keyPressEvent(event);
    }
};

void configureConsoleEditor(void* editor)
{
    auto send = [editor](unsigned m, uintptr_t w = 0, intptr_t l = 0) { return g.data.send(editor, m, w, l); };
    send(SCI_STYLESETFONT, STYLE_DEFAULT, reinterpret_cast<intptr_t>("Monospace"));
    send(SCI_STYLESETSIZE, STYLE_DEFAULT, 10);
    send(SCI_STYLECLEARALL);
    send(SCI_STYLESETFORE, 1, 0x0000FF); // errores en rojo (0x00BBGGRR)
    send(SCI_STYLESETFORE, 2, 0x808080); // entrada repetida en gris
    send(SCI_SETMARGINWIDTHN, 0, 0);
    send(SCI_SETMARGINWIDTHN, 1, 0);
    send(SCI_SETWRAPMODE, SC_WRAP_WORD);
    send(SCI_SETUNDOCOLLECTION, 0);
    send(SCI_SETREADONLY, 1);
}

void ensureConsole()
{
    if (g.console)
        return;
    // ConsoleDialog: salida (Scintilla del host) y una línea de entrada
    auto* widget = new QWidget;
    widget->setObjectName(QStringLiteral("pythonScriptConsole"));
    widget->setWindowTitle(QString::fromUtf8(kPluginName));
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(2, 2, 2, 2);
    g.consoleEditor = reinterpret_cast<void*>(g.data.send(g.data.host, NPPM_CREATESCINTILLAHANDLE, 0,
                                                          reinterpret_cast<intptr_t>(widget)));
    if (auto* output = static_cast<QWidget*>(g.consoleEditor)) {
        output->setObjectName(QStringLiteral("pythonScriptConsoleOutput"));
        configureConsoleEditor(g.consoleEditor);
        layout->addWidget(output, 1);
    }
    auto* row = new QHBoxLayout;
    g.consolePrompt = new QLabel(QStringLiteral(">>>"), widget);
    g.consoleInput = new ConsoleInput(widget);
    g.consoleInput->setObjectName(QStringLiteral("pythonScriptConsoleInput"));
    QFont mono(QStringLiteral("Monospace"));
    mono.setStyleHint(QFont::TypeWriter);
    g.consoleInput->setFont(mono);
    g.consolePrompt->setFont(mono);
    row->addWidget(g.consolePrompt);
    row->addWidget(g.consoleInput, 1);
    layout->addLayout(row);
    g.console = widget;

    EditaDockData dock{};
    dock.hClient = widget;
    dock.pszName = kPluginName;
    dock.dlgID = 1; // "Show Console"
    dock.uMask = DWS_DF_CONT_BOTTOM;
    dock.pszModuleName = kModuleName;
    g.dockRegistered = g.data.send(g.data.host, NPPM_DMMREGASDCKDLG, 0, reinterpret_cast<intptr_t>(&dock)) != 0;
}

void showConsole(bool show)
{
    ensureConsole();
    if (g.dockRegistered)
        g.data.send(g.data.host, show ? NPPM_DMMSHOW : NPPM_DMMHIDE, 0, reinterpret_cast<intptr_t>(g.console.data()));
    else
        g.console->setVisible(show);
    if (show && g.consoleInput)
        g.consoleInput->setFocus();
}

bool consoleVisible()
{
    return g.console && g.console->isVisibleTo(g.console->window()) && g.console->window()->isVisible()
        && g.console->parentWidget() && g.console->parentWidget()->isVisible();
}

void consoleWrite(const QString& text, bool error)
{
    ensureConsole();
    if (!g.consoleEditor || text.isEmpty())
        return;
    const QByteArray utf8 = text.toUtf8();
    auto send = [](unsigned m, uintptr_t w = 0, intptr_t l = 0) { return g.data.send(g.consoleEditor, m, w, l); };
    send(SCI_SETREADONLY, 0);
    const intptr_t start = send(SCI_GETLENGTH);
    send(SCI_APPENDTEXT, static_cast<uintptr_t>(utf8.size()), reinterpret_cast<intptr_t>(utf8.constData()));
    send(SCI_STARTSTYLING, static_cast<uintptr_t>(start));
    send(SCI_SETSTYLING, static_cast<uintptr_t>(utf8.size()), error ? 1 : 0);
    send(SCI_SETREADONLY, 1);
    send(SCI_GOTOPOS, static_cast<uintptr_t>(send(SCI_GETLENGTH)));
    // OPENCONSOLEONERROR: los errores abren la consola
    if (error && g.config && g.config->setting(QStringLiteral("OPENCONSOLEONERROR")) == QLatin1String("1"))
        showConsole(true);
}

void consoleClear()
{
    ensureConsole();
    if (!g.consoleEditor)
        return;
    g.data.send(g.consoleEditor, SCI_SETREADONLY, 0, 0);
    g.data.send(g.consoleEditor, SCI_CLEARALL, 0, 0);
    g.data.send(g.consoleEditor, SCI_SETREADONLY, 1, 0);
}

// --- Python -------------------------------------------------------------------------------

void ensurePython();
bool callNpp(const char* function, PyObject* args);
bool callNppWith(const char* function, const QString& text);

// Un objeto Python con referencia propia
struct PyRef
{
    PyObject* p = nullptr;
    PyRef() = default;
    explicit PyRef(PyObject* o) : p(o) {}
    ~PyRef() { Py_XDECREF(p); }
    PyRef(const PyRef&) = delete;
    PyRef& operator=(const PyRef&) = delete;
    explicit operator bool() const { return p != nullptr; }
};

// Mientras dura, este hilo tiene el GIL
struct Gil
{
    PyGILState_STATE state;
    Gil() : state(PyGILState_Ensure()) {}
    ~Gil() { PyGILState_Release(state); }
    Gil(const Gil&) = delete;
    Gil& operator=(const Gil&) = delete;
};

uintptr_t asWParam(PyObject* o)
{
    if (PyBool_Check(o))
        return o == Py_True ? 1 : 0;
    return static_cast<uintptr_t>(PyLong_AsUnsignedLongLongMask(o));
}

intptr_t asLParam(PyObject* o)
{
    if (PyBool_Check(o))
        return o == Py_True ? 1 : 0;
    return static_cast<intptr_t>(PyLong_AsUnsignedLongLongMask(o));
}

PyObject* fromIntptr(intptr_t v)
{
    return PyLong_FromLongLong(static_cast<long long>(v));
}

// _npp.sci(target, msg, w, l) -> int
PyObject* py_sci(PyObject*, PyObject* args)
{
    long long target = 0;
    unsigned int msg = 0;
    PyObject *w = nullptr, *l = nullptr;
    if (!PyArg_ParseTuple(args, "LIOO", &target, &msg, &w, &l))
        return nullptr;
    const uintptr_t wp = asWParam(w);
    const intptr_t lp = asLParam(l);
    if (PyErr_Occurred())
        return nullptr;
    return fromIntptr(sci(target, msg, wp, lp));
}

// _npp.sci_ls(target, msg, w, bytes) -> int: la cadena va en lParam
PyObject* py_sci_ls(PyObject*, PyObject* args)
{
    long long target = 0;
    unsigned int msg = 0;
    PyObject* w = nullptr;
    const char* text = nullptr;
    Py_ssize_t size = 0;
    if (!PyArg_ParseTuple(args, "LIOy#", &target, &msg, &w, &text, &size))
        return nullptr;
    const uintptr_t wp = asWParam(w);
    if (PyErr_Occurred())
        return nullptr;
    return fromIntptr(sci(target, msg, wp, reinterpret_cast<intptr_t>(text)));
}

// _npp.sci_ws(target, msg, bytes, l) -> int: la cadena va en wParam
PyObject* py_sci_ws(PyObject*, PyObject* args)
{
    long long target = 0;
    unsigned int msg = 0;
    const char* text = nullptr;
    Py_ssize_t size = 0;
    PyObject* l = nullptr;
    if (!PyArg_ParseTuple(args, "LIy#O", &target, &msg, &text, &size, &l))
        return nullptr;
    const intptr_t lp = asLParam(l);
    if (PyErr_Occurred())
        return nullptr;
    return fromIntptr(sci(target, msg, reinterpret_cast<uintptr_t>(text), lp));
}

// _npp.sci_ss(target, msg, bytes, bytes) -> int
PyObject* py_sci_ss(PyObject*, PyObject* args)
{
    long long target = 0;
    unsigned int msg = 0;
    const char *a = nullptr, *b = nullptr;
    Py_ssize_t sa = 0, sb = 0;
    if (!PyArg_ParseTuple(args, "LIy#y#", &target, &msg, &a, &sa, &b, &sb))
        return nullptr;
    return fromIntptr(sci(target, msg, reinterpret_cast<uintptr_t>(a), reinterpret_cast<intptr_t>(b)));
}

PyObject* bytesUpToNul(const std::string& buffer, Py_ssize_t limit)
{
    const std::size_t n = std::min<std::size_t>(static_cast<std::size_t>(std::max<Py_ssize_t>(limit, 0)), buffer.size());
    const std::size_t len = std::char_traits<char>::length(buffer.c_str()) < n ? std::char_traits<char>::length(buffer.c_str()) : n;
    return PyBytes_FromStringAndSize(buffer.data(), static_cast<Py_ssize_t>(len));
}

// _npp.sci_res(target, msg, w, size) -> bytes: cadena de resultado en lParam
PyObject* py_sci_res(PyObject*, PyObject* args)
{
    long long target = 0;
    unsigned int msg = 0;
    PyObject* w = nullptr;
    Py_ssize_t size = 0;
    if (!PyArg_ParseTuple(args, "LIOn", &target, &msg, &w, &size))
        return nullptr;
    const uintptr_t wp = asWParam(w);
    if (PyErr_Occurred() || size < 0)
        return PyErr_Occurred() ? nullptr : PyBytes_FromString("");
    std::string buffer(static_cast<std::size_t>(size) + 2, '\0');
    sci(target, msg, wp, reinterpret_cast<intptr_t>(buffer.data()));
    return bytesUpToNul(buffer, size);
}

// _npp.sci_wres(target, msg, key, size) -> bytes: clave en wParam, resultado en lParam
PyObject* py_sci_wres(PyObject*, PyObject* args)
{
    long long target = 0;
    unsigned int msg = 0;
    const char* key = nullptr;
    Py_ssize_t keySize = 0, size = 0;
    if (!PyArg_ParseTuple(args, "LIy#n", &target, &msg, &key, &keySize, &size))
        return nullptr;
    if (size < 0)
        return PyBytes_FromString("");
    std::string buffer(static_cast<std::size_t>(size) + 2, '\0');
    sci(target, msg, reinterpret_cast<uintptr_t>(key), reinterpret_cast<intptr_t>(buffer.data()));
    return bytesUpToNul(buffer, size);
}

// _npp.sci_range(target, start, end) -> bytes (SCI_GETTEXTRANGEFULL)
PyObject* py_sci_range(PyObject*, PyObject* args)
{
    long long target = 0, start = 0, end = 0;
    if (!PyArg_ParseTuple(args, "LLL", &target, &start, &end))
        return nullptr;
    if (end < 0)
        end = sci(target, SCI_GETLENGTH, 0, 0);
    if (end <= start)
        return PyBytes_FromString("");
    std::string buffer(static_cast<std::size_t>(end - start) + 1, '\0');
    Sci_TextRangeFull range{{static_cast<Sci_Position>(start), static_cast<Sci_Position>(end)}, buffer.data()};
    const intptr_t n = sci(target, SCI_GETTEXTRANGEFULL, 0, reinterpret_cast<intptr_t>(&range));
    return PyBytes_FromStringAndSize(buffer.data(), static_cast<Py_ssize_t>(std::max<intptr_t>(n, 0)));
}

// _npp.sci_styled(target, start, end) -> (bytes, [estilos])
PyObject* py_sci_styled(PyObject*, PyObject* args)
{
    long long target = 0, start = 0, end = 0;
    if (!PyArg_ParseTuple(args, "LLL", &target, &start, &end))
        return nullptr;
    if (end <= start)
        return Py_BuildValue("(y[])", "");
    std::string buffer(static_cast<std::size_t>(end - start) * 2 + 2, '\0');
    Sci_TextRangeFull range{{static_cast<Sci_Position>(start), static_cast<Sci_Position>(end)}, buffer.data()};
    const intptr_t n = sci(target, SCI_GETSTYLEDTEXTFULL, 0, reinterpret_cast<intptr_t>(&range));
    std::string text;
    PyRef styles(PyList_New(0));
    for (intptr_t i = 0; i + 1 < n * 2 + 1 && i + 1 < static_cast<intptr_t>(buffer.size()); i += 2) {
        if (i / 2 >= end - start)
            break;
        text += buffer[static_cast<std::size_t>(i)];
        PyRef style(PyLong_FromLong(static_cast<unsigned char>(buffer[static_cast<std::size_t>(i) + 1])));
        PyList_Append(styles.p, style.p);
    }
    return Py_BuildValue("(y#O)", text.data(), static_cast<Py_ssize_t>(text.size()), styles.p);
}

// _npp.sci_find(target, flags, start, end, bytes) -> (inicio, fin) o None
PyObject* py_sci_find(PyObject*, PyObject* args)
{
    long long target = 0, start = 0, end = 0;
    int flags = 0;
    const char* text = nullptr;
    Py_ssize_t size = 0;
    if (!PyArg_ParseTuple(args, "LiLLy#", &target, &flags, &start, &end, &text, &size))
        return nullptr;
    Sci_TextToFindFull find{};
    find.chrg.cpMin = static_cast<Sci_Position>(start);
    find.chrg.cpMax = static_cast<Sci_Position>(end);
    find.lpstrText = text;
    if (sci(target, SCI_FINDTEXTFULL, static_cast<uintptr_t>(flags), reinterpret_cast<intptr_t>(&find)) == -1)
        Py_RETURN_NONE;
    return Py_BuildValue("(LL)", static_cast<long long>(find.chrgText.cpMin), static_cast<long long>(find.chrgText.cpMax));
}

// _npp.npp(msg, w, l) -> int
PyObject* py_npp(PyObject*, PyObject* args)
{
    unsigned int msg = 0;
    PyObject *w = nullptr, *l = nullptr;
    if (!PyArg_ParseTuple(args, "IOO", &msg, &w, &l))
        return nullptr;
    const uintptr_t wp = asWParam(w);
    const intptr_t lp = asLParam(l);
    if (PyErr_Occurred())
        return nullptr;
    return fromIntptr(host(msg, wp, lp));
}

// _npp.npp_ls(msg, w, bytes) -> int
PyObject* py_npp_ls(PyObject*, PyObject* args)
{
    unsigned int msg = 0;
    PyObject* w = nullptr;
    const char* text = nullptr;
    Py_ssize_t size = 0;
    if (!PyArg_ParseTuple(args, "IOy#", &msg, &w, &text, &size))
        return nullptr;
    const uintptr_t wp = asWParam(w);
    if (PyErr_Occurred())
        return nullptr;
    return fromIntptr(host(msg, wp, reinterpret_cast<intptr_t>(text)));
}

// _npp.npp_ss(msg, bytes, bytes) -> int
PyObject* py_npp_ss(PyObject*, PyObject* args)
{
    unsigned int msg = 0;
    const char *a = nullptr, *b = nullptr;
    Py_ssize_t sa = 0, sb = 0;
    if (!PyArg_ParseTuple(args, "Iy#y#", &msg, &a, &sa, &b, &sb))
        return nullptr;
    return fromIntptr(host(msg, reinterpret_cast<uintptr_t>(a), reinterpret_cast<intptr_t>(b)));
}

// _npp.npp_str(msg, w) -> str: mensajes "(tamaño | id, char* búfer)". Con w = None
// el tamaño es el del búfer (NPPM_GETFULLCURRENTPATH…); si no, w es un id
// (NPPM_GETFULLPATHFROMBUFFERID, NPPM_GETLANGUAGENAME) y primero se pide la longitud.
PyObject* py_npp_str(PyObject*, PyObject* args)
{
    unsigned int msg = 0;
    PyObject* w = Py_None;
    if (!PyArg_ParseTuple(args, "I|O", &msg, &w))
        return nullptr;
    std::string buffer;
    if (w == Py_None) {
        buffer.assign(4096, '\0');
        if (!host(msg, buffer.size(), reinterpret_cast<intptr_t>(buffer.data())))
            Py_RETURN_NONE;
    } else {
        const uintptr_t id = asWParam(w);
        if (PyErr_Occurred())
            return nullptr;
        const intptr_t length = host(msg, id, 0);
        if (length < 0)
            Py_RETURN_NONE;
        buffer.assign(static_cast<std::size_t>(length) + 1, '\0');
        host(msg, id, reinterpret_cast<intptr_t>(buffer.data()));
    }
    return PyUnicode_DecodeUTF8(buffer.c_str(), static_cast<Py_ssize_t>(std::char_traits<char>::length(buffer.c_str())),
                                "replace");
}

// _npp.npp_intout(msg, w) -> (resultado, valor): mensajes con un int* en lParam
PyObject* py_npp_intout(PyObject*, PyObject* args)
{
    unsigned int msg = 0;
    PyObject* w = nullptr;
    if (!PyArg_ParseTuple(args, "IO", &msg, &w))
        return nullptr;
    const uintptr_t wp = asWParam(w);
    if (PyErr_Occurred())
        return nullptr;
    int value = 0;
    const intptr_t result = host(msg, wp, reinterpret_cast<intptr_t>(&value));
    return Py_BuildValue("(Li)", static_cast<long long>(result), value);
}

PyObject* py_console_write(PyObject*, PyObject* args)
{
    const char* text = nullptr;
    int error = 0;
    if (!PyArg_ParseTuple(args, "s|p", &text, &error))
        return nullptr;
    const QString s = QString::fromUtf8(text);
    inMainThread([&] { consoleWrite(s, error != 0); });
    Py_RETURN_NONE;
}

PyObject* py_console_clear(PyObject*, PyObject*)
{
    inMainThread([] { consoleClear(); });
    Py_RETURN_NONE;
}

PyObject* py_console_show(PyObject*, PyObject* args)
{
    int show = 1;
    if (!PyArg_ParseTuple(args, "|p", &show))
        return nullptr;
    inMainThread([show] { showConsole(show != 0); });
    Py_RETURN_NONE;
}

PyObject* py_console_prompt(PyObject*, PyObject* args)
{
    int more = 0;
    if (!PyArg_ParseTuple(args, "p", &more))
        return nullptr;
    inMainThread([more] {
        if (g.consolePrompt)
            g.consolePrompt->setText(more ? QStringLiteral("...") : QStringLiteral(">>>"));
    });
    Py_RETURN_NONE;
}

PyObject* py_console_editor(PyObject*, PyObject*)
{
    inMainThread([] { ensureConsole(); });
    return PyLong_FromVoidPtr(g.consoleEditor);
}

// notepad.messageBox: los botones y el icono con los valores MB_* de Win32
PyObject* py_message_box(PyObject*, PyObject* args)
{
    const char *text = nullptr, *title = nullptr;
    int flags = 0;
    if (!PyArg_ParseTuple(args, "ss|i", &text, &title, &flags))
        return nullptr;
    const QString message = QString::fromUtf8(text);
    const QString caption = QString::fromUtf8(title);
    const int result = inMainThread([&] {
        QMessageBox box(QApplication::activeWindow());
        box.setWindowTitle(caption);
        box.setText(message);
        switch (flags & 0xF0) {
        case 0x10: box.setIcon(QMessageBox::Critical); break;
        case 0x20: box.setIcon(QMessageBox::Question); break;
        case 0x30: box.setIcon(QMessageBox::Warning); break;
        case 0x40: box.setIcon(QMessageBox::Information); break;
        default: break;
        }
        std::vector<std::pair<QMessageBox::StandardButton, int>> buttons;
        switch (flags & 0xF) {
        case 1: buttons = {{QMessageBox::Ok, IdOk}, {QMessageBox::Cancel, IdCancel}}; break;
        case 2: buttons = {{QMessageBox::Abort, IdAbort}, {QMessageBox::Retry, IdRetry}, {QMessageBox::Ignore, IdIgnore}}; break;
        case 3: buttons = {{QMessageBox::Yes, IdYes}, {QMessageBox::No, IdNo}, {QMessageBox::Cancel, IdCancel}}; break;
        case 4: buttons = {{QMessageBox::Yes, IdYes}, {QMessageBox::No, IdNo}}; break;
        case 5: buttons = {{QMessageBox::Retry, IdRetry}, {QMessageBox::Cancel, IdCancel}}; break;
        case 6: buttons = {{QMessageBox::Cancel, IdCancel}, {QMessageBox::Retry, IdTryAgain}, {QMessageBox::Ignore, IdContinue}}; break;
        default: buttons = {{QMessageBox::Ok, IdOk}}; break;
        }
        QMessageBox::StandardButtons standard;
        for (const auto& [button, id] : buttons)
            standard |= button;
        box.setStandardButtons(standard);
        const std::size_t defaultIndex = static_cast<std::size_t>((flags >> 8) & 0xF);
        if (defaultIndex < buttons.size())
            box.setDefaultButton(buttons[defaultIndex].first);
        const int clicked = box.exec();
        for (const auto& [button, id] : buttons) {
            if (clicked == button)
                return id;
        }
        return static_cast<int>(IdCancel);
    });
    return PyLong_FromLong(result);
}

// notepad.prompt: el texto, o None si se cancela
PyObject* py_prompt(PyObject*, PyObject* args)
{
    const char *prompt = nullptr, *title = nullptr, *initial = "";
    if (!PyArg_ParseTuple(args, "ss|s", &prompt, &title, &initial))
        return nullptr;
    const QString label = QString::fromUtf8(prompt);
    const QString caption = QString::fromUtf8(title);
    const QString defaultText = QString::fromUtf8(initial);
    bool ok = false;
    const QString text = inMainThread([&] {
        return QInputDialog::getMultiLineText(QApplication::activeWindow(), caption, label, defaultText, &ok);
    });
    if (!ok)
        Py_RETURN_NONE;
    const QByteArray utf8 = text.toUtf8();
    return PyUnicode_FromStringAndSize(utf8.constData(), utf8.size());
}

QString stripMnemonic(QString text)
{
    text.remove(QRegularExpression(QStringLiteral("&(?!&)")));
    text.replace(QStringLiteral("&&"), QStringLiteral("&"));
    const qsizetype tab = text.indexOf(QLatin1Char('\t'));
    return (tab >= 0 ? text.left(tab) : text).trimmed();
}

// Busca `option` en el menú (y sus submenús) y lo ejecuta
bool triggerInMenu(QMenu* menu, const QString& option)
{
    if (!menu)
        return false;
    for (QAction* action : menu->actions()) {
        if (action->menu()) {
            if (triggerInMenu(action->menu(), option))
                return true;
        } else if (!action->isSeparator() && stripMnemonic(action->text()).compare(option, Qt::CaseInsensitive) == 0) {
            action->trigger();
            return true;
        }
    }
    return false;
}

QMenu* findSubMenu(const QList<QAction*>& actions, const QString& name)
{
    for (QAction* action : actions) {
        if (action->menu() && stripMnemonic(action->text()).compare(name, Qt::CaseInsensitive) == 0)
            return action->menu();
    }
    return nullptr;
}

// notepad.runMenuCommand(menú, opción): el menú se busca por su texto en la barra de menús
PyObject* py_run_menu_command(PyObject*, PyObject* args)
{
    const char *menuName = nullptr, *option = nullptr;
    if (!PyArg_ParseTuple(args, "ss", &menuName, &option))
        return nullptr;
    const QString menuText = QString::fromUtf8(menuName);
    const QString optionText = QString::fromUtf8(option);
    const bool found = inMainThread([&] {
        auto* bar = reinterpret_cast<QMenuBar*>(g.data.send(g.data.host, NPPM_GETMENUHANDLE, NPPMAINMENU, 0));
        if (!bar)
            return false;
        // Primero los menús de la barra; si no, cualquier submenú con ese nombre
        if (QMenu* menu = findSubMenu(bar->actions(), menuText))
            return triggerInMenu(menu, optionText);
        for (QAction* top : bar->actions()) {
            if (top->menu()) {
                if (QMenu* sub = findSubMenu(top->menu()->actions(), menuText))
                    return triggerInMenu(sub, optionText);
            }
        }
        return false;
    });
    return PyBool_FromLong(found);
}

// notepad.runPluginCommand(plugin, opción)
PyObject* py_run_plugin_command(PyObject*, PyObject* args)
{
    const char *pluginName = nullptr, *option = nullptr;
    if (!PyArg_ParseTuple(args, "ss", &pluginName, &option))
        return nullptr;
    const QString pluginText = QString::fromUtf8(pluginName);
    const QString optionText = QString::fromUtf8(option);
    const bool found = inMainThread([&] {
        auto* plugins = reinterpret_cast<QMenu*>(g.data.send(g.data.host, NPPM_GETMENUHANDLE, NPPPLUGINMENU, 0));
        return plugins && triggerInMenu(findSubMenu(plugins->actions(), pluginText), optionText);
    });
    return PyBool_FromLong(found);
}

// Ejecuta fn() en Python cuando se vuelva al bucle de eventos (callbacks asíncronos)
PyObject* py_call_later(PyObject*, PyObject* args)
{
    PyObject* fn = nullptr;
    int ms = 0;
    if (!PyArg_ParseTuple(args, "O|i", &fn, &ms))
        return nullptr;
    Py_INCREF(fn);
    QTimer::singleShot(ms, qApp, [fn] {
        Gil gil;
        PyRef result(PyObject_CallNoArgs(fn));
        if (!result)
            PyErr_Print();
        Py_DECREF(fn);
    });
    Py_RETURN_NONE;
}

// Qué notificaciones interesan a los callbacks de Python (así no se llama a Python
// en cada SCN_PAINTED)
PyObject* py_set_callback_codes(PyObject*, PyObject* args)
{
    PyObject *scintilla = nullptr, *npp = nullptr;
    if (!PyArg_ParseTuple(args, "OO", &scintilla, &npp))
        return nullptr;
    auto fill = [](PyObject* iterable, std::set<unsigned>& out) {
        out.clear();
        PyRef it(PyObject_GetIter(iterable));
        if (!it)
            return false;
        while (PyObject* item = PyIter_Next(it.p)) {
            out.insert(static_cast<unsigned>(PyLong_AsLong(item)));
            Py_DECREF(item);
        }
        return !PyErr_Occurred();
    };
    std::set<unsigned> sci, npp2;
    if (!fill(scintilla, sci) || !fill(npp, npp2))
        return nullptr;
    inMainThread([&] {
        g.scintillaCodes = std::move(sci);
        g.nppCodes = std::move(npp2);
    });
    Py_RETURN_NONE;
}

PyObject* py_flash(PyObject*, PyObject* args)
{
    long long target = 0;
    int ms = 50;
    if (!PyArg_ParseTuple(args, "L|i", &target, &ms))
        return nullptr;
    inMainThread([=] {
        // editor.flash: el fondo cambia un momento
        void* editor = sciTarget(target);
        const intptr_t back = g.data.send(editor, SCI_STYLEGETBACK, STYLE_DEFAULT, 0);
        const intptr_t flashed = back ^ 0x202020;
        for (uintptr_t style = 0; style <= STYLE_DEFAULT; ++style)
            g.data.send(editor, SCI_STYLESETBACK, style, flashed);
        QTimer::singleShot(ms, qApp, [editor, back] {
            for (uintptr_t style = 0; style <= STYLE_DEFAULT; ++style)
                g.data.send(editor, SCI_STYLESETBACK, style, back);
        });
    });
    Py_RETURN_NONE;
}

PyObject* py_flash_window(PyObject*, PyObject* args)
{
    int count = 1, ms = 0;
    if (!PyArg_ParseTuple(args, "|ii", &count, &ms))
        return nullptr;
    inMainThread([=] {
        if (QWidget* window = QApplication::activeWindow())
            QApplication::alert(window, count * ms);
    });
    Py_RETURN_NONE;
}

PyObject* py_create_scintilla(PyObject*, PyObject*)
{
    return PyLong_FromVoidPtr(reinterpret_cast<void*>(host(NPPM_CREATESCINTILLAHANDLE, 0, 0)));
}

PyObject* py_destroy_scintilla(PyObject*, PyObject* args)
{
    long long editor = 0;
    if (!PyArg_ParseTuple(args, "L", &editor))
        return nullptr;
    return PyBool_FromLong(host(NPPM_DESTROYSCINTILLAHANDLE, 0, static_cast<intptr_t>(editor)) != 0);
}

PyObject* py_paths(PyObject*, PyObject*)
{
    return Py_BuildValue("(ss)", g.pluginDir.toUtf8().constData(), g.userBase.toUtf8().constData());
}

PyMethodDef kMethods[] = {
    {"sci", py_sci, METH_VARARGS, "sci(target, msg, wParam, lParam) -> int"},
    {"sci_ls", py_sci_ls, METH_VARARGS, nullptr},
    {"sci_ws", py_sci_ws, METH_VARARGS, nullptr},
    {"sci_ss", py_sci_ss, METH_VARARGS, nullptr},
    {"sci_res", py_sci_res, METH_VARARGS, nullptr},
    {"sci_wres", py_sci_wres, METH_VARARGS, nullptr},
    {"sci_range", py_sci_range, METH_VARARGS, nullptr},
    {"sci_styled", py_sci_styled, METH_VARARGS, nullptr},
    {"sci_find", py_sci_find, METH_VARARGS, nullptr},
    {"npp", py_npp, METH_VARARGS, "npp(msg, wParam, lParam) -> int"},
    {"npp_ls", py_npp_ls, METH_VARARGS, nullptr},
    {"npp_ss", py_npp_ss, METH_VARARGS, nullptr},
    {"npp_str", py_npp_str, METH_VARARGS, nullptr},
    {"npp_intout", py_npp_intout, METH_VARARGS, nullptr},
    {"console_write", py_console_write, METH_VARARGS, nullptr},
    {"console_clear", py_console_clear, METH_NOARGS, nullptr},
    {"console_show", py_console_show, METH_VARARGS, nullptr},
    {"console_editor", py_console_editor, METH_NOARGS, nullptr},
    {"console_prompt", py_console_prompt, METH_VARARGS, nullptr},
    {"message_box", py_message_box, METH_VARARGS, nullptr},
    {"prompt", py_prompt, METH_VARARGS, nullptr},
    {"run_menu_command", py_run_menu_command, METH_VARARGS, nullptr},
    {"run_plugin_command", py_run_plugin_command, METH_VARARGS, nullptr},
    {"call_later", py_call_later, METH_VARARGS, nullptr},
    {"set_callback_codes", py_set_callback_codes, METH_VARARGS, nullptr},
    {"flash", py_flash, METH_VARARGS, nullptr},
    {"flash_window", py_flash_window, METH_VARARGS, nullptr},
    {"create_scintilla", py_create_scintilla, METH_NOARGS, nullptr},
    {"destroy_scintilla", py_destroy_scintilla, METH_VARARGS, nullptr},
    {"paths", py_paths, METH_NOARGS, nullptr},
    {nullptr, nullptr, 0, nullptr},
};

PyModuleDef kModule = {PyModuleDef_HEAD_INIT, "_npp", "Puente de bajo nivel entre Npp y Edita", -1, kMethods,
                       nullptr, nullptr, nullptr, nullptr};

QString selfPath()
{
    Dl_info info{};
    if (dladdr(reinterpret_cast<void*>(&selfPath), &info) && info.dli_fname)
        return QString::fromUtf8(info.dli_fname);
    return {};
}

// Síncrono, en el hilo de la interfaz (startup.py, PYSCRF_SYNC)
void runFile(const QString& path)
{
    ensurePython();
    if (g.pythonReady)
        callNppWith("_run_file", path);
}

void runStatement(const QString& statement)
{
    ensurePython();
    if (g.pythonReady)
        callNppWith("_run_statement", statement);
}

bool scriptRunning()
{
    if (!g.pythonReady)
        return false;
    Gil gil;
    PyRef fn(PyObject_GetAttrString(g.npp, "_script_running"));
    PyRef result(fn ? PyObject_CallNoArgs(fn.p) : nullptr);
    if (!result)
        PyErr_Clear();
    return result && PyObject_IsTrue(result.p) == 1;
}

// En el hilo de los scripts (PythonHandler::runScript sin synchronous): solo uno a la vez
bool submit(const char* function, const QString& text)
{
    ensurePython();
    if (!g.pythonReady)
        return false;
    if (scriptRunning()) {
        if (g.config->setting(QStringLiteral("DISABLEPOPUPWARNING")) == QLatin1String("0"))
            QMessageBox::information(QApplication::activeWindow(), QString::fromUtf8(kPluginName),
                                     QCoreApplication::translate("PythonScript", "Another script is currently running.  Running two scripts at the same "
                                                                               "time could produce unpredicable results, and is therefore disabled."));
        return false;
    }
    return callNppWith(function, text);
}

void consolePush(const QString& line)
{
    ensurePython();
    if (!g.pythonReady)
        return;
    consoleWrite((g.consolePrompt ? g.consolePrompt->text() : QStringLiteral(">>>")) + QLatin1Char(' ') + line
                     + QLatin1Char('\n'),
                 false);
    // La consola usa el hilo de los scripts: la interfaz no se bloquea
    callNppWith("_console_submit", line);
}

void ensurePython()
{
    if (g.pythonReady || g.pythonFailed)
        return;

    // libpython en el espacio global (las extensiones de lib-dynload lo necesitan) y
    // este plugin sin descargar: el intérprete vive mientras dure el proceso
    Dl_info info{};
    if (dladdr(reinterpret_cast<void*>(&Py_Initialize), &info) && info.dli_fname)
        dlopen(info.dli_fname, RTLD_NOW | RTLD_GLOBAL | RTLD_NOLOAD);
    const QByteArray self = selfPath().toUtf8();
    if (!self.isEmpty())
        dlopen(self.constData(), RTLD_NOW | RTLD_NOLOAD | RTLD_NODELETE);

    if (!Py_IsInitialized()) {
        // PythonHandler::initPython
        PyConfig config;
        PyConfig_InitPythonConfig(&config);
        config.install_signal_handlers = 0;
        config.parse_argv = 0;
        config.site_import = 1;
        config.user_site_directory = 1;
        config.use_environment = 1;
        // Paquetes autocontenidos (AppImage): la biblioteca estándar va en <prefijo>/lib/pythonX.Y,
        // junto a <prefijo>/lib/edita/plugins/PythonScript; sin esto Python usaría la del sistema
        const QString prefix = QDir(QDir(g.pluginDir).absoluteFilePath(QStringLiteral("../../../.."))).canonicalPath();
        const QString stdlib = QStringLiteral("%1/lib/python%2.%3/os.py").arg(prefix).arg(PY_MAJOR_VERSION).arg(PY_MINOR_VERSION);
        PyStatus status = PyStatus_Ok();
        if (!prefix.isEmpty() && prefix != QLatin1String("/usr") && QFileInfo::exists(stdlib))
            status = PyConfig_SetString(&config, &config.home, prefix.toStdWString().c_str());
        if (!PyStatus_Exception(status))
            status = Py_InitializeFromConfig(&config);
        PyConfig_Clear(&config);
        if (PyStatus_Exception(status)) {
            g.pythonFailed = true;
            QMessageBox::warning(QApplication::activeWindow(), QString::fromUtf8(kPluginName),
                                 QCoreApplication::translate("PythonScript", "Could not start Python: %1").arg(fromUtf8(status.err_msg)));
            return;
        }
        PyEval_SaveThread(); // el GIL se toma en cada entrada (PyGILState_Ensure)
    }

    Gil gil;
    // sys.path: lib y scripts de la máquina y del usuario, delante (PREFERINSTALLEDPYTHON=0)
    PyObject* sysPath = PySys_GetObject("path");
    const QStringList dirs = {QDir(g.pluginDir).filePath(QStringLiteral("lib")), QDir(g.userBase).filePath(QStringLiteral("lib")),
                              QDir(g.pluginDir).filePath(QStringLiteral("scripts")),
                              QDir(g.userBase).filePath(QStringLiteral("scripts"))};
    for (qsizetype i = 0; sysPath && i < dirs.size(); ++i) {
        PyRef dir(PyUnicode_FromString(dirs[i].toUtf8().constData()));
        if (PySequence_Contains(sysPath, dir.p) != 1)
            PyList_Insert(sysPath, i, dir.p);
    }
    // sys.argv: la línea de órdenes de Edita (initSysArgv)
    PyRef argv(PyList_New(0));
    for (const QString& arg : QCoreApplication::arguments()) {
        PyRef item(PyUnicode_FromString(arg.toUtf8().constData()));
        PyList_Append(argv.p, item.p);
    }
    PySys_SetObject("argv", argv.p);

    // _npp nuevo para esta ventana; Npp se vuelve a importar con él
    PyRef module(PyModule_Create(&kModule));
    PyObject* modules = PyImport_GetModuleDict();
    PyDict_SetItemString(modules, "_npp", module.p);
    PyRef keys(PyDict_Keys(modules));
    for (Py_ssize_t i = 0; i < PyList_Size(keys.p); ++i) {
        PyObject* key = PyList_GetItem(keys.p, i);
        const char* name = PyUnicode_AsUTF8(key);
        if (name && (std::strcmp(name, "Npp") == 0 || std::strncmp(name, "Npp.", 4) == 0))
            PyDict_DelItem(modules, key);
    }
    g.npp = PyImport_ImportModule("Npp");
    if (!g.npp) {
        PyErr_Print();
        g.pythonFailed = true;
        return;
    }
    g.pythonReady = true;

    // PythonHandler::runStartupScripts: el de la máquina y el del usuario
    for (const QString& base : {g.pluginDir, g.userBase}) {
        const QString startup = QDir(base).filePath(QStringLiteral("scripts/startup.py"));
        if (QFileInfo::exists(startup))
            callNpp("_run_file", Py_BuildValue("(s)", startup.toUtf8().constData()));
    }
}

// Npp.<function>(texto)
bool callNppWith(const char* function, const QString& text)
{
    Gil gil;
    return callNpp(function, Py_BuildValue("(s)", text.toUtf8().constData()));
}

// Llama a Npp.<function>(*args); se queda con la referencia de args
bool callNpp(const char* function, PyObject* args)
{
    PyRef owned(args);
    if (!g.npp)
        return false;
    PyRef fn(PyObject_GetAttrString(g.npp, function));
    PyRef result(fn ? PyObject_CallObject(fn.p, args) : nullptr);
    if (!result) {
        PyErr_Print();
        return false;
    }
    return true;
}

// NPPN_SHUTDOWN o un host nuevo: se sueltan los objetos de la ventana anterior.
// El intérprete sigue (no se puede volver a iniciar de forma fiable).
void releaseHost()
{
    if (g.pythonReady) {
        Gil gil;
        callNpp("_shutdown", PyTuple_New(0));
        Py_CLEAR(g.npp);
        PyObject* modules = PyImport_GetModuleDict();
        if (PyDict_GetItemString(modules, "_npp"))
            PyDict_DelItemString(modules, "_npp");
    }
    g.pythonReady = false;
    g.scintillaCodes.clear();
    g.nppCodes.clear();
    g.console = nullptr;
    g.consoleEditor = nullptr;
    g.consoleInput = nullptr;
    g.consolePrompt = nullptr;
    g.dockRegistered = false;
    g.pluginMenu = nullptr;
    g.dynamicActions.clear();
}

// --- Notificaciones hacia Python ---------------------------------------------------------

void dispatchScintilla(const SCNotification* n)
{
    const int view = n->nmhdr.hwndFrom == g.data.mainEditor ? 0 : n->nmhdr.hwndFrom == g.data.subEditor ? 1 : -1;
    if (view < 0)
        return;
    Gil gil;
    // Los mismos campos que ScintillaWrapper::notify
    PyRef args(PyDict_New());
    auto set = [&](const char* key, long long value) {
        PyRef v(PyLong_FromLongLong(value));
        PyDict_SetItemString(args.p, key, v.p);
    };
    set("code", n->nmhdr.code);
    set("position", n->position);
    set("ch", n->ch);
    set("modifiers", n->modifiers);
    set("modificationType", n->modificationType);
    set("length", n->length);
    set("linesAdded", n->linesAdded);
    set("message", n->message);
    set("wParam", static_cast<long long>(n->wParam));
    set("lParam", n->lParam);
    set("line", n->line);
    set("foldLevelNow", n->foldLevelNow);
    set("foldLevelPrev", n->foldLevelPrev);
    set("margin", n->margin);
    set("listType", n->listType);
    set("x", n->x);
    set("y", n->y);
    set("token", n->token);
    set("annotationLinesAdded", n->annotationLinesAdded);
    set("updated", n->updated);
    set("listCompletionMethod", n->listCompletionMethod);
    set("characterSource", n->characterSource);
    if (n->text && n->length > 0) {
        PyRef text(PyUnicode_DecodeUTF8(n->text, static_cast<Py_ssize_t>(n->length), "replace"));
        PyDict_SetItemString(args.p, "text", text.p);
    } else if (n->text) {
        PyRef text(PyUnicode_FromString(n->text));
        PyDict_SetItemString(args.p, "text", text.p);
    }
    Py_INCREF(args.p);
    callNpp("_dispatch_scintilla", Py_BuildValue("(iIN)", view, n->nmhdr.code, args.p));
}

void dispatchNotepad(const SCNotification* n)
{
    Gil gil;
    PyRef args(Py_BuildValue("{s:I,s:K}", "code", n->nmhdr.code, "bufferID", static_cast<unsigned long long>(n->nmhdr.idFrom)));
    Py_INCREF(args.p);
    callNpp("_dispatch_notepad", Py_BuildValue("(IN)", n->nmhdr.code, args.p));
}

// --- Menú --------------------------------------------------------------------------------

void newScript();
void toggleConsole();
void stopScript();
void runPreviousScript();
void showConfiguration();
void showHelp();
void showAbout();

EditaFuncItem g_funcs[] = {
    {"New Script", newScript, 0, false, nullptr},
    {"Show Console", toggleConsole, 0, false, nullptr},
    {"", nullptr, 0, false, nullptr},
    {"Stop Script", stopScript, 0, false, nullptr},
    {"", nullptr, 0, false, nullptr},
    {"Run Previous Script", runPreviousScript, 0, false, nullptr},
    {"", nullptr, 0, false, nullptr},
    {"Configuration...", showConfiguration, 0, false, nullptr},
    {"", nullptr, 0, false, nullptr},
    {"Context-Help", showHelp, 0, false, nullptr},
    {"About", showAbout, 0, false, nullptr},
};
constexpr int kRunPreviousIndex = 5;
constexpr int kConfigurationIndex = 7;

QAction* commandAction(int index)
{
    return g.pluginMenu ? g.pluginMenu->findChild<QAction*>(QStringLiteral("pluginCommand%1").arg(g_funcs[index].cmdID))
                        : nullptr;
}

void runScript(const QString& path)
{
    // Ctrl+clic en el menú Scripts: abrir el script para editarlo (MenuManager)
    if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
        host(NPPM_DOOPEN, 0, reinterpret_cast<intptr_t>(path.toUtf8().constData()));
        return;
    }
    if (!submit("_submit_file", path))
        return;
    g.previousScript = path;
    if (QAction* previous = commandAction(kRunPreviousIndex))
        previous->setText(QStringLiteral("Run Previous Script (%1)").arg(QFileInfo(path).completeBaseName()));
}

void fillScriptsMenu(QMenu* menu, const QString& dir)
{
    const QDir directory(dir);
    for (const QFileInfo& sub : directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        auto* child = menu->addMenu(sub.fileName());
        fillScriptsMenu(child, sub.absoluteFilePath());
        if (child->isEmpty())
            delete child->menuAction();
    }
    for (const QFileInfo& file : directory.entryInfoList({QStringLiteral("*.py")}, QDir::Files, QDir::Name)) {
        if (dir == QDir(g.pluginDir).filePath(QStringLiteral("scripts")) && file.fileName() == QLatin1String("startup.py"))
            continue;
        QAction* action = menu->addAction(file.completeBaseName());
        action->setToolTip(file.absoluteFilePath());
        QObject::connect(action, &QAction::triggered, [path = file.absoluteFilePath()] { runScript(path); });
    }
}

// MenuManager::refreshScriptsMenu: submenú Scripts (máquina, luego usuario) y los
// scripts de la configuración, encima de "Configuration..."
void rebuildDynamicMenu()
{
    if (!g.pluginMenu) {
        auto* plugins = reinterpret_cast<QMenu*>(g.data.send(g.data.host, NPPM_GETMENUHANDLE, NPPPLUGINMENU, 0));
        g.pluginMenu = plugins ? findSubMenu(plugins->actions(), QString::fromUtf8(kPluginName)) : nullptr;
    }
    if (!g.pluginMenu)
        return;
    for (const QPointer<QAction>& action : g.dynamicActions)
        delete action.data();
    g.dynamicActions.clear();
    QAction* configuration = commandAction(kConfigurationIndex);

    auto* scripts = new QMenu(QStringLiteral("Scripts"), g.pluginMenu);
    scripts->setObjectName(QStringLiteral("pythonScriptScriptsMenu"));
    fillScriptsMenu(scripts, QDir(g.pluginDir).filePath(QStringLiteral("scripts")));
    if (!scripts->isEmpty())
        scripts->addSeparator();
    fillScriptsMenu(scripts, g.config->userScriptsDir());
    g.pluginMenu->insertMenu(configuration, scripts);
    g.dynamicActions.emplace_back(scripts->menuAction());

    if (!g.config->menuItems().isEmpty()) {
        for (const QString& item : g.config->menuItems()) {
            auto* action = new QAction(QFileInfo(item).completeBaseName(), g.pluginMenu);
            action->setToolTip(item);
            QObject::connect(action, &QAction::triggered, [item] { runScript(item); });
            g.pluginMenu->insertAction(configuration, action);
            g.dynamicActions.emplace_back(action);
        }
    }
    auto* separator = new QAction(g.pluginMenu);
    separator->setSeparator(true);
    g.pluginMenu->insertAction(configuration, separator);
    g.dynamicActions.emplace_back(separator);
}

void newScript()
{
    QDir().mkpath(g.config->userScriptsDir());
    QString path = QFileDialog::getSaveFileName(QApplication::activeWindow(), QCoreApplication::translate("PythonScript", "New Script"),
                                                g.config->userScriptsDir(),
                                                QCoreApplication::translate("PythonScript", "Python Source Files (*.py);;All Files (*)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QLatin1String(".py")) && QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".py");
    QFile file(path);
    if (!file.exists() && file.open(QIODevice::WriteOnly))
        file.close();
    host(NPPM_DOOPEN, 0, reinterpret_cast<intptr_t>(path.toUtf8().constData()));
    host(NPPM_SETBUFFERLANGTYPE, static_cast<uintptr_t>(host(NPPM_GETCURRENTBUFFERID)), kLangPython);
}

void toggleConsole()
{
    ensurePython(); // la consola sirve para escribir Python
    const bool show = !consoleVisible();
    showConsole(show);
}

void runPreviousScript()
{
    if (!g.previousScript.isEmpty())
        submit("_submit_file", g.previousScript);
}

// PythonHandler::stopScript: KeyboardInterrupt en el hilo del script
void stopScript()
{
    if (!g.pythonReady)
        return;
    Gil gil;
    PyRef fn(PyObject_GetAttrString(g.npp, "_script_thread_id"));
    PyRef ident(fn ? PyObject_CallNoArgs(fn.p) : nullptr);
    if (ident && ident.p != Py_None)
        PyThreadState_SetAsyncExc(PyLong_AsUnsignedLong(ident.p), PyExc_KeyboardInterrupt);
    else
        PyErr_Clear();
}

void showConfiguration()
{
    // ShortcutDlg (simplificado): scripts del menú y cuándo se inicia Python
    QDialog dialog(QApplication::activeWindow());
    dialog.setWindowTitle(QCoreApplication::translate("PythonScript", "Python Script Configuration"));
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(QCoreApplication::translate("PythonScript", "Scripts in the Python Script menu:"), &dialog));
    auto* list = new QListWidget(&dialog);
    list->setObjectName(QStringLiteral("menuItems"));
    for (const QString& item : g.config->menuItems()) {
        auto* row = new QListWidgetItem(QFileInfo(item).completeBaseName(), list);
        row->setData(Qt::UserRole, item);
        row->setToolTip(item);
    }
    layout->addWidget(list);
    auto* buttons = new QHBoxLayout;
    auto* add = new QPushButton(QCoreApplication::translate("PythonScript", "Add…"), &dialog);
    auto* remove = new QPushButton(QCoreApplication::translate("PythonScript", "Remove"), &dialog);
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addStretch();
    layout->addLayout(buttons);
    QObject::connect(add, &QPushButton::clicked, &dialog, [&] {
        const QString path = QFileDialog::getOpenFileName(&dialog, QStringLiteral("Script"), g.config->userScriptsDir(),
                                                          QCoreApplication::translate("PythonScript", "Python Source Files (*.py)"));
        if (path.isEmpty())
            return;
        auto* row = new QListWidgetItem(QFileInfo(path).completeBaseName(), list);
        row->setData(Qt::UserRole, path);
        row->setToolTip(path);
    });
    QObject::connect(remove, &QPushButton::clicked, &dialog, [list] { delete list->currentItem(); });
    auto* form = new QFormLayout;
    auto* startup = new QComboBox(&dialog);
    startup->setObjectName(QStringLiteral("startup"));
    startup->addItem(QCoreApplication::translate("PythonScript", "LAZY (when first used)"), QStringLiteral("LAZY"));
    startup->addItem(QCoreApplication::translate("PythonScript", "ATSTARTUP (when Edita starts)"), QStringLiteral("ATSTARTUP"));
    startup->setCurrentIndex(g.config->startsAtStartup() ? 1 : 0);
    form->addRow(QCoreApplication::translate("PythonScript", "Initialisation:"), startup);
    layout->addLayout(form);
    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(box);
    if (dialog.exec() != QDialog::Accepted)
        return;
    QStringList items;
    for (int i = 0; i < list->count(); ++i)
        items << list->item(i)->data(Qt::UserRole).toString();
    g.config->setMenuItems(items);
    g.config->setSetting(QStringLiteral("STARTUP"), startup->currentData().toString());
    g.config->save();
    rebuildDynamicMenu();
}

void showHelp()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://npppythonscript.sourceforge.net/docs/latest/")));
}

void showAbout()
{
    QString version = QStringLiteral(PY_VERSION);
    QMessageBox::about(QApplication::activeWindow(), QStringLiteral("Python Script"),
                       QCoreApplication::translate("PythonScript", "<b>Python Script for Edita</b><br>"
                                                 "Embedded Python %1, with the API of the PythonScript plugin "
                                                 "for Notepad++ (Dave Brotherstone and contributors).<br>"
                                                 "GNU GPL v3 or later license.")
                           .arg(version));
}

// Mensajes de otros plugins (NPPM_MSGTOPLUGIN): PYSCR_EXECSCRIPT, PYSCR_EXECSTATEMENT…
intptr_t handlePluginMessage(const EditaCommunicationInfo* info)
{
    if (!info)
        return 0;
    if (info->internalMsg == PYSCR_SHOWCONSOLE) {
        ensurePython();
        showConsole(true);
        return 1;
    }
    if (info->internalMsg != PYSCR_EXECSCRIPT && info->internalMsg != PYSCR_EXECSTATEMENT)
        return 0;
    auto* exec = static_cast<PythonScript_Exec*>(info->info);
    if (!exec || exec->structVersion != 1 || !exec->script)
        return 0;
    exec->deliverySuccess = 1;
    const QString script = QString::fromUtf8(exec->script);
    const bool isFile = info->internalMsg == PYSCR_EXECSCRIPT;
    if (exec->flags & PYSCRF_SYNC)
        isFile ? runFile(script) : runStatement(script);
    else
        submit(isFile ? "_submit_file" : "_submit_statement", script);
    return 1;
}

} // namespace

// --- Interfaz del plugin -------------------------------------------------------------------

EDITA_PLUGIN_EXPORT int edita_plugin_api_version(void)
{
    return EDITA_PLUGIN_API_VERSION;
}

EDITA_PLUGIN_EXPORT const char* edita_plugin_version(void)
{
    return "1.0.0";
}

EDITA_PLUGIN_EXPORT void setInfo(EditaData data)
{
    // Los diálogos en el idioma de Edita (QLocale); los menús quedan en inglés, como
    // en N++, porque los scripts los llaman por su nombre (notepad.runPluginCommand)
    static QTranslator translator;
    if (translator.isEmpty() && translator.load(QLocale(), QStringLiteral("pythonscript"), QStringLiteral("_"),
                                                QStringLiteral(":/pythonscript/i18n")))
        QCoreApplication::installTranslator(&translator);
    releaseHost(); // otra ventana (o la primera)
    g.data = data;
    g.pluginDir = QFileInfo(selfPath()).absolutePath();
    std::string buffer(4096, '\0');
    data.send(data.host, NPPM_GETPLUGINSCONFIGDIR, buffer.size(), reinterpret_cast<intptr_t>(buffer.data()));
    const QString configDir = QString::fromUtf8(buffer.c_str());
    g.userBase = QDir(configDir).filePath(QStringLiteral("PythonScript"));
    g.config = std::make_unique<Config>(QDir(configDir).filePath(QStringLiteral("PythonScriptStartup.cnf")),
                                        QDir(g.pluginDir).filePath(QStringLiteral("scripts")),
                                        QDir(g.userBase).filePath(QStringLiteral("scripts")));
    g.config->load();
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

EDITA_PLUGIN_EXPORT void beNotified(SCNotification* n)
{
    const unsigned code = n->nmhdr.code;
    if (n->nmhdr.hwndFrom == g.data.host) {
        switch (code) {
        case NPPN_READY:
            rebuildDynamicMenu();
            if (g.config->startsAtStartup())
                ensurePython();
            break;
        case NPPN_FILESAVED: {
            // Guardar un script actualiza el menú Scripts
            std::string path(4096, '\0');
            g.data.send(g.data.host, NPPM_GETFULLPATHFROMBUFFERID, n->nmhdr.idFrom, reinterpret_cast<intptr_t>(path.data()));
            const QString saved = QString::fromUtf8(path.c_str());
            if (saved.startsWith(g.config->userScriptsDir()) || saved.startsWith(g.config->machineScriptsDir()))
                rebuildDynamicMenu();
            break;
        }
        default:
            break;
        }
        if (g.pythonReady && g.nppCodes.count(code))
            dispatchNotepad(n);
        if (code == NPPN_SHUTDOWN)
            releaseHost();
        return;
    }
    if (g.pythonReady && g.scintillaCodes.count(code))
        dispatchScintilla(n);
}

EDITA_PLUGIN_EXPORT intptr_t messageProc(unsigned int message, uintptr_t, intptr_t lParam)
{
    if (message == NPPM_MSGTOPLUGIN)
        return handlePluginMessage(reinterpret_cast<const EditaCommunicationInfo*>(lParam));
    return 0;
}
