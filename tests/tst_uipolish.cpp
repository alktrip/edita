// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fase 81: el pulido de la interfaz antes de la 1.0. Lo que se puede comprobar solo: que en
// un menú no se repitan las letras de acceso (en inglés y en español), que dos órdenes no
// tengan el mismo atajo y que los textos sigan las mismas reglas

#include "AppInfo.h"
#include "FakePrompter.h"
#include "ui/EditorView.h"
#include "ui/MainWindow.h"
#include "ui/Translations.h"

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>
#include <QDialog>
#include <QLabel>
#include <QTimer>

#include <algorithm>
#include <map>

using namespace edita;
using edita::test::FakePrompter;

namespace {

// La letra de acceso de un texto de menú («&Archivo» → a); vacía si no tiene
QChar mnemonic(const QString& text)
{
    for (qsizetype i = 0; i + 1 < text.size(); ++i) {
        if (text[i] == QLatin1Char('&')) {
            if (text[i + 1] == QLatin1Char('&')) {
                ++i;
                continue;
            }
            return text[i + 1].toLower();
        }
    }
    return {};
}

// Repeticiones que vienen de Notepad++ y se conservan: quien viene de N++ ya las conoce
bool fromNotepadPlusPlus(const QString& text)
{
    static const QSet<QString> kept{QStringLiteral("Load Sess&ion…"), QStringLiteral("Save Sess&ion…"),
                                    QStringLiteral("&Paste"), QStringLiteral("Character &Panel"),
                                    QStringLiteral("&Clone to Other View"),
                                    QStringLiteral("Syn&chronize Horizontal Scrolling")};
    return kept.contains(text);
}

// ¿Le queda al texto otra letra (o cifra) que nadie use en el menú?
bool hasFreeLetter(const QString& text, const QSet<QChar>& used)
{
    const QString plain = QString(text).remove(QLatin1Char('&')); // no en el for: remove() da una referencia al temporal
    for (const QChar c : plain) {
        if (c.unicode() < 128 && c.isLetterOrNumber() && !used.contains(c.toLower()))
            return true;
    }
    return false;
}

// Las letras repetidas que se pueden evitar. En un menú con más órdenes que letras alguna
// se repite (Qt pasa de una a otra al pulsarla); solo vale si ninguna puede cambiar
void collectDuplicateMnemonics(const QString& path, const QList<QAction*>& actions, QStringList& problems)
{
    std::map<QChar, QStringList> byLetter;
    QSet<QChar> used;
    for (QAction* a : actions) {
        if (a->isSeparator() || !a->isVisible())
            continue;
        if (const QChar c = mnemonic(a->text()); !c.isNull()) {
            byLetter[c] << a->text();
            used.insert(c);
        }
        if (QMenu* sub = a->menu())
            collectDuplicateMnemonics(path + QStringLiteral(" › ") + QString(a->text()).remove(QLatin1Char('&')),
                                      sub->actions(), problems);
    }
    for (const auto& [letter, texts] : byLetter) {
        if (texts.size() < 2)
            continue;
        const bool avoidable = std::any_of(texts.begin(), texts.end(), [&](const QString& t) {
            return !fromNotepadPlusPlus(t) && hasFreeLetter(t, used);
        });
        if (avoidable)
            problems << QStringLiteral("%1: «%2» en %3").arg(path, QString(letter), texts.join(QLatin1String(", ")));
    }
}

void collectMenuTexts(const QList<QAction*>& actions, QList<QAction*>& out)
{
    for (QAction* a : actions) {
        if (a->isSeparator())
            continue;
        out << a;
        if (QMenu* sub = a->menu())
            collectMenuTexts(sub->actions(), out);
    }
}

} // namespace

class TestUiPolish : public QObject
{
    Q_OBJECT

    QTemporaryDir _tmp;

