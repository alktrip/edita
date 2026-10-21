// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 12: órdenes de edición de N++ en la ventana (líneas, mayúsculas,
// comentarios, espacios, marcadores, historial de cambios, etiquetas XML,
// estilos de token, plegado y zoom).

#include "FakePrompter.h"

#include "core/Macro.h"
#include "core/Session.h"
#include "syntax/SyntaxStyler.h"
#include "ui/EditCommands.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"

#include <edita_plugin.h>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
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

struct Window
{
    std::unique_ptr<MainWindow> window = std::make_unique<MainWindow>();
    FakePrompter* prompter;

    Window()
    {
        auto fake = std::make_unique<FakePrompter>();
        prompter = fake.get();
        window->setPrompter(std::move(fake));
        Preferences p = window->preferences();
        p.autocStatus = 0;
        p.autoIndent = AutoIndent::None;
        window->setPreferences(p);
    }
    MainWindow& w() const { return *window; }
    EditorView* editor() const { return window->editor(); }
    sptr_t sci(unsigned m, uptr_t w = 0, sptr_t l = 0) const { return editor()->sci(m, w, l); }
    std::string text() const { return window->documents().text(window->currentBuffer()); }
    void setText(const char* t, LangType lang = LangType::Text) const
    {
        window->setLanguage(lang);
        editor()->sends(SCI_SETTEXT, 0, t);
        editor()->sci(SCI_DOCUMENTEND);
    }
    void run(const char* name) const
    {
        QAction* action = window->findChild<QAction*>(QString::fromLatin1(name));
        QVERIFY2(action, name);
        action->trigger();
    }
    void select(sptr_t anchor, sptr_t caret) const { editor()->sci(SCI_SETSEL, static_cast<uptr_t>(anchor), caret); }
    void caretAt(sptr_t pos) const { editor()->sci(SCI_GOTOPOS, static_cast<uptr_t>(pos)); }
    sptr_t indicatorAt(int indicator, sptr_t pos) const
    {
        return editor()->sci(SCI_INDICATORVALUEAT, static_cast<uptr_t>(indicator), pos);
    }
    sptr_t currentLine() const { return sci(SCI_LINEFROMPOSITION, static_cast<uptr_t>(sci(SCI_GETCURRENTPOS))); }
};

} // namespace

