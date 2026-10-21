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

#include "ui/Appearance.h"

#include <QApplication>
#include <QStyleFactory>
#include <QStyleHints>

namespace edita::appearance {

namespace {

// Colores de cada modo; la hoja de estilos se arma con ellos
struct Colors
{
    QColor window, base, alternate, text, muted, disabled, border, hover, pressed, accent, accentText, selection,
        scroll, scrollHover, tooltip, link;
};

const Colors& colors(bool dark)
{
    static const Colors light{
        QColor(0xf5, 0xf6, 0xf8), QColor(0xff, 0xff, 0xff), QColor(0xf0, 0xf2, 0xf4), QColor(0x1f, 0x23, 0x28),
        QColor(0x57, 0x60, 0x6a), QColor(0x9a, 0xa1, 0xa9), QColor(0xd8, 0xdc, 0xe1), QColor(0xe8, 0xeb, 0xee),
        QColor(0xd9, 0xde, 0xe3), QColor(0x2f, 0x9e, 0x44), QColor(0xff, 0xff, 0xff), QColor(0xd3, 0xf0, 0xd9),
        QColor(0xc4, 0xc9, 0xcf), QColor(0x9a, 0xa1, 0xa9), QColor(0xff, 0xff, 0xff), QColor(0x09, 0x69, 0xda)};
    static const Colors darkColors{
        QColor(0x1e, 0x1f, 0x22), QColor(0x26, 0x27, 0x2b), QColor(0x2b, 0x2d, 0x31), QColor(0xe4, 0xe6, 0xe9),
        QColor(0xa3, 0xa9, 0xb1), QColor(0x6b, 0x72, 0x7b), QColor(0x37, 0x3a, 0x40), QColor(0x30, 0x33, 0x38),
        QColor(0x3b, 0x3f, 0x46), QColor(0x3f, 0xb9, 0x50), QColor(0x10, 0x12, 0x14), QColor(0x23, 0x4d, 0x2e),
        QColor(0x4a, 0x4e, 0x55), QColor(0x6b, 0x72, 0x7b), QColor(0x2b, 0x2d, 0x31), QColor(0x58, 0xa6, 0xff)};
    return dark ? darkColors : light;
}

bool& appliedDark()
{
    static bool dark = false;
    return dark;
}

} // namespace

bool isDark(const Preferences& prefs, Qt::ColorScheme scheme)
{
    if (prefs.darkModeFollowSystem && scheme != Qt::ColorScheme::Unknown)
        return scheme == Qt::ColorScheme::Dark;
    return prefs.darkModeEnabled;
}

Qt::ColorScheme systemScheme()
{
    return QGuiApplication::styleHints()->colorScheme();
}

QColor accent(bool dark)
{
    return colors(dark).accent;
}

QColor unsavedColor(bool dark)
{
    return dark ? QColor(0xe3, 0xb3, 0x41) : QColor(0xd2, 0x8b, 0x10);
}

QPalette palette(bool dark)
{
    const Colors& c = colors(dark);
    QPalette p;
    for (const QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        const bool disabled = group == QPalette::Disabled;
        p.setColor(group, QPalette::Window, c.window);
        p.setColor(group, QPalette::WindowText, disabled ? c.disabled : c.text);
        p.setColor(group, QPalette::Base, c.base);
        p.setColor(group, QPalette::AlternateBase, c.alternate);
        p.setColor(group, QPalette::Text, disabled ? c.disabled : c.text);
        p.setColor(group, QPalette::Button, dark ? c.alternate : c.base);
        p.setColor(group, QPalette::ButtonText, disabled ? c.disabled : c.text);
        p.setColor(group, QPalette::BrightText, Qt::white);
        p.setColor(group, QPalette::Highlight, disabled ? c.border : c.accent);
        p.setColor(group, QPalette::HighlightedText, c.accentText);
        p.setColor(group, QPalette::ToolTipBase, c.tooltip);
        p.setColor(group, QPalette::ToolTipText, c.text);
        p.setColor(group, QPalette::PlaceholderText, c.muted);
        p.setColor(group, QPalette::Link, c.link);
        p.setColor(group, QPalette::Light, dark ? c.pressed : c.base);
        p.setColor(group, QPalette::Midlight, c.hover);
        p.setColor(group, QPalette::Mid, c.border);
        p.setColor(group, QPalette::Dark, c.scroll);
        p.setColor(group, QPalette::Shadow, dark ? QColor(0, 0, 0) : QColor(0x8c, 0x95, 0x9f));
        p.setColor(group, QPalette::Accent, c.accent);
    }
    return p;
}

QString styleSheet(bool dark)
{
    const Colors& c = colors(dark);
    QString css = QStringLiteral(R"css(
QMainWindow::separator { background: @border; width: 1px; height: 1px; }
QMenuBar { background: @window; border: none; padding: 2px 4px; }
QMenuBar::item { background: transparent; padding: 4px 10px; border-radius: 6px; }
QMenuBar::item:selected { background: @hover; }
QMenuBar::item:pressed { background: @pressed; }
QMenu { background: @base; border: 1px solid @border; border-radius: 8px; padding: 5px; }
QMenu::item { padding: 5px 28px 5px 30px; border-radius: 5px; background: transparent; }
QMenu::item:selected { background: @selection; color: @text; }
QMenu::item:disabled { color: @disabled; }
QMenu::icon { padding-left: 8px; }
QMenu::separator { height: 1px; background: @border; margin: 4px 8px; }
QToolBar { background: @window; border: none; border-bottom: 1px solid @border; padding: 3px 6px; spacing: 2px; }
QToolBar::separator { background: @border; width: 1px; margin: 5px 6px; }
QToolButton { background: transparent; border: none; border-radius: 6px; padding: 4px; }
QToolButton:hover { background: @hover; }
QToolButton:pressed, QToolButton:checked { background: @pressed; }
QTabBar { background: @window; }
QTabBar::tab { background: transparent; color: @muted; border: none; border-bottom: 2px solid transparent;
               padding: 6px 10px; margin-right: 1px; }
QTabBar::tab:hover { background: @hover; color: @text; }
QTabBar::tab:selected { background: @base; color: @text; border-bottom: 2px solid @accent; }
QTabBar::close-button { image: url(:/edita/icons/ui/tab-close-@mode.svg); subcontrol-position: right; border-radius: 4px;
                        margin: 2px; }
QTabBar::close-button:hover { background: @pressed; }
QTabWidget::pane { border: 1px solid @border; border-radius: 6px; top: -1px; background: @window; }
QStatusBar { background: @window; border-top: 1px solid @border; color: @muted; }
QStatusBar::item { border: none; }
QStatusBar QLabel { color: @muted; padding: 2px 8px; }
QPushButton { background: @button; color: @text; border: 1px solid @border; border-radius: 6px; padding: 5px 14px;
              min-height: 18px; }
QPushButton:hover { background: @hover; }
QPushButton:pressed { background: @pressed; }
QPushButton:default { border: 1px solid @accent; }
QPushButton:disabled { color: @disabled; }
QListWidget#categories { background: @window; border: none; outline: none; padding: 4px; }
QListWidget#categories::item { padding: 6px 8px; border-radius: 6px; }
QListWidget#categories::item:hover { background: @hover; }
QListWidget#categories::item:selected { background: @selection; color: @text; }
QToolTip { background: @tooltip; color: @text; border: 1px solid @border; padding: 4px 6px; }
QScrollBar:vertical { background: transparent; width: 12px; margin: 0; }
QScrollBar:horizontal { background: transparent; height: 12px; margin: 0; }
QScrollBar::handle:vertical { background: @scroll; min-height: 28px; border-radius: 4px; margin: 2px 3px; }
QScrollBar::handle:horizontal { background: @scroll; min-width: 28px; border-radius: 4px; margin: 3px 2px; }
QScrollBar::handle:hover { background: @scrollHover; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; border: none; background: none; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }
)css");
    const std::pair<const char*, QColor> values[] = {
        {"@window", c.window},     {"@base", c.base},       {"@text", c.text},         {"@muted", c.muted},
        {"@disabled", c.disabled}, {"@border", c.border},   {"@hover", c.hover},       {"@pressed", c.pressed},
        {"@accent", c.accent},     {"@selection", c.selection}, {"@scrollHover", c.scrollHover},
        {"@scroll", c.scroll},     {"@tooltip", c.tooltip}, {"@button", dark ? c.alternate : c.base},
    };
    for (const auto& [name, color] : values)
        css.replace(QLatin1String(name), color.name());
    css.replace(QLatin1String("@mode"), dark ? QLatin1String("dark") : QLatin1String("light"));
    return css;
}

void apply(bool dark)
{
    // Fusion en todas partes: el mismo aspecto en cualquier escritorio (y en el AppImage).
    // Una vez: con la hoja de estilos, QApplication::style() ya no se llama "fusion"
    static bool fusion = false;
    if (!fusion) {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        fusion = true;
    }
    QApplication::setPalette(palette(dark));
    qApp->setStyleSheet(styleSheet(dark));
    appliedDark() = dark;
}

bool isDarkApplied()
{
    return appliedDark();
}

int toolbarIconSize(ToolbarIcons icons)
{
    switch (icons) {
    case ToolbarIcons::Small: return 16;
    case ToolbarIcons::Large: return 24;
    case ToolbarIcons::Standard: break;
    }
    return 20;
}

} // namespace edita::appearance