    std::unique_ptr<MainWindow> window(const QString& language)
    {
        i18n::install(language);
        auto w = std::make_unique<MainWindow>();
        w->setPrompter(std::make_unique<FakePrompter>());
        return w;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_tmp.isValid());
        qputenv("XDG_CONFIG_HOME", _tmp.filePath("config").toUtf8());
        qputenv("EDITA_UPDATE_URL", "none");
    }

    void cleanup() { i18n::install(QStringLiteral("en")); }

    void menuMnemonicsAreUnique_data()
    {
        QTest::addColumn<QString>("language");
        QTest::newRow("en") << QStringLiteral("en");
        QTest::newRow("es") << QStringLiteral("es");
    }

    // Alt+letra lleva a una sola orden de cada menú, mientras haya letras
    void menuMnemonicsAreUnique()
    {
        QFETCH(QString, language);
        auto w = window(language);
        QStringList problems;
        collectDuplicateMnemonics(QStringLiteral("Barra"), w->menuBar()->actions(), problems);
        QVERIFY2(problems.isEmpty(), qPrintable(QLatin1Char('\n') + problems.join(QLatin1Char('\n'))));
    }

    // Un atajo, una orden (las que están en dos sitios son la misma QAction)
    void shortcutsAreUnique()
    {
        auto w = window(QStringLiteral("en"));
        std::map<QString, QStringList> byKey;
        QSet<QAction*> seen;
        for (QAction* a : w->findChildren<QAction*>()) {
            if (seen.contains(a) || a->objectName().isEmpty())
                continue;
            seen.insert(a);
            for (const QKeySequence& key : a->shortcuts()) {
                if (!key.isEmpty())
                    byKey[key.toString()] << a->objectName();
            }
        }
        QStringList problems;
        for (const auto& [key, names] : byKey) {
            if (names.size() > 1)
                problems << key + QStringLiteral(": ") + names.join(QLatin1String(", "));
        }
        QVERIFY2(problems.isEmpty(), qPrintable(QLatin1Char('\n') + problems.join(QLatin1Char('\n'))));
    }

    // F12 era a la vez Post-It (N++) e Ir a la definición: Qt no hacía ninguna de las dos, y
    // en Post-It, sin menú, no había forma de volver
    void f12TogglesPostIt()
    {
        auto w = window(QStringLiteral("en"));
        w->show();
        QVERIFY(QTest::qWaitForWindowExposed(w.get()));
        w->activateWindow();
        w->editor()->setFocus();
        QTRY_VERIFY(w->editor()->hasFocus());
        QTest::keyClick(w->editor(), Qt::Key_F12);
        QTRY_VERIFY(w->menuBar()->isHidden());
        w->activateWindow();
        w->editor()->setFocus();
        QTest::keyClick(w->editor(), Qt::Key_F12);
        QTRY_VERIFY(!w->menuBar()->isHidden());
        auto* definition = w->findChild<QAction*>(QStringLiteral("actionGoToDefinition"));
        QVERIFY(definition);
        QCOMPARE(definition->shortcut(), QKeySequence(Qt::ALT | Qt::Key_F12));
    }

    // Acerca de: la versión y la página del proyecto
    void aboutShowsTheProjectPage()
    {
        auto w = window(QStringLiteral("en"));
        QString text;
        QString links;
        QTimer::singleShot(0, this, [&text, &links] {
            if (auto* box = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
                if (auto* version = box->findChild<QLabel*>(QStringLiteral("aboutVersion")))
                    text = version->text();
                if (auto* row = box->findChild<QLabel*>(QStringLiteral("aboutLinks")))
                    links = row->text();
                box->accept();
            }
        });
        w->findChild<QAction*>(QStringLiteral("actionAbout"))->trigger();
        QVERIFY(text.contains(QString::fromUtf8(kAppVersion)));
        QVERIFY(links.contains(QLatin1String("Home Page")));
    }

    void menuTextsFollowTheRules_data() { menuMnemonicsAreUnique_data(); }

    // Puntos suspensivos con «…» (no «...»), sin espacios de más ni dos puntos al final
    void menuTextsFollowTheRules()
    {
        QFETCH(QString, language);
        auto w = window(language);
        QList<QAction*> actions;
        collectMenuTexts(w->menuBar()->actions(), actions);
        QStringList problems;
        static const QRegularExpression doubleSpace(QStringLiteral("\\S  +\\S"));
        for (QAction* a : actions) {
            const QString t = a->text();
            if (t.contains(QLatin1String("...")))
                problems << t + QStringLiteral(": «...» en vez de «…»");
            if (t != t.trimmed() || t.contains(doubleSpace))
                problems << t + QStringLiteral(": espacios de más");
            if (t.endsWith(QLatin1Char(':')))
                problems << t + QStringLiteral(": termina en «:»");
        }
        QVERIFY2(problems.isEmpty(), qPrintable(QLatin1Char('\n') + problems.join(QLatin1Char('\n'))));
    }
};

QTEST_MAIN(TestUiPolish)
#include "tst_uipolish.moc"
