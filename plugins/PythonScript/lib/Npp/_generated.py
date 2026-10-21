# Generado por tools/gen-pythonscript.py a partir de Scintilla.iface y de las
# cabeceras de Notepad++. No editar a mano.
# SPDX-License-Identifier: GPL-3.0-or-later


class _Enum:
    """Constantes con nombre, como los enums de PythonScript (valores enteros)"""


def _bytes(value):
    if isinstance(value, bytes):
        return value
    return str(value).encode('utf-8')


def _str(value):
    return value.decode('utf-8', errors='replace')


def _rgb(c):
    if isinstance(c, int):
        return c
    return (int(c[0]) & 0xFF) | ((int(c[1]) & 0xFF) << 8) | ((int(c[2]) & 0xFF) << 16)


def _rgba(c):
    if isinstance(c, int):
        return c
    a = int(c[3]) if len(c) > 3 else 0xFF
    return _rgb(c) | ((a & 0xFF) << 24)


def _colour_tuple(v):
    return (v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF)


def _colouralpha_tuple(v):
    return (v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, (v >> 24) & 0xFF)


class ScintillaGenerated:
    """Métodos de Scintilla con nombres de Python (editor.getText(), editor.addText(text)…)"""

    def addText(self, text):
        """See Scintilla documentation for SCI_ADDTEXT"""
        _s = _bytes(text)
        self._sci_ls(2001, len(_s), _s)

    def addStyledText(self, c):
        """See Scintilla documentation for SCI_ADDSTYLEDTEXT"""
        _c = bytes(c)
        self._sci_ls(2002, len(_c), _c)

    def insertText(self, pos, text):
        """See Scintilla documentation for SCI_INSERTTEXT"""
        self._sci_ls(2003, int(pos), _bytes(text))

    def changeInsertion(self, text):
        """See Scintilla documentation for SCI_CHANGEINSERTION"""
        _s = _bytes(text)
        self._sci_ls(2672, len(_s), _s)

    def clearAll(self):
        """See Scintilla documentation for SCI_CLEARALL"""
        self._sci(2004, 0, 0)

    def deleteRange(self, start, lengthDelete):
        """See Scintilla documentation for SCI_DELETERANGE"""
        self._sci(2645, int(start), int(lengthDelete))

    def clearDocumentStyle(self):
        """See Scintilla documentation for SCI_CLEARDOCUMENTSTYLE"""
        self._sci(2005, 0, 0)

    def getLength(self):
        """See Scintilla documentation for SCI_GETLENGTH"""
        return self._sci(2006, 0, 0)

    def getCharAt(self, pos):
        """See Scintilla documentation for SCI_GETCHARAT"""
        return self._sci(2007, int(pos), 0)

    def getCurrentPos(self):
        """See Scintilla documentation for SCI_GETCURRENTPOS"""
        return self._sci(2008, 0, 0)

    def getAnchor(self):
        """See Scintilla documentation for SCI_GETANCHOR"""
        return self._sci(2009, 0, 0)

    def getStyleAt(self, pos):
        """See Scintilla documentation for SCI_GETSTYLEAT"""
        return self._sci(2010, int(pos), 0)

    def getStyleIndexAt(self, pos):
        """See Scintilla documentation for SCI_GETSTYLEINDEXAT"""
        return self._sci(2038, int(pos), 0)

    def redo(self):
        """See Scintilla documentation for SCI_REDO"""
        self._sci(2011, 0, 0)

    def setUndoCollection(self, collectUndo):
        """See Scintilla documentation for SCI_SETUNDOCOLLECTION"""
        self._sci(2012, (1 if collectUndo else 0), 0)

    def selectAll(self):
        """See Scintilla documentation for SCI_SELECTALL"""
        self._sci(2013, 0, 0)

    def setSavePoint(self):
        """See Scintilla documentation for SCI_SETSAVEPOINT"""
        self._sci(2014, 0, 0)

    def getStyledText(self, start, end):
        """See Scintilla documentation for SCI_GETSTYLEDTEXT"""
        return self._styled(start, end)

    def getStyledTextFull(self, start, end):
        """See Scintilla documentation for SCI_GETSTYLEDTEXTFULL"""
        return self._styled(start, end)

    def canRedo(self):
        """See Scintilla documentation for SCI_CANREDO"""
        return self._sci(2016, 0, 0) != 0

    def markerLineFromHandle(self, markerHandle):
        """See Scintilla documentation for SCI_MARKERLINEFROMHANDLE"""
        return self._sci(2017, int(markerHandle), 0)

    def markerDeleteHandle(self, markerHandle):
        """See Scintilla documentation for SCI_MARKERDELETEHANDLE"""
        self._sci(2018, int(markerHandle), 0)

    def markerHandleFromLine(self, line, which):
        """See Scintilla documentation for SCI_MARKERHANDLEFROMLINE"""
        return self._sci(2732, int(line), int(which))

    def markerNumberFromLine(self, line, which):
        """See Scintilla documentation for SCI_MARKERNUMBERFROMLINE"""
        return self._sci(2733, int(line), int(which))

    def getUndoCollection(self):
        """See Scintilla documentation for SCI_GETUNDOCOLLECTION"""
        return self._sci(2019, 0, 0) != 0

    def getViewWS(self):
        """See Scintilla documentation for SCI_GETVIEWWS"""
        return self._sci(2020, 0, 0)

    def setViewWS(self, viewWS):
        """See Scintilla documentation for SCI_SETVIEWWS"""
        self._sci(2021, int(viewWS), 0)

    def getTabDrawMode(self):
        """See Scintilla documentation for SCI_GETTABDRAWMODE"""
        return self._sci(2698, 0, 0)

    def setTabDrawMode(self, tabDrawMode):
        """See Scintilla documentation for SCI_SETTABDRAWMODE"""
        self._sci(2699, int(tabDrawMode), 0)

    def positionFromPoint(self, x, y):
        """See Scintilla documentation for SCI_POSITIONFROMPOINT"""
        return self._sci(2022, int(x), int(y))

    def positionFromPointClose(self, x, y):
        """See Scintilla documentation for SCI_POSITIONFROMPOINTCLOSE"""
        return self._sci(2023, int(x), int(y))

    def gotoLine(self, line):
        """See Scintilla documentation for SCI_GOTOLINE"""
        self._sci(2024, int(line), 0)

    def gotoPos(self, caret):
        """See Scintilla documentation for SCI_GOTOPOS"""
        self._sci(2025, int(caret), 0)

    def setAnchor(self, anchor):
        """See Scintilla documentation for SCI_SETANCHOR"""
        self._sci(2026, int(anchor), 0)

    def getCurLine(self):
        """See Scintilla documentation for SCI_GETCURLINE"""
        _n = self._sci(2027, 0, 0)
        return _str(self._sci_res(2027, _n + 1, _n))

    def getEndStyled(self):
        """See Scintilla documentation for SCI_GETENDSTYLED"""
        return self._sci(2028, 0, 0)

    def convertEOLs(self, eolMode):
        """See Scintilla documentation for SCI_CONVERTEOLS"""
        self._sci(2029, int(eolMode), 0)

    def getEOLMode(self):
        """See Scintilla documentation for SCI_GETEOLMODE"""
        return self._sci(2030, 0, 0)

    def setEOLMode(self, eolMode):
        """See Scintilla documentation for SCI_SETEOLMODE"""
        self._sci(2031, int(eolMode), 0)

    def startStyling(self, start, unused):
        """See Scintilla documentation for SCI_STARTSTYLING"""
        self._sci(2032, int(start), int(unused))

    def setStyling(self, length, style):
        """See Scintilla documentation for SCI_SETSTYLING"""
        self._sci(2033, int(length), int(style))

    def getBufferedDraw(self):
        """See Scintilla documentation for SCI_GETBUFFEREDDRAW"""
        return self._sci(2034, 0, 0) != 0

    def setBufferedDraw(self, buffered):
        """See Scintilla documentation for SCI_SETBUFFEREDDRAW"""
        self._sci(2035, (1 if buffered else 0), 0)

    def setTabWidth(self, tabWidth):
        """See Scintilla documentation for SCI_SETTABWIDTH"""
        self._sci(2036, int(tabWidth), 0)

    def getTabWidth(self):
        """See Scintilla documentation for SCI_GETTABWIDTH"""
        return self._sci(2121, 0, 0)

    def setTabMinimumWidth(self, pixels):
        """See Scintilla documentation for SCI_SETTABMINIMUMWIDTH"""
        self._sci(2724, int(pixels), 0)

    def getTabMinimumWidth(self):
        """See Scintilla documentation for SCI_GETTABMINIMUMWIDTH"""
        return self._sci(2725, 0, 0)

    def clearTabStops(self, line):
        """See Scintilla documentation for SCI_CLEARTABSTOPS"""
        self._sci(2675, int(line), 0)

    def addTabStop(self, line, x):
        """See Scintilla documentation for SCI_ADDTABSTOP"""
        self._sci(2676, int(line), int(x))

    def getNextTabStop(self, line, x):
        """See Scintilla documentation for SCI_GETNEXTTABSTOP"""
        return self._sci(2677, int(line), int(x))

    def setCodePage(self, codePage):
        """See Scintilla documentation for SCI_SETCODEPAGE"""
        self._sci(2037, int(codePage), 0)

    def setFontLocale(self, localeName):
        """See Scintilla documentation for SCI_SETFONTLOCALE"""
        self._sci_ls(2760, 0, _bytes(localeName))

    def getFontLocale(self):
        """See Scintilla documentation for SCI_GETFONTLOCALE"""
        _n = self._sci(2761, 0, 0)
        return _str(self._sci_res(2761, 0, _n))

    def getIMEInteraction(self):
        """See Scintilla documentation for SCI_GETIMEINTERACTION"""
        return self._sci(2678, 0, 0)

    def setIMEInteraction(self, imeInteraction):
        """See Scintilla documentation for SCI_SETIMEINTERACTION"""
        self._sci(2679, int(imeInteraction), 0)

    def markerDefine(self, markerNumber, markerSymbol):
        """See Scintilla documentation for SCI_MARKERDEFINE"""
        self._sci(2040, int(markerNumber), int(markerSymbol))

    def markerSetFore(self, markerNumber, fore):
        """See Scintilla documentation for SCI_MARKERSETFORE"""
        self._sci(2041, int(markerNumber), _rgb(fore))

    def markerSetBack(self, markerNumber, back):
        """See Scintilla documentation for SCI_MARKERSETBACK"""
        self._sci(2042, int(markerNumber), _rgb(back))

    def markerSetBackSelected(self, markerNumber, back):
        """See Scintilla documentation for SCI_MARKERSETBACKSELECTED"""
        self._sci(2292, int(markerNumber), _rgb(back))

    def markerSetForeTranslucent(self, markerNumber, fore):
        """See Scintilla documentation for SCI_MARKERSETFORETRANSLUCENT"""
        self._sci(2294, int(markerNumber), _rgba(fore))

    def markerSetBackTranslucent(self, markerNumber, back):
        """See Scintilla documentation for SCI_MARKERSETBACKTRANSLUCENT"""
        self._sci(2295, int(markerNumber), _rgba(back))

    def markerSetBackSelectedTranslucent(self, markerNumber, back):
        """See Scintilla documentation for SCI_MARKERSETBACKSELECTEDTRANSLUCENT"""
        self._sci(2296, int(markerNumber), _rgba(back))

    def markerSetStrokeWidth(self, markerNumber, hundredths):
        """See Scintilla documentation for SCI_MARKERSETSTROKEWIDTH"""
        self._sci(2297, int(markerNumber), int(hundredths))

    def markerEnableHighlight(self, enabled):
        """See Scintilla documentation for SCI_MARKERENABLEHIGHLIGHT"""
        self._sci(2293, (1 if enabled else 0), 0)

    def markerAdd(self, line, markerNumber):
        """See Scintilla documentation for SCI_MARKERADD"""
        return self._sci(2043, int(line), int(markerNumber))

    def markerDelete(self, line, markerNumber):
        """See Scintilla documentation for SCI_MARKERDELETE"""
        self._sci(2044, int(line), int(markerNumber))

    def markerDeleteAll(self, markerNumber):
        """See Scintilla documentation for SCI_MARKERDELETEALL"""
        self._sci(2045, int(markerNumber), 0)

    def markerGet(self, line):
        """See Scintilla documentation for SCI_MARKERGET"""
        return self._sci(2046, int(line), 0)

    def markerNext(self, lineStart, markerMask):
        """See Scintilla documentation for SCI_MARKERNEXT"""
        return self._sci(2047, int(lineStart), int(markerMask))

    def markerPrevious(self, lineStart, markerMask):
        """See Scintilla documentation for SCI_MARKERPREVIOUS"""
        return self._sci(2048, int(lineStart), int(markerMask))

    def markerDefinePixmap(self, markerNumber, pixmap):
        """See Scintilla documentation for SCI_MARKERDEFINEPIXMAP"""
        self._sci_ls(2049, int(markerNumber), _bytes(pixmap))

    def markerAddSet(self, line, markerSet):
        """See Scintilla documentation for SCI_MARKERADDSET"""
        self._sci(2466, int(line), int(markerSet))

    def markerSetAlpha(self, markerNumber, alpha):
        """See Scintilla documentation for SCI_MARKERSETALPHA"""
        self._sci(2476, int(markerNumber), int(alpha))

    def markerGetLayer(self, markerNumber):
        """See Scintilla documentation for SCI_MARKERGETLAYER"""
        return self._sci(2734, int(markerNumber), 0)

    def markerSetLayer(self, markerNumber, layer):
        """See Scintilla documentation for SCI_MARKERSETLAYER"""
        self._sci(2735, int(markerNumber), int(layer))

    def setMarginTypeN(self, margin, marginType):
        """See Scintilla documentation for SCI_SETMARGINTYPEN"""
        self._sci(2240, int(margin), int(marginType))

    def getMarginTypeN(self, margin):
        """See Scintilla documentation for SCI_GETMARGINTYPEN"""
        return self._sci(2241, int(margin), 0)

    def setMarginWidthN(self, margin, pixelWidth):
        """See Scintilla documentation for SCI_SETMARGINWIDTHN"""
        self._sci(2242, int(margin), int(pixelWidth))

    def getMarginWidthN(self, margin):
        """See Scintilla documentation for SCI_GETMARGINWIDTHN"""
        return self._sci(2243, int(margin), 0)

    def setMarginMaskN(self, margin, mask):
        """See Scintilla documentation for SCI_SETMARGINMASKN"""
        self._sci(2244, int(margin), int(mask))

    def getMarginMaskN(self, margin):
        """See Scintilla documentation for SCI_GETMARGINMASKN"""
        return self._sci(2245, int(margin), 0)

    def setMarginSensitiveN(self, margin, sensitive):
        """See Scintilla documentation for SCI_SETMARGINSENSITIVEN"""
        self._sci(2246, int(margin), (1 if sensitive else 0))

    def getMarginSensitiveN(self, margin):
        """See Scintilla documentation for SCI_GETMARGINSENSITIVEN"""
        return self._sci(2247, int(margin), 0) != 0

    def setMarginCursorN(self, margin, cursor):
        """See Scintilla documentation for SCI_SETMARGINCURSORN"""
        self._sci(2248, int(margin), int(cursor))

    def getMarginCursorN(self, margin):
        """See Scintilla documentation for SCI_GETMARGINCURSORN"""
        return self._sci(2249, int(margin), 0)

    def setMarginBackN(self, margin, back):
        """See Scintilla documentation for SCI_SETMARGINBACKN"""
        self._sci(2250, int(margin), _rgb(back))

    def getMarginBackN(self, margin):
        """See Scintilla documentation for SCI_GETMARGINBACKN"""
        return _colour_tuple(self._sci(2251, int(margin), 0))

    def setMargins(self, margins):
        """See Scintilla documentation for SCI_SETMARGINS"""
        self._sci(2252, int(margins), 0)

    def getMargins(self):
        """See Scintilla documentation for SCI_GETMARGINS"""
        return self._sci(2253, 0, 0)

    def styleClearAll(self):
        """See Scintilla documentation for SCI_STYLECLEARALL"""
        self._sci(2050, 0, 0)

    def styleSetFore(self, style, fore):
        """See Scintilla documentation for SCI_STYLESETFORE"""
        self._sci(2051, int(style), _rgb(fore))

    def styleSetBack(self, style, back):
        """See Scintilla documentation for SCI_STYLESETBACK"""
        self._sci(2052, int(style), _rgb(back))

    def styleSetBold(self, style, bold):
        """See Scintilla documentation for SCI_STYLESETBOLD"""
        self._sci(2053, int(style), (1 if bold else 0))

    def styleSetItalic(self, style, italic):
        """See Scintilla documentation for SCI_STYLESETITALIC"""
        self._sci(2054, int(style), (1 if italic else 0))

    def styleSetSize(self, style, sizePoints):
        """See Scintilla documentation for SCI_STYLESETSIZE"""
        self._sci(2055, int(style), int(sizePoints))

    def styleSetFont(self, style, fontName):
        """See Scintilla documentation for SCI_STYLESETFONT"""
        self._sci_ls(2056, int(style), _bytes(fontName))

    def styleSetEOLFilled(self, style, eolFilled):
        """See Scintilla documentation for SCI_STYLESETEOLFILLED"""
        self._sci(2057, int(style), (1 if eolFilled else 0))

    def styleResetDefault(self):
        """See Scintilla documentation for SCI_STYLERESETDEFAULT"""
        self._sci(2058, 0, 0)

    def styleSetUnderline(self, style, underline):
        """See Scintilla documentation for SCI_STYLESETUNDERLINE"""
        self._sci(2059, int(style), (1 if underline else 0))

    def styleGetFore(self, style):
        """See Scintilla documentation for SCI_STYLEGETFORE"""
        return _colour_tuple(self._sci(2481, int(style), 0))

    def styleGetBack(self, style):
        """See Scintilla documentation for SCI_STYLEGETBACK"""
        return _colour_tuple(self._sci(2482, int(style), 0))

    def styleGetBold(self, style):
        """See Scintilla documentation for SCI_STYLEGETBOLD"""
        return self._sci(2483, int(style), 0) != 0

    def styleGetItalic(self, style):
        """See Scintilla documentation for SCI_STYLEGETITALIC"""
        return self._sci(2484, int(style), 0) != 0

    def styleGetSize(self, style):
        """See Scintilla documentation for SCI_STYLEGETSIZE"""
        return self._sci(2485, int(style), 0)

    def styleGetFont(self, style):
        """See Scintilla documentation for SCI_STYLEGETFONT"""
        _n = self._sci(2486, int(style), 0)
        return _str(self._sci_res(2486, int(style), _n))

    def styleGetEOLFilled(self, style):
        """See Scintilla documentation for SCI_STYLEGETEOLFILLED"""
        return self._sci(2487, int(style), 0) != 0

    def styleGetUnderline(self, style):
        """See Scintilla documentation for SCI_STYLEGETUNDERLINE"""
        return self._sci(2488, int(style), 0) != 0

    def styleGetCase(self, style):
        """See Scintilla documentation for SCI_STYLEGETCASE"""
        return self._sci(2489, int(style), 0)

    def styleGetCharacterSet(self, style):
        """See Scintilla documentation for SCI_STYLEGETCHARACTERSET"""
        return self._sci(2490, int(style), 0)

    def styleGetVisible(self, style):
        """See Scintilla documentation for SCI_STYLEGETVISIBLE"""
        return self._sci(2491, int(style), 0) != 0

    def styleGetChangeable(self, style):
        """See Scintilla documentation for SCI_STYLEGETCHANGEABLE"""
        return self._sci(2492, int(style), 0) != 0

    def styleGetHotSpot(self, style):
        """See Scintilla documentation for SCI_STYLEGETHOTSPOT"""
        return self._sci(2493, int(style), 0) != 0

    def styleSetCase(self, style, caseVisible):
        """See Scintilla documentation for SCI_STYLESETCASE"""
        self._sci(2060, int(style), int(caseVisible))

    def styleSetSizeFractional(self, style, sizeHundredthPoints):
        """See Scintilla documentation for SCI_STYLESETSIZEFRACTIONAL"""
        self._sci(2061, int(style), int(sizeHundredthPoints))

    def styleGetSizeFractional(self, style):
        """See Scintilla documentation for SCI_STYLEGETSIZEFRACTIONAL"""
        return self._sci(2062, int(style), 0)

    def styleSetWeight(self, style, weight):
        """See Scintilla documentation for SCI_STYLESETWEIGHT"""
        self._sci(2063, int(style), int(weight))

    def styleGetWeight(self, style):
        """See Scintilla documentation for SCI_STYLEGETWEIGHT"""
        return self._sci(2064, int(style), 0)

    def styleSetCharacterSet(self, style, characterSet):
        """See Scintilla documentation for SCI_STYLESETCHARACTERSET"""
        self._sci(2066, int(style), int(characterSet))

    def styleSetHotSpot(self, style, hotspot):
        """See Scintilla documentation for SCI_STYLESETHOTSPOT"""
        self._sci(2409, int(style), (1 if hotspot else 0))

    def styleSetCheckMonospaced(self, style, checkMonospaced):
        """See Scintilla documentation for SCI_STYLESETCHECKMONOSPACED"""
        self._sci(2254, int(style), (1 if checkMonospaced else 0))

    def styleGetCheckMonospaced(self, style):
        """See Scintilla documentation for SCI_STYLEGETCHECKMONOSPACED"""
        return self._sci(2255, int(style), 0) != 0

    def styleSetStretch(self, style, stretch):
        """See Scintilla documentation for SCI_STYLESETSTRETCH"""
        self._sci(2258, int(style), int(stretch))

    def styleGetStretch(self, style):
        """See Scintilla documentation for SCI_STYLEGETSTRETCH"""
        return self._sci(2259, int(style), 0)

    def styleSetInvisibleRepresentation(self, style, representation):
        """See Scintilla documentation for SCI_STYLESETINVISIBLEREPRESENTATION"""
        self._sci_ls(2256, int(style), _bytes(representation))

    def styleGetInvisibleRepresentation(self, style):
        """See Scintilla documentation for SCI_STYLEGETINVISIBLEREPRESENTATION"""
        _n = self._sci(2257, int(style), 0)
        return _str(self._sci_res(2257, int(style), _n))

    def setElementColour(self, element, colourElement):
        """See Scintilla documentation for SCI_SETELEMENTCOLOUR"""
        self._sci(2753, int(element), _rgba(colourElement))

    def getElementColour(self, element):
        """See Scintilla documentation for SCI_GETELEMENTCOLOUR"""
        return _colouralpha_tuple(self._sci(2754, int(element), 0))

    def resetElementColour(self, element):
        """See Scintilla documentation for SCI_RESETELEMENTCOLOUR"""
        self._sci(2755, int(element), 0)

    def getElementIsSet(self, element):
        """See Scintilla documentation for SCI_GETELEMENTISSET"""
        return self._sci(2756, int(element), 0) != 0

    def getElementAllowsTranslucent(self, element):
        """See Scintilla documentation for SCI_GETELEMENTALLOWSTRANSLUCENT"""
        return self._sci(2757, int(element), 0) != 0

    def getElementBaseColour(self, element):
        """See Scintilla documentation for SCI_GETELEMENTBASECOLOUR"""
        return _colouralpha_tuple(self._sci(2758, int(element), 0))

    def setSelFore(self, useSetting, fore):
        """See Scintilla documentation for SCI_SETSELFORE"""
        self._sci(2067, (1 if useSetting else 0), _rgb(fore))

    def setSelBack(self, useSetting, back):
        """See Scintilla documentation for SCI_SETSELBACK"""
        self._sci(2068, (1 if useSetting else 0), _rgb(back))

    def getSelAlpha(self):
        """See Scintilla documentation for SCI_GETSELALPHA"""
        return self._sci(2477, 0, 0)

    def setSelAlpha(self, alpha):
        """See Scintilla documentation for SCI_SETSELALPHA"""
        self._sci(2478, int(alpha), 0)

    def getSelEOLFilled(self):
        """See Scintilla documentation for SCI_GETSELEOLFILLED"""
        return self._sci(2479, 0, 0) != 0

    def setSelEOLFilled(self, filled):
        """See Scintilla documentation for SCI_SETSELEOLFILLED"""
        self._sci(2480, (1 if filled else 0), 0)

    def getSelectionLayer(self):
        """See Scintilla documentation for SCI_GETSELECTIONLAYER"""
        return self._sci(2762, 0, 0)

    def setSelectionLayer(self, layer):
        """See Scintilla documentation for SCI_SETSELECTIONLAYER"""
        self._sci(2763, int(layer), 0)

    def getCaretLineLayer(self):
        """See Scintilla documentation for SCI_GETCARETLINELAYER"""
        return self._sci(2764, 0, 0)

    def setCaretLineLayer(self, layer):
        """See Scintilla documentation for SCI_SETCARETLINELAYER"""
        self._sci(2765, int(layer), 0)

    def getCaretLineHighlightSubLine(self):
        """See Scintilla documentation for SCI_GETCARETLINEHIGHLIGHTSUBLINE"""
        return self._sci(2773, 0, 0) != 0

    def setCaretLineHighlightSubLine(self, subLine):
        """See Scintilla documentation for SCI_SETCARETLINEHIGHLIGHTSUBLINE"""
        self._sci(2774, (1 if subLine else 0), 0)

    def setCaretFore(self, fore):
        """See Scintilla documentation for SCI_SETCARETFORE"""
        self._sci(2069, _rgb(fore), 0)

    def assignCmdKey(self, keyDefinition, sciCommand):
        """See Scintilla documentation for SCI_ASSIGNCMDKEY"""
        self._sci(2070, int(keyDefinition), int(sciCommand))

    def clearCmdKey(self, keyDefinition):
        """See Scintilla documentation for SCI_CLEARCMDKEY"""
        self._sci(2071, int(keyDefinition), 0)

    def clearAllCmdKeys(self):
        """See Scintilla documentation for SCI_CLEARALLCMDKEYS"""
        self._sci(2072, 0, 0)

    def setStylingEx(self, styles):
        """See Scintilla documentation for SCI_SETSTYLINGEX"""
        _s = _bytes(styles)
        self._sci_ls(2073, len(_s), _s)

    def styleSetVisible(self, style, visible):
        """See Scintilla documentation for SCI_STYLESETVISIBLE"""
        self._sci(2074, int(style), (1 if visible else 0))

    def getCaretPeriod(self):
        """See Scintilla documentation for SCI_GETCARETPERIOD"""
        return self._sci(2075, 0, 0)

    def setCaretPeriod(self, periodMilliseconds):
        """See Scintilla documentation for SCI_SETCARETPERIOD"""
        self._sci(2076, int(periodMilliseconds), 0)

    def setWordChars(self, characters):
        """See Scintilla documentation for SCI_SETWORDCHARS"""
        self._sci_ls(2077, 0, _bytes(characters))

    def getWordChars(self):
        """See Scintilla documentation for SCI_GETWORDCHARS"""
        _n = self._sci(2646, 0, 0)
        return _str(self._sci_res(2646, 0, _n))

    def setCharacterCategoryOptimization(self, countCharacters):
        """See Scintilla documentation for SCI_SETCHARACTERCATEGORYOPTIMIZATION"""
        self._sci(2720, int(countCharacters), 0)

    def getCharacterCategoryOptimization(self):
        """See Scintilla documentation for SCI_GETCHARACTERCATEGORYOPTIMIZATION"""
        return self._sci(2721, 0, 0)

    def beginUndoAction(self):
        """See Scintilla documentation for SCI_BEGINUNDOACTION"""
        self._sci(2078, 0, 0)

    def endUndoAction(self):
        """See Scintilla documentation for SCI_ENDUNDOACTION"""
        self._sci(2079, 0, 0)

    def getUndoSequence(self):
        """See Scintilla documentation for SCI_GETUNDOSEQUENCE"""
        return self._sci(2799, 0, 0)

    def getUndoActions(self):
        """See Scintilla documentation for SCI_GETUNDOACTIONS"""
        return self._sci(2790, 0, 0)

    def setUndoSavePoint(self, action):
        """See Scintilla documentation for SCI_SETUNDOSAVEPOINT"""
        self._sci(2791, int(action), 0)

    def getUndoSavePoint(self):
        """See Scintilla documentation for SCI_GETUNDOSAVEPOINT"""
        return self._sci(2792, 0, 0)

    def setUndoDetach(self, action):
        """See Scintilla documentation for SCI_SETUNDODETACH"""
        self._sci(2793, int(action), 0)

    def getUndoDetach(self):
        """See Scintilla documentation for SCI_GETUNDODETACH"""
        return self._sci(2794, 0, 0)

    def setUndoTentative(self, action):
        """See Scintilla documentation for SCI_SETUNDOTENTATIVE"""
        self._sci(2795, int(action), 0)

    def getUndoTentative(self):
        """See Scintilla documentation for SCI_GETUNDOTENTATIVE"""
        return self._sci(2796, 0, 0)

    def setUndoCurrent(self, action):
        """See Scintilla documentation for SCI_SETUNDOCURRENT"""
        self._sci(2797, int(action), 0)

    def getUndoCurrent(self):
        """See Scintilla documentation for SCI_GETUNDOCURRENT"""
        return self._sci(2798, 0, 0)

    def pushUndoActionType(self, type, pos):
        """See Scintilla documentation for SCI_PUSHUNDOACTIONTYPE"""
        self._sci(2800, int(type), int(pos))

    def changeLastUndoActionText(self, text):
        """See Scintilla documentation for SCI_CHANGELASTUNDOACTIONTEXT"""
        _s = _bytes(text)
        self._sci_ls(2801, len(_s), _s)

    def getUndoActionType(self, action):
        """See Scintilla documentation for SCI_GETUNDOACTIONTYPE"""
        return self._sci(2802, int(action), 0)

    def getUndoActionPosition(self, action):
        """See Scintilla documentation for SCI_GETUNDOACTIONPOSITION"""
        return self._sci(2803, int(action), 0)

    def getUndoActionText(self, action):
        """See Scintilla documentation for SCI_GETUNDOACTIONTEXT"""
        _n = self._sci(2804, int(action), 0)
        return _str(self._sci_res(2804, int(action), _n))

    def indicSetStyle(self, indicator, indicatorStyle):
        """See Scintilla documentation for SCI_INDICSETSTYLE"""
        self._sci(2080, int(indicator), int(indicatorStyle))

    def indicGetStyle(self, indicator):
        """See Scintilla documentation for SCI_INDICGETSTYLE"""
        return self._sci(2081, int(indicator), 0)

    def indicSetFore(self, indicator, fore):
        """See Scintilla documentation for SCI_INDICSETFORE"""
        self._sci(2082, int(indicator), _rgb(fore))

    def indicGetFore(self, indicator):
        """See Scintilla documentation for SCI_INDICGETFORE"""
        return _colour_tuple(self._sci(2083, int(indicator), 0))

    def indicSetUnder(self, indicator, under):
        """See Scintilla documentation for SCI_INDICSETUNDER"""
        self._sci(2510, int(indicator), (1 if under else 0))

    def indicGetUnder(self, indicator):
        """See Scintilla documentation for SCI_INDICGETUNDER"""
        return self._sci(2511, int(indicator), 0) != 0

    def indicSetHoverStyle(self, indicator, indicatorStyle):
        """See Scintilla documentation for SCI_INDICSETHOVERSTYLE"""
        self._sci(2680, int(indicator), int(indicatorStyle))

    def indicGetHoverStyle(self, indicator):
        """See Scintilla documentation for SCI_INDICGETHOVERSTYLE"""
        return self._sci(2681, int(indicator), 0)

    def indicSetHoverFore(self, indicator, fore):
        """See Scintilla documentation for SCI_INDICSETHOVERFORE"""
        self._sci(2682, int(indicator), _rgb(fore))

    def indicGetHoverFore(self, indicator):
        """See Scintilla documentation for SCI_INDICGETHOVERFORE"""
        return _colour_tuple(self._sci(2683, int(indicator), 0))

    def indicSetFlags(self, indicator, flags):
        """See Scintilla documentation for SCI_INDICSETFLAGS"""
        self._sci(2684, int(indicator), int(flags))

    def indicGetFlags(self, indicator):
        """See Scintilla documentation for SCI_INDICGETFLAGS"""
        return self._sci(2685, int(indicator), 0)

    def indicSetStrokeWidth(self, indicator, hundredths):
        """See Scintilla documentation for SCI_INDICSETSTROKEWIDTH"""
        self._sci(2751, int(indicator), int(hundredths))

    def indicGetStrokeWidth(self, indicator):
        """See Scintilla documentation for SCI_INDICGETSTROKEWIDTH"""
        return self._sci(2752, int(indicator), 0)

    def setWhitespaceFore(self, useSetting, fore):
        """See Scintilla documentation for SCI_SETWHITESPACEFORE"""
        self._sci(2084, (1 if useSetting else 0), _rgb(fore))

    def setWhitespaceBack(self, useSetting, back):
        """See Scintilla documentation for SCI_SETWHITESPACEBACK"""
        self._sci(2085, (1 if useSetting else 0), _rgb(back))

    def setWhitespaceSize(self, size):
        """See Scintilla documentation for SCI_SETWHITESPACESIZE"""
        self._sci(2086, int(size), 0)

    def getWhitespaceSize(self):
        """See Scintilla documentation for SCI_GETWHITESPACESIZE"""
        return self._sci(2087, 0, 0)

    def setLineState(self, line, state):
        """See Scintilla documentation for SCI_SETLINESTATE"""
        self._sci(2092, int(line), int(state))

    def getLineState(self, line):
        """See Scintilla documentation for SCI_GETLINESTATE"""
        return self._sci(2093, int(line), 0)

    def getMaxLineState(self):
        """See Scintilla documentation for SCI_GETMAXLINESTATE"""
        return self._sci(2094, 0, 0)

    def getCaretLineVisible(self):
        """See Scintilla documentation for SCI_GETCARETLINEVISIBLE"""
        return self._sci(2095, 0, 0) != 0

    def setCaretLineVisible(self, show):
        """See Scintilla documentation for SCI_SETCARETLINEVISIBLE"""
        self._sci(2096, (1 if show else 0), 0)

    def getCaretLineBack(self):
        """See Scintilla documentation for SCI_GETCARETLINEBACK"""
        return _colour_tuple(self._sci(2097, 0, 0))

    def setCaretLineBack(self, back):
        """See Scintilla documentation for SCI_SETCARETLINEBACK"""
        self._sci(2098, _rgb(back), 0)

    def getCaretLineFrame(self):
        """See Scintilla documentation for SCI_GETCARETLINEFRAME"""
        return self._sci(2704, 0, 0)

    def setCaretLineFrame(self, width):
        """See Scintilla documentation for SCI_SETCARETLINEFRAME"""
        self._sci(2705, int(width), 0)

    def styleSetChangeable(self, style, changeable):
        """See Scintilla documentation for SCI_STYLESETCHANGEABLE"""
        self._sci(2099, int(style), (1 if changeable else 0))

    def autoCShow(self, lengthEntered, itemList):
        """See Scintilla documentation for SCI_AUTOCSHOW"""
        self._sci_ls(2100, int(lengthEntered), _bytes(itemList))

    def autoCCancel(self):
        """See Scintilla documentation for SCI_AUTOCCANCEL"""
        self._sci(2101, 0, 0)

    def autoCActive(self):
        """See Scintilla documentation for SCI_AUTOCACTIVE"""
        return self._sci(2102, 0, 0) != 0

    def autoCPosStart(self):
        """See Scintilla documentation for SCI_AUTOCPOSSTART"""
        return self._sci(2103, 0, 0)

    def autoCComplete(self):
        """See Scintilla documentation for SCI_AUTOCCOMPLETE"""
        self._sci(2104, 0, 0)

    def autoCStops(self, characterSet):
        """See Scintilla documentation for SCI_AUTOCSTOPS"""
        self._sci_ls(2105, 0, _bytes(characterSet))

    def autoCSetSeparator(self, separatorCharacter):
        """See Scintilla documentation for SCI_AUTOCSETSEPARATOR"""
        self._sci(2106, int(separatorCharacter), 0)

    def autoCGetSeparator(self):
        """See Scintilla documentation for SCI_AUTOCGETSEPARATOR"""
        return self._sci(2107, 0, 0)

    def autoCSelect(self, select):
        """See Scintilla documentation for SCI_AUTOCSELECT"""
        self._sci_ls(2108, 0, _bytes(select))

    def autoCSetCancelAtStart(self, cancel):
        """See Scintilla documentation for SCI_AUTOCSETCANCELATSTART"""
        self._sci(2110, (1 if cancel else 0), 0)

    def autoCGetCancelAtStart(self):
        """See Scintilla documentation for SCI_AUTOCGETCANCELATSTART"""
        return self._sci(2111, 0, 0) != 0

    def autoCSetFillUps(self, characterSet):
        """See Scintilla documentation for SCI_AUTOCSETFILLUPS"""
        self._sci_ls(2112, 0, _bytes(characterSet))

    def autoCSetChooseSingle(self, chooseSingle):
        """See Scintilla documentation for SCI_AUTOCSETCHOOSESINGLE"""
        self._sci(2113, (1 if chooseSingle else 0), 0)

    def autoCGetChooseSingle(self):
        """See Scintilla documentation for SCI_AUTOCGETCHOOSESINGLE"""
        return self._sci(2114, 0, 0) != 0

    def autoCSetIgnoreCase(self, ignoreCase):
        """See Scintilla documentation for SCI_AUTOCSETIGNORECASE"""
        self._sci(2115, (1 if ignoreCase else 0), 0)

    def autoCGetIgnoreCase(self):
        """See Scintilla documentation for SCI_AUTOCGETIGNORECASE"""
        return self._sci(2116, 0, 0) != 0

    def userListShow(self, listType, itemList):
        """See Scintilla documentation for SCI_USERLISTSHOW"""
        self._sci_ls(2117, int(listType), _bytes(itemList))

    def autoCSetAutoHide(self, autoHide):
        """See Scintilla documentation for SCI_AUTOCSETAUTOHIDE"""
        self._sci(2118, (1 if autoHide else 0), 0)

    def autoCGetAutoHide(self):
        """See Scintilla documentation for SCI_AUTOCGETAUTOHIDE"""
        return self._sci(2119, 0, 0) != 0

    def autoCSetOptions(self, options):
        """See Scintilla documentation for SCI_AUTOCSETOPTIONS"""
        self._sci(2638, int(options), 0)

    def autoCGetOptions(self):
        """See Scintilla documentation for SCI_AUTOCGETOPTIONS"""
        return self._sci(2639, 0, 0)

    def autoCSetDropRestOfWord(self, dropRestOfWord):
        """See Scintilla documentation for SCI_AUTOCSETDROPRESTOFWORD"""
        self._sci(2270, (1 if dropRestOfWord else 0), 0)

    def autoCGetDropRestOfWord(self):
        """See Scintilla documentation for SCI_AUTOCGETDROPRESTOFWORD"""
        return self._sci(2271, 0, 0) != 0

    def registerImage(self, type, xpmData):
        """See Scintilla documentation for SCI_REGISTERIMAGE"""
        self._sci_ls(2405, int(type), _bytes(xpmData))

    def clearRegisteredImages(self):
        """See Scintilla documentation for SCI_CLEARREGISTEREDIMAGES"""
        self._sci(2408, 0, 0)

    def autoCGetTypeSeparator(self):
        """See Scintilla documentation for SCI_AUTOCGETTYPESEPARATOR"""
        return self._sci(2285, 0, 0)

    def autoCSetTypeSeparator(self, separatorCharacter):
        """See Scintilla documentation for SCI_AUTOCSETTYPESEPARATOR"""
        self._sci(2286, int(separatorCharacter), 0)

    def autoCSetMaxWidth(self, characterCount):
        """See Scintilla documentation for SCI_AUTOCSETMAXWIDTH"""
        self._sci(2208, int(characterCount), 0)

    def autoCGetMaxWidth(self):
        """See Scintilla documentation for SCI_AUTOCGETMAXWIDTH"""
        return self._sci(2209, 0, 0)

    def autoCSetMaxHeight(self, rowCount):
        """See Scintilla documentation for SCI_AUTOCSETMAXHEIGHT"""
        self._sci(2210, int(rowCount), 0)

    def autoCGetMaxHeight(self):
        """See Scintilla documentation for SCI_AUTOCGETMAXHEIGHT"""
        return self._sci(2211, 0, 0)

    def autoCSetStyle(self, style):
        """See Scintilla documentation for SCI_AUTOCSETSTYLE"""
        self._sci(2109, int(style), 0)

    def autoCGetStyle(self):
        """See Scintilla documentation for SCI_AUTOCGETSTYLE"""
        return self._sci(2120, 0, 0)

    def autoCSetImageScale(self, scalePercent):
        """See Scintilla documentation for SCI_AUTOCSETIMAGESCALE"""
        self._sci(2815, int(scalePercent), 0)

    def autoCGetImageScale(self):
        """See Scintilla documentation for SCI_AUTOCGETIMAGESCALE"""
        return self._sci(2816, 0, 0)

    def setIndent(self, indentSize):
        """See Scintilla documentation for SCI_SETINDENT"""
        self._sci(2122, int(indentSize), 0)

    def getIndent(self):
        """See Scintilla documentation for SCI_GETINDENT"""
        return self._sci(2123, 0, 0)

    def setUseTabs(self, useTabs):
        """See Scintilla documentation for SCI_SETUSETABS"""
        self._sci(2124, (1 if useTabs else 0), 0)

    def getUseTabs(self):
        """See Scintilla documentation for SCI_GETUSETABS"""
        return self._sci(2125, 0, 0) != 0

    def setLineIndentation(self, line, indentation):
        """See Scintilla documentation for SCI_SETLINEINDENTATION"""
        self._sci(2126, int(line), int(indentation))

    def getLineIndentation(self, line):
        """See Scintilla documentation for SCI_GETLINEINDENTATION"""
        return self._sci(2127, int(line), 0)

    def getLineIndentPosition(self, line):
        """See Scintilla documentation for SCI_GETLINEINDENTPOSITION"""
        return self._sci(2128, int(line), 0)

    def getColumn(self, pos):
        """See Scintilla documentation for SCI_GETCOLUMN"""
        return self._sci(2129, int(pos), 0)

    def countCharacters(self, start, end):
        """See Scintilla documentation for SCI_COUNTCHARACTERS"""
        return self._sci(2633, int(start), int(end))

    def countCodeUnits(self, start, end):
        """See Scintilla documentation for SCI_COUNTCODEUNITS"""
        return self._sci(2715, int(start), int(end))

    def setHScrollBar(self, visible):
        """See Scintilla documentation for SCI_SETHSCROLLBAR"""
        self._sci(2130, (1 if visible else 0), 0)

    def getHScrollBar(self):
        """See Scintilla documentation for SCI_GETHSCROLLBAR"""
        return self._sci(2131, 0, 0) != 0

    def setIndentationGuides(self, indentView):
        """See Scintilla documentation for SCI_SETINDENTATIONGUIDES"""
        self._sci(2132, int(indentView), 0)

    def getIndentationGuides(self):
        """See Scintilla documentation for SCI_GETINDENTATIONGUIDES"""
        return self._sci(2133, 0, 0)

    def setHighlightGuide(self, column):
        """See Scintilla documentation for SCI_SETHIGHLIGHTGUIDE"""
        self._sci(2134, int(column), 0)

    def getHighlightGuide(self):
        """See Scintilla documentation for SCI_GETHIGHLIGHTGUIDE"""
        return self._sci(2135, 0, 0)

    def getLineEndPosition(self, line):
        """See Scintilla documentation for SCI_GETLINEENDPOSITION"""
        return self._sci(2136, int(line), 0)

    def getCodePage(self):
        """See Scintilla documentation for SCI_GETCODEPAGE"""
        return self._sci(2137, 0, 0)

    def getCaretFore(self):
        """See Scintilla documentation for SCI_GETCARETFORE"""
        return _colour_tuple(self._sci(2138, 0, 0))

    def getReadOnly(self):
        """See Scintilla documentation for SCI_GETREADONLY"""
        return self._sci(2140, 0, 0) != 0

    def setCurrentPos(self, caret):
        """See Scintilla documentation for SCI_SETCURRENTPOS"""
        self._sci(2141, int(caret), 0)

    def setSelectionStart(self, anchor):
        """See Scintilla documentation for SCI_SETSELECTIONSTART"""
        self._sci(2142, int(anchor), 0)

    def getSelectionStart(self):
        """See Scintilla documentation for SCI_GETSELECTIONSTART"""
        return self._sci(2143, 0, 0)

    def setSelectionEnd(self, caret):
        """See Scintilla documentation for SCI_SETSELECTIONEND"""
        self._sci(2144, int(caret), 0)

    def getSelectionEnd(self):
        """See Scintilla documentation for SCI_GETSELECTIONEND"""
        return self._sci(2145, 0, 0)

    def setEmptySelection(self, caret):
        """See Scintilla documentation for SCI_SETEMPTYSELECTION"""
        self._sci(2556, int(caret), 0)

    def setPrintMagnification(self, magnification):
        """See Scintilla documentation for SCI_SETPRINTMAGNIFICATION"""
        self._sci(2146, int(magnification), 0)

    def getPrintMagnification(self):
        """See Scintilla documentation for SCI_GETPRINTMAGNIFICATION"""
        return self._sci(2147, 0, 0)

    def setPrintColourMode(self, mode):
        """See Scintilla documentation for SCI_SETPRINTCOLOURMODE"""
        self._sci(2148, int(mode), 0)

    def getPrintColourMode(self):
        """See Scintilla documentation for SCI_GETPRINTCOLOURMODE"""
        return self._sci(2149, 0, 0)

    def findText(self, searchFlags, start, end, ft):
        """See Scintilla documentation for SCI_FINDTEXT"""
        return self._find(int(searchFlags), start, end, ft)

    def findTextFull(self, searchFlags, start, end, ft):
        """See Scintilla documentation for SCI_FINDTEXTFULL"""
        return self._find(int(searchFlags), start, end, ft)

    def setChangeHistory(self, changeHistory):
        """See Scintilla documentation for SCI_SETCHANGEHISTORY"""
        self._sci(2780, int(changeHistory), 0)

    def getChangeHistory(self):
        """See Scintilla documentation for SCI_GETCHANGEHISTORY"""
        return self._sci(2781, 0, 0)

    def setUndoSelectionHistory(self, undoSelectionHistory):
        """See Scintilla documentation for SCI_SETUNDOSELECTIONHISTORY"""
        self._sci(2782, int(undoSelectionHistory), 0)

    def getUndoSelectionHistory(self):
        """See Scintilla documentation for SCI_GETUNDOSELECTIONHISTORY"""
        return self._sci(2783, 0, 0)

    def setSelectionSerialized(self, selectionString):
        """See Scintilla documentation for SCI_SETSELECTIONSERIALIZED"""
        self._sci_ls(2784, 0, _bytes(selectionString))

    def getSelectionSerialized(self):
        """See Scintilla documentation for SCI_GETSELECTIONSERIALIZED"""
        _n = self._sci(2785, 0, 0)
        return _str(self._sci_res(2785, 0, _n))

    def getFirstVisibleLine(self):
        """See Scintilla documentation for SCI_GETFIRSTVISIBLELINE"""
        return self._sci(2152, 0, 0)

    def getLine(self, line):
        """See Scintilla documentation for SCI_GETLINE"""
        return self._getLine(line)

    def getLineCount(self):
        """See Scintilla documentation for SCI_GETLINECOUNT"""
        return self._sci(2154, 0, 0)

    def allocateLines(self, lines):
        """See Scintilla documentation for SCI_ALLOCATELINES"""
        self._sci(2089, int(lines), 0)

    def setMarginLeft(self, pixelWidth):
        """See Scintilla documentation for SCI_SETMARGINLEFT"""
        self._sci(2155, 0, int(pixelWidth))

    def getMarginLeft(self):
        """See Scintilla documentation for SCI_GETMARGINLEFT"""
        return self._sci(2156, 0, 0)

    def setMarginRight(self, pixelWidth):
        """See Scintilla documentation for SCI_SETMARGINRIGHT"""
        self._sci(2157, 0, int(pixelWidth))

    def getMarginRight(self):
        """See Scintilla documentation for SCI_GETMARGINRIGHT"""
        return self._sci(2158, 0, 0)

    def getModify(self):
        """See Scintilla documentation for SCI_GETMODIFY"""
        return self._sci(2159, 0, 0) != 0

    def setSel(self, anchor, caret):
        """See Scintilla documentation for SCI_SETSEL"""
        self._sci(2160, int(anchor), int(caret))

    def getSelText(self):
        """See Scintilla documentation for SCI_GETSELTEXT"""
        _n = self._sci(2161, 0, 0)
        return _str(self._sci_res(2161, 0, _n))

    def getTextRange(self, start, end):
        """See Scintilla documentation for SCI_GETTEXTRANGE"""
        return self._range(start, end)

    def getTextRangeFull(self, start, end):
        """See Scintilla documentation for SCI_GETTEXTRANGEFULL"""
        return self._range(start, end)

    def hideSelection(self, hide):
        """See Scintilla documentation for SCI_HIDESELECTION"""
        self._sci(2163, (1 if hide else 0), 0)

    def getSelectionHidden(self):
        """See Scintilla documentation for SCI_GETSELECTIONHIDDEN"""
        return self._sci(2088, 0, 0) != 0

    def pointXFromPosition(self, pos):
        """See Scintilla documentation for SCI_POINTXFROMPOSITION"""
        return self._sci(2164, 0, int(pos))

    def pointYFromPosition(self, pos):
        """See Scintilla documentation for SCI_POINTYFROMPOSITION"""
        return self._sci(2165, 0, int(pos))

    def lineFromPosition(self, pos):
        """See Scintilla documentation for SCI_LINEFROMPOSITION"""
        return self._sci(2166, int(pos), 0)

    def positionFromLine(self, line):
        """See Scintilla documentation for SCI_POSITIONFROMLINE"""
        return self._sci(2167, int(line), 0)

    def lineScroll(self, columns, lines):
        """See Scintilla documentation for SCI_LINESCROLL"""
        self._sci(2168, int(columns), int(lines))

    def scrollVertical(self, docLine, subLine):
        """See Scintilla documentation for SCI_SCROLLVERTICAL"""
        self._sci(2817, int(docLine), int(subLine))

    def scrollCaret(self):
        """See Scintilla documentation for SCI_SCROLLCARET"""
        self._sci(2169, 0, 0)

    def scrollRange(self, secondary, primary):
        """See Scintilla documentation for SCI_SCROLLRANGE"""
        self._sci(2569, int(secondary), int(primary))

    def replaceSel(self, text):
        """See Scintilla documentation for SCI_REPLACESEL"""
        self._sci_ls(2170, 0, _bytes(text))

    def setReadOnly(self, readOnly):
        """See Scintilla documentation for SCI_SETREADONLY"""
        self._sci(2171, (1 if readOnly else 0), 0)

    def null(self):
        """See Scintilla documentation for SCI_NULL"""
        self._sci(2172, 0, 0)

    def canPaste(self):
        """See Scintilla documentation for SCI_CANPASTE"""
        return self._sci(2173, 0, 0) != 0

    def canUndo(self):
        """See Scintilla documentation for SCI_CANUNDO"""
        return self._sci(2174, 0, 0) != 0

    def emptyUndoBuffer(self):
        """See Scintilla documentation for SCI_EMPTYUNDOBUFFER"""
        self._sci(2175, 0, 0)

    def undo(self):
        """See Scintilla documentation for SCI_UNDO"""
        self._sci(2176, 0, 0)

    def cut(self):
        """See Scintilla documentation for SCI_CUT"""
        self._sci(2177, 0, 0)

    def copy(self):
        """See Scintilla documentation for SCI_COPY"""
        self._sci(2178, 0, 0)

    def paste(self):
        """See Scintilla documentation for SCI_PASTE"""
        self._sci(2179, 0, 0)

    def clear(self):
        """See Scintilla documentation for SCI_CLEAR"""
        self._sci(2180, 0, 0)

    def setText(self, text):
        """See Scintilla documentation for SCI_SETTEXT"""
        self._sci_ls(2181, 0, _bytes(text))

    def getText(self):
        """See Scintilla documentation for SCI_GETTEXT"""
        _n = self._sci(2182, 0, 0)
        return _str(self._sci_res(2182, _n + 1, _n))

    def getTextLength(self):
        """See Scintilla documentation for SCI_GETTEXTLENGTH"""
        return self._sci(2183, 0, 0)

    def getDirectFunction(self):
        """See Scintilla documentation for SCI_GETDIRECTFUNCTION"""
        return self._sci(2184, 0, 0)

    def getDirectStatusFunction(self):
        """See Scintilla documentation for SCI_GETDIRECTSTATUSFUNCTION"""
        return self._sci(2772, 0, 0)

    def getDirectPointer(self):
        """See Scintilla documentation for SCI_GETDIRECTPOINTER"""
        return self._sci(2185, 0, 0)

    def setOvertype(self, overType):
        """See Scintilla documentation for SCI_SETOVERTYPE"""
        self._sci(2186, (1 if overType else 0), 0)

    def getOvertype(self):
        """See Scintilla documentation for SCI_GETOVERTYPE"""
        return self._sci(2187, 0, 0) != 0

    def setCaretWidth(self, pixelWidth):
        """See Scintilla documentation for SCI_SETCARETWIDTH"""
        self._sci(2188, int(pixelWidth), 0)

    def getCaretWidth(self):
        """See Scintilla documentation for SCI_GETCARETWIDTH"""
        return self._sci(2189, 0, 0)

    def setTargetStart(self, start):
        """See Scintilla documentation for SCI_SETTARGETSTART"""
        self._sci(2190, int(start), 0)

    def getTargetStart(self):
        """See Scintilla documentation for SCI_GETTARGETSTART"""
        return self._sci(2191, 0, 0)

    def setTargetStartVirtualSpace(self, space):
        """See Scintilla documentation for SCI_SETTARGETSTARTVIRTUALSPACE"""
        self._sci(2728, int(space), 0)

    def getTargetStartVirtualSpace(self):
        """See Scintilla documentation for SCI_GETTARGETSTARTVIRTUALSPACE"""
        return self._sci(2729, 0, 0)

    def setTargetEnd(self, end):
        """See Scintilla documentation for SCI_SETTARGETEND"""
        self._sci(2192, int(end), 0)

    def getTargetEnd(self):
        """See Scintilla documentation for SCI_GETTARGETEND"""
        return self._sci(2193, 0, 0)

    def setTargetEndVirtualSpace(self, space):
        """See Scintilla documentation for SCI_SETTARGETENDVIRTUALSPACE"""
        self._sci(2730, int(space), 0)

    def getTargetEndVirtualSpace(self):
        """See Scintilla documentation for SCI_GETTARGETENDVIRTUALSPACE"""
        return self._sci(2731, 0, 0)

    def setTargetRange(self, start, end):
        """See Scintilla documentation for SCI_SETTARGETRANGE"""
        self._sci(2686, int(start), int(end))

    def getTargetText(self):
        """See Scintilla documentation for SCI_GETTARGETTEXT"""
        _n = self._sci(2687, 0, 0)
        return _str(self._sci_res(2687, 0, _n))

    def targetFromSelection(self):
        """See Scintilla documentation for SCI_TARGETFROMSELECTION"""
        self._sci(2287, 0, 0)

    def targetWholeDocument(self):
        """See Scintilla documentation for SCI_TARGETWHOLEDOCUMENT"""
        self._sci(2690, 0, 0)

    def replaceTarget(self, text):
        """See Scintilla documentation for SCI_REPLACETARGET"""
        _s = _bytes(text)
        return self._sci_ls(2194, len(_s), _s)

    def replaceTargetRE(self, text):
        """See Scintilla documentation for SCI_REPLACETARGETRE"""
        _s = _bytes(text)
        return self._sci_ls(2195, len(_s), _s)

    def replaceTargetMinimal(self, text):
        """See Scintilla documentation for SCI_REPLACETARGETMINIMAL"""
        _s = _bytes(text)
        return self._sci_ls(2779, len(_s), _s)

    def searchInTarget(self, text):
        """See Scintilla documentation for SCI_SEARCHINTARGET"""
        _s = _bytes(text)
        return self._sci_ls(2197, len(_s), _s)

    def setSearchFlags(self, searchFlags):
        """See Scintilla documentation for SCI_SETSEARCHFLAGS"""
        self._sci(2198, int(searchFlags), 0)

    def getSearchFlags(self):
        """See Scintilla documentation for SCI_GETSEARCHFLAGS"""
        return self._sci(2199, 0, 0)

    def callTipShow(self, pos, definition):
        """See Scintilla documentation for SCI_CALLTIPSHOW"""
        self._sci_ls(2200, int(pos), _bytes(definition))

    def callTipCancel(self):
        """See Scintilla documentation for SCI_CALLTIPCANCEL"""
        self._sci(2201, 0, 0)

    def callTipActive(self):
        """See Scintilla documentation for SCI_CALLTIPACTIVE"""
        return self._sci(2202, 0, 0) != 0

    def callTipPosStart(self):
        """See Scintilla documentation for SCI_CALLTIPPOSSTART"""
        return self._sci(2203, 0, 0)

    def callTipSetPosStart(self, posStart):
        """See Scintilla documentation for SCI_CALLTIPSETPOSSTART"""
        self._sci(2214, int(posStart), 0)

    def callTipSetHlt(self, highlightStart, highlightEnd):
        """See Scintilla documentation for SCI_CALLTIPSETHLT"""
        self._sci(2204, int(highlightStart), int(highlightEnd))

    def callTipSetBack(self, back):
        """See Scintilla documentation for SCI_CALLTIPSETBACK"""
        self._sci(2205, _rgb(back), 0)

    def callTipSetFore(self, fore):
        """See Scintilla documentation for SCI_CALLTIPSETFORE"""
        self._sci(2206, _rgb(fore), 0)

    def callTipSetForeHlt(self, fore):
        """See Scintilla documentation for SCI_CALLTIPSETFOREHLT"""
        self._sci(2207, _rgb(fore), 0)

    def callTipUseStyle(self, tabSize):
        """See Scintilla documentation for SCI_CALLTIPUSESTYLE"""
        self._sci(2212, int(tabSize), 0)

    def callTipSetPosition(self, above):
        """See Scintilla documentation for SCI_CALLTIPSETPOSITION"""
        self._sci(2213, (1 if above else 0), 0)

    def visibleFromDocLine(self, docLine):
        """See Scintilla documentation for SCI_VISIBLEFROMDOCLINE"""
        return self._sci(2220, int(docLine), 0)

    def docLineFromVisible(self, displayLine):
        """See Scintilla documentation for SCI_DOCLINEFROMVISIBLE"""
        return self._sci(2221, int(displayLine), 0)

    def wrapCount(self, docLine):
        """See Scintilla documentation for SCI_WRAPCOUNT"""
        return self._sci(2235, int(docLine), 0)

    def setFoldLevel(self, line, level):
        """See Scintilla documentation for SCI_SETFOLDLEVEL"""
        self._sci(2222, int(line), int(level))

    def getFoldLevel(self, line):
        """See Scintilla documentation for SCI_GETFOLDLEVEL"""
        return self._sci(2223, int(line), 0)

    def getLastChild(self, line, level):
        """See Scintilla documentation for SCI_GETLASTCHILD"""
        return self._sci(2224, int(line), int(level))

    def getFoldParent(self, line):
        """See Scintilla documentation for SCI_GETFOLDPARENT"""
        return self._sci(2225, int(line), 0)

    def showLines(self, lineStart, lineEnd):
        """See Scintilla documentation for SCI_SHOWLINES"""
        self._sci(2226, int(lineStart), int(lineEnd))

    def hideLines(self, lineStart, lineEnd):
        """See Scintilla documentation for SCI_HIDELINES"""
        self._sci(2227, int(lineStart), int(lineEnd))

    def getLineVisible(self, line):
        """See Scintilla documentation for SCI_GETLINEVISIBLE"""
        return self._sci(2228, int(line), 0) != 0

    def getAllLinesVisible(self):
        """See Scintilla documentation for SCI_GETALLLINESVISIBLE"""
        return self._sci(2236, 0, 0) != 0

    def setFoldExpanded(self, line, expanded):
        """See Scintilla documentation for SCI_SETFOLDEXPANDED"""
        self._sci(2229, int(line), (1 if expanded else 0))

    def getFoldExpanded(self, line):
        """See Scintilla documentation for SCI_GETFOLDEXPANDED"""
        return self._sci(2230, int(line), 0) != 0

    def toggleFold(self, line):
        """See Scintilla documentation for SCI_TOGGLEFOLD"""
        self._sci(2231, int(line), 0)

    def toggleFoldShowText(self, line, text):
        """See Scintilla documentation for SCI_TOGGLEFOLDSHOWTEXT"""
        self._sci_ls(2700, int(line), _bytes(text))

    def foldDisplayTextSetStyle(self, style):
        """See Scintilla documentation for SCI_FOLDDISPLAYTEXTSETSTYLE"""
        self._sci(2701, int(style), 0)

    def foldDisplayTextGetStyle(self):
        """See Scintilla documentation for SCI_FOLDDISPLAYTEXTGETSTYLE"""
        return self._sci(2707, 0, 0)

    def setDefaultFoldDisplayText(self, text):
        """See Scintilla documentation for SCI_SETDEFAULTFOLDDISPLAYTEXT"""
        self._sci_ls(2722, 0, _bytes(text))

    def getDefaultFoldDisplayText(self):
        """See Scintilla documentation for SCI_GETDEFAULTFOLDDISPLAYTEXT"""
        _n = self._sci(2723, 0, 0)
        return _str(self._sci_res(2723, 0, _n))

    def foldLine(self, line, action):
        """See Scintilla documentation for SCI_FOLDLINE"""
        self._sci(2237, int(line), int(action))

    def foldChildren(self, line, action):
        """See Scintilla documentation for SCI_FOLDCHILDREN"""
        self._sci(2238, int(line), int(action))

    def expandChildren(self, line, level):
        """See Scintilla documentation for SCI_EXPANDCHILDREN"""
        self._sci(2239, int(line), int(level))

    def foldAll(self, action):
        """See Scintilla documentation for SCI_FOLDALL"""
        self._sci(2662, int(action), 0)

    def ensureVisible(self, line):
        """See Scintilla documentation for SCI_ENSUREVISIBLE"""
        self._sci(2232, int(line), 0)

    def setAutomaticFold(self, automaticFold):
        """See Scintilla documentation for SCI_SETAUTOMATICFOLD"""
        self._sci(2663, int(automaticFold), 0)

    def getAutomaticFold(self):
        """See Scintilla documentation for SCI_GETAUTOMATICFOLD"""
        return self._sci(2664, 0, 0)

    def setFoldFlags(self, flags):
        """See Scintilla documentation for SCI_SETFOLDFLAGS"""
        self._sci(2233, int(flags), 0)

    def ensureVisibleEnforcePolicy(self, line):
        """See Scintilla documentation for SCI_ENSUREVISIBLEENFORCEPOLICY"""
        self._sci(2234, int(line), 0)

    def setTabIndents(self, tabIndents):
        """See Scintilla documentation for SCI_SETTABINDENTS"""
        self._sci(2260, (1 if tabIndents else 0), 0)

    def getTabIndents(self):
        """See Scintilla documentation for SCI_GETTABINDENTS"""
        return self._sci(2261, 0, 0) != 0

    def setBackSpaceUnIndents(self, bsUnIndents):
        """See Scintilla documentation for SCI_SETBACKSPACEUNINDENTS"""
        self._sci(2262, (1 if bsUnIndents else 0), 0)

    def getBackSpaceUnIndents(self):
        """See Scintilla documentation for SCI_GETBACKSPACEUNINDENTS"""
        return self._sci(2263, 0, 0) != 0

    def setMouseDwellTime(self, periodMilliseconds):
        """See Scintilla documentation for SCI_SETMOUSEDWELLTIME"""
        self._sci(2264, int(periodMilliseconds), 0)

    def getMouseDwellTime(self):
        """See Scintilla documentation for SCI_GETMOUSEDWELLTIME"""
        return self._sci(2265, 0, 0)

    def wordStartPosition(self, pos, onlyWordCharacters):
        """See Scintilla documentation for SCI_WORDSTARTPOSITION"""
        return self._sci(2266, int(pos), (1 if onlyWordCharacters else 0))

    def wordEndPosition(self, pos, onlyWordCharacters):
        """See Scintilla documentation for SCI_WORDENDPOSITION"""
        return self._sci(2267, int(pos), (1 if onlyWordCharacters else 0))

    def isRangeWord(self, start, end):
        """See Scintilla documentation for SCI_ISRANGEWORD"""
        return self._sci(2691, int(start), int(end)) != 0

    def setIdleStyling(self, idleStyling):
        """See Scintilla documentation for SCI_SETIDLESTYLING"""
        self._sci(2692, int(idleStyling), 0)

    def getIdleStyling(self):
        """See Scintilla documentation for SCI_GETIDLESTYLING"""
        return self._sci(2693, 0, 0)

    def setWrapMode(self, wrapMode):
        """See Scintilla documentation for SCI_SETWRAPMODE"""
        self._sci(2268, int(wrapMode), 0)

    def getWrapMode(self):
        """See Scintilla documentation for SCI_GETWRAPMODE"""
        return self._sci(2269, 0, 0)

    def setWrapVisualFlags(self, wrapVisualFlags):
        """See Scintilla documentation for SCI_SETWRAPVISUALFLAGS"""
        self._sci(2460, int(wrapVisualFlags), 0)

    def getWrapVisualFlags(self):
        """See Scintilla documentation for SCI_GETWRAPVISUALFLAGS"""
        return self._sci(2461, 0, 0)

    def setWrapVisualFlagsLocation(self, wrapVisualFlagsLocation):
        """See Scintilla documentation for SCI_SETWRAPVISUALFLAGSLOCATION"""
        self._sci(2462, int(wrapVisualFlagsLocation), 0)

    def getWrapVisualFlagsLocation(self):
        """See Scintilla documentation for SCI_GETWRAPVISUALFLAGSLOCATION"""
        return self._sci(2463, 0, 0)

    def setWrapStartIndent(self, indent):
        """See Scintilla documentation for SCI_SETWRAPSTARTINDENT"""
        self._sci(2464, int(indent), 0)

    def getWrapStartIndent(self):
        """See Scintilla documentation for SCI_GETWRAPSTARTINDENT"""
        return self._sci(2465, 0, 0)

    def setWrapIndentMode(self, wrapIndentMode):
        """See Scintilla documentation for SCI_SETWRAPINDENTMODE"""
        self._sci(2472, int(wrapIndentMode), 0)

    def getWrapIndentMode(self):
        """See Scintilla documentation for SCI_GETWRAPINDENTMODE"""
        return self._sci(2473, 0, 0)

    def setLayoutCache(self, cacheMode):
        """See Scintilla documentation for SCI_SETLAYOUTCACHE"""
        self._sci(2272, int(cacheMode), 0)

    def getLayoutCache(self):
        """See Scintilla documentation for SCI_GETLAYOUTCACHE"""
        return self._sci(2273, 0, 0)

    def setScrollWidth(self, pixelWidth):
        """See Scintilla documentation for SCI_SETSCROLLWIDTH"""
        self._sci(2274, int(pixelWidth), 0)

    def getScrollWidth(self):
        """See Scintilla documentation for SCI_GETSCROLLWIDTH"""
        return self._sci(2275, 0, 0)

    def setScrollWidthTracking(self, tracking):
        """See Scintilla documentation for SCI_SETSCROLLWIDTHTRACKING"""
        self._sci(2516, (1 if tracking else 0), 0)

    def getScrollWidthTracking(self):
        """See Scintilla documentation for SCI_GETSCROLLWIDTHTRACKING"""
        return self._sci(2517, 0, 0) != 0

    def textWidth(self, style, text):
        """See Scintilla documentation for SCI_TEXTWIDTH"""
        return self._sci_ls(2276, int(style), _bytes(text))

    def setEndAtLastLine(self, endAtLastLine):
        """See Scintilla documentation for SCI_SETENDATLASTLINE"""
        self._sci(2277, (1 if endAtLastLine else 0), 0)

    def getEndAtLastLine(self):
        """See Scintilla documentation for SCI_GETENDATLASTLINE"""
        return self._sci(2278, 0, 0) != 0

    def textHeight(self, line):
        """See Scintilla documentation for SCI_TEXTHEIGHT"""
        return self._sci(2279, int(line), 0)

    def setVScrollBar(self, visible):
        """See Scintilla documentation for SCI_SETVSCROLLBAR"""
        self._sci(2280, (1 if visible else 0), 0)

    def getVScrollBar(self):
        """See Scintilla documentation for SCI_GETVSCROLLBAR"""
        return self._sci(2281, 0, 0) != 0

    def appendText(self, text):
        """See Scintilla documentation for SCI_APPENDTEXT"""
        _s = _bytes(text)
        self._sci_ls(2282, len(_s), _s)

    def getPhasesDraw(self):
        """See Scintilla documentation for SCI_GETPHASESDRAW"""
        return self._sci(2673, 0, 0)

    def setPhasesDraw(self, phases):
        """See Scintilla documentation for SCI_SETPHASESDRAW"""
        self._sci(2674, int(phases), 0)

    def setFontQuality(self, fontQuality):
        """See Scintilla documentation for SCI_SETFONTQUALITY"""
        self._sci(2611, int(fontQuality), 0)

    def getFontQuality(self):
        """See Scintilla documentation for SCI_GETFONTQUALITY"""
        return self._sci(2612, 0, 0)

    def setFirstVisibleLine(self, displayLine):
        """See Scintilla documentation for SCI_SETFIRSTVISIBLELINE"""
        self._sci(2613, int(displayLine), 0)

    def setMultiPaste(self, multiPaste):
        """See Scintilla documentation for SCI_SETMULTIPASTE"""
        self._sci(2614, int(multiPaste), 0)

    def getMultiPaste(self):
        """See Scintilla documentation for SCI_GETMULTIPASTE"""
        return self._sci(2615, 0, 0)

    def getTag(self, tagNumber):
        """See Scintilla documentation for SCI_GETTAG"""
        _n = self._sci(2616, int(tagNumber), 0)
        return _str(self._sci_res(2616, int(tagNumber), _n))

    def linesJoin(self):
        """See Scintilla documentation for SCI_LINESJOIN"""
        self._sci(2288, 0, 0)

    def linesSplit(self, pixelWidth):
        """See Scintilla documentation for SCI_LINESSPLIT"""
        self._sci(2289, int(pixelWidth), 0)

    def setFoldMarginColour(self, useSetting, back):
        """See Scintilla documentation for SCI_SETFOLDMARGINCOLOUR"""
        self._sci(2290, (1 if useSetting else 0), _rgb(back))

    def setFoldMarginHiColour(self, useSetting, fore):
        """See Scintilla documentation for SCI_SETFOLDMARGINHICOLOUR"""
        self._sci(2291, (1 if useSetting else 0), _rgb(fore))

    def setAccessibility(self, accessibility):
        """See Scintilla documentation for SCI_SETACCESSIBILITY"""
        self._sci(2702, int(accessibility), 0)

    def getAccessibility(self):
        """See Scintilla documentation for SCI_GETACCESSIBILITY"""
        return self._sci(2703, 0, 0)

    def lineDown(self):
        """See Scintilla documentation for SCI_LINEDOWN"""
        self._sci(2300, 0, 0)

    def lineDownExtend(self):
        """See Scintilla documentation for SCI_LINEDOWNEXTEND"""
        self._sci(2301, 0, 0)

    def lineUp(self):
        """See Scintilla documentation for SCI_LINEUP"""
        self._sci(2302, 0, 0)

    def lineUpExtend(self):
        """See Scintilla documentation for SCI_LINEUPEXTEND"""
        self._sci(2303, 0, 0)

    def charLeft(self):
        """See Scintilla documentation for SCI_CHARLEFT"""
        self._sci(2304, 0, 0)

    def charLeftExtend(self):
        """See Scintilla documentation for SCI_CHARLEFTEXTEND"""
        self._sci(2305, 0, 0)

    def charRight(self):
        """See Scintilla documentation for SCI_CHARRIGHT"""
        self._sci(2306, 0, 0)

    def charRightExtend(self):
        """See Scintilla documentation for SCI_CHARRIGHTEXTEND"""
        self._sci(2307, 0, 0)

    def wordLeft(self):
        """See Scintilla documentation for SCI_WORDLEFT"""
        self._sci(2308, 0, 0)

    def wordLeftExtend(self):
        """See Scintilla documentation for SCI_WORDLEFTEXTEND"""
        self._sci(2309, 0, 0)

    def wordRight(self):
        """See Scintilla documentation for SCI_WORDRIGHT"""
        self._sci(2310, 0, 0)

    def wordRightExtend(self):
        """See Scintilla documentation for SCI_WORDRIGHTEXTEND"""
        self._sci(2311, 0, 0)

    def home(self):
        """See Scintilla documentation for SCI_HOME"""
        self._sci(2312, 0, 0)

    def homeExtend(self):
        """See Scintilla documentation for SCI_HOMEEXTEND"""
        self._sci(2313, 0, 0)

    def lineEnd(self):
        """See Scintilla documentation for SCI_LINEEND"""
        self._sci(2314, 0, 0)

    def lineEndExtend(self):
        """See Scintilla documentation for SCI_LINEENDEXTEND"""
        self._sci(2315, 0, 0)

    def documentStart(self):
        """See Scintilla documentation for SCI_DOCUMENTSTART"""
        self._sci(2316, 0, 0)

    def documentStartExtend(self):
        """See Scintilla documentation for SCI_DOCUMENTSTARTEXTEND"""
        self._sci(2317, 0, 0)

    def documentEnd(self):
        """See Scintilla documentation for SCI_DOCUMENTEND"""
        self._sci(2318, 0, 0)

    def documentEndExtend(self):
        """See Scintilla documentation for SCI_DOCUMENTENDEXTEND"""
        self._sci(2319, 0, 0)

    def pageUp(self):
        """See Scintilla documentation for SCI_PAGEUP"""
        self._sci(2320, 0, 0)

    def pageUpExtend(self):
        """See Scintilla documentation for SCI_PAGEUPEXTEND"""
        self._sci(2321, 0, 0)

    def pageDown(self):
        """See Scintilla documentation for SCI_PAGEDOWN"""
        self._sci(2322, 0, 0)

    def pageDownExtend(self):
        """See Scintilla documentation for SCI_PAGEDOWNEXTEND"""
        self._sci(2323, 0, 0)

    def editToggleOvertype(self):
        """See Scintilla documentation for SCI_EDITTOGGLEOVERTYPE"""
        self._sci(2324, 0, 0)

    def cancel(self):
        """See Scintilla documentation for SCI_CANCEL"""
        self._sci(2325, 0, 0)

    def deleteBack(self):
        """See Scintilla documentation for SCI_DELETEBACK"""
        self._sci(2326, 0, 0)

    def tab(self):
        """See Scintilla documentation for SCI_TAB"""
        self._sci(2327, 0, 0)

    def lineIndent(self):
        """See Scintilla documentation for SCI_LINEINDENT"""
        self._sci(2813, 0, 0)

    def backTab(self):
        """See Scintilla documentation for SCI_BACKTAB"""
        self._sci(2328, 0, 0)

    def lineDedent(self):
        """See Scintilla documentation for SCI_LINEDEDENT"""
        self._sci(2814, 0, 0)

    def newLine(self):
        """See Scintilla documentation for SCI_NEWLINE"""
        self._sci(2329, 0, 0)

    def formFeed(self):
        """See Scintilla documentation for SCI_FORMFEED"""
        self._sci(2330, 0, 0)

    def vCHome(self):
        """See Scintilla documentation for SCI_VCHOME"""
        self._sci(2331, 0, 0)

    def vCHomeExtend(self):
        """See Scintilla documentation for SCI_VCHOMEEXTEND"""
        self._sci(2332, 0, 0)

    def zoomIn(self):
        """See Scintilla documentation for SCI_ZOOMIN"""
        self._sci(2333, 0, 0)

    def zoomOut(self):
        """See Scintilla documentation for SCI_ZOOMOUT"""
        self._sci(2334, 0, 0)

    def delWordLeft(self):
        """See Scintilla documentation for SCI_DELWORDLEFT"""
        self._sci(2335, 0, 0)

    def delWordRight(self):
        """See Scintilla documentation for SCI_DELWORDRIGHT"""
        self._sci(2336, 0, 0)

    def delWordRightEnd(self):
        """See Scintilla documentation for SCI_DELWORDRIGHTEND"""
        self._sci(2518, 0, 0)

    def lineCut(self):
        """See Scintilla documentation for SCI_LINECUT"""
        self._sci(2337, 0, 0)

    def lineDelete(self):
        """See Scintilla documentation for SCI_LINEDELETE"""
        self._sci(2338, 0, 0)

    def lineTranspose(self):
        """See Scintilla documentation for SCI_LINETRANSPOSE"""
        self._sci(2339, 0, 0)

    def lineReverse(self):
        """See Scintilla documentation for SCI_LINEREVERSE"""
        self._sci(2354, 0, 0)

    def lineDuplicate(self):
        """See Scintilla documentation for SCI_LINEDUPLICATE"""
        self._sci(2404, 0, 0)

    def lowerCase(self):
        """See Scintilla documentation for SCI_LOWERCASE"""
        self._sci(2340, 0, 0)

    def upperCase(self):
        """See Scintilla documentation for SCI_UPPERCASE"""
        self._sci(2341, 0, 0)

    def lineScrollDown(self):
        """See Scintilla documentation for SCI_LINESCROLLDOWN"""
        self._sci(2342, 0, 0)

    def lineScrollUp(self):
        """See Scintilla documentation for SCI_LINESCROLLUP"""
        self._sci(2343, 0, 0)

    def deleteBackNotLine(self):
        """See Scintilla documentation for SCI_DELETEBACKNOTLINE"""
        self._sci(2344, 0, 0)

    def homeDisplay(self):
        """See Scintilla documentation for SCI_HOMEDISPLAY"""
        self._sci(2345, 0, 0)

    def homeDisplayExtend(self):
        """See Scintilla documentation for SCI_HOMEDISPLAYEXTEND"""
        self._sci(2346, 0, 0)

    def lineEndDisplay(self):
        """See Scintilla documentation for SCI_LINEENDDISPLAY"""
        self._sci(2347, 0, 0)

    def lineEndDisplayExtend(self):
        """See Scintilla documentation for SCI_LINEENDDISPLAYEXTEND"""
        self._sci(2348, 0, 0)

    def homeWrap(self):
        """See Scintilla documentation for SCI_HOMEWRAP"""
        self._sci(2349, 0, 0)

    def homeWrapExtend(self):
        """See Scintilla documentation for SCI_HOMEWRAPEXTEND"""
        self._sci(2450, 0, 0)

    def lineEndWrap(self):
        """See Scintilla documentation for SCI_LINEENDWRAP"""
        self._sci(2451, 0, 0)

    def lineEndWrapExtend(self):
        """See Scintilla documentation for SCI_LINEENDWRAPEXTEND"""
        self._sci(2452, 0, 0)

    def vCHomeWrap(self):
        """See Scintilla documentation for SCI_VCHOMEWRAP"""
        self._sci(2453, 0, 0)

    def vCHomeWrapExtend(self):
        """See Scintilla documentation for SCI_VCHOMEWRAPEXTEND"""
        self._sci(2454, 0, 0)

    def lineCopy(self):
        """See Scintilla documentation for SCI_LINECOPY"""
        self._sci(2455, 0, 0)

    def moveCaretInsideView(self):
        """See Scintilla documentation for SCI_MOVECARETINSIDEVIEW"""
        self._sci(2401, 0, 0)

    def lineLength(self, line):
        """See Scintilla documentation for SCI_LINELENGTH"""
        return self._sci(2350, int(line), 0)

    def braceHighlight(self, posA, posB):
        """See Scintilla documentation for SCI_BRACEHIGHLIGHT"""
        self._sci(2351, int(posA), int(posB))

    def braceHighlightIndicator(self, useSetting, indicator):
        """See Scintilla documentation for SCI_BRACEHIGHLIGHTINDICATOR"""
        self._sci(2498, (1 if useSetting else 0), int(indicator))

    def braceBadLight(self, pos):
        """See Scintilla documentation for SCI_BRACEBADLIGHT"""
        self._sci(2352, int(pos), 0)

    def braceBadLightIndicator(self, useSetting, indicator):
        """See Scintilla documentation for SCI_BRACEBADLIGHTINDICATOR"""
        self._sci(2499, (1 if useSetting else 0), int(indicator))

    def braceMatch(self, pos, maxReStyle):
        """See Scintilla documentation for SCI_BRACEMATCH"""
        return self._sci(2353, int(pos), int(maxReStyle))

    def braceMatchNext(self, pos, startPos):
        """See Scintilla documentation for SCI_BRACEMATCHNEXT"""
        return self._sci(2369, int(pos), int(startPos))

    def getViewEOL(self):
        """See Scintilla documentation for SCI_GETVIEWEOL"""
        return self._sci(2355, 0, 0) != 0

    def setViewEOL(self, visible):
        """See Scintilla documentation for SCI_SETVIEWEOL"""
        self._sci(2356, (1 if visible else 0), 0)

    def getDocPointer(self):
        """See Scintilla documentation for SCI_GETDOCPOINTER"""
        return self._sci(2357, 0, 0)

    def setDocPointer(self, doc):
        """See Scintilla documentation for SCI_SETDOCPOINTER"""
        self._sci(2358, 0, int(doc))

    def setModEventMask(self, eventMask):
        """See Scintilla documentation for SCI_SETMODEVENTMASK"""
        self._sci(2359, int(eventMask), 0)

    def getEdgeColumn(self):
        """See Scintilla documentation for SCI_GETEDGECOLUMN"""
        return self._sci(2360, 0, 0)

    def setEdgeColumn(self, column):
        """See Scintilla documentation for SCI_SETEDGECOLUMN"""
        self._sci(2361, int(column), 0)

    def getEdgeMode(self):
        """See Scintilla documentation for SCI_GETEDGEMODE"""
        return self._sci(2362, 0, 0)

    def setEdgeMode(self, edgeMode):
        """See Scintilla documentation for SCI_SETEDGEMODE"""
        self._sci(2363, int(edgeMode), 0)

    def getEdgeColour(self):
        """See Scintilla documentation for SCI_GETEDGECOLOUR"""
        return _colour_tuple(self._sci(2364, 0, 0))

    def setEdgeColour(self, edgeColour):
        """See Scintilla documentation for SCI_SETEDGECOLOUR"""
        self._sci(2365, _rgb(edgeColour), 0)

    def multiEdgeAddLine(self, column, edgeColour):
        """See Scintilla documentation for SCI_MULTIEDGEADDLINE"""
        self._sci(2694, int(column), _rgb(edgeColour))

    def multiEdgeClearAll(self):
        """See Scintilla documentation for SCI_MULTIEDGECLEARALL"""
        self._sci(2695, 0, 0)

    def getMultiEdgeColumn(self, which):
        """See Scintilla documentation for SCI_GETMULTIEDGECOLUMN"""
        return self._sci(2749, int(which), 0)

    def searchAnchor(self):
        """See Scintilla documentation for SCI_SEARCHANCHOR"""
        self._sci(2366, 0, 0)

    def searchNext(self, searchFlags, text):
        """See Scintilla documentation for SCI_SEARCHNEXT"""
        return self._sci_ls(2367, int(searchFlags), _bytes(text))

    def searchPrev(self, searchFlags, text):
        """See Scintilla documentation for SCI_SEARCHPREV"""
        return self._sci_ls(2368, int(searchFlags), _bytes(text))

    def linesOnScreen(self):
        """See Scintilla documentation for SCI_LINESONSCREEN"""
        return self._sci(2370, 0, 0)

    def usePopUp(self, popUpMode):
        """See Scintilla documentation for SCI_USEPOPUP"""
        self._sci(2371, int(popUpMode), 0)

    def selectionIsRectangle(self):
        """See Scintilla documentation for SCI_SELECTIONISRECTANGLE"""
        return self._sci(2372, 0, 0) != 0

    def setZoom(self, zoomInPoints):
        """See Scintilla documentation for SCI_SETZOOM"""
        self._sci(2373, int(zoomInPoints), 0)

    def getZoom(self):
        """See Scintilla documentation for SCI_GETZOOM"""
        return self._sci(2374, 0, 0)

    def createDocument(self, bytes, documentOptions):
        """See Scintilla documentation for SCI_CREATEDOCUMENT"""
        return self._sci(2375, int(bytes), int(documentOptions))

    def addRefDocument(self, doc):
        """See Scintilla documentation for SCI_ADDREFDOCUMENT"""
        self._sci(2376, 0, int(doc))

    def releaseDocument(self, doc):
        """See Scintilla documentation for SCI_RELEASEDOCUMENT"""
        self._sci(2377, 0, int(doc))

    def getDocumentOptions(self):
        """See Scintilla documentation for SCI_GETDOCUMENTOPTIONS"""
        return self._sci(2379, 0, 0)

    def getModEventMask(self):
        """See Scintilla documentation for SCI_GETMODEVENTMASK"""
        return self._sci(2378, 0, 0)

    def setCommandEvents(self, commandEvents):
        """See Scintilla documentation for SCI_SETCOMMANDEVENTS"""
        self._sci(2717, (1 if commandEvents else 0), 0)

    def getCommandEvents(self):
        """See Scintilla documentation for SCI_GETCOMMANDEVENTS"""
        return self._sci(2718, 0, 0) != 0

    def setFocus(self, focus):
        """See Scintilla documentation for SCI_SETFOCUS"""
        self._sci(2380, (1 if focus else 0), 0)

    def getFocus(self):
        """See Scintilla documentation for SCI_GETFOCUS"""
        return self._sci(2381, 0, 0) != 0

    def setStatus(self, status):
        """See Scintilla documentation for SCI_SETSTATUS"""
        self._sci(2382, int(status), 0)

    def getStatus(self):
        """See Scintilla documentation for SCI_GETSTATUS"""
        return self._sci(2383, 0, 0)

    def setMouseDownCaptures(self, captures):
        """See Scintilla documentation for SCI_SETMOUSEDOWNCAPTURES"""
        self._sci(2384, (1 if captures else 0), 0)

    def getMouseDownCaptures(self):
        """See Scintilla documentation for SCI_GETMOUSEDOWNCAPTURES"""
        return self._sci(2385, 0, 0) != 0

    def setMouseWheelCaptures(self, captures):
        """See Scintilla documentation for SCI_SETMOUSEWHEELCAPTURES"""
        self._sci(2696, (1 if captures else 0), 0)

    def getMouseWheelCaptures(self):
        """See Scintilla documentation for SCI_GETMOUSEWHEELCAPTURES"""
        return self._sci(2697, 0, 0) != 0

    def setCursor(self, cursorType):
        """See Scintilla documentation for SCI_SETCURSOR"""
        self._sci(2386, int(cursorType), 0)

    def getCursor(self):
        """See Scintilla documentation for SCI_GETCURSOR"""
        return self._sci(2387, 0, 0)

    def setControlCharSymbol(self, symbol):
        """See Scintilla documentation for SCI_SETCONTROLCHARSYMBOL"""
        self._sci(2388, int(symbol), 0)

    def getControlCharSymbol(self):
        """See Scintilla documentation for SCI_GETCONTROLCHARSYMBOL"""
        return self._sci(2389, 0, 0)

    def wordPartLeft(self):
        """See Scintilla documentation for SCI_WORDPARTLEFT"""
        self._sci(2390, 0, 0)

    def wordPartLeftExtend(self):
        """See Scintilla documentation for SCI_WORDPARTLEFTEXTEND"""
        self._sci(2391, 0, 0)

    def wordPartRight(self):
        """See Scintilla documentation for SCI_WORDPARTRIGHT"""
        self._sci(2392, 0, 0)

    def wordPartRightExtend(self):
        """See Scintilla documentation for SCI_WORDPARTRIGHTEXTEND"""
        self._sci(2393, 0, 0)

    def setVisiblePolicy(self, visiblePolicy, visibleSlop):
        """See Scintilla documentation for SCI_SETVISIBLEPOLICY"""
        self._sci(2394, int(visiblePolicy), int(visibleSlop))

    def delLineLeft(self):
        """See Scintilla documentation for SCI_DELLINELEFT"""
        self._sci(2395, 0, 0)

    def delLineRight(self):
        """See Scintilla documentation for SCI_DELLINERIGHT"""
        self._sci(2396, 0, 0)

    def setXOffset(self, xOffset):
        """See Scintilla documentation for SCI_SETXOFFSET"""
        self._sci(2397, int(xOffset), 0)

    def getXOffset(self):
        """See Scintilla documentation for SCI_GETXOFFSET"""
        return self._sci(2398, 0, 0)

    def chooseCaretX(self):
        """See Scintilla documentation for SCI_CHOOSECARETX"""
        self._sci(2399, 0, 0)

    def grabFocus(self):
        """See Scintilla documentation for SCI_GRABFOCUS"""
        self._sci(2400, 0, 0)

    def setXCaretPolicy(self, caretPolicy, caretSlop):
        """See Scintilla documentation for SCI_SETXCARETPOLICY"""
        self._sci(2402, int(caretPolicy), int(caretSlop))

    def setYCaretPolicy(self, caretPolicy, caretSlop):
        """See Scintilla documentation for SCI_SETYCARETPOLICY"""
        self._sci(2403, int(caretPolicy), int(caretSlop))

    def setPrintWrapMode(self, wrapMode):
        """See Scintilla documentation for SCI_SETPRINTWRAPMODE"""
        self._sci(2406, int(wrapMode), 0)

    def getPrintWrapMode(self):
        """See Scintilla documentation for SCI_GETPRINTWRAPMODE"""
        return self._sci(2407, 0, 0)

    def setHotspotActiveFore(self, useSetting, fore):
        """See Scintilla documentation for SCI_SETHOTSPOTACTIVEFORE"""
        self._sci(2410, (1 if useSetting else 0), _rgb(fore))

    def getHotspotActiveFore(self):
        """See Scintilla documentation for SCI_GETHOTSPOTACTIVEFORE"""
        return _colour_tuple(self._sci(2494, 0, 0))

    def setHotspotActiveBack(self, useSetting, back):
        """See Scintilla documentation for SCI_SETHOTSPOTACTIVEBACK"""
        self._sci(2411, (1 if useSetting else 0), _rgb(back))

    def getHotspotActiveBack(self):
        """See Scintilla documentation for SCI_GETHOTSPOTACTIVEBACK"""
        return _colour_tuple(self._sci(2495, 0, 0))

    def setHotspotActiveUnderline(self, underline):
        """See Scintilla documentation for SCI_SETHOTSPOTACTIVEUNDERLINE"""
        self._sci(2412, (1 if underline else 0), 0)

    def getHotspotActiveUnderline(self):
        """See Scintilla documentation for SCI_GETHOTSPOTACTIVEUNDERLINE"""
        return self._sci(2496, 0, 0) != 0

    def setHotspotSingleLine(self, singleLine):
        """See Scintilla documentation for SCI_SETHOTSPOTSINGLELINE"""
        self._sci(2421, (1 if singleLine else 0), 0)

    def getHotspotSingleLine(self):
        """See Scintilla documentation for SCI_GETHOTSPOTSINGLELINE"""
        return self._sci(2497, 0, 0) != 0

    def paraDown(self):
        """See Scintilla documentation for SCI_PARADOWN"""
        self._sci(2413, 0, 0)

    def paraDownExtend(self):
        """See Scintilla documentation for SCI_PARADOWNEXTEND"""
        self._sci(2414, 0, 0)

    def paraUp(self):
        """See Scintilla documentation for SCI_PARAUP"""
        self._sci(2415, 0, 0)

    def paraUpExtend(self):
        """See Scintilla documentation for SCI_PARAUPEXTEND"""
        self._sci(2416, 0, 0)

    def positionBefore(self, pos):
        """See Scintilla documentation for SCI_POSITIONBEFORE"""
        return self._sci(2417, int(pos), 0)

    def positionAfter(self, pos):
        """See Scintilla documentation for SCI_POSITIONAFTER"""
        return self._sci(2418, int(pos), 0)

    def positionRelative(self, pos, relative):
        """See Scintilla documentation for SCI_POSITIONRELATIVE"""
        return self._sci(2670, int(pos), int(relative))

    def positionRelativeCodeUnits(self, pos, relative):
        """See Scintilla documentation for SCI_POSITIONRELATIVECODEUNITS"""
        return self._sci(2716, int(pos), int(relative))

    def copyRange(self, start, end):
        """See Scintilla documentation for SCI_COPYRANGE"""
        self._sci(2419, int(start), int(end))

    def copyText(self, text):
        """See Scintilla documentation for SCI_COPYTEXT"""
        _s = _bytes(text)
        self._sci_ls(2420, len(_s), _s)

    def setSelectionMode(self, selectionMode):
        """See Scintilla documentation for SCI_SETSELECTIONMODE"""
        self._sci(2422, int(selectionMode), 0)

    def changeSelectionMode(self, selectionMode):
        """See Scintilla documentation for SCI_CHANGESELECTIONMODE"""
        self._sci(2659, int(selectionMode), 0)

    def getSelectionMode(self):
        """See Scintilla documentation for SCI_GETSELECTIONMODE"""
        return self._sci(2423, 0, 0)

    def setMoveExtendsSelection(self, moveExtendsSelection):
        """See Scintilla documentation for SCI_SETMOVEEXTENDSSELECTION"""
        self._sci(2719, (1 if moveExtendsSelection else 0), 0)

    def getMoveExtendsSelection(self):
        """See Scintilla documentation for SCI_GETMOVEEXTENDSSELECTION"""
        return self._sci(2706, 0, 0) != 0

    def getLineSelStartPosition(self, line):
        """See Scintilla documentation for SCI_GETLINESELSTARTPOSITION"""
        return self._sci(2424, int(line), 0)

    def getLineSelEndPosition(self, line):
        """See Scintilla documentation for SCI_GETLINESELENDPOSITION"""
        return self._sci(2425, int(line), 0)

    def lineDownRectExtend(self):
        """See Scintilla documentation for SCI_LINEDOWNRECTEXTEND"""
        self._sci(2426, 0, 0)

    def lineUpRectExtend(self):
        """See Scintilla documentation for SCI_LINEUPRECTEXTEND"""
        self._sci(2427, 0, 0)

    def charLeftRectExtend(self):
        """See Scintilla documentation for SCI_CHARLEFTRECTEXTEND"""
        self._sci(2428, 0, 0)

    def charRightRectExtend(self):
        """See Scintilla documentation for SCI_CHARRIGHTRECTEXTEND"""
        self._sci(2429, 0, 0)

    def homeRectExtend(self):
        """See Scintilla documentation for SCI_HOMERECTEXTEND"""
        self._sci(2430, 0, 0)

    def vCHomeRectExtend(self):
        """See Scintilla documentation for SCI_VCHOMERECTEXTEND"""
        self._sci(2431, 0, 0)

    def lineEndRectExtend(self):
        """See Scintilla documentation for SCI_LINEENDRECTEXTEND"""
        self._sci(2432, 0, 0)

    def pageUpRectExtend(self):
        """See Scintilla documentation for SCI_PAGEUPRECTEXTEND"""
        self._sci(2433, 0, 0)

    def pageDownRectExtend(self):
        """See Scintilla documentation for SCI_PAGEDOWNRECTEXTEND"""
        self._sci(2434, 0, 0)

    def stutteredPageUp(self):
        """See Scintilla documentation for SCI_STUTTEREDPAGEUP"""
        self._sci(2435, 0, 0)

    def stutteredPageUpExtend(self):
        """See Scintilla documentation for SCI_STUTTEREDPAGEUPEXTEND"""
        self._sci(2436, 0, 0)

    def stutteredPageDown(self):
        """See Scintilla documentation for SCI_STUTTEREDPAGEDOWN"""
        self._sci(2437, 0, 0)

    def stutteredPageDownExtend(self):
        """See Scintilla documentation for SCI_STUTTEREDPAGEDOWNEXTEND"""
        self._sci(2438, 0, 0)

    def wordLeftEnd(self):
        """See Scintilla documentation for SCI_WORDLEFTEND"""
        self._sci(2439, 0, 0)

    def wordLeftEndExtend(self):
        """See Scintilla documentation for SCI_WORDLEFTENDEXTEND"""
        self._sci(2440, 0, 0)

    def wordRightEnd(self):
        """See Scintilla documentation for SCI_WORDRIGHTEND"""
        self._sci(2441, 0, 0)

    def wordRightEndExtend(self):
        """See Scintilla documentation for SCI_WORDRIGHTENDEXTEND"""
        self._sci(2442, 0, 0)

    def setWhitespaceChars(self, characters):
        """See Scintilla documentation for SCI_SETWHITESPACECHARS"""
        self._sci_ls(2443, 0, _bytes(characters))

    def getWhitespaceChars(self):
        """See Scintilla documentation for SCI_GETWHITESPACECHARS"""
        _n = self._sci(2647, 0, 0)
        return _str(self._sci_res(2647, 0, _n))

    def setPunctuationChars(self, characters):
        """See Scintilla documentation for SCI_SETPUNCTUATIONCHARS"""
        self._sci_ls(2648, 0, _bytes(characters))

    def getPunctuationChars(self):
        """See Scintilla documentation for SCI_GETPUNCTUATIONCHARS"""
        _n = self._sci(2649, 0, 0)
        return _str(self._sci_res(2649, 0, _n))

    def setCharsDefault(self):
        """See Scintilla documentation for SCI_SETCHARSDEFAULT"""
        self._sci(2444, 0, 0)

    def autoCGetCurrent(self):
        """See Scintilla documentation for SCI_AUTOCGETCURRENT"""
        return self._sci(2445, 0, 0)

    def autoCGetCurrentText(self):
        """See Scintilla documentation for SCI_AUTOCGETCURRENTTEXT"""
        _n = self._sci(2610, 0, 0)
        return _str(self._sci_res(2610, 0, _n))

    def autoCSetCaseInsensitiveBehaviour(self, behaviour):
        """See Scintilla documentation for SCI_AUTOCSETCASEINSENSITIVEBEHAVIOUR"""
        self._sci(2634, int(behaviour), 0)

    def autoCGetCaseInsensitiveBehaviour(self):
        """See Scintilla documentation for SCI_AUTOCGETCASEINSENSITIVEBEHAVIOUR"""
        return self._sci(2635, 0, 0)

    def autoCSetMulti(self, multi):
        """See Scintilla documentation for SCI_AUTOCSETMULTI"""
        self._sci(2636, int(multi), 0)

    def autoCGetMulti(self):
        """See Scintilla documentation for SCI_AUTOCGETMULTI"""
        return self._sci(2637, 0, 0)

    def autoCSetOrder(self, order):
        """See Scintilla documentation for SCI_AUTOCSETORDER"""
        self._sci(2660, int(order), 0)

    def autoCGetOrder(self):
        """See Scintilla documentation for SCI_AUTOCGETORDER"""
        return self._sci(2661, 0, 0)

    def allocate(self, bytes):
        """See Scintilla documentation for SCI_ALLOCATE"""
        self._sci(2446, int(bytes), 0)

    def targetAsUTF8(self):
        """See Scintilla documentation for SCI_TARGETASUTF8"""
        _n = self._sci(2447, 0, 0)
        return _str(self._sci_res(2447, 0, _n))

    def setLengthForEncode(self, bytes):
        """See Scintilla documentation for SCI_SETLENGTHFORENCODE"""
        self._sci(2448, int(bytes), 0)

    def encodedFromUTF8(self, utf8):
        """See Scintilla documentation for SCI_ENCODEDFROMUTF8"""
        _k = _bytes(utf8)
        _n = self._sci_ws(2449, _k, 0)
        return _str(self._sci_wres(2449, _k, _n))

    def findColumn(self, line, column):
        """See Scintilla documentation for SCI_FINDCOLUMN"""
        return self._sci(2456, int(line), int(column))

    def getCaretSticky(self):
        """See Scintilla documentation for SCI_GETCARETSTICKY"""
        return self._sci(2457, 0, 0)

    def setCaretSticky(self, useCaretStickyBehaviour):
        """See Scintilla documentation for SCI_SETCARETSTICKY"""
        self._sci(2458, int(useCaretStickyBehaviour), 0)

    def toggleCaretSticky(self):
        """See Scintilla documentation for SCI_TOGGLECARETSTICKY"""
        self._sci(2459, 0, 0)

    def setPasteConvertEndings(self, convert):
        """See Scintilla documentation for SCI_SETPASTECONVERTENDINGS"""
        self._sci(2467, (1 if convert else 0), 0)

    def getPasteConvertEndings(self):
        """See Scintilla documentation for SCI_GETPASTECONVERTENDINGS"""
        return self._sci(2468, 0, 0) != 0

    def replaceRectangular(self, text):
        """See Scintilla documentation for SCI_REPLACERECTANGULAR"""
        _s = _bytes(text)
        self._sci_ls(2771, len(_s), _s)

    def selectionDuplicate(self):
        """See Scintilla documentation for SCI_SELECTIONDUPLICATE"""
        self._sci(2469, 0, 0)

    def setCaretLineBackAlpha(self, alpha):
        """See Scintilla documentation for SCI_SETCARETLINEBACKALPHA"""
        self._sci(2470, int(alpha), 0)

    def getCaretLineBackAlpha(self):
        """See Scintilla documentation for SCI_GETCARETLINEBACKALPHA"""
        return self._sci(2471, 0, 0)

    def setCaretStyle(self, caretStyle):
        """See Scintilla documentation for SCI_SETCARETSTYLE"""
        self._sci(2512, int(caretStyle), 0)

    def getCaretStyle(self):
        """See Scintilla documentation for SCI_GETCARETSTYLE"""
        return self._sci(2513, 0, 0)

    def setIndicatorCurrent(self, indicator):
        """See Scintilla documentation for SCI_SETINDICATORCURRENT"""
        self._sci(2500, int(indicator), 0)

    def getIndicatorCurrent(self):
        """See Scintilla documentation for SCI_GETINDICATORCURRENT"""
        return self._sci(2501, 0, 0)

    def setIndicatorValue(self, value):
        """See Scintilla documentation for SCI_SETINDICATORVALUE"""
        self._sci(2502, int(value), 0)

    def getIndicatorValue(self):
        """See Scintilla documentation for SCI_GETINDICATORVALUE"""
        return self._sci(2503, 0, 0)

    def indicatorFillRange(self, start, lengthFill):
        """See Scintilla documentation for SCI_INDICATORFILLRANGE"""
        self._sci(2504, int(start), int(lengthFill))

    def indicatorClearRange(self, start, lengthClear):
        """See Scintilla documentation for SCI_INDICATORCLEARRANGE"""
        self._sci(2505, int(start), int(lengthClear))

    def indicatorAllOnFor(self, pos):
        """See Scintilla documentation for SCI_INDICATORALLONFOR"""
        return self._sci(2506, int(pos), 0)

    def indicatorValueAt(self, indicator, pos):
        """See Scintilla documentation for SCI_INDICATORVALUEAT"""
        return self._sci(2507, int(indicator), int(pos))

    def indicatorStart(self, indicator, pos):
        """See Scintilla documentation for SCI_INDICATORSTART"""
        return self._sci(2508, int(indicator), int(pos))

    def indicatorEnd(self, indicator, pos):
        """See Scintilla documentation for SCI_INDICATOREND"""
        return self._sci(2509, int(indicator), int(pos))

    def setPositionCache(self, size):
        """See Scintilla documentation for SCI_SETPOSITIONCACHE"""
        self._sci(2514, int(size), 0)

    def getPositionCache(self):
        """See Scintilla documentation for SCI_GETPOSITIONCACHE"""
        return self._sci(2515, 0, 0)

    def setLayoutThreads(self, threads):
        """See Scintilla documentation for SCI_SETLAYOUTTHREADS"""
        self._sci(2775, int(threads), 0)

    def getLayoutThreads(self):
        """See Scintilla documentation for SCI_GETLAYOUTTHREADS"""
        return self._sci(2776, 0, 0)

    def copyAllowLine(self):
        """See Scintilla documentation for SCI_COPYALLOWLINE"""
        self._sci(2519, 0, 0)

    def cutAllowLine(self):
        """See Scintilla documentation for SCI_CUTALLOWLINE"""
        self._sci(2810, 0, 0)

    def setCopySeparator(self, separator):
        """See Scintilla documentation for SCI_SETCOPYSEPARATOR"""
        self._sci_ls(2811, 0, _bytes(separator))

    def getCopySeparator(self):
        """See Scintilla documentation for SCI_GETCOPYSEPARATOR"""
        _n = self._sci(2812, 0, 0)
        return _str(self._sci_res(2812, 0, _n))

    def getCharacterPointer(self):
        """See Scintilla documentation for SCI_GETCHARACTERPOINTER"""
        return self.getText()

    def getRangePointer(self, start, lengthRange):
        """See Scintilla documentation for SCI_GETRANGEPOINTER"""
        return self._range(start, start + lengthRange)

    def getGapPosition(self):
        """See Scintilla documentation for SCI_GETGAPPOSITION"""
        return self._sci(2644, 0, 0)

    def indicSetAlpha(self, indicator, alpha):
        """See Scintilla documentation for SCI_INDICSETALPHA"""
        self._sci(2523, int(indicator), int(alpha))

    def indicGetAlpha(self, indicator):
        """See Scintilla documentation for SCI_INDICGETALPHA"""
        return self._sci(2524, int(indicator), 0)

    def indicSetOutlineAlpha(self, indicator, alpha):
        """See Scintilla documentation for SCI_INDICSETOUTLINEALPHA"""
        self._sci(2558, int(indicator), int(alpha))

    def indicGetOutlineAlpha(self, indicator):
        """See Scintilla documentation for SCI_INDICGETOUTLINEALPHA"""
        return self._sci(2559, int(indicator), 0)

    def setExtraAscent(self, extraAscent):
        """See Scintilla documentation for SCI_SETEXTRAASCENT"""
        self._sci(2525, int(extraAscent), 0)

    def getExtraAscent(self):
        """See Scintilla documentation for SCI_GETEXTRAASCENT"""
        return self._sci(2526, 0, 0)

    def setExtraDescent(self, extraDescent):
        """See Scintilla documentation for SCI_SETEXTRADESCENT"""
        self._sci(2527, int(extraDescent), 0)

    def getExtraDescent(self):
        """See Scintilla documentation for SCI_GETEXTRADESCENT"""
        return self._sci(2528, 0, 0)

    def markerSymbolDefined(self, markerNumber):
        """See Scintilla documentation for SCI_MARKERSYMBOLDEFINED"""
        return self._sci(2529, int(markerNumber), 0)

    def marginSetText(self, line, text):
        """See Scintilla documentation for SCI_MARGINSETTEXT"""
        self._sci_ls(2530, int(line), _bytes(text))

    def marginGetText(self, line):
        """See Scintilla documentation for SCI_MARGINGETTEXT"""
        _n = self._sci(2531, int(line), 0)
        return _str(self._sci_res(2531, int(line), _n))

    def marginSetStyle(self, line, style):
        """See Scintilla documentation for SCI_MARGINSETSTYLE"""
        self._sci(2532, int(line), int(style))

    def marginGetStyle(self, line):
        """See Scintilla documentation for SCI_MARGINGETSTYLE"""
        return self._sci(2533, int(line), 0)

    def marginSetStyles(self, line, styles):
        """See Scintilla documentation for SCI_MARGINSETSTYLES"""
        self._sci_ls(2534, int(line), _bytes(styles))

    def marginGetStyles(self, line):
        """See Scintilla documentation for SCI_MARGINGETSTYLES"""
        _n = self._sci(2535, int(line), 0)
        return _str(self._sci_res(2535, int(line), _n))

    def marginTextClearAll(self):
        """See Scintilla documentation for SCI_MARGINTEXTCLEARALL"""
        self._sci(2536, 0, 0)

    def marginSetStyleOffset(self, style):
        """See Scintilla documentation for SCI_MARGINSETSTYLEOFFSET"""
        self._sci(2537, int(style), 0)

    def marginGetStyleOffset(self):
        """See Scintilla documentation for SCI_MARGINGETSTYLEOFFSET"""
        return self._sci(2538, 0, 0)

    def setMarginOptions(self, marginOptions):
        """See Scintilla documentation for SCI_SETMARGINOPTIONS"""
        self._sci(2539, int(marginOptions), 0)

    def getMarginOptions(self):
        """See Scintilla documentation for SCI_GETMARGINOPTIONS"""
        return self._sci(2557, 0, 0)

    def annotationSetText(self, line, text):
        """See Scintilla documentation for SCI_ANNOTATIONSETTEXT"""
        if text is None:
            self._sci(2540, int(line), 0)
        else:
            self._sci_ls(2540, int(line), _bytes(text))

    def annotationGetText(self, line):
        """See Scintilla documentation for SCI_ANNOTATIONGETTEXT"""
        _n = self._sci(2541, int(line), 0)
        return _str(self._sci_res(2541, int(line), _n))

    def annotationSetStyle(self, line, style):
        """See Scintilla documentation for SCI_ANNOTATIONSETSTYLE"""
        self._sci(2542, int(line), int(style))

    def annotationGetStyle(self, line):
        """See Scintilla documentation for SCI_ANNOTATIONGETSTYLE"""
        return self._sci(2543, int(line), 0)

    def annotationSetStyles(self, line, styles):
        """See Scintilla documentation for SCI_ANNOTATIONSETSTYLES"""
        self._sci_ls(2544, int(line), _bytes(styles))

    def annotationGetStyles(self, line):
        """See Scintilla documentation for SCI_ANNOTATIONGETSTYLES"""
        _n = self._sci(2545, int(line), 0)
        return _str(self._sci_res(2545, int(line), _n))

    def annotationGetLines(self, line):
        """See Scintilla documentation for SCI_ANNOTATIONGETLINES"""
        return self._sci(2546, int(line), 0)

    def annotationClearAll(self):
        """See Scintilla documentation for SCI_ANNOTATIONCLEARALL"""
        self._sci(2547, 0, 0)

    def annotationSetVisible(self, visible):
        """See Scintilla documentation for SCI_ANNOTATIONSETVISIBLE"""
        self._sci(2548, int(visible), 0)

    def annotationGetVisible(self):
        """See Scintilla documentation for SCI_ANNOTATIONGETVISIBLE"""
        return self._sci(2549, 0, 0)

    def annotationSetStyleOffset(self, style):
        """See Scintilla documentation for SCI_ANNOTATIONSETSTYLEOFFSET"""
        self._sci(2550, int(style), 0)

    def annotationGetStyleOffset(self):
        """See Scintilla documentation for SCI_ANNOTATIONGETSTYLEOFFSET"""
        return self._sci(2551, 0, 0)

    def releaseAllExtendedStyles(self):
        """See Scintilla documentation for SCI_RELEASEALLEXTENDEDSTYLES"""
        self._sci(2552, 0, 0)

    def allocateExtendedStyles(self, numberStyles):
        """See Scintilla documentation for SCI_ALLOCATEEXTENDEDSTYLES"""
        return self._sci(2553, int(numberStyles), 0)

    def addUndoAction(self, token, flags):
        """See Scintilla documentation for SCI_ADDUNDOACTION"""
        self._sci(2560, int(token), int(flags))

    def charPositionFromPoint(self, x, y):
        """See Scintilla documentation for SCI_CHARPOSITIONFROMPOINT"""
        return self._sci(2561, int(x), int(y))

    def charPositionFromPointClose(self, x, y):
        """See Scintilla documentation for SCI_CHARPOSITIONFROMPOINTCLOSE"""
        return self._sci(2562, int(x), int(y))

    def setMouseSelectionRectangularSwitch(self, mouseSelectionRectangularSwitch):
        """See Scintilla documentation for SCI_SETMOUSESELECTIONRECTANGULARSWITCH"""
        self._sci(2668, (1 if mouseSelectionRectangularSwitch else 0), 0)

    def getMouseSelectionRectangularSwitch(self):
        """See Scintilla documentation for SCI_GETMOUSESELECTIONRECTANGULARSWITCH"""
        return self._sci(2669, 0, 0) != 0

    def setMultipleSelection(self, multipleSelection):
        """See Scintilla documentation for SCI_SETMULTIPLESELECTION"""
        self._sci(2563, (1 if multipleSelection else 0), 0)

    def getMultipleSelection(self):
        """See Scintilla documentation for SCI_GETMULTIPLESELECTION"""
        return self._sci(2564, 0, 0) != 0

    def setAdditionalSelectionTyping(self, additionalSelectionTyping):
        """See Scintilla documentation for SCI_SETADDITIONALSELECTIONTYPING"""
        self._sci(2565, (1 if additionalSelectionTyping else 0), 0)

    def getAdditionalSelectionTyping(self):
        """See Scintilla documentation for SCI_GETADDITIONALSELECTIONTYPING"""
        return self._sci(2566, 0, 0) != 0

    def setAdditionalCaretsBlink(self, additionalCaretsBlink):
        """See Scintilla documentation for SCI_SETADDITIONALCARETSBLINK"""
        self._sci(2567, (1 if additionalCaretsBlink else 0), 0)

    def getAdditionalCaretsBlink(self):
        """See Scintilla documentation for SCI_GETADDITIONALCARETSBLINK"""
        return self._sci(2568, 0, 0) != 0

    def setAdditionalCaretsVisible(self, additionalCaretsVisible):
        """See Scintilla documentation for SCI_SETADDITIONALCARETSVISIBLE"""
        self._sci(2608, (1 if additionalCaretsVisible else 0), 0)

    def getAdditionalCaretsVisible(self):
        """See Scintilla documentation for SCI_GETADDITIONALCARETSVISIBLE"""
        return self._sci(2609, 0, 0) != 0

    def getSelections(self):
        """See Scintilla documentation for SCI_GETSELECTIONS"""
        return self._sci(2570, 0, 0)

    def getSelectionEmpty(self):
        """See Scintilla documentation for SCI_GETSELECTIONEMPTY"""
        return self._sci(2650, 0, 0) != 0

    def clearSelections(self):
        """See Scintilla documentation for SCI_CLEARSELECTIONS"""
        self._sci(2571, 0, 0)

    def setSelection(self, caret, anchor):
        """See Scintilla documentation for SCI_SETSELECTION"""
        self._sci(2572, int(caret), int(anchor))

    def addSelection(self, caret, anchor):
        """See Scintilla documentation for SCI_ADDSELECTION"""
        self._sci(2573, int(caret), int(anchor))

    def selectionFromPoint(self, x, y):
        """See Scintilla documentation for SCI_SELECTIONFROMPOINT"""
        return self._sci(2474, int(x), int(y))

    def dropSelectionN(self, selection):
        """See Scintilla documentation for SCI_DROPSELECTIONN"""
        self._sci(2671, int(selection), 0)

    def setMainSelection(self, selection):
        """See Scintilla documentation for SCI_SETMAINSELECTION"""
        self._sci(2574, int(selection), 0)

    def getMainSelection(self):
        """See Scintilla documentation for SCI_GETMAINSELECTION"""
        return self._sci(2575, 0, 0)

    def setSelectionNCaret(self, selection, caret):
        """See Scintilla documentation for SCI_SETSELECTIONNCARET"""
        self._sci(2576, int(selection), int(caret))

    def getSelectionNCaret(self, selection):
        """See Scintilla documentation for SCI_GETSELECTIONNCARET"""
        return self._sci(2577, int(selection), 0)

    def setSelectionNAnchor(self, selection, anchor):
        """See Scintilla documentation for SCI_SETSELECTIONNANCHOR"""
        self._sci(2578, int(selection), int(anchor))

    def getSelectionNAnchor(self, selection):
        """See Scintilla documentation for SCI_GETSELECTIONNANCHOR"""
        return self._sci(2579, int(selection), 0)

    def setSelectionNCaretVirtualSpace(self, selection, space):
        """See Scintilla documentation for SCI_SETSELECTIONNCARETVIRTUALSPACE"""
        self._sci(2580, int(selection), int(space))

    def getSelectionNCaretVirtualSpace(self, selection):
        """See Scintilla documentation for SCI_GETSELECTIONNCARETVIRTUALSPACE"""
        return self._sci(2581, int(selection), 0)

    def setSelectionNAnchorVirtualSpace(self, selection, space):
        """See Scintilla documentation for SCI_SETSELECTIONNANCHORVIRTUALSPACE"""
        self._sci(2582, int(selection), int(space))

    def getSelectionNAnchorVirtualSpace(self, selection):
        """See Scintilla documentation for SCI_GETSELECTIONNANCHORVIRTUALSPACE"""
        return self._sci(2583, int(selection), 0)

    def setSelectionNStart(self, selection, anchor):
        """See Scintilla documentation for SCI_SETSELECTIONNSTART"""
        self._sci(2584, int(selection), int(anchor))

    def getSelectionNStart(self, selection):
        """See Scintilla documentation for SCI_GETSELECTIONNSTART"""
        return self._sci(2585, int(selection), 0)

    def getSelectionNStartVirtualSpace(self, selection):
        """See Scintilla documentation for SCI_GETSELECTIONNSTARTVIRTUALSPACE"""
        return self._sci(2726, int(selection), 0)

    def setSelectionNEnd(self, selection, caret):
        """See Scintilla documentation for SCI_SETSELECTIONNEND"""
        self._sci(2586, int(selection), int(caret))

    def getSelectionNEndVirtualSpace(self, selection):
        """See Scintilla documentation for SCI_GETSELECTIONNENDVIRTUALSPACE"""
        return self._sci(2727, int(selection), 0)

    def getSelectionNEnd(self, selection):
        """See Scintilla documentation for SCI_GETSELECTIONNEND"""
        return self._sci(2587, int(selection), 0)

    def setRectangularSelectionCaret(self, caret):
        """See Scintilla documentation for SCI_SETRECTANGULARSELECTIONCARET"""
        self._sci(2588, int(caret), 0)

    def getRectangularSelectionCaret(self):
        """See Scintilla documentation for SCI_GETRECTANGULARSELECTIONCARET"""
        return self._sci(2589, 0, 0)

    def setRectangularSelectionAnchor(self, anchor):
        """See Scintilla documentation for SCI_SETRECTANGULARSELECTIONANCHOR"""
        self._sci(2590, int(anchor), 0)

    def getRectangularSelectionAnchor(self):
        """See Scintilla documentation for SCI_GETRECTANGULARSELECTIONANCHOR"""
        return self._sci(2591, 0, 0)

    def setRectangularSelectionCaretVirtualSpace(self, space):
        """See Scintilla documentation for SCI_SETRECTANGULARSELECTIONCARETVIRTUALSPACE"""
        self._sci(2592, int(space), 0)

    def getRectangularSelectionCaretVirtualSpace(self):
        """See Scintilla documentation for SCI_GETRECTANGULARSELECTIONCARETVIRTUALSPACE"""
        return self._sci(2593, 0, 0)

    def setRectangularSelectionAnchorVirtualSpace(self, space):
        """See Scintilla documentation for SCI_SETRECTANGULARSELECTIONANCHORVIRTUALSPACE"""
        self._sci(2594, int(space), 0)

    def getRectangularSelectionAnchorVirtualSpace(self):
        """See Scintilla documentation for SCI_GETRECTANGULARSELECTIONANCHORVIRTUALSPACE"""
        return self._sci(2595, 0, 0)

    def setVirtualSpaceOptions(self, virtualSpaceOptions):
        """See Scintilla documentation for SCI_SETVIRTUALSPACEOPTIONS"""
        self._sci(2596, int(virtualSpaceOptions), 0)

    def getVirtualSpaceOptions(self):
        """See Scintilla documentation for SCI_GETVIRTUALSPACEOPTIONS"""
        return self._sci(2597, 0, 0)

    def setRectangularSelectionModifier(self, modifier):
        """See Scintilla documentation for SCI_SETRECTANGULARSELECTIONMODIFIER"""
        self._sci(2598, int(modifier), 0)

    def getRectangularSelectionModifier(self):
        """See Scintilla documentation for SCI_GETRECTANGULARSELECTIONMODIFIER"""
        return self._sci(2599, 0, 0)

    def setAdditionalSelFore(self, fore):
        """See Scintilla documentation for SCI_SETADDITIONALSELFORE"""
        self._sci(2600, _rgb(fore), 0)

    def setAdditionalSelBack(self, back):
        """See Scintilla documentation for SCI_SETADDITIONALSELBACK"""
        self._sci(2601, _rgb(back), 0)

    def setAdditionalSelAlpha(self, alpha):
        """See Scintilla documentation for SCI_SETADDITIONALSELALPHA"""
        self._sci(2602, int(alpha), 0)

    def getAdditionalSelAlpha(self):
        """See Scintilla documentation for SCI_GETADDITIONALSELALPHA"""
        return self._sci(2603, 0, 0)

    def setAdditionalCaretFore(self, fore):
        """See Scintilla documentation for SCI_SETADDITIONALCARETFORE"""
        self._sci(2604, _rgb(fore), 0)

    def getAdditionalCaretFore(self):
        """See Scintilla documentation for SCI_GETADDITIONALCARETFORE"""
        return _colour_tuple(self._sci(2605, 0, 0))

    def rotateSelection(self):
        """See Scintilla documentation for SCI_ROTATESELECTION"""
        self._sci(2606, 0, 0)

    def swapMainAnchorCaret(self):
        """See Scintilla documentation for SCI_SWAPMAINANCHORCARET"""
        self._sci(2607, 0, 0)

    def multipleSelectAddNext(self):
        """See Scintilla documentation for SCI_MULTIPLESELECTADDNEXT"""
        self._sci(2688, 0, 0)

    def multipleSelectAddEach(self):
        """See Scintilla documentation for SCI_MULTIPLESELECTADDEACH"""
        self._sci(2689, 0, 0)

    def changeLexerState(self, start, end):
        """See Scintilla documentation for SCI_CHANGELEXERSTATE"""
        return self._sci(2617, int(start), int(end))

    def contractedFoldNext(self, lineStart):
        """See Scintilla documentation for SCI_CONTRACTEDFOLDNEXT"""
        return self._sci(2618, int(lineStart), 0)

    def verticalCentreCaret(self):
        """See Scintilla documentation for SCI_VERTICALCENTRECARET"""
        self._sci(2619, 0, 0)

    def moveSelectedLinesUp(self):
        """See Scintilla documentation for SCI_MOVESELECTEDLINESUP"""
        self._sci(2620, 0, 0)

    def moveSelectedLinesDown(self):
        """See Scintilla documentation for SCI_MOVESELECTEDLINESDOWN"""
        self._sci(2621, 0, 0)

    def setIdentifier(self, identifier):
        """See Scintilla documentation for SCI_SETIDENTIFIER"""
        self._sci(2622, int(identifier), 0)

    def getIdentifier(self):
        """See Scintilla documentation for SCI_GETIDENTIFIER"""
        return self._sci(2623, 0, 0)

    def rGBAImageSetWidth(self, width):
        """See Scintilla documentation for SCI_RGBAIMAGESETWIDTH"""
        self._sci(2624, int(width), 0)

    def rGBAImageSetHeight(self, height):
        """See Scintilla documentation for SCI_RGBAIMAGESETHEIGHT"""
        self._sci(2625, int(height), 0)

    def rGBAImageSetScale(self, scalePercent):
        """See Scintilla documentation for SCI_RGBAIMAGESETSCALE"""
        self._sci(2651, int(scalePercent), 0)

    def markerDefineRGBAImage(self, markerNumber, pixels):
        """See Scintilla documentation for SCI_MARKERDEFINERGBAIMAGE"""
        self._sci_ls(2626, int(markerNumber), _bytes(pixels))

    def registerRGBAImage(self, type, pixels):
        """See Scintilla documentation for SCI_REGISTERRGBAIMAGE"""
        self._sci_ls(2627, int(type), _bytes(pixels))

    def scrollToStart(self):
        """See Scintilla documentation for SCI_SCROLLTOSTART"""
        self._sci(2628, 0, 0)

    def scrollToEnd(self):
        """See Scintilla documentation for SCI_SCROLLTOEND"""
        self._sci(2629, 0, 0)

    def setTechnology(self, technology):
        """See Scintilla documentation for SCI_SETTECHNOLOGY"""
        self._sci(2630, int(technology), 0)

    def getTechnology(self):
        """See Scintilla documentation for SCI_GETTECHNOLOGY"""
        return self._sci(2631, 0, 0)

    def createLoader(self, bytes, documentOptions):
        """See Scintilla documentation for SCI_CREATELOADER"""
        return self._sci(2632, int(bytes), int(documentOptions))

    def findIndicatorShow(self, start, end):
        """See Scintilla documentation for SCI_FINDINDICATORSHOW"""
        self._sci(2640, int(start), int(end))

    def findIndicatorFlash(self, start, end):
        """See Scintilla documentation for SCI_FINDINDICATORFLASH"""
        self._sci(2641, int(start), int(end))

    def findIndicatorHide(self):
        """See Scintilla documentation for SCI_FINDINDICATORHIDE"""
        self._sci(2642, 0, 0)

    def vCHomeDisplay(self):
        """See Scintilla documentation for SCI_VCHOMEDISPLAY"""
        self._sci(2652, 0, 0)

    def vCHomeDisplayExtend(self):
        """See Scintilla documentation for SCI_VCHOMEDISPLAYEXTEND"""
        self._sci(2653, 0, 0)

    def getCaretLineVisibleAlways(self):
        """See Scintilla documentation for SCI_GETCARETLINEVISIBLEALWAYS"""
        return self._sci(2654, 0, 0) != 0

    def setCaretLineVisibleAlways(self, alwaysVisible):
        """See Scintilla documentation for SCI_SETCARETLINEVISIBLEALWAYS"""
        self._sci(2655, (1 if alwaysVisible else 0), 0)

    def setLineEndTypesAllowed(self, lineEndBitSet):
        """See Scintilla documentation for SCI_SETLINEENDTYPESALLOWED"""
        self._sci(2656, int(lineEndBitSet), 0)

    def getLineEndTypesAllowed(self):
        """See Scintilla documentation for SCI_GETLINEENDTYPESALLOWED"""
        return self._sci(2657, 0, 0)

    def getLineEndTypesActive(self):
        """See Scintilla documentation for SCI_GETLINEENDTYPESACTIVE"""
        return self._sci(2658, 0, 0)

    def setRepresentation(self, encodedCharacter, representation):
        """See Scintilla documentation for SCI_SETREPRESENTATION"""
        self._sci_ss(2665, _bytes(encodedCharacter), _bytes(representation))

    def getRepresentation(self, encodedCharacter):
        """See Scintilla documentation for SCI_GETREPRESENTATION"""
        _k = _bytes(encodedCharacter)
        _n = self._sci_ws(2666, _k, 0)
        return _str(self._sci_wres(2666, _k, _n))

    def clearRepresentation(self, encodedCharacter):
        """See Scintilla documentation for SCI_CLEARREPRESENTATION"""
        self._sci_ws(2667, _bytes(encodedCharacter), 0)

    def clearAllRepresentations(self):
        """See Scintilla documentation for SCI_CLEARALLREPRESENTATIONS"""
        self._sci(2770, 0, 0)

    def setRepresentationAppearance(self, encodedCharacter, appearance):
        """See Scintilla documentation for SCI_SETREPRESENTATIONAPPEARANCE"""
        self._sci_ws(2766, _bytes(encodedCharacter), int(appearance))

    def getRepresentationAppearance(self, encodedCharacter):
        """See Scintilla documentation for SCI_GETREPRESENTATIONAPPEARANCE"""
        return self._sci_ws(2767, _bytes(encodedCharacter), 0)

    def setRepresentationColour(self, encodedCharacter, colour):
        """See Scintilla documentation for SCI_SETREPRESENTATIONCOLOUR"""
        self._sci_ws(2768, _bytes(encodedCharacter), _rgba(colour))

    def getRepresentationColour(self, encodedCharacter):
        """See Scintilla documentation for SCI_GETREPRESENTATIONCOLOUR"""
        return _colouralpha_tuple(self._sci_ws(2769, _bytes(encodedCharacter), 0))

    def eOLAnnotationSetText(self, line, text):
        """See Scintilla documentation for SCI_EOLANNOTATIONSETTEXT"""
        if text is None:
            self._sci(2740, int(line), 0)
        else:
            self._sci_ls(2740, int(line), _bytes(text))

    def eOLAnnotationGetText(self, line):
        """See Scintilla documentation for SCI_EOLANNOTATIONGETTEXT"""
        _n = self._sci(2741, int(line), 0)
        return _str(self._sci_res(2741, int(line), _n))

    def eOLAnnotationSetStyle(self, line, style):
        """See Scintilla documentation for SCI_EOLANNOTATIONSETSTYLE"""
        self._sci(2742, int(line), int(style))

    def eOLAnnotationGetStyle(self, line):
        """See Scintilla documentation for SCI_EOLANNOTATIONGETSTYLE"""
        return self._sci(2743, int(line), 0)

    def eOLAnnotationClearAll(self):
        """See Scintilla documentation for SCI_EOLANNOTATIONCLEARALL"""
        self._sci(2744, 0, 0)

    def eOLAnnotationSetVisible(self, visible):
        """See Scintilla documentation for SCI_EOLANNOTATIONSETVISIBLE"""
        self._sci(2745, int(visible), 0)

    def eOLAnnotationGetVisible(self):
        """See Scintilla documentation for SCI_EOLANNOTATIONGETVISIBLE"""
        return self._sci(2746, 0, 0)

    def eOLAnnotationSetStyleOffset(self, style):
        """See Scintilla documentation for SCI_EOLANNOTATIONSETSTYLEOFFSET"""
        self._sci(2747, int(style), 0)

    def eOLAnnotationGetStyleOffset(self):
        """See Scintilla documentation for SCI_EOLANNOTATIONGETSTYLEOFFSET"""
        return self._sci(2748, 0, 0)

    def supportsFeature(self, feature):
        """See Scintilla documentation for SCI_SUPPORTSFEATURE"""
        return self._sci(2750, int(feature), 0) != 0

    def getLineCharacterIndex(self):
        """See Scintilla documentation for SCI_GETLINECHARACTERINDEX"""
        return self._sci(2710, 0, 0)

    def allocateLineCharacterIndex(self, lineCharacterIndex):
        """See Scintilla documentation for SCI_ALLOCATELINECHARACTERINDEX"""
        self._sci(2711, int(lineCharacterIndex), 0)

    def releaseLineCharacterIndex(self, lineCharacterIndex):
        """See Scintilla documentation for SCI_RELEASELINECHARACTERINDEX"""
        self._sci(2712, int(lineCharacterIndex), 0)

    def lineFromIndexPosition(self, pos, lineCharacterIndex):
        """See Scintilla documentation for SCI_LINEFROMINDEXPOSITION"""
        return self._sci(2713, int(pos), int(lineCharacterIndex))

    def indexPositionFromLine(self, line, lineCharacterIndex):
        """See Scintilla documentation for SCI_INDEXPOSITIONFROMLINE"""
        return self._sci(2714, int(line), int(lineCharacterIndex))

    def getDragDropEnabled(self):
        """See Scintilla documentation for SCI_GETDRAGDROPENABLED"""
        return self._sci(2818, 0, 0) != 0

    def setDragDropEnabled(self, dragDropEnabled):
        """See Scintilla documentation for SCI_SETDRAGDROPENABLED"""
        self._sci(2819, (1 if dragDropEnabled else 0), 0)

    def startRecord(self):
        """See Scintilla documentation for SCI_STARTRECORD"""
        self._sci(3001, 0, 0)

    def stopRecord(self):
        """See Scintilla documentation for SCI_STOPRECORD"""
        self._sci(3002, 0, 0)

    def getLexer(self):
        """See Scintilla documentation for SCI_GETLEXER"""
        return self._sci(4002, 0, 0)

    def colourise(self, start, end):
        """See Scintilla documentation for SCI_COLOURISE"""
        self._sci(4003, int(start), int(end))

    def setProperty(self, key, value):
        """See Scintilla documentation for SCI_SETPROPERTY"""
        self._sci_ss(4004, _bytes(key), _bytes(value))

    def setKeyWords(self, keyWordSet, keyWords):
        """See Scintilla documentation for SCI_SETKEYWORDS"""
        self._sci_ls(4005, int(keyWordSet), _bytes(keyWords))

    def getProperty(self, key):
        """See Scintilla documentation for SCI_GETPROPERTY"""
        _k = _bytes(key)
        _n = self._sci_ws(4008, _k, 0)
        return _str(self._sci_wres(4008, _k, _n))

    def getPropertyExpanded(self, key):
        """See Scintilla documentation for SCI_GETPROPERTYEXPANDED"""
        _k = _bytes(key)
        _n = self._sci_ws(4009, _k, 0)
        return _str(self._sci_wres(4009, _k, _n))

    def getPropertyInt(self, key, defaultValue):
        """See Scintilla documentation for SCI_GETPROPERTYINT"""
        return self._sci_ws(4010, _bytes(key), int(defaultValue))

    def getLexerLanguage(self):
        """See Scintilla documentation for SCI_GETLEXERLANGUAGE"""
        _n = self._sci(4012, 0, 0)
        return _str(self._sci_res(4012, 0, _n))

    def privateLexerCall(self, operation, pointer):
        """See Scintilla documentation for SCI_PRIVATELEXERCALL"""
        return self._sci(4013, int(operation), int(pointer))

    def propertyNames(self):
        """See Scintilla documentation for SCI_PROPERTYNAMES"""
        _n = self._sci(4014, 0, 0)
        return _str(self._sci_res(4014, 0, _n))

    def propertyType(self, name):
        """See Scintilla documentation for SCI_PROPERTYTYPE"""
        return self._sci_ws(4015, _bytes(name), 0)

    def describeProperty(self, name):
        """See Scintilla documentation for SCI_DESCRIBEPROPERTY"""
        _k = _bytes(name)
        _n = self._sci_ws(4016, _k, 0)
        return _str(self._sci_wres(4016, _k, _n))

    def describeKeyWordSets(self):
        """See Scintilla documentation for SCI_DESCRIBEKEYWORDSETS"""
        _n = self._sci(4017, 0, 0)
        return _str(self._sci_res(4017, 0, _n))

    def getLineEndTypesSupported(self):
        """See Scintilla documentation for SCI_GETLINEENDTYPESSUPPORTED"""
        return self._sci(4018, 0, 0)

    def allocateSubStyles(self, styleBase, numberStyles):
        """See Scintilla documentation for SCI_ALLOCATESUBSTYLES"""
        return self._sci(4020, int(styleBase), int(numberStyles))

    def getSubStylesStart(self, styleBase):
        """See Scintilla documentation for SCI_GETSUBSTYLESSTART"""
        return self._sci(4021, int(styleBase), 0)

    def getSubStylesLength(self, styleBase):
        """See Scintilla documentation for SCI_GETSUBSTYLESLENGTH"""
        return self._sci(4022, int(styleBase), 0)

    def getStyleFromSubStyle(self, subStyle):
        """See Scintilla documentation for SCI_GETSTYLEFROMSUBSTYLE"""
        return self._sci(4027, int(subStyle), 0)

    def getPrimaryStyleFromStyle(self, style):
        """See Scintilla documentation for SCI_GETPRIMARYSTYLEFROMSTYLE"""
        return self._sci(4028, int(style), 0)

    def freeSubStyles(self):
        """See Scintilla documentation for SCI_FREESUBSTYLES"""
        self._sci(4023, 0, 0)

    def setIdentifiers(self, style, identifiers):
        """See Scintilla documentation for SCI_SETIDENTIFIERS"""
        self._sci_ls(4024, int(style), _bytes(identifiers))

    def distanceToSecondaryStyles(self):
        """See Scintilla documentation for SCI_DISTANCETOSECONDARYSTYLES"""
        return self._sci(4025, 0, 0)

    def getSubStyleBases(self):
        """See Scintilla documentation for SCI_GETSUBSTYLEBASES"""
        _n = self._sci(4026, 0, 0)
        return _str(self._sci_res(4026, 0, _n))

    def getNamedStyles(self):
        """See Scintilla documentation for SCI_GETNAMEDSTYLES"""
        return self._sci(4029, 0, 0)

    def nameOfStyle(self, style):
        """See Scintilla documentation for SCI_NAMEOFSTYLE"""
        _n = self._sci(4030, int(style), 0)
        return _str(self._sci_res(4030, int(style), _n))

    def tagsOfStyle(self, style):
        """See Scintilla documentation for SCI_TAGSOFSTYLE"""
        _n = self._sci(4031, int(style), 0)
        return _str(self._sci_res(4031, int(style), _n))

    def descriptionOfStyle(self, style):
        """See Scintilla documentation for SCI_DESCRIPTIONOFSTYLE"""
        _n = self._sci(4032, int(style), 0)
        return _str(self._sci_res(4032, int(style), _n))

    def setILexer(self, ilexer):
        """See Scintilla documentation for SCI_SETILEXER"""
        self._sci(4033, 0, int(ilexer))

    def setScaleTechnique(self, technique):
        """See Scintilla documentation for SCI_SETSCALETECHNIQUE"""
        self._sci(2820, int(technique), 0)

    def getScaleTechnique(self):
        """See Scintilla documentation for SCI_GETSCALETECHNIQUE"""
        return self._sci(2821, 0, 0)

    def getBidirectional(self):
        """See Scintilla documentation for SCI_GETBIDIRECTIONAL"""
        return self._sci(2708, 0, 0)

    def setBidirectional(self, bidirectional):
        """See Scintilla documentation for SCI_SETBIDIRECTIONAL"""
        self._sci(2709, int(bidirectional), 0)


