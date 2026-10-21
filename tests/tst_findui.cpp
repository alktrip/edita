// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 4 en la interfaz: diálogo de búsqueda, reemplazos en el editor, panel de
// resultados, documentos abiertos y buscar/reemplazar en archivos.

#include "FakePrompter.h"

#include "core/Locale.h"
#include "core/TextFile.h"
#include "doc/DocumentManager.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/FindReplaceDialog.h"
#include "ui/MainWindow.h"
#include "ui/SearchResultsPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QPushButton>
#include <QRadioButton>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;
using edita::test::FakePrompter;
using namespace std::string_literals;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QByteArray readAll(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

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
    EditorView* editor() const { return window.editor(); }
    void setText(const char* text) const { editor()->sends(SCI_SETTEXT, 0, text); }
    std::string text() { return window.documents().text(window.currentBuffer()); }
    std::string selection()
    {
        const sptr_t s = editor()->sci(SCI_GETSELECTIONSTART);
        const sptr_t e = editor()->sci(SCI_GETSELECTIONEND);
        return text().substr(static_cast<std::size_t>(s), static_cast<std::size_t>(e - s));
    }
    std::string results() const
    {
        ScintillaEditBase* v = window.resultsPanel()->view();
        const auto len = static_cast<std::size_t>(v->send(SCI_GETLENGTH));
        std::string out(len + 1, '\0');
        v->send(SCI_GETTEXT, len + 1, reinterpret_cast<sptr_t>(out.data()));
        out.resize(len);
        return out;
    }
};

SearchOptions plain(const char* pattern, const char* replacement = "")
{
    SearchOptions o;
    o.pattern = pattern;
    o.replacement = replacement;
    return o;
}

} // namespace

