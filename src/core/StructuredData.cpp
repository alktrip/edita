// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "StructuredData.h"

#include <QCoreApplication>
#include <QHash>
#include <QRegularExpression>
#include <QStringList>

namespace edita::data {

namespace {

// La línea y la columna (desde 1) de una posición
void locate(const QString& text, qsizetype offset, Error& e)
{
    offset = std::clamp<qsizetype>(offset, 0, text.size());
    e.offset = offset;
    const qsizetype lineStart = offset == 0 ? 0 : text.lastIndexOf(QLatin1Char('\n'), offset - 1) + 1;
    e.line = static_cast<int>(text.left(offset).count(QLatin1Char('\n'))) + 1;
    e.column = static_cast<int>(offset - lineStart) + 1;
}

QString escapeJson(const QString& s)
{
    QString out = QStringLiteral("\"");
    for (const QChar c : s) {
        switch (c.unicode()) {
        case '"':
            out += QStringLiteral("\\\"");
            break;
        case '\\':
            out += QStringLiteral("\\\\");
            break;
        case '\n':
            out += QStringLiteral("\\n");
            break;
        case '\r':
            out += QStringLiteral("\\r");
            break;
        case '\t':
            out += QStringLiteral("\\t");
            break;
        case '\b':
            out += QStringLiteral("\\b");
            break;
        case '\f':
            out += QStringLiteral("\\f");
            break;
        default:
            if (c.unicode() < 0x20)
                out += QStringLiteral("\\u%1").arg(static_cast<int>(c.unicode()), 4, 16, QLatin1Char('0'));
            else
                out += c;
        }
    }
    return out + QLatin1Char('"');
}

// --- JSON ------------------------------------------------------------------------------------

class JsonParser
{
public:
    explicit JsonParser(const QString& text)
        : _t(text)
    {
    }

    std::optional<Node> parse(Error* error)
    {
        skip();
        if (_p >= _t.size())
            return fail(QCoreApplication::translate("StructuredData", "The document is empty"), _p, error);
        Node root;
        if (!value(root))
            return fail(_message, _errorAt, error);
        skip();
        if (_p < _t.size())
            return fail(QCoreApplication::translate("StructuredData", "Unexpected text after the end of the document"), _p, error);
        return root;
    }

private:
    std::optional<Node> fail(const QString& message, qsizetype at, Error* error)
    {
        if (error) {
            error->message = message;
            locate(_t, at, *error);
        }
        return std::nullopt;
    }

    bool error(const QString& message, qsizetype at)
    {
        _message = message;
        _errorAt = at;
        return false;
    }

    void skip()
    {
        while (_p < _t.size() && (_t[_p] == QLatin1Char(' ') || _t[_p] == QLatin1Char('\t') || _t[_p] == QLatin1Char('\n')
                                  || _t[_p] == QLatin1Char('\r')))
            ++_p;
    }

    bool value(Node& n)
    {
        skip();
        if (_p >= _t.size())
            return error(QCoreApplication::translate("StructuredData", "Unexpected end of the document"), _p);
        n.start = _p;
        const QChar c = _t[_p];
        bool ok = false;
        if (c == QLatin1Char('{'))
            ok = object(n);
        else if (c == QLatin1Char('['))
            ok = array(n);
        else if (c == QLatin1Char('"'))
            ok = string(n);
        else if (c == QLatin1Char('-') || c.isDigit())
            ok = number(n);
        else if (c.isLetter())
            ok = literal(n);
        else
            return error(QCoreApplication::translate("StructuredData", "Unexpected character «%1»").arg(c), _p);
        n.end = _p;
        return ok;
    }

    bool object(Node& n)
    {
        n.kind = Node::Kind::Object;
        ++_p;
        skip();
        if (_p < _t.size() && _t[_p] == QLatin1Char('}')) {
            ++_p;
            return true;
        }
        QHash<QString, bool> seen;
        while (true) {
            skip();
            if (_p >= _t.size() || _t[_p] != QLatin1Char('"'))
                return error(_p >= _t.size() ? QCoreApplication::translate("StructuredData", "Unexpected end of the document") : QCoreApplication::translate("StructuredData", "Expected a key between quotes"), _p);
            const qsizetype keyStart = _p;
            Node key;
            if (!string(key))
                return false;
            skip();
            if (_p >= _t.size() || _t[_p] != QLatin1Char(':'))
                return error(QCoreApplication::translate("StructuredData", "Expected «:» after the key"), _p);
            ++_p;
            Node v;
            if (!value(v))
                return false;
            v.start = keyStart; // para la ruta: el miembro empieza en su clave
            n.members.emplace_back(key.scalar, std::move(v));
            skip();
            if (_p < _t.size() && _t[_p] == QLatin1Char(',')) {
                ++_p;
                continue;
            }
            if (_p < _t.size() && _t[_p] == QLatin1Char('}')) {
                ++_p;
                return true;
            }
            return error(_p >= _t.size() ? QCoreApplication::translate("StructuredData", "Unexpected end of the document") : QCoreApplication::translate("StructuredData", "Expected «,» or «}»"), _p);
        }
    }

