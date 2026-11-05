// This file is part of Edita.
// Copyright (C) 2026 Edita contributors
//
// Edita is derived from Notepad++ (Copyright (C) Don HO and Notepad++ team).
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "ui/Icons.h"

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QHash>
#include <QIconEngine>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QSvgRenderer>
#include <QWidget>

#include <algorithm>

namespace edita::icons {

namespace {

const char* const kIconProperty = "editaIcon";

QByteArray svgSource(const QString& name)
{
    static QHash<QString, QByteArray> cache;
    auto it = cache.find(name);
    if (it == cache.end()) {
        QFile file(QStringLiteral(":/edita/icons/ui/%1.svg").arg(name));
        it = cache.insert(name, file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray());
    }
    return *it;
}

QPixmap render(const QByteArray& svg, const QColor& color, int size, qreal ratio)
{
    QByteArray colored = svg;
    colored.replace("currentColor", color.name(QColor::HexRgb).toLatin1());
    QSvgRenderer renderer(colored);
    QPixmap pixmap(QSize(size, size) * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setOpacity(color.alphaF());
    renderer.render(&painter, QRectF(0, 0, size, size));
    return pixmap;
}

// Un icono que se pinta con la paleta del momento: si el tema cambia (claro u oscuro, o el
// del sistema), los iconos ya puestos en árboles, botones y pestañas cambian con él al
// volver a dibujarse, sin tener que crearlos de nuevo
class PaletteIconEngine final : public QIconEngine
{
public:
    explicit PaletteIconEngine(QString name) : _name(std::move(name)) {}

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override
    {
        const qreal ratio = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
        painter->drawPixmap(rect, scaledPixmap(rect.size(), mode, state, ratio));
    }
    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }
    QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State, qreal scale) override
    {
        const QPalette palette = QApplication::palette();
        const QColor color = mode == QIcon::Disabled ? palette.color(QPalette::Disabled, QPalette::WindowText)
                                                     : palette.color(QPalette::Active, QPalette::WindowText);
        const int side = std::max(1, std::min(size.width(), size.height()));
        const QString key = QStringLiteral("%1|%2|%3|%4").arg(_name, color.name(QColor::HexArgb)).arg(side).arg(scale);
        static QHash<QString, QPixmap> cache;
        if (const auto it = cache.constFind(key); it != cache.constEnd())
            return *it;
        const QPixmap pixmap = render(svgSource(_name), color, side, scale);
        cache.insert(key, pixmap);
        return pixmap;
    }
    QSize actualSize(const QSize& size, QIcon::Mode, QIcon::State) override
    {
        const int side = std::min(size.width(), size.height());
        return QSize(side, side);
    }
    QIconEngine* clone() const override { return new PaletteIconEngine(_name); }
    QString key() const override { return QStringLiteral("edita-palette"); }
    QString iconName() override { return _name; }
    bool isNull() override { return svgSource(_name).isEmpty(); }

private:
    QString _name;
};

} // namespace

QIcon icon(const QString& name, const QColor& color)
{
    const QByteArray svg = svgSource(name);
    if (svg.isEmpty())
        return {};
    static QHash<QString, QIcon> cache;
    const QString key = name + color.name(QColor::HexArgb);
    if (const auto it = cache.constFind(key); it != cache.constEnd())
        return *it;
    QIcon result;
    const QColor disabled = QApplication::palette().color(QPalette::Disabled, QPalette::WindowText);
    for (const int size : {16, 20, 24, 32, 48}) {
        result.addPixmap(render(svg, color, size, 1.0), QIcon::Normal);
        result.addPixmap(render(svg, color, size, 2.0), QIcon::Normal);
        result.addPixmap(render(svg, disabled, size, 1.0), QIcon::Disabled);
    }
    cache.insert(key, result);
    return result;
}

QIcon icon(const QString& name)
{
    if (svgSource(name).isEmpty())
        return {};
    return QIcon(new PaletteIconEngine(name));
}

void setIcon(QAction* action, const QString& name)
{
    action->setProperty(kIconProperty, name);
    action->setIcon(icon(name));
}

QString iconName(const QAction* action)
{
    return action->property(kIconProperty).toString();
}

void refresh(QWidget* root)
{
    for (QAction* action : root->findChildren<QAction*>()) {
        const QString name = iconName(action);
        if (!name.isEmpty())
            action->setIcon(icon(name));
    }
}

} // namespace edita::icons
