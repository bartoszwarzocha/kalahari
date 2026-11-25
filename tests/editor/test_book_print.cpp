// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_book_print.cpp
/// @brief The print of a book: the range of the pages, their parity, the sides of the sheets
///        of each layout (a booklet too), where the pages go on a sheet, and the sides drawn
///        on a picture and on a PDF writer

#include "test_support/export_samples.h"

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_pages.h>
#include <kalahari/editor/book_print.h>
#include <kalahari/gui/pdf_document.h>

#include <QBuffer>
#include <QImage>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QRegularExpression>
#include <QTemporaryDir>

#include <cmath>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

/// The pages from 0 of the numbers @p numbers, as the pages show them
std::vector<int> pagesNumbered(std::initializer_list<int> numbers) {
    std::vector<int> pages;
    for (const int number : numbers) {
        pages.push_back(number - 1);
    }
    return pages;
}

/// Whether two rectangles are the same to a hundredth of a point
bool sameRect(const QRectF& a, const QRectF& b) {
    constexpr double tolerance = 0.01;
    return std::abs(a.left() - b.left()) < tolerance && std::abs(a.top() - b.top()) < tolerance &&
           std::abs(a.width() - b.width()) < tolerance &&
           std::abs(a.height() - b.height()) < tolerance;
}

/// Whether a part of a picture has anything drawn on its white
bool drawnIn(const QImage& image, const QRect& part) {
    for (int y = part.top(); y <= part.bottom(); ++y) {
        for (int x = part.left(); x <= part.right(); ++x) {
            if (image.pixel(x, y) != qRgb(255, 255, 255)) {
                return true;
            }
        }
    }
    return false;
}

/// The sizes of the papers, in points
const QSizeF A4(595.0, 842.0);
const QSizeF A4_SIDEWAYS(842.0, 595.0);
const QSizeF A5(420.0, 595.0);

}  // namespace

TEST_CASE("Print: the range of the pages, as the pages show their numbers", "[print]") {
    CHECK(pagesOfRange(QStringLiteral("3-5, 9"), 10) == pagesNumbered({3, 4, 5, 9}));
    CHECK(pagesOfRange(QStringLiteral("3–5; 9"), 10) == pagesNumbered({3, 4, 5, 9}));
    CHECK(pagesOfRange(QStringLiteral("9 3 - 5"), 10) == pagesNumbered({3, 4, 5, 9}));
    // In order, each once; a range backwards is the same range
    CHECK(pagesOfRange(QStringLiteral("5-3, 4, 1"), 10) == pagesNumbered({1, 3, 4, 5}));
    // From the first page, to the last one
    CHECK(pagesOfRange(QStringLiteral("-2"), 10) == pagesNumbered({1, 2}));
    CHECK(pagesOfRange(QStringLiteral("8-"), 10) == pagesNumbered({8, 9, 10}));
    // Pages beyond the book: none, or as many as the book has
    CHECK(pagesOfRange(QStringLiteral("12"), 10) == std::vector<int>{});
    CHECK(pagesOfRange(QStringLiteral("0"), 10) == std::vector<int>{});
    CHECK(pagesOfRange(QStringLiteral("99999999999"), 10) == std::vector<int>{});
    CHECK(pagesOfRange(QStringLiteral("9-99999999999"), 10) == pagesNumbered({9, 10}));
    // Not a range
    CHECK_FALSE(pagesOfRange(QString(), 10).has_value());
    CHECK_FALSE(pagesOfRange(QStringLiteral(" , "), 10).has_value());
    CHECK_FALSE(pagesOfRange(QStringLiteral("abc"), 10).has_value());
    CHECK_FALSE(pagesOfRange(QStringLiteral("3-5-7"), 10).has_value());
    CHECK_FALSE(pagesOfRange(QStringLiteral("-"), 10).has_value());
    CHECK_FALSE(pagesOfRange(QStringLiteral("3.5"), 10).has_value());
}