class TestFindUi : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir _tmp;

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        initGlobalLocale();
    }

    void findNextSelectsAndWraps()
    {
        Window w;
        w.setText("uno dos uno");
        FindResult r = w.window.findNextInCurrent(plain("uno"));
        QVERIFY(r.found());
        QCOMPARE(w.selection(), "uno"s);
        QCOMPARE(w.editor()->sci(SCI_GETSELECTIONSTART), sptr_t{0});
        w.window.findNextInCurrent(plain("uno"));
        QCOMPARE(w.editor()->sci(SCI_GETSELECTIONSTART), sptr_t{8});
        r = w.window.findNextInCurrent(plain("uno"));
        QCOMPARE(r.status, FindStatus::EndReached);
        QVERIFY(w.window.lastSearchStatus().contains(QStringLiteral("end of the document")));
        w.window.findNextInCurrent(plain("zzz"));
        QVERIFY(w.window.lastSearchStatus().contains(QStringLiteral("Can't find")));
    }

    void f3AndShiftF3UseLastSearch()
    {
        Window w;
        w.setText("a x a x a");
        w.window.findNextInCurrent(plain("x"));
        QCOMPARE(w.editor()->sci(SCI_GETSELECTIONSTART), sptr_t{2});
        w.window.findChild<QAction*>("actionFindNext")->trigger();
        QCOMPARE(w.editor()->sci(SCI_GETSELECTIONSTART), sptr_t{6});
        w.window.findChild<QAction*>("actionFindPrevious")->trigger();
        QCOMPARE(w.editor()->sci(SCI_GETSELECTIONSTART), sptr_t{2});
    }

    void replaceAndReplaceAllWithSingleUndo()
    {
        Window w;
        w.setText("gato perro gato");
        w.editor()->sci(SCI_EMPTYUNDOBUFFER);
        QVERIFY(!w.window.replaceInCurrent(plain("gato", "felino"))); // primero selecciona
        QVERIFY(w.window.replaceInCurrent(plain("gato", "felino")));  // ahora reemplaza
        QCOMPARE(w.text(), "felino perro gato"s);
        QCOMPARE(w.selection(), "gato"s); // queda seleccionada la siguiente
        QVERIFY(w.window.lastSearchStatus().contains(QStringLiteral("next occurrence")));

        w.setText("a1 b2 c3");
        w.editor()->sci(SCI_EMPTYUNDOBUFFER);
        SearchOptions re = plain("(\\w)(\\d)", "$2$1");
        re.mode = SearchMode::Regex;
        QCOMPARE(w.window.replaceAllInCurrent(re), 3);
        QCOMPARE(w.text(), "1a 2b 3c"s);
        QVERIFY(w.window.lastSearchStatus().contains(QStringLiteral("3 occurrences")));
        w.editor()->sci(SCI_UNDO); // un solo paso deshace todo
        QCOMPARE(w.text(), "a1 b2 c3"s);
    }

    void replaceAllInSelectionAndCount()
    {
        Window w;
        w.setText("x x x x");
        w.editor()->sci(SCI_SETSEL, 2, 5); // "x x"
        SearchOptions o = plain("x", "y");
        o.inSelection = true;
        QCOMPARE(w.window.countInCurrent(o), 2);
        QCOMPARE(w.window.replaceAllInCurrent(o), 2);
        QCOMPARE(w.text(), "x y y x"s);
        QCOMPARE(w.selection(), "y y"s);
        QCOMPARE(w.window.countInCurrent(plain("x")), 2);
        QVERIFY(w.window.lastSearchStatus().contains(QStringLiteral("2 matches")));
    }

    void readOnlyDocumentRefusesReplace()
    {
        Window w;
        w.setText("abc");
        w.window.documents().buffer(w.window.currentBuffer())->setUserReadOnly(true);
        QCOMPARE(w.window.replaceAllInCurrent(plain("a", "b")), 0);
        QVERIFY(w.window.lastSearchStatus().contains(QStringLiteral("read only")));
        QCOMPARE(w.text(), "abc"s);
    }

    void invalidRegexIsReported()
    {
        Window w;
        w.setText("abc");
        SearchOptions o = plain("(");
        o.mode = SearchMode::Regex;
        QCOMPARE(w.window.findNextInCurrent(o).status, FindStatus::InvalidRegex);
        QVERIFY(w.window.lastSearchStatus().contains(QStringLiteral("Invalid regular expression")));
        QCOMPARE(w.window.replaceAllInCurrent(o), -1);
    }

    void findAllInCurrentFillsPanelAndNavigates()
    {
        Window w;
        w.setText("hola\nnada\nhola hola\n");
        QCOMPARE(w.window.findAllInCurrent(plain("hola")), 3);
        QVERIFY(w.window.resultsPanel()->isVisibleTo(&w.window));
        const std::string res = w.results();
        QVERIFY(res.starts_with("Search \"hola\" (3 hits in 1 file of 1 searched) [Normal]\n"));
        QVERIFY(res.find("  new 1 (3 hits)\n") != std::string::npos);
        QVERIFY(res.find("\tLine 1: hola\n") != std::string::npos);
        QVERIFY(res.find("\tLine 3: hola hola\n") != std::string::npos);
        QCOMPARE(w.window.resultsPanel()->resultLineCount(), 2); // dos líneas (la 3 con dos coincidencias)

        // El lexer searchResult colorea el número de línea y la palabra buscada
        ScintillaEditBase* view = w.window.resultsPanel()->view();
        const auto lineStart = view->send(SCI_POSITIONFROMLINE, 2);
        // "\tLine 1: hola": el número en +6 y "hola" en +9
        QCOMPARE(view->send(SCI_GETSTYLEAT, static_cast<uptr_t>(lineStart + 6)), sptr_t{3}); // número
        QCOMPARE(view->send(SCI_GETSTYLEAT, static_cast<uptr_t>(lineStart + 9)), sptr_t{4}); // "hola"

        // Ir al segundo resultado (F4 dos veces)
        w.editor()->sci(SCI_GOTOPOS, 0);
        QVERIFY(w.window.resultsPanel()->activateAdjacentResult(true));
        QVERIFY(w.window.resultsPanel()->activateAdjacentResult(true));
        QCOMPARE(w.editor()->sci(SCI_GETSELECTIONSTART), sptr_t{10});
        QCOMPARE(w.selection(), "hola"s);
    }

    void newSearchesGoOnTop()
    {
        Window w;
        w.setText("a b");
        w.window.findAllInCurrent(plain("a"));
        w.window.findAllInCurrent(plain("b"));
        QVERIFY(w.results().starts_with("Search \"b\""));
        QVERIFY(w.results().find("Search \"a\"") != std::string::npos);
    }

    void findAndReplaceInOpenDocuments()
    {
        Window w;
        w.setText("rojo verde");
        const QString path = _tmp.filePath("abierto.txt");
        writeFile(path, "rojo azul rojo\n");
        const BufferId file = w.window.openFile(path);
        QCOMPARE(w.window.findAllInOpenDocuments(plain("rojo")), 3);
        QVERIFY(w.results().find("(3 hits in 2 files of 2 searched)") != std::string::npos);

        QCOMPARE(w.window.replaceAllInOpenDocuments(plain("rojo", "rosa")), 3);
        QCOMPARE(w.window.documents().text(file), "rosa azul rosa\n"s);
        QVERIFY(w.window.documents().buffer(file)->isDirty());
        QCOMPARE(readAll(path), QByteArray("rojo azul rojo\n")); // en el documento, sin guardar
        const BufferId untitled = w.window.pane()->buffers().front();
        QCOMPARE(w.window.documents().text(untitled), "rosa verde"s);
        QVERIFY(w.window.documents().buffer(untitled)->isDirty()); // no estaba a la vista
    }

    void findInFilesAsyncAndOpenResult()
    {
        const QString dir = _tmp.filePath("proyecto");
        writeFile(dir + "/main.cpp", "int main() {\n    return aguja;\n}\n");
        writeFile(dir + "/notas.txt", "sin nada\n");
        writeFile(dir + "/sub/otra.cpp", "// aguja aguja\n");
        Window w;
        w.window.findInFiles(dir, "*.cpp", true, false, plain("aguja"));
        QTRY_VERIFY(!w.window.isFileSearchRunning());
        QTRY_VERIFY(w.window.lastSearchStatus().contains(QStringLiteral("3 hits in 2 of 2 files")));
        const std::string res = w.results();
        QVERIFY(res.find("  " + QDir(dir).filePath("main.cpp").toStdString() + " (1 hit)") != std::string::npos);
        QVERIFY(res.find("\tLine 2:     return aguja;") != std::string::npos);

        // Activar el primer resultado abre el archivo y selecciona la coincidencia
        QVERIFY(w.window.resultsPanel()->activateAdjacentResult(true));
        QCOMPARE(w.window.documents().buffer(w.window.currentBuffer())->filePath(), QDir(dir).filePath("main.cpp"));
        QCOMPARE(w.selection(), "aguja"s);
    }

    void findInFilesUsesUnsavedChanges()
    {
        const QString dir = _tmp.filePath("sin-guardar");
        writeFile(dir + "/a.txt", "nada\n");
        Window w;
        w.window.openFile(dir + "/a.txt");
        w.editor()->sends(SCI_APPENDTEXT, 6, "aguja\n");
        w.window.findInFiles(dir, "*.*", true, false, plain("aguja"));
        QTRY_VERIFY(!w.window.isFileSearchRunning());
        QTRY_VERIFY(w.window.lastSearchStatus().contains(QStringLiteral("1 hit ")));
    }

    void replaceInFilesSavesOpenAndClosedFiles()
    {
        const QString dir = _tmp.filePath("reemplazar");
        writeFile(dir + "/abierto.txt", "viejo y viejo\n");
        writeFile(dir + "/cerrado.txt", QByteArray("viejo con acentos: \xE1\xE9\n")); // Windows-1252
        Window w;
        const BufferId open = w.window.openFile(dir + "/abierto.txt");
        w.prompter->confirmations.enqueue(true);
        w.window.replaceInFiles(dir, "*.txt", true, false, plain("viejo", "nuevo"));
        QTRY_VERIFY(!w.window.isFileSearchRunning());
        QTRY_VERIFY(w.window.lastSearchStatus().contains(QStringLiteral("3 occurrences were replaced in 2 files")));
        // El abierto: cambiado en el documento y guardado
        QCOMPARE(w.window.documents().text(open), "nuevo y nuevo\n"s);
        QVERIFY(!w.window.documents().buffer(open)->isDirty());
        QCOMPARE(readAll(dir + "/abierto.txt"), QByteArray("nuevo y nuevo\n"));
        // El cerrado: en disco, con su codificación original
        QCOMPARE(readAll(dir + "/cerrado.txt"), QByteArray("nuevo con acentos: \xE1\xE9\n"));
    }

    void replaceInFilesCanBeCancelledByUser()
    {
        const QString dir = _tmp.filePath("no-tocar");
        writeFile(dir + "/a.txt", "viejo\n");
        Window w;
        w.prompter->confirmations.enqueue(false);
        w.window.replaceInFiles(dir, "*.txt", true, false, plain("viejo", "nuevo"));
        QVERIFY(!w.window.isFileSearchRunning());
        QCOMPARE(readAll(dir + "/a.txt"), QByteArray("viejo\n"));
    }

    void missingDirectoryIsReported()
    {
        Window w;
        w.window.findInFiles(_tmp.filePath("no-existe"), "*.*", true, false, plain("x"));
        QVERIFY(w.window.lastSearchStatus().contains(QStringLiteral("doesn't exist")));
    }

    // --- Diálogo ---------------------------------------------------------------------

    void dialogPrefillsSelectionAndDrivesSearch()
    {
        Window w;
        w.setText("buscar esto y esto");
        w.editor()->sci(SCI_SETSEL, 7, 11);
        w.window.showFindDialog(FindReplaceDialog::Tab::Find);
        FindReplaceDialog* d = w.window.findDialog();
        QCOMPARE(d->options().pattern, "esto"s);
        QVERIFY(d->isVisible());

        d->findChild<QPushButton*>("countButton")->click();
        QCOMPARE(d->statusText(), w.window.lastSearchStatus());
        QVERIFY(d->statusText().contains(QStringLiteral("2 matches")));

        d->findChild<QPushButton*>("findNextButton")->click();
        QCOMPARE(w.editor()->sci(SCI_GETSELECTIONSTART), sptr_t{14});
        QCOMPARE(d->findHistory().value(0), QStringLiteral("esto"));
    }

    void dialogRegexModeDisablesWholeWord()
    {
        Window w;
        FindReplaceDialog* d = w.window.findDialog();
        auto* whole = d->findChild<QCheckBox*>("wholeWordCheck");
        auto* dot = d->findChild<QCheckBox*>("dotNewlineCheck");
        QVERIFY(whole->isEnabled());
        QVERIFY(!dot->isEnabled());
        whole->setChecked(true);
        d->findChild<QRadioButton*>("modeRegex")->setChecked(true);
        QVERIFY(!whole->isEnabled());
        QVERIFY(dot->isEnabled());
        QVERIFY(!d->options().wholeWord); // no se aplica a una regex
    }

    void dialogTabsShowTheirFields()
    {
        Window w;
        FindReplaceDialog* d = w.window.findDialog();
        d->showTab(FindReplaceDialog::Tab::Replace);
        QVERIFY(d->findChild<QComboBox*>("replaceCombo")->isVisible());
        QVERIFY(!d->findChild<QComboBox*>("directoryCombo")->isVisible());
        d->showTab(FindReplaceDialog::Tab::FindInFiles);
        QVERIFY(d->findChild<QComboBox*>("directoryCombo")->isVisible());
        QVERIFY(d->findChild<QPushButton*>("findInFilesButton")->isVisible());
        QVERIFY(!d->findChild<QPushButton*>("countButton")->isVisible());
    }

    void findInFilesDirectoryDefaultsToCurrentDocument()
    {
        const QString dir = _tmp.filePath("carpeta-doc");
        writeFile(dir + "/x.txt", "x\n");
        Window w;
        w.window.openFile(dir + "/x.txt");
        w.window.showFindDialog(FindReplaceDialog::Tab::FindInFiles);
        QCOMPARE(w.window.findDialog()->directory(), QDir::toNativeSeparators(dir));
    }

    void volatileFindOfWordUnderCaret()
    {
        Window w;
        w.setText("alfa beta alfa");
        w.editor()->sci(SCI_GOTOPOS, 1); // dentro de "alfa"
        w.window.findSelectedText(false);
        QCOMPARE(w.editor()->sci(SCI_GETSELECTIONSTART), sptr_t{10});
    }
};

QTEST_MAIN(TestFindUi)
#include "tst_findui.moc"
