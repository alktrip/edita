// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 16: internacionalización. El código está en inglés y translations/edita_es.ts
// trae el español completo.

#include "FakePrompter.h"
#include "core/ConfigXml.h"
#include "core/Preferences.h"
#include "ui/DocStatusBar.h"
#include "ui/FindReplaceDialog.h"
#include "ui/MainWindow.h"
#include "ui/PluginManager.h"
#include "ui/PluginsAdminDialog.h"
#include "ui/PreferencesDialog.h"
#include "ui/Translations.h"
#include "ui/UserLanguageDialog.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QGroupBox>
#include <QLabel>
#include <QLibraryInfo>
#include <QListWidget>
#include <QMenuBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QXmlStreamReader>

#include <map>

using namespace edita;
using edita::test::FakePrompter;

namespace {

struct Message
{
    QString context;
    QString source;
    QStringList translations; // una, o las formas de plural
    bool finished = true;
};

std::vector<Message> readTs(const QString& path)
{
    std::vector<Message> messages;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return messages;
    QXmlStreamReader xml(&file);
    QString context;
    Message current;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement())
            continue;
        if (xml.name() == QLatin1String("name"))
            context = xml.readElementText();
        else if (xml.name() == QLatin1String("message"))
            current = Message{context, {}, {}, true};
        else if (xml.name() == QLatin1String("source"))
            current.source = xml.readElementText();
        else if (xml.name() == QLatin1String("translation")) {
            current.finished = xml.attributes().value(QLatin1String("type")).isEmpty();
            // Sin plurales es texto; con plurales, <numerusform>…
            QString text;
            while (!(xml.isEndElement() && xml.name() == QLatin1String("translation"))) {
                xml.readNext();
                if (xml.isCharacters())
                    text += xml.text();
                else if (xml.isEndElement() && xml.name() == QLatin1String("numerusform")) {
                    current.translations << text;
                    text.clear();
                }
            }
            if (current.translations.isEmpty())
                current.translations << text;
            messages.push_back(current);
        }
    }
    return messages;
}

QStringList placeholders(const QString& text)
{
    QStringList found;
    static const QRegularExpression re(QStringLiteral("%L?[0-9n]"));
    for (auto it = re.globalMatch(text); it.hasNext();)
        found << it.next().captured().remove(QLatin1Char('L'));
    found.sort();
    return found;
}

int mnemonics(const QString& text)
{
    return static_cast<int>(QString(text).remove(QStringLiteral("&&")).count(QLatin1Char('&')));
}

// Los textos de una ventana que el usuario ve
QStringList visibleTexts(QWidget* root)
{
    QStringList texts;
    static const QRegularExpression tags(QStringLiteral("<[^>]+>"));
    for (QLabel* label : root->findChildren<QLabel*>())
        texts << label->text().remove(tags);
    for (QAbstractButton* button : root->findChildren<QAbstractButton*>())
        texts << button->text();
    for (QGroupBox* group : root->findChildren<QGroupBox*>())
        texts << group->title();
    for (QTabWidget* tabs : root->findChildren<QTabWidget*>()) {
        for (int i = 0; i < tabs->count(); ++i)
            texts << tabs->tabText(i);
    }
    for (QListWidget* list : root->findChildren<QListWidget*>()) {
        for (int i = 0; i < list->count(); ++i)
            texts << list->item(i)->text();
    }
    texts << root->windowTitle();
    texts.removeIf([](const QString& t) { return !t.contains(QRegularExpression(QStringLiteral("[A-Za-z]{2}"))); });
    return texts;
}

// Menús y acciones de la barra (sin los submenús de los plugins ni las listas de
// nombres: lenguajes, temas, codificaciones y archivos recientes)
QStringList menuTexts(QMenuBar* bar)
{
    QStringList texts;
    std::function<void(QMenu*)> walk = [&](QMenu* menu) {
        if (menu->title().contains(QRegularExpression(QStringLiteral("[A-Za-z]{2}"))))
            texts << menu->title(); // no las letras del menú Lenguaje
        for (QAction* action : menu->actions()) {
            if (action->menu()) {
                if (!action->menu()->objectName().startsWith(QLatin1String("plugin")))
                    walk(action->menu());
            } else if (action->objectName().startsWith(QLatin1String("action")) && !action->text().isEmpty()) {
                texts << action->text();
            }
        }
    };
    for (QAction* top : bar->actions()) {
        if (top->menu())
            walk(top->menu());
    }
    return texts;
}

} // namespace

