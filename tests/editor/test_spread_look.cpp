// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_spread_look.cpp
/// @brief How a spread lies on its two pages, its caption, and its place among the pages of
///        the book (BookPages): always a left page and the right one after it

#include "test_support/export_samples.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/page_start.h>
#include <kalahari/editor/book_export.h>
#include <kalahari/editor/book_pages.h>
#include <kalahari/editor/spread_look.h>

#include <QImage>
#include <QPainter>

#include <vector>

using Catch::Approx;
using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

/// A spread of the export with the title @p title
ExportItem spreadItem(const QString& id, const QString& title) {
    ExportItem item;
    item.kind = ExportItemKind::Spread;
    item.elementId = id;
    item.title = title;
    item.start = kalahari::core::PageStart::Left;
    return item;
}

}  // namespace

TEST_CASE("Spread: where its picture lies", "[editor][spread]") {
    SpreadPage page;  // 148 x 210 mm, margins 20 / 22 / 18
    SpreadLook look;

    SECTION("Within the margins, the whole picture as large as the area allows") {
        const SpreadGeometry geometry = spreadGeometry(look, page, QSize(4000, 1000));
        CHECK(geometry.area == QRectF(18.0, 20.0, 296.0 - 36.0, 210.0 - 42.0));
        CHECK(geometry.picture.width() == Approx(260.0));
        CHECK(geometry.picture.height() == Approx(65.0));
        CHECK(geometry.picture.center().y() == Approx(geometry.area.center().y()));
        // 4000 pixels over 260 mm: 391 dpi
        CHECK(spreadDpi(geometry, QSize(4000, 1000)) == 391);
        CHECK(spreadDpi(spreadGeometry(look, page, QSize()), QSize()) == 0);
    }

    SECTION("To the edges of the paper, filling them: larger by its zoom, moved by its crop") {
        look.toEdges = true;
        look.fills = true;
        const SpreadGeometry geometry = spreadGeometry(look, page, QSize(3000, 3000));
        CHECK(geometry.area == QRectF(0.0, 0.0, 296.0, 210.0));
        CHECK(geometry.picture.width() == Approx(296.0));
        CHECK(geometry.picture.height() == Approx(296.0));
        CHECK(geometry.picture.top() == Approx(-43.0));

        // Dragged down by 10 mm: towards its top edge; it moves only as far as it is larger
        const QPointF moved = spreadCropMovedBy(geometry, 0.0, 0.0, QPointF(5.0, 10.0));
        CHECK(moved.x() == 0.0);
        CHECK(moved.y() == Approx(-20.0 / 86.0));
        look.cropY = -1.0;
        CHECK(spreadGeometry(look, page, QSize(3000, 3000)).picture.top() == Approx(0.0));
        CHECK(spreadCropMovedBy(geometry, 0.0, -1.0, QPointF(0.0, 500.0)).y() == -1.0);

        look.zoom = 200;
        look.cropY = 0.0;
        CHECK(spreadGeometry(look, page, QSize(3000, 3000)).picture.width() ==
              Approx(592.0));
    }

    SECTION("Turned by a right angle, its sides swap") {
        CHECK(turnedSize(QSize(300, 200), 90) == QSize(200, 300));
        CHECK(turnedSize(QSize(300, 200), 180) == QSize(300, 200));
        CHECK(turnedSize(QSize(300, 200), -90) == QSize(200, 300));
    }
}

TEST_CASE("Spread: its caption, numbered with the captions of the book", "[editor][spread]") {
    CaptionContext context;
    CHECK(spreadCaptionText(QString(), QStringLiteral(" The  route "), context) ==
          QStringLiteral("The route"));
    CHECK(spreadCaptionText(QStringLiteral("Map"), QStringLiteral("The route"), context) ==
          QStringLiteral("Map 1. The route"));
    BookCaption before;
    before.label = QStringLiteral("Map");
    context.before = {before, before};
    CHECK(spreadCaptionText(QStringLiteral("Map"), QString(), context) ==
          QStringLiteral("Map 3."));

    // In a file that shows a page at a time: the picture and its caption, with its number
    SpreadLook look;
    look.picture = QStringLiteral("spread-e1/map.png");
    const QString kml = spreadKml(look, QStringLiteral("Map"), QStringLiteral("Map"),
                                  QStringLiteral("The route"), QStringLiteral("spread-e1"));
    CHECK(kml.contains(QStringLiteral("<img src=\"spread-e1/map.png\"")));
    CHECK(kml.contains(QStringLiteral("<caption-number label=\"Map\" id=\"spread-e1\"/>")));
    CHECK(kml.contains(QStringLiteral(". The route</p>")));
}

