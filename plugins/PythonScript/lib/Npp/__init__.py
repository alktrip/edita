# This file is part of Edita.
# Copyright (C) 2026 Edita contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Módulo Npp: la API del plugin PythonScript de Notepad++ (notepad, editor,
# editor1, editor2, console y los enums) sobre el puente _npp de Edita.
# Los métodos de Scintilla de Editor se generan desde Scintilla.iface
# (_generated.py, con tools/gen-pythonscript.py); aquí están los métodos de
# ayuda de PythonScript, Notepad y Console.

import bisect
import code as _code
import re as _re
import queue as _queue
import subprocess as _subprocess
import sys as _sys
import threading as _threading
import traceback as _traceback
import xml.etree.ElementTree as _ET

import _npp

from ._generated import *  # noqa: F401,F403  (enums de Scintilla y de N++)
from ._generated import ScintillaGenerated, _Enum, _bytes, _str, LANGTYPE, NOTIFICATION, MENUCOMMAND, SCINTILLANOTIFICATION, MODIFICATIONFLAGS

__all__ = [name for name in dir() if not name.startswith("_") and name not in ("bisect",)]

# --- Mensajes de Notepad++ (Notepad_plus_msgs.h) -------------------------------------------

_NPPMSG = 0x0400 + 1000
_RUNCOMMAND_USER = 0x0400 + 3000
NPPM_GETCURRENTSCINTILLA = _NPPMSG + 4
NPPM_GETCURRENTLANGTYPE = _NPPMSG + 5
NPPM_SETCURRENTLANGTYPE = _NPPMSG + 6
NPPM_GETNBOPENFILES = _NPPMSG + 7
NPPM_GETNBSESSIONFILES = _NPPMSG + 13
NPPM_SAVECURRENTSESSION = _NPPMSG + 16
NPPM_GETNBUSERLANG = _NPPMSG + 22
NPPM_GETCURRENTDOCINDEX = _NPPMSG + 23
NPPM_SETSTATUSBAR = _NPPMSG + 24
NPPM_GETMENUHANDLE = _NPPMSG + 25
NPPM_ACTIVATEDOC = _NPPMSG + 28
NPPM_LAUNCHFINDINFILESDLG = _NPPMSG + 29
NPPM_LOADSESSION = _NPPMSG + 34
NPPM_RELOADFILE = _NPPMSG + 36
NPPM_SWITCHTOFILE = _NPPMSG + 37
NPPM_SAVECURRENTFILE = _NPPMSG + 38
NPPM_SAVEALLFILES = _NPPMSG + 39
NPPM_MAKECURRENTBUFFERDIRTY = _NPPMSG + 44
NPPM_GETPLUGINSCONFIGDIR = _NPPMSG + 46
NPPM_MENUCOMMAND = _NPPMSG + 48
NPPM_GETNPPVERSION = _NPPMSG + 50
NPPM_HIDETABBAR = _NPPMSG + 51
NPPM_ISTABBARHIDDEN = _NPPMSG + 52
NPPM_GETPOSFROMBUFFERID = _NPPMSG + 57
NPPM_GETFULLPATHFROMBUFFERID = _NPPMSG + 58
NPPM_GETBUFFERIDFROMPOS = _NPPMSG + 59
NPPM_GETCURRENTBUFFERID = _NPPMSG + 60
NPPM_RELOADBUFFERID = _NPPMSG + 61
NPPM_GETBUFFERLANGTYPE = _NPPMSG + 64
NPPM_SETBUFFERLANGTYPE = _NPPMSG + 65
NPPM_GETBUFFERENCODING = _NPPMSG + 66
NPPM_SETBUFFERENCODING = _NPPMSG + 67
NPPM_GETBUFFERFORMAT = _NPPMSG + 68
NPPM_SETBUFFERFORMAT = _NPPMSG + 69
NPPM_HIDEMENU = _NPPMSG + 72
NPPM_ISMENUHIDDEN = _NPPMSG + 73
NPPM_HIDESTATUSBAR = _NPPMSG + 74
NPPM_ISSTATUSBARHIDDEN = _NPPMSG + 75
NPPM_DOOPEN = _NPPMSG + 77
NPPM_SAVECURRENTFILEAS = _NPPMSG + 78
NPPM_ALLOCATECMDID = _NPPMSG + 81
NPPM_ALLOCATEMARKER = _NPPMSG + 82
NPPM_GETLANGUAGENAME = _NPPMSG + 83
NPPM_GETLANGUAGEDESC = _NPPMSG + 84
NPPM_GETCURRENTVIEW = _NPPMSG + 88
NPPM_GETEDITORDEFAULTFOREGROUNDCOLOR = _NPPMSG + 90
NPPM_GETEDITORDEFAULTBACKGROUNDCOLOR = _NPPMSG + 91
NPPM_SAVEFILE = _NPPMSG + 94
NPPM_GETPLUGINHOMEPATH = _NPPMSG + 97
NPPM_GETSETTINGSONCLOUDPATH = _NPPMSG + 98
NPPM_ISAUTOINDENTON = _NPPMSG + 105
NPPM_CREATELEXER = _NPPMSG + 110
NPPM_ADDSCNMODIFIEDFLAGS = _NPPMSG + 117
NPPM_ISDARKMODEENABLED = _NPPMSG + 107
NPPM_GETCURRENTCMDLINE = _NPPMSG + 109
NPPM_GETBOOKMARKID = _NPPMSG + 111
NPPM_ALLOCATEINDICATOR = _NPPMSG + 113
NPPM_GETFULLCURRENTPATH = _RUNCOMMAND_USER + 1
NPPM_GETNPPDIRECTORY = _RUNCOMMAND_USER + 7

