// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file book_print.cpp
/// @brief The print of a book: the pages that go to the printer, the sides of the sheets, and
///        their drawing

#include <kalahari/editor/book_print.h>

#include <kalahari/editor/book_pages.h>
#include <kalahari/editor/render_context.h>

#include <QPageLayout>
#include <QPagedPaintDevice>
#include <QPainter>
#include <QPen>
#include <QRegularExpression>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <limits>
#include <set>

namespace kalahari::editor {

namespace {

/// The crop marks: from the corner of the page so far, so long, so thick (points: 3 mm, 5 mm
/// and a hairline that a printer still prints)
constexpr double CROP_MARK_OFFSET = 8.5;
constexpr double CROP_MARK_LENGTH = 14.0;
constexpr double CROP_MARK_WIDTH = 0.3;

/// A booklet's sheet has four pages: two on its front, two on its back
constexpr int PAGES_OF_BOOKLET_SHEET = 4;

/// The number of a page as text, from 1; a number too large for an int is beyond any book
int pageNumberOf(const QString& digits) {
    bool ok = false;
    const int number = digits.toInt(&ok);
    return ok ? number : std::numeric_limits<int>::max();
}

/// The crop marks of the corners of @p page: two short lines at each, apart from it
void paintCropMarks(QPainter* painter, const QRectF& page) {
    painter->save();
    QPen pen(Qt::black);
    pen.setWidthF(CROP_MARK_WIDTH);
    painter->setPen(pen);
    const double near = CROP_MARK_OFFSET;
    const double far = CROP_MARK_OFFSET + CROP_MARK_LENGTH;
    for (const double x : {page.left(), page.right()}) {
        const double outward = x == page.left() ? -1.0 : 1.0;
        for (const double y : {page.top(), page.bottom()}) {
            const double down = y == page.top() ? -1.0 : 1.0;
            // The line along the edge of the top or the bottom, and the one along the side
            painter->drawLine(QPointF(x + outward * near, y), QPointF(x + outward * far, y));
            painter->drawLine(QPointF(x, y + down * near), QPointF(x, y + down * far));
        }
    }
    painter->restore();
}

}  // namespace

bool sheetSideways(const SheetOptions& options, const QSizeF& pageSize) {
    return options.layout != SheetLayout::OnePage || pageSize.width() > pageSize.height();
}

std::optional<std::vector<int>> pagesOfRange(const QString& text, int count) {
    // A dash between two numbers, with or without spaces around it, joins them
    static const QRegularExpression dash(QStringLiteral("\\s*[-\\x{2013}\\x{2014}]\\s*"));
    static const QRegularExpression separators(QStringLiteral("[,;\\s]+"));
    static const QRegularExpression part(QStringLiteral("^(\\d*)-(\\d*)$|^(\\d+)$"));
    QString joined = text.trimmed();
    joined.replace(dash, QStringLiteral("-"));
    const QStringList parts = joined.split(separators, Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return std::nullopt;
    }
    std::set<int> pages;
    for (const QString& each : parts) {
        const QRegularExpressionMatch match = part.match(each);
        if (!match.hasMatch() || each == QLatin1String("-")) {
            return std::nullopt;
        }
        int first = 0;
        int last = 0;
        if (match.capturedLength(3) > 0) {
            first = pageNumberOf(match.captured(3));
            last = first;
        } else {
            // "-5": from the first page; "5-": to the last one
            first = match.capturedLength(1) > 0 ? pageNumberOf(match.captured(1)) : 1;
            last = match.capturedLength(2) > 0 ? pageNumberOf(match.captured(2)) : count;
        }
        if (first > last) {
            std::swap(first, last);
        }
        for (int page = std::max(first, 1); page <= std::min(last, count); ++page) {
            pages.insert(page - 1);
        }
    }
    return std::vector<int>(pages.cbegin(), pages.cend());
}

std::vector<int> pagesOfParity(const std::vector<int>& pages, PageParity parity) {
    if (parity == PageParity::All) {
        return pages;
    }
    // A page from 0 shows a number one higher: the odd numbers are on the even pages
    const int remainder = parity == PageParity::Odd ? 0 : 1;
    std::vector<int> chosen;
    std::copy_if(pages.cbegin(), pages.cend(), std::back_inserter(chosen),
                 [remainder](int page) { return page % 2 == remainder; });
    return chosen;
}

std::vector<SheetSide> sheetSides(const std::vector<int>& pages, SheetLayout layout) {
    std::vector<SheetSide> sides;
    const auto count = static_cast<int>(pages.size());
    switch (layout) {
        case SheetLayout::OnePage:
            for (const int page : pages) {
                sides.push_back({page});
            }
            break;
        case SheetLayout::TwoPages:
            // As an open book: a left page (an odd index) and the right one after it, so the
            // halves of a spread lie on one side; page 1 alone on the right, and so is each
            // page whose neighbour does not print
            for (size_t i = 0; i < pages.size(); ++i) {
                const int page = pages[i];
                if (page % 2 == 0) {
                    sides.push_back({-1, page});
                } else if (i + 1 < pages.size() && pages[i + 1] == page + 1) {
                    sides.push_back({page, page + 1});
                    ++i;
                } else {
                    sides.push_back({page, -1});
                }
            }
            break;
        case SheetLayout::Booklet: {
            // Sheet i: on its front the last page but 2i and page 2i, on its back page 2i + 1
            // and the last but 2i + 1; folded, the stack reads in order
            const int padded = (count + PAGES_OF_BOOKLET_SHEET - 1) / PAGES_OF_BOOKLET_SHEET *
                               PAGES_OF_BOOKLET_SHEET;
            const auto at = [&pages, count](int index) {
                return index < count ? pages[static_cast<size_t>(index)] : -1;
            };
            for (int i = 0; i < padded / PAGES_OF_BOOKLET_SHEET; ++i) {
                sides.push_back({at(padded - 1 - 2 * i), at(2 * i)});
                sides.push_back({at(2 * i + 1), at(padded - 2 - 2 * i)});
            }
            break;
        }
    }
    return sides;
}

int sheetCount(int sides, bool twoSided) {
    return twoSided ? (sides + 1) / 2 : sides;
}

std::vector<SheetSide> copiesOf(const std::vector<SheetSide>& sides, int copies, bool collate,
                                bool twoSided) {
    // A sheet: its front, and its back when it prints on both sides
    const size_t perSheet = twoSided ? 2 : 1;
    std::vector<SheetSide> sheets = sides;
    if (twoSided && copies > 1 && sheets.size() % 2 != 0) {
        sheets.emplace_back();
    }
    std::vector<SheetSide> printed;
    printed.reserve(sheets.size() * static_cast<size_t>(std::max(copies, 0)));
    if (collate) {
        for (int copy = 0; copy < copies; ++copy) {
            printed.insert(printed.end(), sheets.cbegin(), sheets.cend());
        }
        return printed;
    }
    for (size_t sheet = 0; sheet < sheets.size(); sheet += perSheet) {
        for (int copy = 0; copy < copies; ++copy) {
            printed.insert(printed.end(), sheets.cbegin() + static_cast<std::ptrdiff_t>(sheet),
                           sheets.cbegin() + static_cast<std::ptrdiff_t>(sheet + perSheet));
        }
    }
    return printed;
}

std::vector<SheetSide> reversedSheets(const std::vector<SheetSide>& sides, bool twoSided) {
    std::vector<SheetSide> sheets = sides;
    if (!twoSided) {
        std::ranges::reverse(sheets);
        return sheets;
    }
    if (sheets.size() % 2 != 0) {
        sheets.emplace_back();
    }
    std::vector<SheetSide> reversed;
    reversed.reserve(sheets.size());
    for (size_t end = sheets.size(); end >= 2; end -= 2) {
        reversed.push_back(sheets[end - 2]);
        reversed.push_back(sheets[end - 1]);
    }
    return reversed;
}

std::vector<SheetSide> sidesOfSheets(const std::vector<SheetSide>& sides, bool backs) {
    std::vector<SheetSide> chosen;
    chosen.reserve((sides.size() + 1) / 2);
    for (size_t side = backs ? 1 : 0; side < sides.size(); side += 2) {
        chosen.push_back(sides[side]);
    }
    // The last sheet of an odd number of sides has an empty back
    if (backs && sides.size() % 2 != 0) {
        chosen.emplace_back();
    }
    return chosen;
}

std::vector<QRectF> pageRects(const QSizeF& pageSize, int pagesOnSide, const QSizeF& paper,
                              const QRectF& printable, const SheetOptions& options) {
    if (pageSize.isEmpty() || paper.isEmpty() || pagesOnSide <= 0) {
        return {};
    }
    const QRectF area = printable.isEmpty() ? QRectF(QPointF(0, 0), paper) : printable;
    if (options.layout == SheetLayout::OnePage) {
        if (!options.fit) {
            // Its own size, in the middle of the sheet
            QRectF page(QPointF(0, 0), pageSize);
            page.moveCenter(QPointF(paper.width() / 2.0, paper.height() / 2.0));
            return {page};
        }
        const double scale =
            std::min(area.width() / pageSize.width(), area.height() / pageSize.height());
        QRectF page(QPointF(0, 0), pageSize * scale);
        page.moveCenter(area.center());
        return {page};
    }
    // Two pages: their spine in the middle of the sheet, where a booklet folds
    const double spine = paper.width() / 2.0;
    const double scale = std::max(
        0.0, std::min({(spine - area.left()) / pageSize.width(),
                       (area.right() - spine) / pageSize.width(),
                       area.height() / pageSize.height()}));
    const QSizeF size = pageSize * scale;
    const double top = area.center().y() - size.height() / 2.0;
    return {QRectF(QPointF(spine - size.width(), top), size), QRectF(QPointF(spine, top), size)};
}

void paintSheetSide(QPainter* painter, const BookPages& pages, const SheetSide& side,
                    const QSizeF& paper, const QRectF& printable, const SheetOptions& options) {
    if (painter == nullptr || side.empty()) {
        return;
    }
    const QSizeF pageSize = pages.pageSize();
    const std::vector<QRectF> rects =
        pageRects(pageSize, static_cast<int>(side.size()), paper, printable, options);
    for (size_t i = 0; i < rects.size() && i < side.size(); ++i) {
        if (side[i] < 0) {
            continue;  // left empty
        }
        painter->save();
        painter->translate(rects[i].topLeft());
        painter->scale(rects[i].width() / pageSize.width(), rects[i].height() / pageSize.height());
        painter->setClipRect(QRectF(QPointF(0, 0), pageSize), Qt::IntersectClip);
        pages.paint(painter, side[i]);
        painter->restore();
    }
    if (options.layout == SheetLayout::OnePage && !options.fit && options.cropMarks &&
        !rects.empty()) {
        paintCropMarks(painter, rects.front());
    }
}

bool printSheetSides(QPagedPaintDevice* device, const BookPages& pages,
                     const std::vector<SheetSide>& sides, const SheetOptions& options,
                     const std::function<bool(int printed)>& progress) {
    if (device == nullptr) {
        return false;
    }
    QPageLayout layout = device->pageLayout();
    layout.setUnits(QPageLayout::Point);
    const QRectF paper = layout.fullRect(QPageLayout::Point);
    const QRectF printable =
        QRectF(QPointF(0, 0), paper.size()).marginsRemoved(layout.minimumMargins());

    QPainter painter;
    if (!painter.begin(device)) {
        return false;
    }
    const double scaleX = device->logicalDpiX() / POINTS_PER_INCH;
    const double scaleY = device->logicalDpiY() / POINTS_PER_INCH;
    for (size_t i = 0; i < sides.size(); ++i) {
        if (i > 0 && !device->newPage()) {
            painter.end();
            return false;
        }
        painter.save();
        painter.scale(scaleX, scaleY);
        paintSheetSide(&painter, pages, sides[i], paper.size(), printable, options);
        painter.restore();
        if (progress && !progress(static_cast<int>(i) + 1)) {
            painter.end();
            return false;
        }
    }
    return painter.end();
}

}  // namespace kalahari::editor
