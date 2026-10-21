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


#include "ui/Printer.h"

#include "ui/EditorView.h"

#include <QFileInfo>
#include <QFontMetrics>
#include <QLocale>
#include <QPagedPaintDevice>
#include <QPainter>

#include <algorithm>

namespace edita {

namespace {

constexpr int kMarginLineNumber = 0;

QFont printFont(const QString& family, int size, int style)
{
    // Por defecto Arial a 9 puntos, como N++ (Qt la sustituye si no está)
    QFont font(family.isEmpty() ? QStringLiteral("Arial") : family);
    font.setStyleHint(QFont::SansSerif);
    font.setPointSize(size > 0 ? size : 9);
    font.setBold((style & 1) != 0);
    font.setItalic((style & 2) != 0);
    return font;
}

struct Band
{
    QString left;
    QString middle;
    QString right;
    QFont font;
    int lineHeight = 0;

    bool present() const { return !left.isEmpty() || !middle.isEmpty() || !right.isEmpty(); }
};

// Una línea de cabecera o pie: izquierda, centro y derecha, y una raya que la separa del texto
void drawBand(QPainter& painter, const Band& band, const QRect& area, int lineY, const QString& path,
              const QDateTime& now, int page)
{
    painter.save();
    painter.setClipping(false);
    painter.setFont(band.font);
    painter.setPen(Qt::black);
    const QRect text = area.adjusted(5, 0, 0, 0);
    if (!band.left.isEmpty())
        painter.drawText(text, Qt::AlignLeft | Qt::AlignVCenter, expandPrintVariables(band.left, path, now, page));
    if (!band.middle.isEmpty())
        painter.drawText(area, Qt::AlignHCenter | Qt::AlignVCenter, expandPrintVariables(band.middle, path, now, page));
    if (!band.right.isEmpty())
        painter.drawText(area, Qt::AlignRight | Qt::AlignVCenter, expandPrintVariables(band.right, path, now, page));
    painter.drawLine(area.left(), lineY, area.right(), lineY);
    painter.restore();
}

} // namespace

QString expandPrintVariables(QString text, const QString& documentPath, const QDateTime& now, int page)
{
    const QFileInfo info(documentPath);
    const QLocale locale;
    const std::pair<const char*, QString> variables[] = {
        {"$(FULL_CURRENT_PATH)", documentPath},
        {"$(CURRENT_DIRECTORY)", info.isAbsolute() ? info.absolutePath() : QString()},
        {"$(FILE_NAME)", info.fileName()},
        {"$(NAME_PART)", info.completeBaseName()},
        {"$(EXT_PART)", info.suffix().isEmpty() ? QString() : QStringLiteral(".") + info.suffix()},
        {"$(SHORT_DATE)", locale.toString(now.date(), QLocale::ShortFormat)},
        {"$(LONG_DATE)", locale.toString(now.date(), QLocale::LongFormat)},
        {"$(TIME)", locale.toString(now.time(), QLocale::ShortFormat)},
        {"$(CURRENT_PRINTING_PAGE)", QString::number(page)},
    };
    for (const auto& [name, value] : variables)
        text.replace(QLatin1String(name), value);
    return text;
}

int printDocument(EditorView& view, QPagedPaintDevice& device, const Preferences& prefs, const PrintRequest& request)
{
    // Márgenes del usuario en mm; la impresora impone los suyos como mínimo
    device.setPageMargins(QMarginsF(prefs.printMarginLeft, prefs.printMarginTop, prefs.printMarginRight,
                                    prefs.printMarginBottom),
                          QPageLayout::Millimeter);
    QPainter painter;
    if (!painter.begin(&device))
        return 0;

    Band header{prefs.printHeaderLeft, prefs.printHeaderMiddle, prefs.printHeaderRight,
                printFont(prefs.printHeaderFontName, prefs.printHeaderFontSize, prefs.printHeaderFontStyle)};
    Band footer{prefs.printFooterLeft, prefs.printFooterMiddle, prefs.printFooterRight,
                printFont(prefs.printFooterFontName, prefs.printFooterFontSize, prefs.printFooterFontStyle)};
    header.lineHeight = QFontMetrics(header.font, &device).height();
    footer.lineHeight = QFontMetrics(footer.font, &device).height();

    // Área del texto: la página menos un margen interior de línea y media (printMarge)
    const QRect page(0, 0, device.width(), device.height());
    const int printMarge = footer.lineHeight + footer.lineHeight / 2;
    QRect rc = page.adjusted(printMarge, printMarge, -printMarge, -printMarge);
    if (header.present())
        rc.setTop(rc.top() + header.lineHeight + header.lineHeight / 2);
    if (footer.present())
        rc.setBottom(rc.bottom() - footer.lineHeight - footer.lineHeight / 2);

    sptr_t printed = 0;
    sptr_t end = view.sci(SCI_GETLENGTH);
    if (request.selectionOnly && request.selectionStart != request.selectionEnd) {
        printed = std::min(request.selectionStart, request.selectionEnd);
        end = std::min(std::max(request.selectionStart, request.selectionEnd), end);
    }

    const sptr_t lineNumberWidth = view.sci(SCI_GETMARGINWIDTHN, kMarginLineNumber);
    if (!prefs.printLineNumber)
        view.sci(SCI_SETMARGINWIDTHN, kMarginLineNumber, 0);
    view.sci(SCI_SETPRINTCOLOURMODE, static_cast<uptr_t>(prefs.printColourMode));

    const QDateTime now = QDateTime::currentDateTime();
    int pageNum = 1;
    int emitted = 0;
    do {
        const bool printPage = request.fromPage <= 0 || (pageNum >= request.fromPage && pageNum <= request.toPage);
        if (printPage) {
            if (emitted > 0)
                device.newPage();
            ++emitted;
            if (header.present()) {
                const QRect area(rc.left(), rc.top() - header.lineHeight - header.lineHeight / 2, rc.width(), header.lineHeight);
                drawBand(painter, header, area, rc.top() - header.lineHeight / 4, request.documentPath, now, pageNum);
            }
        }

        Sci_RangeToFormatFull range{};
        range.hdc = &device;
        range.hdcTarget = &device;
        range.rc = {rc.left(), rc.top(), rc.right(), rc.bottom()};
        range.rcPage = {page.left(), page.top(), page.right(), page.bottom()};
        range.chrg = {printed, end};

        // Un salto de página (\f) termina la página justo antes. Scintilla imprime
        // líneas enteras hasta la de cpMax, así que, como N++, se para un carácter antes
        sptr_t formFeed = -1;
        if (prefs.printFormFeedPageBreak) {
            Sci_TextToFindFull find{{printed, end}, "\f", {0, 0}};
            formFeed = view.sci(SCI_FINDTEXTFULL, SCFIND_NONE, reinterpret_cast<sptr_t>(&find));
            if (formFeed > 0)
                range.chrg.cpMax = formFeed - 1;
            else if (formFeed == 0)
                range.chrg.cpMax = 0;
        }
        sptr_t next = printed < end ? view.sci(SCI_FORMATRANGEFULL, printPage ? 1 : 0, reinterpret_cast<sptr_t>(&range)) : end;
        if (formFeed != -1 && next <= formFeed)
            next = formFeed + 1;
        if (next <= printed && printed < end)
            next = end; // no debería pasar, pero evita un bucle infinito
        printed = next;

        if (printPage && footer.present()) {
            const QRect area(rc.left(), rc.bottom() + footer.lineHeight / 2, rc.width(), footer.lineHeight);
            drawBand(painter, footer, area, rc.bottom() + footer.lineHeight / 4, request.documentPath, now, pageNum);
        }
        ++pageNum;
        if (request.fromPage > 0 && pageNum > request.toPage)
            break;
    } while (printed < end);

    view.sci(SCI_FORMATRANGEFULL, 0, 0);
    if (!prefs.printLineNumber)
        view.sci(SCI_SETMARGINWIDTHN, kMarginLineNumber, lineNumberWidth);
    painter.end();
    return emitted;
}

} // namespace edita
