// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 2: pestañas, abrir/guardar/cerrar, barra de estado, codificación y fin
// de línea desde la ventana principal.

#include "FakePrompter.h"

#include "doc/DocumentManager.h"
#include "syntax/SyntaxManager.h"
#include "ui/DocStatusBar.h"
#include "ui/DocTabBar.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QMenuBar>
#include <QMimeData>
#include <QTemporaryDir>
#include <QPixmap>
#include <QTest>

#include <SciLexer.h>

using namespace edita;
using edita::test::FakePrompter;
using namespace std::string_literals;

namespace {

QString fixture(const QString& relative)
{
    return QStringLiteral(EDITA_FIXTURES_DIR "/") + relative;
}

QByteArray readAll(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void writeFile(const QString& path, const QByteArray& data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

// Ventana con un FakePrompter accesible
struct Window
{
    MainWindow window;
    FakePrompter* prompter;

    Window()
    {
        auto fake = std::make_unique<FakePrompter>();
        prompter = fake.get();
        window.setPrompter(std::move(fake));
    }

    DocTabBar* tabs() const { return window.pane()->tabBar(); }
    EditorView* editor() const { return window.editor(); }
    const Buffer* current() { return window.documents().buffer(window.currentBuffer()); }
    void type(const char* text) const { editor()->sends(SCI_REPLACESEL, 0, text); }
    QAction* action(const char* name) const { return window.findChild<QAction*>(QString::fromLatin1(name)); }
};

} // namespace

class TestMainWindow : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir _tmp;

    // Copia un fixture a un directorio temporal para poder modificarlo
    QString copyFixture(const QString& relative, const QString& name)
    {
        const QString dst = _tmp.filePath(name);
        QFile::remove(dst);
        QFile::copy(fixture(relative), dst);
        QFile::setPermissions(dst, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        return dst;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        // Configuración aislada: no tocar ~/.config/edita del usuario
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    void startsWithOneUntitledDocument()
    {
        Window w;
        QCOMPARE(w.tabs()->count(), 1);
        QCOMPARE(w.current()->displayName(), QStringLiteral("new 1"));
        QCOMPARE(w.window.windowTitle(), QStringLiteral("new 1 - Edita"));
        // Archivo, Editar, Buscar, Ver, Codificación, Lenguaje, Configuración, Macro, Plugins, Ayuda
        QCOMPARE(w.window.menuBar()->actions().size(), 10);
        // El ícono de Edita (resources/), del tamaño de la barra de tareas al del menú
        const QList<QSize> sizes = w.window.windowIcon().availableSizes();
        QVERIFY(sizes.contains(QSize(16, 16)));
        QVERIFY(sizes.contains(QSize(512, 512)));
        QVERIFY(!QPixmap(QStringLiteral(":/edita/logo.png")).isNull());
    }

    // --- Criterios de la Fase 2 ------------------------------------------------

    void openingFilesCreatesTabs()
    {
        Window w;
        const BufferId a = w.window.openFile(fixture("eol/lf.txt"));
        const BufferId b = w.window.openFile(fixture("eol/crlf.txt"));
        const BufferId c = w.window.openFile(fixture("eol/cr.txt"));
        // El "new 1" vacío fue sustituido por el primer archivo
        QCOMPARE(w.tabs()->count(), 3);
        QCOMPARE(w.window.currentBuffer(), c);
        QCOMPARE(w.tabs()->tabText(0), QStringLiteral("lf.txt"));

        // Abrir uno ya abierto activa su pestaña sin duplicarla
        QCOMPARE(w.window.openFile(fixture("eol/lf.txt")), a);
        QCOMPARE(w.tabs()->count(), 3);
        QCOMPARE(w.window.currentBuffer(), a);
        QCOMPARE(w.tabs()->currentIndex(), 0);
        QVERIFY(b != a && b != c);
    }

    void untitledWithChangesIsNotReplaced()
    {
        Window w;
        w.type("algo");
        w.window.openFile(fixture("eol/lf.txt"));
        QCOMPARE(w.tabs()->count(), 2);
    }

    void editingMarksTabAndTitle()
    {
        Window w;
        const QString path = copyFixture("eol/lf.txt", "editar.txt");
        w.window.openFile(path);
        QCOMPARE(w.tabs()->stateAt(0), DocTabBar::TabState::Saved);

        w.type("nuevo texto\n");
        QVERIFY(w.current()->isDirty());
        QCOMPARE(w.tabs()->stateAt(0), DocTabBar::TabState::Dirty);
        QVERIFY(w.window.windowTitle().startsWith(QLatin1Char('*')));

        QVERIFY(w.window.saveBuffer(w.window.currentBuffer()));
        QVERIFY(!w.current()->isDirty());
        QCOMPARE(w.tabs()->stateAt(0), DocTabBar::TabState::Saved);
        QVERIFY(!w.window.windowTitle().startsWith(QLatin1Char('*')));
        QCOMPARE(readAll(path), QByteArray("nuevo texto\nuno\ndos\ntres\n"));

        // Deshacer hasta el punto de guardado vuelve a marcarlo como guardado
        w.type("x");
        QVERIFY(w.current()->isDirty());
        w.editor()->sci(SCI_UNDO);
        QVERIFY(!w.current()->isDirty());
    }

    void undoHistoryAndCaretArePerDocument()
    {
        Window w;
        const BufferId a = w.window.openFile(copyFixture("eol/lf.txt", "a.txt"));
        w.editor()->sci(SCI_DOCUMENTEND);
        w.type("A1");
        w.editor()->sci(SCI_GOTOPOS, 4); // dentro de "dos"
        const BufferId b = w.window.openFile(copyFixture("eol/crlf.txt", "b.txt"));
        w.type("B1");
        QCOMPARE(w.window.documents().text(b), "B1uno\r\ndos\r\ntres\r\n"s);

        // Volver a A: su cursor está donde se dejó y su deshacer es el suyo
        w.window.pane()->activateBuffer(a);
        QCOMPARE(w.editor()->sci(SCI_GETCURRENTPOS), sptr_t{4});
        w.editor()->sci(SCI_UNDO);
        QCOMPARE(w.window.documents().text(a), "uno\ndos\ntres\n"s);
        QVERIFY(!w.editor()->sci(SCI_CANUNDO));
        QCOMPARE(w.window.documents().text(b), "B1uno\r\ndos\r\ntres\r\n"s);

        // Y B conserva su propio historial
        w.window.pane()->activateBuffer(b);
        QVERIFY(w.editor()->sci(SCI_CANUNDO));
        w.editor()->sci(SCI_UNDO);
        QCOMPARE(w.window.documents().text(b), "uno\r\ndos\r\ntres\r\n"s);
    }

    void scrollPositionIsRestored()
    {
        Window w;
        w.window.resize(600, 300);
        w.window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w.window));
        QByteArray many;
        for (int i = 1; i <= 500; ++i)
            many += "línea " + QByteArray::number(i) + "\n";
        const QString path = _tmp.filePath("largo.txt");
        writeFile(path, many);
        const BufferId big = w.window.openFile(path);
        w.window.goToLine(300);
        const sptr_t top = w.editor()->sci(SCI_GETFIRSTVISIBLELINE);
        QVERIFY(top > 200);
        w.window.newDocument();
        w.window.pane()->activateBuffer(big);
        QCOMPARE(w.editor()->sci(SCI_GETFIRSTVISIBLELINE), top);
        QCOMPARE(w.editor()->sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(w.editor()->sci(SCI_GETCURRENTPOS))), sptr_t{299});
    }

