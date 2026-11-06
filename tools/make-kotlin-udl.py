#!/usr/bin/env python3
# This file is part of Edita.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Genera el lenguaje Kotlin preinstalado (data/userDefineLangs/kotlin._preinstalled*.udl.xml),
# en claro y en oscuro, como los Markdown preinstalados de N++. Ni N++ ni Lexilla traen Kotlin:
# es un lenguaje definido por el usuario (el lexer «user» de N++), así que el archivo también
# sirve en Notepad++. Los colores son los de C++ en los temas Edita Light y Edita Dark.
#
#   tools/make-kotlin-udl.py

import os
from xml.sax.saxutils import escape

OUT = os.path.join(os.path.dirname(__file__), '..', 'data', 'userDefineLangs')

# Palabras reservadas (https://kotlinlang.org/docs/keyword-reference.html)
HARD = ('as break class continue do else false for fun if in interface is null object package '
        'return super this throw true try typealias typeof val var when while')
SOFT = ('by catch constructor delegate dynamic field file finally get import init param property '
        'receiver set setparam value where actual abstract annotation companion const crossinline '
        'data enum expect external final infix inline inner internal lateinit noinline open operator '
        'out override private protected public reified sealed suspend tailrec vararg')
TYPES = ('Any Unit Nothing String CharSequence Int Long Short Byte UInt ULong UShort UByte Double Float '
         'Boolean Char Number Comparable Array IntArray LongArray ShortArray ByteArray DoubleArray '
         'FloatArray BooleanArray CharArray List MutableList ArrayList Map MutableMap HashMap '
         'LinkedHashMap Set MutableSet HashSet Collection MutableCollection Iterable Sequence Pair '
         'Triple Result Lazy Regex Throwable Exception RuntimeException Error')

# Las cadenas: primero la cruda («"""», sin escapes ni fin de línea), después la normal, el
# carácter y los nombres entre acentos graves (`nombre con espacios`)
DELIMITERS = ('00""" 01 02""" 03" 04\\ 05((EOL ")) 06\' 07\\ 08((EOL \')) 09` 10 11((EOL `)) '
              '12 13 14 15 16 17 18 19 20 21 22 23')

COLORS = {
    'light': dict(comment='6E7781', doc='116329', keyword='CF222E', type='8250DF', number='0550AE',
                  string='0A3069', annotation='953800', operator='1F2328', text='1F2328', bg='FFFFFF'),
    'dark': dict(comment='8B949E', doc='7EE787', keyword='FF7B72', type='D2A8FF', number='79C0FF',
                 string='A5D6FF', annotation='FFA657', operator='E4E6E9', text='E4E6E9', bg='1E1F22'),
}


def styles(c):
    # colorStyle 0: los colores del tema en uso (el estilo por defecto); 1: solo el del texto,
    # con el fondo del tema. fontStyle: 1 negrita, 2 cursiva
    rows = [
        ('DEFAULT', c['text'], 0, 0), ('COMMENTS', c['comment'], 1, 2), ('LINE COMMENTS', c['comment'], 1, 2),
        ('NUMBERS', c['number'], 1, 0), ('KEYWORDS1', c['keyword'], 1, 1), ('KEYWORDS2', c['keyword'], 1, 0),
        ('KEYWORDS3', c['type'], 1, 0), ('KEYWORDS4', c['annotation'], 1, 0), ('KEYWORDS5', c['text'], 0, 0),
        ('KEYWORDS6', c['text'], 0, 0), ('KEYWORDS7', c['text'], 0, 0), ('KEYWORDS8', c['text'], 0, 0),
        ('OPERATORS', c['operator'], 1, 1), ('FOLDER IN CODE1', c['operator'], 1, 1),
        ('FOLDER IN CODE2', c['text'], 0, 0), ('FOLDER IN COMMENT', c['doc'], 1, 2),
        ('DELIMITERS1', c['string'], 1, 0), ('DELIMITERS2', c['string'], 1, 0), ('DELIMITERS3', c['string'], 1, 0),
        ('DELIMITERS4', c['text'], 1, 0), ('DELIMITERS5', c['text'], 0, 0), ('DELIMITERS6', c['text'], 0, 0),
        ('DELIMITERS7', c['text'], 0, 0), ('DELIMITERS8', c['text'], 0, 0),
    ]
    return '\n'.join(
        f'            <WordsStyle name="{n}" fgColor="{fg}" bgColor="{c["bg"]}" colorStyle="{cs}" '
        f'fontName="" fontStyle="{fs}" nesting="0" />' for n, fg, cs, fs in rows)