# --- Enums propios de PythonScript (NotepadPython.cpp) ---------------------------------------


class FORMATTYPE(_Enum):
    WIN = 0
    MAC = 1
    UNIX = 2


class BUFFERENCODING(_Enum):
    ENC8BIT = 0
    UTF8 = 1
    UCS2BE = 2
    UCS2LE = 3
    COOKIE = 4
    UTF8_NOBOM = 4
    ANSI = 5
    UCS2BE_NOBOM = 6
    UCS2LE_NOBOM = 7


class STATUSBARSECTION(_Enum):
    DOCTYPE = 0
    DOCSIZE = 1
    CURPOS = 2
    EOFFORMAT = 3
    UNICODETYPE = 4
    TYPINGMODE = 5


class MESSAGEBOXFLAGS(_Enum):
    OK = 0x00
    OKCANCEL = 0x01
    ABORTRETRYIGNORE = 0x02
    YESNOCANCEL = 0x03
    YESNO = 0x04
    RETRYCANCEL = 0x05
    CANCELTRYCONTINUE = 0x06
    ICONHAND = 0x10
    ICONQUESTION = 0x20
    ICONEXCLAMATION = 0x30
    ICONASTERISK = 0x40
    ICONWARNING = 0x30
    ICONERROR = 0x10
    ICONINFORMATION = 0x40
    ICONSTOP = 0x10
    DEFBUTTON1 = 0x000
    DEFBUTTON2 = 0x100
    DEFBUTTON3 = 0x200
    DEFBUTTON4 = 0x300
    RESULTOK = 1
    RESULTCANCEL = 2
    RESULTABORT = 3
    RESULTRETRY = 4
    RESULTIGNORE = 5
    RESULTYES = 6
    RESULTNO = 7
    RESULTTRYAGAIN = 10
    RESULTCONTINUE = 11


class LINENUMWIDTHMODE(_Enum):
    DYNAMIC = 0
    CONSTANT = 1


class AUTOINDENTMODE(_Enum):
    STANDARD = 0
    CLIKE = 1
    CUSTOM = 2


class WINVER(_Enum):
    UNKNOWN = 0


class PLATFORM(_Enum):
    UNKNOWN = 0
    X86 = 1
    X64 = 2
    ARM64 = 3


# --- Callbacks ------------------------------------------------------------------------------

class _Callbacks:
    """Funciones registradas para unas notificaciones (síncronas o no)"""

    def __init__(self):
        self.entries = []  # (función, códigos, síncrona)

    def add(self, function, codes, sync):
        self.entries.append((function, frozenset(int(c) for c in codes), sync))

    def clear(self, *args):
        function, events = None, None
        for a in args:
            if callable(a):
                function = a
            elif a is not None:
                events = frozenset(int(c) for c in a)
        kept = []
        for f, codes, sync in self.entries:
            if function is not None and f != function:
                kept.append((f, codes, sync))
                continue
            if events is not None:
                rest = codes - events
                if rest:
                    kept.append((f, rest, sync))
                continue
        self.entries = kept if args else []

    def codes(self):
        out = set()
        for _, codes, _ in self.entries:
            out |= codes
        return out


class _Worker:
    """Un hilo que ejecuta trabajos en orden (PyProducerConsumer de PythonScript)"""

    def __init__(self, name):
        self.name = name
        self.queue = _queue.Queue()
        self.busy = False      # hay trabajo en curso o pendiente
        self.running = False   # un trabajo se está ejecutando (se puede interrumpir)
        self.cancelled = False # Stop Script llegó antes de que empezara el trabajo ya sacado de la cola
        self.lock = _threading.Lock()
        self.thread = None

    def submit(self, job):
        if self.thread is None or not self.thread.is_alive():
            self.thread = _threading.Thread(target=self._loop, name=self.name, daemon=True)
            self.thread.start()
        self.queue.put(job)

    def stop(self):
        if self.thread is not None and self.thread.is_alive():
            self.queue.put(None)

    def interrupt(self):
        """Stop Script: quita lo pendiente y devuelve el hilo que hay que interrumpir
        (KeyboardInterrupt) si un trabajo se está ejecutando; si no, None"""
        with self.lock:
            drained = 0
            while True:
                try:
                    self.queue.get_nowait()
                    drained += 1
                except _queue.Empty:
                    break
            if self.running and self.thread is not None and self.thread.is_alive():
                return self.thread.ident
            if self.busy and drained == 0:
                # Sacado de la cola pero aún sin empezar: no se ejecuta
                self.cancelled = True
            if self.busy:
                self.busy = False
                _sys.stderr.write("KeyboardInterrupt\n")
            return None

    def _step(self):
        job = self.queue.get()
        if job is None:
            return False
        with self.lock:
            if self.cancelled:
                self.cancelled = False
                job = None
            else:
                self.busy = True
                self.running = True
        if job is not None:
            try:
                job()
            except BaseException:
                _traceback.print_exc()
        with self.lock:
            self.running = False
            self.busy = self.queue.qsize() > 0
        return True

    def _loop(self):
        while True:
            try:
                if not self._step():
                    return
            except BaseException:
                # Un KeyboardInterrupt de Stop Script que llega justo fuera del trabajo
                # (al terminar): el hilo sigue vivo y deja de estar ocupado
                with self.lock:
                    self.running = False
                    self.busy = self.queue.qsize() > 0


# Los callbacks asíncronos se ejecutan en su propio hilo, en el orden en que llegaron
_callback_worker = _Worker("PythonScript callbacks")
# Los scripts (menú, consola, otros plugins) en otro, para que la interfaz siga viva
_script_worker = _Worker("PythonScript")


