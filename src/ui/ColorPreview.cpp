// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/ColorPreview.h"

#include "ui/EditorView.h"

#include <QApplication>
#include <QColorDialog>
#include <QHelpEvent>
#include <QRegularExpression>
#include <QToolTip>

#include <algorithm>
#include <cmath>

namespace edita {

namespace {

constexpr int kMaxScanBytes = 256 * 1024; // lo visible; más es una línea larguísima

int hexValue(QChar c)
{
    return c.isDigit() ? c.unicode() - '0' : c.toLower().unicode() - 'a' + 10;
}

// Un componente de rgb(): 0–255 o porcentaje
int rgbComponent(const QString& text)
{
    if (text.endsWith(QLatin1Char('%')))
        return std::clamp(qRound(text.chopped(1).toDouble() * 255.0 / 100.0), 0, 255);
    return std::clamp(qRound(text.toDouble()), 0, 255);
}

// La transparencia: 0–1 o porcentaje
int alphaComponent(const QString& text)
{
    if (text.isEmpty())
        return 255;
    if (text.endsWith(QLatin1Char('%')))
        return std::clamp(qRound(text.chopped(1).toDouble() * 255.0 / 100.0), 0, 255);
    return std::clamp(qRound(text.toDouble() * 255), 0, 255);
}

QString alphaText(const QColor& c)
{
    QString text = QString::number(c.alphaF(), 'f', 2);
    while (text.endsWith(QLatin1Char('0')))
        text.chop(1);
    if (text.endsWith(QLatin1Char('.')))
        text.chop(1);
    return text;
}

} // namespace

std::vector<ColorLiteral> findColorLiterals(const QByteArray& bytes, bool shortHex)
{
    // Latin-1: un carácter por byte, así las posiciones son las del texto UTF-8 (los
    // colores son ASCII; lo demás no coincide con nada)
    const QString text = QString::fromLatin1(bytes);
    std::vector<ColorLiteral> found;
    static const QRegularExpression hex(QStringLiteral("(?<![\\w&#])#([0-9a-fA-F]{8}|[0-9a-fA-F]{6}|[0-9a-fA-F]{4}|[0-9a-fA-F]{3})(?![\\w])"));
    for (auto it = hex.globalMatch(text); it.hasNext();) {
        const QRegularExpressionMatch m = it.next();
        const QString digits = m.captured(1);
        if (!shortHex && digits.size() < 6)
            continue;
        int r, g, b, a = 255;
        if (digits.size() <= 4) {
            r = hexValue(digits[0]) * 17;
            g = hexValue(digits[1]) * 17;
            b = hexValue(digits[2]) * 17;
            if (digits.size() == 4)
                a = hexValue(digits[3]) * 17;
        } else {
            r = digits.mid(0, 2).toInt(nullptr, 16);
            g = digits.mid(2, 2).toInt(nullptr, 16);
            b = digits.mid(4, 2).toInt(nullptr, 16);
            if (digits.size() == 8)
                a = digits.mid(6, 2).toInt(nullptr, 16); // CSS: la transparencia al final
        }
        found.push_back({static_cast<int>(m.capturedStart()), static_cast<int>(m.capturedLength()), QColor(r, g, b, a),
                         ColorLiteral::Format::Hex});
    }
    static const QRegularExpression rgb(QStringLiteral("\\b(rgba?)\\(\\s*([\\d.]+%?)\\s*[,\\s]\\s*([\\d.]+%?)\\s*[,\\s]\\s*([\\d.]+%?)"
                                                       "\\s*(?:[,/]\\s*([\\d.]+%?)\\s*)?\\)"),
                                        QRegularExpression::CaseInsensitiveOption);
    for (auto it = rgb.globalMatch(text); it.hasNext();) {
        const QRegularExpressionMatch m = it.next();
        found.push_back({static_cast<int>(m.capturedStart()), static_cast<int>(m.capturedLength()),
                         QColor(rgbComponent(m.captured(2)), rgbComponent(m.captured(3)), rgbComponent(m.captured(4)),
                                alphaComponent(m.captured(5))),
                         ColorLiteral::Format::Rgb});
    }
    static const QRegularExpression hsl(QStringLiteral("\\b(hsla?)\\(\\s*([\\d.]+)(?:deg)?\\s*[,\\s]\\s*([\\d.]+)%\\s*[,\\s]\\s*([\\d.]+)%"
                                                       "\\s*(?:[,/]\\s*([\\d.]+%?)\\s*)?\\)"),
                                        QRegularExpression::CaseInsensitiveOption);
    for (auto it = hsl.globalMatch(text); it.hasNext();) {
        const QRegularExpressionMatch m = it.next();
        const double h = std::fmod(m.captured(2).toDouble(), 360.0) / 360.0;
        QColor color = QColor::fromHslF(static_cast<float>(h), static_cast<float>(std::clamp(m.captured(3).toDouble() / 100.0, 0.0, 1.0)),
                                        static_cast<float>(std::clamp(m.captured(4).toDouble() / 100.0, 0.0, 1.0)));
        color.setAlpha(alphaComponent(m.captured(5)));
        found.push_back({static_cast<int>(m.capturedStart()), static_cast<int>(m.capturedLength()), color, ColorLiteral::Format::Hsl});
    }
    std::sort(found.begin(), found.end(), [](const ColorLiteral& a, const ColorLiteral& b) { return a.start < b.start; });
    return found;
}

QString formatColorLike(const QString& original, const QColor& color)
{
    const QString lower = original.toLower();
    if (original.startsWith(QLatin1Char('#'))) {
        const int digits = static_cast<int>(original.size()) - 1;
        const bool withAlpha = digits == 4 || digits == 8 || color.alpha() < 255;
        QString text = QStringLiteral("#%1%2%3").arg(color.red(), 2, 16, QLatin1Char('0')).arg(color.green(), 2, 16, QLatin1Char('0'))
                           .arg(color.blue(), 2, 16, QLatin1Char('0'));
        if (withAlpha)
            text += QStringLiteral("%1").arg(color.alpha(), 2, 16, QLatin1Char('0'));
        // Corto si era corto y se puede (#abc)
        if (digits <= 4) {
            bool canShorten = true;
            for (qsizetype i = 1; i < text.size(); i += 2)
                canShorten = canShorten && text[i] == text[i + 1];
            if (canShorten) {
                QString shortText = QStringLiteral("#");
                for (qsizetype i = 1; i < text.size(); i += 2)
                    shortText += text[i];
                text = shortText;
            }
        }
        // Las mayúsculas del original
        const bool upper = original.mid(1) != original.mid(1).toLower();
        return upper ? text.toUpper() : text;
    }
    if (lower.startsWith(QLatin1String("hsl"))) {
        const bool alpha = lower.startsWith(QLatin1String("hsla")) || color.alpha() < 255;
        const QString base = QStringLiteral("%1, %2%, %3%").arg(qMax(0, qRound(color.hslHueF() * 360))).arg(qRound(color.hslSaturationF() * 100))
                                 .arg(qRound(color.lightnessF() * 100));
        return alpha ? QStringLiteral("hsla(%1, %2)").arg(base, alphaText(color)) : QStringLiteral("hsl(%1)").arg(base);
    }
    const bool alpha = lower.startsWith(QLatin1String("rgba")) || color.alpha() < 255;
    const QString base = QStringLiteral("%1, %2, %3").arg(color.red()).arg(color.green()).arg(color.blue());
    return alpha ? QStringLiteral("rgba(%1, %2)").arg(base, alphaText(color)) : QStringLiteral("rgb(%1)").arg(base);
}

ColorPreview::ColorPreview(EditorView* view)
    : QObject(view)
    , _view(view)
{
    // Un recuadro con el color de cada valor (SC_INDICFLAG_VALUEFORE), debajo del texto
    view->send(SCI_INDICSETSTYLE, kIndicator, INDIC_FULLBOX);
    view->send(SCI_INDICSETFLAGS, kIndicator, SC_INDICFLAG_VALUEFORE);
    view->send(SCI_INDICSETALPHA, kIndicator, 90);
    view->send(SCI_INDICSETOUTLINEALPHA, kIndicator, 255);
    view->send(SCI_INDICSETUNDER, kIndicator, 1);
    view->send(SCI_INDICSETSTYLE, kOutlineIndicator, INDIC_BOX);
    view->send(SCI_INDICSETALPHA, kOutlineIndicator, 160);
    _timer.setSingleShot(true);
    _timer.setInterval(40);
    connect(&_timer, &QTimer::timeout, this, &ColorPreview::updateNow);
    connect(view, &ScintillaEditBase::updateUi, this, [this](Scintilla::Update updated) {
        if (static_cast<int>(updated) & (SC_UPDATE_CONTENT | SC_UPDATE_V_SCROLL | SC_UPDATE_H_SCROLL))
            schedule();
    });
    connect(view, &ScintillaEditBase::modified, this, [this] { schedule(); });
    // Ctrl+clic sobre un color: el selector
    connect(view, &ScintillaEditBase::notify, this, [this](Scintilla::NotificationData* n) {
        if (static_cast<int>(n->nmhdr.code) != SCN_INDICATORRELEASE || !_enabled || !_supported)
            return;
        if (!(static_cast<int>(n->modifiers) & SCMOD_CTRL))
            return;
        const sptr_t pos = n->position;
        if (!_view->send(SCI_INDICATORVALUEAT, kIndicator, pos))
            return;
        QTimer::singleShot(0, this, [this, pos] { pickColorAt(pos); });
    });
    view->viewport()->installEventFilter(this);
}

bool ColorPreview::supports(LangType lang)
{
    switch (lang) {
    case LangType::Css: case LangType::Html: case LangType::Xml: case LangType::JavaScript: case LangType::TypeScript:
    case LangType::Json: case LangType::Json5: case LangType::Python: case LangType::Yaml: case LangType::Ini:
    case LangType::Props: case LangType::Php: case LangType::Asp:
        return true;
    default:
        return false;
    }
}

void ColorPreview::setEnabled(bool enabled)
{
    _enabled = enabled;
    if (enabled)
        updateNow();
    else
        clearAll();
}

void ColorPreview::setLanguage(LangType lang)
{
    _lang = lang;
    _supported = supports(lang);
    updateNow();
}

void ColorPreview::schedule()
{
    if (_enabled && _supported)
        _timer.start();
}

void ColorPreview::clearAll()
{
    if (!_view)
        return;
    for (const int indicator : {kIndicator, kOutlineIndicator}) {
        _view->send(SCI_SETINDICATORCURRENT, static_cast<uptr_t>(indicator));
        _view->send(SCI_INDICATORCLEARRANGE, 0, _view->send(SCI_GETLENGTH));
    }
}

void ColorPreview::updateNow()
{
    _timer.stop();
    if (!_view)
        return;
    if (!_enabled || !_supported) {
        clearAll();
        return;
    }
    // Solo lo visible (con un poco de margen)
    const sptr_t firstVisible = _view->send(SCI_GETFIRSTVISIBLELINE);
    const sptr_t firstLine = _view->send(SCI_DOCLINEFROMVISIBLE, static_cast<uptr_t>(firstVisible));
    const sptr_t lastLine = _view->send(SCI_DOCLINEFROMVISIBLE, static_cast<uptr_t>(firstVisible + _view->send(SCI_LINESONSCREEN) + 1));
    const sptr_t start = _view->send(SCI_POSITIONFROMLINE, static_cast<uptr_t>(firstLine));
    const sptr_t end = std::min(_view->send(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(lastLine)), start + kMaxScanBytes);
    if (end <= start)
        return;
    QByteArray text(static_cast<qsizetype>(end - start) + 1, '\0');
    Sci_TextRangeFull range{{start, end}, text.data()};
    _view->send(SCI_GETTEXTRANGEFULL, 0, reinterpret_cast<sptr_t>(&range));
    text.chop(1);
    for (const int indicator : {kIndicator, kOutlineIndicator}) {
        _view->send(SCI_SETINDICATORCURRENT, static_cast<uptr_t>(indicator));
        _view->send(SCI_INDICATORCLEARRANGE, static_cast<uptr_t>(start), end - start);
    }
    // El fondo y el texto del editor: un color parecido al fondo lleva borde del color del texto
    const sptr_t backValue = _view->send(SCI_STYLEGETBACK, STYLE_DEFAULT);
    const QColor back(static_cast<int>(backValue & 0xFF), static_cast<int>((backValue >> 8) & 0xFF), static_cast<int>((backValue >> 16) & 0xFF));
    _view->send(SCI_INDICSETFORE, kOutlineIndicator, _view->send(SCI_STYLEGETFORE, STYLE_DEFAULT));
    // #abc de 3 o 4 cifras, solo donde no se confunde con comentarios (# en Python, YAML…)
    const bool shortHex = _lang == LangType::Css || _lang == LangType::Html || _lang == LangType::Xml || _lang == LangType::JavaScript
                       || _lang == LangType::TypeScript || _lang == LangType::Json || _lang == LangType::Json5 || _lang == LangType::Php
                       || _lang == LangType::Asp;
    for (const ColorLiteral& c : findColorLiterals(text, shortHex)) {
        const QColor& color = c.color;
        _view->send(SCI_SETINDICATORCURRENT, kIndicator);
        _view->send(SCI_SETINDICATORVALUE, static_cast<uptr_t>(SC_INDICVALUEBIT | color.red() | (color.green() << 8) | (color.blue() << 16)));
        _view->send(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(start + c.start), c.length);
        const int distance = std::abs(color.red() - back.red()) + std::abs(color.green() - back.green()) + std::abs(color.blue() - back.blue());
        if (distance < 90 || color.alpha() < 90) {
            _view->send(SCI_SETINDICATORCURRENT, kOutlineIndicator);
            _view->send(SCI_SETINDICATORVALUE, 1);
            _view->send(SCI_INDICATORFILLRANGE, static_cast<uptr_t>(start + c.start), c.length);
        }
    }
}

bool ColorPreview::colorAt(intptr_t pos, intptr_t* start, intptr_t* end, QColor* color) const
{
    if (!_view || !_view->send(SCI_INDICATORVALUEAT, kIndicator, pos))
        return false;
    const sptr_t s = _view->send(SCI_INDICATORSTART, kIndicator, pos);
    const sptr_t e = _view->send(SCI_INDICATOREND, kIndicator, pos);
    QByteArray text(static_cast<qsizetype>(e - s) + 1, '\0');
    Sci_TextRangeFull range{{s, e}, text.data()};
    _view->send(SCI_GETTEXTRANGEFULL, 0, reinterpret_cast<sptr_t>(&range));
    text.chop(1);
    const std::vector<ColorLiteral> found = findColorLiterals(text);
    if (found.empty() || found.front().start != 0)
        return false;
    *start = s;
    *end = s + found.front().length;
    *color = found.front().color;
    return true;
}

bool ColorPreview::pickColorAt(intptr_t pos)
{
    intptr_t start = 0;
    intptr_t end = 0;
    QColor color;
    if (!colorAt(pos, &start, &end, &color))
        return false;
    const QColor chosen = _picker ? _picker(color)
                                  : QColorDialog::getColor(color, _view, tr("Choose a color"), QColorDialog::ShowAlphaChannel);
    if (!chosen.isValid() || !_view)
        return false;
    QByteArray original(static_cast<qsizetype>(end - start) + 1, '\0');
    Sci_TextRangeFull range{{start, end}, original.data()};
    _view->send(SCI_GETTEXTRANGEFULL, 0, reinterpret_cast<sptr_t>(&range));
    original.chop(1);
    const QByteArray replacement = formatColorLike(QString::fromLatin1(original), chosen).toLatin1();
    _view->send(SCI_BEGINUNDOACTION);
    _view->send(SCI_SETTARGETRANGE, static_cast<uptr_t>(start), end);
    _view->send(SCI_REPLACETARGET, static_cast<uptr_t>(replacement.size()), reinterpret_cast<sptr_t>(replacement.constData()));
    _view->send(SCI_ENDUNDOACTION);
    _view->send(SCI_SETSEL, static_cast<uptr_t>(start), start + replacement.size());
    updateNow();
    return true;
}

// La muestra grande al pasar el ratón
bool ColorPreview::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() != QEvent::ToolTip || !_view || watched != _view->viewport() || !_enabled || !_supported)
        return false;
    auto* help = static_cast<QHelpEvent*>(event);
    const sptr_t pos = _view->send(SCI_POSITIONFROMPOINTCLOSE, static_cast<uptr_t>(help->pos().x()), help->pos().y());
    intptr_t start = 0;
    intptr_t end = 0;
    QColor color;
    if (pos < 0 || !colorAt(pos, &start, &end, &color))
        return false;
    const QString solid = color.name(QColor::HexRgb);
    const QString values = QStringLiteral("%1<br>rgb(%2, %3, %4)%5<br>hsl(%6, %7%, %8%)")
                               .arg(color.alpha() < 255 ? color.name(QColor::HexArgb) : solid)
                               .arg(color.red()).arg(color.green()).arg(color.blue())
                               .arg(color.alpha() < 255 ? QStringLiteral(" · α %1").arg(alphaText(color)) : QString())
                               .arg(qMax(0, qRound(color.hslHueF() * 360))).arg(qRound(color.hslSaturationF() * 100))
                               .arg(qRound(color.lightnessF() * 100));
    const QString html = QStringLiteral("<table cellspacing=\"0\" cellpadding=\"0\"><tr>"
                                        "<td style=\"background-color:%1\" width=\"44\" height=\"44\">&nbsp;</td>"
                                        "<td style=\"padding-left:8px\">%2<br><i>%3</i></td></tr></table>")
                             .arg(solid, values, tr("Ctrl+click to choose a color").toHtmlEscaped());
    QToolTip::showText(help->globalPos(), html, _view->viewport());
    return true;
}

} // namespace edita
