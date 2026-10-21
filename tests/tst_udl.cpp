// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 10: lenguajes definidos por el usuario (userDefineLang.xml de N++ con LexUser)
// y su diálogo.

#include "FakePrompter.h"

#include "core/Paths.h"
#include "core/Session.h"
#include "syntax/UserLanguages.h"
#include "ui/DocStatusBar.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/UserLanguageDialog.h"

#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QPlainTextEdit>
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

// Un lenguaje sencillo: palabras clave, comentarios de línea y de bloque, cadenas
UserLanguage sampleLanguage()
{
    UserLanguage lang = newUserLanguage(QStringLiteral("Mini"));
    lang.extensions = QStringLiteral("mini mn");
    lang.keywordLists[UserKeywordList::Keywords1] = "si entonces fin";
    lang.keywordLists[UserKeywordList::Comments] = userComposeSlots({"//", "", "", "/*", "*/"});
    QStringList delimiters(24, QString());
    delimiters[0] = QStringLiteral("\"");
    delimiters[1] = QStringLiteral("\\");
    delimiters[2] = QStringLiteral("\"");
    lang.keywordLists[UserKeywordList::Delimiters] = userComposeSlots(delimiters);
    lang.style(4)->fgColor = 0xFF0000; // azul (BGR)
    lang.style(4)->fontStyle = kFontStyleBold;
    return lang;
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
    }
    MainWindow& w() const { return *window; }
    EditorView* editor() const { return window->editor(); }
    const Buffer* current() const { return window->documents().buffer(window->currentBuffer()); }
    int styleAt(sptr_t pos) const
    {
        editor()->sci(SCI_COLOURISE, 0, -1);
        return static_cast<int>(editor()->sci(SCI_GETSTYLEAT, static_cast<uptr_t>(pos)));
    }
};

} // namespace