def _call(entries, code, args):
    for function, codes, sync in list(entries):
        if code not in codes:
            continue
        if sync:
            try:
                function(args)
            except Exception:
                _sys.excepthook(*_sys.exc_info())
        else:
            _callback_worker.submit(lambda f=function, a=dict(args): f(a))


def _update_codes():
    scintilla = set()
    for e in (editor, editor1, editor2):
        scintilla |= e._callbacks.codes()
    _npp.set_callback_codes(scintilla, notepad._callbacks.codes())


def _dispatch_scintilla(view, code, args):
    current = _npp.npp(NPPM_GETCURRENTVIEW, 0, 0)
    if view == current:
        _call(editor._callbacks.entries, code, args)
    _call((editor1 if view == 0 else editor2)._callbacks.entries, code, args)


def _dispatch_notepad(code, args):
    _call(notepad._callbacks.entries, code, args)


def _shutdown():
    for e in (editor, editor1, editor2):
        e._callbacks.clear()
    notepad._callbacks.clear()
    _callback_worker.stop()
    _script_worker.stop()


# --- Ejecución de scripts -------------------------------------------------------------------

def _print_script_error():
    """El traceback sin el marco de Npp, como PyRun_SimpleFile en PythonScript"""
    kind, value, tb = _sys.exc_info()
    _traceback.print_exception(kind, value, tb.tb_next if tb is not None else None)


def _run_file(path):
    """Ejecuta un archivo en __main__ (PyRun_SimpleFile en PythonHandler::runScriptWorker)"""
    import __main__
    namespace = __main__.__dict__
    had_file, old_file = "__file__" in namespace, namespace.get("__file__")
    namespace["__file__"] = path
    try:
        with open(path, "rb") as f:
            source = f.read()
        exec(compile(source, path, "exec"), namespace)
    except SystemExit:
        pass
    except BaseException:
        _print_script_error()
    finally:
        if had_file:
            namespace["__file__"] = old_file
        else:
            namespace.pop("__file__", None)


def _run_statement(statement):
    import __main__
    try:
        exec(compile(statement, "<string>", "exec"), __main__.__dict__)
    except SystemExit:
        pass
    except BaseException:
        _print_script_error()


def _script_running():
    return _script_worker.busy


def _script_thread_id():
    t = _script_worker.thread
    return t.ident if t is not None and t.is_alive() and _script_worker.running else None


def _stop_script():
    return _script_worker.interrupt()


def _submit_file(path):
    _script_worker.busy = True
    _script_worker.submit(lambda: _run_file(path))


def _submit_statement(statement):
    _script_worker.busy = True
    _script_worker.submit(lambda: _run_statement(statement))


# --- Editor ---------------------------------------------------------------------------------

class _Match:
    """Parecido a re.Match, con posiciones del documento (bytes), como en PythonScript"""

    def __init__(self, match, to_doc):
        self._m = match
        self._to_doc = to_doc

    def group(self, *groups):
        return self._m.group(*groups)

    def groups(self, default=None):
        return self._m.groups(default)

    def groupdict(self, default=None):
        raise RuntimeError("groupdict() not supported under Notepad++")

    @property
    def re(self):
        raise RuntimeError("re not supported under Notepad++")

    @property
    def pos(self):
        raise RuntimeError("pos not supported under Notepad++")

    @property
    def endpos(self):
        raise RuntimeError("endpos not supported under Notepad++")

    @property
    def string(self):
        raise RuntimeError("string not supported under Notepad++")

    def expand(self, template):
        return self._m.expand(template)

    def start(self, group=0):
        s = self._m.start(group)
        return -1 if s < 0 else self._to_doc(s)

    def end(self, group=0):
        e = self._m.end(group)
        return -1 if e < 0 else self._to_doc(e)

    def span(self, group=0):
        return (self.start(group), self.end(group))

    @property
    def lastindex(self):
        return self._m.lastindex

    @property
    def lastgroup(self):
        return self._m.lastgroup

    def __getitem__(self, group):
        return self._m.group(group)


def _boost_pattern(pattern):
    """Lo que Boost (Perl) escribe distinto que Python: grupos con nombre y \\< \\>"""
    pattern = _re.sub(r"\(\?<(?=[A-Za-z_])", "(?P<", pattern)
    pattern = _re.sub(r"\(\?'([A-Za-z_]\w*)'", r"(?P<\1>", pattern)
    pattern = _re.sub(r"\\k(?:<(\w+)>|'(\w+)'|\{(\w+)\})",
                      lambda m: "(?P=%s)" % (m.group(1) or m.group(2) or m.group(3)), pattern)
    pattern = pattern.replace("\\<", r"\b(?=\w)").replace("\\>", r"\b(?<=\w)")
    return pattern


_ESCAPES = {"a": "\a", "e": "\x1b", "f": "\f", "n": "\n", "r": "\r", "t": "\t", "v": "\v"}