    void lineNumberMarginGrowsWithLineCount()
    {
        Window w;
        const sptr_t narrow = w.editor()->sci(SCI_GETMARGINWIDTHN, 0);
        QByteArray many;
        for (int i = 0; i < 100000; ++i)
            many += "x\n";
        const QString path = _tmp.filePath("cien-mil.txt");
        writeFile(path, many);
        const BufferId big = w.window.openFile(path);
        // Ancho dinámico (por defecto en N++): según la última línea visible
        w.editor()->sci(SCI_DOCUMENTEND);
        w.editor()->updateLineNumberWidth();
        QVERIFY(w.editor()->sci(SCI_GETMARGINWIDTHN, 0) > narrow); // 6 dígitos
        w.window.newDocument();
        QCOMPARE(w.editor()->sci(SCI_GETMARGINWIDTHN, 0), narrow);
        QVERIFY(big != kInvalidBuffer);

        // Sin ancho dinámico: según el total de líneas, aunque se vea el principio
        const Preferences before = w.window.preferences();
        Preferences p = before;
        p.lineNumberDynamicWidth = false;
        w.window.setPreferences(p);
        w.window.pane()->activateBuffer(big);
        w.editor()->sci(SCI_DOCUMENTSTART);
        w.editor()->updateLineNumberWidth();
        QVERIFY(w.editor()->sci(SCI_GETMARGINWIDTHN, 0) > narrow);
        w.window.setPreferences(before);
    }

