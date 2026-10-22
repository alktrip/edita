#!/usr/bin/env python3
# This file is part of Edita.
# Copyright (C) 2026 Edita contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Genera data/themes/Edita Light.xml, Edita Dark.xml y los temas modernos (One Dark,
# Dracula, Catppuccin Mocha y Latte, Nord, Tokyo Night) a partir de
# data/stylers.model.xml (el de Notepad++), para que el editor combine con la
# interfaz de Edita (ui/Appearance): los estilos globales con su paleta y los
# colores de sintaxis de N++ (azul puro, verde, naranja…) cambiados por una paleta
# más suave, clara u oscura. Todos los lenguajes quedan cubiertos.
#
# Uso: tools/make-edita-themes.py   (desde editor/)

import colorsys
import os
import sys
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(HERE, '..', 'data')
FONT = 'Monospace'  # alias de fontconfig: la monoespaciada del sistema

# Colores de sintaxis de N++ (RRGGBB) → su papel en cada tema
SYNTAX = {
    '000000': 'text',
    '0000FF': 'keyword',
    '8000FF': 'type',
    '008000': 'comment', '007F00': 'comment',
    '808080': 'string',
    'FF8000': 'number', '00007F': 'number', '0080FF': 'number', '0080C0': 'number',
    '804000': 'preproc', 'FF8040': 'preproc',
    '000080': 'text',      # operadores
    'FF0000': 'keyword',
    '008080': 'attribute', '007F7F': 'attribute',  # atributos, etiquetas
    '7F007F': 'type',
    '800000': 'darkred', '7F0000': 'darkred',
    '8080FF': 'purple',
    '7F7F00': 'yellow', '808000': 'yellow',
}

