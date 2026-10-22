// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/FileIcons.h"

#include "ui/Appearance.h"

#include <QApplication>
#include <QFileInfo>
#include <QFont>
#include <QFontMetricsF>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>

namespace edita::fileicons {

namespace {

bool g_enabled = true;

struct Entry
{
    const char* label;
    unsigned color; // 0xRRGGBB (los de GitHub Linguist, algo ajustados para leerse)
};

// Por extensión (en minúsculas)
const QHash<QString, Entry>& extensions()
{
    static const QHash<QString, Entry> table = [] {
        QHash<QString, Entry> t;
        auto add = [&t](std::initializer_list<const char*> exts, Entry e) {
            for (const char* ext : exts)
                t.insert(QString::fromLatin1(ext), e);
        };
        add({"c"}, {"C", 0x5C6BC0});
        add({"h"}, {"H", 0x8E7CC3});
        add({"cpp", "cc", "cxx", "c++", "ino"}, {"C++", 0xF34B7D});
        add({"hpp", "hh", "hxx", "h++", "inl"}, {"H++", 0xC2558A});
        add({"cs"}, {"C#", 0x178600});
        add({"java"}, {"JAV", 0xB07219});
        add({"kt", "kts"}, {"KT", 0xA97BFF});
        add({"go"}, {"GO", 0x00ADD8});
        add({"rs"}, {"RS", 0xDEA584});
        add({"swift"}, {"SW", 0xF05138});
        add({"py", "pyw", "pyi"}, {"PY", 0x3572A5});
        add({"rb"}, {"RB", 0xCC342D});
        add({"php", "phtml"}, {"PHP", 0x4F5D95});
        add({"pl", "pm"}, {"PL", 0x0298C3});
        add({"lua"}, {"LUA", 0x2C2D72});
        add({"js", "mjs", "cjs", "jsx"}, {"JS", 0xE8D44D});
        add({"ts", "tsx", "mts", "cts"}, {"TS", 0x3178C6});
        add({"json", "jsonc", "json5"}, {"{}", 0xCBA72F});
        add({"html", "htm", "xhtml"}, {"<>", 0xE34C26});
        add({"css", "qss"}, {"CSS", 0x663399});
        add({"scss", "sass", "less"}, {"CSS", 0xC6538C});
        add({"xml", "xsd", "xsl", "xslt", "plist", "ui", "qrc", "ts.xml"}, {"XML", 0x0060AC});
        add({"svg"}, {"SVG", 0xFF9A00});
        add({"md", "markdown", "mdown"}, {"MD", 0x2B7489});
        add({"rst"}, {"RST", 0x6E7781});
        add({"tex", "latex", "sty"}, {"TEX", 0x3D6117});
        add({"txt", "text", "log"}, {"TXT", 0x7D8590});
        add({"csv", "tsv"}, {"CSV", 0x237346});
        add({"yaml", "yml"}, {"YML", 0xCB171E});
        add({"toml"}, {"TML", 0x9C4221});
        add({"ini", "cfg", "conf", "properties", "desktop", "env"}, {"INI", 0x6E7781});
        add({"sh", "bash", "zsh", "fish", "ksh"}, {"SH", 0x4EAA25});
        add({"bat", "cmd"}, {"BAT", 0x8C9C1E});
        add({"ps1", "psm1"}, {"PS", 0x2671BE});
        add({"sql"}, {"SQL", 0xE38C00});
        add({"cmake"}, {"CM", 0xDA3434});
        add({"mk", "mak"}, {"MK", 0x427819});
        add({"diff", "patch"}, {"DIF", 0x88562A});
        add({"asm", "s", "nasm"}, {"ASM", 0x6E4C13});
        add({"r"}, {"R", 0x198CE7});
        add({"dart"}, {"DRT", 0x00B4AB});
        add({"vb", "vbs", "bas"}, {"VB", 0x945DB7});
        add({"pas", "pp", "dpr"}, {"PAS", 0xE3F171});
        add({"f", "f90", "f95", "for"}, {"F", 0x4D41B1});
        add({"hs"}, {"HS", 0x5E5086});
        add({"erl", "ex", "exs"}, {"EX", 0x6E4A7E});
        add({"nsi", "nsh"}, {"NSI", 0x9A3C52});
        add({"pdf"}, {"PDF", 0xD73A49});
        add({"png", "jpg", "jpeg", "gif", "bmp", "ico", "webp"}, {"IMG", 0x8957E5});
        add({"zip", "gz", "xz", "bz2", "7z", "tar", "rar"}, {"ZIP", 0x7D8590});
        return t;
    }();
    return table;
}

// Por nombre completo (en minúsculas)
const QHash<QString, Entry>& names()
{
    static const QHash<QString, Entry> table = {
        {QStringLiteral("makefile"), {"MK", 0x427819}},  {QStringLiteral("gnumakefile"), {"MK", 0x427819}},
        {QStringLiteral("cmakelists.txt"), {"CM", 0xDA3434}}, {QStringLiteral("dockerfile"), {"DK", 0x384D54}},
        {QStringLiteral("license"), {"TXT", 0x7D8590}},   {QStringLiteral("copying"), {"TXT", 0x7D8590}},
        {QStringLiteral(".gitignore"), {"GIT", 0xF05033}}, {QStringLiteral(".gitattributes"), {"GIT", 0xF05033}},
        {QStringLiteral(".bashrc"), {"SH", 0x4EAA25}},    {QStringLiteral(".zshrc"), {"SH", 0x4EAA25}},
    };
    return table;
}

QColor adjusted(QColor color, bool dark)
{
    // Los muy oscuros, más claros sobre fondo oscuro (y al revés)
    if (dark && color.lightness() < 100)
        color = color.lighter(170);
    if (!dark && color.lightness() > 200)
        color = color.darker(150);
    return color;
}

QPixmap draw(const FileType& type, State state, int size, bool dark)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const qreal s = size;
    const QColor color = adjusted(type.color, dark);
    // La hoja, con la esquina doblada
    const qreal left = s * 0.12;
    const qreal right = s * 0.88;
    const qreal top = s * 0.04;
    const qreal bottom = s * 0.96;
    const qreal fold = s * 0.26;
    QPainterPath page;
    page.moveTo(left, top);
    page.lineTo(right - fold, top);
    page.lineTo(right, top + fold);
    page.lineTo(right, bottom);
    page.lineTo(left, bottom);
    page.closeSubpath();
    QColor fill = color;
    fill.setAlphaF(dark ? 0.22f : 0.14f);
    p.setPen(QPen(color, std::max(1.0, s / 16.0)));
    p.setBrush(fill);
    p.drawPath(page);
    // La etiqueta, lo más grande que quepa
    if (!type.label.isEmpty()) {
        QFont font = QApplication::font();
        font.setBold(true);
        font.setStretch(type.label.size() >= 3 ? QFont::Condensed : QFont::Unstretched);
        const qreal width = (right - left) * 0.96;
        qreal pixels = s * (type.label.size() >= 3 ? 0.40 : 0.50);
        font.setPixelSize(std::max(4, qRound(pixels)));
        while (QFontMetricsF(font).horizontalAdvance(type.label) > width && pixels > 4) {
            pixels -= 0.5;
            font.setPixelSize(std::max(4, qRound(pixels)));
        }
        p.setFont(font);
        p.setPen(dark ? color.lighter(130) : color.darker(115));
        p.drawText(QRectF(left, top + s * 0.18, right - left, bottom - top - s * 0.18), Qt::AlignCenter, type.label);
    }
    // El estado, abajo a la derecha
    if (state == State::Dirty) {
        const qreal r = s * 0.2;
        const QPointF center(s - r - s * 0.02, s - r - s * 0.02);
        p.setPen(QPen(QApplication::palette().color(QPalette::Window), s / 16.0 + 0.5));
        p.setBrush(appearance::unsavedColor(dark));
        p.drawEllipse(center, r, r);
    } else if (state == State::ReadOnly) {
        // Un candado
        const qreal w = s * 0.42;
        const QRectF body(s - w - s * 0.02, s - w * 0.72 - s * 0.02, w, w * 0.72);
        const QColor lock = QApplication::palette().color(QPalette::PlaceholderText);
        p.setPen(QPen(QApplication::palette().color(QPalette::Window), s / 16.0 + 0.5));
        p.setBrush(lock);
        p.drawRoundedRect(body, s * 0.04, s * 0.04);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(lock, std::max(1.0, s / 14.0)));
        const qreal shackle = w * 0.56;
        p.drawArc(QRectF(body.center().x() - shackle / 2, body.top() - shackle * 0.62, shackle, shackle * 1.1), 0, 180 * 16);
    }
    return pixmap;
}

