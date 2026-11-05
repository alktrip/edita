// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 63: los lexers de los plugins (los lenguajes «externos» de N++). El plugin de la
// prueba (fixtures/lexer-plugin) exporta GetLexerCount, GetLexerName y CreateLexer, y su
// XML va en plugins/config, como plugins\Config de N++.

#include "FakePrompter.h"
#include "doc/Buffer.h"
#include "syntax/ExternalLexers.h"
#include "ui/DocStatusBar.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"

#include <edita_plugin.h>

#include <Scintilla.h>

#include <QAction>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

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

int styleAt(EditorView& view, sptr_t pos)
{
    return static_cast<int>(view.sci(SCI_GETSTYLEAT, static_cast<uptr_t>(pos)));
}

} // namespace

class TestExternalLexers : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

    QString configXml() const { return _tmp.filePath("config/edita/plugins/config/EditaTestLexer.xml"); }

    std::unique_ptr<MainWindow> window()
    {
        auto w = std::make_unique<MainWindow>();
        w->setPrompter(std::make_unique<FakePrompter>());
        QCoreApplication::processEvents();
        return w;
    }

    BufferId open(MainWindow& w, const QString& name, const QByteArray& text)
    {
        const QString path = _tmp.filePath(name);
        writeFile(path, text);
        const BufferId id = w.openFile(path);
        w.editor()->sci(SCI_COLOURISE, 0, -1);
        return id;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        qputenv("EDITA_PLUGIN_PATH", EDITA_TEST_PLUGIN_DIR);
        QVERIFY2(QFileInfo::exists(EDITA_TEST_PLUGIN_DIR "/EditaTestLexer/EditaTestLexer.so"), "falta el plugin de prueba");
    }

    void init()
    {
        QDir(_tmp.filePath("config")).removeRecursively();
        writeFile(configXml(), readFile(EDITA_FIXTURES_DIR "/lexer-plugin/EditaTestLexer.xml"));
    }

    // El XML: sus <Languages> (extensiones, palabras clave, comentarios) y sus estilos
    void readsTheXml()
    {
        ExternalLexers lexers;
        const QStringList warnings = lexers.add(QStringLiteral("Prueba"), {"Fichas", "", "Fichas", "Otro"}, nullptr, configXml());
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
        QCOMPARE(lexers.languages().size(), std::size_t{2}); // sin el vacío ni el repetido
        const ExternalLanguage* fichas = lexers.find("Fichas");
        QVERIFY(fichas && fichas->hasStyles);
        QCOMPARE(fichas->definition.keywords[0], std::string("mover girar"));
        QCOMPARE(fichas->definition.commentLine, std::string("#"));
        QCOMPARE(fichas->styles.styles.size(), std::size_t{4});
        QVERIFY(!lexers.find("Otro")->hasStyles);
        // Las extensiones de <Language> y las del estilo
        QCOMPARE(lexers.languageForExtension("fic"), std::string("Fichas"));
        QCOMPARE(lexers.languageForExtension("FICHA"), std::string("Fichas"));
        QVERIFY(lexers.languageForExtension("txt").empty());
        // Hasta 30, como N++
        ExternalLexers many;
        std::vector<std::string> names;
        for (int i = 0; i < 35; ++i)
            names.push_back("L" + std::to_string(i));
        QCOMPARE(many.add(QStringLiteral("Muchos"), names, nullptr, QString()).size(), 1);
        QCOMPARE(many.languages().size(), std::size_t{kMaxExternalLanguages});
        // El XML se busca en la carpeta de configuración y, si no, en la del plugin
        QCOMPARE(findExternalLexerXml(QFileInfo(configXml()).absolutePath(), "/x/EditaTestLexer/EditaTestLexer.so"), configXml());
        QVERIFY(findExternalLexerXml(_tmp.filePath("nada"), "/x/EditaTestLexer/EditaTestLexer.so").isEmpty());
    }

    // Se abre un archivo por su extensión con el lexer del plugin, sus estilos y sus
    // palabras clave (las de <Language> y las del estilo)
    void colorsByExtension()
    {
        auto w = window();
        const BufferId id = open(*w, "programa.fic", "# nota\nmover 10\nsaltar x\n");
        const Buffer* buf = w->documents().buffer(id);
        QCOMPARE(buf->language(), LangType::External);
        QCOMPARE(buf->userLanguage(), QStringLiteral("Fichas"));
        EditorView& view = *w->editor();
        QCOMPARE(styleAt(view, 0), 1);  // # nota
        QCOMPARE(styleAt(view, 7), 2);  // mover (de <Language>)
        QCOMPARE(styleAt(view, 13), 3); // 10
        QCOMPARE(styleAt(view, 16), 2); // saltar (palabras del estilo)
        QCOMPARE(styleAt(view, 23), 0); // x
        QCOMPARE(view.sci(SCI_STYLEGETFORE, 2), sptr_t{0xFF0000}); // 0000FF del XML, en BGR
        QCOMPARE(view.sci(SCI_STYLEGETBOLD, 2), sptr_t{1});
        QCOMPARE(view.sci(SCI_STYLEGETITALIC, 1), sptr_t{1});
        // La barra de estado y el menú
        QCOMPARE(w->docStatusBar()->findChild<QToolButton*>(QStringLiteral("statusLanguage"))->text(), QStringLiteral("Fichas"));
        QAction* fichas = w->findChild<QAction*>(QStringLiteral("actionExternalLang:Fichas"));
        QVERIFY(fichas && fichas->isChecked());
        // La extensión del estilo (atributo ext del LexerType) también vale
        open(*w, "otra.ficha", "girar\n");
        QCOMPARE(w->documents().buffer(w->currentBuffer())->userLanguage(), QStringLiteral("Fichas"));
        QCOMPARE(styleAt(*w->editor(), 0), 2);
        // Comentar usa el comentario del XML
        w->editor()->sci(SCI_GOTOPOS, 0);
        w->findChild<QAction*>(QStringLiteral("actionToggleComment"))->trigger();
        QVERIFY(w->editor()->text().starts_with("#"));
    }

    // El menú Lenguaje: los del plugin, en orden y sin repetidos, antes del de usuario
    void languageMenuAndPluginMessages()
    {
        auto w = window();
        const QList<QAction*> external = w->findChildren<QAction*>(QRegularExpression(QStringLiteral("^actionExternalLang:")));
        QCOMPARE(external.size(), 2);
        QMenu* menu = w->findChild<QMenu*>(QStringLiteral("menuUserLanguages"));
        QVERIFY(menu);
        auto* languageMenu = qobject_cast<QMenu*>(menu->parent());
        const QList<QAction*> actions = languageMenu->actions();
        const qsizetype fichas = actions.indexOf(w->findChild<QAction*>(QStringLiteral("actionExternalLang:Fichas")));
        const qsizetype vacio = actions.indexOf(w->findChild<QAction*>(QStringLiteral("actionExternalLang:Vacío")));
        QVERIFY(fichas > 0 && vacio == fichas + 1 && vacio < actions.indexOf(menu->menuAction()));

        // Elegirlo a mano en un archivo de texto
        open(*w, "notas.txt", "mover 5\n");
        Buffer* buf = w->documents().buffer(w->currentBuffer());
        QCOMPARE(buf->language(), LangType::Text);
        actions[fichas]->trigger();
        QCOMPARE(buf->language(), LangType::External);
        QVERIFY(buf->isLanguageSetFromMenu());
        w->editor()->sci(SCI_COLOURISE, 0, -1);
        QCOMPARE(styleAt(*w->editor(), 0), 2);
        // Un lenguaje sin estilos en el XML: su lexer, con los estilos por defecto
        actions[vacio]->trigger();
        w->editor()->sci(SCI_COLOURISE, 0, -1);
        QCOMPARE(buf->userLanguage(), QStringLiteral("Vacío"));
        QCOMPARE(styleAt(*w->editor(), 0), 0);

        // Para los plugins, como en N++: L_EXTERNAL + el índice, y su nombre
        const intptr_t external0 = static_cast<intptr_t>(LangType::External);
        QCOMPARE(w->handlePluginMessage(NPPM_GETBUFFERLANGTYPE, static_cast<uintptr_t>(buf->id()), 0), external0 + 1);
        char name[64] = {};
        QCOMPARE(w->handlePluginMessage(NPPM_GETLANGUAGENAME, static_cast<uintptr_t>(external0), reinterpret_cast<intptr_t>(name)),
                 intptr_t{6});
        QCOMPARE(QString::fromUtf8(name), QStringLiteral("Fichas"));
        w->handlePluginMessage(NPPM_SETCURRENTLANGTYPE, 0, external0);
        QCOMPARE(buf->userLanguage(), QStringLiteral("Fichas"));
    }

    // El Configurador de estilos: los del plugin se guardan en su XML, como en N++
    void stylesAreSavedInThePluginXml()
    {
        auto w = window();
        StyleSheet styles = w->syntax().styles();
        auto it = std::find_if(styles.lexers().begin(), styles.lexers().end(), [](const LexerStyles& l) { return l.name == "Fichas"; });
        QVERIFY(it != styles.lexers().end());
        auto style = std::find_if(it->styles.begin(), it->styles.end(), [](const Style& s) { return s.id == 3; });
        style->fgColor = 0x0000FF; // rojo, en BGR
        QString error;
        QVERIFY2(w->syntax().saveStyles(styles, &error), qPrintable(error));
        const QByteArray xml = readFile(configXml());
        QVERIFY2(xml.contains("name=\"NUMBER\" styleID=\"3\" fgColor=\"FF0000\""), xml.constData());
        // stylers.xml no gana un LexerType que no tenía
        QVERIFY(!readFile(w->syntax().stylersPath()).contains("Fichas"));
        // Al volver a abrir, el color guardado
        w.reset();
        auto again = window();
        open(*again, "programa.fic", "12\n");
        QCOMPARE(again->editor()->sci(SCI_STYLEGETFORE, 3), sptr_t{0x0000FF});
        // Otro tema: los estilos del plugin siguen ahí (no son del tema)
        const auto themes = again->syntax().themes();
        QVERIFY(themes.size() > 1);
        QVERIFY(again->syntax().setTheme(themes[1].path));
        QVERIFY(again->syntax().styles().lexer("Fichas"));
    }

    // Sin XML el lexer funciona igual, con los estilos por defecto
    void withoutXml()
    {
        QFile::remove(configXml());
        auto w = window();
        open(*w, "notas.txt", "mover\n");
        w->findChild<QAction*>(QStringLiteral("actionExternalLang:Fichas"))->trigger();
        w->editor()->sci(SCI_COLOURISE, 0, -1);
        QCOMPARE(w->documents().buffer(w->currentBuffer())->language(), LangType::External);
        QCOMPARE(styleAt(*w->editor(), 0), 0); // sin palabras clave: ninguna instrucción
        // Y la extensión .fic ya no es suya (venía en el XML)
        open(*w, "programa.fic", "x\n");
        QCOMPARE(w->documents().buffer(w->currentBuffer())->language(), LangType::Text);
    }
};

QTEST_MAIN(TestExternalLexers)
#include "tst_externallexers.moc"
