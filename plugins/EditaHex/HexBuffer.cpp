// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "HexBuffer.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <algorithm>
#include <cstring>

namespace edita::hex {

bool HexBuffer::load(const QString& path, QString* error)
{
    QFile file(path);
    if (file.size() > kMaxBytes) {
        if (error)
            *error = QStringLiteral("too large");
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    _data = file.readAll();
    _path = QFileInfo(path).absoluteFilePath();
    _undo.clear();
    _redo.clear();
    _mergeOpen = false;
    _savedId = 0;
    return true;
}

bool HexBuffer::save(const QString& path, QString* error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(_data) != _data.size() || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    _path = QFileInfo(path).absoluteFilePath();
    _savedId = topId();
    _mergeOpen = false; // lo siguiente es otro paso: deshacer vuelve justo a lo guardado
    return true;
}

void HexBuffer::setData(const QByteArray& data)
{
    _data = data;
    _path.clear();
    _undo.clear();
    _redo.clear();
    _mergeOpen = false;
    _savedId = 0;
}

void HexBuffer::apply(Change change, bool merge)
{
    _data.remove(change.at, change.removed.size());
    _data.insert(change.at, change.inserted);
    _redo.clear();
    // Juntar con el paso anterior si sigue justo detrás (escribir o borrar seguido)
    if (merge && _mergeOpen && !_undo.empty()) {
        Change& last = _undo.back();
        if (change.removed.isEmpty() && last.removed.isEmpty() && change.at == last.at + last.inserted.size()) {
            last.inserted += change.inserted; // insertar seguido
            return;
        }
        if (change.removed.size() == change.inserted.size() && last.removed.size() == last.inserted.size()
            && change.at == last.at + last.inserted.size()) {
            last.removed += change.removed; // sobrescribir seguido
            last.inserted += change.inserted;
            return;
        }
        if (change.inserted.isEmpty() && last.inserted.isEmpty() && change.at + change.removed.size() == last.at) {
            last.at = change.at; // retroceso seguido
            last.removed.prepend(change.removed);
            return;
        }
    }
    change.id = _nextId++;
    _undo.push_back(change);
    _mergeOpen = merge;
}

void HexBuffer::replace(qsizetype at, const QByteArray& bytes)
{
    if (at < 0 || at > _data.size() || bytes.isEmpty())
        return;
    const qsizetype overlap = std::min(bytes.size(), _data.size() - at);
    apply({0, at, _data.mid(at, overlap), bytes}, true);
}

void HexBuffer::insert(qsizetype at, const QByteArray& bytes)
{
    if (at < 0 || at > _data.size() || bytes.isEmpty())
        return;
    apply({0, at, QByteArray(), bytes}, true);
}

void HexBuffer::remove(qsizetype at, qsizetype count)
{
    if (at < 0 || count <= 0 || at >= _data.size())
        return;
    count = std::min(count, _data.size() - at);
    apply({0, at, _data.mid(at, count), QByteArray()}, count == 1);
}

void HexBuffer::setNibble(qsizetype at, bool high, int value)
{
    if (at < 0 || at > _data.size() || value < 0 || value > 15)
        return;
    const auto old = at < _data.size() ? static_cast<unsigned char>(_data[at]) : 0u;
    const auto nibble = static_cast<unsigned>(value);
    const auto byte = high ? static_cast<char>((nibble << 4) | (old & 0x0Fu)) : static_cast<char>((old & 0xF0u) | nibble);
    if (at == _data.size()) {
        apply({0, at, QByteArray(), QByteArray(1, byte)}, !high);
        return;
    }
    // El segundo dígito del mismo byte va en el mismo paso que el primero
    if (!high && _mergeOpen && !_undo.empty()) {
        Change& last = _undo.back();
        if (last.at + last.inserted.size() - 1 == at && !last.inserted.isEmpty()) {
            _data[at] = byte;
            last.inserted[last.inserted.size() - 1] = byte;
            _redo.clear();
            return;
        }
    }
    apply({0, at, _data.mid(at, 1), QByteArray(1, byte)}, true);
}

qsizetype HexBuffer::undo()
{
    if (_undo.empty())
        return 0;
    const Change c = _undo.back();
    _undo.pop_back();
    _data.remove(c.at, c.inserted.size());
    _data.insert(c.at, c.removed);
    _redo.push_back(c);
    _mergeOpen = false;
    return c.at;
}

qsizetype HexBuffer::redo()
{
    if (_redo.empty())
        return 0;
    const Change c = _redo.back();
    _redo.pop_back();
    _data.remove(c.at, c.removed.size());
    _data.insert(c.at, c.inserted);
    _undo.push_back(c);
    _mergeOpen = false;
    return c.at + c.inserted.size();
}

std::optional<qsizetype> HexBuffer::find(const QByteArray& pattern, qsizetype from, bool forward) const
{
    if (pattern.isEmpty() || _data.isEmpty())
        return std::nullopt;
    if (forward) {
        qsizetype at = _data.indexOf(pattern, std::clamp<qsizetype>(from, 0, _data.size()));
        if (at < 0)
            at = _data.indexOf(pattern, 0);
        return at < 0 ? std::nullopt : std::optional<qsizetype>(at);
    }
    qsizetype at = from > 0 ? _data.lastIndexOf(pattern, from - 1) : -1;
    if (at < 0)
        at = _data.lastIndexOf(pattern);
    return at < 0 ? std::nullopt : std::optional<qsizetype>(at);
}

std::optional<QByteArray> parseHex(const QString& text)
{
    QString digits;
    for (const QChar c : text) {
        if (c.isSpace() || c == QLatin1Char(','))
            continue;
        if (!c.isDigit() && !(c.toLower() >= QLatin1Char('a') && c.toLower() <= QLatin1Char('f')))
            return std::nullopt;
        digits += c;
    }
    if (digits.isEmpty() || digits.size() % 2 != 0)
        return std::nullopt;
    return QByteArray::fromHex(digits.toLatin1());
}

QString toHex(const QByteArray& bytes, int max)
{
    const QByteArray part = max >= 0 ? bytes.left(max) : bytes;
    QString out = QString::fromLatin1(part.toHex(' ').toUpper());
    if (max >= 0 && bytes.size() > max)
        out += QStringLiteral(" …");
    return out;
}

namespace {

template<typename T>
std::optional<T> read(const QByteArray& data, qsizetype at, bool bigEndian)
{
    if (at < 0 || at + static_cast<qsizetype>(sizeof(T)) > data.size())
        return std::nullopt;
    unsigned char bytes[sizeof(T)];
    std::memcpy(bytes, data.constData() + at, sizeof(T));
    if (bigEndian == (QSysInfo::ByteOrder == QSysInfo::LittleEndian))
        std::reverse(std::begin(bytes), std::end(bytes));
    T value;
    std::memcpy(&value, bytes, sizeof(T));
    return value;
}

} // namespace

Values valuesAt(const QByteArray& data, qsizetype at)
{
    Values v;
    if (auto x = read<qint8>(data, at, false)) {
        v.i8 = *x;
        v.u8 = static_cast<quint8>(*x);
    }
    v.i16le = read<qint16>(data, at, false);
    v.i16be = read<qint16>(data, at, true);
    v.u16le = read<quint16>(data, at, false);
    v.u16be = read<quint16>(data, at, true);
    v.i32le = read<qint32>(data, at, false);
    v.i32be = read<qint32>(data, at, true);
    v.u32le = read<quint32>(data, at, false);
    v.u32be = read<quint32>(data, at, true);
    v.i64le = read<qint64>(data, at, false);
    v.i64be = read<qint64>(data, at, true);
    v.u64le = read<quint64>(data, at, false);
    v.u64be = read<quint64>(data, at, true);
    v.f32le = read<float>(data, at, false);
    v.f32be = read<float>(data, at, true);
    v.f64le = read<double>(data, at, false);
    v.f64be = read<double>(data, at, true);
    // Un carácter UTF-8 completo (1 a 4 bytes) que empieza en `at`
    if (at >= 0 && at < data.size()) {
        const auto lead = static_cast<unsigned char>(data[at]);
        const int length = lead < 0x80 ? 1 : (lead >> 5) == 0x6 ? 2 : (lead >> 4) == 0xE ? 3 : (lead >> 3) == 0x1E ? 4 : 0;
        if (length > 0 && at + length <= data.size()) {
            const QString text = QString::fromUtf8(data.mid(at, length));
            if (text.size() >= 1 && !text.contains(QChar::ReplacementCharacter))
                v.utf8 = text;
        }
    }
    return v;
}

} // namespace edita::hex