class WHITESPACE(_Enum):
    INVISIBLE = 0
    VISIBLEALWAYS = 1
    VISIBLEAFTERINDENT = 2
    VISIBLEONLYININDENT = 3


class TABDRAWMODE(_Enum):
    LONGARROW = 0
    STRIKEOUT = 1
    CONTROLCHAR = 2


class ENDOFLINE(_Enum):
    CRLF = 0
    CR = 1
    LF = 2


class IMEINTERACTION(_Enum):
    WINDOWED = 0
    INLINE = 1


class ALPHA(_Enum):
    TRANSPARENT = 0
    OPAQUE = 255
    NOALPHA = 256


class CURSORSHAPE(_Enum):
    NORMAL = -1
    ARROW = 2
    WAIT = 4
    REVERSEARROW = 7


class MARKERSYMBOL(_Enum):
    CIRCLE = 0
    ROUNDRECT = 1
    ARROW = 2
    SMALLRECT = 3
    SHORTARROW = 4
    EMPTY = 5
    ARROWDOWN = 6
    MINUS = 7
    PLUS = 8
    VLINE = 9
    LCORNER = 10
    TCORNER = 11
    BOXPLUS = 12
    BOXPLUSCONNECTED = 13
    BOXMINUS = 14
    BOXMINUSCONNECTED = 15
    LCORNERCURVE = 16
    TCORNERCURVE = 17
    CIRCLEPLUS = 18
    CIRCLEPLUSCONNECTED = 19
    CIRCLEMINUS = 20
    CIRCLEMINUSCONNECTED = 21
    BACKGROUND = 22
    DOTDOTDOT = 23
    ARROWS = 24
    PIXMAP = 25
    FULLRECT = 26
    LEFTRECT = 27
    AVAILABLE = 28
    UNDERLINE = 29
    RGBAIMAGE = 30
    BOOKMARK = 31
    VERTICALBOOKMARK = 32
    BAR = 33
    CHARACTER = 10000


