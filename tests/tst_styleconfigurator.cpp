// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 19: Configurador de estilos (WordStyleDlg de N++) y guardar los temas.

#include "FakePrompter.h"
#include "core/Paths.h"
#include "syntax/StyleSheet.h"
#include "syntax/SyntaxManager.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/StyleConfiguratorDialog.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QFile>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QTest>

#include <SciLexer.h>

using namespace edita;
using edita::test::FakePrompter;

namespace {

QByteArray readAll(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

int lexerIndex(const StyleSheet& styles, const char* name)
{
    for (std::size_t i = 0; i < styles.lexers().size(); ++i) {
        if (styles.lexers()[i].name == name)
            return static_cast<int>(i);
    }
    return -1;
}

constexpr int kRed = 0x0000FF;   // 0x00BBGGRR
constexpr int kGreen = 0x00FF00;

} // namespace

class TestStyleConfigurator : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir _tmp;
    QString config() const { return _tmp.filePath(QStringLiteral("config/edita")); }

    struct Window
    {
        MainWindow window;
        FakePrompter* prompter = nullptr;
        Window()
        {
            auto fake = std::make_unique<FakePrompter>();
            prompter = fake.get();
            window.setPrompter(std::move(fake));
        }
    };

    QString cppFile()
    {
        const QString path = _tmp.filePath(QStringLiteral("prueba.cpp"));
        QFile f(path);
        if (f.open(QIODevice::WriteOnly))
            f.write("int main() { return 0; }\n");
        return path;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    void cleanup()
    {
        QDir(_tmp.filePath("config")).removeRecursively();
    }

    void styleSheetSavesOnlyWhatChanged()
    {
        // writeStyles: colores, fuente, extensiones y palabras del usuario; el resto igual
        const QString path = _tmp.filePath("estilos.xml");
        QFile::remove(path);
        QVERIFY(QFile::copy(paths::findDataFile(QStringLiteral("stylers.model.xml")), path));
        QFile::setPermissions(path, QFile::permissions(path) | QFileDevice::WriteOwner);
        StyleSheet styles;
        QVERIFY(styles.load(path));
        const int cpp = lexerIndex(styles, "cpp");
        QVERIFY(cpp >= 0);
        LexerStyles& lexer = styles.lexers()[static_cast<std::size_t>(cpp)];
        Style* word = nullptr;
        for (Style& s : lexer.styles) {
            if (s.name == "INSTRUCTION WORD")
                word = &s;
        }
        QVERIFY(word);
        word->fgColor = kRed;
        word->fontStyle = kFontStyleItalic;
        word->fontSize = 12;
        word->keywords = "mipalabra otra";
        lexer.userExtensions = "cppm ixx";
        for (Style& s : styles.globalStyles()) {
            if (s.name == "Default Style")
                s.bgColor = kGreen;
        }
        const QByteArray before = readAll(path);
        QVERIFY(styles.save(path));
        const QByteArray after = readAll(path);
        QVERIFY(after.contains("<!--")); // los comentarios del archivo siguen
        QCOMPARE(after.count("<LexerType "), before.count("<LexerType "));
        QCOMPARE(after.count("<WordsStyle "), before.count("<WordsStyle "));

        StyleSheet again;
        QVERIFY(again.load(path));
        const LexerStyles* l = again.lexer("cpp");
        const Style* w = l->findByName("INSTRUCTION WORD");
        QCOMPARE(w->fgColor, kRed);
        QCOMPARE(w->fontStyle, kFontStyleItalic);
        QCOMPARE(w->fontSize, 12);
        QCOMPARE(w->keywords, std::string("mipalabra otra"));
        QCOMPARE(l->userExtensions, std::string("cppm ixx"));
        QCOMPARE(again.globalByName("Default Style")->bgColor, kGreen);
        // Lo que no se tocó queda igual
        QCOMPARE(l->findByName("COMMENT")->fgColor, styles.lexer("cpp")->findByName("COMMENT")->fgColor);
        QCOMPARE(again.langFromUserExtension("ixx"), LangType::Cpp);
    }

    void changesPreviewLiveAndCancelRestores()
    {
        Window x;
        x.window.openFile(cppFile());
        EditorView* editor = x.window.editor();
        const sptr_t original = editor->send(SCI_STYLEGETFORE, SCE_C_WORD);
        StyleConfiguratorDialog dialog(x.window.syntax(), *x.prompter, [&](const QString& p) { return x.window.setTheme(p); });
        QVERIFY(dialog.selectLanguage(lexerIndex(dialog.workingStyles(), "cpp")));
        QVERIFY(dialog.selectStyle(QStringLiteral("INSTRUCTION WORD")));
        QCOMPARE(dialog.currentStyle()->id, SCE_C_WORD);
        dialog.setStyleColor(true, QColor(255, 0, 0));
        // Se ve al momento en el editor
        QCOMPARE(editor->send(SCI_STYLEGETFORE, SCE_C_WORD), sptr_t{kRed});
        dialog.findChild<QCheckBox*>(QStringLiteral("underline"))->setChecked(true);
        QCOMPARE(editor->send(SCI_STYLEGETUNDERLINE, SCE_C_WORD), sptr_t{1});
        QVERIFY(dialog.isDirty());
        // Cancelar: vuelve lo de antes, y no se escribió nada
        const QByteArray file = readAll(QDir(config()).filePath("stylers.xml"));
        dialog.reject();
        QCOMPARE(editor->send(SCI_STYLEGETFORE, SCE_C_WORD), original);
        QCOMPARE(editor->send(SCI_STYLEGETUNDERLINE, SCE_C_WORD), sptr_t{0});
        QCOMPARE(readAll(QDir(config()).filePath("stylers.xml")), file);
    }

    void saveWritesTheStylesFile()
    {
        {
            Window x;
            StyleConfiguratorDialog dialog(x.window.syntax(), *x.prompter, {});
            QVERIFY(dialog.selectLanguage(lexerIndex(dialog.workingStyles(), "cpp")));
            QVERIFY(dialog.selectStyle(QStringLiteral("INSTRUCTION WORD")));
            dialog.setStyleColor(true, QColor(0, 255, 0));
            auto* userExt = dialog.findChild<QLineEdit*>(QStringLiteral("userExtensions"));
            userExt->setText(QStringLiteral("edc"));
            emit userExt->textEdited(userExt->text());
            dialog.findChild<QPlainTextEdit*>(QStringLiteral("userKeywords"))->setPlainText(QStringLiteral("palabraNueva"));
            // Global override: forzar el color de fondo
            QVERIFY(dialog.selectLanguage(-1));
            QVERIFY(dialog.selectStyle(QStringLiteral("Global override")));
            dialog.findChild<QCheckBox*>(QStringLiteral("override1"))->setChecked(true);
            QVERIFY(dialog.saveAndClose());
        }
        // stylers.xml y config.xml guardados: una ventana nueva los usa
        Window y;
        const LexerStyles* cpp = y.window.syntax().styles().lexer("cpp");
        QCOMPARE(cpp->findByName("INSTRUCTION WORD")->fgColor, kGreen);
        QCOMPARE(cpp->findByName("INSTRUCTION WORD")->keywords, std::string("palabraNueva"));
        QCOMPARE(cpp->userExtensions, std::string("edc"));
        QVERIFY(y.window.syntax().styles().globalOverride().background);
        QVERIFY(!y.window.syntax().styles().globalOverride().foreground);
        QVERIFY(readAll(QDir(config()).filePath("config.xml")).contains("name=\"globalOverride\""));
        // La extensión del usuario: un .edc es C++
        const QString edc = _tmp.filePath("codigo.edc");
        QFile f(edc);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        const BufferId id = y.window.openFile(edc);
        QCOMPARE(y.window.documents().buffer(id)->language(), LangType::Cpp);
    }

    void applicationThemesAreCopiedBeforeSaving()
    {
        Window x;
        QString monokai;
        for (const auto& theme : x.window.syntax().themes()) {
            if (theme.name == QLatin1String("Monokai"))
                monokai = theme.path;
        }
        QVERIFY(!monokai.isEmpty());
        const QByteArray shipped = QCryptographicHash::hash(readAll(monokai), QCryptographicHash::Sha256);
        StyleConfiguratorDialog dialog(x.window.syntax(), *x.prompter, [&](const QString& p) { return x.window.setTheme(p); });
        auto* themes = dialog.findChild<QComboBox*>(QStringLiteral("themes"));
        themes->setCurrentIndex(themes->findText(QStringLiteral("Monokai")));
        QCOMPARE(x.window.syntax().activeThemePath(), monokai);
        dialog.selectLanguage(-1);
        dialog.selectStyle(QStringLiteral("Default Style"));
        dialog.setStyleColor(false, QColor(0, 0, 255));

        // Cambiar de tema con cambios sin guardar avisa (aquí: no guardar)
        x.prompter->confirmations.enqueue(false);
        themes->setCurrentIndex(themes->findText(QStringLiteral("Zenburn")));
        QCOMPARE(x.prompter->asked.size(), 1);
        QVERIFY(!dialog.isDirty());
        themes->setCurrentIndex(themes->findText(QStringLiteral("Monokai")));
        dialog.selectLanguage(-1);
        dialog.selectStyle(QStringLiteral("Default Style"));
        dialog.setStyleColor(false, QColor(0, 0, 255));
        QVERIFY(dialog.saveAndClose());

        // Se guardó una copia en ~/.config/edita/themes, que pasa a ser el tema en uso
        const QString copy = QDir(config()).filePath(QStringLiteral("themes/Monokai.xml"));
        QVERIFY(QFileInfo::exists(copy));
        QCOMPARE(x.window.syntax().activeThemePath(), copy);
        StyleSheet saved;
        QVERIFY(saved.load(copy));
        QCOMPARE(saved.globalByName("Default Style")->bgColor, 0xFF0000);
        // El de la aplicación no se tocó
        QCOMPARE(QCryptographicHash::hash(readAll(monokai), QCryptographicHash::Sha256), shipped);
    }

    void menuOpensIt()
    {
        Window x;
        QAction* action = x.window.findChild<QAction*>(QStringLiteral("actionStyleConfigurator"));
        QVERIFY(action);
        QVERIFY(!action->icon().isNull());
    }
};

QTEST_MAIN(TestStyleConfigurator)
#include "tst_styleconfigurator.moc"