class TestUdl : public QObject
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
        QFile::remove(QDir(configDir()).filePath("userDefineLang.xml"));
        QDir(QDir(configDir()).filePath("userDefineLangs")).removeRecursively();
        QFile::remove(QDir(configDir()).filePath("session.xml"));
        QFile::remove(QDir(configDir()).filePath("config.xml"));
        QDir(_tmp.filePath("files")).removeRecursively();
    }

    // --- Formato --------------------------------------------------------------------

    void readsTheMarkdownShippedWithNotepadPlusPlus()
    {
        std::vector<UserLanguage> langs;
        QVERIFY(UserLanguages::readFile(QStringLiteral(EDITA_SOURCE_DIR "/data/userDefineLangs/markdown._preinstalled.udl.xml"), langs));
        QCOMPARE(langs.size(), std::size_t{1});
        const UserLanguage& md = langs[0];
        QCOMPARE(md.name, QStringLiteral("Markdown (preinstalled)"));
        QCOMPARE(md.extensions, QStringLiteral("md markdown"));
        QVERIFY(md.caseIgnored);
        QCOMPARE(md.forcePureLC, 2);
        QVERIFY(md.isPrefix[0] && !md.isPrefix[7]);
        QCOMPARE(md.keywordLists[UserKeywordList::Comments], "00# 01 02((EOL)) 03<!-- 04-->"s);
        QCOMPARE(md.styles.size(), std::size_t{kUserStyleCount});
        QVERIFY(md.style(4)->fgColor != kStyleNotUsed);
        QVERIFY(md.matches(QStringLiteral("/x/LEEME.MD")));
        QVERIFY(!md.matches(QStringLiteral("/x/nota.txt")));
    }

    void slotsOfCommentsAndDelimiters()
    {
        const std::string comments = "00# 00; 01 02((EOL)) 03<!-- 04-->";
        QCOMPARE(userSlotWords(comments, 0), QStringLiteral("# ;"));
        QCOMPARE(userSlotWords(comments, 1), QString());
        QCOMPARE(userSlotWords(comments, 2), QStringLiteral("((EOL))"));
        QCOMPARE(userSlotWords(comments, 3), QStringLiteral("<!--"));
        QCOMPARE(userComposeSlots({"# ;", "", "((EOL))", "<!--", "-->"}), comments);
        // Un grupo con espacios se conserva entero
        const std::string grouped = userComposeSlots({"((fin de linea)) x"});
        QCOMPARE(grouped, "00((fin de linea)) 00x"s);
        QCOMPARE(userSlotWords(grouped, 0), QStringLiteral("((fin de linea)) x"));
        QCOMPARE(userSlotWords("10a 11b 12c", 11), QStringLiteral("b")); // casillas de dos cifras
    }

    void oldFormatsAreConverted()
    {
        const QString path = _tmp.filePath("viejo.xml");
        writeFile(path, R"XML(<NotepadPlus><UserLang name="Viejo" ext="old">
<Settings><Global caseIgnored="yes" /><Prefix words1="yes" words2="no" words3="no" words4="no" /></Settings>
<KeywordLists>
<Keywords name="Delimiters">"00"00</Keywords>
<Keywords name="Comment">1/* 2*/ 0//</Keywords>
<Keywords name="Words1">alfa beta</Keywords>
<Keywords name="Operators">+ -</Keywords>
<Keywords name="Folder+">{</Keywords>
</KeywordLists>
<Styles><WordsStyle name="KEYWORD1" fgColor="FF0000" fontStyle="1" /><WordsStyle name="COMMENT LINE" fgColor="008000" /></Styles>
</UserLang></NotepadPlus>)XML");
        std::vector<UserLanguage> langs;
        QVERIFY(UserLanguages::readFile(path, langs));
        const UserLanguage& old = langs.at(0);
        QVERIFY(old.isPrefix[0]);
        QCOMPARE(old.keywordLists[UserKeywordList::Keywords1], "alfa beta"s);
        QCOMPARE(old.keywordLists[UserKeywordList::Operators1], "+ -"s);
        QCOMPARE(old.keywordLists[UserKeywordList::FoldersInCode1Open], "{"s);
        QCOMPARE(userSlotWords(old.keywordLists[UserKeywordList::Comments], 0), QStringLiteral("//"));
        QCOMPARE(userSlotWords(old.keywordLists[UserKeywordList::Comments], 3), QStringLiteral("/*"));
        QCOMPARE(userSlotWords(old.keywordLists[UserKeywordList::Comments], 4), QStringLiteral("*/"));
        QCOMPARE(userSlotWords(old.keywordLists[UserKeywordList::Delimiters], 0), QStringLiteral("\""));
        QCOMPARE(userSlotWords(old.keywordLists[UserKeywordList::Delimiters], 2), QStringLiteral("\""));
        QCOMPARE(old.style(4)->fgColor, 0x0000FF); // RRGGBB → BGR
        QCOMPARE(old.style(4)->fontStyle, kFontStyleBold);
        QCOMPARE(old.style(2)->fgColor, 0x008000);
    }

    void writeAndReadBack()
    {
        const QString path = _tmp.filePath("ida.xml");
        UserLanguage lang = sampleLanguage();
        lang.isPrefix[3] = true;
        lang.style(16)->nesting = 0x100 | 0x1;
        QVERIFY(UserLanguages::writeFile(path, {lang}));
        std::vector<UserLanguage> back;
        QVERIFY(UserLanguages::readFile(path, back));
        const UserLanguage& b = back.at(0);
        QCOMPARE(b.name, lang.name);
        QCOMPARE(b.extensions, lang.extensions);
        QVERIFY(b.keywordLists == lang.keywordLists);
        QVERIFY(b.isPrefix == lang.isPrefix);
        QCOMPARE(b.style(4)->fgColor, 0xFF0000);
        QCOMPARE(b.style(4)->fontStyle, kFontStyleBold);
        QCOMPARE(b.style(16)->nesting, 0x101);
        QCOMPARE(b.style(1)->colorStyle, kColorStyleForeground);
        const QByteArray xml = readAll(path);
        QVERIFY(xml.contains("<UserLang name=\"Mini\" ext=\"mini mn\" udlVersion=\"2.1\">"));
        QVERIFY(xml.contains("<Keywords name=\"Keywords1\">si entonces fin</Keywords>"));
        QVERIFY(xml.contains("<WordsStyle name=\"KEYWORDS1\" fgColor=\"0000FF\""));
    }

    void registryLoadsUserAndInstalledFilesAndPrefersLightThemes()
    {
        UserLanguages::writeFile(QDir(configDir()).filePath("userDefineLang.xml"), {sampleLanguage()});
        UserLanguages registry;
        QVERIFY(registry.load(configDir()).isEmpty());
        QVERIFY(registry.find(QStringLiteral("Mini")));
        // Los Markdown preinstalados (claro y oscuro); sin modo oscuro gana el claro
        QVERIFY(registry.find(QStringLiteral("Markdown (preinstalled dark mode)")));
        QCOMPARE(registry.forFile(QStringLiteral("/a/b.md"))->name, QStringLiteral("Markdown (preinstalled)"));
        QCOMPARE(registry.forFile(QStringLiteral("/a/b.MN"))->name, QStringLiteral("Mini"));
        QVERIFY(!registry.forFile(QStringLiteral("/a/b.txt")));

        // Uno instalado no se borra; al guardarlo queda una copia del usuario
        QString error;
        QVERIFY(!registry.remove(QStringLiteral("Markdown (preinstalled)"), &error));
        QVERIFY(!error.isEmpty());
        UserLanguage md = *registry.find(QStringLiteral("Markdown (preinstalled)"));
        md.extensions = QStringLiteral("md");
        QVERIFY(registry.save(md));
        QVERIFY(registry.find(QStringLiteral("Markdown (preinstalled)"))->sourceFile.startsWith(configDir()));
        QVERIFY(QFileInfo::exists(QDir(configDir()).filePath("userDefineLangs/Markdown (preinstalled).xml")));
    }

    // --- En la ventana ------------------------------------------------------------------

    void filesAreColouredWithTheUserLanguage()
    {
        UserLanguages::writeFile(QDir(configDir()).filePath("userDefineLangs/mini.xml"), {sampleLanguage()});
        writeFile(file("prueba.mini"), "si x entonces \"cadena\" // nota\n/* bloque */ fin\n");
        Window x;
        x.w().openFile(file("prueba.mini"));
        QCOMPARE(x.current()->language(), LangType::User);
        QCOMPARE(x.current()->userLanguage(), QStringLiteral("Mini"));
        QCOMPARE(x.w().docStatusBar()->languageText(), QStringLiteral("User Defined language - Mini"));
        char lexer[32] = {};
        x.editor()->sci(SCI_GETLEXERLANGUAGE, 0, reinterpret_cast<sptr_t>(lexer));
        QCOMPARE(QByteArray(lexer), QByteArray("user"));
        QCOMPARE(x.styleAt(0), 4);   // "si": Palabras 1
        QCOMPARE(x.styleAt(3), 0);   // "x"
        QCOMPARE(x.styleAt(16), 16); // dentro de la cadena: Delimitadores 1
        QCOMPARE(x.styleAt(26), 2);  // comentario de línea
        QCOMPARE(x.styleAt(36), 1);  // comentario de bloque
        QCOMPARE(x.editor()->sci(SCI_STYLEGETFORE, 4), sptr_t{0xFF0000});
        QCOMPARE(x.editor()->sci(SCI_STYLEGETBOLD, 4), sptr_t{1});
    }

    void languageMenuAndSession()
    {
        UserLanguages::writeFile(QDir(configDir()).filePath("userDefineLangs/mini.xml"), {sampleLanguage()});
        writeFile(file("otro.txt"), "si fin\n");
        {
            Window x;
            x.w().setSessionEnabled(true);
            x.w().openFile(file("otro.txt"));
            QCOMPARE(x.current()->language(), LangType::Text);
            x.w().setUserLanguage(QStringLiteral("Mini"));
            QCOMPARE(x.current()->userLanguage(), QStringLiteral("Mini"));
            QCOMPARE(x.styleAt(0), 4);
            // Volver a un lenguaje de N++ olvida el del usuario
            x.w().setLanguage(LangType::Cpp);
            QVERIFY(x.current()->userLanguage().isEmpty());
            x.w().setUserLanguage(QStringLiteral("Mini"));
            QVERIFY(x.w().close());
        }
        Session s;
        QVERIFY(readSessionFile(QDir(configDir()).filePath("session.xml"), s));
        QCOMPARE(s.mainView.at(0).languageName, QStringLiteral("Mini")); // como N++: el nombre del lenguaje
        Window y;
        QVERIFY(y.w().restoreLastSession());
        QCOMPARE(y.current()->language(), LangType::User);
        QCOMPARE(y.current()->userLanguage(), QStringLiteral("Mini"));
    }

    void dialogCreatesEditsAndRemovesLanguages()
    {
        writeFile(file("doc.nuevo"), "hola mundo // fin\n");
        Window x;
        x.w().openFile(file("doc.nuevo"));
        QCOMPARE(x.current()->language(), LangType::Text);

        UserLanguages& registry = x.w().syntax().userLanguages();
        UserLanguageDialog dialog(registry, *x.prompter, &x.w());
        QObject::connect(&dialog, &UserLanguageDialog::languagesChanged, &x.w(), [&x] {
            // Lo que hace la ventana al recibir la señal (MainWindow::userLanguagesChanged)
            x.w().userLanguagesChanged();
        });
        QVERIFY(dialog.createLanguage(QStringLiteral("Nuevo")));
        QCOMPARE(dialog.currentName(), QStringLiteral("Nuevo"));
        QVERIFY(!dialog.createLanguage(QStringLiteral("Nuevo"))); // repetido
        QCOMPARE(x.prompter->warnings.size(), 1);

        dialog.findChild<QLineEdit*>("extensions")->setText(QStringLiteral("nuevo"));
        dialog.findChild<QPlainTextEdit*>("keywords1")->setPlainText(QStringLiteral("hola"));
        dialog.findChild<QLineEdit*>("lineCommentOpen")->setText(QStringLiteral("//"));
        dialog.findChild<QCheckBox*>("caseIgnored")->setChecked(true);
        QVERIFY(dialog.saveCurrent());
        const UserLanguage* saved = registry.find(QStringLiteral("Nuevo"));
        QVERIFY(saved);
        QVERIFY(saved->caseIgnored);
        QCOMPARE(userSlotWords(saved->keywordLists[UserKeywordList::Comments], 0), QStringLiteral("//"));
        QVERIFY(readAll(QDir(configDir()).filePath("userDefineLangs/Nuevo.xml")).contains("hola"));

        // El documento abierto pasa a ese lenguaje y se colorea
        QCOMPARE(x.current()->userLanguage(), QStringLiteral("Nuevo"));
        QCOMPARE(x.styleAt(0), 4);
        QCOMPARE(x.styleAt(12), 2);

        QVERIFY(dialog.renameCurrent(QStringLiteral("Renombrado")));
        QVERIFY(!registry.find(QStringLiteral("Nuevo")));
        QVERIFY(registry.find(QStringLiteral("Renombrado")));
        QVERIFY(dialog.removeCurrent());
        QVERIFY(!registry.find(QStringLiteral("Renombrado")));
        QVERIFY(!QFileInfo::exists(QDir(configDir()).filePath("userDefineLangs/Renombrado.xml")));
        QCOMPARE(x.current()->language(), LangType::Text); // su lenguaje ya no existe
    }

    void importNotepadPlusPlusFile()
    {
        const QString exported = _tmp.filePath("exportado.xml");
        UserLanguages::writeFile(exported, {sampleLanguage()});
        writeFile(file("a.mini"), "si\n");
        Window x;
        x.w().openFile(file("a.mini"));
        QCOMPARE(x.current()->language(), LangType::Text);
        QCOMPARE(x.w().importUserLanguage(exported), QStringList{"Mini"});
        QVERIFY(QFileInfo::exists(QDir(configDir()).filePath("userDefineLangs/exportado.xml")));
        QCOMPARE(x.current()->userLanguage(), QStringLiteral("Mini"));

        // Y exportarlo de nuevo
        QVERIFY(x.w().syntax().userLanguages().exportLanguage(QStringLiteral("Mini"), _tmp.filePath("otra.xml")));
        std::vector<UserLanguage> back;
        QVERIFY(UserLanguages::readFile(_tmp.filePath("otra.xml"), back));
        QCOMPARE(back.at(0).name, QStringLiteral("Mini"));

        x.w().importUserLanguage(_tmp.filePath("no-existe.xml"));
        QCOMPARE(x.prompter->warnings.size(), 1);
    }
};

QTEST_MAIN(TestUdl)
#include "tst_udl.moc"