class TestI18n : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir _tmp;
    std::vector<Message> _es;
    QSet<QString> _changedSources;       // los textos en inglés que el español cambia
    std::vector<QRegularExpression> _sourcePatterns; // los que llevan %1… (texto con .arg)
    QSet<QString> _changedSpanish;       // sus traducciones

    // ¿Es un texto en inglés que la traducción cambia? (con los %1 rellenos)
    bool needsTranslation(const QString& text) const
    {
        if (_changedSources.contains(text))
            return true;
        return std::any_of(_sourcePatterns.begin(), _sourcePatterns.end(),
                           [&](const QRegularExpression& re) { return re.match(text).hasMatch(); });
    }

    // Los textos que salen igual en inglés y en español aunque deberían cambiar
    QStringList untranslated(const QStringList& english, const QStringList& spanish) const
    {
        if (english.size() != spanish.size())
            return {QStringLiteral("distinto número de textos: %1 / %2").arg(english.size()).arg(spanish.size())};
        QStringList missing;
        for (qsizetype i = 0; i < english.size(); ++i) {
            if (english[i] == spanish[i] && needsTranslation(english[i]))
                missing << english[i];
        }
        return missing;
    }

    // Todos los textos de la interfaz en un idioma: menús y diálogos
    std::map<QString, QStringList> interfaceTexts(const QString& code)
    {
        i18n::install(code);
        std::map<QString, QStringList> texts;
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        texts["menús"] = menuTexts(w.menuBar());
        PreferencesDialog prefs(w.preferences(), {});
        texts["Preferencias"] = visibleTexts(&prefs);
        texts["Buscar"] = visibleTexts(w.findDialog());
        FakePrompter prompter;
        UserLanguageDialog udl(w.syntax().userLanguages(), prompter, &w);
        texts["Lenguajes del usuario"] = visibleTexts(&udl);
        PluginsAdminDialog admin(w.pluginsAdminContext(), prompter);
        texts["Administrador de plugins"] = visibleTexts(&admin);
        return texts;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        qputenv("EDITA_PLUGIN_PATH", EDITA_PLUGIN_BUILD_DIR);
        _es = readTs(QStringLiteral(EDITA_SOURCE_DIR "/translations/edita_es.ts"));
        QVERIFY(_es.size() > 800);
        for (const Message& m : _es) {
            if (m.translations.contains(m.source))
                continue; // igual en los dos idiomas ("Regex", "UTF-8"…)
            _changedSources.insert(m.source);
            for (const QString& t : m.translations)
                _changedSpanish.insert(t);
            if (m.source.contains(QLatin1Char('%'))) {
                QString pattern = QRegularExpression::escape(m.source);
                pattern.replace(QRegularExpression(QStringLiteral("%L?[0-9n]")), QStringLiteral(".*"));
                _sourcePatterns.emplace_back(QStringLiteral("^%1$").arg(pattern), QRegularExpression::DotMatchesEverythingOption);
            }
        }
    }

    void cleanup()
    {
        i18n::install(QStringLiteral("en"));
    }

    // --- Los archivos de traducción ---------------------------------------------------

    void translationFilesAreComplete_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<int>("minimum");
        QTest::newRow("Edita") << QStringLiteral(EDITA_SOURCE_DIR "/translations/edita_es.ts") << 800;
        QTest::newRow("plurales del inglés") << QStringLiteral(EDITA_SOURCE_DIR "/translations/edita_en.ts") << 15;
        QTest::newRow("Python Script") << QStringLiteral(EDITA_SOURCE_DIR "/plugins/PythonScript/translations/pythonscript_es.ts")
                                       << 10;
    }

    void translationFilesAreComplete()
    {
        QFETCH(QString, path);
        QFETCH(int, minimum);
        const std::vector<Message> messages = readTs(path);
        QVERIFY(static_cast<int>(messages.size()) >= minimum);
        for (const Message& m : messages) {
            const QString where = m.context + QStringLiteral(": ") + m.source;
            QVERIFY2(m.finished, qPrintable(QStringLiteral("sin terminar: ") + where));
            for (const QString& t : m.translations) {
                QVERIFY2(!t.isEmpty(), qPrintable(QStringLiteral("vacía: ") + where));
                // Los mismos %1, %2, %n (en otro orden si hace falta)
                QVERIFY2(placeholders(t) == placeholders(m.source), qPrintable(where + QStringLiteral(" → ") + t));
                // Un atajo de teclado (&) si el original lo tiene
                QVERIFY2(mnemonics(t) == mnemonics(m.source), qPrintable(where + QStringLiteral(" → ") + t));
            }
        }
    }

    // --- Elegir el idioma -----------------------------------------------------------

    void languageSelection()
    {
        const std::vector<i18n::Language> languages = i18n::availableLanguages();
        QCOMPARE(languages.size(), std::size_t{2});
        QCOMPARE(languages[0].code, QStringLiteral("en"));
        QCOMPARE(languages[0].nativeName, QStringLiteral("English"));
        QCOMPARE(languages[1].code, QStringLiteral("es"));
        QCOMPARE(languages[1].nativeName, QStringLiteral("Español"));

        // Sin preferencia, el del sistema; si no hay traducción, inglés
        QCOMPARE(i18n::resolveLanguage(QString(), {"es-MX", "en-US"}), QStringLiteral("es"));
        QCOMPARE(i18n::resolveLanguage(QString(), {"fr-FR", "es_ES"}), QStringLiteral("es"));
        QCOMPARE(i18n::resolveLanguage(QString(), {"fr-FR"}), QStringLiteral("en"));
        QCOMPARE(i18n::resolveLanguage(QStringLiteral("en"), {"es-MX"}), QStringLiteral("en"));
        QCOMPARE(i18n::resolveLanguage(QStringLiteral("de"), {"es-MX"}), QStringLiteral("es"));

        i18n::install(QStringLiteral("es"));
        QCOMPARE(i18n::currentLanguage(), QStringLiteral("es"));
        QCOMPARE(QLocale().language(), QLocale::Spanish); // los plugins leen el idioma de aquí
        QCOMPARE(QCoreApplication::translate("edita::MainWindow", "&File"), QStringLiteral("&Archivo"));
        i18n::install(QStringLiteral("en"));
        QCOMPARE(QCoreApplication::translate("edita::MainWindow", "&File"), QStringLiteral("&File"));
        QCOMPARE(QCoreApplication::translate("edita::SearchResultsPanel", "(%Ln hit(s))", nullptr, 1), QStringLiteral("(1 hit)"));
        QCOMPARE(QCoreApplication::translate("edita::SearchResultsPanel", "(%Ln hit(s))", nullptr, 3), QStringLiteral("(3 hits)"));
    }

    void preferencePersistsAndAsksToRestart()
    {
        // <GUIConfig name="EditaLanguage" value="es"/> y EditaQtFileDialog
        ConfigXml config;
        config.load(_tmp.filePath("prefs.xml"));
        Preferences prefs;
        prefs.language = QStringLiteral("es");
        prefs.qtFileDialog = true;
        storePreferences(config, prefs);
        QCOMPARE(config.guiConfig(QStringLiteral("EditaLanguage"), QStringLiteral("value")), QStringLiteral("es"));
        const Preferences loaded = loadPreferences(config);
        QCOMPARE(loaded.language, QStringLiteral("es"));
        QVERIFY(loaded.qtFileDialog);
        QVERIFY(loadPreferences(ConfigXml()).language.isEmpty()); // por defecto, el del sistema

        // En Preferencias > General: el idioma del sistema, English y Español
        MainWindow w;
        auto prompter = std::make_unique<FakePrompter>();
        FakePrompter* fake = prompter.get();
        w.setPrompter(std::move(prompter));
        PreferencesDialog dialog(w.preferences(), {});
        auto* combo = dialog.findChild<QComboBox*>(QStringLiteral("language"));
        QVERIFY(combo);
        QCOMPARE(combo->count(), 3);
        QCOMPARE(combo->itemText(2), QStringLiteral("Español"));
        combo->setCurrentIndex(2);
        dialog.findChild<QAbstractButton*>(QStringLiteral("qtFileDialog"))->setChecked(true);
        QCOMPARE(dialog.preferences().language, QStringLiteral("es"));

        // Cambiar el idioma ofrece reiniciar (no se acepta: no hay que reiniciar el test)
        fake->confirmations.enqueue(false);
        w.setPreferences(dialog.preferences());
        QCOMPARE(fake->asked, QStringList{"confirm:Localization"});
        QVERIFY(QCoreApplication::testAttribute(Qt::AA_DontUseNativeDialogs));
        Preferences back = w.preferences();
        back.qtFileDialog = false;
        w.setPreferences(back); // el idioma no cambia: no pregunta
        QCOMPARE(fake->asked.size(), 1);
        QVERIFY(!QCoreApplication::testAttribute(Qt::AA_DontUseNativeDialogs));
    }

    // --- La interfaz en cada idioma ------------------------------------------------

    void englishInterfaceHasNoSpanish()
    {
        i18n::install(QStringLiteral("en"));
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        QCOMPARE(w.windowTitle(), QStringLiteral("new 1 - Edita"));
        QStringList texts = menuTexts(w.menuBar());
        PreferencesDialog prefs(w.preferences(), {});
        texts << visibleTexts(&prefs) << visibleTexts(w.findDialog());
        FakePrompter prompter;
        UserLanguageDialog udl(w.syntax().userLanguages(), prompter, &w);
        texts << visibleTexts(&udl);
        QVERIFY(texts.size() > 300);
        QStringList spanish;
        for (const QString& text : texts) {
            if (_changedSpanish.contains(text) && !text.contains(QRegularExpression(QStringLiteral("^[A-Za-z &().:-]+$"))))
                spanish << text;
            else if (text.contains(QRegularExpression(QStringLiteral("[áéíóúñ¿¡]"))))
                spanish << text;
        }
        QVERIFY2(spanish.isEmpty(), qPrintable(spanish.join(QLatin1String(" | "))));
    }

    void spanishInterfaceIsComplete()
    {
        // Menús y diálogos, en inglés y en español: lo que el español cambia, cambia
        const std::map<QString, QStringList> english = interfaceTexts(QStringLiteral("en"));
        const std::map<QString, QStringList> spanish = interfaceTexts(QStringLiteral("es"));
        int compared = 0;
        for (const auto& [where, texts] : english) {
            const QStringList missing = untranslated(texts, spanish.at(where));
            QVERIFY2(missing.isEmpty(), qPrintable(where + QStringLiteral(": ") + missing.join(QLatin1String(" | "))));
            compared += static_cast<int>(texts.size());
        }
        QVERIFY(compared > 500);

        i18n::install(QStringLiteral("es"));
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        QCOMPARE(w.windowTitle(), QStringLiteral("nuevo 1 - Edita"));
        QCOMPARE(w.menuBar()->actions().first()->text(), QStringLiteral("&Archivo"));
        QVERIFY(w.docStatusBar()->positionText().startsWith(QStringLiteral("Lín: 1")));
        QCOMPARE(w.docStatusBar()->languageText(), QStringLiteral("Archivo de texto normal"));

        // Los plurales del español
        QCOMPARE(QCoreApplication::translate("edita::SearchResultsPanel", "(%Ln hit(s))", nullptr, 1),
                 QStringLiteral("(1 coincidencia)"));
        QCOMPARE(QCoreApplication::translate("edita::SearchResultsPanel", "(%Ln hit(s))", nullptr, 4),
                 QStringLiteral("(4 coincidencias)"));

        // Los textos estándar de Qt (botones de los diálogos), si están instalados
        if (!QFile::exists(QLibraryInfo::path(QLibraryInfo::TranslationsPath) + QStringLiteral("/qtbase_es.qm")))
            QSKIP("no están las traducciones de Qt (qtbase_es.qm)");
        QDialogButtonBox box(QDialogButtonBox::Cancel);
        QCOMPARE(box.button(QDialogButtonBox::Cancel)->text().remove(QLatin1Char('&')), QStringLiteral("Cancelar"));
    }

    void pluginsGetTheLanguage()
    {
        // NPPM_GETNATIVELANGFILENAME como en N++: "spanish.xml"; el plugin de ejemplo lo usa
        for (const QString& code : {QStringLiteral("es"), QStringLiteral("en")}) {
            i18n::install(code);
            MainWindow w;
            w.setPrompter(std::make_unique<FakePrompter>());
            const PluginManager::Plugin* demo = nullptr;
            for (const auto& p : w.plugins()->plugins()) {
                if (p->name == QLatin1String("Demo"))
                    demo = p.get();
            }
            QVERIFY(demo);
            QCOMPARE(demo->commands[0].name, code == QLatin1String("es") ? QStringLiteral("Hola, Edita") : QStringLiteral("Hello, Edita"));
        }
    }
};

QTEST_MAIN(TestI18n)
#include "tst_i18n.moc"