    void closingDirtyDocumentAsks()
    {
        Window w;
        const QString path = copyFixture("eol/lf.txt", "cerrar.txt");
        w.window.newDocument();
        const BufferId id = w.window.openFile(path);
        w.type("cambio ");

        // Cancelar: no se cierra
        w.prompter->saveChoices.enqueue(Prompter::SaveChoice::Cancel);
        QVERIFY(!w.window.closeBuffer(id));
        QVERIFY(w.window.pane()->contains(id));
        QCOMPARE(w.prompter->asked.last(), QStringLiteral("save:cerrar.txt"));

        // No guardar: se cierra y el archivo no cambia
        w.prompter->saveChoices.enqueue(Prompter::SaveChoice::Discard);
        QVERIFY(w.window.closeBuffer(id));
        QVERIFY(!w.window.pane()->contains(id));
        QVERIFY(w.window.documents().buffer(id) == nullptr);
        QCOMPARE(readAll(path), QByteArray("uno\ndos\ntres\n"));

        // Guardar: se guarda y se cierra
        const BufferId again = w.window.openFile(path);
        w.type("guardado ");
        w.prompter->saveChoices.enqueue(Prompter::SaveChoice::Save);
        QVERIFY(w.window.closeBuffer(again));
        QCOMPARE(readAll(path), QByteArray("guardado uno\ndos\ntres\n"));
    }

    void closingLastTabLeavesAnUntitledDocument()
    {
        Window w;
        const BufferId id = w.window.openFile(fixture("eol/lf.txt"));
        QVERIFY(w.window.closeBuffer(id));
        QCOMPARE(w.tabs()->count(), 1);
        QVERIFY(w.current()->isUntitled());
        // Cerrar ese "nuevo" vacío no hace nada (N++)
        const BufferId untitled = w.window.currentBuffer();
        QVERIFY(w.window.closeBuffer(untitled));
        QCOMPARE(w.window.currentBuffer(), untitled);
    }

    void closeAllAsksForEachDirtyDocumentAndCanSaveAll()
    {
        Window w;
        const QString p1 = copyFixture("eol/lf.txt", "todos1.txt");
        const QString p2 = copyFixture("eol/lf.txt", "todos2.txt");
        w.window.openFile(p1);
        w.type("1");
        w.window.openFile(p2);
        w.type("2");
        w.window.openFile(fixture("eol/cr.txt")); // sin cambios: no pregunta

        w.prompter->saveChoices.enqueue(Prompter::SaveChoice::SaveAll);
        QVERIFY(w.window.closeAll());
        QCOMPARE(w.prompter->asked, QStringList{QStringLiteral("save:todos1.txt:all")});
        QCOMPARE(readAll(p1), QByteArray("1uno\ndos\ntres\n"));
        QCOMPARE(readAll(p2), QByteArray("2uno\ndos\ntres\n"));
        QCOMPARE(w.tabs()->count(), 1);
        QVERIFY(w.current()->isUntitled());
        QCOMPARE(w.current()->displayName(), QStringLiteral("new 1"));
    }