TEST_CASE("Print: the odd and the even pages, as the pages show their numbers", "[print]") {
    const std::vector<int> pages = pagesNumbered({1, 2, 3, 4, 5, 6});
    CHECK(pagesOfParity(pages, PageParity::All) == pages);
    CHECK(pagesOfParity(pages, PageParity::Odd) == pagesNumbered({1, 3, 5}));
    CHECK(pagesOfParity(pages, PageParity::Even) == pagesNumbered({2, 4, 6}));
}

TEST_CASE("Print: the sides of the sheets of each layout", "[print]") {
    SECTION("A page on a side") {
        CHECK(sheetSides({0, 1, 2}, SheetLayout::OnePage) ==
              std::vector<SheetSide>{{0}, {1}, {2}});
        CHECK(sheetCount(3, false) == 3);
        CHECK(sheetCount(3, true) == 2);
    }

    SECTION("Two pages side by side as an open book: page 1 alone on the right") {
        CHECK(sheetSides({0, 1, 2, 3, 4}, SheetLayout::TwoPages) ==
              std::vector<SheetSide>{{-1, 0}, {1, 2}, {3, 4}});
        CHECK(sheetSides({0, 1, 2, 3}, SheetLayout::TwoPages) ==
              std::vector<SheetSide>{{-1, 0}, {1, 2}, {3, -1}});
        // A range: a right page without its left one on the right, a left page without its
        // right one on the left
        CHECK(sheetSides({2, 3, 4, 5}, SheetLayout::TwoPages) ==
              std::vector<SheetSide>{{-1, 2}, {3, 4}, {5, -1}});
        // Pages that are not neighbours lie alone, each on its own side
        CHECK(sheetSides({0, 2, 4}, SheetLayout::TwoPages) ==
              std::vector<SheetSide>{{-1, 0}, {-1, 2}, {-1, 4}});
        CHECK(sheetSides({1, 3}, SheetLayout::TwoPages) ==
              std::vector<SheetSide>{{1, -1}, {3, -1}});
        CHECK(sheetSides({}, SheetLayout::TwoPages).empty());
    }

    SECTION("A booklet: folded in half, the stack reads in order") {
        // Two sheets of four pages: the outer sheet has the first two and the last two pages
        CHECK(sheetSides({0, 1, 2, 3, 4, 5, 6, 7}, SheetLayout::Booklet) ==
              std::vector<SheetSide>{{7, 0}, {1, 6}, {5, 2}, {3, 4}});
        // Five pages: as many as two sheets have, the last three empty
        CHECK(sheetSides({0, 1, 2, 3, 4}, SheetLayout::Booklet) ==
              std::vector<SheetSide>{{-1, 0}, {1, -1}, {-1, 2}, {3, 4}});
        // The pages chosen, not the pages of the book
        CHECK(sheetSides({10, 11}, SheetLayout::Booklet) ==
              std::vector<SheetSide>{{-1, 10}, {11, -1}});
        CHECK(sheetSides({}, SheetLayout::Booklet).empty());
    }
}