class MARKEROUTLINE(_Enum):
    HISTORY_REVERTED_TO_ORIGIN = 21
    HISTORY_SAVED = 22
    HISTORY_MODIFIED = 23
    HISTORY_REVERTED_TO_MODIFIED = 24
    FOLDEREND = 25
    FOLDEROPENMID = 26
    FOLDERMIDTAIL = 27
    FOLDERTAIL = 28
    FOLDERSUB = 29
    FOLDER = 30
    FOLDEROPEN = 31


class MARGINTYPE(_Enum):
    SYMBOL = 0
    NUMBER = 1
    BACK = 2
    FORE = 3
    TEXT = 4
    RTEXT = 5
    COLOUR = 6


class STYLESCOMMON(_Enum):
    DEFAULT = 32
    LINENUMBER = 33
    BRACELIGHT = 34
    BRACEBAD = 35
    CONTROLCHAR = 36
    INDENTGUIDE = 37
    CALLTIP = 38
    FOLDDISPLAYTEXT = 39
    LASTPREDEFINED = 39
    MAX = 255


class CHARACTERSET(_Enum):
    ANSI = 0
    DEFAULT = 1
    BALTIC = 186
    CHINESEBIG5 = 136
    EASTEUROPE = 238
    GB2312 = 134
    GREEK = 161
    HANGUL = 129
    MAC = 77
    OEM = 255
    RUSSIAN = 204
    OEM866 = 866
    CYRILLIC = 1251
    SHIFTJIS = 128
    SYMBOL = 2
    TURKISH = 162
    JOHAB = 130
    HEBREW = 177
    ARABIC = 178
    VIETNAMESE = 163
    THAI = 222