    void closeLeftRightAndOthers()
    {
        Window w;
        const BufferId a = w.window.openFile(fixture("eol/lf.txt"));
        const BufferId b = w.window.openFile(fixture("eol/crlf.txt"));
        const BufferId c = w.window.openFile(fixture("eol/cr.txt"));
        const BufferId d = w.window.openFile(fixture("eol/none.txt"));
        QVERIFY(w.window.closeAllToLeft(b));
        QCOMPARE(w.window.pane()->buffers(), (std::vector<BufferId>{b, c, d}));
        QVERIFY(w.window.closeAllToRight(c));
        QCOMPARE(w.window.pane()->buffers(), (std::vector<BufferId>{b, c}));
        QVERIFY(w.window.closeAllBut(c));
        QCOMPARE(w.window.pane()->buffers(), (std::vector<BufferId>{c}));
        QVERIFY(a != b);
    }

    void exitingWithDirtyDocumentsAsks()
    {
        Window w;
        w.window.setSessionEnabled(false); // sin modo snapshot: se pregunta como en N++
        w.type("sin guardar");
        w.prompter->saveChoices.enqueue(Prompter::SaveChoice::Cancel);
        QVERIFY(!w.window.close());
        QCOMPARE(w.tabs()->count(), 1);

        w.prompter->saveChoices.enqueue(Prompter::SaveChoice::Discard);
        QVERIFY(w.window.close());
    }

    void saveUntitledAsksForName()
    {
        Window w;
        w.type("hola ñ\n");
        const QString path = _tmp.filePath("nombrado.txt");
        w.prompter->saveNames.enqueue(path);
        QVERIFY(w.window.saveBuffer(w.window.currentBuffer()));
        QVERIFY(w.prompter->asked.last().startsWith(QStringLiteral("saveAs:")));
        QVERIFY(w.prompter->asked.last().endsWith(QStringLiteral("new 1")));
        QCOMPARE(w.current()->filePath(), path);
        QCOMPARE(w.tabs()->tabText(0), QStringLiteral("nombrado.txt"));
        QCOMPARE(readAll(path), QByteArray("hola ñ\n"));

        // Cancelar el diálogo no guarda
        w.window.newDocument();
        w.type("x");
        QVERIFY(!w.window.saveBuffer(w.window.currentBuffer()));
    }

    void saveAsOverAnotherOpenDocumentIsRefused()
    {
        Window w;
        const QString other = copyFixture("eol/lf.txt", "ocupado.txt");
        w.window.openFile(other);
        w.window.newDocument();
        w.type("x");
        w.prompter->saveNames.enqueue(other);
        QVERIFY(!w.window.saveBufferAs(w.window.currentBuffer()));
        QCOMPARE(w.prompter->warnings.size(), 1);
        QCOMPARE(readAll(other), QByteArray("uno\ndos\ntres\n"));
    }

    void statusBarShowsEncodingAndEol_data()
    {
        QTest::addColumn<QString>("file");
        QTest::addColumn<QString>("encoding");
        QTest::addColumn<QString>("eol");
        QTest::newRow("utf-8") << "encoding/utf8.txt" << "UTF-8" << "Unix (LF)";
        QTest::newRow("utf-8 bom") << "encoding/utf8-bom.txt" << "UTF-8-BOM" << "Unix (LF)";
        QTest::newRow("utf-16 le") << "encoding/utf16le-bom.txt" << "UTF-16 LE BOM" << "Unix (LF)";
        QTest::newRow("1252") << "encoding/windows-1252.txt" << "Windows-1252" << "Unix (LF)";
        QTest::newRow("shift-jis") << "encoding/shift_jis.txt" << "Shift-JIS" << "Unix (LF)";
        QTest::newRow("crlf") << "eol/crlf.txt" << "UTF-8" << "Windows (CR LF)";
        QTest::newRow("cr") << "eol/cr.txt" << "UTF-8" << "Macintosh (CR)";
    }

