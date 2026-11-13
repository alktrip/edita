// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Editor hexadecimal (fase 89): los bytes de un archivo y sus cambios, sin interfaz.
// Sobrescribir, insertar y borrar, con deshacer y rehacer; buscar; leer un valor
// numérico en una posición. El archivo se lee y se escribe tal cual: no pasa por la
// codificación del editor.

#pragma once

#include <QByteArray>
#include <QString>

#include <optional>
#include <vector>

namespace edita::hex {

class HexBuffer
{
public:
    // Los archivos más grandes no se abren (se cargan enteros en memoria)
    static constexpr qint64 kMaxBytes = 512LL * 1024 * 1024;

    bool load(const QString& path, QString* error = nullptr);
    bool save(const QString& path, QString* error = nullptr);
    void setData(const QByteArray& data); // sin archivo (pruebas, «nuevo»)

    const QByteArray& data() const { return _data; }
    qsizetype size() const { return _data.size(); }
    const QString& path() const { return _path; }
    // Distinto de lo guardado (deshacer hasta ahí vuelve a «sin cambios»)
    bool isModified() const { return topId() != _savedId; }

    // Cambios: cada uno es un paso de deshacer (los seguidos del mismo tipo y contiguos se
    // juntan, como al escribir: un deshacer quita la palabra entera)
    void replace(qsizetype at, const QByteArray& bytes); // sobrescribir (y crecer al final)
    void insert(qsizetype at, const QByteArray& bytes);
    void remove(qsizetype at, qsizetype count);
    void setNibble(qsizetype at, bool high, int value); // un dígito hexadecimal
    bool canUndo() const { return !_undo.empty(); }
    bool canRedo() const { return !_redo.empty(); }
    qsizetype undo(); // devuelve la posición del cambio deshecho
    qsizetype redo();
    void breakMerge() { _mergeOpen = false; } // el siguiente cambio empieza otro paso

    // La siguiente (o anterior) aparición de `pattern` desde `from`, dando la vuelta
    std::optional<qsizetype> find(const QByteArray& pattern, qsizetype from, bool forward) const;

private:
    struct Change
    {
        quint64 id = 0;
        qsizetype at = 0;
        QByteArray removed;
        QByteArray inserted;
    };
    void apply(Change change, bool merge);
    quint64 topId() const { return _undo.empty() ? 0 : _undo.back().id; }

    QByteArray _data;
    QString _path;
    std::vector<Change> _undo;
    std::vector<Change> _redo;
    bool _mergeOpen = false;
    quint64 _nextId = 1;
    quint64 _savedId = 0;
};

// «4F 6B ff» o «4f6bff» → bytes; nullopt si no es hexadecimal válido
std::optional<QByteArray> parseHex(const QString& text);
// Bytes → «4F 6B FF»
QString toHex(const QByteArray& bytes, int max = -1);

// El valor que empieza en `at`, para el inspector
struct Values
{
    std::optional<qint64> i8, i16le, i16be, i32le, i32be, i64le, i64be;
    std::optional<quint64> u8, u16le, u16be, u32le, u32be, u64le, u64be;
    std::optional<float> f32le, f32be;
    std::optional<double> f64le, f64be;
    QString utf8; // el carácter UTF-8 que empieza ahí, si es válido
};
Values valuesAt(const QByteArray& data, qsizetype at);

} // namespace edita::hex