TEST_CASE("Print: the copies, and the fronts and the backs printed by hand", "[print]") {
    const std::vector<SheetSide> three{{0}, {1}, {2}};

    SECTION("On one side: the copies whole, or the copies of each page together") {
        CHECK(copiesOf(three, 2, true, false) ==
              std::vector<SheetSide>{{0}, {1}, {2}, {0}, {1}, {2}});
        CHECK(copiesOf(three, 2, false, false) ==
              std::vector<SheetSide>{{0}, {0}, {1}, {1}, {2}, {2}});
        CHECK(copiesOf(three, 1, true, false) == three);
    }

    SECTION("On both sides: a copy starts on a sheet of its own") {
        CHECK(copiesOf(three, 2, true, true) ==
              std::vector<SheetSide>{{0}, {1}, {2}, {}, {0}, {1}, {2}, {}});
        // The copies of each sheet together: its front and its back
        CHECK(copiesOf(three, 2, false, true) ==
              std::vector<SheetSide>{{0}, {1}, {0}, {1}, {2}, {}, {2}, {}});
        CHECK(copiesOf({}, 3, true, true).empty());
        // One copy: no empty back after the last front
        CHECK(copiesOf(three, 1, true, true) == three);
    }

    SECTION("By hand: the fronts, then the backs, the last one empty") {
        CHECK(sidesOfSheets(three, false) == std::vector<SheetSide>{{0}, {2}});
        CHECK(sidesOfSheets(three, true) == std::vector<SheetSide>{{1}, {}});
        const std::vector<SheetSide> booklet{{7, 0}, {1, 6}, {5, 2}, {3, 4}};
        CHECK(sidesOfSheets(booklet, false) == std::vector<SheetSide>{{7, 0}, {5, 2}});
        CHECK(sidesOfSheets(booklet, true) == std::vector<SheetSide>{{1, 6}, {3, 4}});
    }

    SECTION("In the reverse order: the last sheet first, each with its front and its back") {
        CHECK(reversedSheets(three, false) == std::vector<SheetSide>{{2}, {1}, {0}});
        // The last sheet of an odd number of sides: its back empty, so the next sheet's
        // front does not print on it
        CHECK(reversedSheets(three, true) == std::vector<SheetSide>{{2}, {}, {0}, {1}});
        const std::vector<SheetSide> booklet{{7, 0}, {1, 6}, {5, 2}, {3, 4}};
        CHECK(reversedSheets(booklet, true) ==
              std::vector<SheetSide>{{5, 2}, {3, 4}, {7, 0}, {1, 6}});
        CHECK(reversedSheets({}, true).empty());
        // The copies of the reversed sheets, and by hand their fronts and their backs
        CHECK(copiesOf(reversedSheets(three, true), 2, true, true) ==
              std::vector<SheetSide>{{2}, {}, {0}, {1}, {2}, {}, {0}, {1}});
        CHECK(sidesOfSheets(reversedSheets(three, true), false) ==
              std::vector<SheetSide>{{2}, {0}});
        CHECK(sidesOfSheets(reversedSheets(three, true), true) ==
              std::vector<SheetSide>{{}, {1}});
    }
}

TEST_CASE("Print: the sheet lies sideways for two pages side by side", "[print]") {
    const QSizeF a5(420.0, 595.0);
    CHECK_FALSE(sheetSideways(SheetOptions{SheetLayout::OnePage, true, false}, a5));
    CHECK(sheetSideways(SheetOptions{SheetLayout::OnePage, true, false}, a5.transposed()));
    CHECK(sheetSideways(SheetOptions{SheetLayout::TwoPages, true, false}, a5));
    CHECK(sheetSideways(SheetOptions{SheetLayout::Booklet, true, false}, a5));
}

