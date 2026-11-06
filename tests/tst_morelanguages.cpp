// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 65: Kotlin (un lenguaje preinstalado, como los Markdown de N++), Delphi en Pascal, y la
// actualización de langs.xml desde el modelo (NppParameters::updateLangXml)

#include "FakePrompter.h"
#include "doc/Buffer.h"
#include "syntax/LanguageRegistry.h"
#include "syntax/SyntaxManager.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"

#include <SciLexer.h>
#include <Scintilla.h>

#include <QAction>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;
using edita::test::FakePrompter;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QByteArray readFile(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

} // namespace

class TestMoreLanguages : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

    std::unique_ptr<MainWindow> window()
    {
        auto w = std::make_unique<MainWindow>();
        w->setPrompter(std::make_unique<FakePrompter>());
        QCoreApplication::processEvents();
        return w;
    }

    // El estilo en la primera aparición de `what` (tras colorear todo el documento)
    static int styleOf(MainWindow& w, const QByteArray& text, const char* what)
    {
        const qsizetype at = text.indexOf(what);
        if (at < 0)
            return -100;
        w.editor()->sci(SCI_COLOURISE, 0, -1);
        return static_cast<int>(w.editor()->sci(SCI_GETSTYLEAT, static_cast<uptr_t>(at)));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    // Kotlin: se abre por su extensión con su lenguaje, que colorea lo de Kotlin
    void kotlin()
    {
        const QByteArray text = "// nota\n"
                                "fun main() {\n"
                                "    val s: String = \"hola\"\n"
                                "    @Deprecated private val x = 0x1FL\n"
                                "    val r = \"\"\"crudo\"\"\"\n"
                                "    val c = 'a'\n"
                                "    val n = 1.5f + 0xFF_FF\n"
                                "    /* bloque */\n"
                                "}\n";
        const QString path = _tmp.filePath("Main.kt");
        writeFile(path, text);
        auto w = window();
        w->openFile(path);
        const Buffer* buf = w->documents().buffer(w->currentBuffer());
        QCOMPARE(buf->language(), LangType::User);
        QCOMPARE(buf->userLanguage(), QStringLiteral("Kotlin (preinstalled)"));
        QCOMPARE(styleOf(*w, text, "// nota"), SCE_USER_STYLE_COMMENTLINE);
        QCOMPARE(styleOf(*w, text, "fun"), SCE_USER_STYLE_KEYWORD1);
        QCOMPARE(styleOf(*w, text, "main"), SCE_USER_STYLE_DEFAULT);
        QCOMPARE(styleOf(*w, text, "String"), SCE_USER_STYLE_KEYWORD3);
        QCOMPARE(styleOf(*w, text, "\"hola\""), SCE_USER_STYLE_DELIMITER2);
        QCOMPARE(styleOf(*w, text, "@Deprecated"), SCE_USER_STYLE_KEYWORD4);
        QCOMPARE(styleOf(*w, text, "private"), SCE_USER_STYLE_KEYWORD2);
        QCOMPARE(styleOf(*w, text, "0x1FL"), SCE_USER_STYLE_NUMBER);
        QCOMPARE(styleOf(*w, text, "L\n"), SCE_USER_STYLE_NUMBER); // el sufijo, con el número
        QCOMPARE(styleOf(*w, text, "1.5f"), SCE_USER_STYLE_NUMBER);
        QCOMPARE(styleOf(*w, text, "0xFF_FF"), SCE_USER_STYLE_NUMBER);
        QCOMPARE(styleOf(*w, text, "\"\"\"crudo"), SCE_USER_STYLE_DELIMITER1);
        QCOMPARE(styleOf(*w, text, "'a'"), SCE_USER_STYLE_DELIMITER3);
        QCOMPARE(styleOf(*w, text, "/* bloque"), SCE_USER_STYLE_COMMENT);
        QCOMPARE(styleOf(*w, text, "{"), SCE_USER_STYLE_FOLDER_IN_CODE1);
        // Se pliega por las llaves
        QVERIFY(w->editor()->sci(SCI_GETFOLDLEVEL, 1) & SC_FOLDLEVELHEADERFLAG); // fun main() {
        QVERIFY(!(w->editor()->sci(SCI_GETFOLDLEVEL, 0) & SC_FOLDLEVELHEADERFLAG)); // el comentario de antes, no
        // Tras un comentario de bloque de una línea (el /** */ de la documentación), el plegado
        // sigue en su línea: LexUser procesaba dos veces ese fin de línea (0007-lexuser-eol-una-vez)
        {
            const QString doc = _tmp.filePath("Doc.kt");
            writeFile(doc, "/** documentación */\nfun b() {\n    x\n}\nval s = \"a\"\nfun c() {\n}\n");
            w->openFile(doc);
            w->editor()->sci(SCI_COLOURISE, 0, -1);
            QVERIFY(w->editor()->sci(SCI_GETFOLDLEVEL, 1) & SC_FOLDLEVELHEADERFLAG);  // fun b() {
            QVERIFY(!(w->editor()->sci(SCI_GETFOLDLEVEL, 2) & SC_FOLDLEVELHEADERFLAG));
            QVERIFY(w->editor()->sci(SCI_GETFOLDLEVEL, 5) & SC_FOLDLEVELHEADERFLAG);  // fun c() {
            w->openFile(path);
        }
        // Y .kts (los scripts de Gradle)
        QCOMPARE(w->syntax().detectUserLanguage(_tmp.filePath("build.gradle.kts")), QStringLiteral("Kotlin (preinstalled)"));
        // En modo oscuro, su versión oscura (como Markdown)
        w->syntax().setDarkMode(true);
        QCOMPARE(w->syntax().detectUserLanguage(path), QStringLiteral("Kotlin (preinstalled dark mode)"));
        w->syntax().setDarkMode(false);
        // Comentar usa // (la lista «Comments» del lenguaje)
        w->editor()->sci(SCI_GOTOLINE, 1);
        w->findChild<QAction*>(QStringLiteral("actionToggleComment"))->trigger();
        const std::string after = w->editor()->text();
        QVERIFY2(after.find("//fun main()") != std::string::npos || after.find("// fun main()") != std::string::npos, after.c_str());
    }

    // Delphi: el lenguaje Pascal (su lexer es el de Delphi), con sus extensiones y palabras
    void delphi()
    {
        auto w = window();
        // En el menú se encuentra también como Delphi
        QAction* pascal = w->findChild<QAction*>(QStringLiteral("actionLang%1").arg(static_cast<int>(LangType::Pascal)));
        QVERIFY(pascal);
        QCOMPARE(pascal->text(), QStringLiteral("Pascal / Delphi"));
        for (const char* ext : {"dpr", "dpk", "dfm", "lfm", "pas"})
            QCOMPARE(w->syntax().detect(_tmp.filePath(QStringLiteral("x.") + ext), {}), LangType::Pascal);

        const QByteArray text = "unit Prueba;\n"
                                "type\n"
                                "  TAyudante = class helper for TObject\n"
                                "  strict private\n"
                                "    procedure Hola; reintroduce;\n"
                                "  end;\n"
                                "// fin\n";
        const QString path = _tmp.filePath("Prueba.pas");
        writeFile(path, text);
        w->openFile(path);
        QCOMPARE(styleOf(*w, text, "unit"), SCE_PAS_WORD);
        QCOMPARE(styleOf(*w, text, "helper"), SCE_PAS_WORD);
        QCOMPARE(styleOf(*w, text, "strict"), SCE_PAS_WORD);
        QCOMPARE(styleOf(*w, text, "reintroduce"), SCE_PAS_WORD);
        QCOMPARE(styleOf(*w, text, "// fin"), SCE_PAS_COMMENTLINE);
        QCOMPARE(styleOf(*w, text, "TAyudante"), SCE_PAS_IDENTIFIER);
        // Comentar una línea: con // (antes Pascal solo tenía el comentario de bloque)
        w->editor()->sci(SCI_GOTOLINE, 0);
        w->findChild<QAction*>(QStringLiteral("actionToggleComment"))->trigger();
        QVERIFY(w->editor()->text().starts_with("//"));
    }

    // Lo nuevo del modelo llega al langs.xml que el usuario ya tenía, sin quitarle nada
    void langsXmlIsUpdatedFromTheModel()
    {
        const QString user = _tmp.filePath("merge/langs.xml");
        const QString model = _tmp.filePath("merge/langs.model.xml");
        writeFile(user, R"xml(<?xml version="1.0" encoding="UTF-8" ?>
<NotepadPlus modelDate="20260101">
    <!-- del usuario -->
    <Languages>
        <Language name="pascal" ext="pas mipas" commentStart="{" commentEnd="}">
            <Keywords name="instre1">begin end mipalabra</Keywords>
        </Language>
        <Language name="vacio" ext="">
        </Language>
    </Languages>
</NotepadPlus>
)xml");
        writeFile(model, R"xml(<?xml version="1.0" encoding="UTF-8" ?>
<NotepadPlus modelDate="20261106">
    <Languages>
        <Language name="pascal" ext="pas dpr dpk" commentLine="//" commentStart="(*" commentEnd="*)">
            <Keywords name="instre1">begin end strict</Keywords>
            <Keywords name="type1">Integer</Keywords>
        </Language>
        <Language name="vacio" ext="vac">
        </Language>
        <Language name="nuevo" ext="nv" commentLine="#">
            <Keywords name="instre1">uno dos</Keywords>
        </Language>
    </Languages>
</NotepadPlus>
)xml");
        QString error;
        QVERIFY2(updateLangsFromModel(user, model, &error), qPrintable(error));
        LanguageRegistry merged;
        QVERIFY(merged.load(user, &error));
        auto find = [&merged](const char* name) -> const LanguageDefinition* {
            for (const LanguageDefinition& d : merged.languages())
                if (d.name == name)
                    return &d;
            return nullptr;
        };
        const LanguageDefinition* pascal = find("pascal");
        QVERIFY(pascal);
        QCOMPARE(pascal->extensions, std::string("pas mipas dpr dpk")); // las del usuario, y las nuevas
        QCOMPARE(pascal->keywords[0], std::string("begin end mipalabra strict"));
        QCOMPARE(pascal->keywords[KeywordIndex::Type], std::string("Integer"));
        QCOMPARE(pascal->commentLine, std::string("//"));  // el atributo que faltaba
        QCOMPARE(pascal->commentStart, std::string("{"));  // el del usuario se respeta
        QCOMPARE(find("vacio")->extensions, std::string()); // vacío a propósito: se respeta
        QVERIFY(find("nuevo"));
        QCOMPARE(find("nuevo")->keywords[0], std::string("uno dos"));
        const QByteArray xml = readFile(user);
        QVERIFY(xml.contains("<!-- del usuario -->"));
        QVERIFY(xml.contains("modelDate=\"20261106\""));
        // Ya al día: no se vuelve a tocar
        QVERIFY(!updateLangsFromModel(user, model));
        QCOMPARE(readFile(user), xml);

        // Y al arrancar: un langs.xml viejo (de la versión anterior) recibe lo de Delphi
        QFile::remove(_tmp.filePath("config/edita/langs.xml"));
        {
            auto first = window(); // crea langs.xml desde el modelo
        }
        QByteArray old = readFile(_tmp.filePath("config/edita/langs.xml"));
        QVERIFY(!old.isEmpty());
        // Como el de la beta anterior: sin lo de Delphi y con un modelDate más viejo
        QVERIFY(old.contains("ext=\"pas pp p inc lpr dpr dpk dfm lfm\""));
        old.replace("ext=\"pas pp p inc lpr dpr dpk dfm lfm\"", "ext=\"pas pp p inc lpr dpr\"");
        const QRegularExpression date(QStringLiteral("modelDate=\"(\\d+)\""));
        const QString modelDate = date.match(QString::fromUtf8(old)).captured(1);
        QVERIFY(!modelDate.isEmpty());
        old.replace(QStringLiteral("modelDate=\"%1\"").arg(modelDate).toUtf8(), "modelDate=\"20000101\"");
        writeFile(_tmp.filePath("config/edita/langs.xml"), old);
        auto w = window();
        QCOMPARE(w->syntax().detect(_tmp.filePath("x.dpk"), {}), LangType::Pascal);
        QVERIFY(readFile(_tmp.filePath("config/edita/langs.xml")).contains(QStringLiteral("modelDate=\"%1\"").arg(modelDate).toUtf8()));
    }
};

QTEST_MAIN(TestMoreLanguages)
#include "tst_morelanguages.moc"
