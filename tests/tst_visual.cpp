// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Mejoras visuales (no existen en N++): iconos por tipo de archivo, vista previa de
// colores, número de línea actual, guía del bloque actual, temas modernos y avisos sin
// ventanas modales.

#include "FakePrompter.h"
#include "core/ConfigXml.h"
#include "core/Paths.h"
#include "syntax/StyleSheet.h"
#include "syntax/SyntaxManager.h"
#include "ui/ColorPreview.h"
#include "ui/DocTabBar.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/FileIcons.h"
#include "ui/MainWindow.h"
#include "ui/NotificationBar.h"

#include <QDir>
#include <QFile>
#include <QLabel>
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

QImage image(const QIcon& icon)
{
    return icon.pixmap(32, 32).toImage();
}

} // namespace

class TestVisual : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    void fileTypeIcons()
    {
        QCOMPARE(fileicons::typeOf(QStringLiteral("main.cpp")).label, QStringLiteral("C++"));
        QCOMPARE(fileicons::typeOf(QStringLiteral("/a/b/script.PY")).label, QStringLiteral("PY"));
        QCOMPARE(fileicons::typeOf(QStringLiteral("CMakeLists.txt")).label, QStringLiteral("CM"));
        QCOMPARE(fileicons::typeOf(QStringLiteral("Makefile")).label, QStringLiteral("MK"));
        QCOMPARE(fileicons::typeOf(QStringLiteral("datos.desconocido")).label, QString());
        // Tipos distintos, iconos distintos; el estado se ve en el icono
        QVERIFY(image(fileicons::forFile(QStringLiteral("a.cpp"))) != image(fileicons::forFile(QStringLiteral("a.py"))));
        QVERIFY(image(fileicons::forFile(QStringLiteral("a.cpp"))) != image(fileicons::forFile(QStringLiteral("a.cpp"), fileicons::State::Dirty)));
        QVERIFY(image(fileicons::forFile(QStringLiteral("a.cpp"), fileicons::State::ReadOnly))
                != image(fileicons::forFile(QStringLiteral("a.cpp"), fileicons::State::Dirty)));

        // En las pestañas; desactivados, los de estado de siempre
        const QString path = _tmp.filePath("files/prueba.py");
        writeFile(path, "print(1)\n");
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        QVERIFY(w.openFile(path) != kInvalidBuffer);
        DocTabBar* tabs = w.findChild<EditorPane*>(QStringLiteral("mainPane"))->tabBar();
        const int index = tabs->indexOf(w.currentBuffer());
        QCOMPARE(image(tabs->tabIcon(index)), image(fileicons::forFile(path)));
        w.editor()->sends(SCI_APPENDTEXT, 1, "x");
        QCOMPARE(image(tabs->tabIcon(index)), image(fileicons::forFile(path, fileicons::State::Dirty)));
        Preferences p = w.preferences();
        p.fileTypeIcons = false;
        w.setPreferences(p);
        QCOMPARE(image(tabs->tabIcon(index)), image(DocTabBar::iconForState(DocTabBar::TabState::Dirty)));
        p.fileTypeIcons = true;
        w.setPreferences(p);
        QCOMPARE(image(tabs->tabIcon(index)), image(fileicons::forFile(path, fileicons::State::Dirty)));
    }

    void colorLiterals()
    {
        const QByteArray css = "a { color: #3fb950; background: rgba(255, 0, 0, 0.5); border: hsl(120, 100%, 50%); x: #ABC; "
                               "y: rgb(10% 20% 30% / 50%); }";
        const auto found = findColorLiterals(css);
        QCOMPARE(found.size(), size_t{5});
        QCOMPARE(found[0].color, QColor(0x3f, 0xb9, 0x50));
        QCOMPARE(css.mid(found[0].start, found[0].length), QByteArray("#3fb950"));
        QCOMPARE(found[1].color, QColor(255, 0, 0, 128));
        QCOMPARE(found[2].color.name(), QStringLiteral("#00ff00"));
        QCOMPARE(found[3].color, QColor(0xaa, 0xbb, 0xcc));
        QCOMPARE(found[4].color, QColor(26, 51, 77, 128));
        // Sin los cortos donde # es comentario; nada en #include, &#123; ni anclas con letras
        QCOMPARE(findColorLiterals(css, false).size(), size_t{4});
        QVERIFY(findColorLiterals("#include <x>\n#define Y\n&#123; #faded-text #12345").empty());
        // #RRGGBBAA: la transparencia al final, como en CSS
        QCOMPARE(findColorLiterals("#ff000080").front().color, QColor(255, 0, 0, 128));
        // El mismo formato al cambiar el color
        QCOMPARE(formatColorLike(QStringLiteral("#ABC"), QColor(255, 0, 0)), QStringLiteral("#F00"));
        QCOMPARE(formatColorLike(QStringLiteral("#abc"), QColor(0x12, 0x34, 0x56)), QStringLiteral("#123456"));
        QCOMPARE(formatColorLike(QStringLiteral("#3fb950"), QColor(1, 2, 3, 128)), QStringLiteral("#01020380"));
        QCOMPARE(formatColorLike(QStringLiteral("rgb(1, 2, 3)"), QColor(4, 5, 6)), QStringLiteral("rgb(4, 5, 6)"));
        QCOMPARE(formatColorLike(QStringLiteral("rgb(1, 2, 3)"), QColor(4, 5, 6, 128)), QStringLiteral("rgba(4, 5, 6, 0.5)"));
        QCOMPARE(formatColorLike(QStringLiteral("hsl(0, 0%, 0%)"), QColor(0, 255, 0)), QStringLiteral("hsl(120, 100%, 50%)"));
    }

    void colorPreviewInTheEditor()
    {
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        w.setLanguage(LangType::Css);
        w.editor()->sends(SCI_SETTEXT, 0, "body {\n  color: #3fb950;\n}\n");
        ColorPreview* preview = w.colorPreviewOfCurrentView();
        QVERIFY(preview);
        preview->updateNow();
        const sptr_t pos = w.editor()->sci(SCI_POSITIONFROMLINE, 1) + 9; // dentro de #3fb950
        const sptr_t value = w.editor()->sci(SCI_INDICATORVALUEAT, ColorPreview::kIndicator, pos);
        QCOMPARE(value & 0xFFFFFF, sptr_t{0x3f | (0xb9 << 8) | (0x50 << 16)});
        // Ctrl+clic: el selector (aquí, uno de prueba) reescribe el color en su formato
        preview->setColorPicker([](const QColor&) { return QColor(0x12, 0x34, 0x56); });
        QVERIFY(preview->pickColorAt(pos));
        QCOMPARE(w.documents().text(w.currentBuffer()), "body {\n  color: #123456;\n}\n"s);
        // En texto normal, nada
        w.setLanguage(LangType::Text);
        QCOMPARE(w.editor()->sci(SCI_INDICATORVALUEAT, ColorPreview::kIndicator, pos), sptr_t{0});
        // Desactivada en Preferencias
        w.setLanguage(LangType::Css);
        QVERIFY(w.editor()->sci(SCI_INDICATORVALUEAT, ColorPreview::kIndicator, pos) != 0);
        Preferences p = w.preferences();
        p.colorPreview = false;
        w.setPreferences(p);
        QCOMPARE(w.editor()->sci(SCI_INDICATORVALUEAT, ColorPreview::kIndicator, pos), sptr_t{0});
    }

    void currentLineNumberAndBlockGuide()
    {
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        // Se actualizan con SCN_UPDATEUI, que Scintilla manda al pintar
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        EditorView* v = w.editor();
        w.setLanguage(LangType::Python);
        v->sends(SCI_SETTEXT, 0, "def f():\n    a = 1\n    if a:\n        b = 2\n\n        c = 3\n    d = 4\n");
        auto marked = [v](int line) { return (v->sci(SCI_MARKERGET, static_cast<uptr_t>(line)) & (1 << EditorView::kCurrentLineMarker)) != 0; };
        // El número de la línea actual
        v->sci(SCI_GOTOLINE, 3);
        QTRY_VERIFY(marked(3));
        QVERIFY(!marked(0));
        v->sci(SCI_GOTOLINE, 6);
        QTRY_VERIFY(marked(6));
        QVERIFY(!marked(3));
        // La guía del bloque: en «b = 2» (y en la línea en blanco del bloque), la del «if»
        QVERIFY(v->sci(SCI_GETINDENTATIONGUIDES) != SC_IV_NONE);
        v->sci(SCI_GOTOLINE, 3);
        QTRY_COMPARE(v->sci(SCI_GETHIGHLIGHTGUIDE), sptr_t{4});
        v->sci(SCI_GOTOLINE, 4);
        QTest::qWait(50);
        QCOMPARE(v->sci(SCI_GETHIGHLIGHTGUIDE), sptr_t{4});
        // En el primer nivel no hay guía que resaltar
        v->sci(SCI_GOTOLINE, 1);
        QTRY_COMPARE(v->sci(SCI_GETHIGHLIGHTGUIDE), sptr_t{0});
        // Desactivados
        Preferences p = w.preferences();
        p.currentLineNumber = false;
        p.activeIndentGuide = false;
        w.setPreferences(p);
        v->sci(SCI_GOTOLINE, 3);
        QTest::qWait(50);
        QVERIFY(!marked(3));
        QCOMPARE(v->sci(SCI_GETHIGHLIGHTGUIDE), sptr_t{0});
    }

    void modernThemes()
    {
        QTemporaryDir config;
        SyntaxManager manager(config.path());
        manager.initialize();
        const QStringList expected = {"One Dark", "Dracula", "Catppuccin Mocha", "Catppuccin Latte", "Nord", "Tokyo Night"};
        for (const QString& name : expected) {
            const auto themes = manager.themes();
            const auto it = std::find_if(themes.begin(), themes.end(), [&](const SyntaxManager::Theme& t) { return t.name == name; });
            QVERIFY2(it != themes.end(), qPrintable(name));
            StyleSheet sheet;
            QVERIFY2(sheet.load(it->path), qPrintable(name));
            // Todos los lenguajes de N++, como los demás temas
            QVERIFY(sheet.lexers().size() > 80);
        }
        StyleSheet dracula;
        QVERIFY(dracula.load(QDir(paths::findDataFile(QStringLiteral("themes"))).filePath(QStringLiteral("Dracula.xml"))));
        QCOMPARE(dracula.globalById(STYLE_DEFAULT)->bgColor, 0x362A28); // 282A36 (BGR)
    }

    void notificationsDoNotBlock()
    {
        NotificationBar bar;
        bar.show(NotificationBar::Kind::Warning, QStringLiteral("Uno"), QStringLiteral("primer aviso"));
        bar.show(NotificationBar::Kind::Info, QStringLiteral("Dos"), QStringLiteral("segundo"));
        QVERIFY(bar.currentText().contains(QStringLiteral("primer aviso")));
        QCOMPARE(bar.pending(), 1);
        bar.dismiss();
        QVERIFY(bar.currentText().contains(QStringLiteral("segundo")));
        bar.dismiss();
        QVERIFY(bar.isHidden());
        // Muchas líneas: tres y «…y N más»
        bar.show(NotificationBar::Kind::Warning, QString(), QStringLiteral("1\n2\n3\n4\n5"));
        QVERIFY(bar.currentText().contains(QStringLiteral("2 more")));
        bar.dismiss();

        // Los avisos al arrancar (un tema que ya no existe) van a la barra, no a un cuadro
        writeFile(_tmp.filePath("config/edita/config.xml"),
                  "<?xml version=\"1.0\"?>\n<NotepadPlus><GUIConfigs><GUIConfig name=\"stylerTheme\" path=\"/nada/Perdido.xml\" />"
                  "</GUIConfigs></NotepadPlus>\n");
        MainWindow w;
        auto prompter = std::make_unique<FakePrompter>();
        FakePrompter* fake = prompter.get();
        w.setPrompter(std::move(prompter));
        QTRY_VERIFY(w.notificationBar()->currentText().contains(QStringLiteral("no longer exists")));
        QVERIFY(fake->warnings.isEmpty());
        QFile::remove(_tmp.filePath("config/edita/config.xml"));
    }
};

QTEST_MAIN(TestVisual)
#include "tst_visual.moc"