class CASEVISIBLE(_Enum):
    MIXED = 0
    UPPER = 1
    LOWER = 2
    CAMEL = 3


class FONTWEIGHT(_Enum):
    NORMAL = 400
    SEMIBOLD = 600
    BOLD = 700


class FONTSTRETCH(_Enum):
    ULTRA_CONDENSED = 1
    EXTRA_CONDENSED = 2
    CONDENSED = 3
    SEMI_CONDENSED = 4
    NORMAL = 5
    SEMI_EXPANDED = 6
    EXPANDED = 7
    EXTRA_EXPANDED = 8
    ULTRA_EXPANDED = 9


class ELEMENT(_Enum):
    LIST = 0
    LIST_BACK = 1
    LIST_SELECTED = 2
    LIST_SELECTED_BACK = 3
    SELECTION_TEXT = 10
    SELECTION_BACK = 11
    SELECTION_ADDITIONAL_TEXT = 12
    SELECTION_ADDITIONAL_BACK = 13
    SELECTION_SECONDARY_TEXT = 14
    SELECTION_SECONDARY_BACK = 15
    SELECTION_INACTIVE_TEXT = 16
    SELECTION_INACTIVE_BACK = 17
    SELECTION_INACTIVE_ADDITIONAL_TEXT = 18
    SELECTION_INACTIVE_ADDITIONAL_BACK = 19
    CARET = 40
    CARET_ADDITIONAL = 41
    CARET_LINE_BACK = 50
    WHITE_SPACE = 60
    WHITE_SPACE_BACK = 61
    HOT_SPOT_ACTIVE = 70
    HOT_SPOT_ACTIVE_BACK = 71
    FOLD_LINE = 80
    HIDDEN_LINE = 81