def udl(name, dark, c):
    kw = {
        # Sin cierre: el comentario de línea acaba en el fin de línea. Con «02((EOL))», LexUser se come
        # el salto antes de calcular el plegado y la línea siguiente pliega una línea antes
        'Comments': '00// 01 02 03/* 04*/',
        # LexUser: prefix2 admite los «extras1» (dígitos hexadecimales y _, en 0xFF_FF) y los
        # sufijos de suffix2 van detrás de cualquier número (suffix1, solo sin prefijo). Un decimal
        # con _ (1_000) no se puede: en LexUser, los extras de un número sin prefijo exigen sufijo
        'Numbers, prefix1': '', 'Numbers, prefix2': '0x 0X 0b 0B',
        'Numbers, extras1': 'a b c d e f A B C D E F _', 'Numbers, extras2': '',
        'Numbers, suffix1': '', 'Numbers, suffix2': 'L f F u U uL UL', 'Numbers, range': '',
        'Operators1': '( ) [ ] . , ; : ? ! = + - * / % < > & | ^ ~',
        'Operators2': '',
        'Folders in code1, open': '{', 'Folders in code1, middle': '', 'Folders in code1, close': '}',
        'Folders in code2, open': '', 'Folders in code2, middle': '', 'Folders in code2, close': '',
        'Folders in comment, open': 'region', 'Folders in comment, middle': '',
        'Folders in comment, close': 'endregion',
        'Keywords1': HARD, 'Keywords2': SOFT, 'Keywords3': TYPES, 'Keywords4': '@',
        'Keywords5': '', 'Keywords6': '', 'Keywords7': '', 'Keywords8': '',
        'Delimiters': DELIMITERS,
    }
    lists = '\n'.join(f'            <Keywords name="{k}">{escape(v)}</Keywords>' for k, v in kw.items())
    dm = ' darkModeTheme="yes"' if dark else ''
    return f'''<?xml version="1.0" encoding="UTF-8" ?>
<!--
  Kotlin para Edita (y Notepad++): un lenguaje definido por el usuario, porque ni N++ ni Lexilla
  traen Kotlin. Generado por tools/make-kotlin-udl.py: no editar a mano.
  Copyright (C) 2026 Edita contributors. GPL-3.0-or-later.
-->
<NotepadPlus>
    <UserLang name="{name}" ext="kt kts"{dm} udlVersion="2.1">
        <Settings>
            <Global caseIgnored="no" allowFoldOfComments="yes" foldCompact="no" forcePureLC="0" decimalSeparator="0" />
            <Prefix Keywords1="no" Keywords2="no" Keywords3="no" Keywords4="yes" Keywords5="no" Keywords6="no" Keywords7="no" Keywords8="no" />
        </Settings>
        <KeywordLists>
{lists}
        </KeywordLists>
        <Styles>
{styles(c)}
        </Styles>
    </UserLang>
</NotepadPlus>
'''


for suffix, name, dark in (('', 'Kotlin (preinstalled)', False), ('_DM', 'Kotlin (preinstalled dark mode)', True)):
    path = os.path.join(OUT, f'kotlin._preinstalled{suffix}.udl.xml')
    with open(path, 'w', encoding='utf-8') as f:
        f.write(udl(name, dark, COLORS['dark' if dark else 'light']))
    print(path)