TEST_CASE("Print: where the pages go on a sheet", "[print]") {
    const QRectF wholeA4(QPointF(0, 0), A4);
    SECTION("A page fit to the sheet, or of its own size in its middle") {
        // A5 fit to A4: as high as the sheet, which is a little narrower
        const std::vector<QRectF> fit = pageRects(A5, 1, A4, wholeA4, SheetOptions{});
        REQUIRE(fit.size() == 1);
        const double scale = 842.0 / 595.0;
        CHECK(sameRect(fit[0], QRectF((595.0 - 420.0 * scale) / 2.0, 0, 420.0 * scale, 842.0)));
        const std::vector<QRectF> actual =
            pageRects(A5, 1, A4, wholeA4, SheetOptions{SheetLayout::OnePage, false, true});
        REQUIRE(actual.size() == 1);
        CHECK(sameRect(actual[0], QRectF(87.5, 123.5, 420.0, 595.0)));
        // The printable area makes a fit page smaller, in its middle
        const std::vector<QRectF> inside =
            pageRects(A4, 1, A4, QRectF(20, 20, 555, 802), SheetOptions{});
        REQUIRE(inside.size() == 1);
        const double height = 842.0 * 555.0 / 595.0;
        CHECK(sameRect(inside[0], QRectF(20, 20 + (802.0 - height) / 2.0, 555.0, height)));
    }

    SECTION("Two pages: their spine in the middle of the sheet, where a booklet folds") {
        const SheetOptions two{SheetLayout::TwoPages, true, false};
        const std::vector<QRectF> whole =
            pageRects(A5, 2, A4_SIDEWAYS, QRectF(QPointF(0, 0), A4_SIDEWAYS), two);
        REQUIRE(whole.size() == 2);
        CHECK(sameRect(whole[0], QRectF(1.0, 0, 420.0, 595.0)));
        CHECK(sameRect(whole[1], QRectF(421.0, 0, 420.0, 595.0)));
        // Margins of the printer that are not the same on both sides: the spine stays
        const SheetOptions booklet{SheetLayout::Booklet, true, false};
        const std::vector<QRectF> margins =
            pageRects(A5, 2, A4_SIDEWAYS, QRectF(30, 10, 802, 575), booklet);
        REQUIRE(margins.size() == 2);
        const double scale = 391.0 / 420.0;
        const QSizeF size(420.0 * scale, 595.0 * scale);
        CHECK(sameRect(margins[0], QRectF(QPointF(421.0 - size.width(),
                                                  10 + (575.0 - size.height()) / 2.0),
                                          size)));
        CHECK(margins[0].right() == margins[1].left());
        CHECK(margins[1].left() == 421.0);
    }
}

TEST_CASE("Print: the sides drawn on a picture and printed on a PDF writer", "[print]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    BookExport book = bookOf({textItem(QStringLiteral("e1"), QStringLiteral("Jeden"), longKml(3)),
                              textItem(QStringLiteral("e2"), QStringLiteral("Dwa"), longKml(3)),
                              textItem(QStringLiteral("e3"), QStringLiteral("Trzy"), longKml(3))});
    BookPages pages(book, BookPages::Options{false});
    pages.layOut();
    REQUIRE(pages.pageCount() == 3);

    SECTION("A booklet's side: an empty page at the left, the first page at the right") {
        // A sheet of A4 sideways, a pixel a point
        QImage image(A4_SIDEWAYS.toSize(), QImage::Format_RGB32);
        image.fill(Qt::white);
        {
            QPainter painter(&image);
            paintSheetSide(&painter, pages, {-1, 0}, A4_SIDEWAYS,
                           QRectF(QPointF(0, 0), A4_SIDEWAYS),
                           SheetOptions{SheetLayout::Booklet, true, false});
        }
        CHECK_FALSE(drawnIn(image, QRect(0, 0, 420, 595)));
        CHECK(drawnIn(image, QRect(422, 0, 420, 595)));
    }

    SECTION("A page of its own size with the marks of its corners") {
        QImage image(QSize(842, 1191), QImage::Format_RGB32);  // A3: an A4 page in its middle
        image.fill(Qt::white);
        {
            QPainter painter(&image);
            paintSheetSide(&painter, pages, {0}, QSizeF(842, 1191), QRectF(0, 0, 842, 1191),
                           SheetOptions{SheetLayout::OnePage, false, true});
        }
        // The page from (123.5, 174.5); the marks beside its top left corner, apart from it
        CHECK(drawnIn(image, QRect(100, 174, 10, 1)));
        CHECK(drawnIn(image, QRect(123, 150, 1, 10)));
        CHECK_FALSE(drawnIn(image, QRect(118, 169, 4, 4)));
        // Without the marks: nothing there
        image.fill(Qt::white);
        {
            QPainter painter(&image);
            paintSheetSide(&painter, pages, {0}, QSizeF(842, 1191), QRectF(0, 0, 842, 1191),
                           SheetOptions{SheetLayout::OnePage, false, false});
        }
        CHECK_FALSE(drawnIn(image, QRect(100, 150, 20, 30)));
    }

    SECTION("A booklet on a PDF writer: a side a page, the sheet sideways") {
        QByteArray data;
        QBuffer buffer(&data);
        REQUIRE(buffer.open(QIODevice::WriteOnly));
        QPdfWriter writer(&buffer);
        writer.setResolution(300);
        QPageLayout layout(QPageSize(QPageSize::A4), QPageLayout::Landscape, QMarginsF());
        layout.setMode(QPageLayout::FullPageMode);
        REQUIRE(writer.setPageLayout(layout));
        const std::vector<SheetSide> sides =
            sheetSides({0, 1, 2}, SheetLayout::Booklet);  // a sheet: two sides
        std::vector<int> printed;
        REQUIRE(printSheetSides(&writer, pages, sides,
                                SheetOptions{SheetLayout::Booklet, true, false},
                                [&printed](int done) {
                                    printed.push_back(done);
                                    return true;
                                }));
        buffer.close();
        CHECK(printed == std::vector<int>{1, 2});
        static const QRegularExpression page(QStringLiteral("/Type /Page\\b(?!s)"));
        CHECK(QString::fromLatin1(data).count(page) == 2);
        static const QRegularExpression sideways(
            QStringLiteral("/MediaBox \\[0 0 842(\\.0+)? 595(\\.0+)?\\]"));
        CHECK(QString::fromLatin1(data).contains(sideways));

        // Stopped after the first side
        QByteArray stopped;
        QBuffer stoppedBuffer(&stopped);
        REQUIRE(stoppedBuffer.open(QIODevice::WriteOnly));
        QPdfWriter second(&stoppedBuffer);
        REQUIRE(second.setPageLayout(layout));
        CHECK_FALSE(printSheetSides(&second, pages, sides,
                                    SheetOptions{SheetLayout::Booklet, true, false},
                                    [](int) { return false; }));
    }
}