def _boost_format(fmt, m, text=""):
    """match.format(fmt, format_all) de Boost: $n ${n} $& $` $' \\n, condicionales
    (?n sí:no), agrupación con paréntesis y \\l \\u \\L \\U \\E"""
    state = {"mode": None, "one": None}
    out = []

    def group(key):
        try:
            g = m.group(int(key) if str(key).isdigit() else key)
        except (IndexError, ValueError):
            return ""
        return g or ""

    def emit(piece):
        for ch in piece:
            if state["one"] == "u":
                ch, state["one"] = ch.upper(), None
            elif state["one"] == "l":
                ch, state["one"] = ch.lower(), None
            elif state["mode"] == "U":
                ch = ch.upper()
            elif state["mode"] == "L":
                ch = ch.lower()
            out.append(ch)

    def number(i):
        if i < len(fmt) and fmt[i] == "{":
            j = fmt.find("}", i)
            if j > 0:
                return fmt[i + 1:j], j + 1
        j = i
        while j < len(fmt) and fmt[j].isdigit():
            j += 1
        return fmt[i:j], j

    def parse(i, stops, active):
        """Recorre hasta un carácter de `stops`; escribe solo si `active`"""
        while i < len(fmt):
            c = fmt[i]
            if c in stops:
                return i
            if c == "$" and i + 1 < len(fmt):
                n = fmt[i + 1]
                if n == "&":
                    active and emit(m.group(0))
                    i += 2
                elif n == "$":
                    active and emit("$")
                    i += 2
                elif n == "`":
                    active and emit(text[:m.start()] if text else "")
                    i += 2
                elif n == "'":
                    active and emit(text[m.end():] if text else "")
                    i += 2
                elif n == "+":
                    active and emit(group(m.lastindex) if m.lastindex else "")
                    i += 2
                elif n.isdigit() or n == "{":
                    key, i = number(i + 1)
                    active and emit(group(key))
                else:
                    active and emit("$")
                    i += 1
            elif c == "\\" and i + 1 < len(fmt):
                n = fmt[i + 1]
                if n.isdigit():
                    active and emit(group(n))
                elif n in _ESCAPES:
                    active and emit(_ESCAPES[n])
                elif n in "lu":
                    if active:
                        state["one"] = n
                elif n in "LU":
                    if active:
                        state["mode"] = n
                elif n == "E":
                    if active:
                        state["mode"] = None
                elif n == "x":
                    j = i + 2
                    if j < len(fmt) and fmt[j] == "{":
                        k = fmt.find("}", j)
                        code, nxt = fmt[j + 1:k], k + 1
                    else:
                        code, nxt = fmt[j:j + 2], j + 2
                    try:
                        active and emit(chr(int(code, 16)))
                    except ValueError:
                        pass
                    i = nxt
                    continue
                else:
                    active and emit(n)
                i += 2
            elif c == "(":
                if i + 1 < len(fmt) and fmt[i + 1] == "?":
                    key, j = number(i + 2)
                    matched = bool(key) and m.group(int(key) if key.isdigit() else key) is not None if key else False
                    j = parse(j, ":)", active and matched)
                    if j < len(fmt) and fmt[j] == ":":
                        j = parse(j + 1, ")", active and not matched)
                    i = j + 1
                else:
                    i = parse(i + 1, ")", active) + 1
            elif c == "?" and ")" not in stops and ":" not in stops:
                key, j = number(i + 1)
                if not key:
                    active and emit("?")
                    i += 1
                    continue
                matched = m.group(int(key) if key.isdigit() else key) is not None
                j = parse(j, ":", active and matched)
                if j < len(fmt):
                    j = parse(j + 1, "", active and not matched)
                i = j
            else:
                active and emit(c)
                i += 1
        return i

    parse(0, "", True)
    return "".join(out)


class Cell:
    """Texto con estilos para editor.addStyledText(Cell('texto', [estilos]))"""

    def __init__(self, text, styles):
        self.text = text
        self.styles = list(styles)

    def __bytes__(self):
        data = _bytes(self.text)
        styles = self.styles + [0] * max(0, len(data) - len(self.styles))
        return bytes(b for pair in zip(data, (int(s) & 0xFF for s in styles)) for b in pair)


