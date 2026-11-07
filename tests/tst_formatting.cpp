// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 72: formatear con herramientas externas (core/Formatters), aplicar solo las líneas que
// cambian (replaceChangedLines), Editar › Formatear documento y formatear al guardar

#include "FakePrompter.h"
#include "core/Formatters.h"
#include "core/Preferences.h"
#include "doc/Buffer.h"
#include "ui/EditCommands.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;
using edita::test::FakePrompter;

class TestFormatting : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;
    FakePrompter* _prompter = nullptr;

    // Un formateador de mentira: un script de shell
    QString script(const QString& name, const QByteArray& body)
    {
        const QString path = _tmp.filePath(QStringLiteral("bin/") + name);
        QDir().mkpath(_tmp.filePath(QStringLiteral("bin")));
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            return {};
        f.write("#!/bin/sh\n" + body + "\n");
        f.close();
        f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        return path;
    }

    QString file(const QString& name, const QByteArray& text)
    {
        const QString path = _tmp.filePath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            return {};
        f.write(text);
        return path;
    }

    static ResolvedFormatter formatter(const QString& program, QStringList arguments = {}, QStringList environment = {})
    {
        return ResolvedFormatter{QStringLiteral("test"), program, arguments, environment, program, false};
    }

    std::unique_ptr<MainWindow> window(const QString& formatters, bool onSave = false)
    {
        auto w = std::make_unique<MainWindow>();
        auto prompter = std::make_unique<FakePrompter>();
        _prompter = prompter.get();
        w->setPrompter(std::move(prompter));
        Preferences p = w->preferences();
        p.formatters = formatters;
        p.formatOnSave = onSave;
        w->setPreferences(p);
        return w;
    }

    static QString text(MainWindow& w) { return QString::fromStdString(w.editor()->text()); }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    // La herramienta de cada lenguaje: la primera instalada, otra orden o «off»
    void resolve()
    {
        const FormatterSpec* python = formatterForLanguage(QStringLiteral("python"));
        QVERIFY(python);
        QCOMPARE(python->id, QStringLiteral("python"));
        QCOMPARE(formatterForLanguage(QStringLiteral("cpp"))->id, QStringLiteral("clang-format"));
        QCOMPARE(formatterForLanguage(QStringLiteral("jsonc"))->id, QStringLiteral("json"));
        QVERIFY(!formatterForLanguage(QStringLiteral("no-existe")));
        const QString tool = script(QStringLiteral("mi-formato"), "cat");
        auto resolved = resolveFormatter(*python, {{QStringLiteral("python"), tool + QStringLiteral(" --rapido")}});
        QVERIFY(resolved);
        QCOMPARE(resolved->program, tool);
        QCOMPARE(resolved->arguments, QStringList{QStringLiteral("--rapido")});
        QVERIFY(!resolved->supportsLines);
        QVERIFY(!resolveFormatter(*python, {{QStringLiteral("python"), QStringLiteral("off")}}));
        QVERIFY(!resolveFormatter(*python, {{QStringLiteral("python"), QStringLiteral("no-existe-este-programa")}}));
        // clang-format sabe formatear solo unas líneas
        const QString clang = script(QStringLiteral("clang-format-14"), "cat");
        QVERIFY(resolveFormatter(*python, {{QStringLiteral("python"), clang}})->supportsLines);
    }

    // La entrada y la salida, las variables, la carpeta, los errores y el tiempo
    void run()
    {
        FormatRequest request;
        request.filePath = _tmp.filePath(QStringLiteral("src/a.py"));
        QDir().mkpath(_tmp.filePath(QStringLiteral("src")));
        request.tabSize = 2;
        const QString upper = script(QStringLiteral("upper"), "tr a-z A-Z");
        FormatResult r = runFormatter(formatter(upper), "hola\nmundo\n", request);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.output, QByteArray("HOLA\nMUNDO\n"));
        // $(FILE_PATH), $(TAB_SIZE), $(INDENT) en una variable, y la carpeta del archivo
        const QString echo = script(QStringLiteral("eco"), "cat >/dev/null; printf '%s|%s|[%s]|%s' \"$1\" \"$2\" \"$INDENTA\" \"$(pwd)\"");
        r = runFormatter(formatter(echo, {QStringLiteral("--file=$(FILE_PATH)"), QStringLiteral("$(TAB_SIZE)")},
                                   {QStringLiteral("INDENTA=$(INDENT)")}),
                         "x", request);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(QString::fromUtf8(r.output),
                 QStringLiteral("--file=%1|2|[  ]|%2").arg(request.filePath, QFileInfo(request.filePath).absolutePath()));
        request.useTabs = true;
        r = runFormatter(formatter(echo, {}, {QStringLiteral("INDENTA=$(INDENT)")}), "x", request);
        QVERIFY(r.output.contains("[\t]"));
        // Con supportsLines, --lines
        ResolvedFormatter lines = formatter(script(QStringLiteral("args"), "cat >/dev/null; printf '%s' \"$*\""));
        lines.supportsLines = true;
        request.firstLine = 2;
        request.lastLine = 4;
        QCOMPARE(runFormatter(lines, "x", request).output, QByteArray("--lines=2:4"));
        request.firstLine = request.lastLine = 0;
        // Falla: el mensaje de la herramienta
        r = runFormatter(formatter(script(QStringLiteral("falla"), "cat >/dev/null; echo 'sintaxis en la línea 3' >&2; exit 2")),
                         "x", request);
        QVERIFY(!r.ok);
        QCOMPARE(r.error, QStringLiteral("sintaxis en la línea 3"));
        // Sin salida para un documento que no está vacío: no vale (no se borra todo)
        r = runFormatter(formatter(script(QStringLiteral("nada"), "cat >/dev/null")), "algo", request);
        QVERIFY(!r.ok);
        // Se tarda demasiado
        request.timeoutMs = 300;
        r = runFormatter(formatter(script(QStringLiteral("lento"), "sleep 5")), "x", request);
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains(QStringLiteral("timeout")));
        // No existe
        r = runFormatter(formatter(_tmp.filePath(QStringLiteral("no-existe"))), "x", request);
        QVERIFY(!r.ok);
    }

    // Solo cambian las líneas distintas: los marcadores de las demás se quedan; un paso de deshacer
    void replaceOnlyChangedLines()
    {
        EditorView view;
        view.sends(SCI_SETTEXT, 0, "uno\ndos\ntres\ncuatro\n");
        toggleBookmark(view, 0);
        toggleBookmark(view, 3);
        QCOMPARE(replaceChangedLines(view, "uno\nDOS\ntres\ncuatro\n"), 1);
        QCOMPARE(QString::fromStdString(view.text()), QStringLiteral("uno\nDOS\ntres\ncuatro\n"));
        QCOMPARE(bookmarkedLines(view), (std::vector<sptr_t>{0, 3}));
        QCOMPARE(replaceChangedLines(view, "uno\nDOS\ntres\ncuatro\n"), 0); // igual: nada
        QVERIFY(replaceChangedLines(view, "cero\nuno\nDOS\ncuatro") > 0); // añade, quita y el último salto
        QCOMPARE(QString::fromStdString(view.text()), QStringLiteral("cero\nuno\nDOS\ncuatro"));
        QCOMPARE(bookmarkedLines(view).front(), 1); // el de «uno», que no cambió, se movió con ella
        view.sci(SCI_UNDO);
        QCOMPARE(QString::fromStdString(view.text()), QStringLiteral("uno\nDOS\ntres\ncuatro\n"));
        // Fines de línea de Windows
        view.sends(SCI_SETTEXT, 0, "a\r\nb\r\n");
        QCOMPARE(replaceChangedLines(view, "a\r\nB\r\n"), 1);
        QCOMPARE(QString::fromStdString(view.text()), QStringLiteral("a\r\nB\r\n"));
    }

    // Editar › Formatear documento: la herramienta del lenguaje; el cursor se queda
    void formatDocument()
    {
        const QString upper = script(QStringLiteral("upper2"), "tr a-z A-Z");
        const QString path = file(QStringLiteral("doc.py"), "def f():\n    return 1\n");
        auto w = window(QStringLiteral("python=") + upper);
        QVERIFY(w->openFile(path) != kInvalidBuffer);
        w->editor()->sci(SCI_GOTOPOS, 13); // línea 1, columna 4
        w->findChild<QAction*>(QStringLiteral("actionFormatDocument"))->trigger();
        QCOMPARE(text(*w), QStringLiteral("DEF F():\n    RETURN 1\n"));
        QCOMPARE(w->editor()->sci(SCI_GETCURRENTPOS), 13);
        QVERIFY(w->statusBar()->currentMessage().contains(QStringLiteral("upper2")));
        w->editor()->sci(SCI_UNDO);
        QCOMPARE(text(*w), QStringLiteral("def f():\n    return 1\n"));
        // Falla: aviso y nada cambia
        Preferences p = w->preferences();
        p.formatters = QStringLiteral("python=") + script(QStringLiteral("roto"), "echo 'no entiendo' >&2; exit 1");
        w->setPreferences(p);
        w->formatDocument();
        QCOMPARE(text(*w), QStringLiteral("def f():\n    return 1\n"));
        QCOMPARE(_prompter->warnings.size(), 1);
        QVERIFY(_prompter->warnings.front().contains(QStringLiteral("no entiendo")));
        // Sin herramienta (y sin servidor de lenguaje): lo dice
        p.formatters = QStringLiteral("python=off");
        w->setPreferences(p);
        w->formatDocument();
        QCOMPARE(text(*w), QStringLiteral("def f():\n    return 1\n"));
        QVERIFY(w->statusBar()->currentMessage().contains(QStringLiteral("Formatting")));
    }

    // Formatear al guardar (con la herramienta) y no en «Guardar una copia como»
    void formatOnSave()
    {
        const QString upper = script(QStringLiteral("upper3"), "tr a-z A-Z");
        const QString path = file(QStringLiteral("guardar.py"), "x = 1\n");
        auto w = window(QStringLiteral("python=") + upper, true);
        const BufferId id = w->openFile(path);
        w->editor()->sends(SCI_APPENDTEXT, 6, "y = 2\n");
        QVERIFY(w->saveBuffer(id));
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArray("X = 1\nY = 2\n"));
        QCOMPARE(text(*w), QStringLiteral("X = 1\nY = 2\n"));
        // Si la herramienta falla, se guarda igual
        Preferences p = w->preferences();
        p.formatters = QStringLiteral("python=") + script(QStringLiteral("roto2"), "exit 3");
        w->setPreferences(p);
        w->editor()->sends(SCI_APPENDTEXT, 6, "z = 3\n");
        QVERIFY(w->saveBuffer(id));
        f.close();
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArray("X = 1\nY = 2\nz = 3\n"));
    }
};

QTEST_MAIN(TestFormatting)
#include "tst_formatting.moc"