class LAYER(_Enum):
    BASE = 0
    UNDER_TEXT = 1
    OVER_TEXT = 2


class INDICATORSTYLE(_Enum):
    PLAIN = 0
    SQUIGGLE = 1
    TT = 2
    DIAGONAL = 3
    STRIKE = 4
    HIDDEN = 5
    BOX = 6
    ROUNDBOX = 7
    STRAIGHTBOX = 8
    DASH = 9
    DOTS = 10
    SQUIGGLELOW = 11
    DOTBOX = 12
    SQUIGGLEPIXMAP = 13
    COMPOSITIONTHICK = 14
    COMPOSITIONTHIN = 15
    FULLBOX = 16
    TEXTFORE = 17
    POINT = 18
    POINTCHARACTER = 19
    GRADIENT = 20
    GRADIENTCENTRE = 21
    POINT_TOP = 22
    CONTAINER = 8
    IME = 32
    IME_MAX = 35
    MAX = 35


class INDICATORNUMBERS(_Enum):
    CONTAINER = 8
    IME = 32
    IME_MAX = 35
    HISTORY_REVERTED_TO_ORIGIN_INSERTION = 36
    HISTORY_REVERTED_TO_ORIGIN_DELETION = 37
    HISTORY_SAVED_INSERTION = 38
    HISTORY_SAVED_DELETION = 39
    HISTORY_MODIFIED_INSERTION = 40
    HISTORY_MODIFIED_DELETION = 41
    HISTORY_REVERTED_TO_MODIFIED_INSERTION = 42
    HISTORY_REVERTED_TO_MODIFIED_DELETION = 43
    MAX = 43