TEST_CASE("Print: the marks of a scene break as large as the text", "[print]") {
    // A printer or a PDF of more dots than the screen's resolves a font set on its painter
    // at its own resolution: the marks came out several times as large as the text
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    BookExport book = bookOf({textItem(
        QStringLiteral("t"), QStringLiteral("Chapter 1"),
        QStringLiteral("<kml><p>Before the break, the wind moved the sand.</p><scene-break/>"
                       "<p>After the break, the caravan waited.</p></kml>"))});
    BookPages pages(book, BookPages::Options{false});
    pages.layOut();
    REQUIRE(pages.pageCount() == 1);

    const QString path = dir.filePath(QStringLiteral("print.pdf"));
    {
        QPdfWriter writer(path);
        writer.setResolution(600);
        QPageLayout layout(QPageSize(QPageSize::A4), QPageLayout::Portrait, QMarginsF());
        layout.setMode(QPageLayout::FullPageMode);
        REQUIRE(writer.setPageLayout(layout));
        REQUIRE(printSheetSides(&writer, pages, {SheetSide{0}}, SheetOptions{}));
    }
    kalahari::gui::PdfDocument pdf;
    REQUIRE(pdf.load(path) == kalahari::gui::PdfDocument::Error::None);
    const QList<kalahari::gui::PdfHit> text = pdf.find(0, QStringLiteral("Before the break"));
    const QList<kalahari::gui::PdfHit> marks = pdf.find(0, QStringLiteral("*"));
    REQUIRE(text.size() == 1);
    REQUIRE(marks.size() == 3);
    const double textHeight = text.front().rectangles.value(0).height();
    REQUIRE(textHeight > 0.0);
    for (const kalahari::gui::PdfHit& mark : marks) {
        // In the text's font
        CHECK(mark.rectangles.value(0).height() < textHeight * 1.2);
    }
}