QHash<QString, QIcon>& cache()
{
    static QHash<QString, QIcon> instance;
    return instance;
}

} // namespace

FileType typeOf(const QString& fileName)
{
    const QString name = QFileInfo(fileName).fileName().toLower();
    if (const auto it = names().constFind(name); it != names().constEnd())
        return {QString::fromLatin1(it->label), QColor::fromRgb(it->color)};
    const QString suffix = QFileInfo(name).suffix();
    if (const auto it = extensions().constFind(suffix); it != extensions().constEnd())
        return {QString::fromLatin1(it->label), QColor::fromRgb(it->color)};
    return {QString(), QColor::fromRgb(0x7D8590)};
}

QIcon forFile(const QString& fileName, State state)
{
    const bool dark = appearance::isDarkApplied();
    const FileType type = typeOf(fileName);
    const QString key = type.label + QLatin1Char('|') + type.color.name() + QLatin1Char('|')
                      + QString::number(static_cast<int>(state)) + (dark ? QStringLiteral("|d") : QStringLiteral("|l"));
    if (const auto it = cache().constFind(key); it != cache().constEnd())
        return *it;
    QIcon icon;
    for (const int size : {16, 20, 24, 32, 48})
        icon.addPixmap(draw(type, state, size, dark));
    cache().insert(key, icon);
    return icon;
}

void setEnabled(bool enabled)
{
    g_enabled = enabled;
}

bool enabled()
{
    return g_enabled;
}

void clearCache()
{
    cache().clear();
}

} // namespace edita::fileicons
