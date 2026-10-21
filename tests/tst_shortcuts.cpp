// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 20: Shortcut Mapper de N++ y los atajos de shortcuts.xml.

#include "FakePrompter.h"
#include "core/Macro.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/ShortcutMapperDialog.h"
#include "ui/Shortcuts.h"

#include <QAction>
#include <QFile>
#include <QLineEdit>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>

using namespace edita;
using edita::test::FakePrompter;
using Kind = ShortcutEntry::Kind;
using namespace std::string_literals;

namespace {

QByteArray readAll(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

const ShortcutEntry* find(const std::vector<ShortcutEntry>& entries, Kind kind, const QString& id)
{
    for (const ShortcutEntry& e : entries) {
        if (e.kind == kind && e.id == id)
            return &e;
    }
    return nullptr;
}

const ShortcutEntry* findByName(const std::vector<ShortcutEntry>& entries, Kind kind, const QString& name)
{
    for (const ShortcutEntry& e : entries) {
        if (e.kind == kind && e.name == name)
            return &e;
    }
    return nullptr;
}

} // namespace

class TestShortcuts : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir _tmp;
    QString shortcutsFile() const { return _tmp.filePath(QStringLiteral("config/edita/shortcuts.xml")); }

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

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        qputenv("EDITA_PLUGIN_PATH", EDITA_PLUGIN_BUILD_DIR);
    }

    void cleanup()
    {
        QDir(_tmp.filePath("config")).removeRecursively();
    }

    void keyCombosLikeNotepadPlusPlus()
    {
        // Ctrl/Alt/Mayús + tecla virtual de Windows ↔ QKeySequence
        const KeyCombo ctrlShiftN{true, false, true, 'N'};
        QCOMPARE(ctrlShiftN.toKeySequence(), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
        QCOMPARE(KeyCombo::fromKeySequence(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N)), ctrlShiftN);
        QCOMPARE(KeyCombo::fromKeySequence(QKeySequence(Qt::ALT | Qt::Key_F4)), (KeyCombo{false, true, false, 0x73}));
        QCOMPARE(KeyCombo::fromKeySequence(QKeySequence(Qt::CTRL | Qt::Key_Slash)).key, 0xBF); // VK_OEM_2
        QVERIFY(KeyCombo::fromKeySequence(QKeySequence(Qt::CTRL | Qt::Key_MediaPlay)).isEmpty()); // sin equivalente
        // SCI_ASSIGNCMDKEY: SCK_* o carácter, con SCMOD_* arriba
        QCOMPARE(scintillaKeyDefinition({true, false, false, 'L'}), 'L' | (2 << 16));
        QCOMPARE(scintillaKeyDefinition({false, false, true, 0x28}), 300 | (1 << 16)); // Mayús+Abajo
        QCOMPARE(scintillaKeyDefinition({true, true, false, 0xBF}), '/' | (6 << 16));
        QCOMPARE(scintillaKeyDefinition({true, false, false, 0x99}), 0);
    }

    void shortcutsXmlKeepsTheFormatOfNotepadPlusPlus()
    {
        QDir().mkpath(QFileInfo(shortcutsFile()).absolutePath());
        // Con una macro ya guardada, que no se tiene que perder
        QVERIFY(writeMacros(shortcutsFile(), {defaultTrimAndSaveMacro()}));
        ShortcutsConfig config;
        config.internal[41001] = {true, true, false, 'N'};
        config.internal[41002] = {}; // sin atajo
        config.edita[QStringLiteral("actionExportPdf")] = {true, false, true, 'E'};
        config.plugins.push_back({QStringLiteral("mimeTools.so"), 2, {false, true, true, 'B'}});
        config.scintilla[2337] = {{true, false, false, 'K'}, {false, true, false, 0x28}};
        QVERIFY(writeShortcuts(shortcutsFile(), config));
        const QByteArray xml = readAll(shortcutsFile());
        QVERIFY(xml.contains(R"(<Shortcut id="41001" Ctrl="yes" Alt="yes" Shift="no" Key="78")"));
        QVERIFY(xml.contains(R"(<PluginCommand moduleName="mimeTools.so" internalID="2")"));
        QVERIFY(xml.contains(R"(<ScintKey ScintID="2337" menuCmdID="0" Ctrl="yes")"));
        QVERIFY(xml.contains("<NextKey"));
        QVERIFY(xml.contains("Trim Trailing Space and Save")); // la macro sigue
        ShortcutsConfig back;
        QVERIFY(readShortcuts(shortcutsFile(), back));
        QCOMPARE(back, config);
        std::vector<SavedMacro> macros;
        QVERIFY(readMacros(shortcutsFile(), macros));
        QCOMPARE(macros.size(), std::size_t{1});
    }

    void entriesListEveryKindOfCommand()
    {
        Window x;
        const std::vector<ShortcutEntry> entries = x.window.shortcutEntries();
        const ShortcutEntry* fileNew = find(entries, Kind::Menu, QStringLiteral("actionNew"));
        QVERIFY(fileNew);
        QCOMPARE(fileNew->name, QStringLiteral("New"));
        QCOMPARE(fileNew->category, QStringLiteral("File"));
        QCOMPARE(fileNew->keys, (std::vector<KeyCombo>{{true, false, false, 'N'}}));
        QCOMPARE(fileNew->keys, fileNew->defaults);
        QVERIFY(findByName(entries, Kind::Macros, QStringLiteral("Trim Trailing Space and Save")));
        const ShortcutEntry* demo = findByName(entries, Kind::Plugin, QStringLiteral("Insert date and time"));
        QVERIFY(demo);
        QCOMPARE(demo->id, QStringLiteral("EditaDemo.so:1"));
        QCOMPARE(demo->keys, (std::vector<KeyCombo>{{true, true, false, 'D'}})); // el que pone el plugin
        const ShortcutEntry* lineCut = findByName(entries, Kind::Scintilla, QStringLiteral("SCI_LINECUT"));
        QVERIFY(lineCut);
        QCOMPARE(lineCut->keys, (std::vector<KeyCombo>{{true, false, false, 'L'}}));
        // Las macros no salen también en el menú principal
        QVERIFY(!find(entries, Kind::Menu, QStringLiteral("actionMacro0")));
    }

    void menuShortcutsAreSavedAndApplied()
    {
        {
            Window x;
            QVERIFY(x.window.setShortcutKeys(Kind::Menu, QStringLiteral("actionNew"), {{true, true, false, 'N'}}));
            QCOMPARE(x.window.findChild<QAction*>(QStringLiteral("actionNew"))->shortcut(),
                     QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_N));
            // Una orden que N++ no tiene: por su nombre
            QVERIFY(x.window.setShortcutKeys(Kind::Menu, QStringLiteral("actionExportPdf"), {{true, false, true, 'E'}}));
        }
        const QByteArray xml = readAll(shortcutsFile());
        QVERIFY(xml.contains(R"(<Shortcut id="41001" Ctrl="yes" Alt="yes" Shift="no" Key="78")"));
        QVERIFY(xml.contains(R"(<Shortcut name="actionExportPdf")"));
        {
            Window y; // al volver a abrir
            QCOMPARE(y.window.findChild<QAction*>(QStringLiteral("actionNew"))->shortcut(),
                     QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_N));
            QCOMPARE(y.window.findChild<QAction*>(QStringLiteral("actionExportPdf"))->shortcut(),
                     QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
            // Volver al de serie lo quita de shortcuts.xml
            QVERIFY(y.window.setShortcutKeys(Kind::Menu, QStringLiteral("actionNew"), {{true, false, false, 'N'}}));
        }
        QVERIFY(!readAll(shortcutsFile()).contains(R"(id="41001")"));
    }

    void pluginAndMacroShortcuts()
    {
        {
            Window x;
            QVERIFY(x.window.setShortcutKeys(Kind::Plugin, QStringLiteral("EditaDemo.so:1"), {{true, true, false, 'Y'}}));
            QVERIFY(x.window.setShortcutKeys(Kind::Macros, QStringLiteral("0"), {{true, false, true, 'T'}}));
        }
        QVERIFY(readAll(shortcutsFile()).contains(R"(<PluginCommand moduleName="EditaDemo.so" internalID="1")"));
        Window y;
        const auto entries = y.window.shortcutEntries();
        QCOMPARE(find(entries, Kind::Plugin, QStringLiteral("EditaDemo.so:1"))->keys,
                 (std::vector<KeyCombo>{{true, true, false, 'Y'}}));
        QCOMPARE(find(entries, Kind::Macros, QStringLiteral("0"))->keys, (std::vector<KeyCombo>{{true, false, true, 'T'}}));
        QCOMPARE(y.window.findChild<QAction*>(QStringLiteral("actionMacro0"))->shortcut(),
                 QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    }

    void scintillaKeysAreReassigned()
    {
        Window x;
        EditorView* editor = x.window.editor();
        editor->sends(SCI_SETTEXT, 0, "uno\ndos\n");
        // SCI_LINECUT con Ctrl+Alt+K en lugar de Ctrl+L
        QVERIFY(x.window.setShortcutKeys(Kind::Scintilla, QString::number(SCI_LINECUT), {{true, true, false, 'K'}}));
        editor->setFocus();
        QTest::keyClick(editor, Qt::Key_L, Qt::ControlModifier);
        QCOMPARE(x.window.documents().text(x.window.currentBuffer()), "uno\ndos\n"s); // Ctrl+L ya no corta
        QTest::keyClick(editor, Qt::Key_K, Qt::ControlModifier | Qt::AltModifier);
        QCOMPARE(x.window.documents().text(x.window.currentBuffer()), "dos\n"s);
        QVERIFY(readAll(shortcutsFile()).contains(QByteArray("ScintID=\"") + QByteArray::number(SCI_LINECUT)));
        // De vuelta al de serie
        QVERIFY(x.window.setShortcutKeys(Kind::Scintilla, QString::number(SCI_LINECUT), {{true, false, false, 'L'}}));
        QTest::keyClick(editor, Qt::Key_L, Qt::ControlModifier);
        QCOMPARE(x.window.documents().text(x.window.currentBuffer()), ""s);
    }

    void mapperDialogResolvesConflicts()
    {
        Window x;
        ShortcutMapperDialog dialog(x.window, *x.prompter);
        QVERIFY(dialog.table(Kind::Menu)->rowCount() > 200);
        QVERIFY(dialog.table(Kind::Macros)->rowCount() >= 1);
        QVERIFY(dialog.table(Kind::Plugin)->rowCount() >= 5);
        QVERIFY(dialog.table(Kind::Scintilla)->rowCount() > 80);

        // Ctrl+O ya es de Abrir: si no se acepta, no cambia nada
        const KeyCombo ctrlO{true, false, false, 'O'};
        x.prompter->confirmations.enqueue(false);
        QVERIFY(!dialog.assign(Kind::Menu, QStringLiteral("actionNew"), ctrlO));
        QCOMPARE(x.prompter->asked, QStringList{"confirm:Shortcut conflict"});
        QCOMPARE(x.window.findChild<QAction*>(QStringLiteral("actionOpen"))->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_O));
        // Si se acepta, se le quita a Abrir
        x.prompter->confirmations.enqueue(true);
        QVERIFY(dialog.assign(Kind::Menu, QStringLiteral("actionNew"), ctrlO));
        QVERIFY(x.window.findChild<QAction*>(QStringLiteral("actionOpen"))->shortcut().isEmpty());
        QCOMPARE(x.window.findChild<QAction*>(QStringLiteral("actionNew"))->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_O));
        // Borrar
        QVERIFY(dialog.clear(Kind::Menu, QStringLiteral("actionNew")));
        QVERIFY(x.window.findChild<QAction*>(QStringLiteral("actionNew"))->shortcut().isEmpty());

        // Filtro por nombre
        dialog.setFilter(QStringLiteral("zoom"));
        QTableWidget* menu = dialog.table(Kind::Menu);
        int visible = 0;
        for (int row = 0; row < menu->rowCount(); ++row) {
            if (!menu->isRowHidden(row)) {
                ++visible;
                QVERIFY(menu->item(row, 0)->text().contains(QStringLiteral("Zoom"), Qt::CaseInsensitive));
            }
        }
        QVERIFY(visible >= 3);
        QVERIFY(x.window.findChild<QAction*>(QStringLiteral("actionShortcutMapper")));
    }
};

QTEST_MAIN(TestShortcuts)
#include "tst_shortcuts.moc"