    bool array(Node& n)
    {
        n.kind = Node::Kind::Array;
        ++_p;
        skip();
        if (_p < _t.size() && _t[_p] == QLatin1Char(']')) {
            ++_p;
            return true;
        }
        while (true) {
            skip();
            if (_p < _t.size() && _t[_p] == QLatin1Char(']'))
                return error(QCoreApplication::translate("StructuredData", "Unexpected «]»: a comma too many?"), _p);
            Node v;
            if (!value(v))
                return false;
            n.items.push_back(std::move(v));
            skip();
            if (_p < _t.size() && _t[_p] == QLatin1Char(',')) {
                ++_p;
                continue;
            }
            if (_p < _t.size() && _t[_p] == QLatin1Char(']')) {
                ++_p;
                return true;
            }
            return error(_p >= _t.size() ? QCoreApplication::translate("StructuredData", "Unexpected end of the document") : QCoreApplication::translate("StructuredData", "Expected «,» or «]»"), _p);
        }
    }

    bool string(Node& n)
    {
        n.kind = Node::Kind::String;
        const qsizetype start = _p++;
        QString out;
        while (_p < _t.size()) {
            const QChar c = _t[_p];
            if (c == QLatin1Char('"')) {
                ++_p;
                n.scalar = out;
                n.raw = _t.mid(start, _p - start);
                return true;
            }
            if (c.unicode() < 0x20)
                return error(QCoreApplication::translate("StructuredData", "A control character inside a string (use \\n, \\t…)"), _p);
            if (c == QLatin1Char('\\')) {
                if (_p + 1 >= _t.size())
                    break;
                const QChar e = _t[_p + 1];
                switch (e.unicode()) {
                case '"': out += QLatin1Char('"'); break;
                case '\\': out += QLatin1Char('\\'); break;
                case '/': out += QLatin1Char('/'); break;
                case 'b': out += QLatin1Char('\b'); break;
                case 'f': out += QLatin1Char('\f'); break;
                case 'n': out += QLatin1Char('\n'); break;
                case 'r': out += QLatin1Char('\r'); break;
                case 't': out += QLatin1Char('\t'); break;
                case 'u': {
                    bool ok = false;
                    const ushort code = _t.mid(_p + 2, 4).toUShort(&ok, 16);
                    if (!ok || _p + 6 > _t.size())
                        return error(QCoreApplication::translate("StructuredData", "Invalid \\u escape"), _p);
                    out += QChar(code);
                    _p += 4;
                    break;
                }
                default:
                    return error(QCoreApplication::translate("StructuredData", "Invalid escape «\\%1»").arg(e), _p);
                }
                _p += 2;
                continue;
            }
            out += c;
            ++_p;
        }
        return error(QCoreApplication::translate("StructuredData", "The string is not closed"), start);
    }

    bool number(Node& n)
    {
        static const QRegularExpression re(QStringLiteral(R"(-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][-+]?[0-9]+)?)"));
        const auto m = re.match(_t, _p, QRegularExpression::NormalMatch, QRegularExpression::AnchorAtOffsetMatchOption);
        if (!m.hasMatch())
            return error(QCoreApplication::translate("StructuredData", "Invalid number"), _p);
        const qsizetype end = m.capturedEnd();
        if (end < _t.size() && (_t[end].isDigit() || _t[end] == QLatin1Char('.')))
            return error(QCoreApplication::translate("StructuredData", "Invalid number (a number cannot start with 0)"), _p);
        n.kind = Node::Kind::Number;
        n.scalar = m.captured();
        _p = end;
        return true;
    }

    bool literal(Node& n)
    {
        qsizetype end = _p;
        while (end < _t.size() && _t[end].isLetterOrNumber())
            ++end;
        const QString word = _t.mid(_p, end - _p);
        if (word == QLatin1String("true") || word == QLatin1String("false"))
            n.kind = Node::Kind::Bool;
        else if (word == QLatin1String("null"))
            n.kind = Node::Kind::Null;
        else
            return error(QCoreApplication::translate("StructuredData", "Unknown value «%1» (true, false or null?)").arg(word), _p);
        n.scalar = word;
        _p = end;
        return true;
    }

