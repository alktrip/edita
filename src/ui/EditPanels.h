// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Los paneles del menú Editar de N++: el panel de caracteres (AnsiCharPanel) y el historial
// del portapapeles (ClipboardHistoryPanel)

#pragma once

#include <QStringList>
#include <QWidget>

class QListWidget;
class QListWidgetItem;
class QTreeWidget;
class QTreeWidgetItem;

namespace edita {

// Los 256 caracteres de la tabla ANSI (Windows-1252, la de N++ en un sistema occidental) con su
// valor, su código hexadecimal y sus entidades HTML. Doble clic en el carácter (o Intro) lo
// inserta; en otra columna, inserta lo que pone (el número, la entidad…)
class CharacterPanel : public QWidget
{
    Q_OBJECT

public:
    enum Column { Value, Hex, Character, HtmlName, HtmlDecimal, HtmlHex, ColumnCount };

    explicit CharacterPanel(QWidget* parent = nullptr);
    QTreeWidget* list() const { return _list; }

    // El carácter de la tabla (U+FFFD en los huecos de Windows-1252) y lo que muestra la
    // columna «Carácter» (NULL, TAB, Space… para los de control)
    static QChar character(int value);
    static QString characterLabel(int value);
    static QString htmlName(int value);
    // El número de la entidad HTML (-1 si no tiene)
    static int htmlNumber(int value);

    // Lo que se inserta al activar esa fila en esa columna (vacío: nada)
    static QString textFor(int value, int column);

signals:
    void insertText(const QString& text);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QTreeWidget* _list = nullptr;
};

// Lo último copiado (en Edita o en otra aplicación), lo más reciente arriba y sin repetir.
// Doble clic (o Intro) lo pega en el documento
class ClipboardHistoryPanel : public QWidget
{
    Q_OBJECT

public:
    // Cuántos textos guarda como máximo
    static constexpr int kMaxEntries = 100;
    // Cuánto se muestra de cada uno (MAX_DISPLAY_LENGTH de N++)
    static constexpr int kDisplayLength = 64;

    explicit ClipboardHistoryPanel(QWidget* parent = nullptr);
    QListWidget* list() const { return _list; }
    const QStringList& entries() const { return _entries; }

    // Empieza a seguir el portapapeles (al abrir el panel la primera vez, como N++)
    void startTracking();
    bool isTracking() const { return _tracking; }
    // Lo añade arriba (si ya estaba, lo sube)
    void addEntry(const QString& text);
    void clear();

    static QString displayText(const QString& text);

signals:
    void insertText(const QString& text);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void rebuild();

    QListWidget* _list = nullptr;
    QStringList _entries;
    bool _tracking = false;
};

} // namespace edita