class INDICVALUE(_Enum):
    BIT = 16777216
    MASK = 16777215


class INDICFLAG(_Enum):
    NONE = 0
    VALUEFORE = 1


class AUTOCOMPLETEOPTION(_Enum):
    NORMAL = 0
    FIXED_SIZE = 1
    SELECT_FIRST_ITEM = 2


class INDENTVIEW(_Enum):
    NONE = 0
    REAL = 1
    LOOKFORWARD = 2
    LOOKBOTH = 3


class PRINTOPTION(_Enum):
    NORMAL = 0
    INVERTLIGHT = 1
    BLACKONWHITE = 2
    COLOURONWHITE = 3
    COLOURONWHITEDEFAULTBG = 4
    SCREENCOLOURS = 5


class FINDOPTION(_Enum):
    NONE = 0
    WHOLEWORD = 2
    MATCHCASE = 4
    WORDSTART = 1048576
    REGEXP = 2097152
    POSIX = 4194304
    CXX11REGEX = 8388608


class CHANGEHISTORYOPTION(_Enum):
    DISABLED = 0
    ENABLED = 1
    MARKERS = 2
    INDICATORS = 4


class UNDOSELECTIONHISTORYOPTION(_Enum):
    DISABLED = 0
    ENABLED = 1
    SCROLL = 2


class FOLDLEVEL(_Enum):
    NONE = 0
    BASE = 1024
    WHITEFLAG = 4096
    HEADERFLAG = 8192
    NUMBERMASK = 4095


class FOLDDISPLAYTEXTSTYLE(_Enum):
    HIDDEN = 0
    STANDARD = 1
    BOXED = 2


class FOLDACTION(_Enum):
    CONTRACT = 0
    EXPAND = 1
    TOGGLE = 2
    CONTRACT_EVERY_LEVEL = 4


class AUTOMATICFOLD(_Enum):
    NONE = 0
    SHOW = 1
    CLICK = 2
    CHANGE = 4


class FOLDFLAG(_Enum):
    NONE = 0
    LINEBEFORE_EXPANDED = 2
    LINEBEFORE_CONTRACTED = 4
    LINEAFTER_EXPANDED = 8
    LINEAFTER_CONTRACTED = 16
    LEVELNUMBERS = 64
    LINESTATE = 128


class IDLESTYLING(_Enum):
    NONE = 0
    TOVISIBLE = 1
    AFTERVISIBLE = 2
    ALL = 3


class WRAP(_Enum):
    NONE = 0
    WORD = 1
    CHAR = 2
    WHITESPACE = 3


class WRAPVISUALFLAG(_Enum):
    NONE = 0
    END = 1
    START = 2
    MARGIN = 4


class WRAPVISUALLOCATION(_Enum):
    DEFAULT = 0
    END_BY_TEXT = 1
    START_BY_TEXT = 2


class WRAPINDENTMODE(_Enum):
    FIXED = 0
    SAME = 1
    INDENT = 2
    DEEPINDENT = 3


class LINECACHE(_Enum):
    NONE = 0
    CARET = 1
    PAGE = 2
    DOCUMENT = 3


class PHASESDRAW(_Enum):
    ONE = 0
    TWO = 1
    MULTIPLE = 2


class FONTQUALITY(_Enum):
    QUALITY_MASK = 15
    QUALITY_DEFAULT = 0
    QUALITY_NON_ANTIALIASED = 1
    QUALITY_ANTIALIASED = 2
    QUALITY_LCD_OPTIMIZED = 3


class MULTIPASTE(_Enum):
    ONCE = 0
    EACH = 1


class ACCESSIBILITY(_Enum):
    DISABLED = 0
    ENABLED = 1


class EDGEVISUALSTYLE(_Enum):
    NONE = 0
    LINE = 1
    BACKGROUND = 2
    MULTILINE = 3


class POPUP(_Enum):
    NEVER = 0
    ALL = 1
    TEXT = 2


class DOCUMENTOPTION(_Enum):
    DEFAULT = 0
    STYLES_NONE = 1
    TEXT_LARGE = 256


class STATUS(_Enum):
    OK = 0
    FAILURE = 1
    BADALLOC = 2
    OUTSIDE_DOCUMENT = 3
    WARN_START = 1000
    WARN_REGEX = 1001


class VISIBLEPOLICY(_Enum):
    SLOP = 1
    STRICT = 4


class CARETPOLICY(_Enum):
    SLOP = 1
    STRICT = 4
    JUMPS = 16
    EVEN = 8


class SELECTIONMODE(_Enum):
    STREAM = 0
    RECTANGLE = 1
    LINES = 2
    THIN = 3


class CASEINSENSITIVEBEHAVIOUR(_Enum):
    RESPECTCASE = 0
    IGNORECASE = 1


class MULTIAUTOCOMPLETE(_Enum):
    ONCE = 0
    EACH = 1


class ORDERING(_Enum):
    PRESORTED = 0
    PERFORMSORT = 1
    CUSTOM = 2


class CARETSTICKY(_Enum):
    OFF = 0
    ON = 1
    WHITESPACE = 2


class CARETSTYLE(_Enum):
    INVISIBLE = 0
    LINE = 1
    BLOCK = 2
    OVERSTRIKE_BAR = 0
    OVERSTRIKE_BLOCK = 16
    CURSES = 32
    INS_MASK = 15
    BLOCK_AFTER = 256


class MARGINOPTION(_Enum):
    NONE = 0
    SUBLINESELECT = 1


class ANNOTATIONVISIBLE(_Enum):
    HIDDEN = 0
    STANDARD = 1
    BOXED = 2
    INDENTED = 3


class UNDOFLAGS(_Enum):
    NONE = 0
    MAY_COALESCE = 1


class VIRTUALSPACE(_Enum):
    NONE = 0
    RECTANGULARSELECTION = 1
    USERACCESSIBLE = 2
    NOWRAPLINESTART = 4


class TECHNOLOGY(_Enum):
    DEFAULT = 0
    DIRECTWRITE = 1
    DIRECTWRITERETAIN = 2
    DIRECTWRITEDC = 3
    DIRECT_WRITE_1 = 4


class LINEENDTYPE(_Enum):
    DEFAULT = 0
    UNICODE = 1


class REPRESENTATIONAPPEARANCE(_Enum):
    _PLAIN = 0
    _BLOB = 1
    _COLOUR = 16


class EOLANNOTATIONVISIBLE(_Enum):
    HIDDEN = 0
    STANDARD = 1
    BOXED = 2
    STADIUM = 256
    FLAT_CIRCLE = 257
    ANGLE_CIRCLE = 258
    CIRCLE_FLAT = 272
    FLATS = 273
    ANGLE_FLAT = 274
    CIRCLE_ANGLE = 288
    FLAT_ANGLE = 289
    ANGLES = 290


class SUPPORTS(_Enum):
    LINE_DRAWS_FINAL = 0
    PIXEL_DIVISIONS = 1
    FRACTIONAL_STROKE_WIDTH = 2
    TRANSLUCENT_STROKE = 3
    PIXEL_MODIFICATION = 4
    THREAD_SAFE_MEASURE_WIDTHS = 5


class LINECHARACTERINDEXTYPE(_Enum):
    NONE = 0
    UTF32 = 1
    UTF16 = 2


class TYPEPROPERTY(_Enum):
    BOOLEAN = 0
    INTEGER = 1
    STRING = 2


class MODIFICATIONFLAGS(_Enum):
    NONE = 0
    INSERTTEXT = 1
    DELETETEXT = 2
    CHANGESTYLE = 4
    CHANGEFOLD = 8
    USER = 16
    UNDO = 32
    REDO = 64
    MULTISTEPUNDOREDO = 128
    LASTSTEPINUNDOREDO = 256
    CHANGEMARKER = 512
    BEFOREINSERT = 1024
    BEFOREDELETE = 2048
    MULTILINEUNDOREDO = 4096
    STARTACTION = 8192
    CHANGEINDICATOR = 16384
    CHANGELINESTATE = 32768
    CHANGEMARGIN = 65536
    CHANGEANNOTATION = 131072
    CONTAINER = 262144
    LEXERSTATE = 524288
    INSERTCHECK = 1048576
    CHANGETABSTOPS = 2097152
    CHANGEEOLANNOTATION = 4194304
    MODEVENTMASKALL = 8388607


class UPDATE(_Enum):
    NONE = 0
    CONTENT = 1
    SELECTION = 2
    V_SCROLL = 4
    H_SCROLL = 8
    TEXT = 16
    LINE_COUNT = 32


class FOCUSCHANGE(_Enum):
    CHANGE = 768
    SETFOCUS = 512
    KILLFOCUS = 256


class KEYS(_Enum):
    DOWN = 300
    UP = 301
    LEFT = 302
    RIGHT = 303
    HOME = 304
    END = 305
    PRIOR = 306
    NEXT = 307
    DELETE = 308
    INSERT = 309
    ESCAPE = 7
    BACK = 8
    TAB = 9
    RETURN = 13
    ADD = 310
    SUBTRACT = 311
    DIVIDE = 312
    WIN = 313
    RWIN = 314
    MENU = 315


class KEYMOD(_Enum):
    NORM = 0
    SHIFT = 1
    CTRL = 2
    ALT = 4
    SUPER = 8
    META = 16


class COMPLETIONMETHODS(_Enum):
    FILLUP = 1
    DOUBLECLICK = 2
    TAB = 3
    NEWLINE = 4
    COMMAND = 5
    SINGLE_CHOICE = 6


class CHARACTERSOURCE(_Enum):
    DIRECT_INPUT = 0
    TENTATIVE_INPUT = 1
    IME_RESULT = 2


class SCALETECHNIQUE(_Enum):
    DEFAULT = 0
    PIXEL_ALIGNED = 1


class BIDIRECTIONAL(_Enum):
    DISABLED = 0
    L2R = 1
    R2L = 2


class SCINTILLANOTIFICATION(_Enum):
    STYLENEEDED = 2000
    CHARADDED = 2001
    SAVEPOINTREACHED = 2002
    SAVEPOINTLEFT = 2003
    MODIFYATTEMPTRO = 2004
    KEY = 2005
    DOUBLECLICK = 2006
    UPDATEUI = 2007
    MODIFIED = 2008
    MACRORECORD = 2009
    MARGINCLICK = 2010
    NEEDSHOWN = 2011
    PAINTED = 2013
    USERLISTSELECTION = 2014
    URIDROPPED = 2015
    DWELLSTART = 2016
    DWELLEND = 2017
    ZOOM = 2018
    HOTSPOTCLICK = 2019
    HOTSPOTDOUBLECLICK = 2020
    CALLTIPCLICK = 2021
    AUTOCSELECTION = 2022
    INDICATORCLICK = 2023
    INDICATORRELEASE = 2024
    AUTOCCANCELLED = 2025
    AUTOCCHARDELETED = 2026
    HOTSPOTRELEASECLICK = 2027
    FOCUSIN = 2028
    FOCUSOUT = 2029
    AUTOCCOMPLETED = 2030
    MARGINRIGHTCLICK = 2031
    AUTOCSELECTIONCHANGE = 2032