    const QString& _t;
    qsizetype _p = 0;
    QString _message;
    qsizetype _errorAt = 0;
};

// --- YAML ------------------------------------------------------------------------------------

struct YamlError
{
    QString message;
    qsizetype offset = 0;
};

// Un escalar simple (sin comillas): null, booleano, número (como en JSON) o cadena
Node plainScalar(const QString& text)
{
    static const QRegularExpression integer(QStringLiteral(R"(^[-+]?(?:0|[1-9][0-9]*)$)"));
    static const QRegularExpression decimal(QStringLiteral(R"(^[-+]?(?:\.[0-9]+|[0-9]+(?:\.[0-9]*)?)(?:[eE][-+]?[0-9]+)?$)"));
    Node n;
    const QString v = text.trimmed();
    if (v.isEmpty() || v == QLatin1String("~") || v.compare(QLatin1String("null"), Qt::CaseInsensitive) == 0) {
        n.kind = Node::Kind::Null;
        n.scalar = QStringLiteral("null");
    } else if (v.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0 || v.compare(QLatin1String("false"), Qt::CaseInsensitive) == 0) {
        n.kind = Node::Kind::Bool;
        n.scalar = v.toLower();
    } else if (integer.match(v).hasMatch() || decimal.match(v).hasMatch()) {
        // En la forma de JSON: sin +, con un 0 delante de «.5» y detrás de «1.»
        QString number = v.startsWith(QLatin1Char('+')) ? v.mid(1) : v;
        const bool negative = number.startsWith(QLatin1Char('-'));
        QString body = negative ? number.mid(1) : number;
        if (body.startsWith(QLatin1Char('.')))
            body.prepend(QLatin1Char('0'));
        body.replace(QRegularExpression(QStringLiteral(R"(\.(?=[eE]|$))")), QStringLiteral(".0"));
        n.kind = Node::Kind::Number;
        n.scalar = (negative ? QStringLiteral("-") : QString()) + body;
    } else {
        n.kind = Node::Kind::String;
        n.scalar = v;
    }
    return n;
}

class YamlParser
{
public:
    explicit YamlParser(const QString& text)
        : _t(text)
    {
    }

    std::optional<Node> parse(Error* error)
    {
        if (!split()) {
            report(error);
            return std::nullopt;
        }
        if (_lines.empty()) {
            Node n;
            n.scalar = QStringLiteral("null");
            return n;
        }
        Node root;
        if (!node(_lines.front().indent, root)) {
            report(error);
            return std::nullopt;
        }
        if (_pos < _lines.size()) {
            fail(QCoreApplication::translate("StructuredData", "Unexpected indentation"), _lines[_pos].offset);
            report(error);
            return std::nullopt;
        }
        return root;
    }

private:
    struct Line
    {
        int indent = 0;
        QString text;           // sin la sangría ni los comentarios
        qsizetype offset = 0;   // donde empieza el texto (después de la sangría)
        qsizetype end = 0;      // el final de la línea
        qsizetype raw = 0;      // su índice en _raw
    };
    struct RawLine
    {
        qsizetype start = 0;
        QString text;
    };

    void report(Error* error) const
    {
        if (error) {
            error->message = _error.message;
            locate(_t, _error.offset, *error);
        }
    }

    bool fail(const QString& message, qsizetype offset)
    {
        _error = {message, offset};
        return false;
    }

    // El texto sin el comentario (# al principio o después de un espacio, fuera de comillas)
    static QString stripComment(const QString& s)
    {
        QChar quote;
        for (qsizetype i = 0; i < s.size(); ++i) {
            const QChar c = s[i];
            if (!quote.isNull()) {
                if (c == QLatin1Char('\\') && quote == QLatin1Char('"'))
                    ++i;
                else if (c == quote)
                    quote = QChar();
            } else if ((c == QLatin1Char('"') || c == QLatin1Char('\'')) && (i == 0 || s[i - 1].isSpace() || QStringLiteral("[{,:-").contains(s[i - 1]))) {
                quote = c;
            } else if (c == QLatin1Char('#') && (i == 0 || s[i - 1].isSpace())) {
                return s.left(i);
            }
        }
        return s;
    }

    bool split()
    {
        qsizetype start = 0;
        while (start <= _t.size()) {
            qsizetype end = _t.indexOf(QLatin1Char('\n'), start);
            if (end < 0)
                end = _t.size();
            QString text = _t.mid(start, end - start);
            if (text.endsWith(QLatin1Char('\r')))
                text.chop(1);
            _raw.push_back({start, text});
            if (end == _t.size())
                break;
            start = end + 1;
        }
        for (qsizetype r = 0; r < static_cast<qsizetype>(_raw.size()); ++r) {
            const RawLine& raw = _raw[static_cast<size_t>(r)];
            int indent = 0;
            while (indent < raw.text.size() && raw.text[indent] == QLatin1Char(' '))
                ++indent;
            const QString content = stripComment(raw.text.mid(indent)).trimmed();
            if (content.isEmpty())
                continue;
            if (indent < raw.text.size() && raw.text[indent] == QLatin1Char('\t'))
                return fail(QCoreApplication::translate("StructuredData", "Tabs are not allowed in the indentation"), raw.start + indent);
            if (indent == 0 && (content == QLatin1String("---") || content.startsWith(QLatin1String("--- "))))
                continue;
            if (indent == 0 && content == QLatin1String("..."))
                break;
            _lines.push_back({indent, content, raw.start + indent, raw.start + raw.text.size(), r});
        }
        return true;
    }

