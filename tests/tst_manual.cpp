// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Manual de usuario (Ayuda › Manual de usuario): el visor y el contenido de
// data/manual/<idioma>, que tiene que estar completo en todos los idiomas.

#include "FakePrompter.h"
#include "core/Paths.h"
#include "ui/MainWindow.h"
#include "ui/ManualWindow.h"

#include <QAction>
#include <QDir>
#include <QLineEdit>
#include <QListWidget>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

using namespace edita;
using edita::test::FakePrompter;

namespace {

const QStringList kLanguages{QStringLiteral("en"), QStringLiteral("es")};

QString sourceDir(const QString& language)
{
    return QStringLiteral(EDITA_SOURCE_DIR "/data/manual/") + language;
}

} // namespace

class TestManual : public QObject
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

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        qputenv("EDITA_DATA_DIR", EDITA_SOURCE_DIR "/data");
    }

    void foldAndSlug()
    {
        QCOMPARE(Manual::fold(QStringLiteral("Búsqueda AÑO ü")), QStringLiteral("busqueda ano u"));
        QCOMPARE(Manual::fold(QStringLiteral("Árbol")).size(), 5);
        QCOMPARE(Manual::slug(QStringLiteral("Sesiones y copias de seguridad")), QStringLiteral("sesiones-y-copias-de-seguridad"));
        QCOMPARE(Manual::slug(QStringLiteral("Paleta de órdenes")), QStringLiteral("paleta-de-ordenes"));
        QCOMPARE(Manual::slug(QStringLiteral("Problems and Terminal")), QStringLiteral("problems-and-terminal"));
        QCOMPARE(Manual::slug(QStringLiteral("Go to: lines (fast)")), QStringLiteral("go-to-lines-fast"));
    }

    void expandsCommands()
    {
        Manual::Commands commands;
        commands.insert(QStringLiteral("actionFind"), {QStringLiteral("Search › Find…"), QStringLiteral("Ctrl+F")});
        commands.insert(QStringLiteral("actionWrap"), {QStringLiteral("View › Word wrap"), QString()});
        commands.insert(QStringLiteral("actionStyleAll1"), {QStringLiteral("Search › Style All › Using 1st Style"), QString()});
        QStringList unresolved;
        QCOMPARE(Manual::expand(QStringLiteral("Use {{actionFind}}."), commands, &unresolved),
                 QStringLiteral("Use **Search › Find…** (`Ctrl+F`)."));
        QCOMPARE(Manual::expand(QStringLiteral("{{key:actionFind}} or {{key:actionWrap}}"), commands, &unresolved),
                 QStringLiteral("`Ctrl+F` or **View › Word wrap**"));
        QCOMPARE(Manual::expand(QStringLiteral("{{item:actionStyleAll1}}"), commands, &unresolved),
                 QStringLiteral("**Using 1st Style**"));
        QVERIFY(unresolved.isEmpty());
        QCOMPARE(Manual::expand(QStringLiteral("{{actionNope}}"), commands, &unresolved), QStringLiteral("`actionNope`"));
        QCOMPARE(unresolved, QStringList{QStringLiteral("actionNope")});
        QCOMPARE(Manual::commandsIn(QStringLiteral("{{actionA}} {{key:actionB}} {{item:actionA}}")),
                 (QStringList{QStringLiteral("actionA"), QStringLiteral("actionB")}));
    }

    void loadsChaptersAndHeadings()
    {
        QTemporaryDir dir;
        QFile f(dir.filePath("01-one.md"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("# Título uno\n\nTexto con {{actionFind}}.\n\n## Opciones\n\n```\n# no es encabezado\n```\n\n"
                "## Opciones\n\n| a | b |\n|---|---|\n| 1 | 2 |\n");
        f.close();
        Manual manual;
        Manual::Commands commands;
        commands.insert(QStringLiteral("actionFind"), {QStringLiteral("Buscar › Buscar…"), QStringLiteral("Ctrl+F")});
        QVERIFY(manual.load(dir.path(), commands));
        QCOMPARE(manual.chapters().size(), size_t(1));
        const Manual::Chapter& c = manual.chapters().front();
        QCOMPARE(c.title, QStringLiteral("Título uno"));
        QCOMPARE(c.headings.size(), size_t(3));
        QCOMPARE(c.headings[1].anchor, QStringLiteral("opciones"));
        QCOMPARE(c.headings[2].anchor, QStringLiteral("opciones-1"));
        const QString html = manual.html(0);
        QVERIFY(html.contains(QStringLiteral("<a name=\"titulo-uno\"></a>")));
        QVERIFY(html.contains(QStringLiteral("<a name=\"opciones-1\"></a>")));
        QVERIFY(html.contains(QStringLiteral("<table border=\"1\"")));
        QVERIFY(html.contains(QStringLiteral("Buscar › Buscar…")));
        // Buscar: sin acentos, todas las palabras, el título del apartado pesa más
        const auto hits = manual.search(QStringLiteral("TITULO"));
        QCOMPARE(hits.size(), size_t(1));
        QCOMPARE(hits.front().anchor, QStringLiteral("titulo-uno"));
        QVERIFY(manual.search(QStringLiteral("opciones")).size() >= 2);
        QVERIFY(manual.search(QStringLiteral("texto inexistente")).empty());
    }

    void languageFallback()
    {
        QCOMPARE(QDir(Manual::directoryFor(QStringLiteral("es"))).dirName(), QStringLiteral("es"));
        QCOMPARE(QDir(Manual::directoryFor(QStringLiteral("es_MX"))).dirName(), QStringLiteral("es"));
        QCOMPARE(QDir(Manual::directoryFor(QStringLiteral("fr"))).dirName(), QStringLiteral("en"));
    }

    // Lo mismo en todos los idiomas: los mismos capítulos, todas las órdenes existen y todos
    // los enlaces llevan a un capítulo y un apartado que existen
    void contentIsComplete()
    {
        auto w = window();
        const Manual::Commands commands = w->manualCommands();
        QVERIFY(commands.contains(QStringLiteral("actionUserManual")));
        QStringList files;
        for (const QString& language : kLanguages) {
            Manual manual;
            QVERIFY2(manual.load(sourceDir(language), commands), qPrintable(language));
            QVERIFY2(manual.unresolved().isEmpty(), qPrintable(language + QStringLiteral(": ") + manual.unresolved().join(' ')));
            QStringList names;
            for (const Manual::Chapter& c : manual.chapters())
                names << c.fileName;
            if (files.isEmpty())
                files = names;
            QCOMPARE(names, files);
            static const QRegularExpression link(QStringLiteral("\\]\\(([^)\\s]+)\\)"));
            for (const Manual::Chapter& c : manual.chapters()) {
                QVERIFY2(!c.title.isEmpty(), qPrintable(c.fileName));
                QVERIFY2(!manual.html(manual.indexOf(c.fileName)).isEmpty(), qPrintable(c.fileName));
                for (auto it = link.globalMatch(c.markdown); it.hasNext();) {
                    const QString target = it.next().captured(1);
                    if (target.startsWith(QLatin1String("http")))
                        continue;
                    const QString file = target.section(QLatin1Char('#'), 0, 0);
                    const QString anchor = target.section(QLatin1Char('#'), 1);
                    const int index = file.isEmpty() ? manual.indexOf(c.fileName) : manual.indexOf(file);
                    const QString where = language + QLatin1Char('/') + c.fileName + QStringLiteral(" → ") + target;
                    if (file.startsWith(QLatin1String("images/"))) {
                        QVERIFY2(QFileInfo::exists(QDir(manual.directory()).filePath(file)), qPrintable(where));
                        continue;
                    }
                    QVERIFY2(index >= 0, qPrintable(where));
                    if (anchor.isEmpty())
                        continue;
                    bool found = false;
                    for (const Manual::Heading& h : manual.chapters()[static_cast<size_t>(index)].headings)
                        found = found || h.anchor == anchor;
                    QVERIFY2(found, qPrintable(where));
                }
            }
        }
        QVERIFY(files.size() >= 5);
    }

    void windowNavigatesAndSearches()
    {
        auto w = window();
        QAction* action = w->findChild<QAction*>(QStringLiteral("actionUserManual"));
        QVERIFY(action);
        QCOMPARE(action->shortcut(), QKeySequence(QKeySequence::HelpContents));
        action->trigger();
        ManualWindow* manual = w->manualWindow();
        QVERIFY(manual);
        QVERIFY(manual->isVisible());
        QCOMPARE(manual->currentChapter(), manual->manual().chapters().front().fileName);
        QVERIFY(manual->browser()->toPlainText().contains(manual->manual().chapters().front().title));
        // El índice: un elemento por capítulo, con sus apartados
        QCOMPARE(manual->contents()->topLevelItemCount(), static_cast<int>(manual->manual().chapters().size()));
        QVERIFY(manual->contents()->topLevelItem(1)->childCount() > 0);

        // Abrir un apartado y volver atrás
        w->showManual(QStringLiteral("02-files.md"), QStringLiteral("saving"));
        QCOMPARE(manual->currentChapter(), QStringLiteral("02-files.md"));
        QCOMPARE(manual->browser()->source().fragment(), QStringLiteral("saving"));
        QCOMPARE(manual->contents()->currentItem()->data(0, Qt::UserRole + 1).toString(), QStringLiteral("saving"));
        QVERIFY(manual->browser()->isBackwardAvailable());
        manual->browser()->backward();
        QCOMPARE(manual->currentChapter(), manual->manual().chapters().front().fileName);

        // Pulsar en el índice navega
        manual->contents()->setCurrentItem(manual->contents()->topLevelItem(3));
        QCOMPARE(manual->currentChapter(), manual->manual().chapters()[3].fileName);

        // Las órdenes se ven con su menú y su atajo
        w->showManual(QStringLiteral("04-search.md"));
        QVERIFY(manual->browser()->toPlainText().contains(QStringLiteral("Search › Find…")));

        // Buscar: lista de resultados, abrir el primero y marcar las palabras
        manual->searchField()->setText(QStringLiteral("regular EXPRESSION"));
        QVERIFY(manual->results()->isVisible());
        QVERIFY(manual->results()->count() > 0);
        QTest::keyClick(manual->searchField(), Qt::Key_Return);
        QVERIFY(!manual->browser()->extraSelections().isEmpty());
        manual->searchField()->setText(QStringLiteral("zzzqqq"));
        QCOMPARE(manual->results()->count(), 1);
        QCOMPARE(manual->results()->item(0)->flags(), Qt::NoItemFlags);
        QTest::keyClick(manual->searchField(), Qt::Key_Escape);
        QVERIFY(manual->searchField()->text().isEmpty());
        QVERIFY(manual->contents()->isVisible());
        QVERIFY(manual->browser()->extraSelections().isEmpty());

        // Un enlace a otro capítulo
        w->showManual(QStringLiteral("01-introduction.md"));
        // (como al pulsar un enlace: relativo al capítulo actual)
        manual->browser()->setSource(manual->browser()->source().resolved(QUrl(QStringLiteral("02-files.md#sessions-and-backups"))));
        QCOMPARE(manual->browser()->source().fragment(), QStringLiteral("sessions-and-backups"));
        QCOMPARE(manual->currentChapter(), QStringLiteral("02-files.md"));
    }
};

QTEST_MAIN(TestManual)
#include "tst_manual.moc"
