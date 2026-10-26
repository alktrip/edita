// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ruta de navegación (no existe en Notepad++): una barra encima del editor con
//   carpeta › carpeta › archivo › clase › función
// según dónde está el cursor. Pulsar una carpeta abre la lista de sus archivos; pulsar
// el archivo o un símbolo, la de los símbolos del documento (con buscador). Los símbolos
// son los de la Lista de funciones; sin analizador, los bloques plegables que contienen
// al cursor (encabezados de Markdown, etiquetas…).

#pragma once

#include <QWidget>

#include <vector>

class QHBoxLayout;

namespace edita {

class QuickPicker;

class Breadcrumbs : public QWidget
{
    Q_OBJECT

public:
    struct Symbol
    {
        QString name;
        bool isClass = false;
        int line = 0;     // primera línea (desde 0)
        int endLine = 0;  // última línea del bloque
    };

    explicit Breadcrumbs(QWidget* parent = nullptr);

    // El archivo (vacío: sin título) y las carpetas de trabajo (la ruta empieza en la suya)
    void setFile(const QString& path, const QString& displayName, const QStringList& roots);
    // Los símbolos del documento, ordenados por línea; `fallback`: son bloques, no símbolos
    void setSymbols(std::vector<Symbol> symbols, bool fallback = false);
    // La línea del cursor: qué símbolos la contienen
    void setCaretLine(int line);

    // Para las pruebas
    QStringList segments() const;
    QuickPicker* picker() const { return _picker; }
    bool clickSegment(int index);

signals:
    void openFileRequested(const QString& path);
    void goToLineRequested(int line);

private:
    void rebuild();
    void showFolder(const QString& folder, QWidget* anchor);
    void showSymbols(QWidget* anchor);

    QHBoxLayout* _layout = nullptr;
    QString _path;
    QString _displayName;
    QStringList _roots;
    std::vector<Symbol> _symbols;
    bool _fallback = false;
    int _caretLine = 0;
    std::vector<int> _trail; // índices en _symbols de los que contienen al cursor
    QuickPicker* _picker = nullptr;
};

} // namespace edita