    static bool isSequence(const QString& text)
    {
        return text == QLatin1String("-") || text.startsWith(QLatin1String("- "));
    }

    // La posición de los dos puntos de «clave: valor» (fuera de comillas y corchetes), o -1
    static qsizetype keyColon(const QString& text)
    {
        if (text.startsWith(QLatin1Char('[')) || text.startsWith(QLatin1Char('{')))
            return -1;
        QChar quote;
        for (qsizetype i = 0; i < text.size(); ++i) {
            const QChar c = text[i];
            if (!quote.isNull()) {
                if (c == QLatin1Char('\\') && quote == QLatin1Char('"'))
                    ++i;
                else if (c == quote)
                    quote = QChar();
            } else if (i == 0 && (c == QLatin1Char('"') || c == QLatin1Char('\''))) {
                quote = c;
            } else if (c == QLatin1Char(':') && (i + 1 == text.size() || text[i + 1] == QLatin1Char(' '))) {
                return i;
            }
        }
        return -1;
    }

    bool node(int indent, Node& out)
    {
        const Line& line = _lines[_pos];
        if (isSequence(line.text))
            return sequence(line.indent, out);
        if (keyColon(line.text) >= 0)
            return mapping(line.indent, out);
        // Un escalar (o una colección [ ] { }) solo en su línea
        Q_UNUSED(indent);
        ++_pos;
        return inlineValue(line.text, line.offset, _pos - 1, out);
    }

    bool mapping(int indent, Node& out)
    {
        out.kind = Node::Kind::Object;
        out.start = _lines[_pos].offset;
        QHash<QString, bool> seen;
        while (_pos < _lines.size() && _lines[_pos].indent == indent) {
            const Line line = _lines[_pos];
            if (isSequence(line.text))
                return fail(QCoreApplication::translate("StructuredData", "A list item where a key was expected"), line.offset);
            const qsizetype colon = keyColon(line.text);
            if (colon < 0)
                return fail(QCoreApplication::translate("StructuredData", "Expected «key: value»"), line.offset);
            QString key = line.text.left(colon).trimmed();
            if (key.size() >= 2 && (key.front() == QLatin1Char('"') || key.front() == QLatin1Char('\''))) {
                Node k;
                if (!quoted(key, line.offset, k))
                    return false;
                key = k.scalar;
            }
            if (seen.contains(key))
                return fail(QCoreApplication::translate("StructuredData", "The key «%1» is repeated").arg(key), line.offset);
            seen.insert(key, true);
            const QString rest = line.text.mid(colon + 1).trimmed();
            const qsizetype restOffset = line.offset + line.text.indexOf(rest, colon + 1);
            ++_pos;
            Node v;
            if (!valueAfter(rest, restOffset, indent, true, v))
                return false;
            v.start = line.offset;
            v.end = _pos > 0 ? _lines[_pos - 1].end : line.end;
            out.members.emplace_back(key, std::move(v));
            if (_pos < _lines.size() && _lines[_pos].indent > indent)
                return fail(QCoreApplication::translate("StructuredData", "Unexpected indentation"), _lines[_pos].offset);
        }
        out.end = _lines[_pos - 1].end;
        return true;
    }

    bool sequence(int indent, Node& out)
    {
        out.kind = Node::Kind::Array;
        out.start = _lines[_pos].offset;
        while (_pos < _lines.size() && _lines[_pos].indent == indent && isSequence(_lines[_pos].text)) {
            Line& line = _lines[_pos];
            const QString rest = line.text.mid(1).trimmed();
            const qsizetype itemOffset = line.offset;
            Node v;
            if (!rest.isEmpty() && (isSequence(rest) || keyColon(rest) >= 0) && !rest.startsWith(QLatin1Char('&'))) {
                // «- - x» o «- clave: valor»: lo de después del guion, como una línea más adentro
                const int column = static_cast<int>(line.text.indexOf(rest, 1));
                line.indent += column;
                line.offset += column;
                line.text = rest;
                if (!node(line.indent, v))
                    return false;
            } else {
                const qsizetype restOffset = line.offset + (rest.isEmpty() ? 1 : line.text.indexOf(rest, 1));
                ++_pos;
                if (!valueAfter(rest, restOffset, indent, false, v))
                    return false;
            }
            v.start = itemOffset;
            v.end = _lines[_pos - 1].end;
            out.items.push_back(std::move(v));
            if (_pos < _lines.size() && _lines[_pos].indent > indent)
                return fail(QCoreApplication::translate("StructuredData", "Unexpected indentation"), _lines[_pos].offset);
        }
        out.end = _lines[_pos - 1].end;
        return true;
    }