class TestEditCommands : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir _tmp;
    QString file(const QString& name) const { return _tmp.filePath(QStringLiteral("files/") + name); }
    QString configDir() const { return _tmp.filePath(QStringLiteral("config/edita")); }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    void cleanup()
    {
        QFile::remove(QDir(configDir()).filePath("config.xml"));
        QFile::remove(QDir(configDir()).filePath("session.xml"));
        QFile::remove(QDir(configDir()).filePath("shortcuts.xml"));
        QDir(QDir(configDir()).filePath("userDefineLangs")).removeRecursively();
        QDir(_tmp.filePath("files")).removeRecursively();
    }

    // --- Líneas ------------------------------------------------------------------------------

    void sortLinesFromTheMenu()
    {
        Window x;
        x.setText("pera\nManzana\n10\n9\nuva");
        x.run("actionSortLexAsc");
        QCOMPARE(x.text(), "10\n9\nManzana\npera\nuva"s);
        x.sci(SCI_UNDO); // de una vez
        QCOMPARE(x.text(), "pera\nManzana\n10\n9\nuva"s);
        x.run("actionSortCaseInsDesc");
        QCOMPARE(x.text(), "uva\npera\nManzana\n9\n10"s);

        // Solo las líneas seleccionadas (la que empieza donde acaba la selección no cuenta)
        x.setText("c\nb\na\nz\n");
        x.select(0, 6); // "c\nb\na\n"
        x.run("actionSortLexAsc");
        QCOMPARE(x.text(), "a\nb\nc\nz\n"s);
        QCOMPARE(x.sci(SCI_GETSELECTIONSTART), sptr_t{0});
        QCOMPARE(x.sci(SCI_GETSELECTIONEND), sptr_t{5});
        // Una sola línea seleccionada: no se ordena
        x.setText("b a\nc");
        x.select(0, 3);
        x.run("actionSortLexDesc");
        QCOMPARE(x.text(), "b a\nc"s);

        // Fin de línea CRLF conservado; enteros
        x.setText("10\r\n9\r\n100");
        x.sci(SCI_SETEOLMODE, SC_EOL_CRLF);
        x.run("actionSortIntAsc");
        QCOMPARE(x.text(), "9\r\n10\r\n100"s);

        // Error de orden numérico: aviso con la línea y sin cambios
        x.setText("1.5\n2.5\n-");
        x.sci(SCI_SETEOLMODE, SC_EOL_LF);
        x.run("actionSortDotAsc");
        QCOMPARE(x.text(), "1.5\n2.5\n-"s);
        QCOMPARE(x.prompter->warnings.size(), 1);
        QVERIFY2(x.prompter->warnings.front().contains("line 3"), qPrintable(x.prompter->warnings.front()));
    }

    void sortByRectangularSelection()
    {
        Window x;
        x.setText("b 3 x\na 1 y\nc 2 z");
        // Columna 2 de las tres líneas
        x.sci(SCI_SETRECTANGULARSELECTIONANCHOR, 2);
        x.sci(SCI_SETRECTANGULARSELECTIONCARET, 15);
        QCOMPARE(x.sci(SCI_GETSELECTIONS), sptr_t{3});
        x.run("actionSortLexAsc");
        QCOMPARE(x.text(), "a 1 y\nc 2 z\nb 3 x"s);
        x.caretAt(0);
        x.run("actionSortReverse");
        QCOMPARE(x.text(), "b 3 x\nc 2 z\na 1 y"s);
    }

    void lineOperations()
    {
        Window x;
        x.setText("a\nb\na\na\nc");
        x.run("actionRemoveConsecutiveDuplicates");
        QCOMPARE(x.text(), "a\nb\na\nc"s);
        x.run("actionRemoveDuplicateLines");
        QCOMPARE(x.text(), "a\nb\nc"s);

        x.setText("a\n\n  \nb\n\n");
        x.run("actionRemoveEmptyLines");
        QCOMPARE(x.text(), "a\n  \nb"s); // la última línea vacía también se va, como en N++
        x.run("actionRemoveBlankLines");
        QCOMPARE(x.text(), "a\nb"s);

        x.setText("uno\ndos");
        x.caretAt(5);
        x.run("actionBlankLineAbove");
        QCOMPARE(x.text(), "uno\n\ndos"s);
        QCOMPARE(x.currentLine(), sptr_t{1});
        x.caretAt(0);
        x.run("actionBlankLineBelow");
        QCOMPARE(x.text(), "uno\n\n\ndos"s);
        x.caretAt(x.sci(SCI_GETLENGTH));
        x.run("actionBlankLineBelow");
        QCOMPARE(x.text(), "uno\n\n\ndos\n"s);
        x.setText("uno\ndos");
        x.caretAt(0);
        x.run("actionBlankLineAbove");
        QCOMPARE(x.text(), "\nuno\ndos"s);

        // Unir solo con varias líneas
        x.setText("a\nb\nc");
        x.select(0, 3);
        x.run("actionJoinLines");
        QCOMPARE(x.text(), "a b\nc"s);
    }

    void caseConversion()
    {
        Window x;
        x.setText("hola mUNDO. adiós");
        x.select(0, x.sci(SCI_GETLENGTH));
        x.run("actionProperCase");
        QCOMPARE(x.text(), "Hola Mundo. Adiós"s);
        x.run("actionInvertCase");
        QCOMPARE(x.text(), "hOLA mUNDO. aDIÓS"s);
        x.run("actionSentenceCase");
        QCOMPARE(x.text(), "Hola mundo. Adiós"s);
        QCOMPARE(x.sci(SCI_GETSELECTIONEND), x.sci(SCI_GETLENGTH)); // la selección sigue entera
        x.run("actionUpperCase");
        QCOMPARE(x.text(), "HOLA MUNDO. ADIÓS"s);
        // Sin selección no hace nada
        x.caretAt(0);
        x.run("actionLowerCase");
        QCOMPARE(x.text(), "HOLA MUNDO. ADIÓS"s);

        // Varias selecciones
        x.setText("abc def ghi");
        x.sci(SCI_SETSELECTION, 3, 0);
        x.sci(SCI_ADDSELECTION, 11, 8);
        x.run("actionUpperCase");
        QCOMPARE(x.text(), "ABC def GHI"s);
        QCOMPARE(x.sci(SCI_GETSELECTIONS), sptr_t{2});
    }

    // --- Comentarios -------------------------------------------------------------------------

    void lineCommentsInCpp()
    {
        Window x;
        x.setText("int a;\n  int b;\n\nint c;", LangType::Cpp);
        x.select(0, x.sci(SCI_GETLENGTH));
        x.run("actionToggleComment");
        QCOMPARE(x.text(), "// int a;\n  // int b;\n\n// int c;"s); // tras la sangría; las vacías no
        x.run("actionToggleComment");
        QCOMPARE(x.text(), "int a;\n  int b;\n\nint c;"s);

        // Comentar dos veces y descomentar una
        x.caretAt(0);
        x.run("actionLineComment");
        x.run("actionLineComment");
        QCOMPARE(x.text(), "// // int a;\n  int b;\n\nint c;"s);
        x.run("actionLineUncomment");
        QCOMPARE(x.text(), "// int a;\n  int b;\n\nint c;"s);
        // También sin el espacio
        x.setText("//int a;", LangType::Cpp);
        x.run("actionLineUncomment");
        QCOMPARE(x.text(), "int a;"s);
        x.sci(SCI_UNDO);
        QCOMPARE(x.text(), "//int a;"s);

        // Comentario de bloque: la selección o, sin ella, la línea sin sangría
        x.setText("int a = 1;", LangType::Cpp);
        x.select(4, 5);
        x.run("actionStreamComment");
        QCOMPARE(x.text(), "int /* a */ = 1;"s);
        x.caretAt(8);
        x.run("actionStreamUncomment");
        QCOMPARE(x.text(), "int a = 1;"s);
        x.setText("  x = 1;", LangType::Cpp);
        x.caretAt(4);
        x.run("actionStreamComment");
        QCOMPARE(x.text(), "  /* x = 1; */"s);
        // Descomentar línea sin comentarios de línea prueba con los de bloque
        x.run("actionLineUncomment");
        QCOMPARE(x.text(), "  x = 1;"s);
    }

    void commentsInOtherLanguages()
    {
        Window x;
        // Python: "#"; los .bat no distinguen mayúsculas ("REM"/"rem")
        x.setText("x = 1", LangType::Python);
        x.run("actionToggleComment");
        QCOMPARE(x.text(), "# x = 1"s);
        x.setText("rem hola", LangType::Batch);
        x.run("actionLineUncomment");
        QCOMPARE(x.text(), "hola"s);

        // HTML no tiene comentario de línea: "<!-- … -->" en cada línea (modo avanzado)
        x.setText("<b>uno</b>\n<i>dos</i>", LangType::Html);
        x.select(0, x.sci(SCI_GETLENGTH));
        x.run("actionToggleComment");
        QCOMPARE(x.text(), "<!-- <b>uno</b> -->\n<!-- <i>dos</i> -->"s);
        x.run("actionToggleComment");
        QCOMPARE(x.text(), "<b>uno</b>\n<i>dos</i>"s);

        // Texto normal: sin comentarios, no hace nada
        x.setText("hola");
        x.run("actionToggleComment");
        x.run("actionStreamComment");
        QCOMPARE(x.text(), "hola"s);

        // Solo lectura: tampoco
        x.setText("int a;", LangType::Cpp);
        x.sci(SCI_SETREADONLY, 1);
        x.run("actionToggleComment");
        QCOMPARE(x.text(), "int a;"s);
        x.sci(SCI_SETREADONLY, 0);
    }

    void commentsInUserLanguage()
    {
        Window x;
        UserLanguage lang = newUserLanguage(QStringLiteral("MiLenguaje"));
        lang.extensions = QStringLiteral("mil");
        lang.keywordLists[UserKeywordList::Comments] = "00-- 03{- 04-}";
        QVERIFY(x.w().syntax().userLanguages().save(lang));
        x.w().userLanguagesChanged();
        x.setText("uno\ndos");
        x.w().setUserLanguage(QStringLiteral("MiLenguaje"));
        x.select(0, x.sci(SCI_GETLENGTH));
        x.run("actionToggleComment");
        QCOMPARE(x.text(), "-- uno\n-- dos"s);
        x.setText("uno");
        x.w().setUserLanguage(QStringLiteral("MiLenguaje"));
        x.select(0, 3);
        x.run("actionStreamComment");
        QCOMPARE(x.text(), "{- uno -}"s);
    }

    // --- Espacios ------------------------------------------------------------------------------

    void blankOperations()
    {
        Window x;
        x.setText("  uno  \n\tdos\t\n tres ");
        x.run("actionTrimLeading");
        QCOMPARE(x.text(), "uno  \ndos\t\ntres "s);
        x.run("actionTrimTrailing");
        QCOMPARE(x.text(), "uno\ndos\ntres"s);
        // Solo en las líneas seleccionadas
        x.setText(" a \n b \n c ");
        x.select(0, 5); // líneas 1 y 2
        x.run("actionTrimBoth");
        QCOMPARE(x.text(), "a\nb\n c "s);

        x.setText("uno\ndos\ntres");
        x.run("actionEolToSpace");
        QCOMPARE(x.text(), "uno dos tres"s);
        x.setText("  uno  \n  dos  ");
        x.run("actionTrimAll");
        QCOMPARE(x.text(), "uno dos"s);

        Preferences p = x.w().preferences();
        p.tabSize = 4;
        x.w().setPreferences(p);
        x.setText("\tuno\n\t\tdos");
        x.run("actionTabToSpace");
        QCOMPARE(x.text(), "    uno\n        dos"s);
        x.run("actionSpaceToTabLeading");
        QCOMPARE(x.text(), "\tuno\n\t\tdos"s);
        x.setText("a       b");
        x.run("actionSpaceToTabAll");
        QCOMPARE(x.text(), "a\t\tb"s);
    }

    // --- Marcadores ------------------------------------------------------------------------------

    void bookmarks()
    {
        Window x;
        x.setText("l0\nl1\nl2\nl3\nl4");
        x.caretAt(3); // línea 1
        x.run("actionToggleBookmark");
        QVERIFY(hasBookmark(*x.editor(), 1));
        x.caretAt(12); // línea 4
        x.run("actionToggleBookmark");
        QCOMPARE(bookmarkedLines(*x.editor()), (std::vector<sptr_t>{1, 4}));

        // F2 / Mayús+F2 dan la vuelta
        x.caretAt(0);
        x.run("actionNextBookmark");
        QCOMPARE(x.currentLine(), sptr_t{1});
        x.run("actionNextBookmark");
        QCOMPARE(x.currentLine(), sptr_t{4});
        x.run("actionNextBookmark");
        QCOMPARE(x.currentLine(), sptr_t{1});
        x.run("actionPreviousBookmark");
        QCOMPARE(x.currentLine(), sptr_t{4});

        // Clic en el margen de símbolos
        EditorView* view = x.editor();
        emit view->marginClicked(x.sci(SCI_POSITIONFROMLINE, 2), Scintilla::KeyMod::Norm, static_cast<int>(kMarginSymbol));
        QCOMPARE(bookmarkedLines(*view), (std::vector<sptr_t>{1, 2, 4}));
        emit view->marginClicked(x.sci(SCI_POSITIONFROMLINE, 2), Scintilla::KeyMod::Norm, static_cast<int>(kMarginSymbol));
        QCOMPARE(bookmarkedLines(*view), (std::vector<sptr_t>{1, 4}));
        QVERIFY(x.sci(SCI_GETMARGINWIDTHN, kMarginSymbol) > 0);

        // Copiar, invertir, eliminar las no marcadas, cortar y pegar
        x.run("actionCopyMarkedLines");
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("l1\nl4"));
        x.run("actionInverseBookmarks");
        QCOMPARE(bookmarkedLines(*view), (std::vector<sptr_t>{0, 2, 3}));
        x.run("actionDeleteUnmarkedLines");
        QCOMPARE(x.text(), "l0\nl2\nl3\n"s);
        QApplication::clipboard()->setText(QStringLiteral("X"));
        x.run("actionPasteMarkedLines");
        QCOMPARE(x.text(), "X\nX\nX\n"s);
        x.sci(SCI_UNDO);
        x.run("actionCutMarkedLines");
        QCOMPARE(x.text(), ""s);
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("l0\nl2\nl3\n"));

        x.setText("a\nb\nc");
        toggleBookmark(*view, 0);
        toggleBookmark(*view, 2);
        x.run("actionDeleteMarkedLines");
        QCOMPARE(x.text(), "b\n"s);
        toggleBookmark(*view, 0);
        x.run("actionClearBookmarks");
        QVERIFY(bookmarkedLines(*view).empty());

        // Sin margen de marcadores
        Preferences p = x.w().preferences();
        p.bookmarkMargin = false;
        x.w().setPreferences(p);
        QCOMPARE(x.sci(SCI_GETMARGINWIDTHN, kMarginSymbol), sptr_t{0});
    }

    void bookmarksAreKeptInTheSession()
    {
        writeFile(file("a.txt"), "uno\ndos\ntres\ncuatro\n");
        Session session;
        {
            Window x;
            x.w().openFile(file("a.txt"));
            toggleBookmark(*x.editor(), 1);
            toggleBookmark(*x.editor(), 3);
            session = x.w().captureSession();
            QCOMPARE(session.mainView.front().bookmarks, (std::vector<std::size_t>{1, 3}));
            QVERIFY(writeSessionFile(QDir(configDir()).filePath("s.xml"), session));
        }
        Session read;
        QVERIFY(readSessionFile(QDir(configDir()).filePath("s.xml"), read));
        Window y;
        y.w().restoreSession(read);
        QCOMPARE(bookmarkedLines(*y.editor()), (std::vector<sptr_t>{1, 3}));
    }

    // --- Historial de cambios ------------------------------------------------------------------

    void changeHistory()
    {
        writeFile(file("h.txt"), "l0\nl1\nl2\nl3\nl4\nl5\n");
        Window x;
        x.w().openFile(file("h.txt"));
        constexpr int modified = 1 << SC_MARKNUM_HISTORY_MODIFIED;
        QVERIFY(x.sci(SCI_GETCHANGEHISTORY) & SC_CHANGE_HISTORY_MARKERS);
        QVERIFY(x.sci(SCI_GETMARGINWIDTHN, kMarginChangeHistory) > 0);
        auto edit = [&](sptr_t line) {
            x.caretAt(x.sci(SCI_POSITIONFROMLINE, static_cast<uptr_t>(line)));
            x.editor()->sends(SCI_REPLACESEL, 0, "x");
        };
        edit(1);
        edit(4);
        QVERIFY(x.sci(SCI_MARKERGET, 1) & modified);
        QVERIFY(!(x.sci(SCI_MARKERGET, 2) & modified));

        x.caretAt(0);
        x.run("actionNextChange");
        QCOMPARE(x.currentLine(), sptr_t{1});
        x.run("actionNextChange");
        QCOMPARE(x.currentLine(), sptr_t{4});
        x.run("actionNextChange"); // vuelta
        QCOMPARE(x.currentLine(), sptr_t{1});
        x.run("actionPreviousChange");
        QCOMPARE(x.currentLine(), sptr_t{4});

        // Al guardar pasan a "guardado"
        QVERIFY(x.w().saveBuffer(x.w().currentBuffer()));
        QVERIFY(x.sci(SCI_MARKERGET, 1) & (1 << SC_MARKNUM_HISTORY_SAVED));

        // Borrar el historial también vacía el deshacer
        x.run("actionClearChangeHistory");
        QCOMPARE(x.sci(SCI_MARKERGET, 1) & (modified | (1 << SC_MARKNUM_HISTORY_SAVED)), sptr_t{0});
        QCOMPARE(x.sci(SCI_CANUNDO), sptr_t{0});

        // Desactivado: sin margen
        Preferences p = x.w().preferences();
        p.changeHistory = 0;
        x.w().setPreferences(p);
        QCOMPARE(x.sci(SCI_GETMARGINWIDTHN, kMarginChangeHistory), sptr_t{0});
        QCOMPARE(x.sci(SCI_GETCHANGEHISTORY), sptr_t{SC_CHANGE_HISTORY_DISABLED});
    }

    // --- Etiquetas XML ------------------------------------------------------------------------

    void xmlTagsAreMatched()
    {
        Window x;
        x.w().show();
        QVERIFY(QTest::qWaitForWindowExposed(&x.w()));
        const char* xml = "<a><b x=\"1\" y='2'>t<b/></b></a>";
        x.setText(xml, LangType::Xml);
        x.sci(SCI_COLOURISE, 0, -1);
        auto tag = [&](sptr_t pos) { return x.indicatorAt(kIndicatorTagMatch, pos); };
        auto attr = [&](sptr_t pos) { return x.indicatorAt(kIndicatorTagAttribute, pos); };

        x.caretAt(4); // dentro de "<b"
        QTRY_COMPARE(tag(3), sptr_t{1});      // "<b"
        QCOMPARE(tag(17), sptr_t{1});         // ">" del final de la apertura
        const auto close = static_cast<sptr_t>(std::string(xml).find("</b>"));
        QCOMPARE(tag(close), sptr_t{1});      // "</b>"
        QCOMPARE(tag(0), sptr_t{0});          // "<a" no
        QCOMPARE(tag(19), sptr_t{0});         // "<b/>" interior no
        QCOMPARE(attr(6), sptr_t{1});         // x="1"
        QCOMPARE(attr(12), sptr_t{1});        // y='2'

        // Desde la etiqueta de cierre
        x.caretAt(close + 2);
        QTRY_COMPARE(tag(3), sptr_t{1});
        // Etiqueta que se cierra a sí misma
        x.caretAt(20);
        QTRY_COMPARE(tag(19), sptr_t{1});
        QCOMPARE(tag(3), sptr_t{0});

        // HTML no distingue mayúsculas
        x.setText("<DIV>t</div>", LangType::Html);
        x.sci(SCI_COLOURISE, 0, -1);
        x.caretAt(2);
        QTRY_COMPARE(tag(7), sptr_t{1});

        // Desactivado en las preferencias: sin marcas
        Preferences p = x.w().preferences();
        p.tagsMatchHighlight = false;
        x.w().setPreferences(p);
        x.caretAt(3);
        QTRY_COMPARE(tag(7), sptr_t{0});

        // Otros lenguajes, nada
        p.tagsMatchHighlight = true;
        x.w().setPreferences(p);
        x.setText("<a></a>", LangType::Cpp);
        x.caretAt(1);
        QTest::qWait(50);
        QCOMPARE(tag(4), sptr_t{0});
    }

    // --- Estilos de token y llaves ---------------------------------------------------------------

    void tokenStyles()
    {
        Window x;
        x.setText("uno dos uno unos Uno");
        x.caretAt(1);
        x.run("actionStyleAll1");
        const int style1 = kIndicatorTokenStyles[0];
        QCOMPARE(x.indicatorAt(style1, 0), sptr_t{1});
        QCOMPARE(x.indicatorAt(style1, 8), sptr_t{1});
        QCOMPARE(x.indicatorAt(style1, 12), sptr_t{0}); // "unos": palabra completa
        QCOMPARE(x.indicatorAt(style1, 17), sptr_t{1}); // "Uno": no distingue mayúsculas
        x.caretAt(5);
        x.run("actionStyleOne2");
        QCOMPARE(x.indicatorAt(kIndicatorTokenStyles[1], 4), sptr_t{1});

        x.caretAt(0);
        x.run("actionNextStyle1");
        QCOMPARE(x.sci(SCI_GETSELECTIONSTART), sptr_t{8});
        x.run("actionNextStyle1");
        QCOMPARE(x.sci(SCI_GETSELECTIONSTART), sptr_t{17});
        x.run("actionPreviousStyle1");
        QCOMPARE(x.sci(SCI_GETSELECTIONSTART), sptr_t{8});

        x.run("actionUnstyle1");
        QCOMPARE(x.indicatorAt(style1, 0), sptr_t{0});
        x.run("actionUnstyleAll");
        QCOMPARE(x.indicatorAt(kIndicatorTokenStyles[1], 4), sptr_t{0});
    }

    void matchingBraces()
    {
        Window x;
        x.setText("f(a[1]) {x}", LangType::Cpp);
        x.caretAt(1);
        x.run("actionGoToMatchingBrace");
        QCOMPARE(x.sci(SCI_GETCURRENTPOS), sptr_t{6});
        x.caretAt(1);
        x.run("actionSelectMatchingBraces");
        QCOMPARE(x.sci(SCI_GETSELECTIONSTART), sptr_t{1});
        QCOMPARE(x.sci(SCI_GETSELECTIONEND), sptr_t{7});
    }

    // --- Plegado y zoom ----------------------------------------------------------------------

    void folding()
    {
        Window x;
        x.setText("void f() {\n  if (a) {\n    b();\n  }\n}\nint g;", LangType::Cpp);
        x.sci(SCI_COLOURISE, 0, -1);
        auto visible = [&](sptr_t line) { return x.sci(SCI_GETLINEVISIBLE, static_cast<uptr_t>(line)) != 0; };
        x.run("actionFoldAll");
        QVERIFY(!visible(1));
        QVERIFY(visible(5));
        x.run("actionUnfoldAll");
        QVERIFY(visible(2));

        x.run("actionFoldLevel2"); // el "if"
        QVERIFY(visible(1));
        QVERIFY(!visible(2));
        x.run("actionUnfoldLevel2");
        QVERIFY(visible(2));

        x.caretAt(x.sci(SCI_POSITIONFROMLINE, 2));
        x.run("actionFoldCurrent");
        QVERIFY(!visible(2));
        QVERIFY(visible(1));
        x.run("actionUnfoldCurrent");
        QVERIFY(visible(2));

        // Python: los niveles los da la sangría
        x.setText("def f():\n    if a:\n        b()\n    c()\n", LangType::Python);
        x.sci(SCI_COLOURISE, 0, -1);
        x.run("actionFoldLevel2");
        QVERIFY(visible(1));
        QVERIFY(!visible(2));
        x.run("actionFoldLevel1");
        QVERIFY(!visible(1));
        x.run("actionUnfoldAll");
        QVERIFY(visible(2));
    }

    void zoom()
    {
        Window x;
        x.run("actionZoomIn");
        x.run("actionZoomIn");
        QCOMPARE(x.sci(SCI_GETZOOM), sptr_t{2});
        x.run("actionZoomOut");
        QCOMPARE(x.sci(SCI_GETZOOM), sptr_t{1});
        x.run("actionZoomRestore");
        QCOMPARE(x.sci(SCI_GETZOOM), sptr_t{0});
    }

    // --- Macros y plugins ---------------------------------------------------------------------

    void newCommandsWorkInMacrosAndPlugins()
    {
        Window x;
        x.setText("b\na", LangType::Cpp);
        x.w().toggleMacroRecording();
        x.run("actionSortLexAsc");
        x.run("actionToggleComment");
        x.w().toggleMacroRecording();
        const Macro macro = x.w().currentMacro();
        QCOMPARE(macro.size(), std::size_t{2});
        QCOMPARE(macro[0].type, MacroStep::MenuCommand);
        QCOMPARE(macro[0].wParam, std::uintptr_t{42059}); // IDM_EDIT_SORTLINES_LEXICOGRAPHIC_ASCENDING
        QCOMPARE(macro[1].wParam, std::uintptr_t{42022}); // IDM_EDIT_BLOCK_COMMENT
        QCOMPARE(x.text(), "// a\nb"s);

        x.setText("d\nc", LangType::Cpp);
        x.w().playMacro(macro);
        QCOMPARE(x.text(), "// c\nd"s);

        // NPPM_MENUCOMMAND con el IDM de N++
        x.setText("int a;", LangType::Cpp);
        QCOMPARE(x.w().handlePluginMessage(NPPM_MENUCOMMAND, 0, 42022), intptr_t{1});
        QCOMPARE(x.text(), "// int a;"s);
        QCOMPARE(x.w().handlePluginMessage(NPPM_MENUCOMMAND, 0, 44010), intptr_t{1}); // IDM_VIEW_FOLDALL
    }
};

QTEST_MAIN(TestEditCommands)
#include "tst_editcommands.moc"