class Editor(ScintillaGenerated):
    """Un editor de Scintilla: la vista activa (editor), una concreta (editor1,
    editor2) o uno creado por el plugin (console.editor, notepad.createScintilla())"""

    WHOLEDOC = 0x100000  # flag de PythonScript: ^ y $ para todo el documento

    def __init__(self, target=-1):
        self._target = target
        self._callbacks = _Callbacks()

    @property
    def hwnd(self):
        return self._target

    # Llamadas de bajo nivel (las usa _generated.py)
    def _sci(self, msg, w=0, l=0):
        return _npp.sci(self._target, msg, w, l)

    def _sci_ls(self, msg, w, s):
        return _npp.sci_ls(self._target, msg, w, s)

    def _sci_ws(self, msg, s, l):
        return _npp.sci_ws(self._target, msg, s, l)

    def _sci_ss(self, msg, a, b):
        return _npp.sci_ss(self._target, msg, a, b)

    def _sci_res(self, msg, w, size):
        return _npp.sci_res(self._target, msg, w, size)

    def _sci_wres(self, msg, key, size):
        return _npp.sci_wres(self._target, msg, key, size)

    def _range_bytes(self, start, end):
        return _npp.sci_range(self._target, int(start), int(end))

    def _range(self, start, end):
        return _str(self._range_bytes(start, end))

    def _styled(self, start, end):
        text, styles = _npp.sci_styled(self._target, int(start), int(end))
        return (_str(text), styles)

    def _find(self, flags, start, end, text):
        return _npp.sci_find(self._target, flags, int(start), int(end), _bytes(text))

    def _getLine(self, line):
        if line >= self.getLineCount() or line < 0:
            raise IndexError("Line number out of range")
        start = self.positionFromLine(line)
        return self._range(start, start + self.lineLength(line))

    # --- Métodos de ayuda de PythonScript (ScintillaWrapper.cpp) -----------------------
    def write(self, text):
        self.addText(text)

    def flash(self, milliseconds=50):
        _npp.flash(self._target, int(milliseconds))

    def forEachLine(self, function):
        line = 0
        lines = self.getLineCount()
        while line < lines:
            result = function(self.getLine(line).rstrip("\r\n"), line, lines)
            line += 1 if result is None else int(result)
            lines = self.getLineCount()

    def deleteLine(self, line):
        start = self.positionFromLine(line)
        end = self.positionFromLine(line + 1) if line + 1 < self.getLineCount() else self.getLength()
        self.setTargetRange(start, end)
        self.replaceTarget("")

    def replaceLine(self, line, newContents):
        self.setTargetRange(self.positionFromLine(line), self.getLineEndPosition(line))
        self.replaceTarget(newContents)

    def replaceWholeLine(self, line, newContents):
        start = self.positionFromLine(line)
        end = self.positionFromLine(line + 1) if line + 1 < self.getLineCount() else self.getLength()
        self.setTargetRange(start, end)
        self.replaceTarget(newContents)

    def setTarget(self, start, end):
        self.setTargetRange(start, end)

    def getUserLineSelection(self):
        start, end = self.getSelectionStart(), self.getSelectionEnd()
        if start == end:
            return (0, self.getLineCount() - 1)
        return (self.lineFromPosition(start), self.lineFromPosition(end))

    def getUserCharSelection(self):
        start, end = self.getSelectionStart(), self.getSelectionEnd()
        if start == end:
            return (0, self.getLength())
        return (start, end)

    def getWord(self, position=None, useOnlyWordChars=True):
        if position is None:
            position = self.getCurrentPos()
        only = True if useOnlyWordChars is None else bool(useOnlyWordChars)
        return self._range(self.wordStartPosition(position, only), self.wordEndPosition(position, only))

    def getCurrentWord(self):
        return self.getWord()

    def setLexerLanguage(self, language):
        """SCI_SETLEXERLANGUAGE ya no existe en Scintilla 5: el lexer lo crea Lexilla
        (NPPM_CREATELEXER, como en PythonScript 3)"""
        lexer = _npp.npp_ls(NPPM_CREATELEXER, 0, _bytes(language))
        if lexer:
            self.setILexer(lexer)

    # Callbacks de Scintilla
    def callback(self, function, eventsList):
        self._callbacks.add(function, eventsList, False)
        _update_codes()
        return True

    def callbackSync(self, function, eventsList):
        self._callbacks.add(function, eventsList, True)
        _update_codes()
        return True

    def clearCallbacks(self, *args):
        self._callbacks.clear(*args)
        _update_codes()

    # --- Buscar y reemplazar -----------------------------------------------------------
    def _matches(self, search, flags, startPosition, endPosition, maxCount, literal):
        if search is None or search == "":
            raise TypeError("search must not be empty or None")
        flags = int(flags or 0)
        start = 0 if startPosition is None or startPosition < 0 else int(startPosition)
        end = self.getLength() if endPosition is None or endPosition < 0 else int(endPosition)
        raw = self._range_bytes(start, end)
        text = raw.decode("utf-8", errors="surrogateescape")
        # Fines de línea: ^ y $ con \n como en N++ (CRLF y CR se tratan como un fin de línea)
        dropped = []
        if "\r" in text:
            parts, i, n = [], 0, len(text)
            out_index = 0  # longitud del texto normalizado hasta aquí
            for m in _re.finditer(r"\r\n?", text):
                parts.append(text[i:m.start()])
                out_index += m.start() - i
                parts.append("\n")
                out_index += 1
                if m.end() - m.start() == 2:
                    dropped.append(out_index)  # tras el \n normalizado falta un carácter
                i = m.end()
            parts.append(text[i:n])
            text = "".join(parts)
        pattern = _re.escape(str(search)) if literal else _boost_pattern(str(search))
        reflags = flags & ~Editor.WHOLEDOC & ~_re.MULTILINE
        if not literal and not (flags & Editor.WHOLEDOC):
            reflags |= _re.MULTILINE
        regex = _re.compile(pattern, reflags)

        # Índice en el texto normalizado → posición del documento (bytes). Se avanza
        # de forma incremental porque los resultados llegan en orden.
        decoded = raw.decode("utf-8", errors="surrogateescape")
        cache = {"index": 0, "bytes": 0}

        def to_doc(index):
            original = index + bisect.bisect_right(dropped, index) if dropped else index
            if original < cache["index"]:
                cache["index"], cache["bytes"] = 0, 0
            cache["bytes"] += len(decoded[cache["index"]:original].encode("utf-8", errors="surrogateescape"))
            cache["index"] = original
            return start + cache["bytes"]

        found = []
        for m in regex.finditer(text):
            found.append(m)
            if maxCount and maxCount > 0 and len(found) >= maxCount:
                break
        self._last_text = text
        return found, to_doc

    def _replace_all(self, search, replace, flags, startPosition, endPosition, maxCount, literal):
        if replace is None:
            raise TypeError("replace must not be None")
        found, to_doc = self._matches(search, flags, startPosition, endPosition, maxCount, literal)
        edits = []
        for m in found:
            span = (to_doc(m.start()), to_doc(m.end()))
            if callable(replace):
                result = replace(_Match(m, to_doc))
                new = "" if result is None else str(result)
            elif literal:
                new = str(replace)
            else:
                new = _boost_format(str(replace), m, self._last_text)
            edits.append((span, new))
        if not edits:
            return 0
        self.beginUndoAction()
        try:
            for (s, e), new in reversed(edits):
                self.setTargetRange(s, e)
                self.replaceTarget(new)
        finally:
            self.endUndoAction()
        return len(edits)

    def replace(self, search, replace, flags=0, startPosition=None, endPosition=None, maxCount=0):
        return self._replace_all(search, replace, flags, startPosition, endPosition, maxCount, True)

    def rereplace(self, search, replace, flags=0, startPosition=None, endPosition=None, maxCount=0):
        return self._replace_all(search, replace, flags, startPosition, endPosition, maxCount, False)

    def research(self, search, matchFunction, flags=0, startPosition=None, endPosition=None, maxCount=0):
        found, to_doc = self._matches(search, flags, startPosition, endPosition, maxCount, False)
        for m in found:
            matchFunction(_Match(m, to_doc))

    def search(self, search, handlerFunction, flags=0, startPosition=None, endPosition=None, maxCount=0):
        found, to_doc = self._matches(search, flags & _re.IGNORECASE, startPosition, endPosition, maxCount, True)
        for m in found:
            handlerFunction(_Match(m, to_doc))

    # Para usar editor como sys.stdout
    def flush(self):
        pass