    // El valor de una clave o de un elemento de lista: en la misma línea, en bloque debajo o
    // un bloque de texto (| >)
    bool valueAfter(QString rest, qsizetype restOffset, int indent, bool inMapping, Node& out)
    {
        QString anchor;
        bool forceString = false;
        // Etiquetas (!!str, !Ref…): solo !!str cambia algo
        while (rest.startsWith(QLatin1Char('!')) || rest.startsWith(QLatin1Char('&'))) {
            const qsizetype space = rest.indexOf(QLatin1Char(' '));
            const QString token = space < 0 ? rest : rest.left(space);
            if (token.startsWith(QLatin1Char('&')))
                anchor = token.mid(1);
            else if (token == QLatin1String("!!str"))
                forceString = true;
            rest = space < 0 ? QString() : rest.mid(space + 1).trimmed();
            restOffset += token.size() + 1;
        }
        bool ok = true;
        if (rest.startsWith(QLatin1Char('*'))) {
            const QString name = rest.mid(1).trimmed();
            if (!_anchors.contains(name))
                return fail(QCoreApplication::translate("StructuredData", "The alias «*%1» has no anchor «&%1»").arg(name), restOffset);
            out = _anchors.value(name);
        } else if (rest.startsWith(QLatin1Char('|')) || rest.startsWith(QLatin1Char('>'))) {
            ok = blockScalar(rest, indent, out);
        } else if (rest.isEmpty()) {
            // Debajo, más adentro (o una lista a la misma altura que su clave)
            if (_pos < _lines.size()
                && (_lines[_pos].indent > indent || (inMapping && _lines[_pos].indent == indent && isSequence(_lines[_pos].text)))) {
                ok = node(_lines[_pos].indent, out);
            } else {
                out = plainScalar(QString());
            }
        } else {
            ok = inlineValue(rest, restOffset, _pos - 1, out);
        }
        if (!ok)
            return false;
        if (forceString && out.kind != Node::Kind::Array && out.kind != Node::Kind::Object) {
            out.kind = Node::Kind::String;
        }
        if (!anchor.isEmpty())
            _anchors.insert(anchor, out);
        return true;
    }

    // | y > con su indicador (+ conserva los saltos del final, - los quita)
    bool blockScalar(const QString& header, int parentIndent, Node& out)
    {
        const bool folded = header.startsWith(QLatin1Char('>'));
        const QChar chomp = header.contains(QLatin1Char('-')) ? QLatin1Char('-') : header.contains(QLatin1Char('+')) ? QLatin1Char('+') : QChar();
        const qsizetype firstRaw = _lines[_pos - 1].raw + 1;
        QStringList content;
        int blockIndent = -1;
        qsizetype r = firstRaw;
        for (; r < static_cast<qsizetype>(_raw.size()); ++r) {
            const QString& text = _raw[static_cast<size_t>(r)].text;
            int indent = 0;
            while (indent < text.size() && text[indent] == QLatin1Char(' '))
                ++indent;
            if (text.trimmed().isEmpty()) {
                content << QString();
                continue;
            }
            if (indent <= parentIndent)
                break;
            if (blockIndent < 0)
                blockIndent = indent;
            if (indent < blockIndent)
                break;
            content << text.mid(blockIndent);
        }
        // Las líneas del bloque ya no son nodos
        while (_pos < _lines.size() && _lines[_pos].raw < r)
            ++_pos;
        // Los vacíos del final, aparte (para el indicador)
        int trailing = 0;
        while (!content.isEmpty() && content.back().isEmpty()) {
            content.removeLast();
            ++trailing;
        }
        QString text;
        if (folded) {
            for (qsizetype i = 0; i < content.size(); ++i) {
                if (i > 0)
                    text += content[i].isEmpty() || content[i - 1].isEmpty() ? QStringLiteral("\n") : QStringLiteral(" ");
                text += content[i];
            }
        } else {
            text = content.join(QLatin1Char('\n'));
        }
        if (chomp == QLatin1Char('+'))
            text += QString(trailing + 1, QLatin1Char('\n'));
        else if (chomp != QLatin1Char('-') && !content.isEmpty())
            text += QLatin1Char('\n');
        out.kind = Node::Kind::String;
        out.scalar = text;
        return true;
    }