    void statusBarShowsEncodingAndEol()
    {
        QFETCH(QString, file);
        QFETCH(QString, encoding);
        QFETCH(QString, eol);
        Window w;
        w.window.openFile(fixture(file));
        QCOMPARE(w.window.docStatusBar()->encodingText(), encoding);
        QCOMPARE(w.window.docStatusBar()->eolText(), eol);
    }

    void statusBarShowsCaretAndSelection()
    {
        // Scintilla avisa de cursor y selección (SCN_UPDATEUI) al repintar
        Window w;
        w.window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w.window));
        w.type("añadir\nsegunda");
        DocStatusBar* status = w.window.docStatusBar();
        QTRY_COMPARE(status->positionText(), QStringLiteral("Ln: 2    Col: 8    Pos: 16"));
        QVERIFY(status->lengthText().contains(QStringLiteral("lines: 2")));
        w.editor()->sci(SCI_SETSEL, 0, 8); // "añadir\n": 7 caracteres (8 bytes), 2 líneas
        QTRY_COMPARE(status->positionText(), QStringLiteral("Ln: 2    Col: 1    Sel: 7 | 2"));
        QCOMPARE(status->insertModeText(), QStringLiteral("INS"));
        w.editor()->sci(SCI_SETOVERTYPE, 1);
        w.editor()->sci(SCI_GOTOPOS, 1);
        QTRY_COMPARE(status->insertModeText(), QStringLiteral("OVR"));
    }

    // --- Codificación y fin de línea ----------------------------------------------

    void reinterpretWithCharset()
    {
        Window w;
        w.window.openFile(fixture("encoding/iso-8859-15.txt"));
        QCOMPARE(w.window.docStatusBar()->encodingText(), QStringLiteral("Windows-1252"));
        w.action("actionCharset28605")->trigger();
        QCOMPARE(w.window.docStatusBar()->encodingText(), QStringLiteral("ISO 8859-15"));
        QVERIFY(w.action("actionCharset28605")->isChecked());
        QCOMPARE(QByteArray::fromStdString(w.window.documents().text(w.window.currentBuffer())),
                 readAll(fixture("encoding/reference/spanish-euro.txt")));
    }

    void reinterpretAsksToSaveFirst()
    {
        Window w;
        const QString path = copyFixture("encoding/windows-1252.txt", "reinterpretar.txt");
        w.window.openFile(path);
        w.type("x");
        w.prompter->confirmations.enqueue(false); // no guardar → no se hace nada
        w.window.reinterpretAs(28591);
        QCOMPARE(w.window.docStatusBar()->encodingText(), QStringLiteral("Windows-1252"));
        QVERIFY(w.current()->isDirty());
        QVERIFY(w.action("actionCharset1252")->isChecked());
    }

    void convertToUtf8BomAndSave()
    {
        Window w;
        const QString path = copyFixture("encoding/windows-1252.txt", "a-bom.txt");
        w.window.openFile(path);
        w.action("actionConvert1")->trigger(); // UniMode::Utf8Bom
        QCOMPARE(w.window.docStatusBar()->encodingText(), QStringLiteral("UTF-8-BOM"));
        QVERIFY(w.current()->isDirty());
        QVERIFY(w.window.saveBuffer(w.window.currentBuffer()));
        QCOMPARE(readAll(path), "\xEF\xBB\xBF" + readAll(fixture("encoding/reference/spanish-euro.txt")));
    }

    void encodeAsUtf8FromCharsetReinterprets()
    {
        Window w;
        // Un archivo UTF-8 que se abrió forzando Latin-1: "Codificar en UTF-8" lo lee bien
        const BufferId id = w.window.openFile(fixture("encoding/utf8.txt"));
        w.window.reinterpretAs(28591);
        QVERIFY(w.current()->encoding() == TextEncoding::fromCodepage(28591));
        w.window.encodeAs(UniMode::Utf8);
        QVERIFY(w.current()->encoding() == TextEncoding::utf8());
        QCOMPARE(QByteArray::fromStdString(w.window.documents().text(id)), readAll(fixture("encoding/utf8.txt")));
    }

    void unmappableCharactersAreDetectedBeforeSaving()
    {
        Window w;
        const QString path = copyFixture("encoding/windows-1252.txt", "no-cabe.txt");
        w.window.openFile(path);
        w.editor()->sci(SCI_DOCUMENTEND);
        w.type("中文");

        // Cancelar: no se toca el archivo
        w.prompter->unmappableChoices.enqueue(Prompter::UnmappableChoice::Cancel);
        QVERIFY(!w.window.saveBuffer(w.window.currentBuffer()));
        QCOMPARE(w.prompter->lastUnmappableCount, std::size_t{2});
        QCOMPARE(readAll(path), readAll(fixture("encoding/windows-1252.txt")));

        // Convertir a UTF-8 y guardar: no se pierde nada
        w.prompter->unmappableChoices.enqueue(Prompter::UnmappableChoice::ConvertToUtf8);
        QVERIFY(w.window.saveBuffer(w.window.currentBuffer()));
        QCOMPARE(w.window.docStatusBar()->encodingText(), QStringLiteral("UTF-8"));
        QCOMPARE(readAll(path), readAll(fixture("encoding/reference/spanish-euro.txt")) + "中文");
    }

    void convertEolFromMenu()
    {
        Window w;
        const QString path = copyFixture("eol/crlf.txt", "eol.txt");
        w.window.openFile(path);
        QVERIFY(w.action("actionEol0")->isChecked()); // Windows
        w.action("actionEol2")->trigger();            // Unix
        QCOMPARE(w.window.docStatusBar()->eolText(), QStringLiteral("Unix (LF)"));
        QVERIFY(w.current()->isDirty());
        QCOMPARE(w.editor()->sci(SCI_GETEOLMODE), sptr_t{SC_EOL_LF});
        QVERIFY(w.window.saveBuffer(w.window.currentBuffer()));
        QCOMPARE(readAll(path), QByteArray("uno\ndos\ntres\n"));
    }

    void readOnlyDocumentCannotBeEdited()
    {
        Window w;
        w.window.openFile(fixture("eol/lf.txt"));
        w.action("actionReadOnly")->trigger();
        QVERIFY(w.current()->isUserReadOnly());
        QCOMPARE(w.tabs()->stateAt(0), DocTabBar::TabState::ReadOnly);
        w.type("no entra");
        QCOMPARE(w.window.documents().text(w.window.currentBuffer()), "uno\ndos\ntres\n"s);
    }

    // --- Lenguaje y estilos (Fase 3) -----------------------------------------------

    void openingDetectsLanguageAndColours()
    {
        Window w;
        w.window.openFile(QStringLiteral(EDITA_SOURCE_DIR "/src/ui/MainWindow.cpp"));
        QCOMPARE(w.current()->language(), LangType::Cpp);
        QCOMPARE(w.window.docStatusBar()->languageText(), QStringLiteral("C++ source file"));
        QVERIFY(w.action("actionLang3")->isChecked()); // LangType::Cpp
        // El lexer de C++ está activo y colorea
        QCOMPARE(w.editor()->sci(SCI_GETLEXER), sptr_t{SCLEX_CPP});
        w.editor()->sci(SCI_COLOURISE, 0, -1);
        QCOMPARE(w.editor()->sci(SCI_GETSTYLEAT, 0), sptr_t{SCE_C_COMMENTLINE}); // "// This file is part…"
    }

    void languageFromMenuStaysAfterSaveAs()
    {
        Window w;
        w.type("print('hola')\n");
        QCOMPARE(w.current()->language(), LangType::Text);
        w.action("actionLang22")->trigger(); // Python
        QCOMPARE(w.current()->language(), LangType::Python);
        QCOMPARE(w.window.docStatusBar()->languageText(), QStringLiteral("Python file"));
        QCOMPARE(w.editor()->sci(SCI_GETLEXER), sptr_t{SCLEX_PYTHON});

        // Elegido a mano: guardar como .txt no lo cambia (N++: _hasLangBeenSetFromMenu)
        w.prompter->saveNames.enqueue(_tmp.filePath("menu.txt"));
        QVERIFY(w.window.saveBuffer(w.window.currentBuffer()));
        QCOMPARE(w.current()->language(), LangType::Python);
    }

    void saveAsChangesDetectedLanguage()
    {
        Window w;
        w.type("echo hola\n");
        w.prompter->saveNames.enqueue(_tmp.filePath("script.sh"));
        QVERIFY(w.window.saveBuffer(w.window.currentBuffer()));
        QCOMPARE(w.current()->language(), LangType::Bash);
        QCOMPARE(w.editor()->sci(SCI_GETLEXER), sptr_t{SCLEX_BASH});
        // El diálogo ofrece filtros por lenguaje y preselecciona el del documento
        QVERIFY(w.prompter->lastFilters.startsWith(QStringLiteral("All files (*)")));
        QVERIFY(w.prompter->lastFilters.contains(QStringLiteral("Python file (*.py")));
    }

    void contentDetectionForFilesWithoutExtension()
    {
        Window w;
        const QString path = _tmp.filePath("sin-extension");
        writeFile(path, "#!/usr/bin/env python3\nprint(1)\n");
        w.window.openFile(path);
        QCOMPARE(w.current()->language(), LangType::Python);
    }

    void eachTabKeepsItsLexer()
    {
        Window w;
        const BufferId cpp = w.window.openFile(QStringLiteral(EDITA_SOURCE_DIR "/src/main.cpp"));
        const BufferId xml = w.window.openFile(QStringLiteral(EDITA_SOURCE_DIR "/data/langs.model.xml"));
        QCOMPARE(w.editor()->sci(SCI_GETLEXER), sptr_t{SCLEX_XML});
        w.window.pane()->activateBuffer(cpp);
        QCOMPARE(w.editor()->sci(SCI_GETLEXER), sptr_t{SCLEX_CPP});
        w.window.pane()->activateBuffer(xml);
        QCOMPARE(w.editor()->sci(SCI_GETLEXER), sptr_t{SCLEX_XML});
    }

    void themeChangeRestylesAndIsRemembered()
    {
        const auto themes = [] {
            Window probe;
            return probe.window.syntax().themes();
        }();
        const auto deepBlack = std::find_if(themes.begin(), themes.end(),
                                            [](const SyntaxManager::Theme& t) { return t.name == "Deep Black"; });
        QVERIFY(deepBlack != themes.end());
        {
            Window w;
            const sptr_t before = w.editor()->sci(SCI_STYLEGETBACK, STYLE_DEFAULT);
            QVERIFY(w.window.setTheme(deepBlack->path));
            QVERIFY(w.editor()->sci(SCI_STYLEGETBACK, STYLE_DEFAULT) != before);
        }
        {
            Window again;
            QVERIFY(again.window.syntax().activeThemePath().endsWith(QStringLiteral("Deep Black.xml")));
            QVERIFY(again.window.setTheme(QString())); // dejarlo como estaba para el resto de tests
        }
    }

    void savingStylersXmlReloadsStyles()
    {
        Window w;
        const QString stylers = w.window.syntax().stylersPath();
        w.window.openFile(stylers);
        // Cambiar el color de "INSTRUCTION WORD" de C++ desde el propio editor
        std::string text = w.window.documents().text(w.window.currentBuffer());
        const std::string from = "name=\"INSTRUCTION WORD\" styleID=\"5\" fgColor=\"0000FF\"";
        const std::size_t pos = text.find(from, text.find("<LexerType name=\"cpp\""));
        QVERIFY(pos != std::string::npos);
        w.editor()->sci(SCI_SETTARGETRANGE, pos, static_cast<sptr_t>(pos + from.size()));
        const std::string to = "name=\"INSTRUCTION WORD\" styleID=\"5\" fgColor=\"00AA00\"";
        w.editor()->sci(SCI_REPLACETARGET, to.size(), reinterpret_cast<sptr_t>(to.c_str()));
        QVERIFY(w.window.saveBuffer(w.window.currentBuffer()));

        w.window.openFile(QStringLiteral(EDITA_SOURCE_DIR "/src/main.cpp"));
        QCOMPARE(w.editor()->sci(SCI_STYLEGETFORE, SCE_C_WORD), sptr_t{0x00AA00}); // 00AA00 es simétrico en BGR

        // Restaurar el original
        QFile::remove(stylers);
        w.window.reloadSyntax();
    }

    void braceMatching()
    {
        Window w;
        w.window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w.window));
        w.type("f(a[1]);");
        w.editor()->sci(SCI_GOTOPOS, 1); // justo después de "f", delante de "("
        w.editor()->matchBraces();
        // Scintilla no expone el resaltado: se comprueba que la pareja existe
        QCOMPARE(w.editor()->sci(SCI_BRACEMATCH, 1, 0), sptr_t{6});
    }

    // --- Abrir: línea de comandos, directorios, arrastrar -------------------------

    void commandLineOffersToCreateMissingFile()
    {
        Window w;
        const QString path = _tmp.filePath("creado-desde-cli.txt");
        w.prompter->confirmations.enqueue(true);
        const BufferId id = w.window.openFile(path, /*askToCreate*/ true);
        QVERIFY(id != kInvalidBuffer);
        QVERIFY(QFileInfo::exists(path));

        // Sin askToCreate (diálogo, arrastrar) solo avisa
        QCOMPARE(w.window.openFile(_tmp.filePath("no-existe.txt")), kInvalidBuffer);
        QCOMPARE(w.prompter->warnings.size(), 1);
    }

    void openingADirectoryOpensItsFiles()
    {
        Window w;
        w.window.openFile(fixture("eol"));
        QCOMPARE(w.tabs()->count(), static_cast<int>(QDir(fixture("eol")).entryList(QDir::Files).size()));
    }

    void goToLineAndColumn()
    {
        Window w;
        w.window.openFile(fixture("eol/lf.txt"));
        w.window.goToLine(2, 3);
        QCOMPARE(w.editor()->sci(SCI_GETCURRENTPOS), sptr_t{6}); // "uno\n" + "do"
    }

    void droppingFilesOpensThem()
    {
        Window w;
        auto* mime = new QMimeData;
        mime->setUrls({QUrl::fromLocalFile(fixture("eol/lf.txt")), QUrl::fromLocalFile(fixture("eol/cr.txt"))});
        QDragEnterEvent enter(QPointF(10, 10), Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&w.window, &enter);
        QVERIFY(enter.isAccepted());
        QDropEvent drop(QPointF(10, 10), Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&w.window, &drop);
        QCOMPARE(w.tabs()->count(), 2);
        delete mime;
    }

    void droppingFilesOnTheEditorOpensThem()
    {
        // Sobre el editor, ScintillaEditBase emite uriDropped por cada URL
        Window w;
        emit w.editor()->uriDropped(QUrl::fromLocalFile(fixture("eol/lf.txt")).toString());
        QCOMPARE(w.current()->displayName(), QStringLiteral("lf.txt"));
    }

    void typingGoesToEditor()
    {
        Window w;
        w.window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w.window));
        w.editor()->setFocus();
        QTest::keyClicks(w.editor(), QStringLiteral("hola mundo"));
        QCOMPARE(w.editor()->text(), "hola mundo"s);
        QTest::keyClick(w.editor(), Qt::Key_Z, Qt::ControlModifier);
        QVERIFY(w.editor()->text() != "hola mundo");
    }
};

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
