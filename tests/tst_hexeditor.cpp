// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 89: el editor hexadecimal (plugins/EditaHex), escrito desde cero en Qt. Hace lo que el
// plugin HEX-Editor de Notepad++, sobre los bytes del archivo en el disco.

#include "FakePrompter.h"
#include "../plugins/EditaHex/HexBuffer.h"
#include "../plugins/EditaHex/HexView.h"
#include "ui/MainWindow.h"
#include "ui/PluginManager.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

using namespace edita;
using namespace edita::hex;
using edita::test::FakePrompter;

namespace {

QByteArray readFile(const QString& path)
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

void type(QWidget* w, const QString& text)
{
    for (const QChar c : text)
        QTest::keyClick(w, c.toLatin1());
}

} // namespace

class TestHexEditor : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        qputenv("EDITA_UPDATE_URL", "none");
        QVERIFY2(QFileInfo::exists(EDITA_PLUGIN_BUILD_DIR "/EditaHex/EditaHex.so"), "falta el plugin");
        qputenv("EDITA_PLUGIN_PATH", EDITA_PLUGIN_BUILD_DIR);
    }

    // --- HexBuffer -------------------------------------------------------------------------

    void loadSaveAndModified()
    {
        const QString path = _tmp.filePath(QStringLiteral("bin.dat"));
        const QByteArray original("\x00\x01\xff\x7f" "ABC", 7);
        writeFile(path, original);
        HexBuffer b;
        QVERIFY(b.load(path));
        QCOMPARE(b.data(), original);
        QVERIFY(!b.isModified());
        b.replace(1, QByteArray("\x10", 1));
        QVERIFY(b.isModified());
        QVERIFY(b.save(path));
        QVERIFY(!b.isModified());
        QCOMPARE(readFile(path), QByteArray("\x00\x10\xff\x7f" "ABC", 7));
        // Deshacer hasta lo guardado vuelve a «sin cambios»; más allá, cambiado otra vez
        b.insert(0, "Z");
        QVERIFY(b.isModified());
        b.undo();
        QVERIFY(!b.isModified());
        b.undo();
        QVERIFY(b.isModified());
        QCOMPARE(b.data(), original);
    }

    void editsAndUndo()
    {
        HexBuffer b;
        b.setData("hello");
        // Escribir seguido es un solo paso de deshacer
        b.replace(0, "J");
        b.replace(1, "E");
        b.replace(2, "L");
        QCOMPARE(b.data(), QByteArray("JELlo"));
        b.undo();
        QCOMPARE(b.data(), QByteArray("hello"));
        b.redo();
        QCOMPARE(b.data(), QByteArray("JELlo"));
        b.breakMerge();
        b.insert(5, "!");
        b.insert(6, "!");
        QCOMPARE(b.data(), QByteArray("JELlo!!"));
        b.undo();
        QCOMPARE(b.data(), QByteArray("JELlo"));
        // Sobrescribir al final hace crecer
        b.replace(5, "XY");
        QCOMPARE(b.data(), QByteArray("JELloXY"));
        b.breakMerge();
        b.remove(0, 3);
        QCOMPARE(b.data(), QByteArray("loXY"));
        b.undo();
        QCOMPARE(b.data(), QByteArray("JELloXY"));
        QVERIFY(b.canRedo());
        b.insert(0, "-"); // un cambio nuevo borra lo que se podía rehacer
        QVERIFY(!b.canRedo());
    }

    void nibbles()
    {
        HexBuffer b;
        b.setData(QByteArray("\x00\x00", 2));
        b.setNibble(0, true, 0xA);
        b.setNibble(0, false, 0xB);
        QCOMPARE(b.data(), QByteArray("\xab\x00", 2));
        b.undo(); // los dos dígitos del byte, un paso
        QCOMPARE(b.data(), QByteArray("\x00\x00", 2));
        b.setNibble(2, true, 0x4); // al final: crece
        QCOMPARE(b.size(), qsizetype{3});
        QCOMPARE(static_cast<unsigned char>(b.data()[2]), 0x40u);
    }

    void findWrapsAround()
    {
        HexBuffer b;
        b.setData("abcXYabcXY");
        QCOMPARE(b.find("XY", 0, true), std::optional<qsizetype>(3));
        QCOMPARE(b.find("XY", 4, true), std::optional<qsizetype>(8));
        QCOMPARE(b.find("XY", 9, true), std::optional<qsizetype>(3)); // da la vuelta
        QCOMPARE(b.find("XY", 8, false), std::optional<qsizetype>(3));
        QCOMPARE(b.find("XY", 0, false), std::optional<qsizetype>(8));
        QVERIFY(!b.find("zz", 0, true));
    }

    void hexTextAndValues()
    {
        QCOMPARE(parseHex(QStringLiteral("4F 6b,ff")), std::optional<QByteArray>(QByteArray("\x4f\x6b\xff", 3)));
        QVERIFY(!parseHex(QStringLiteral("4G")));
        QVERIFY(!parseHex(QStringLiteral("123")));
        QCOMPARE(toHex(QByteArray("\x01\xab", 2)), QStringLiteral("01 AB"));
        const QByteArray data("\x01\x02\x03\x04\x00\x00\x80\x3f", 8);
        const Values v = valuesAt(data, 0);
        QCOMPARE(*v.u8, quint64{1});
        QCOMPARE(*v.u16le, quint64{0x0201});
        QCOMPARE(*v.u16be, quint64{0x0102});
        QCOMPARE(*v.u32le, quint64{0x04030201});
        QCOMPARE(*valuesAt(data, 4).f32le, 1.0f);
        QVERIFY(!valuesAt(data, 6).i32le); // no caben 4 bytes
        QCOMPARE(valuesAt(QByteArray("\xc3\xb1", 2), 0).utf8, QStringLiteral("ñ"));
        QVERIFY(valuesAt(QByteArray("\xc3", 1), 0).utf8.isEmpty());
    }

    // --- HexView -----------------------------------------------------------------------------

    void typingInTheHexColumn()
    {
        HexBuffer b;
        b.setData(QByteArray("\x00\x00\x00", 3));
        HexView v;
        v.setBuffer(&b);
        v.resize(700, 300);
        v.show();
        v.setFocus();
        type(&v, QStringLiteral("a1b2"));
        QCOMPARE(b.data(), QByteArray("\xa1\xb2\x00", 3));
        QCOMPARE(v.position(), qsizetype{2});
        // Insertar (tecla Insert)
        QTest::keyClick(&v, Qt::Key_Insert);
        QVERIFY(!v.overwrite());
        type(&v, QStringLiteral("ff"));
        QCOMPARE(b.data(), QByteArray("\xa1\xb2\xff\x00", 4));
        // Retroceso y deshacer
        QTest::keyClick(&v, Qt::Key_Backspace);
        QCOMPARE(b.data(), QByteArray("\xa1\xb2\x00", 3));
        QTest::keyClick(&v, Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(b.data(), QByteArray("\xa1\xb2\xff\x00", 4));
        QTest::keyClick(&v, Qt::Key_Y, Qt::ControlModifier); // rehacer
        QCOMPARE(b.data(), QByteArray("\xa1\xb2\x00", 3));
        QTest::keyClick(&v, Qt::Key_Z, Qt::ControlModifier);
        // Lo que no es hexadecimal no escribe nada
        QTest::keyClick(&v, 'g');
        QCOMPARE(b.size(), qsizetype{4});
    }

    void typingInTheAsciiColumn()
    {
        HexBuffer b;
        b.setData("....");
        HexView v;
        v.setBuffer(&b);
        v.show();
        v.setFocus();
        QTest::keyClick(&v, Qt::Key_Tab); // a la columna de ASCII
        QCOMPARE(v.area(), HexView::Area::Ascii);
        type(&v, QStringLiteral("Hi"));
        QCOMPARE(b.data(), QByteArray("Hi.."));
    }

    void selectionCopyAndPaste()
    {
        HexBuffer b;
        b.setData("ABCDEF");
        HexView v;
        v.setBuffer(&b);
        v.show();
        v.setFocus();
        QTest::keyClick(&v, Qt::Key_Right, Qt::ShiftModifier);
        QTest::keyClick(&v, Qt::Key_Right, Qt::ShiftModifier);
        QCOMPARE(v.selectedBytes(), QByteArray("AB"));
        QTest::keyClick(&v, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("41 42"));
        // Pegar hexadecimal escrito como texto (sobrescribe desde el cursor)
        QApplication::clipboard()->setText(QStringLiteral("31 32 33"));
        v.setPosition(3);
        QTest::keyClick(&v, Qt::Key_V, Qt::ControlModifier);
        QCOMPARE(b.data(), QByteArray("ABC123"));
        // Seleccionar todo y borrar
        QTest::keyClick(&v, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClick(&v, Qt::Key_Delete);
        QCOMPARE(b.size(), qsizetype{0});
    }

    void clickPlacesTheCursor()
    {
        HexBuffer b;
        b.setData(QByteArray(64, 'x'));
        HexView v;
        v.setBuffer(&b);
        v.resize(800, 300);
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v));
        // El tercer byte de la segunda fila, en la columna de bytes
        const QFontMetrics fm(v.font());
        const int cw = fm.horizontalAdvance(QLatin1Char('0'));
        const int x = 6 + 10 * cw + 2 * 3 * cw + cw / 2;
        const int y = (fm.height() + 2) + 2;
        QTest::mouseClick(v.viewport(), Qt::LeftButton, {}, QPoint(x, y));
        QCOMPARE(v.position(), qsizetype{16 + 2});
        QCOMPARE(v.area(), HexView::Area::Hex);
    }

    // --- El plugin en Edita ------------------------------------------------------------------

    void openEditAndSaveTheCurrentFile()
    {
        const QString path = _tmp.filePath(QStringLiteral("doc.txt"));
        writeFile(path, "Hola\n");
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        w.notifyPluginsReady();
        const PluginManager::Plugin* plugin = nullptr;
        for (const auto& p : w.plugins()->plugins()) {
            if (p->name == QLatin1String("Hex Editor"))
                plugin = p.get();
        }
        QVERIFY(plugin);
        QVERIFY(w.openFile(path) != kInvalidBuffer);
        QVERIFY(w.plugins()->runCommand(plugin->commands.at(1).cmdId)); // Abrir el archivo actual
        // La vista del .so es otra clase que la HexView de la prueba: se usa por su nombre,
        // como la usaría alguien con el teclado
        auto* view = w.findChild<QAbstractScrollArea*>(QStringLiteral("hexView"));
        QVERIFY(view);
        auto* info = w.findChild<QLabel*>(QStringLiteral("hexInfo"));
        QVERIFY(info->text().contains(QLatin1String("doc.txt")));
        QVERIFY(info->text().contains(QLatin1String("5 bytes")));
        // H → h (0x48 → 0x68)
        view->setFocus();
        type(view, QStringLiteral("68"));
        QVERIFY(info->text().contains(QStringLiteral("•"))); // cambiado
        auto* save = w.findChild<QToolButton*>(QStringLiteral("hexSave"));
        QVERIFY(save->isEnabled());
        save->click();
        QCOMPARE(readFile(path), QByteArray("hola\n"));
        QVERIFY(!save->isEnabled());
        QVERIFY(!info->text().contains(QStringLiteral("•")));
        // Ir a una dirección y buscar texto
        auto* goTo = w.findChild<QLineEdit*>(QStringLiteral("hexGoTo"));
        goTo->setText(QStringLiteral("0x3"));
        QTest::keyClick(goTo, Qt::Key_Return);
        QVERIFY(info->text().contains(QLatin1String("0x3 (3)")));
        w.findChild<QComboBox*>(QStringLiteral("hexFindMode"))->setCurrentIndex(1);
        auto* find = w.findChild<QLineEdit*>(QStringLiteral("hexFind"));
        find->setText(QStringLiteral("ol"));
        QTest::keyClick(find, Qt::Key_Return);
        QVERIFY(info->text().contains(QLatin1String("2 bytes"))); // seleccionados
        // El inspector, con el valor del byte del cursor
        QVERIFY(w.findChild<QLabel*>(QStringLiteral("hexValues"))->text().contains(QLatin1String("uint8")));
    }

    // Fase 94: el panel de un plugin vuelve a donde lo dejó el usuario (se crea cuando el
    // plugin lo registra, después de restaurar los demás)
    void pluginPanelRemembersItsPlace()
    {
        auto panelCommand = [](MainWindow& w) {
            for (const auto& p : w.plugins()->plugins()) {
                if (p->name == QLatin1String("Hex Editor"))
                    return p->commands.at(0).cmdId;
            }
            return 0;
        };
        const QString name = QStringLiteral("pluginDock_EditaHex.so_0");
        {
            MainWindow w;
            w.setPrompter(std::make_unique<FakePrompter>());
            w.notifyPluginsReady();
            w.resize(1000, 700);
            w.show();
            QVERIFY(w.plugins()->runCommand(panelCommand(w)));
            auto* dock = w.findChild<QDockWidget*>(name);
            QVERIFY(dock);
            QCOMPARE(w.dockWidgetArea(dock), Qt::BottomDockWidgetArea); // donde lo pide el plugin
            w.addDockWidget(Qt::TopDockWidgetArea, dock);
            QVERIFY(w.close());
        }
        MainWindow w;
        w.setPrompter(std::make_unique<FakePrompter>());
        w.restoreWindowGeometry();
        w.notifyPluginsReady();
        w.show();
        QVERIFY(w.plugins()->runCommand(panelCommand(w)));
        auto* dock = w.findChild<QDockWidget*>(name);
        QVERIFY(dock && dock->isVisibleTo(&w));
        QCOMPARE(w.dockWidgetArea(dock), Qt::TopDockWidgetArea);
    }
};

QTEST_MAIN(TestHexEditor)
#include "tst_hexeditor.moc"