TEST_CASE("Spread: always a left page and the right one after it", "[editor][spread]") {
    SECTION("Without mirror margins too, a page before it left empty when needed") {
        BookExport book =
            bookOf({textItem(QStringLiteral("e1"), QStringLiteral("One"), longKml(1)),
                    spreadItem(QStringLiteral("s1"), QStringLiteral("The Route")),
                    textItem(QStringLiteral("e2"), QStringLiteral("Two"), longKml(1)),
                    spreadItem(QStringLiteral("s2"), QStringLiteral("The Tree"))});
        book.page.mirrored = false;
        book.textsOnNewPage = false;
        BookPages pages(book, BookPages::Options{});
        pages.layOut();
        // One (page 1, right), The Route (2-3), Two (4), empty (5), The Tree (6-7)
        REQUIRE(pages.pageCount() == 7);
        CHECK(pages.pageAt(QStringLiteral("s1"), 0) == 1);
        CHECK(pages.pageAt(QStringLiteral("e2"), 0) == 3);
        CHECK(pages.isBlank(4));
        CHECK(pages.pageAt(QStringLiteral("s2"), 0) == 5);
        REQUIRE(pages.spreads().size() == 2);
        CHECK(pages.spreads()[0].elementId == QStringLiteral("s1"));
        CHECK(pages.spreads()[0].page == 1);
        CHECK(pages.spreads()[1].page == 5);
        // Its title is a heading of the book only when it is in the table of contents
        CHECK(pages.headings().size() == 4);
    }

    SECTION("The first item of the book: a page empty before it") {
        BookExport book = bookOf({spreadItem(QStringLiteral("s1"), QStringLiteral("Map"))});
        book.items[0].inContents = false;
        BookPages pages(book, BookPages::Options{});
        pages.layOut();
        REQUIRE(pages.pageCount() == 3);
        CHECK(pages.isBlank(0));
        CHECK(pages.pageAt(QStringLiteral("s1"), 0) == 1);
        CHECK(pages.headings().empty());

        // Its pages are drawn: the paper, without a picture
        QImage image(pages.pageSize().toSize(), QImage::Format_ARGB32);
        image.fill(Qt::black);
        QPainter painter(&image);
        pages.paint(&painter, 1);
        painter.end();
        CHECK(image.pixelColor(image.width() / 2, image.height() / 2) == QColor(Qt::white));
    }
}

TEST_CASE("Spread: a text in a file that shows a page at a time", "[editor][spread]") {
    ExportItem spread = spreadItem(QStringLiteral("s1"), QStringLiteral("The Route"));
    spread.spread.picture = QStringLiteral("spread-s1/route.png");
    spread.spreadCaption = true;
    spread.spreadLabel = QStringLiteral("Map");
    spread.spreadText = QStringLiteral("Through the dunes");
    ExportItem hidden = spreadItem(QStringLiteral("s2"), QStringLiteral("Not Listed"));
    hidden.inContents = false;
    BookExport book = bookOf({spread, hidden});
    spreadsAsTexts(book);
    REQUIRE(book.items.size() == 2);
    CHECK(book.items[0].kind == ExportItemKind::Text);
    CHECK(book.items[0].title == QStringLiteral("The Route"));
    CHECK(book.items[0].kml.contains(QStringLiteral("route.png")));
    CHECK(book.items[0].kml.contains(QStringLiteral("Through the dunes")));
    CHECK(book.items[1].kind == ExportItemKind::Text);
    CHECK(book.items[1].title.isEmpty());
    CHECK(spreadCaptionId(QStringLiteral("s1")) == QStringLiteral("spread-s1"));
}