class SCINTILLAMESSAGE(_Enum):
    SCI_ADDTEXT = 2001
    SCI_ADDSTYLEDTEXT = 2002
    SCI_INSERTTEXT = 2003
    SCI_CHANGEINSERTION = 2672
    SCI_CLEARALL = 2004
    SCI_DELETERANGE = 2645
    SCI_CLEARDOCUMENTSTYLE = 2005
    SCI_GETLENGTH = 2006
    SCI_GETCHARAT = 2007
    SCI_GETCURRENTPOS = 2008
    SCI_GETANCHOR = 2009
    SCI_GETSTYLEAT = 2010
    SCI_GETSTYLEINDEXAT = 2038
    SCI_REDO = 2011
    SCI_SETUNDOCOLLECTION = 2012
    SCI_SELECTALL = 2013
    SCI_SETSAVEPOINT = 2014
    SCI_GETSTYLEDTEXT = 2015
    SCI_GETSTYLEDTEXTFULL = 2778
    SCI_CANREDO = 2016
    SCI_MARKERLINEFROMHANDLE = 2017
    SCI_MARKERDELETEHANDLE = 2018
    SCI_MARKERHANDLEFROMLINE = 2732
    SCI_MARKERNUMBERFROMLINE = 2733
    SCI_GETUNDOCOLLECTION = 2019
    SCI_GETVIEWWS = 2020
    SCI_SETVIEWWS = 2021
    SCI_GETTABDRAWMODE = 2698
    SCI_SETTABDRAWMODE = 2699
    SCI_POSITIONFROMPOINT = 2022
    SCI_POSITIONFROMPOINTCLOSE = 2023
    SCI_GOTOLINE = 2024
    SCI_GOTOPOS = 2025
    SCI_SETANCHOR = 2026
    SCI_GETCURLINE = 2027
    SCI_GETENDSTYLED = 2028
    SCI_CONVERTEOLS = 2029
    SCI_GETEOLMODE = 2030
    SCI_SETEOLMODE = 2031
    SCI_STARTSTYLING = 2032
    SCI_SETSTYLING = 2033
    SCI_GETBUFFEREDDRAW = 2034
    SCI_SETBUFFEREDDRAW = 2035
    SCI_SETTABWIDTH = 2036
    SCI_GETTABWIDTH = 2121
    SCI_SETTABMINIMUMWIDTH = 2724
    SCI_GETTABMINIMUMWIDTH = 2725
    SCI_CLEARTABSTOPS = 2675
    SCI_ADDTABSTOP = 2676
    SCI_GETNEXTTABSTOP = 2677
    SCI_SETCODEPAGE = 2037
    SCI_SETFONTLOCALE = 2760
    SCI_GETFONTLOCALE = 2761
    SCI_GETIMEINTERACTION = 2678
    SCI_SETIMEINTERACTION = 2679
    SCI_MARKERDEFINE = 2040
    SCI_MARKERSETFORE = 2041
    SCI_MARKERSETBACK = 2042
    SCI_MARKERSETBACKSELECTED = 2292
    SCI_MARKERSETFORETRANSLUCENT = 2294
    SCI_MARKERSETBACKTRANSLUCENT = 2295
    SCI_MARKERSETBACKSELECTEDTRANSLUCENT = 2296
    SCI_MARKERSETSTROKEWIDTH = 2297
    SCI_MARKERENABLEHIGHLIGHT = 2293
    SCI_MARKERADD = 2043
    SCI_MARKERDELETE = 2044
    SCI_MARKERDELETEALL = 2045
    SCI_MARKERGET = 2046
    SCI_MARKERNEXT = 2047
    SCI_MARKERPREVIOUS = 2048
    SCI_MARKERDEFINEPIXMAP = 2049
    SCI_MARKERADDSET = 2466
    SCI_MARKERSETALPHA = 2476
    SCI_MARKERGETLAYER = 2734
    SCI_MARKERSETLAYER = 2735
    SCI_SETMARGINTYPEN = 2240
    SCI_GETMARGINTYPEN = 2241
    SCI_SETMARGINWIDTHN = 2242
    SCI_GETMARGINWIDTHN = 2243
    SCI_SETMARGINMASKN = 2244
    SCI_GETMARGINMASKN = 2245
    SCI_SETMARGINSENSITIVEN = 2246
    SCI_GETMARGINSENSITIVEN = 2247
    SCI_SETMARGINCURSORN = 2248
    SCI_GETMARGINCURSORN = 2249
    SCI_SETMARGINBACKN = 2250
    SCI_GETMARGINBACKN = 2251
    SCI_SETMARGINS = 2252
    SCI_GETMARGINS = 2253
    SCI_STYLECLEARALL = 2050
    SCI_STYLESETFORE = 2051
    SCI_STYLESETBACK = 2052
    SCI_STYLESETBOLD = 2053
    SCI_STYLESETITALIC = 2054
    SCI_STYLESETSIZE = 2055
    SCI_STYLESETFONT = 2056
    SCI_STYLESETEOLFILLED = 2057
    SCI_STYLERESETDEFAULT = 2058
    SCI_STYLESETUNDERLINE = 2059
    SCI_STYLEGETFORE = 2481
    SCI_STYLEGETBACK = 2482
    SCI_STYLEGETBOLD = 2483
    SCI_STYLEGETITALIC = 2484
    SCI_STYLEGETSIZE = 2485
    SCI_STYLEGETFONT = 2486
    SCI_STYLEGETEOLFILLED = 2487
    SCI_STYLEGETUNDERLINE = 2488
    SCI_STYLEGETCASE = 2489
    SCI_STYLEGETCHARACTERSET = 2490
    SCI_STYLEGETVISIBLE = 2491
    SCI_STYLEGETCHANGEABLE = 2492
    SCI_STYLEGETHOTSPOT = 2493
    SCI_STYLESETCASE = 2060
    SCI_STYLESETSIZEFRACTIONAL = 2061
    SCI_STYLEGETSIZEFRACTIONAL = 2062
    SCI_STYLESETWEIGHT = 2063
    SCI_STYLEGETWEIGHT = 2064
    SCI_STYLESETCHARACTERSET = 2066
    SCI_STYLESETHOTSPOT = 2409
    SCI_STYLESETCHECKMONOSPACED = 2254
    SCI_STYLEGETCHECKMONOSPACED = 2255
    SCI_STYLESETSTRETCH = 2258
    SCI_STYLEGETSTRETCH = 2259
    SCI_STYLESETINVISIBLEREPRESENTATION = 2256
    SCI_STYLEGETINVISIBLEREPRESENTATION = 2257
    SCI_SETELEMENTCOLOUR = 2753
    SCI_GETELEMENTCOLOUR = 2754
    SCI_RESETELEMENTCOLOUR = 2755
    SCI_GETELEMENTISSET = 2756
    SCI_GETELEMENTALLOWSTRANSLUCENT = 2757
    SCI_GETELEMENTBASECOLOUR = 2758
    SCI_SETSELFORE = 2067
    SCI_SETSELBACK = 2068
    SCI_GETSELALPHA = 2477
    SCI_SETSELALPHA = 2478
    SCI_GETSELEOLFILLED = 2479
    SCI_SETSELEOLFILLED = 2480
    SCI_GETSELECTIONLAYER = 2762
    SCI_SETSELECTIONLAYER = 2763
    SCI_GETCARETLINELAYER = 2764
    SCI_SETCARETLINELAYER = 2765
    SCI_GETCARETLINEHIGHLIGHTSUBLINE = 2773
    SCI_SETCARETLINEHIGHLIGHTSUBLINE = 2774
    SCI_SETCARETFORE = 2069
    SCI_ASSIGNCMDKEY = 2070
    SCI_CLEARCMDKEY = 2071
    SCI_CLEARALLCMDKEYS = 2072
    SCI_SETSTYLINGEX = 2073
    SCI_STYLESETVISIBLE = 2074
    SCI_GETCARETPERIOD = 2075
    SCI_SETCARETPERIOD = 2076
    SCI_SETWORDCHARS = 2077
    SCI_GETWORDCHARS = 2646
    SCI_SETCHARACTERCATEGORYOPTIMIZATION = 2720
    SCI_GETCHARACTERCATEGORYOPTIMIZATION = 2721
    SCI_BEGINUNDOACTION = 2078
    SCI_ENDUNDOACTION = 2079
    SCI_GETUNDOSEQUENCE = 2799
    SCI_GETUNDOACTIONS = 2790
    SCI_SETUNDOSAVEPOINT = 2791
    SCI_GETUNDOSAVEPOINT = 2792
    SCI_SETUNDODETACH = 2793
    SCI_GETUNDODETACH = 2794
    SCI_SETUNDOTENTATIVE = 2795
    SCI_GETUNDOTENTATIVE = 2796
    SCI_SETUNDOCURRENT = 2797
    SCI_GETUNDOCURRENT = 2798
    SCI_PUSHUNDOACTIONTYPE = 2800
    SCI_CHANGELASTUNDOACTIONTEXT = 2801
    SCI_GETUNDOACTIONTYPE = 2802
    SCI_GETUNDOACTIONPOSITION = 2803
    SCI_GETUNDOACTIONTEXT = 2804
    SCI_INDICSETSTYLE = 2080
    SCI_INDICGETSTYLE = 2081
    SCI_INDICSETFORE = 2082
    SCI_INDICGETFORE = 2083
    SCI_INDICSETUNDER = 2510
    SCI_INDICGETUNDER = 2511
    SCI_INDICSETHOVERSTYLE = 2680
    SCI_INDICGETHOVERSTYLE = 2681
    SCI_INDICSETHOVERFORE = 2682
    SCI_INDICGETHOVERFORE = 2683
    SCI_INDICSETFLAGS = 2684
    SCI_INDICGETFLAGS = 2685
    SCI_INDICSETSTROKEWIDTH = 2751
    SCI_INDICGETSTROKEWIDTH = 2752
    SCI_SETWHITESPACEFORE = 2084
    SCI_SETWHITESPACEBACK = 2085
    SCI_SETWHITESPACESIZE = 2086
    SCI_GETWHITESPACESIZE = 2087
    SCI_SETLINESTATE = 2092
    SCI_GETLINESTATE = 2093
    SCI_GETMAXLINESTATE = 2094
    SCI_GETCARETLINEVISIBLE = 2095
    SCI_SETCARETLINEVISIBLE = 2096
    SCI_GETCARETLINEBACK = 2097
    SCI_SETCARETLINEBACK = 2098
    SCI_GETCARETLINEFRAME = 2704
    SCI_SETCARETLINEFRAME = 2705
    SCI_STYLESETCHANGEABLE = 2099
    SCI_AUTOCSHOW = 2100
    SCI_AUTOCCANCEL = 2101
    SCI_AUTOCACTIVE = 2102
    SCI_AUTOCPOSSTART = 2103
    SCI_AUTOCCOMPLETE = 2104
    SCI_AUTOCSTOPS = 2105
    SCI_AUTOCSETSEPARATOR = 2106
    SCI_AUTOCGETSEPARATOR = 2107
    SCI_AUTOCSELECT = 2108
    SCI_AUTOCSETCANCELATSTART = 2110
    SCI_AUTOCGETCANCELATSTART = 2111
    SCI_AUTOCSETFILLUPS = 2112
    SCI_AUTOCSETCHOOSESINGLE = 2113
    SCI_AUTOCGETCHOOSESINGLE = 2114
    SCI_AUTOCSETIGNORECASE = 2115
    SCI_AUTOCGETIGNORECASE = 2116
    SCI_USERLISTSHOW = 2117
    SCI_AUTOCSETAUTOHIDE = 2118
    SCI_AUTOCGETAUTOHIDE = 2119
    SCI_AUTOCSETOPTIONS = 2638
    SCI_AUTOCGETOPTIONS = 2639
    SCI_AUTOCSETDROPRESTOFWORD = 2270
    SCI_AUTOCGETDROPRESTOFWORD = 2271
    SCI_REGISTERIMAGE = 2405
    SCI_CLEARREGISTEREDIMAGES = 2408
    SCI_AUTOCGETTYPESEPARATOR = 2285
    SCI_AUTOCSETTYPESEPARATOR = 2286
    SCI_AUTOCSETMAXWIDTH = 2208
    SCI_AUTOCGETMAXWIDTH = 2209
    SCI_AUTOCSETMAXHEIGHT = 2210
    SCI_AUTOCGETMAXHEIGHT = 2211
    SCI_AUTOCSETSTYLE = 2109
    SCI_AUTOCGETSTYLE = 2120
    SCI_AUTOCSETIMAGESCALE = 2815
    SCI_AUTOCGETIMAGESCALE = 2816
    SCI_SETINDENT = 2122
    SCI_GETINDENT = 2123
    SCI_SETUSETABS = 2124
    SCI_GETUSETABS = 2125
    SCI_SETLINEINDENTATION = 2126
    SCI_GETLINEINDENTATION = 2127
    SCI_GETLINEINDENTPOSITION = 2128
    SCI_GETCOLUMN = 2129
    SCI_COUNTCHARACTERS = 2633
    SCI_COUNTCODEUNITS = 2715
    SCI_SETHSCROLLBAR = 2130
    SCI_GETHSCROLLBAR = 2131
    SCI_SETINDENTATIONGUIDES = 2132
    SCI_GETINDENTATIONGUIDES = 2133
    SCI_SETHIGHLIGHTGUIDE = 2134
    SCI_GETHIGHLIGHTGUIDE = 2135
    SCI_GETLINEENDPOSITION = 2136
    SCI_GETCODEPAGE = 2137
    SCI_GETCARETFORE = 2138
    SCI_GETREADONLY = 2140
    SCI_SETCURRENTPOS = 2141
    SCI_SETSELECTIONSTART = 2142
    SCI_GETSELECTIONSTART = 2143
    SCI_SETSELECTIONEND = 2144
    SCI_GETSELECTIONEND = 2145
    SCI_SETEMPTYSELECTION = 2556
    SCI_SETPRINTMAGNIFICATION = 2146
    SCI_GETPRINTMAGNIFICATION = 2147
    SCI_SETPRINTCOLOURMODE = 2148
    SCI_GETPRINTCOLOURMODE = 2149
    SCI_FINDTEXT = 2150
    SCI_FINDTEXTFULL = 2196
    SCI_FORMATRANGE = 2151
    SCI_FORMATRANGEFULL = 2777
    SCI_SETCHANGEHISTORY = 2780
    SCI_GETCHANGEHISTORY = 2781
    SCI_SETUNDOSELECTIONHISTORY = 2782
    SCI_GETUNDOSELECTIONHISTORY = 2783
    SCI_SETSELECTIONSERIALIZED = 2784
    SCI_GETSELECTIONSERIALIZED = 2785
    SCI_GETFIRSTVISIBLELINE = 2152
    SCI_GETLINE = 2153
    SCI_GETLINECOUNT = 2154
    SCI_ALLOCATELINES = 2089
    SCI_SETMARGINLEFT = 2155
    SCI_GETMARGINLEFT = 2156
    SCI_SETMARGINRIGHT = 2157
    SCI_GETMARGINRIGHT = 2158
    SCI_GETMODIFY = 2159
    SCI_SETSEL = 2160
    SCI_GETSELTEXT = 2161
    SCI_GETTEXTRANGE = 2162
    SCI_GETTEXTRANGEFULL = 2039
    SCI_HIDESELECTION = 2163
    SCI_GETSELECTIONHIDDEN = 2088
    SCI_POINTXFROMPOSITION = 2164
    SCI_POINTYFROMPOSITION = 2165
    SCI_LINEFROMPOSITION = 2166
    SCI_POSITIONFROMLINE = 2167
    SCI_LINESCROLL = 2168
    SCI_SCROLLVERTICAL = 2817
    SCI_SCROLLCARET = 2169
    SCI_SCROLLRANGE = 2569
    SCI_REPLACESEL = 2170
    SCI_SETREADONLY = 2171
    SCI_NULL = 2172
    SCI_CANPASTE = 2173
    SCI_CANUNDO = 2174
    SCI_EMPTYUNDOBUFFER = 2175
    SCI_UNDO = 2176
    SCI_CUT = 2177
    SCI_COPY = 2178
    SCI_PASTE = 2179
    SCI_CLEAR = 2180
    SCI_SETTEXT = 2181
    SCI_GETTEXT = 2182
    SCI_GETTEXTLENGTH = 2183
    SCI_GETDIRECTFUNCTION = 2184
    SCI_GETDIRECTSTATUSFUNCTION = 2772
    SCI_GETDIRECTPOINTER = 2185
    SCI_SETOVERTYPE = 2186
    SCI_GETOVERTYPE = 2187
    SCI_SETCARETWIDTH = 2188
    SCI_GETCARETWIDTH = 2189
    SCI_SETTARGETSTART = 2190
    SCI_GETTARGETSTART = 2191
    SCI_SETTARGETSTARTVIRTUALSPACE = 2728
    SCI_GETTARGETSTARTVIRTUALSPACE = 2729
    SCI_SETTARGETEND = 2192
    SCI_GETTARGETEND = 2193
    SCI_SETTARGETENDVIRTUALSPACE = 2730
    SCI_GETTARGETENDVIRTUALSPACE = 2731
    SCI_SETTARGETRANGE = 2686
    SCI_GETTARGETTEXT = 2687
    SCI_TARGETFROMSELECTION = 2287
    SCI_TARGETWHOLEDOCUMENT = 2690
    SCI_REPLACETARGET = 2194
    SCI_REPLACETARGETRE = 2195
    SCI_REPLACETARGETMINIMAL = 2779
    SCI_SEARCHINTARGET = 2197
    SCI_SETSEARCHFLAGS = 2198
    SCI_GETSEARCHFLAGS = 2199
    SCI_CALLTIPSHOW = 2200
    SCI_CALLTIPCANCEL = 2201
    SCI_CALLTIPACTIVE = 2202
    SCI_CALLTIPPOSSTART = 2203
    SCI_CALLTIPSETPOSSTART = 2214
    SCI_CALLTIPSETHLT = 2204
    SCI_CALLTIPSETBACK = 2205
    SCI_CALLTIPSETFORE = 2206
    SCI_CALLTIPSETFOREHLT = 2207
    SCI_CALLTIPUSESTYLE = 2212
    SCI_CALLTIPSETPOSITION = 2213
    SCI_VISIBLEFROMDOCLINE = 2220
    SCI_DOCLINEFROMVISIBLE = 2221
    SCI_WRAPCOUNT = 2235
    SCI_SETFOLDLEVEL = 2222
    SCI_GETFOLDLEVEL = 2223
    SCI_GETLASTCHILD = 2224
    SCI_GETFOLDPARENT = 2225
    SCI_SHOWLINES = 2226
    SCI_HIDELINES = 2227
    SCI_GETLINEVISIBLE = 2228
    SCI_GETALLLINESVISIBLE = 2236
    SCI_SETFOLDEXPANDED = 2229
    SCI_GETFOLDEXPANDED = 2230
    SCI_TOGGLEFOLD = 2231
    SCI_TOGGLEFOLDSHOWTEXT = 2700
    SCI_FOLDDISPLAYTEXTSETSTYLE = 2701
    SCI_FOLDDISPLAYTEXTGETSTYLE = 2707
    SCI_SETDEFAULTFOLDDISPLAYTEXT = 2722
    SCI_GETDEFAULTFOLDDISPLAYTEXT = 2723
    SCI_FOLDLINE = 2237
    SCI_FOLDCHILDREN = 2238
    SCI_EXPANDCHILDREN = 2239
    SCI_FOLDALL = 2662
    SCI_ENSUREVISIBLE = 2232
    SCI_SETAUTOMATICFOLD = 2663
    SCI_GETAUTOMATICFOLD = 2664
    SCI_SETFOLDFLAGS = 2233
    SCI_ENSUREVISIBLEENFORCEPOLICY = 2234
    SCI_SETTABINDENTS = 2260
    SCI_GETTABINDENTS = 2261
    SCI_SETBACKSPACEUNINDENTS = 2262
    SCI_GETBACKSPACEUNINDENTS = 2263
    SCI_SETMOUSEDWELLTIME = 2264
    SCI_GETMOUSEDWELLTIME = 2265
    SCI_WORDSTARTPOSITION = 2266
    SCI_WORDENDPOSITION = 2267
    SCI_ISRANGEWORD = 2691
    SCI_SETIDLESTYLING = 2692
    SCI_GETIDLESTYLING = 2693
    SCI_SETWRAPMODE = 2268
    SCI_GETWRAPMODE = 2269
    SCI_SETWRAPVISUALFLAGS = 2460
    SCI_GETWRAPVISUALFLAGS = 2461
    SCI_SETWRAPVISUALFLAGSLOCATION = 2462
    SCI_GETWRAPVISUALFLAGSLOCATION = 2463
    SCI_SETWRAPSTARTINDENT = 2464
    SCI_GETWRAPSTARTINDENT = 2465
    SCI_SETWRAPINDENTMODE = 2472
    SCI_GETWRAPINDENTMODE = 2473
    SCI_SETLAYOUTCACHE = 2272
    SCI_GETLAYOUTCACHE = 2273
    SCI_SETSCROLLWIDTH = 2274
    SCI_GETSCROLLWIDTH = 2275
    SCI_SETSCROLLWIDTHTRACKING = 2516
    SCI_GETSCROLLWIDTHTRACKING = 2517
    SCI_TEXTWIDTH = 2276
    SCI_SETENDATLASTLINE = 2277
    SCI_GETENDATLASTLINE = 2278
    SCI_TEXTHEIGHT = 2279
    SCI_SETVSCROLLBAR = 2280
    SCI_GETVSCROLLBAR = 2281
    SCI_APPENDTEXT = 2282
    SCI_GETPHASESDRAW = 2673
    SCI_SETPHASESDRAW = 2674
    SCI_SETFONTQUALITY = 2611
    SCI_GETFONTQUALITY = 2612
    SCI_SETFIRSTVISIBLELINE = 2613
    SCI_SETMULTIPASTE = 2614
    SCI_GETMULTIPASTE = 2615
    SCI_GETTAG = 2616
    SCI_LINESJOIN = 2288
    SCI_LINESSPLIT = 2289
    SCI_SETFOLDMARGINCOLOUR = 2290
    SCI_SETFOLDMARGINHICOLOUR = 2291
    SCI_SETACCESSIBILITY = 2702
    SCI_GETACCESSIBILITY = 2703
    SCI_LINEDOWN = 2300
    SCI_LINEDOWNEXTEND = 2301
    SCI_LINEUP = 2302
    SCI_LINEUPEXTEND = 2303
    SCI_CHARLEFT = 2304
    SCI_CHARLEFTEXTEND = 2305
    SCI_CHARRIGHT = 2306
    SCI_CHARRIGHTEXTEND = 2307
    SCI_WORDLEFT = 2308
    SCI_WORDLEFTEXTEND = 2309
    SCI_WORDRIGHT = 2310
    SCI_WORDRIGHTEXTEND = 2311
    SCI_HOME = 2312
    SCI_HOMEEXTEND = 2313
    SCI_LINEEND = 2314
    SCI_LINEENDEXTEND = 2315
    SCI_DOCUMENTSTART = 2316
    SCI_DOCUMENTSTARTEXTEND = 2317
    SCI_DOCUMENTEND = 2318
    SCI_DOCUMENTENDEXTEND = 2319
    SCI_PAGEUP = 2320
    SCI_PAGEUPEXTEND = 2321
    SCI_PAGEDOWN = 2322
    SCI_PAGEDOWNEXTEND = 2323
    SCI_EDITTOGGLEOVERTYPE = 2324
    SCI_CANCEL = 2325
    SCI_DELETEBACK = 2326
    SCI_TAB = 2327
    SCI_LINEINDENT = 2813
    SCI_BACKTAB = 2328
    SCI_LINEDEDENT = 2814
    SCI_NEWLINE = 2329
    SCI_FORMFEED = 2330
    SCI_VCHOME = 2331
    SCI_VCHOMEEXTEND = 2332
    SCI_ZOOMIN = 2333
    SCI_ZOOMOUT = 2334
    SCI_DELWORDLEFT = 2335
    SCI_DELWORDRIGHT = 2336
    SCI_DELWORDRIGHTEND = 2518
    SCI_LINECUT = 2337
    SCI_LINEDELETE = 2338
    SCI_LINETRANSPOSE = 2339
    SCI_LINEREVERSE = 2354
    SCI_LINEDUPLICATE = 2404
    SCI_LOWERCASE = 2340
    SCI_UPPERCASE = 2341
    SCI_LINESCROLLDOWN = 2342
    SCI_LINESCROLLUP = 2343
    SCI_DELETEBACKNOTLINE = 2344
    SCI_HOMEDISPLAY = 2345
    SCI_HOMEDISPLAYEXTEND = 2346
    SCI_LINEENDDISPLAY = 2347
    SCI_LINEENDDISPLAYEXTEND = 2348
    SCI_HOMEWRAP = 2349
    SCI_HOMEWRAPEXTEND = 2450
    SCI_LINEENDWRAP = 2451
    SCI_LINEENDWRAPEXTEND = 2452
    SCI_VCHOMEWRAP = 2453
    SCI_VCHOMEWRAPEXTEND = 2454
    SCI_LINECOPY = 2455
    SCI_MOVECARETINSIDEVIEW = 2401
    SCI_LINELENGTH = 2350
    SCI_BRACEHIGHLIGHT = 2351
    SCI_BRACEHIGHLIGHTINDICATOR = 2498
    SCI_BRACEBADLIGHT = 2352
    SCI_BRACEBADLIGHTINDICATOR = 2499
    SCI_BRACEMATCH = 2353
    SCI_BRACEMATCHNEXT = 2369
    SCI_GETVIEWEOL = 2355
    SCI_SETVIEWEOL = 2356
    SCI_GETDOCPOINTER = 2357
    SCI_SETDOCPOINTER = 2358
    SCI_SETMODEVENTMASK = 2359
    SCI_GETEDGECOLUMN = 2360
    SCI_SETEDGECOLUMN = 2361
    SCI_GETEDGEMODE = 2362
    SCI_SETEDGEMODE = 2363
    SCI_GETEDGECOLOUR = 2364
    SCI_SETEDGECOLOUR = 2365
    SCI_MULTIEDGEADDLINE = 2694
    SCI_MULTIEDGECLEARALL = 2695
    SCI_GETMULTIEDGECOLUMN = 2749
    SCI_SEARCHANCHOR = 2366
    SCI_SEARCHNEXT = 2367
    SCI_SEARCHPREV = 2368
    SCI_LINESONSCREEN = 2370
    SCI_USEPOPUP = 2371
    SCI_SELECTIONISRECTANGLE = 2372
    SCI_SETZOOM = 2373
    SCI_GETZOOM = 2374
    SCI_CREATEDOCUMENT = 2375
    SCI_ADDREFDOCUMENT = 2376
    SCI_RELEASEDOCUMENT = 2377
    SCI_GETDOCUMENTOPTIONS = 2379
    SCI_GETMODEVENTMASK = 2378
    SCI_SETCOMMANDEVENTS = 2717
    SCI_GETCOMMANDEVENTS = 2718
    SCI_SETFOCUS = 2380
    SCI_GETFOCUS = 2381
    SCI_SETSTATUS = 2382
    SCI_GETSTATUS = 2383
    SCI_SETMOUSEDOWNCAPTURES = 2384
    SCI_GETMOUSEDOWNCAPTURES = 2385
    SCI_SETMOUSEWHEELCAPTURES = 2696
    SCI_GETMOUSEWHEELCAPTURES = 2697
    SCI_SETCURSOR = 2386
    SCI_GETCURSOR = 2387
    SCI_SETCONTROLCHARSYMBOL = 2388
    SCI_GETCONTROLCHARSYMBOL = 2389
    SCI_WORDPARTLEFT = 2390
    SCI_WORDPARTLEFTEXTEND = 2391
    SCI_WORDPARTRIGHT = 2392
    SCI_WORDPARTRIGHTEXTEND = 2393
    SCI_SETVISIBLEPOLICY = 2394
    SCI_DELLINELEFT = 2395
    SCI_DELLINERIGHT = 2396
    SCI_SETXOFFSET = 2397
    SCI_GETXOFFSET = 2398
    SCI_CHOOSECARETX = 2399
    SCI_GRABFOCUS = 2400
    SCI_SETXCARETPOLICY = 2402
    SCI_SETYCARETPOLICY = 2403
    SCI_SETPRINTWRAPMODE = 2406
    SCI_GETPRINTWRAPMODE = 2407
    SCI_SETHOTSPOTACTIVEFORE = 2410
    SCI_GETHOTSPOTACTIVEFORE = 2494
    SCI_SETHOTSPOTACTIVEBACK = 2411
    SCI_GETHOTSPOTACTIVEBACK = 2495
    SCI_SETHOTSPOTACTIVEUNDERLINE = 2412
    SCI_GETHOTSPOTACTIVEUNDERLINE = 2496
    SCI_SETHOTSPOTSINGLELINE = 2421
    SCI_GETHOTSPOTSINGLELINE = 2497
    SCI_PARADOWN = 2413
    SCI_PARADOWNEXTEND = 2414
    SCI_PARAUP = 2415
    SCI_PARAUPEXTEND = 2416
    SCI_POSITIONBEFORE = 2417
    SCI_POSITIONAFTER = 2418
    SCI_POSITIONRELATIVE = 2670
    SCI_POSITIONRELATIVECODEUNITS = 2716
    SCI_COPYRANGE = 2419
    SCI_COPYTEXT = 2420
    SCI_SETSELECTIONMODE = 2422
    SCI_CHANGESELECTIONMODE = 2659
    SCI_GETSELECTIONMODE = 2423
    SCI_SETMOVEEXTENDSSELECTION = 2719
    SCI_GETMOVEEXTENDSSELECTION = 2706
    SCI_GETLINESELSTARTPOSITION = 2424
    SCI_GETLINESELENDPOSITION = 2425
    SCI_LINEDOWNRECTEXTEND = 2426
    SCI_LINEUPRECTEXTEND = 2427
    SCI_CHARLEFTRECTEXTEND = 2428
    SCI_CHARRIGHTRECTEXTEND = 2429
    SCI_HOMERECTEXTEND = 2430
    SCI_VCHOMERECTEXTEND = 2431
    SCI_LINEENDRECTEXTEND = 2432
    SCI_PAGEUPRECTEXTEND = 2433
    SCI_PAGEDOWNRECTEXTEND = 2434
    SCI_STUTTEREDPAGEUP = 2435
    SCI_STUTTEREDPAGEUPEXTEND = 2436
    SCI_STUTTEREDPAGEDOWN = 2437
    SCI_STUTTEREDPAGEDOWNEXTEND = 2438
    SCI_WORDLEFTEND = 2439
    SCI_WORDLEFTENDEXTEND = 2440
    SCI_WORDRIGHTEND = 2441
    SCI_WORDRIGHTENDEXTEND = 2442
    SCI_SETWHITESPACECHARS = 2443
    SCI_GETWHITESPACECHARS = 2647
    SCI_SETPUNCTUATIONCHARS = 2648
    SCI_GETPUNCTUATIONCHARS = 2649
    SCI_SETCHARSDEFAULT = 2444
    SCI_AUTOCGETCURRENT = 2445
    SCI_AUTOCGETCURRENTTEXT = 2610
    SCI_AUTOCSETCASEINSENSITIVEBEHAVIOUR = 2634
    SCI_AUTOCGETCASEINSENSITIVEBEHAVIOUR = 2635
    SCI_AUTOCSETMULTI = 2636
    SCI_AUTOCGETMULTI = 2637
    SCI_AUTOCSETORDER = 2660
    SCI_AUTOCGETORDER = 2661
    SCI_ALLOCATE = 2446
    SCI_TARGETASUTF8 = 2447
    SCI_SETLENGTHFORENCODE = 2448
    SCI_ENCODEDFROMUTF8 = 2449
    SCI_FINDCOLUMN = 2456
    SCI_GETCARETSTICKY = 2457
    SCI_SETCARETSTICKY = 2458
    SCI_TOGGLECARETSTICKY = 2459
    SCI_SETPASTECONVERTENDINGS = 2467
    SCI_GETPASTECONVERTENDINGS = 2468
    SCI_REPLACERECTANGULAR = 2771
    SCI_SELECTIONDUPLICATE = 2469
    SCI_SETCARETLINEBACKALPHA = 2470
    SCI_GETCARETLINEBACKALPHA = 2471
    SCI_SETCARETSTYLE = 2512
    SCI_GETCARETSTYLE = 2513
    SCI_SETINDICATORCURRENT = 2500
    SCI_GETINDICATORCURRENT = 2501
    SCI_SETINDICATORVALUE = 2502
    SCI_GETINDICATORVALUE = 2503
    SCI_INDICATORFILLRANGE = 2504
    SCI_INDICATORCLEARRANGE = 2505
    SCI_INDICATORALLONFOR = 2506
    SCI_INDICATORVALUEAT = 2507
    SCI_INDICATORSTART = 2508
    SCI_INDICATOREND = 2509
    SCI_SETPOSITIONCACHE = 2514
    SCI_GETPOSITIONCACHE = 2515
    SCI_SETLAYOUTTHREADS = 2775
    SCI_GETLAYOUTTHREADS = 2776
    SCI_COPYALLOWLINE = 2519
    SCI_CUTALLOWLINE = 2810
    SCI_SETCOPYSEPARATOR = 2811
    SCI_GETCOPYSEPARATOR = 2812
    SCI_GETCHARACTERPOINTER = 2520
    SCI_GETRANGEPOINTER = 2643
    SCI_GETGAPPOSITION = 2644
    SCI_INDICSETALPHA = 2523
    SCI_INDICGETALPHA = 2524
    SCI_INDICSETOUTLINEALPHA = 2558
    SCI_INDICGETOUTLINEALPHA = 2559
    SCI_SETEXTRAASCENT = 2525
    SCI_GETEXTRAASCENT = 2526
    SCI_SETEXTRADESCENT = 2527
    SCI_GETEXTRADESCENT = 2528
    SCI_MARKERSYMBOLDEFINED = 2529
    SCI_MARGINSETTEXT = 2530
    SCI_MARGINGETTEXT = 2531
    SCI_MARGINSETSTYLE = 2532
    SCI_MARGINGETSTYLE = 2533
    SCI_MARGINSETSTYLES = 2534
    SCI_MARGINGETSTYLES = 2535
    SCI_MARGINTEXTCLEARALL = 2536
    SCI_MARGINSETSTYLEOFFSET = 2537
    SCI_MARGINGETSTYLEOFFSET = 2538
    SCI_SETMARGINOPTIONS = 2539
    SCI_GETMARGINOPTIONS = 2557
    SCI_ANNOTATIONSETTEXT = 2540
    SCI_ANNOTATIONGETTEXT = 2541
    SCI_ANNOTATIONSETSTYLE = 2542
    SCI_ANNOTATIONGETSTYLE = 2543
    SCI_ANNOTATIONSETSTYLES = 2544
    SCI_ANNOTATIONGETSTYLES = 2545
    SCI_ANNOTATIONGETLINES = 2546
    SCI_ANNOTATIONCLEARALL = 2547
    SCI_ANNOTATIONSETVISIBLE = 2548
    SCI_ANNOTATIONGETVISIBLE = 2549
    SCI_ANNOTATIONSETSTYLEOFFSET = 2550
    SCI_ANNOTATIONGETSTYLEOFFSET = 2551
    SCI_RELEASEALLEXTENDEDSTYLES = 2552
    SCI_ALLOCATEEXTENDEDSTYLES = 2553
    SCI_ADDUNDOACTION = 2560
    SCI_CHARPOSITIONFROMPOINT = 2561
    SCI_CHARPOSITIONFROMPOINTCLOSE = 2562
    SCI_SETMOUSESELECTIONRECTANGULARSWITCH = 2668
    SCI_GETMOUSESELECTIONRECTANGULARSWITCH = 2669
    SCI_SETMULTIPLESELECTION = 2563
    SCI_GETMULTIPLESELECTION = 2564
    SCI_SETADDITIONALSELECTIONTYPING = 2565
    SCI_GETADDITIONALSELECTIONTYPING = 2566
    SCI_SETADDITIONALCARETSBLINK = 2567
    SCI_GETADDITIONALCARETSBLINK = 2568
    SCI_SETADDITIONALCARETSVISIBLE = 2608
    SCI_GETADDITIONALCARETSVISIBLE = 2609
    SCI_GETSELECTIONS = 2570
    SCI_GETSELECTIONEMPTY = 2650
    SCI_CLEARSELECTIONS = 2571
    SCI_SETSELECTION = 2572
    SCI_ADDSELECTION = 2573
    SCI_SELECTIONFROMPOINT = 2474
    SCI_DROPSELECTIONN = 2671
    SCI_SETMAINSELECTION = 2574
    SCI_GETMAINSELECTION = 2575
    SCI_SETSELECTIONNCARET = 2576
    SCI_GETSELECTIONNCARET = 2577
    SCI_SETSELECTIONNANCHOR = 2578
    SCI_GETSELECTIONNANCHOR = 2579
    SCI_SETSELECTIONNCARETVIRTUALSPACE = 2580
    SCI_GETSELECTIONNCARETVIRTUALSPACE = 2581
    SCI_SETSELECTIONNANCHORVIRTUALSPACE = 2582
    SCI_GETSELECTIONNANCHORVIRTUALSPACE = 2583
    SCI_SETSELECTIONNSTART = 2584
    SCI_GETSELECTIONNSTART = 2585
    SCI_GETSELECTIONNSTARTVIRTUALSPACE = 2726
    SCI_SETSELECTIONNEND = 2586
    SCI_GETSELECTIONNENDVIRTUALSPACE = 2727
    SCI_GETSELECTIONNEND = 2587
    SCI_SETRECTANGULARSELECTIONCARET = 2588
    SCI_GETRECTANGULARSELECTIONCARET = 2589
    SCI_SETRECTANGULARSELECTIONANCHOR = 2590
    SCI_GETRECTANGULARSELECTIONANCHOR = 2591
    SCI_SETRECTANGULARSELECTIONCARETVIRTUALSPACE = 2592
    SCI_GETRECTANGULARSELECTIONCARETVIRTUALSPACE = 2593
    SCI_SETRECTANGULARSELECTIONANCHORVIRTUALSPACE = 2594
    SCI_GETRECTANGULARSELECTIONANCHORVIRTUALSPACE = 2595
    SCI_SETVIRTUALSPACEOPTIONS = 2596
    SCI_GETVIRTUALSPACEOPTIONS = 2597
    SCI_SETRECTANGULARSELECTIONMODIFIER = 2598
    SCI_GETRECTANGULARSELECTIONMODIFIER = 2599
    SCI_SETADDITIONALSELFORE = 2600
    SCI_SETADDITIONALSELBACK = 2601
    SCI_SETADDITIONALSELALPHA = 2602
    SCI_GETADDITIONALSELALPHA = 2603
    SCI_SETADDITIONALCARETFORE = 2604
    SCI_GETADDITIONALCARETFORE = 2605
    SCI_ROTATESELECTION = 2606
    SCI_SWAPMAINANCHORCARET = 2607
    SCI_MULTIPLESELECTADDNEXT = 2688
    SCI_MULTIPLESELECTADDEACH = 2689
    SCI_CHANGELEXERSTATE = 2617
    SCI_CONTRACTEDFOLDNEXT = 2618
    SCI_VERTICALCENTRECARET = 2619
    SCI_MOVESELECTEDLINESUP = 2620
    SCI_MOVESELECTEDLINESDOWN = 2621
    SCI_SETIDENTIFIER = 2622
    SCI_GETIDENTIFIER = 2623
    SCI_RGBAIMAGESETWIDTH = 2624
    SCI_RGBAIMAGESETHEIGHT = 2625
    SCI_RGBAIMAGESETSCALE = 2651
    SCI_MARKERDEFINERGBAIMAGE = 2626
    SCI_REGISTERRGBAIMAGE = 2627
    SCI_SCROLLTOSTART = 2628
    SCI_SCROLLTOEND = 2629
    SCI_SETTECHNOLOGY = 2630
    SCI_GETTECHNOLOGY = 2631
    SCI_CREATELOADER = 2632
    SCI_FINDINDICATORSHOW = 2640
    SCI_FINDINDICATORFLASH = 2641
    SCI_FINDINDICATORHIDE = 2642
    SCI_VCHOMEDISPLAY = 2652
    SCI_VCHOMEDISPLAYEXTEND = 2653
    SCI_GETCARETLINEVISIBLEALWAYS = 2654
    SCI_SETCARETLINEVISIBLEALWAYS = 2655
    SCI_SETLINEENDTYPESALLOWED = 2656
    SCI_GETLINEENDTYPESALLOWED = 2657
    SCI_GETLINEENDTYPESACTIVE = 2658
    SCI_SETREPRESENTATION = 2665
    SCI_GETREPRESENTATION = 2666
    SCI_CLEARREPRESENTATION = 2667
    SCI_CLEARALLREPRESENTATIONS = 2770
    SCI_SETREPRESENTATIONAPPEARANCE = 2766
    SCI_GETREPRESENTATIONAPPEARANCE = 2767
    SCI_SETREPRESENTATIONCOLOUR = 2768
    SCI_GETREPRESENTATIONCOLOUR = 2769
    SCI_EOLANNOTATIONSETTEXT = 2740
    SCI_EOLANNOTATIONGETTEXT = 2741
    SCI_EOLANNOTATIONSETSTYLE = 2742
    SCI_EOLANNOTATIONGETSTYLE = 2743
    SCI_EOLANNOTATIONCLEARALL = 2744
    SCI_EOLANNOTATIONSETVISIBLE = 2745
    SCI_EOLANNOTATIONGETVISIBLE = 2746
    SCI_EOLANNOTATIONSETSTYLEOFFSET = 2747
    SCI_EOLANNOTATIONGETSTYLEOFFSET = 2748
    SCI_SUPPORTSFEATURE = 2750
    SCI_GETLINECHARACTERINDEX = 2710
    SCI_ALLOCATELINECHARACTERINDEX = 2711
    SCI_RELEASELINECHARACTERINDEX = 2712
    SCI_LINEFROMINDEXPOSITION = 2713
    SCI_INDEXPOSITIONFROMLINE = 2714
    SCI_GETDRAGDROPENABLED = 2818
    SCI_SETDRAGDROPENABLED = 2819
    SCI_STARTRECORD = 3001
    SCI_STOPRECORD = 3002
    SCI_GETLEXER = 4002
    SCI_COLOURISE = 4003
    SCI_SETPROPERTY = 4004
    SCI_SETKEYWORDS = 4005
    SCI_GETPROPERTY = 4008
    SCI_GETPROPERTYEXPANDED = 4009
    SCI_GETPROPERTYINT = 4010
    SCI_GETLEXERLANGUAGE = 4012
    SCI_PRIVATELEXERCALL = 4013
    SCI_PROPERTYNAMES = 4014
    SCI_PROPERTYTYPE = 4015
    SCI_DESCRIBEPROPERTY = 4016
    SCI_DESCRIBEKEYWORDSETS = 4017
    SCI_GETLINEENDTYPESSUPPORTED = 4018
    SCI_ALLOCATESUBSTYLES = 4020
    SCI_GETSUBSTYLESSTART = 4021
    SCI_GETSUBSTYLESLENGTH = 4022
    SCI_GETSTYLEFROMSUBSTYLE = 4027
    SCI_GETPRIMARYSTYLEFROMSTYLE = 4028
    SCI_FREESUBSTYLES = 4023
    SCI_SETIDENTIFIERS = 4024
    SCI_DISTANCETOSECONDARYSTYLES = 4025
    SCI_GETSUBSTYLEBASES = 4026
    SCI_GETNAMEDSTYLES = 4029
    SCI_NAMEOFSTYLE = 4030
    SCI_TAGSOFSTYLE = 4031
    SCI_DESCRIPTIONOFSTYLE = 4032
    SCI_SETILEXER = 4033
    SCI_SETSCALETECHNIQUE = 2820
    SCI_GETSCALETECHNIQUE = 2821
    SCI_GETBIDIRECTIONAL = 2708
    SCI_SETBIDIRECTIONAL = 2709


