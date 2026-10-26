// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Probador de expresiones regulares (no existe en Notepad++): un panel con la expresión,
// un texto de prueba en el que se resaltan en vivo las coincidencias y sus grupos (cada
// grupo con su color), la lista de coincidencias con los grupos de cada una, y la vista
// previa de un reemplazo. Usa la misma regex Boost que Buscar (search/SearchEngine), así
// que lo que coincide aquí coincide igual en el documento. «Usar en Buscar» pasa la
// expresión al diálogo.

#pragma once

#include "search/SearchOptions.h"
#include "search/SearchTarget.h"

#include <QTimer>
#include <QWidget>

#include <vector>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QToolButton;
class QTreeWidget;
class ScintillaEditBase;

namespace edita {

class SyntaxManager;

class RegexTesterPanel : public QWidget
{
    Q_OBJECT

public:
    struct Group
    {
        int number = 0;
        QString name; // (?<nombre>…)
        SciPos start = -1;
        SciPos end = -1;
    };
    struct Match
    {
        SciPos start = 0;
        SciPos end = 0;
        std::vector<Group> groups; // del 1 en adelante
    };

    explicit RegexTesterPanel(QWidget* parent = nullptr);

    void applyStyles(const SyntaxManager& syntax);
    void setSample(const QString& text);
    QString sample() const;
    void setPattern(const QString& pattern);
    void setToggles(bool matchCase, bool dotMatchesNewline);
    SearchOptions options() const;

    // Evalúa ya (si no, se hace un momento después de cada cambio)
    void evaluate();

    // Los nombres de los grupos de captura en orden (el índice es el número de grupo; el 0,
    // la coincidencia entera, sin nombre). Vacío si el grupo no tiene nombre
    static std::vector<QString> groupNames(const QString& pattern);

    // Para las pruebas
    QLineEdit* patternField() const { return _pattern; }
    QLineEdit* replacementField() const { return _replacement; }
    ScintillaEditBase* sampleView() const { return _sample; }
    QTreeWidget* tree() const { return _tree; }
    QPlainTextEdit* previewView() const { return _preview; }
    QString statusText() const;
    const std::vector<Match>& matches() const { return _matches; }
    int count() const { return _count; }

signals:
    void sampleFromEditorRequested();
    void useInFindRequested(const edita::SearchOptions& options);

private:
    void configureSample();
    void clearHighlights();
    void fillTree();
    void updatePreview();
    QColor groupColor(int number) const;
    void selectInSample(SciPos start, SciPos end);
    QToolButton* referenceButton();

    QLineEdit* _pattern = nullptr;
    QLineEdit* _replacement = nullptr;
    QToolButton* _matchCase = nullptr;
    QToolButton* _dotAll = nullptr;
    QLabel* _status = nullptr;
    ScintillaEditBase* _sample = nullptr;
    QTreeWidget* _tree = nullptr;
    QPlainTextEdit* _preview = nullptr;
    QTimer _timer;
    std::vector<Match> _matches;
    int _count = 0;
    QString _error;
    bool _dark = false;
};

} // namespace edita