    // Un valor en una línea: entre comillas, una colección [ ] { } (que puede seguir en las
    // líneas siguientes) o un escalar simple
    bool inlineValue(QString text, qsizetype offset, size_t lineIndex, Node& out)
    {
        out.start = offset;
        out.end = _lines[lineIndex].end;
        if (text.startsWith(QLatin1Char('[')) || text.startsWith(QLatin1Char('{'))) {
            // Hasta que se cierre, juntando líneas
            while (!balanced(text)) {
                if (_pos >= _lines.size() || _lines[_pos].indent <= _lines[lineIndex].indent)
                    return fail(QCoreApplication::translate("StructuredData", "«%1» is not closed").arg(text.front()), offset);
                text += QLatin1Char(' ') + _lines[_pos++].text;
            }
            qsizetype p = 0;
            if (!flow(text, p, offset, out))
                return false;
            while (p < text.size() && text[p].isSpace())
                ++p;
            if (p < text.size())
                return fail(QCoreApplication::translate("StructuredData", "Unexpected text after «%1»").arg(text.left(p).right(1)), offset + p);
            return true;
        }
        if (text.startsWith(QLatin1Char('"')) || text.startsWith(QLatin1Char('\'')))
            return quoted(text, offset, out);
        out = plainScalar(text);
        out.start = offset;
        out.end = _lines[lineIndex].end;
        return true;
    }

    static bool balanced(const QString& text)
    {
        int depth = 0;
        QChar quote;
        for (qsizetype i = 0; i < text.size(); ++i) {
            const QChar c = text[i];
            if (!quote.isNull()) {
                if (c == QLatin1Char('\\') && quote == QLatin1Char('"'))
                    ++i;
                else if (c == quote)
                    quote = QChar();
            } else if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
                quote = c;
            } else if (c == QLatin1Char('[') || c == QLatin1Char('{')) {
                ++depth;
            } else if (c == QLatin1Char(']') || c == QLatin1Char('}')) {
                --depth;
            }
        }
        return depth <= 0 && quote.isNull();
    }

    bool quoted(const QString& text, qsizetype offset, Node& out)
    {
        qsizetype p = 0;
        if (!quotedAt(text, p, offset, out))
            return false;
        if (!text.mid(p).trimmed().isEmpty())
            return fail(QCoreApplication::translate("StructuredData", "Unexpected text after the quotes"), offset + p);
        return true;
    }

    bool quotedAt(const QString& text, qsizetype& p, qsizetype offset, Node& out)
    {
        const QChar quote = text[p];
        const qsizetype start = p++;
        QString value;
        while (p < text.size()) {
            const QChar c = text[p];
            if (c == quote) {
                if (quote == QLatin1Char('\'') && p + 1 < text.size() && text[p + 1] == QLatin1Char('\'')) {
                    value += quote;
                    p += 2;
                    continue;
                }
                ++p;
                out.kind = Node::Kind::String;
                out.scalar = value;
                return true;
            }
            if (c == QLatin1Char('\\') && quote == QLatin1Char('"') && p + 1 < text.size()) {
                const QChar e = text[p + 1];
                p += 2;
                switch (e.unicode()) {
                case 'n': value += QLatin1Char('\n'); break;
                case 't': value += QLatin1Char('\t'); break;
                case 'r': value += QLatin1Char('\r'); break;
                case '0': value += QChar(0); break;
                case '"': value += QLatin1Char('"'); break;
                case '\\': value += QLatin1Char('\\'); break;
                case '/': value += QLatin1Char('/'); break;
                case ' ': value += QLatin1Char(' '); break;
                case 'x':
                case 'u': {
                    const int digits = e == QLatin1Char('x') ? 2 : 4;
                    bool ok = false;
                    const ushort code = text.mid(p, digits).toUShort(&ok, 16);
                    if (!ok)
                        return fail(QCoreApplication::translate("StructuredData", "Invalid escape"), offset + p - 2);
                    value += QChar(code);
                    p += digits;
                    break;
                }
                default:
                    return fail(QCoreApplication::translate("StructuredData", "Invalid escape «\\%1»").arg(e), offset + p - 2);
                }
                continue;
            }
            value += c;
            ++p;
        }
        return fail(QCoreApplication::translate("StructuredData", "The quotes are not closed"), offset + start);
    }

