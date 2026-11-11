// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 69: Editar › Sobre la selección (abrir la ruta, tapar, buscar en internet), el panel de
// caracteres, el historial del portapapeles y Archivo › Renombrar, Mover a la papelera, Abrir
// con la aplicación predeterminada y Abrir la carpeta contenedora

#include "FakePrompter.h"
#include "core/OnSelection.h"
#include "core/Preferences.h"
#include "doc/Buffer.h"
#include "ui/EditPanels.h"
#include "ui/EditorPane.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/PreferencesDialog.h"

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QGuiApplication>
#include <QLineEdit>
#include <QListWidget>
#include <QRadioButton>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <QUrlQuery>

using namespace edita;
using edita::test::FakePrompter;

class TestOnSelection : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;
    FakePrompter* _prompter = nullptr;
    QList<QUrl> _opened;

    std::unique_ptr<MainWindow> window()
    {
        auto w = std::make_unique<MainWindow>();
        auto prompter = std::make_unique<FakePrompter>();
        _prompter = prompter.get();
        w->setPrompter(std::move(prompter));
        _opened.clear();
        w->setUrlOpener([this](const QUrl& url) {
            _opened << url;
            return true;
        });
        QCoreApplication::processEvents();
        return w;
    }

    QString file(const QString& name, const QByteArray& text)
    {
        const QString path = _tmp.filePath(name);
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            return {};
        f.write(text);
        return path;
    }

    static QAction* action(MainWindow& w, const char* name) { return w.findChild<QAction*>(QString::fromLatin1(name)); }
    static QString text(MainWindow& w) { return QString::fromStdString(w.editor()->text()); }
    static void setText(MainWindow& w, const char* text) { w.editor()->sends(SCI_SETTEXT, 0, text); }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
    }

    // Las direcciones de N++ para cada buscador, con el texto codificado
    void searchEngines()
    {
        QCOMPARE(searchEngineTemplate(SearchEngine::Google, {}), QStringLiteral("https://www.google.com/search?q=$(CURRENT_WORD)"));
        QCOMPARE(searchEngineTemplate(SearchEngine::DuckDuckGo, {}), QStringLiteral("https://duckduckgo.com/?q=$(CURRENT_WORD)"));
        QCOMPARE(searchEngineTemplate(SearchEngine::Bing, {}), searchEngineTemplate(SearchEngine::DuckDuckGo, {}));
        QCOMPARE(searchEngineTemplate(SearchEngine::Yahoo, {}), QStringLiteral("https://search.yahoo.com/search?q=$(CURRENT_WORD)"));
        QCOMPARE(searchEngineTemplate(SearchEngine::StackOverflow, {}),
                 QStringLiteral("https://stackoverflow.com/search?q=$(CURRENT_WORD)"));
        // La personalizada: sin espacios; si no es http(s), Google
        QCOMPARE(searchEngineTemplate(SearchEngine::Custom, QStringLiteral(" https://example.org/s?q= $(CURRENT_WORD) ")),
                 QStringLiteral("https://example.org/s?q=$(CURRENT_WORD)"));
        QCOMPARE(searchEngineTemplate(SearchEngine::Custom, QStringLiteral("example.org")),
                 searchEngineTemplate(SearchEngine::Google, {}));
        QCOMPARE(searchEngineTemplate(SearchEngine::Custom, {}), searchEngineTemplate(SearchEngine::Google, {}));

        const QUrl url = searchOnInternetUrl(SearchEngine::DuckDuckGo, {}, QStringLiteral("año & C++ #1"));
        QCOMPARE(url.host(), QStringLiteral("duckduckgo.com"));
        QCOMPARE(QUrlQuery(url).queryItemValue(QStringLiteral("q"), QUrl::FullyDecoded), QStringLiteral("año & C++ #1"));
        QCOMPARE(url.fragment(), QString()); // el # no corta la dirección
    }

    // NPPM_GETFILENAMEATCURSOR: lo que parece una ruta alrededor del cursor
    void fileNameAtCaret()
    {
        const QString line = QStringLiteral("#include \"src/main.cpp\" // ver (docs/a.md:12)");
        QCOMPARE(fileNameAtColumn(line, 15), QStringLiteral("src/main.cpp"));
        QCOMPARE(fileNameAtColumn(line, 34), QStringLiteral("docs/a.md")); // acaba en «:»
        QCOMPARE(fileNameAtColumn(QStringLiteral("abrir /etc/hosts"), 16), QStringLiteral("/etc/hosts")); // al final
        QCOMPARE(fileNameAtColumn(QStringLiteral("<a.txt>"), 3), QStringLiteral("a.txt"));
        QCOMPARE(fileNameAtColumn(QString(), 0), QString());
    }

    // Las variables de entorno, ~, file:// y las rutas relativas a la carpeta del documento
    void resolvePaths()
    {
        qputenv("EDITA_TEST_DIR", _tmp.path().toUtf8());
        QCOMPARE(resolveSelectedPath(QStringLiteral("$EDITA_TEST_DIR/a.txt"), {}), _tmp.filePath("a.txt"));
        QCOMPARE(resolveSelectedPath(QStringLiteral("${EDITA_TEST_DIR}/b"), {}), _tmp.filePath("b"));
        QCOMPARE(resolveSelectedPath(QStringLiteral("$NO_EXISTE_EDITA/x"), {}), QStringLiteral("$NO_EXISTE_EDITA/x"));
        QCOMPARE(resolveSelectedPath(QStringLiteral("~/x.txt"), {}), QDir::homePath() + QStringLiteral("/x.txt"));
        QCOMPARE(resolveSelectedPath(QStringLiteral("  file:///etc/hosts "), {}), QStringLiteral("/etc/hosts"));
        QCOMPARE(resolveSelectedPath(QStringLiteral("sub/../c.txt"), QStringLiteral("/proyecto")), QStringLiteral("/proyecto/c.txt"));
        QCOMPARE(resolveSelectedPath(QString(), QStringLiteral("/proyecto")), QString());
    }

    // IDM_EDIT_REDACT_SELECTION: █ o ●, salvo los fines de línea; un carácter es un solo símbolo
    void redact()
    {
        QCOMPARE(redactText(QStringLiteral("año\r\n😀x"), false), QStringLiteral("███\r\n██"));
        QCOMPARE(redactText(QStringLiteral("ab"), true), QStringLiteral("●●"));

        auto w = window();
        setText(*w, "clave: secreta\nusuario: ana");
        EditorView* v = w->editor();
        // Dos selecciones: las dos se tapan, aunque la primera cambie de tamaño en bytes
        v->sci(SCI_SETSELECTION, 14, 7);
        v->sci(SCI_ADDSELECTION, 27, 24);
        action(*w, "actionRedactSelection")->trigger();
        QCOMPARE(text(*w), QStringLiteral("clave: ███████\nusuario: ███"));
        v->sci(SCI_UNDO); // un solo paso
        QCOMPARE(text(*w), QStringLiteral("clave: secreta\nusuario: ana"));
        v->sci(SCI_SETSEL, 0, 5);
        w->redactSelection(true);
        QCOMPARE(text(*w), QStringLiteral("●●●●●: secreta\nusuario: ana"));
    }

    // IDM_EDIT_SEARCHONINTERNET con el buscador de las preferencias
    void searchOnInternet()
    {
        auto w = window();
        setText(*w, "qt signals");
        w->editor()->sci(SCI_SETSEL, 0, 10);
        action(*w, "actionSearchOnInternet")->trigger();
        QCOMPARE(_opened.size(), 1);
        QCOMPARE(_opened.front().host(), QStringLiteral("www.google.com")); // el de serie
        QCOMPARE(QUrlQuery(_opened.front()).queryItemValue(QStringLiteral("q"), QUrl::FullyDecoded), QStringLiteral("qt signals"));

        Preferences p = w->preferences();
        p.searchEngineChoice = static_cast<int>(SearchEngine::Custom);
        p.searchEngineCustom = QStringLiteral("https://example.org/buscar?texto=$(CURRENT_WORD)");
        w->setPreferences(p);
        // Sin selección: la palabra del cursor
        w->editor()->sci(SCI_SETSEL, 1, 1);
        QVERIFY(w->searchOnInternet());
        QCOMPARE(_opened.back().toString(), QStringLiteral("https://example.org/buscar?texto=qt"));
        // Con varias selecciones no hace nada (como N++)
        w->editor()->sci(SCI_SETSELECTION, 0, 2);
        w->editor()->sci(SCI_ADDSELECTION, 3, 5);
        QVERIFY(!w->searchOnInternet());
        QCOMPARE(_opened.size(), 2);
    }

    // La página «Search Engine» de Preferencias y su orden en el menú
    void searchEnginePreferences()
    {
        Preferences p;
        p.searchEngineChoice = static_cast<int>(SearchEngine::Bing); // ya no está: DuckDuckGo
        PreferencesDialog dialog(p, {{0, QStringLiteral("Normal text")}});
        auto* custom = dialog.findChild<QLineEdit*>(QStringLiteral("searchEngineCustom"));
        QVERIFY(custom);
        QVERIFY(dialog.findChild<QRadioButton*>(QStringLiteral("searchEngine1"))->isChecked());
        QVERIFY(!custom->isEnabled());
        dialog.findChild<QRadioButton*>(QStringLiteral("searchEngine0"))->setChecked(true);
        QVERIFY(custom->isEnabled());
        custom->setText(QStringLiteral(" https://example.org/?q=$(CURRENT_WORD) "));
        QCOMPARE(dialog.preferences().searchEngineChoice, 0);
        QCOMPARE(dialog.preferences().searchEngineCustom, QStringLiteral("https://example.org/?q=$(CURRENT_WORD)"));
        dialog.showPage(QStringLiteral("searchEngine"));
        QVERIFY(dialog.pageNames().contains(QStringLiteral("searchEngine")));

        auto w = window();
        QVERIFY(action(*w, "actionChangeSearchEngine"));
    }

    // Abrir la ruta seleccionada (o junto al cursor) en Edita, o su carpeta
    void openSelectedPath()
    {
        const QString target = file(QStringLiteral("proyecto/docs/leeme.txt"), "hola");
        const QString source = file(QStringLiteral("proyecto/main.c"), "#include \"docs/leeme.txt\"\n");
        auto w = window();
        QVERIFY(w->openFile(source) != kInvalidBuffer);
        w->editor()->sci(SCI_GOTOPOS, 14); // dentro de la ruta, sin selección: relativa al documento
        QCOMPARE(w->selectedPath(), target);
        action(*w, "actionOpenSelectedFile")->trigger();
        const Buffer* buf = w->documents().buffer(w->currentBuffer());
        QVERIFY(buf);
        QCOMPARE(buf->filePath(), target);

        QVERIFY(w->openFile(source) != kInvalidBuffer);
        w->editor()->sci(SCI_GOTOPOS, 14);
        QVERIFY(w->openSelectedPath(true));
        QCOMPARE(_opened.back(), QUrl::fromLocalFile(QFileInfo(target).absolutePath()));

        // No existe: aviso
        setText(*w, "no/existe.txt");
        w->editor()->sci(SCI_SELECTALL);
        QVERIFY(!w->openSelectedPath(false));
        QCOMPARE(_prompter->warnings.size(), 1);
        QVERIFY(_prompter->warnings.front().startsWith(QStringLiteral("Open Path:")));
    }

    // Archivo › Renombrar: el archivo en disco, sin cerrar el documento
    void renameFile()
    {
        const QString path = file(QStringLiteral("viejo.txt"), "texto");
        auto w = window();
        const BufferId id = w->openFile(path);
        QVERIFY(id != kInvalidBuffer);
        const QString renamed = _tmp.filePath("nuevo.py");
        _prompter->saveNames.enqueue(renamed);
        action(*w, "actionRename")->trigger();
        QCOMPARE(_prompter->asked.back(), QStringLiteral("saveAs:%1").arg(path)); // en su carpeta y con su nombre
        QVERIFY(!QFile::exists(path));
        QVERIFY(QFile::exists(renamed));
        const Buffer* buf = w->documents().buffer(id);
        QCOMPARE(buf->filePath(), renamed);
        QCOMPARE(buf->language(), LangType::Python); // con el lenguaje del nombre nuevo
        QCOMPARE(w->currentBuffer(), id);

        // Cancelar: nada
        QVERIFY(!w->renameBuffer(id));
        QCOMPARE(buf->filePath(), renamed);

        // A otro documento abierto: no
        const QString other = file(QStringLiteral("otro.txt"), "x");
        QVERIFY(w->openFile(other) != kInvalidBuffer);
        _prompter->saveNames.enqueue(other);
        QVERIFY(!w->renameBuffer(id));
        QVERIFY(_prompter->warnings.back().contains(QStringLiteral("Close it first")));
        QVERIFY(QFile::exists(renamed));
    }

    // Sin archivo, el nombre de la pestaña («Rename Current Tab»)
    void renameUntitled()
    {
        auto w = window();
        const BufferId id = w->currentBuffer();
        _prompter->texts.enqueue(QStringLiteral("  notas  "));
        QVERIFY(w->renameBuffer(id));
        QCOMPARE(w->documents().buffer(id)->displayName(), QStringLiteral("notas"));
        QVERIFY(w->documents().buffer(id)->isUntitled());
        // Repetido, vacío o con «/»: no
        const BufferId second = w->newDocument();
        for (const char* name : {"notas", "   ", "a/b"}) {
            _prompter->texts.enqueue(QString::fromLatin1(name));
            const QString before = w->documents().buffer(second)->displayName();
            QVERIFY(!w->renameBuffer(second));
            QCOMPARE(w->documents().buffer(second)->displayName(), before);
        }
        QCOMPARE(_prompter->warnings.size(), 3);
    }

    // Archivo › Mover a la papelera: pregunta, lo manda a la papelera y lo cierra sin preguntar
    void moveToTrash()
    {
        const QString path = file(QStringLiteral("basura.txt"), "x");
        auto w = window();
        QStringList trashed;
        bool trashWorks = true;
        w->setFileTrasher([&](const QString& p) {
            trashed << p;
            if (trashWorks)
                QFile::remove(p);
            return trashWorks;
        });
        const BufferId id = w->openFile(path);
        w->editor()->sends(SCI_APPENDTEXT, 1, "y"); // con cambios: no se pregunta por ellos

        _prompter->confirmations.enqueue(false); // «no»
        QVERIFY(!w->moveBufferToTrash(id));
        QVERIFY(trashed.isEmpty());

        trashWorks = false; // falla: aviso, y sigue abierto
        _prompter->confirmations.enqueue(true);
        QVERIFY(!w->moveBufferToTrash(id));
        QVERIFY(w->documents().buffer(id));
        QVERIFY(_prompter->warnings.back().startsWith(QStringLiteral("Move to Trash:")));

        trashWorks = true;
        _prompter->confirmations.enqueue(true);
        action(*w, "actionMoveToTrash")->trigger();
        QCOMPARE(trashed, (QStringList{path, path}));
        QVERIFY(!w->documents().buffer(id));
        QVERIFY(!_prompter->asked.join(QLatin1Char(',')).contains(QStringLiteral("save:")));

        // Sin archivo: nada que mandar
        QVERIFY(!w->moveBufferToTrash(w->currentBuffer()));
    }

    // Abrir con la aplicación predeterminada y la carpeta contenedora
    void defaultViewerAndFolder()
    {
        const QString path = file(QStringLiteral("dibujo.svg"), "<svg/>");
        auto w = window();
        QVERIFY(!w->openInDefaultViewer(w->currentBuffer())); // sin archivo
        QVERIFY(w->openFile(path) != kInvalidBuffer);
        action(*w, "actionOpenDefaultViewer")->trigger();
        QCOMPARE(_opened, (QList<QUrl>{QUrl::fromLocalFile(path)}));
        action(*w, "actionOpenFolderInFileManager")->trigger();
        QCOMPARE(_opened.back(), QUrl::fromLocalFile(_tmp.path()));
        QVERIFY(action(*w, "actionOpenFolderInTerminal"));
        QVERIFY(action(*w, "actionContainingFolderAsWorkspace"));
    }

    // El panel de caracteres: la tabla de N++ y lo que inserta cada columna
    void characterPanel()
    {
        QCOMPARE(CharacterPanel::characterLabel(0), QStringLiteral("NULL"));
        QCOMPARE(CharacterPanel::characterLabel(32), QStringLiteral("Space"));
        QCOMPARE(CharacterPanel::characterLabel(127), QStringLiteral("DEL"));
        QCOMPARE(CharacterPanel::characterLabel(0x81), QString()); // hueco de Windows-1252
        QCOMPARE(CharacterPanel::character(0x80), QChar(0x20AC));    // €
        QCOMPARE(CharacterPanel::character(0xF1), QChar(0xF1));      // ñ
        QCOMPARE(CharacterPanel::textFor(0x80, CharacterPanel::HtmlName), QStringLiteral("&euro;"));
        QCOMPARE(CharacterPanel::textFor(0x80, CharacterPanel::HtmlDecimal), QStringLiteral("&#8364;"));
        QCOMPARE(CharacterPanel::textFor(0x80, CharacterPanel::HtmlHex), QStringLiteral("&#x20ac;"));
        QCOMPARE(CharacterPanel::textFor(0xF1, CharacterPanel::HtmlName), QStringLiteral("&ntilde;"));
        QCOMPARE(CharacterPanel::textFor(10, CharacterPanel::Hex), QStringLiteral("0A"));
        QCOMPARE(CharacterPanel::textFor(10, CharacterPanel::HtmlDecimal), QString());
        QCOMPARE(CharacterPanel::textFor(45, CharacterPanel::HtmlDecimal), QStringLiteral("&#8722;")); // como N++

        auto w = window();
        auto* panel = w->findChild<CharacterPanel*>();
        QVERIFY(panel);
        QCOMPARE(panel->list()->topLevelItemCount(), 256);
        QDockWidget* dock = qobject_cast<QDockWidget*>(panel->parentWidget());
        QVERIFY(dock);
        QAction* toggle = action(*w, "actionCharPanel");
        w->show();
        QVERIFY(QTest::qWaitForWindowExposed(w.get()));
        toggle->setChecked(true);
        QTRY_VERIFY(dock->isVisible());
        setText(*w, "ab");
        w->editor()->sci(SCI_SETSEL, 1, 1);
        emit panel->list()->itemDoubleClicked(panel->list()->topLevelItem(0xF1), CharacterPanel::Character);
        emit panel->list()->itemDoubleClicked(panel->list()->topLevelItem(0x26), CharacterPanel::HtmlName);
        QCOMPARE(text(*w), QStringLiteral("añ&amp;b"));
        // Intro: el carácter de la fila
        panel->list()->setCurrentItem(panel->list()->topLevelItem(0x80));
        QTest::keyClick(panel->list(), Qt::Key_Return);
        QCOMPARE(text(*w), QStringLiteral("añ&amp;€b"));
        toggle->setChecked(false);
        QTRY_VERIFY(!dock->isVisible());
    }

    // El historial del portapapeles: lo último arriba, sin repetir; doble clic lo pega
    void clipboardHistory()
    {
        ClipboardHistoryPanel alone;
        alone.addEntry(QStringLiteral("uno"));
        alone.addEntry(QStringLiteral("dos"));
        alone.addEntry(QStringLiteral("uno"));
        alone.addEntry(QString());
        QCOMPARE(alone.entries(), (QStringList{"uno", "dos"}));
        for (int i = 0; i < ClipboardHistoryPanel::kMaxEntries + 5; ++i)
            alone.addEntry(QString::number(i));
        QCOMPARE(alone.entries().size(), ClipboardHistoryPanel::kMaxEntries);
        QCOMPARE(ClipboardHistoryPanel::displayText(QStringLiteral("a\nb\tc")), QStringLiteral("a b c"));
        QCOMPARE(ClipboardHistoryPanel::displayText(QString(70, QLatin1Char('x'))), QString(64, QLatin1Char('x')) + QStringLiteral("…"));
        // Sin partir un carácter de dos unidades UTF-16 (un emoji) al recortar (N++ 8.9.9)
        const QString emoji = QString(63, QLatin1Char('x')) + QString::fromUtf8("😀") + QStringLiteral("yy");
        QCOMPARE(ClipboardHistoryPanel::displayText(emoji), QString(63, QLatin1Char('x')) + QStringLiteral("…"));
        QVERIFY(ClipboardHistoryPanel::displayText(emoji).isValidUtf16());

        auto w = window();
        auto* panel = w->findChild<ClipboardHistoryPanel*>();
        QVERIFY(panel);
        QVERIFY(!panel->isTracking()); // hasta que se abre
        w->show();
        QVERIFY(QTest::qWaitForWindowExposed(w.get()));
        action(*w, "actionClipboardHistory")->setChecked(true);
        QTRY_VERIFY(panel->isTracking());
        QApplication::clipboard()->setText(QStringLiteral("primero"));
        QApplication::clipboard()->setText(QStringLiteral("segundo"));
        QTRY_COMPARE(panel->entries().value(0), QStringLiteral("segundo"));
        QCOMPARE(panel->entries().value(1), QStringLiteral("primero"));
        setText(*w, "");
        emit panel->list()->itemDoubleClicked(panel->list()->item(1));
        QCOMPARE(text(*w), QStringLiteral("primero"));
    }
};

QTEST_MAIN(TestOnSelection)
#include "tst_onselection.moc"
