// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// El manual de usuario (Ayuda › Manual de usuario, F1). Los capítulos son archivos
// Markdown en data/manual/<idioma>/NN-nombre.md (las imágenes, en images/), con el mismo
// nombre de archivo en todos los idiomas para que los enlaces entre capítulos valgan en
// todos; si el idioma de la interfaz no tiene manual se usa el inglés.
//
// Dentro del texto, {{actionFind}} se convierte en el menú y el atajo de esa orden tal
// como están en la aplicación («Buscar › Buscar… (Ctrl+F)»): traducidos, con los atajos
// que el usuario haya cambiado y los de macOS. {{key:actionFind}} pone solo el atajo, y
// {{item:actionStyleAll1}}, solo el nombre en su submenú («Estilo 1») con el atajo.
//
// La ventana: índice (capítulos y sus apartados), búsqueda en todo el manual sin
// distinguir acentos ni mayúsculas, historial (atrás y adelante) y colores del tema.

#pragma once

#include <QHash>
#include <QTextBrowser>
#include <QWidget>

#include <functional>
#include <vector>

class QLineEdit;
class QListWidget;
class QSplitter;
class QStackedWidget;
class QToolButton;
class QTreeWidget;

namespace edita {

class Manual
{
public:
    struct Heading
    {
        int level = 1;
        QString text;
        QString anchor;
    };
    struct Chapter
    {
        QString fileName; // "03-editing.md"
        QString title;    // el primer encabezado
        QString markdown; // con las órdenes ya sustituidas
        std::vector<Heading> headings;
    };
    struct Hit
    {
        int chapter = -1;
        QString anchor;
        QString heading;
        QString excerpt;
        int score = 0;
    };
    struct Command
    {
        QString path;     // "Search › Find…"
        QString shortcut; // "Ctrl+F" (vacío si no tiene)
    };
    using Commands = QHash<QString, Command>; // por nombre del objeto ("actionFind")

    // La carpeta del manual en ese idioma ("es", "es_MX"…) o, si no la hay, la del inglés
    static QString directoryFor(const QString& language);

    bool load(const QString& directory, const Commands& commands = {});
    QString directory() const { return _directory; }
    const std::vector<Chapter>& chapters() const { return _chapters; }
    int indexOf(const QString& fileName) const;
    // El capítulo en HTML, con un <a name> antes de cada encabezado
    QString html(int chapter) const;
    // Los apartados que contienen todas las palabras, los mejores primero
    std::vector<Hit> search(const QString& query) const;
    // Las órdenes de {{…}} que no están en las que se dieron a load()
    QStringList unresolved() const { return _unresolved; }

    // "Buscar y reemplazar" → "buscar-y-reemplazar" (sin acentos)
    static QString slug(const QString& heading);
    // Minúsculas y sin acentos, con la misma longitud que el original
    static QString fold(const QString& text);
    // Los nombres que aparecen en {{…}}, {{key:…}} y {{item:…}}
    static QStringList commandsIn(const QString& markdown);
    static QString expand(const QString& markdown, const Commands& commands, QStringList* unresolved = nullptr);
    // El texto sin la sintaxis de Markdown (para buscar y para los extractos)
    static QString plainText(const QString& markdown);

private:
    QString _directory;
    std::vector<Chapter> _chapters;
    QStringList _unresolved;
};

class ManualBrowser : public QTextBrowser
{
    Q_OBJECT

public:
    explicit ManualBrowser(const Manual& manual, QWidget* parent = nullptr);
    QVariant loadResource(int type, const QUrl& name) override;

protected:
    void doSetSource(const QUrl& name, QTextDocument::ResourceType type) override;

private:
    const Manual& _manual;
};

class ManualWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ManualWindow(std::function<Manual::Commands()> commands, QWidget* parent = nullptr);

    // Muestra la ventana en ese capítulo y apartado (sin capítulo: donde estaba, o el primero)
    void open(const QString& fileName = {}, const QString& anchor = {});
    // Vuelve a leer el manual (otro idioma, otros atajos)
    void reload();

    const Manual& manual() const { return _manual; }
    // Para las pruebas
    ManualBrowser* browser() const { return _browser; }
    QLineEdit* searchField() const { return _search; }
    QTreeWidget* contents() const { return _tree; }
    QListWidget* results() const { return _results; }
    QString currentChapter() const;

protected:
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void retranslate();
    void applyTheme();
    void fillContents();
    void runSearch();
    void openHit(int row);
    void syncContents();
    void highlightTerms();
    QUrl urlFor(const QString& fileName, const QString& anchor) const;

    std::function<Manual::Commands()> _commands;
    Manual _manual;
    QString _language;
    QToolButton* _back = nullptr;
    QToolButton* _forward = nullptr;
    QToolButton* _home = nullptr;
    QLineEdit* _search = nullptr;
    QStackedWidget* _side = nullptr;
    QTreeWidget* _tree = nullptr;
    QListWidget* _results = nullptr;
    QSplitter* _splitter = nullptr;
    ManualBrowser* _browser = nullptr;
    QStringList _highlight; // las palabras buscadas (ya plegadas), para marcarlas al abrir
    bool _syncing = false;
};

} // namespace edita