    // [a, b] y {k: v}, también anidadas
    bool flow(const QString& text, qsizetype& p, qsizetype offset, Node& out)
    {
        auto skip = [&] {
            while (p < text.size() && text[p].isSpace())
                ++p;
        };
        skip();
        if (p >= text.size())
            return fail(QCoreApplication::translate("StructuredData", "Unexpected end of the line"), offset + p);
        const QChar c = text[p];
        if (c == QLatin1Char('[') || c == QLatin1Char('{')) {
            const bool isMap = c == QLatin1Char('{');
            const QChar close = isMap ? QLatin1Char('}') : QLatin1Char(']');
            out.kind = isMap ? Node::Kind::Object : Node::Kind::Array;
            ++p;
            skip();
            if (p < text.size() && text[p] == close) {
                ++p;
                return true;
            }
            while (true) {
                skip();
                if (isMap) {
                    // La clave: entre comillas o hasta «:»
                    QString key;
                    if (p < text.size() && (text[p] == QLatin1Char('"') || text[p] == QLatin1Char('\''))) {
                        Node k;
                        if (!quotedAt(text, p, offset, k))
                            return false;
                        key = k.scalar;
                        skip();
                    } else {
                        const qsizetype start = p;
                        while (p < text.size() && text[p] != QLatin1Char(':') && text[p] != QLatin1Char(',') && text[p] != close)
                            ++p;
                        key = text.mid(start, p - start).trimmed();
                    }
                    Node v;
                    if (p < text.size() && text[p] == QLatin1Char(':')) {
                        ++p;
                        if (!flow(text, p, offset, v))
                            return false;
                    } else {
                        v = plainScalar(QString());
                    }
                    out.members.emplace_back(key, std::move(v));
                } else {
                    Node v;
                    if (!flow(text, p, offset, v))
                        return false;
                    out.items.push_back(std::move(v));
                }
                skip();
                if (p < text.size() && text[p] == QLatin1Char(',')) {
                    ++p;
                    continue;
                }
                if (p < text.size() && text[p] == close) {
                    ++p;
                    return true;
                }
                return fail(QCoreApplication::translate("StructuredData", "Expected «,» or «%1»").arg(close), offset + p);
            }
        }
        if (c == QLatin1Char('"') || c == QLatin1Char('\''))
            return quotedAt(text, p, offset, out);
        if (c == QLatin1Char('*')) {
            const qsizetype start = ++p;
            while (p < text.size() && text[p] != QLatin1Char(',') && text[p] != QLatin1Char(']') && text[p] != QLatin1Char('}'))
                ++p;
            const QString name = text.mid(start, p - start).trimmed();
            if (!_anchors.contains(name))
                return fail(QCoreApplication::translate("StructuredData", "The alias «*%1» has no anchor «&%1»").arg(name), offset + start - 1);
            out = _anchors.value(name);
            return true;
        }
        // Un escalar simple: hasta la coma o el cierre
        const qsizetype start = p;
        while (p < text.size() && text[p] != QLatin1Char(',') && text[p] != QLatin1Char(']') && text[p] != QLatin1Char('}'))
            ++p;
        out = plainScalar(text.mid(start, p - start));
        return true;
    }

