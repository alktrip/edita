#!/usr/bin/env python3
# This file is part of Edita.
# Copyright (C) 2026 Edita contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Genera data/themes/Edita Light.xml y Edita Dark.xml a partir de
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

# Colores de sintaxis de N++ (RRGGBB) → paleta clara y oscura
SYNTAX = {
    #            claro     oscuro
    '000000': ('1F2328', 'E4E6E9'),  # texto
    '0000FF': ('CF222E', 'FF7B72'),  # palabras clave
    '8000FF': ('8250DF', 'D2A8FF'),  # tipos
    '008000': ('6E7781', '8B949E'),  # comentarios
    '007F00': ('6E7781', '8B949E'),
    '808080': ('0A3069', 'A5D6FF'),  # cadenas
    'FF8000': ('0550AE', '79C0FF'),  # números
    '804000': ('953800', 'FFA657'),  # preprocesador
    '000080': ('1F2328', 'E4E6E9'),  # operadores
    '00007F': ('0550AE', '79C0FF'),
    'FF0000': ('CF222E', 'FF7B72'),
    '008080': ('116329', '7EE787'),  # atributos, etiquetas
    '007F7F': ('116329', '7EE787'),
    '7F007F': ('8250DF', 'D2A8FF'),
    '800000': ('82071E', 'FFA198'),
    '7F0000': ('82071E', 'FFA198'),
    '0080FF': ('0550AE', '79C0FF'),
    '0080C0': ('0550AE', '79C0FF'),
    '8080FF': ('6639BA', 'BC8CFF'),
    'FF8040': ('953800', 'FFA657'),
    '7F7F00': ('7D4E00', 'E3B341'),
    '808000': ('7D4E00', 'E3B341'),
}

# Estilos globales (WidgetStyle) por nombre: (fg, bg) claro y oscuro; None = sin cambiar
GLOBAL = {
    'Default Style':                    (('1F2328', 'FFFFFF'), ('E4E6E9', '1E1F22')),
    'Indent guideline style':           (('D8DCE1', 'FFFFFF'), ('373A40', '1E1F22')),
    'Brace highlight style':            (('1F2328', 'D3F0D9'), ('E4E6E9', '234D2E')),
    'Bad brace colour':                 (('CF222E', 'FFEBE9'), ('FF7B72', '4A1F22')),
    'Current line background colour':   ((None, 'F3F6FA'), (None, '26282C')),
    'Selected text colour':             (('1F2328', 'D3F0D9'), ('E4E6E9', '2D4A35')),
    'Multi-selected text color':        ((None, 'D3F0D9'), (None, '2D4A35')),
    'Caret colour':                     (('1F2328', None), ('E4E6E9', None)),
    'Multi-edit carets color':          (('57606A', None), ('A3A9B1', None)),
    'Edge colour':                      (('D8DCE1', None), ('373A40', None)),
    'Line number margin':               (('8C959F', 'FFFFFF'), ('6B727B', '1E1F22')),
    'Bookmark margin':                  ((None, 'FFFFFF'), (None, '1E1F22')),
    'Change History margin':            ((None, 'FFFFFF'), (None, '1E1F22')),
    'Fold':                             (('8C959F', 'FFFFFF'), ('6B727B', '1E1F22')),
    'Fold active':                      (('2F9E44', None), ('3FB950', None)),
    'Fold margin':                      (('FFFFFF', 'FFFFFF'), ('1E1F22', '1E1F22')),
    'White space symbol':               (('C4C9CF', None), ('4A4E55', None)),
    'Smart Highlighting':               ((None, '9BE3A9'), (None, '2F6B3C')),
    'Find Mark Style':                  ((None, 'FFB86C'), (None, 'A0612A')),
    'Incremental highlight all':        ((None, '54AEFF'), (None, '1F6FEB')),
    'Tags match highlighting':          ((None, 'C297FF'), (None, '6E40C9')),
    'Tags attribute':                   ((None, 'FFE16B'), (None, '9E6A03')),
    'URL hovered':                      (('0969DA', None), ('58A6FF', None)),
    'Document map':                     (('2F9E44', 'FFFFFF'), ('3FB950', '1E1F22')),
    'EOL custom color':                 (('C4C9CF', None), ('4A4E55', None)),
    'Non-printing characters custom color': (('C4C9CF', None), ('4A4E55', None)),
    'Active tab focused indicator':     (('2F9E44', None), ('3FB950', None)),
    'Active tab text':                  (('1F2328', None), ('E4E6E9', None)),
    'Inactive tabs':                    (('57606A', 'F5F6F8'), ('A3A9B1', '1E1F22')),
}


def hls(color):
    r, g, b = (int(color[i:i + 2], 16) / 255 for i in (0, 2, 4))
    return colorsys.rgb_to_hls(r, g, b)


def from_hls(h, l, s):
    r, g, b = colorsys.hls_to_rgb(h, max(0, min(1, l)), max(0, min(1, s)))
    return '%02X%02X%02X' % (round(r * 255), round(g * 255), round(b * 255))


def foreground(color, dark):
    color = color.upper()
    if color in SYNTAX:
        return SYNTAX[color][1 if dark else 0]
    h, l, s = hls(color)
    s *= 0.8  # menos saturado que los de N++
    if dark:  # colores claros sobre fondo oscuro
        return from_hls(h, max(0.62, 1 - l * 0.55), s)
    return from_hls(h, min(l, 0.42), s)


def background(color, dark):
    color = color.upper()
    if color == 'FFFFFF':
        return '1E1F22' if dark else 'FFFFFF'
    h, l, s = hls(color)
    if dark:
        if l > 0.85:  # fondos claros de N++ (realce): oscuros y suaves
            return from_hls(h, 0.18, s * 0.5)
        return from_hls(h, min(l, 0.30), s * 0.6)
    if l > 0.85:
        return color
    return from_hls(h, max(l, 0.88), s * 0.6)


def make(dark):
    tree = ET.parse(os.path.join(DATA, 'stylers.model.xml'))
    root = tree.getroot()
    for node in root.iter('WordsStyle'):
        if node.get('fgColor'):
            node.set('fgColor', foreground(node.get('fgColor'), dark))
        if node.get('bgColor'):
            node.set('bgColor', background(node.get('bgColor'), dark))
    for node in root.iter('WidgetStyle'):
        name = node.get('name')
        if name in GLOBAL:
            fg, bg = GLOBAL[name][1 if dark else 0]
            if fg:
                node.set('fgColor', fg)
            if bg:
                node.set('bgColor', bg)
        if name in ('Default Style', 'Global override'):
            node.set('fontName', FONT)
    name = 'Edita Dark' if dark else 'Edita Light'
    ET.indent(tree, space='    ')
    with open(os.path.join(DATA, 'themes', name + '.xml'), 'wb') as out:
        out.write(b'<?xml version="1.0" encoding="UTF-8" ?>\n')
        out.write(('<!-- %s: tema de Edita generado por tools/make-edita-themes.py a partir de\n'
                   '     stylers.model.xml de Notepad++ (GPL v3). -->\n' % name).encode())
        tree.write(out, encoding='utf-8', xml_declaration=False)
    print('escrito', name)


if __name__ == '__main__':
    make(False)
    make(True)
    sys.exit(0)