class LANGTYPE(_Enum):
    TXT = 0
    PHP = 1
    C = 2
    CPP = 3
    CS = 4
    OBJC = 5
    JAVA = 6
    RC = 7
    XML = 9
    MAKEFILE = 10
    PASCAL = 11
    BATCH = 12
    INI = 13
    USER = 15
    SQL = 17
    VB = 18
    JS = 19
    CSS = 20
    PERL = 21
    PYTHON = 22
    LUA = 23
    FORTRAN = 25
    BASH = 26
    FLASH = 27
    NSIS = 28
    TCL = 29
    LISP = 30
    SCHEME = 31
    DIFF = 33
    PROPS = 34
    PS = 35
    RUBY = 36
    SMALLTALK = 37
    VHDL = 38
    KIX = 39
    AU3 = 40
    ADA = 42
    VERILOG = 43
    MATLAB = 44
    HASKELL = 45
    INNO = 46
    SEARCHRESULT = 47
    YAML = 49
    COBOL = 50
    GUI4CLI = 51
    D = 52
    POWERSHELL = 53
    R = 54
    JSP = 55
    JSON = 57
    JAVASCRIPT = 58
    FORTRAN_77 = 59
    BAANC = 60
    SREC = 61
    TEHEX = 63
    SWIFT = 64
    AVS = 66
    BLITZBASIC = 67
    PUREBASIC = 68
    FREEBASIC = 69
    ERLANG = 71
    ESCRIPT = 72
    FORTH = 73
    LATEX = 74
    NIM = 76
    NNCRONTAB = 77
    OSCRIPT = 78
    REBOL = 79
    RUST = 81
    SPICE = 82
    TXT2TAGS = 83
    VISUALPROLOG = 84
    JSON5 = 86
    MSSQL = 87
    GDSCRIPT = 88
    HOLLYWOOD = 89
    RAKU = 91
    TOML = 92
    SAS = 93
    ERRORLIST = 94


class NOTIFICATION(_Enum):
    READY = 1001
    TBMODIFICATION = 1002
    FILEBEFORECLOSE = 1003
    FILEOPENED = 1004
    FILECLOSED = 1005
    FILEBEFOREOPEN = 1006
    FILEBEFORESAVE = 1007
    FILESAVED = 1008
    SHUTDOWN = 1009
    BUFFERACTIVATED = 1010
    LANGCHANGED = 1011
    WORDSTYLESUPDATED = 1012
    SHORTCUTREMAPPED = 1013
    FILEBEFORELOAD = 1014
    FILELOADFAILED = 1015
    READONLYCHANGED = 1016
    DOCORDERCHANGED = 1017
    SNAPSHOTDIRTYFILELOADED = 1018
    BEFORESHUTDOWN = 1019
    CANCELSHUTDOWN = 1020
    FILEBEFORERENAME = 1021
    FILERENAMECANCEL = 1022
    FILERENAMED = 1023
    FILEBEFOREDELETE = 1024
    FILEDELETEFAILED = 1025
    FILEDELETED = 1026
    DARKMODECHANGED = 1027
    CMDLINEPLUGINMSG = 1028
    EXTERNALLEXERBUFFER = 1029
    GLOBALMODIFIED = 1030
    NATIVELANGCHANGED = 1031
    TOOLBARICONSETCHANGED = 1032


class MENUCOMMAND(_Enum):
    FILEMENU_EXISTCMDPOSITION = 22
    FILE_PRINTNOW = 1001
    VIEW_GOTO_ANOTHER_VIEW = 10001
    VIEW_CLONE_TO_ANOTHER_VIEW = 10002
    VIEW_GOTO_NEW_INSTANCE = 10003
    VIEW_LOAD_IN_NEW_INSTANCE = 10004
    VIEW_GOTO_START = 10005
    VIEW_GOTO_END = 10006
    WINDOW_WINDOWS = 11001
    WINDOW_SORT_FN_ASC = 11002
    WINDOW_SORT_FN_DSC = 11003
    WINDOW_SORT_FP_ASC = 11004
    WINDOW_SORT_FP_DSC = 11005
    WINDOW_SORT_FT_ASC = 11006
    WINDOW_SORT_FT_DSC = 11007
    WINDOW_SORT_FS_ASC = 11008
    WINDOW_SORT_FS_DSC = 11009
    WINDOW_SORT_FD_ASC = 11010
    WINDOW_SORT_FD_DSC = 11011
    WINDOW_MRU_FIRST = 11020
    WINDOW_MRU_LIMIT = 11059
    WINDOW_COPY_NAME = 11060
    WINDOW_COPY_PATH = 11061
    DROPLIST_LIST = 14001
    DROPLIST_MRU_FIRST = 14020
    FILE = 41000
    FILE_NEW = 41001
    FILE_OPEN = 41002
    FILE_CLOSE = 41003
    FILE_CLOSEALL = 41004
    FILE_CLOSEALL_BUT_CURRENT = 41005
    FILE_SAVE = 41006
    FILE_SAVEALL = 41007
    FILE_SAVEAS = 41008
    FILE_CLOSEALL_TOLEFT = 41009
    FILE_PRINT = 41010
    FILE_EXIT = 41011
    FILE_LOADSESSION = 41012
    FILE_SAVESESSION = 41013
    FILE_RELOAD = 41014
    FILE_SAVECOPYAS = 41015
    FILE_DELETE = 41016
    FILE_RENAME = 41017
    FILE_CLOSEALL_TORIGHT = 41018
    FILE_OPEN_FOLDER = 41019
    FILE_OPEN_CMD = 41020
    FILE_RESTORELASTCLOSEDFILE = 41021
    FILE_OPENFOLDERASWORKSPACE = 41022
    FILE_OPEN_DEFAULT_VIEWER = 41023
    FILE_CLOSEALL_UNCHANGED = 41024
    FILE_CONTAININGFOLDERASWORKSPACE = 41025
    FILE_CLOSEALL_BUT_PINNED = 41026
    FILE_OPEN_POWERSHELL = 41027
    FILEMENU_LASTONE = 41027
    EDIT = 42000
    EDIT_CUT = 42001
    EDIT_COPY = 42002
    EDIT_UNDO = 42003
    EDIT_REDO = 42004
    EDIT_PASTE = 42005
    EDIT_DELETE = 42006
    EDIT_SELECTALL = 42007
    EDIT_INS_TAB = 42008
    EDIT_RMV_TAB = 42009
    EDIT_DUP_LINE = 42010
    EDIT_TRANSPOSE_LINE = 42011
    EDIT_SPLIT_LINES = 42012
    EDIT_JOIN_LINES = 42013
    EDIT_LINE_UP = 42014
    EDIT_LINE_DOWN = 42015
    EDIT_UPPERCASE = 42016
    EDIT_LOWERCASE = 42017
    MACRO_STARTRECORDINGMACRO = 42018
    MACRO_STOPRECORDINGMACRO = 42019
    EDIT_BEGINENDSELECT = 42020
    MACRO_PLAYBACKRECORDEDMACRO = 42021
    EDIT_BLOCK_COMMENT = 42022
    EDIT_STREAM_COMMENT = 42023
    EDIT_TRIMTRAILING = 42024
    MACRO_SAVECURRENTMACRO = 42025
    EDIT_RTL = 42026
    EDIT_LTR = 42027
    EDIT_TOGGLEREADONLY = 42028
    EDIT_FULLPATHTOCLIP = 42029
    EDIT_FILENAMETOCLIP = 42030
    EDIT_CURRENTDIRTOCLIP = 42031
    MACRO_RUNMULTIMACRODLG = 42032
    EDIT_TOGGLESYSTEMREADONLY = 42033
    EDIT_COLUMNMODE = 42034
    EDIT_BLOCK_COMMENT_SET = 42035
    EDIT_BLOCK_UNCOMMENT = 42036
    EDIT_COLUMNMODETIP = 42037
    EDIT_PASTE_AS_HTML = 42038
    EDIT_PASTE_AS_RTF = 42039
    OPEN_ALL_RECENT_FILE = 42040
    CLEAN_RECENT_FILE_LIST = 42041
    EDIT_TRIMLINEHEAD = 42042
    EDIT_TRIM_BOTH = 42043
    EDIT_EOL2WS = 42044
    EDIT_TRIMALL = 42045
    EDIT_TAB2SW = 42046
    EDIT_STREAM_UNCOMMENT = 42047
    EDIT_COPY_BINARY = 42048
    EDIT_CUT_BINARY = 42049
    EDIT_PASTE_BINARY = 42050
    EDIT_CHAR_PANEL = 42051
    EDIT_CLIPBOARDHISTORY_PANEL = 42052
    EDIT_SW2TAB_LEADING = 42053
    EDIT_SW2TAB_ALL = 42054
    EDIT_REMOVEEMPTYLINES = 42055
    EDIT_REMOVEEMPTYLINESWITHBLANK = 42056
    EDIT_BLANKLINEABOVECURRENT = 42057
    EDIT_BLANKLINEBELOWCURRENT = 42058
    EDIT_SORTLINES_LEXICOGRAPHIC_ASCENDING = 42059
    EDIT_SORTLINES_LEXICOGRAPHIC_DESCENDING = 42060
    EDIT_SORTLINES_INTEGER_ASCENDING = 42061
    EDIT_SORTLINES_INTEGER_DESCENDING = 42062
    EDIT_SORTLINES_DECIMALCOMMA_ASCENDING = 42063
    EDIT_SORTLINES_DECIMALCOMMA_DESCENDING = 42064
    EDIT_SORTLINES_DECIMALDOT_ASCENDING = 42065
    EDIT_SORTLINES_DECIMALDOT_DESCENDING = 42066
    EDIT_PROPERCASE_FORCE = 42067
    EDIT_PROPERCASE_BLEND = 42068
    EDIT_SENTENCECASE_FORCE = 42069
    EDIT_SENTENCECASE_BLEND = 42070
    EDIT_INVERTCASE = 42071
    EDIT_RANDOMCASE = 42072
    EDIT_OPENSELECTEDFILETOEDIT = 42073
    EDIT_OPENSELECTEDFILEFOLDERINEXPLORER = 42074
    EDIT_SEARCHONINTERNET = 42075
    EDIT_CHANGESEARCHENGINE = 42076
    EDIT_REMOVE_CONSECUTIVE_DUP_LINES = 42077
    EDIT_SORTLINES_RANDOMLY = 42078
    EDIT_REMOVE_ANY_DUP_LINES = 42079
    EDIT_SORTLINES_LEXICO_CASE_INSENS_ASCENDING = 42080
    EDIT_SORTLINES_LEXICO_CASE_INSENS_DESCENDING = 42081
    EDIT_COPY_LINK = 42082
    EDIT_SORTLINES_REVERSE_ORDER = 42083
    EDIT_INSERT_DATETIME_SHORT = 42084
    EDIT_INSERT_DATETIME_LONG = 42085
    EDIT_INSERT_DATETIME_CUSTOMIZED = 42086
    EDIT_COPY_ALL_NAMES = 42087
    EDIT_COPY_ALL_PATHS = 42088
    EDIT_BEGINENDSELECT_COLUMNMODE = 42089
    EDIT_MULTISELECTALL = 42090
    EDIT_MULTISELECTALLMATCHCASE = 42091
    EDIT_MULTISELECTALLWHOLEWORD = 42092
    EDIT_MULTISELECTALLMATCHCASEWHOLEWORD = 42093
    EDIT_MULTISELECTNEXT = 42094
    EDIT_MULTISELECTNEXTMATCHCASE = 42095
    EDIT_MULTISELECTNEXTWHOLEWORD = 42096
    EDIT_MULTISELECTNEXTMATCHCASEWHOLEWORD = 42097
    EDIT_MULTISELECTUNDO = 42098
    EDIT_MULTISELECTSSKIP = 42099
    EDIT_SORTLINES_LOCALE_ASCENDING = 42100
    EDIT_SORTLINES_LOCALE_DESCENDING = 42101
    EDIT_SETREADONLYFORALLDOCS = 42102
    EDIT_CLEARREADONLYFORALLDOCS = 42103
    EDIT_SORTLINES_LENGTH_ASCENDING = 42104
    EDIT_SORTLINES_LENGTH_DESCENDING = 42105
    EDIT_REDACT_SELECTION = 42106
    SEARCH = 43000
    SEARCH_FIND = 43001
    SEARCH_FINDNEXT = 43002
    SEARCH_REPLACE = 43003
    SEARCH_GOTOLINE = 43004
    SEARCH_TOGGLE_BOOKMARK = 43005
    SEARCH_NEXT_BOOKMARK = 43006
    SEARCH_PREV_BOOKMARK = 43007
    SEARCH_CLEAR_BOOKMARKS = 43008
    SEARCH_GOTOMATCHINGBRACE = 43009
    SEARCH_FINDPREV = 43010
    SEARCH_FINDINCREMENT = 43011
    SEARCH_FINDINFILES = 43013
    SEARCH_VOLATILE_FINDNEXT = 43014
    SEARCH_VOLATILE_FINDPREV = 43015
    SEARCH_CUTMARKEDLINES = 43018
    SEARCH_COPYMARKEDLINES = 43019
    SEARCH_PASTEMARKEDLINES = 43020
    SEARCH_DELETEMARKEDLINES = 43021
    SEARCH_MARKALLEXT1 = 43022
    SEARCH_UNMARKALLEXT1 = 43023
    SEARCH_MARKALLEXT2 = 43024
    SEARCH_UNMARKALLEXT2 = 43025
    SEARCH_MARKALLEXT3 = 43026
    SEARCH_UNMARKALLEXT3 = 43027
    SEARCH_MARKALLEXT4 = 43028
    SEARCH_UNMARKALLEXT4 = 43029
    SEARCH_MARKALLEXT5 = 43030
    SEARCH_UNMARKALLEXT5 = 43031
    SEARCH_CLEARALLMARKS = 43032
    SEARCH_GOPREVMARKER1 = 43033
    SEARCH_GOPREVMARKER2 = 43034
    SEARCH_GOPREVMARKER3 = 43035
    SEARCH_GOPREVMARKER4 = 43036
    SEARCH_GOPREVMARKER5 = 43037
    SEARCH_GOPREVMARKER_DEF = 43038
    SEARCH_GONEXTMARKER1 = 43039
    SEARCH_GONEXTMARKER2 = 43040
    SEARCH_GONEXTMARKER3 = 43041
    SEARCH_GONEXTMARKER4 = 43042
    SEARCH_GONEXTMARKER5 = 43043
    SEARCH_GONEXTMARKER_DEF = 43044
    FOCUS_ON_FOUND_RESULTS = 43045
    SEARCH_GOTONEXTFOUND = 43046
    SEARCH_GOTOPREVFOUND = 43047
    SEARCH_SETANDFINDNEXT = 43048
    SEARCH_SETANDFINDPREV = 43049
    SEARCH_INVERSEMARKS = 43050
    SEARCH_DELETEUNMARKEDLINES = 43051
    SEARCH_FINDCHARINRANGE = 43052
    SEARCH_SELECTMATCHINGBRACES = 43053
    SEARCH_MARK = 43054
    SEARCH_STYLE1TOCLIP = 43055
    SEARCH_STYLE2TOCLIP = 43056
    SEARCH_STYLE3TOCLIP = 43057
    SEARCH_STYLE4TOCLIP = 43058
    SEARCH_STYLE5TOCLIP = 43059
    SEARCH_ALLSTYLESTOCLIP = 43060
    SEARCH_MARKEDTOCLIP = 43061
    SEARCH_MARKONEEXT1 = 43062
    SEARCH_MARKONEEXT2 = 43063
    SEARCH_MARKONEEXT3 = 43064
    SEARCH_MARKONEEXT4 = 43065
    SEARCH_MARKONEEXT5 = 43066
    SEARCH_CHANGED_NEXT = 43067
    SEARCH_CHANGED_PREV = 43068
    SEARCH_CLEAR_CHANGE_HISTORY = 43069
    SYSTRAYPOPUP = 43100
    SYSTRAYPOPUP_ACTIVATE = 43101
    SYSTRAYPOPUP_NEWDOC = 43102
    SYSTRAYPOPUP_NEW_AND_PASTE = 43103
    SYSTRAYPOPUP_OPENFILE = 43104
    SYSTRAYPOPUP_CLOSE = 43105
    MISC = 43500
    DOCLIST_FILESCLOSE = 43501
    DOCLIST_FILESCLOSEOTHERS = 43502
    DOCLIST_COPYNAMES = 43503
    DOCLIST_COPYPATHS = 43504
    VIEW = 44000
    VIEW_POSTIT = 44009
    VIEW_FOLDALL = 44010
    VIEW_DISTRACTIONFREE = 44011
    VIEW_ALL_CHARACTERS = 44019
    VIEW_INDENT_GUIDE = 44020
    VIEW_WRAP = 44022
    VIEW_ZOOMIN = 44023
    VIEW_ZOOMOUT = 44024
    VIEW_TAB_SPACE = 44025
    VIEW_EOL = 44026
    VIEW_ZOOM_SYNC = 44027
    VIEW_UNFOLDALL = 44029
    VIEW_FOLD_CURRENT = 44030
    VIEW_UNFOLD_CURRENT = 44031
    VIEW_FULLSCREENTOGGLE = 44032
    VIEW_ZOOMRESTORE = 44033
    VIEW_ALWAYSONTOP = 44034
    VIEW_SYNSCROLLV = 44035
    VIEW_SYNSCROLLH = 44036
    VIEW_WRAP_SYMBOL = 44041
    VIEW_HIDELINES = 44042
    PINTAB = 44048
    VIEW_SUMMARY = 44049
    VIEW_FOLD = 44050
    VIEW_FOLD_1 = 44051
    VIEW_FOLD_2 = 44052
    VIEW_FOLD_3 = 44053
    VIEW_FOLD_4 = 44054
    VIEW_FOLD_5 = 44055
    VIEW_FOLD_6 = 44056
    VIEW_FOLD_7 = 44057
    VIEW_FOLD_8 = 44058
    VIEW_UNFOLD = 44060
    VIEW_UNFOLD_1 = 44061
    VIEW_UNFOLD_2 = 44062
    VIEW_UNFOLD_3 = 44063
    VIEW_UNFOLD_4 = 44064
    VIEW_UNFOLD_5 = 44065
    VIEW_UNFOLD_6 = 44066
    VIEW_UNFOLD_7 = 44067
    VIEW_UNFOLD_8 = 44068
    VIEW_DOCLIST = 44070
    VIEW_SWITCHTO_OTHER_VIEW = 44072
    EXPORT_FUNC_LIST_AND_QUIT = 44073
    VIEW_DOC_MAP = 44080
    VIEW_PROJECT_PANEL_1 = 44081
    VIEW_PROJECT_PANEL_2 = 44082
    VIEW_PROJECT_PANEL_3 = 44083
    VIEW_FUNC_LIST = 44084
    VIEW_FILEBROWSER = 44085
    VIEW_TAB1 = 44086
    VIEW_TAB2 = 44087
    VIEW_TAB3 = 44088
    VIEW_TAB4 = 44089
    VIEW_TAB5 = 44090
    VIEW_TAB6 = 44091
    VIEW_TAB7 = 44092
    VIEW_TAB8 = 44093
    VIEW_TAB9 = 44094
    VIEW_TAB_NEXT = 44095
    VIEW_TAB_PREV = 44096
    VIEW_MONITORING = 44097
    VIEW_TAB_MOVEFORWARD = 44098
    VIEW_TAB_MOVEBACKWARD = 44099
    VIEW_IN_FIREFOX = 44100
    VIEW_IN_CHROME = 44101
    VIEW_IN_EDGE = 44102
    VIEW_IN_IE = 44103
    VIEW_SWITCHTO_PROJECT_PANEL_1 = 44104
    VIEW_SWITCHTO_PROJECT_PANEL_2 = 44105
    VIEW_SWITCHTO_PROJECT_PANEL_3 = 44106
    VIEW_SWITCHTO_FILEBROWSER = 44107
    VIEW_SWITCHTO_FUNC_LIST = 44108
    VIEW_SWITCHTO_DOCLIST = 44109
    VIEW_TAB_COLOUR_NONE = 44110
    VIEW_TAB_COLOUR_1 = 44111
    VIEW_TAB_COLOUR_2 = 44112
    VIEW_TAB_COLOUR_3 = 44113
    VIEW_TAB_COLOUR_4 = 44114
    VIEW_TAB_COLOUR_5 = 44115
    VIEW_TAB_START = 44116
    VIEW_TAB_END = 44117
    VIEW_NPC = 44130
    VIEW_NPC_CCUNIEOL = 44131
    FORMAT = 45000
    FORMAT_TODOS = 45001
    FORMAT_TOUNIX = 45002
    FORMAT_TOMAC = 45003
    FORMAT_ANSI = 45004
    FORMAT_UTF_8 = 45005
    FORMAT_UTF_16BE = 45006
    FORMAT_UTF_16LE = 45007
    FORMAT_AS_UTF_8 = 45008
    FORMAT_CONV2_ANSI = 45009
    FORMAT_CONV2_AS_UTF_8 = 45010
    FORMAT_CONV2_UTF_8 = 45011
    FORMAT_CONV2_UTF_16BE = 45012
    FORMAT_CONV2_UTF_16LE = 45013
    FORMAT_ENCODE = 45020
    FORMAT_WIN_1250 = 45020
    FORMAT_WIN_1251 = 45021
    FORMAT_WIN_1252 = 45022
    FORMAT_WIN_1253 = 45023
    FORMAT_WIN_1254 = 45024
    FORMAT_WIN_1255 = 45025
    FORMAT_WIN_1256 = 45026
    FORMAT_WIN_1257 = 45027
    FORMAT_WIN_1258 = 45028
    FORMAT_ISO_8859_1 = 45029
    FORMAT_ISO_8859_2 = 45030
    FORMAT_ISO_8859_3 = 45031
    FORMAT_ISO_8859_4 = 45032
    FORMAT_ISO_8859_5 = 45033
    FORMAT_ISO_8859_6 = 45034
    FORMAT_ISO_8859_7 = 45035
    FORMAT_ISO_8859_8 = 45036
    FORMAT_ISO_8859_9 = 45037
    FORMAT_ISO_8859_13 = 45040
    FORMAT_ISO_8859_14 = 45041
    FORMAT_ISO_8859_15 = 45042
    FORMAT_DOS_437 = 45044
    FORMAT_DOS_720 = 45045
    FORMAT_DOS_737 = 45046
    FORMAT_DOS_775 = 45047
    FORMAT_DOS_850 = 45048
    FORMAT_DOS_852 = 45049
    FORMAT_DOS_855 = 45050
    FORMAT_DOS_857 = 45051
    FORMAT_DOS_858 = 45052
    FORMAT_DOS_860 = 45053
    FORMAT_DOS_861 = 45054
    FORMAT_DOS_862 = 45055
    FORMAT_DOS_863 = 45056
    FORMAT_DOS_865 = 45057
    FORMAT_DOS_866 = 45058
    FORMAT_DOS_869 = 45059
    FORMAT_BIG5 = 45060
    FORMAT_GB2312 = 45061
    FORMAT_SHIFT_JIS = 45062
    FORMAT_KOREAN_WIN = 45063
    FORMAT_EUC_KR = 45064
    FORMAT_TIS_620 = 45065
    FORMAT_MAC_CYRILLIC = 45066
    FORMAT_KOI8U_CYRILLIC = 45067
    FORMAT_KOI8R_CYRILLIC = 45068
    FORMAT_ENCODE_END = 45068
    LANG = 46000
    LANGSTYLE_CONFIG_DLG = 46001
    LANG_C = 46002
    LANG_CPP = 46003
    LANG_JAVA = 46004
    LANG_HTML = 46005
    LANG_XML = 46006
    LANG_JS = 46007
    LANG_PHP = 46008
    LANG_ASP = 46009
    LANG_CSS = 46010
    LANG_PASCAL = 46011
    LANG_PYTHON = 46012
    LANG_PERL = 46013
    LANG_OBJC = 46014
    LANG_ASCII = 46015
    LANG_TEXT = 46016
    LANG_RC = 46017
    LANG_MAKEFILE = 46018
    LANG_INI = 46019
    LANG_SQL = 46020
    LANG_VB = 46021
    LANG_BATCH = 46022
    LANG_CS = 46023
    LANG_LUA = 46024
    LANG_TEX = 46025
    LANG_FORTRAN = 46026
    LANG_BASH = 46027
    LANG_FLASH = 46028
    LANG_NSIS = 46029
    LANG_TCL = 46030
    LANG_LISP = 46031
    LANG_SCHEME = 46032
    LANG_ASM = 46033
    LANG_DIFF = 46034
    LANG_PROPS = 46035
    LANG_PS = 46036
    LANG_RUBY = 46037
    LANG_SMALLTALK = 46038
    LANG_VHDL = 46039
    LANG_CAML = 46040
    LANG_KIX = 46041
    LANG_ADA = 46042
    LANG_VERILOG = 46043
    LANG_AU3 = 46044
    LANG_MATLAB = 46045
    LANG_HASKELL = 46046
    LANG_INNO = 46047
    LANG_CMAKE = 46048
    LANG_YAML = 46049
    LANG_COBOL = 46050
    LANG_D = 46051
    LANG_GUI4CLI = 46052
    LANG_POWERSHELL = 46053
    LANG_R = 46054
    LANG_JSP = 46055
    LANG_COFFEESCRIPT = 46056
    LANG_JSON = 46057
    LANG_FORTRAN_77 = 46058
    LANG_BAANC = 46059
    LANG_SREC = 46060
    LANG_IHEX = 46061
    LANG_TEHEX = 46062
    LANG_SWIFT = 46063
    LANG_ASN1 = 46064
    LANG_AVS = 46065
    LANG_BLITZBASIC = 46066
    LANG_PUREBASIC = 46067
    LANG_FREEBASIC = 46068
    LANG_CSOUND = 46069
    LANG_ERLANG = 46070
    LANG_ESCRIPT = 46071
    LANG_FORTH = 46072
    LANG_LATEX = 46073
    LANG_MMIXAL = 46074
    LANG_NIM = 46075
    LANG_NNCRONTAB = 46076
    LANG_OSCRIPT = 46077
    LANG_REBOL = 46078
    LANG_REGISTRY = 46079
    LANG_RUST = 46080
    LANG_SPICE = 46081
    LANG_TXT2TAGS = 46082
    LANG_VISUALPROLOG = 46083
    LANG_TYPESCRIPT = 46084
    LANG_JSON5 = 46085
    LANG_MSSQL = 46086
    LANG_GDSCRIPT = 46087
    LANG_HOLLYWOOD = 46088
    LANG_GOLANG = 46089
    LANG_RAKU = 46090
    LANG_TOML = 46091
    LANG_SAS = 46092
    LANG_ERRORLIST = 46093
    LANG_ESCSEQ = 46094
    LANG_STTXT = 46095
    LANG_EXTERNAL = 46165
    LANG_EXTERNAL_LIMIT = 46179
    LANG_USER = 46180
    LANG_USER_LIMIT = 46210
    LANG_USER_DLG = 46250
    LANG_OPENUDLDIR = 46300
    LANG_UDLCOLLECTION_PROJECT_SITE = 46301
    ABOUT = 47000
    HOMESWEETHOME = 47001
    PROJECTPAGE = 47002
    ONLINEDOCUMENT = 47003
    FORUM = 47004
    UPDATE_NPP = 47006
    CONFUPDATERPROXY = 47009
    CMDLINEARGUMENTS = 47010
    DEBUGINFO = 47012
    SETTING = 48000
    SETTING_IMPORTPLUGIN = 48005
    SETTING_IMPORTSTYLETHEMES = 48006
    SETTING_TRAYICON = 48008
    SETTING_SHORTCUT_MAPPER = 48009
    SETTING_REMEMBER_LAST_SESSION = 48010
    SETTING_PREFERENCE = 48011
    SETTING_OPENPLUGINSDIR = 48014
    SETTING_PLUGINADM = 48015
    SETTING_SHORTCUT_MAPPER_MACRO = 48016
    SETTING_SHORTCUT_MAPPER_RUN = 48017
    SETTING_EDITCONTEXTMENU = 48018
    TOOL = 48500
    TOOL_MD5_GENERATE = 48501
    TOOL_MD5_GENERATEFROMFILE = 48502
    TOOL_MD5_GENERATEINTOCLIPBOARD = 48503
    TOOL_SHA256_GENERATE = 48504
    TOOL_SHA256_GENERATEFROMFILE = 48505
    TOOL_SHA256_GENERATEINTOCLIPBOARD = 48506
    TOOL_SHA1_GENERATE = 48507
    TOOL_SHA1_GENERATEFROMFILE = 48508
    TOOL_SHA1_GENERATEINTOCLIPBOARD = 48509
    TOOL_SHA512_GENERATE = 48510
    TOOL_SHA512_GENERATEFROMFILE = 48511
    TOOL_SHA512_GENERATEINTOCLIPBOARD = 48512
    EXECUTE = 49000
    EXECUTE_VALIDATE_SHORTCUTSXML = 49001
    EDIT_AUTOCOMPLETE = 50000
    EDIT_AUTOCOMPLETE_CURRENTFILE = 50001
    EDIT_FUNCCALLTIP = 50002
    EDIT_AUTOCOMPLETE_PATH = 50006
    EDIT_FUNCCALLTIP_PREVIOUS = 50010
    EDIT_FUNCCALLTIP_NEXT = 50011