    const QString& _t;
    std::vector<RawLine> _raw;
    std::vector<Line> _lines;
    size_t _pos = 0;
    QHash<QString, Node> _anchors;
    YamlError _error;
};

// --- Escribir --------------------------------------------------------------------------------

void writeJson(const Node& n, int indent, bool useTabs, int level, QString& out)
{
    const QString unit = useTabs ? QStringLiteral("\t") : QString(indent, QLatin1Char(' '));
    auto pad = [&](int depth) {
        for (int i = 0; i < depth; ++i)
            out += unit;
    };
    const bool pretty = indent > 0;
    switch (n.kind) {
    case Node::Kind::Object:
        if (n.members.empty()) {
            out += QStringLiteral("{}");
            return;
        }
        out += QLatin1Char('{');
        for (size_t i = 0; i < n.members.size(); ++i) {
            if (i > 0)
                out += QLatin1Char(',');
            if (pretty) {
                out += QLatin1Char('\n');
                pad(level + 1);
            }
            out += escapeJson(n.members[i].first);
            out += pretty ? QStringLiteral(": ") : QStringLiteral(":");
            writeJson(n.members[i].second, indent, useTabs, level + 1, out);
        }
        if (pretty) {
            out += QLatin1Char('\n');
            pad(level);
        }
        out += QLatin1Char('}');
        return;
    case Node::Kind::Array:
        if (n.items.empty()) {
            out += QStringLiteral("[]");
            return;
        }
        out += QLatin1Char('[');
        for (size_t i = 0; i < n.items.size(); ++i) {
            if (i > 0)
                out += QLatin1Char(',');
            if (pretty) {
                out += QLatin1Char('\n');
                pad(level + 1);
            }
            writeJson(n.items[i], indent, useTabs, level + 1, out);
        }
        if (pretty) {
            out += QLatin1Char('\n');
            pad(level);
        }
        out += QLatin1Char(']');
        return;
    case Node::Kind::String:
        out += n.raw.isEmpty() ? escapeJson(n.scalar) : n.raw;
        return;
    default:
        out += n.scalar.isEmpty() ? QStringLiteral("null") : n.scalar;
    }
}

// ¿Hay que poner este texto entre comillas en YAML? Si se leería como otra cosa (null, un
// número, un booleano, también los de YAML 1.1: yes, no, on, off) o rompería la línea
bool yamlNeedsQuotes(const QString& s)
{
    if (s.isEmpty() || s != s.trimmed())
        return true;
    if (plainScalar(s).kind != Node::Kind::String)
        return true;
    static const QStringList yaml11{QStringLiteral("yes"), QStringLiteral("no"), QStringLiteral("on"), QStringLiteral("off"),
                                    QStringLiteral("y"), QStringLiteral("n")};
    if (yaml11.contains(s.toLower()))
        return true;
    if (QStringLiteral("-?:,[]{}#&*!|>'\"%@`").contains(s.front()))
        return true;
    if (s.contains(QLatin1String(": ")) || s.contains(QLatin1String(" #")) || s.endsWith(QLatin1Char(':')))
        return true;
    for (const QChar c : s) {
        if (c.unicode() < 0x20)
            return true;
    }
    return false;
}

QString yamlScalar(const Node& n)
{
    if (n.kind != Node::Kind::String)
        return n.scalar.isEmpty() ? QStringLiteral("null") : n.scalar;
    return yamlNeedsQuotes(n.scalar) ? escapeJson(n.scalar) : n.scalar;
}

void writeYaml(const Node& n, int level, QString& out)
{
    const QString pad(level * 2, QLatin1Char(' '));
    auto inlineOf = [](const Node& v) -> QString {
        if (v.kind == Node::Kind::Object && v.members.empty())
            return QStringLiteral("{}");
        if (v.kind == Node::Kind::Array && v.items.empty())
            return QStringLiteral("[]");
        if (v.kind == Node::Kind::Object || v.kind == Node::Kind::Array)
            return QString();
        return yamlScalar(v);
    };
    if (n.kind == Node::Kind::Object) {
        for (const auto& [key, value] : n.members) {
            out += pad + (yamlNeedsQuotes(key) ? escapeJson(key) : key) + QLatin1Char(':');
            const QString single = inlineOf(value);
            if (!single.isEmpty()) {
                out += QLatin1Char(' ') + single + QLatin1Char('\n');
            } else {
                out += QLatin1Char('\n');
                writeYaml(value, level + 1, out);
            }
        }
    } else if (n.kind == Node::Kind::Array) {
        for (const Node& item : n.items) {
            const QString single = inlineOf(item);
            if (!single.isEmpty()) {
                out += pad + QStringLiteral("- ") + single + QLatin1Char('\n');
            } else if (item.kind == Node::Kind::Object) {
                // «- clave: valor», y las demás claves alineadas con la primera
                QString nested;
                writeYaml(item, level + 1, nested);
                out += pad + QStringLiteral("- ") + nested.mid((level + 1) * 2);
            } else {
                out += pad + QStringLiteral("-\n");
                writeYaml(item, level + 1, out);
            }
        }
    } else {
        out += pad + yamlScalar(n) + QLatin1Char('\n');
    }
}

void pathOf(const Node& n, qsizetype offset, QString& path)
{
    static const QRegularExpression identifier(QStringLiteral(R"(^[A-Za-z_][A-Za-z0-9_-]*$)"));
    if (n.kind == Node::Kind::Object) {
        for (const auto& [key, value] : n.members) {
            if (offset >= value.start && offset < value.end) {
                if (identifier.match(key).hasMatch())
                    path += QLatin1Char('.') + key;
                else
                    path += QStringLiteral("['") + QString(key).replace(QLatin1Char('\''), QStringLiteral("\\'")) + QStringLiteral("']");
                pathOf(value, offset, path);
                return;
            }
        }
    } else if (n.kind == Node::Kind::Array) {
        for (size_t i = 0; i < n.items.size(); ++i) {
            if (offset >= n.items[i].start && offset < n.items[i].end) {
                path += QStringLiteral("[%1]").arg(i);
                pathOf(n.items[i], offset, path);
                return;
            }
        }
    }
}

} // namespace

std::optional<Node> parseJson(const QString& text, Error* error)
{
    return JsonParser(text).parse(error);
}

std::optional<Node> parseYaml(const QString& text, Error* error)
{
    return YamlParser(text).parse(error);
}

QString formatJson(const Node& node, int indent, bool useTabs)
{
    QString out;
    writeJson(node, indent, useTabs, 0, out);
    if (indent > 0)
        out += QLatin1Char('\n');
    return out;
}

QString toYaml(const Node& node)
{
    if (node.kind == Node::Kind::Object && node.members.empty())
        return QStringLiteral("{}\n");
    if (node.kind == Node::Kind::Array && node.items.empty())
        return QStringLiteral("[]\n");
    QString out;
    writeYaml(node, 0, out);
    return out;
}

QString pathAt(const Node& root, qsizetype offset)
{
    QString path = QStringLiteral("$");
    pathOf(root, offset, path);
    return path;
}

} // namespace edita::data