# Cada tema: si es oscuro, los colores de sintaxis por papel y los de la interfaz
# del editor (fondo, márgenes, selección, marcas…)
THEMES = {
    'Edita Light': dict(dark=False,
        text='1F2328', keyword='CF222E', type='8250DF', comment='6E7781', string='0A3069', number='0550AE',
        preproc='953800', attribute='116329', darkred='82071E', purple='6639BA', yellow='7D4E00',
        bg='FFFFFF', guide='D8DCE1', brace='D3F0D9', bad=('CF222E', 'FFEBE9'), caretLine='F3F6FA', selection='D3F0D9',
        secondary='57606A', gutter='8C959F', accent='2F9E44', whitespace='C4C9CF', smart='9BE3A9', find='FFB86C',
        incremental='54AEFF', tagMatch='C297FF', tagAttribute='FFE16B', url='0969DA', inactiveTab='F5F6F8'),
    'Edita Dark': dict(dark=True,
        text='E4E6E9', keyword='FF7B72', type='D2A8FF', comment='8B949E', string='A5D6FF', number='79C0FF',
        preproc='FFA657', attribute='7EE787', darkred='FFA198', purple='BC8CFF', yellow='E3B341',
        bg='1E1F22', guide='373A40', brace='234D2E', bad=('FF7B72', '4A1F22'), caretLine='26282C', selection='2D4A35',
        secondary='A3A9B1', gutter='6B727B', accent='3FB950', whitespace='4A4E55', smart='2F6B3C', find='A0612A',
        incremental='1F6FEB', tagMatch='6E40C9', tagAttribute='9E6A03', url='58A6FF', inactiveTab='1E1F22'),
    # Inspirados en las paletas de esos temas (todas con licencia MIT)
    'One Dark': dict(dark=True,
        text='ABB2BF', keyword='C678DD', type='E5C07B', comment='7F848E', string='98C379', number='D19A66',
        preproc='56B6C2', attribute='E06C75', darkred='BE5046', purple='61AFEF', yellow='E5C07B',
        bg='282C34', guide='3B4048', brace='3E4A5E', bad=('E06C75', '4B2A30'), caretLine='2C313C', selection='3E4451',
        secondary='9DA5B4', gutter='5C6370', accent='61AFEF', whitespace='4B5263', smart='3D5A45', find='7A5A2A',
        incremental='2B5A8A', tagMatch='5A4A7A', tagAttribute='6A5A2A', url='61AFEF', inactiveTab='21252B'),
    'Dracula': dict(dark=True,
        text='F8F8F2', keyword='FF79C6', type='8BE9FD', comment='6272A4', string='F1FA8C', number='BD93F9',
        preproc='FFB86C', attribute='50FA7B', darkred='FF5555', purple='BD93F9', yellow='F1FA8C',
        bg='282A36', guide='3B3E4E', brace='44475A', bad=('FF5555', '4A2530'), caretLine='343746', selection='44475A',
        secondary='BFBFBF', gutter='6272A4', accent='BD93F9', whitespace='4D5168', smart='3B5E48', find='7A5A30',
        incremental='4A3F7A', tagMatch='5A3F7A', tagAttribute='6A6030', url='8BE9FD', inactiveTab='21222C'),
    'Catppuccin Mocha': dict(dark=True,
        text='CDD6F4', keyword='CBA6F7', type='F9E2AF', comment='7F849C', string='A6E3A1', number='FAB387',
        preproc='F5C2E7', attribute='94E2D5', darkred='F38BA8', purple='B4BEFE', yellow='F9E2AF',
        bg='1E1E2E', guide='313244', brace='45475A', bad=('F38BA8', '45263A'), caretLine='2A2B3C', selection='45475A',
        secondary='BAC2DE', gutter='6C7086', accent='CBA6F7', whitespace='45475A', smart='3A5A48', find='7A5838',
        incremental='3A4F7A', tagMatch='5A4A7A', tagAttribute='6A5F3A', url='89B4FA', inactiveTab='181825'),
    'Catppuccin Latte': dict(dark=False,
        text='4C4F69', keyword='8839EF', type='DF8E1D', comment='8C8FA1', string='40A02B', number='FE640B',
        preproc='EA76CB', attribute='179299', darkred='D20F39', purple='7287FD', yellow='DF8E1D',
        bg='EFF1F5', guide='CCD0DA', brace='CCD0DA', bad=('D20F39', 'F5D5DC'), caretLine='E6E9EF', selection='CCD0DA',
        secondary='6C6F85', gutter='8C8FA1', accent='8839EF', whitespace='BCC0CC', smart='C6E8BE', find='F9CFA5',
        incremental='B7C9F7', tagMatch='DCC8F7', tagAttribute='F3E0B5', url='1E66F5', inactiveTab='E6E9EF'),
    'Nord': dict(dark=True,
        text='D8DEE9', keyword='81A1C1', type='8FBCBB', comment='7B88A1', string='A3BE8C', number='B48EAD',
        preproc='5E81AC', attribute='88C0D0', darkred='BF616A', purple='B48EAD', yellow='EBCB8B',
        bg='2E3440', guide='3B4252', brace='434C5E', bad=('BF616A', '4A3338'), caretLine='3B4252', selection='434C5E',
        secondary='E5E9F0', gutter='4C566A', accent='88C0D0', whitespace='4C566A', smart='4A5E48', find='7A6A40',
        incremental='3F5A7A', tagMatch='5A4F6A', tagAttribute='6A6040', url='88C0D0', inactiveTab='2A2F3A'),
    'Tokyo Night': dict(dark=True,
        text='C0CAF5', keyword='BB9AF7', type='2AC3DE', comment='565F89', string='9ECE6A', number='FF9E64',
        preproc='7DCFFF', attribute='73DACA', darkred='F7768E', purple='7AA2F7', yellow='E0AF68',
        bg='1A1B26', guide='292E42', brace='33467C', bad=('F7768E', '3F2433'), caretLine='1F2335', selection='283457',
        secondary='A9B1D6', gutter='3B4261', accent='7AA2F7', whitespace='3B4261', smart='2F4A3A', find='6A4A28',
        incremental='2A3F7A', tagMatch='4A3A7A', tagAttribute='5A4A28', url='7AA2F7', inactiveTab='16161E'),
}