# --- Notepad --------------------------------------------------------------------------------

def _path(path):
    return _bytes(str(path))


class Notepad:
    """La ventana de Notepad++ (Edita): documentos, menús, sesiones…"""

    def __init__(self):
        self._callbacks = _Callbacks()

    # Callbacks de Notepad++
    def callback(self, function, notifications):
        self._callbacks.add(function, notifications, True)
        _update_codes()
        return True

    def clearCallbacks(self, *args):
        self._callbacks.clear(*args)
        _update_codes()

    # Documentos
    def new(self):
        self.menuCommand(MENUCOMMAND.FILE_NEW)

    def open(self, filename):
        return _npp.npp_ls(NPPM_DOOPEN, 0, _path(filename)) != 0

    def save(self):
        return _npp.npp(NPPM_SAVECURRENTFILE, 0, 0) != 0

    def saveAllFiles(self):
        return _npp.npp(NPPM_SAVEALLFILES, 0, 0) != 0

    def saveAs(self, filename):
        return _npp.npp_ls(NPPM_SAVECURRENTFILEAS, 0, _path(filename)) != 0

    def saveAsCopy(self, filename):
        return _npp.npp_ls(NPPM_SAVECURRENTFILEAS, 1, _path(filename)) != 0

    def saveFile(self, filename):
        return _npp.npp_ls(NPPM_SAVEFILE, 0, _path(filename)) != 0

    def close(self):
        self.menuCommand(MENUCOMMAND.FILE_CLOSE)

    def closeAll(self):
        self.menuCommand(MENUCOMMAND.FILE_CLOSEALL)

    def closeAllButCurrent(self):
        self.menuCommand(MENUCOMMAND.FILE_CLOSEALL_BUT_CURRENT)

    def reloadFile(self, filename):
        return _npp.npp_ls(NPPM_RELOADFILE, 0, _path(filename)) != 0

    def reloadBuffer(self, bufferID):
        return _npp.npp(NPPM_RELOADBUFFERID, bufferID, 0) != 0

    def reloadCurrentDocument(self):
        return self.reloadBuffer(self.getCurrentBufferID())

    def activateFile(self, filename):
        return _npp.npp_ls(NPPM_SWITCHTOFILE, 0, _path(filename)) != 0

    def activateIndex(self, view, index):
        return _npp.npp(NPPM_ACTIVATEDOC, view, index) != 0

    def activateBufferID(self, bufferID):
        pos = _npp.npp(NPPM_GETPOSFROMBUFFERID, bufferID, 0)
        if pos < 0:
            return False
        return self.activateIndex(pos >> 30, pos & 0x3FFFFFFF)

    def getCurrentBufferID(self):
        return _npp.npp(NPPM_GETCURRENTBUFFERID, 0, 0)

    def getCurrentView(self):
        return _npp.npp(NPPM_GETCURRENTVIEW, 0, 0)

    def getCurrentDocIndex(self, view):
        return _npp.npp(NPPM_GETCURRENTDOCINDEX, 0, view)

    def getCurrentFilename(self):
        return _npp.npp_str(NPPM_GETFULLCURRENTPATH) or ""

    def getBufferFilename(self, bufferID):
        return _npp.npp_str(NPPM_GETFULLPATHFROMBUFFERID, bufferID) or ""

    def getFiles(self):
        files = []
        for view in (0, 1):
            count = _npp.npp(NPPM_GETNBOPENFILES, 0, 1 if view == 0 else 2)
            for index in range(count):
                buffer_id = _npp.npp(NPPM_GETBUFFERIDFROMPOS, index, view)
                if buffer_id:
                    files.append((self.getBufferFilename(buffer_id), buffer_id, index, view))
        return files

    def isSingleView(self):
        return _npp.npp(NPPM_GETNBOPENFILES, 0, 2) == 0

    def makeCurrentBufferDirty(self):
        _npp.npp(NPPM_MAKECURRENTBUFFERDIRTY, 0, 0)

    # Lenguaje, codificación y formato
    def getCurrentLang(self):
        return _npp.npp_intout(NPPM_GETCURRENTLANGTYPE, 0)[1]

    def setCurrentLang(self, langType):
        _npp.npp(NPPM_SETCURRENTLANGTYPE, 0, int(langType))

    def getLangType(self, bufferID=None):
        if bufferID is None:
            return self.getCurrentLang()
        return _npp.npp(NPPM_GETBUFFERLANGTYPE, bufferID, 0)

    def setLangType(self, langType, bufferID=None):
        if bufferID is None:
            bufferID = self.getCurrentBufferID()
        return _npp.npp(NPPM_SETBUFFERLANGTYPE, bufferID, int(langType)) != 0

    def getLanguageName(self, langType):
        return _npp.npp_str(NPPM_GETLANGUAGENAME, int(langType)) or ""

    def getLanguageDesc(self, langType):
        return _npp.npp_str(NPPM_GETLANGUAGEDESC, int(langType)) or ""

    def getNbUserLang(self):
        return _npp.npp(NPPM_GETNBUSERLANG, 0, 0)

    def getEncoding(self, bufferID=None):
        if bufferID is None:
            bufferID = self.getCurrentBufferID()
        return _npp.npp(NPPM_GETBUFFERENCODING, bufferID, 0)

    def setEncoding(self, encoding, bufferID=None):
        if bufferID is None:
            bufferID = self.getCurrentBufferID()
        return _npp.npp(NPPM_SETBUFFERENCODING, bufferID, int(encoding)) != 0

    def getFormatType(self, bufferID=None):
        if bufferID is None:
            bufferID = self.getCurrentBufferID()
        return _npp.npp(NPPM_GETBUFFERFORMAT, bufferID, 0)

    def setFormatType(self, formatType, bufferID=None):
        if bufferID is None:
            bufferID = self.getCurrentBufferID()
        return _npp.npp(NPPM_SETBUFFERFORMAT, bufferID, int(formatType)) != 0

    # Menús
    def menuCommand(self, menuCommand):
        return _npp.npp(NPPM_MENUCOMMAND, 0, int(menuCommand)) != 0

    def runMenuCommand(self, menuName, menuOption, refreshCache=False):
        return _npp.run_menu_command(str(menuName), str(menuOption))

    def runPluginCommand(self, pluginName, menuOption, refreshCache=False):
        return _npp.run_plugin_command(str(pluginName), str(menuOption))

    def getMenuHandle(self, menu=0):
        return _npp.npp(NPPM_GETMENUHANDLE, menu, 0)

    def getPluginMenuHandle(self):
        return self.getMenuHandle(0)

    # Diálogos y estado
    def messageBox(self, message, title="Python Script for Notepad++", flags=0):
        return _npp.message_box(str(message), str(title), int(flags))

    def prompt(self, prompt, title, defaultText=""):
        return _npp.prompt(str(prompt), str(title), str(defaultText))

    def setStatusBar(self, statusBarSection, text):
        return _npp.npp_ls(NPPM_SETSTATUSBAR, int(statusBarSection), _bytes(str(text))) != 0

    def hideTabBar(self):
        _npp.npp(NPPM_HIDETABBAR, 0, 1)

    def showTabBar(self):
        _npp.npp(NPPM_HIDETABBAR, 0, 0)

    def isTabBarHidden(self):
        return _npp.npp(NPPM_ISTABBARHIDDEN, 0, 0) != 0

    def hideStatusBar(self, hide=True):
        return _npp.npp(NPPM_HIDESTATUSBAR, 0, 1 if hide else 0) != 0

    def isStatusBarHidden(self):
        return _npp.npp(NPPM_ISSTATUSBARHIDDEN, 0, 0) != 0

    def hideMenu(self, hide=True):
        return _npp.npp(NPPM_HIDEMENU, 0, 1 if hide else 0) != 0

    def isMenuHidden(self):
        return _npp.npp(NPPM_ISMENUHIDDEN, 0, 0) != 0

    def hideToolBar(self, hide=True):
        return False  # Edita no tiene barra de herramientas

    def isToolBarHidden(self):
        return True

    def isAutoIndention(self):
        return _npp.npp(NPPM_ISAUTOINDENTON, 0, 0) != 0

    def isDarkModeEnabled(self):
        return _npp.npp(NPPM_ISDARKMODEENABLED, 0, 0) != 0

    def isDocSwitcherShown(self):
        return False

    def showDocSwitcher(self, show=True):
        pass

    def docSwitcherDisableColumn(self, disable=True):
        pass

    def getEditorDefaultForegroundColor(self):
        c = _npp.npp(NPPM_GETEDITORDEFAULTFOREGROUNDCOLOR, 0, 0)
        return (c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF)

    def getEditorDefaultBackgroundColor(self):
        c = _npp.npp(NPPM_GETEDITORDEFAULTBACKGROUNDCOLOR, 0, 0)
        return (c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF)

    def flashWindow(self, count=1, milliseconds=0):
        _npp.flash_window(int(count), int(milliseconds))

    def disableAutoUpdate(self):
        pass

    def setSmoothFont(self, smooth=True):
        pass

    def setEditorBorderEdge(self, edge=True):
        pass

    def getLineNumberWidthMode(self):
        return LINENUMWIDTHMODE.DYNAMIC

    def setLineNumberWidthMode(self, mode):
        return False

    def launchFindInFilesDlg(self, dir2Search="", filter=""):
        return _npp.npp_ss(NPPM_LAUNCHFINDINFILESDLG, _bytes(dir2Search), _bytes(filter)) != 0

    # Información
    def getVersion(self):
        v = _npp.npp(NPPM_GETNPPVERSION, 0, 0)
        minor = v & 0xFFFF
        return (v >> 16, minor // 100, (minor % 100) // 10)

    def getPluginVersion(self):
        return "3.0.0 (Edita)"

    def getNppDir(self):
        return _npp.npp_str(NPPM_GETNPPDIRECTORY) or ""

    def getPluginConfigDir(self):
        return _npp.npp_str(NPPM_GETPLUGINSCONFIGDIR) or ""

    def getCommandLine(self):
        return _npp.npp_str(NPPM_GETCURRENTCMDLINE, 0) or ""

    def getPluginHomePath(self):
        return _npp.npp_str(NPPM_GETPLUGINHOMEPATH, 0) or ""

    def getSettingsOnCloudPath(self):
        return _npp.npp_str(NPPM_GETSETTINGSONCLOUDPATH, 0) or ""

    def outputDebugString(self, text):
        _sys.__stderr__.write(str(text))

    def docSwitcherDisableExtColumn(self, disable=True):
        pass

    def getBookMarkID(self):
        return _npp.npp(NPPM_GETBOOKMARKID, 0, 0)

    def getWindowsVersion(self):
        return WINVER.UNKNOWN

    def getAppdataPluginsAllowed(self):
        return True

    def allocateSupported(self):
        return True

    def _allocate(self, message, count):
        ok, start = _npp.npp_intout(message, int(count))
        return start if ok else None

    def allocateCmdID(self, numberRequested):
        return self._allocate(NPPM_ALLOCATECMDID, numberRequested)

    def allocateMarker(self, numberRequested):
        return self._allocate(NPPM_ALLOCATEMARKER, numberRequested)

    def allocateIndicator(self, numberRequested):
        return self._allocate(NPPM_ALLOCATEINDICATOR, numberRequested)

    # Sesiones (session.xml de N++)
    def saveCurrentSession(self, filename):
        return _npp.npp_ls(NPPM_SAVECURRENTSESSION, 0, _path(filename)) != 0

    def loadSession(self, filename):
        return _npp.npp_ls(NPPM_LOADSESSION, 0, _path(filename)) != 0

    def getSessionFiles(self, sessionFileName):
        try:
            root = _ET.parse(str(sessionFileName)).getroot()
        except (OSError, _ET.ParseError):
            return []
        return [f.get("filename") for f in root.iter("File") if f.get("filename")]

    def saveSession(self, filename, filesList):
        root = _ET.Element("NotepadPlus")
        session = _ET.SubElement(root, "Session", activeView="0")
        view = _ET.SubElement(session, "mainView", activeIndex="0")
        for f in filesList:
            _ET.SubElement(view, "File", filename=str(f))
        _ET.SubElement(session, "subView", activeIndex="0")
        _ET.ElementTree(root).write(str(filename), encoding="UTF-8", xml_declaration=True)

    # Editores adicionales
    def createScintilla(self):
        return Editor(_npp.create_scintilla())

    def destroyScintilla(self, editor):
        return _npp.destroy_scintilla(int(editor.hwnd))

    # Sin equivalente en Edita
    def getCurrentNativeLangEncoding(self):
        return 65001

    def getNativeLangFileName(self):
        return ""

    def getTabColorID(self, view=-1, tabIndex=-1):
        return -1

    def triggerTabbarContextMenu(self, view, index2Activate):
        pass

    def setUntitledName(self, newName, bufferID=None):
        return False

    def addModificationFlags(self, flags):
        if int(flags) & ~MODIFICATIONFLAGS.MODEVENTMASKALL:
            raise RuntimeError("Provided flags value is not a valid MODIFICATION enum value.")
        return _npp.npp(NPPM_ADDSCNMODIFIEDFLAGS, 0, int(flags)) != 0

    def getEnableThemeTextureFunc(self):
        return False

    def decodeSci(self, view):
        return 65001

    def encodeSci(self, view):
        return 65001

    def getExternalLexerAutoIndentMode(self, externalLexerName):
        return AUTOINDENTMODE.STANDARD

    def setExternalLexerAutoIndentMode(self, externalLexerName, indentMode):
        return False


# --- Console --------------------------------------------------------------------------------

class Console:
    """La consola de Python Script"""

    def write(self, text):
        _npp.console_write(str(text), False)

    def writeError(self, text):
        _npp.console_write(str(text), True)

    def clear(self):
        _npp.console_clear()

    def show(self):
        _npp.console_show(True)

    def hide(self):
        _npp.console_show(False)

    def flush(self):
        pass

    @property
    def editor(self):
        return Editor(_npp.console_editor())

    def run(self, commandLine, stdout=None, stderr=None):
        """Ejecuta un programa y escribe su salida en la consola (o en stdout/stderr)"""
        out = stdout if stdout is not None else self
        err = stderr if stderr is not None else _sys.stderr
        process = _subprocess.Popen(commandLine, shell=isinstance(commandLine, str), stdout=_subprocess.PIPE,
                                    stderr=_subprocess.PIPE, text=True, errors="replace")
        output, errors = process.communicate()
        if output:
            out.write(output)
        if errors:
            err.write(errors)
        return process.returncode


# --- Consola interactiva ----------------------------------------------------------------------

_interpreter = None


def _console_push(line):
    """Una línea escrita en la consola; True si la sentencia sigue en la siguiente"""
    global _interpreter
    if _interpreter is None:
        import __main__
        _interpreter = _code.InteractiveConsole(__main__.__dict__)
    return _interpreter.push(line)


def _console_submit(line):
    """La consola ejecuta en el hilo de los scripts y luego actualiza el indicador"""
    def job():
        more = False
        try:
            more = _console_push(line)
        finally:
            _npp.console_prompt(bool(more))
    _script_worker.busy = True
    _script_worker.submit(job)


notepad = Notepad()
editor = Editor(-1)
editor1 = Editor(0)
editor2 = Editor(1)
console = Console()

__all__ += ["notepad", "editor", "editor1", "editor2", "console", "Editor", "Notepad", "Console", "Cell", "FORMATTYPE",
            "BUFFERENCODING", "STATUSBARSECTION", "MESSAGEBOXFLAGS", "LINENUMWIDTHMODE", "AUTOINDENTMODE", "WINVER",
            "PLATFORM"]
__all__ = sorted(set(n for n in __all__ if not n.startswith("NPPM_")))