def global_styles(t):
    """Estilos globales (WidgetStyle) por nombre: (fg, bg); None = sin cambiar"""
    fg, bg = t['text'], t['bg']
    return {
        'Default Style': (fg, bg),
        'Indent guideline style': (t['guide'], bg),
        'Brace highlight style': (fg, t['brace']),
        'Bad brace colour': t['bad'],
        'Current line background colour': (None, t['caretLine']),
        'Selected text colour': (fg, t['selection']),
        'Multi-selected text color': (None, t['selection']),
        'Caret colour': (fg, None),
        'Multi-edit carets color': (t['secondary'], None),
        'Edge colour': (t['guide'], None),
        'Line number margin': (t['gutter'], bg),
        'Bookmark margin': (None, bg),
        'Change History margin': (None, bg),
        'Fold': (t['gutter'], bg),
        'Fold active': (t['accent'], None),
        'Fold margin': (bg, bg),
        'White space symbol': (t['whitespace'], None),
        'Smart Highlighting': (None, t['smart']),
        'Find Mark Style': (None, t['find']),
        'Incremental highlight all': (None, t['incremental']),
        'Tags match highlighting': (None, t['tagMatch']),
        'Tags attribute': (None, t['tagAttribute']),
        'URL hovered': (t['url'], None),
        'Document map': (t['accent'], bg),
        'EOL custom color': (t['whitespace'], None),
        'Non-printing characters custom color': (t['whitespace'], None),
        'Active tab focused indicator': (t['accent'], None),
        'Active tab text': (fg, None),
        'Inactive tabs': (t['secondary'], t['inactiveTab']),
    }


def hls(color):
    r, g, b = (int(color[i:i + 2], 16) / 255 for i in (0, 2, 4))
    return colorsys.rgb_to_hls(r, g, b)


def from_hls(h, l, s):
    r, g, b = colorsys.hls_to_rgb(h, max(0, min(1, l)), max(0, min(1, s)))
    return '%02X%02X%02X' % (round(r * 255), round(g * 255), round(b * 255))


def foreground(color, t):
    color = color.upper()
    if color in SYNTAX:
        return t[SYNTAX[color]]
    h, l, s = hls(color)
    s *= 0.8  # menos saturado que los de N++
    if t['dark']:  # colores claros sobre fondo oscuro
        return from_hls(h, max(0.62, 1 - l * 0.55), s)
    return from_hls(h, min(l, 0.42), s)


def background(color, t):
    color = color.upper()
    if color == 'FFFFFF':
        return t['bg']
    h, l, s = hls(color)
    if t['dark']:
        if l > 0.85:  # fondos claros de N++ (realce): oscuros y suaves
            return from_hls(h, 0.18, s * 0.5)
        return from_hls(h, min(l, 0.30), s * 0.6)
    if l > 0.85:
        return color
    return from_hls(h, max(l, 0.88), s * 0.6)


def make(name):
    t = THEMES[name]
    tree = ET.parse(os.path.join(DATA, 'stylers.model.xml'))
    root = tree.getroot()
    for node in root.iter('WordsStyle'):
        if node.get('fgColor'):
            node.set('fgColor', foreground(node.get('fgColor'), t))
        if node.get('bgColor'):
            node.set('bgColor', background(node.get('bgColor'), t))
    styles = global_styles(t)
    for node in root.iter('WidgetStyle'):
        style = node.get('name')
        if style in styles:
            fg, bg = styles[style]
            if fg:
                node.set('fgColor', fg)
            if bg:
                node.set('bgColor', bg)
        if style in ('Default Style', 'Global override'):
            node.set('fontName', FONT)
    ET.indent(tree, space='    ')
    origin = 'tema de Edita' if name.startswith('Edita') else 'tema inspirado en la paleta «%s» (MIT),' % name
    with open(os.path.join(DATA, 'themes', name + '.xml'), 'wb') as out:
        out.write(b'<?xml version="1.0" encoding="UTF-8" ?>\n')
        out.write(('<!-- %s: %s generado por tools/make-edita-themes.py a partir de\n'
                   '     stylers.model.xml de Notepad++ (GPL v3). -->\n' % (name, origin)).encode())
        tree.write(out, encoding='utf-8', xml_declaration=False)
    print('escrito', name)


if __name__ == '__main__':
    for theme in THEMES:
        make(theme)
    sys.exit(0)
