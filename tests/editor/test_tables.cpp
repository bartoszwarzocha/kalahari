/// @file test_tables.cpp
/// @brief Tables in the text: the model, KML, the layout of the cells, the keys, the clipboard

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/editor/kml_document_model.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/kml_serializer.h>
#include <kalahari/editor/search_engine.h>
#include <kalahari/editor/spell_check_service.h>
#include <kalahari/editor/text_source_adapter.h>
#include <kalahari/editor/text_tables.h>
#include "editor_test_utils.h"

#include <QCoreApplication>
#include <QImage>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QTextTable>
#include <cmath>
#include <memory>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

std::unique_ptr<BookEditor> editorWith(const QString& content) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(600, 400));
    editor->fromKml(content);
    return editor;
}

/// The KML of the editor's text after reading @p kml
QString roundTrip(const QString& kml) {
    return editorWith(kml)->toKml();
}

/// The texts of the document's paragraphs, a cell's as "row,column:text"
QStringList paragraphsOf(const QTextDocument& document) {
    QStringList texts;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        if (const QTextTable* table = tableOf(block)) {
            const QTextTableCell cell = table->cellAt(block.position());
            texts << QStringLiteral("%1,%2:%3").arg(cell.row()).arg(cell.column()).arg(block.text());
        } else {
            texts << block.text();
        }
    }
    return texts;
}

const QString TABLE_KML = QStringLiteral(
    "<kml><p>Before</p>"
    "<table><col width=\"40%\"/><col width=\"60%\"/>"
    "<tr><th><p>Who</p></th><th><p>Role</p></th></tr>"
    "<tr><td><p>Anna</p></td><td><p>Narrator</p><p>Second</p></td></tr>"
    "<tr><td colspan=\"2\"><p>Both</p></td></tr>"
    "</table><p>After</p></kml>");

}  // anonymous namespace

// =============================================================================
// KML
// =============================================================================

TEST_CASE("Tables: KML reads a table into the model", "[editor][tables][kml]") {
    KmlDocumentModel model;
    REQUIRE(model.loadKml(TABLE_KML));
    REQUIRE(model.tableCount() == 1);
    const TableShape shape = model.table(0);
    CHECK(shape.rows == 3);
    CHECK(shape.columns == 2);
    CHECK(shape.headerRows == 1);
    CHECK(shape.columnWidths == std::vector<double>{40.0, 60.0});
    CHECK(shape.spans == std::vector<TableSpan>{{2, 0, 1, 2}});

    REQUIRE(model.paragraphCount() == 8);
    CHECK_FALSE(model.paragraphCell(0).has_value());
    CHECK(model.paragraphCell(1) == TableCellPlace{0, 0, 0});
    CHECK(model.paragraphCell(4) == TableCellPlace{0, 1, 1});
    CHECK(model.paragraphCell(5) == TableCellPlace{0, 1, 1});
    CHECK(model.paragraphText(5) == QStringLiteral("Second"));
    CHECK(model.paragraphCell(6) == TableCellPlace{0, 2, 0});
    CHECK_FALSE(model.paragraphCell(7).has_value());
}

TEST_CASE("Tables: a table of KML in the editor's document, and written back", "[editor][tables][kml]") {
    auto editor = editorWith(TABLE_KML);
    const QTextDocument& document = *editor->textDocument();
    CHECK(paragraphsOf(document).join(QLatin1Char('|')).toStdString() ==
          "Before|0,0:Who|0,1:Role|1,0:Anna|1,1:Narrator|1,1:Second|2,0:Both|After");
    const QTextTable* table = tableOf(document.findBlockByNumber(1));
    REQUIRE(table != nullptr);
    CHECK(table->format().headerRowCount() == 1);
    CHECK(table->cellAt(2, 0).columnSpan() == 2);
    CHECK(editor->toKml().toStdString() == TABLE_KML.toStdString());
}

TEST_CASE("Tables: cells spanning rows, short rows, empty cells", "[editor][tables][kml]") {
    SECTION("rowspan: the cells under it go to the next free column") {
        const QString kml = QStringLiteral(
            "<kml><p>A</p><table>"
            "<tr><td rowspan=\"2\"><p>Tall</p></td><td><p>B1</p></td></tr>"
            "<tr><td><p>B2</p></td></tr>"
            "</table><p>Z</p></kml>");
        auto editor = editorWith(kml);
        CHECK(paragraphsOf(*editor->textDocument()).join(QLatin1Char('|')).toStdString() ==
              "A|0,0:Tall|0,1:B1|1,1:B2|Z");
        CHECK(editor->toKml().toStdString() == kml.toStdString());
    }
    SECTION("a short row gets empty cells, empty cells an empty paragraph") {
        CHECK(roundTrip(QStringLiteral("<kml><p>A</p><table><tr><td>x</td><td/><td><p/></td></tr>"
                                       "<tr><td><p>y</p></td></tr></table><p>Z</p></kml>"))
                  .toStdString() ==
              "<kml><p>A</p><table><tr><td><p>x</p></td><td><p></p></td><td><p></p></td></tr>"
              "<tr><td><p>y</p></td><td><p></p></td><td><p></p></td></tr></table><p>Z</p></kml>");
    }
    SECTION("a header only of a first row of header cells") {
        CHECK(roundTrip(QStringLiteral("<kml><p>A</p><table><tr><th><p>h</p></th><td><p>d</p></td></tr>"
                                       "</table><p>Z</p></kml>"))
                  .toStdString() ==
              "<kml><p>A</p><table><tr><td><p>h</p></td><td><p>d</p></td></tr></table><p>Z</p></kml>");
    }
    SECTION("widths in another unit, and missing ones") {
        KmlDocumentModel model;
        REQUIRE(model.loadKml(QStringLiteral(
            "<kml><table><col width=\"100px\"/><col width=\"300px\"/><tr><td/><td/></tr></table></kml>")));
        CHECK(model.table(0).columnWidths == std::vector<double>{25.0, 75.0});
        REQUIRE(model.loadKml(QStringLiteral(
            "<kml><table><col width=\"20%\"/><tr><td/><td/></tr></table></kml>")));
        CHECK(model.table(0).columnWidths == std::vector<double>{50.0, 50.0});
        REQUIRE(model.loadKml(QStringLiteral(
            "<kml><table><col width=\"auto\"/><col width=\"20%\"/><tr><td/><td/></tr></table></kml>")));
        CHECK(model.table(0).columnWidths.empty());
    }
    SECTION("a table without cells is left out") {
        KmlDocumentModel model;
        REQUIRE(model.loadKml(QStringLiteral("<kml><p>A</p><table><tr></tr></table><p>B</p></kml>")));
        CHECK(model.tableCount() == 0);
        CHECK(model.paragraphCount() == 2);
    }
}

TEST_CASE("Tables: a table at the start and at the end of a chapter", "[editor][tables][kml]") {
    auto editor = editorWith(QStringLiteral("<kml><table><tr><td><p>x</p></td></tr></table></kml>"));
    // Qt keeps a paragraph before and after the table
    CHECK(paragraphsOf(*editor->textDocument()).join(QLatin1Char('|')).toStdString() == "|0,0:x|");
    const QString written = editor->toKml();
    CHECK(written.toStdString() ==
          "<kml><p></p><table><tr><td><p>x</p></td></tr></table><p></p></kml>");
    CHECK(roundTrip(written).toStdString() == written.toStdString());
    CHECK_FALSE(editor->isModified());
}

TEST_CASE("Tables: paragraphs, lists, links and annotations in cells", "[editor][tables][kml]") {
    const QString kml = QStringLiteral(
        "<kml><annotations><annotation id=\"n1\" kind=\"comment\">Check</annotation></annotations>"
        "<p>A</p><table><tr>"
        "<td><h2>Title</h2><p align=\"center\">Centered</p></td>"
        "<td><ul><li>one</li><li>two</li></ul></td>"
        "<td><p>See <a href=\"https://example.com\">this</a> <anchor ref=\"n1\">here</anchor></p></td>"
        "</tr></table><p>Z</p></kml>");
    KmlDocumentModel model;
    REQUIRE(model.loadKml(kml));
    REQUIRE(model.paragraphCount() == 7);
    CHECK(model.paragraphCell(3) == TableCellPlace{0, 0, 1});
    CHECK(model.paragraphList(3).has_value());
    CHECK(model.paragraphCell(4) == TableCellPlace{0, 0, 1});

    const QString written = roundTrip(kml);
    CHECK(written.toStdString() ==
          "<kml><annotations><annotation id=\"n1\" kind=\"comment\">Check</annotation></annotations>"
          "<p>A</p><table><tr>"
          "<td><h2>Title</h2><p align=\"center\">Centered</p></td>"
          "<td><ul><li>one</li><li>two</li></ul></td>"
          "<td><p>See <a href=\"https://example.com\">this</a> <anchor ref=\"n1\">here</anchor></p></td>"
          "</tr></table><p>Z</p></kml>");
}

TEST_CASE("Tables: text outside paragraphs in a cell, a table inside a cell", "[editor][tables][kml]") {
    CHECK(roundTrip(QStringLiteral("<kml><p>A</p><table><tr><td>\n  loose <b>text</b>\n  <p>p</p>\n</td>"
                                   "<td><table><tr><td><p>in1</p></td><td><p>in2</p></td></tr></table></td>"
                                   "</tr></table><p>Z</p></kml>"))
              .toStdString() ==
          "<kml><p>A</p><table><tr><td><p>loose <b>text</b></p><p>p</p></td>"
          "<td><p>in1</p><p>in2</p></td></tr></table><p>Z</p></kml>");
    // A cell holds no line or break
    CHECK(roundTrip(QStringLiteral("<kml><p>A</p><table><tr><td><p>x</p><hr/><scene-break/>"
                                   "<page-break/><p>y</p></td></tr></table><p>Z</p></kml>"))
              .toStdString() ==
          "<kml><p>A</p><table><tr><td><p>x</p><p>y</p></td></tr></table><p>Z</p></kml>");
}

TEST_CASE("Tables: KML of a part of the text with a table", "[editor][tables][kml]") {
    auto editor = editorWith(TABLE_KML);
    const QTextDocument& document = *editor->textDocument();
    const QTextTable* table = tableOf(document.findBlockByNumber(1));
    REQUIRE(table != nullptr);
    KmlSerializer serializer;

    SECTION("inside one cell: the text of its paragraphs") {
        const QTextTableCell cell = table->cellAt(1, 1);
        CHECK(serializer.toKml(&document, cell.firstPosition() + 2, cell.lastPosition() - 2).toStdString() ==
              "<kml><p>rrator</p><p>Seco</p></kml>");
    }
    SECTION("over cells: the rows it touches, the cells cut to it") {
        const int from = table->cellAt(0, 1).firstPosition() + 1;
        const int to = table->cellAt(1, 0).lastPosition() - 1;
        CHECK(serializer.toKml(&document, from, to).toStdString() ==
              "<kml><table><col width=\"40%\"/><col width=\"60%\"/>"
              "<tr><th><p></p></th><th><p>ole</p></th></tr>"
              "<tr><td><p>Ann</p></td><td><p></p></td></tr></table></kml>");
    }
    SECTION("from the text before the table into it") {
        const int to = table->cellAt(0, 0).lastPosition();
        CHECK(serializer.toKml(&document, 3, to).toStdString() ==
              "<kml><p>ore</p><table><col width=\"40%\"/><col width=\"60%\"/>"
              "<tr><th><p>Who</p></th><th><p></p></th></tr></table></kml>");
    }
    SECTION("from the table into the text after it") {
        const int from = table->cellAt(2, 0).firstPosition();
        const int to = document.lastBlock().position() + 2;
        CHECK(serializer.toKml(&document, from, to).toStdString() ==
              "<kml><table><col width=\"40%\"/><col width=\"60%\"/>"
              "<tr><td colspan=\"2\"><p>Both</p></td></tr></table><p>Af</p></kml>");
    }
}

// =============================================================================
// Layout
// =============================================================================

namespace {

/// A document with the editor's layout, a paragraph before and after a table
struct TableDocument {
    std::unique_ptr<QTextDocument> doc = std::make_unique<QTextDocument>();
    KalahariTextDocumentLayout* layout = nullptr;
    QTextTable* table = nullptr;

    explicit TableDocument(const TableShape& shape, const QStringList& cells = {}) {
        layout = new KalahariTextDocumentLayout(doc.get());
        doc->setDocumentLayout(layout);
        QFont font(QStringLiteral("DejaVu Sans"));
        font.setPixelSize(16);
        doc->setDefaultFont(font);
        doc->setDocumentMargin(0);
        doc->setTextWidth(600);

        QTextCursor cursor(doc.get());
        cursor.insertText(QStringLiteral("Before"));
        cursor.insertBlock();
        cursor.insertText(QStringLiteral("After"));
        cursor.setPosition(6);
        table = insertTable(cursor, shape);
        spanCells(*table, shape.spans);
        // The texts of the cells, row by row
        int index = 0;
        for (int row = 0; row < shape.rows; ++row) {
            for (int column = 0; column < shape.columns; ++column) {
                const QTextTableCell cell = table->cellAt(row, column);
                if (cell.row() != row || cell.column() != column) {
                    continue;
                }
                if (index < cells.size()) {
                    cell.firstCursorPosition().insertText(cells[index]);
                }
                ++index;
            }
        }
        layout->layoutPendingBlocks();
    }

    int number(int row, int column) const {
        return doc->findBlock(table->cellAt(row, column).firstPosition()).blockNumber();
    }
    QTextBlock block(int number) const { return doc->findBlockByNumber(number); }
};

const QString LONG_TEXT = QStringLiteral(
    "A cell with enough words to wrap onto several lines in half of the width of the text.");

}  // anonymous namespace

TEST_CASE("Tables: the cells of a row side by side, the row as high as its highest cell",
          "[editor][tables][layout]") {
    TableDocument d(TableShape{2, 2, {}, 0, {}},
                    {QStringLiteral("A"), LONG_TEXT, QStringLiteral("C"), QStringLiteral("D")});
    KalahariTextDocumentLayout& layout = *d.layout;
    const std::vector<qreal> edges = layout.tableColumnEdges(*d.table);
    REQUIRE(edges == std::vector<qreal>{0.0, 300.0, 600.0});
    const qreal padding = layout.cellPadding();

    // Row 0: blocks 1 and 2 at the row's top; the row's height on its last block
    CHECK(d.number(0, 0) == 1);
    CHECK(d.number(0, 1) == 2);
    CHECK(layout.blockY(1) == layout.blockY(2));
    CHECK(layout.blockY(1) == layout.blockY(0) + layout.blockHeight(0));
    CHECK(layout.blockHeight(1) == 0.0);
    const QTextLayout* longCell = d.block(2).layout();
    REQUIRE(longCell->lineCount() >= 2);
    const qreal longBottom = longCell->lineAt(longCell->lineCount() - 1).y() +
                             longCell->lineAt(longCell->lineCount() - 1).height();
    CHECK(layout.blockHeight(2) >= longBottom + padding);
    CHECK(layout.blockY(3) == layout.blockY(1) + layout.blockHeight(2));

    // The lines in their cells
    const QTextLine first = d.block(1).layout()->lineAt(0);
    CHECK(first.x() >= padding);
    CHECK(first.y() >= padding);
    for (int i = 0; i < longCell->lineCount(); ++i) {
        CHECK(longCell->lineAt(i).x() >= 300.0 + padding);
        CHECK(longCell->lineAt(i).x() + longCell->lineAt(i).width() <= 600.0 - padding + 0.5);
    }

    // The rows as the grid draws them
    const std::vector<KalahariTextDocumentLayout::TableRow> rows = layout.tableRows(*d.table);
    REQUIRE(rows.size() == 2);
    CHECK(rows[0].top == layout.blockY(1));
    CHECK(rows[0].height == layout.blockHeight(2));
    CHECK(rows[1].top == rows[0].top + rows[0].height);
    CHECK(layout.blockY(5) == rows[1].top + rows[1].height);

    // The blocks of a row, from either end
    CHECK(layout.firstBlockOfRow(2) == 1);
    CHECK(layout.lastBlockOfRow(1) == 2);
    CHECK(layout.firstBlockOfRow(5) == 5);
    CHECK(layout.blockNumberAtY(rows[0].top + 1.0) == 2);
}

TEST_CASE("Tables: a row grows with the text typed in a cell", "[editor][tables][layout]") {
    TableDocument d(TableShape{2, 2, {50.0, 50.0}, 0, {}});
    KalahariTextDocumentLayout& layout = *d.layout;
    const qreal before = layout.tableRows(*d.table)[1].height;
    const qreal afterTable = layout.blockY(6);

    QTextCursor cursor = d.table->cellAt(1, 0).firstCursorPosition();
    cursor.insertText(LONG_TEXT);
    const qreal grown = layout.tableRows(*d.table)[1].height;
    CHECK(grown > before);
    CHECK(layout.blockY(6) == afterTable + (grown - before));

    // A second paragraph in the cell goes below the first
    cursor.insertBlock();
    cursor.insertText(QStringLiteral("Second"));
    const int second = cursor.blockNumber();
    REQUIRE(tableOf(d.block(second)) == d.table);
    const QTextLayout* lines = d.block(second).layout();
    const QTextLayout* above = d.block(second - 1).layout();
    CHECK(lines->lineAt(0).y() >= above->lineAt(above->lineCount() - 1).y() +
                                       above->lineAt(above->lineCount() - 1).height());
    CHECK(layout.tableRows(*d.table)[1].height > grown);
}

TEST_CASE("Tables: cells spanning columns and rows", "[editor][tables][layout]") {
    TableDocument d(TableShape{2, 3, {}, 0, {{0, 0, 2, 1}, {1, 1, 1, 2}}},
                    {LONG_TEXT, QStringLiteral("B"), QStringLiteral("C"), QStringLiteral("Wide")});
    KalahariTextDocumentLayout& layout = *d.layout;
    const std::vector<qreal> edges = layout.tableColumnEdges(*d.table);
    const std::vector<KalahariTextDocumentLayout::TableRow> rows = layout.tableRows(*d.table);
    REQUIRE(rows.size() == 2);

    // The tall cell: the two rows together hold its lines
    const QTextLayout* tall = d.block(d.number(0, 0)).layout();
    const QTextLine last = tall->lineAt(tall->lineCount() - 1);
    CHECK(rows[0].height + rows[1].height >= last.y() + last.height());

    // The wide cell: its lines on the width of two columns
    const QTextLine wide = d.block(d.number(1, 1)).layout()->lineAt(0);
    CHECK(wide.x() >= edges[1]);
    CHECK(wide.width() > edges[2] - edges[1]);
}

TEST_CASE("Tables: a point in a row is in the cell under it", "[editor][tables][layout]") {
    TableDocument d(TableShape{2, 2, {}, 0, {}},
                    {QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C"), QStringLiteral("D")});
    KalahariTextDocumentLayout& layout = *d.layout;
    // A second paragraph in cell (0, 1)
    QTextCursor cursor = d.table->cellAt(0, 1).lastCursorPosition();
    cursor.insertBlock();
    cursor.insertText(QStringLiteral("B2"));
    const std::vector<KalahariTextDocumentLayout::TableRow> rows = layout.tableRows(*d.table);
    const qreal padding = layout.cellPadding();

    CHECK(layout.blockNumberAt(QPointF(10, rows[0].top + padding + 1)) == d.number(0, 0));
    CHECK(layout.blockNumberAt(QPointF(310, rows[0].top + padding + 1)) == d.number(0, 1));
    CHECK(layout.blockNumberAt(QPointF(310, rows[0].top + rows[0].height - padding - 1)) ==
          d.number(0, 1) + 1);
    CHECK(layout.blockNumberAt(QPointF(10, rows[1].top + 2)) == d.number(1, 0));
    CHECK(layout.blockNumberAt(QPointF(590, rows[1].top + 2)) == d.number(1, 1));
    // Outside the table, by y
    CHECK(layout.blockNumberAt(QPointF(310, 1)) == 0);

    // The position under a point
    const int hit = layout.hitTest(QPointF(320, rows[1].top + padding + 2), Qt::FuzzyHit);
    CHECK(d.doc->findBlock(hit).blockNumber() == d.number(1, 1));
}

TEST_CASE("Tables: the header row in bold", "[editor][tables][layout]") {
    TableDocument d(TableShape{2, 1, {}, 1, {}}, {QStringLiteral("Head"), QStringLiteral("Body")});
    const QList<QTextLayout::FormatRange> header = d.block(d.number(0, 0)).layout()->formats();
    REQUIRE_FALSE(header.isEmpty());
    CHECK(header.first().format.fontWeight() == QFont::Bold);
    CHECK(d.block(d.number(1, 0)).layout()->formats().isEmpty());
}

TEST_CASE("Tables: in the page view a row that does not fit goes to the next page",
          "[editor][tables][layout]") {
    QStringList cells;
    for (int i = 0; i < 24; ++i) {
        cells << (i % 2 == 0 ? QStringLiteral("Row %1").arg(i / 2) : LONG_TEXT);
    }
    TableDocument d(TableShape{12, 2, {}, 0, {}}, cells);
    KalahariTextDocumentLayout& layout = *d.layout;
    constexpr qreal pitch = 400.0;
    constexpr qreal textHeight = 300.0;
    layout.setPageFlow(PageFlow{true, pitch, textHeight});
    layout.layoutPendingBlocks();

    const std::vector<KalahariTextDocumentLayout::TableRow> rows = layout.tableRows(*d.table);
    bool nextPage = false;
    for (const KalahariTextDocumentLayout::TableRow& row : rows) {
        const qreal offset = std::fmod(row.top, pitch);
        CHECK((offset < 0.01 || offset + row.height <= textHeight + 0.01));
        nextPage |= row.top >= pitch;
    }
    CHECK(nextPage);
    CHECK(layout.pageCount() >= 2);
}

// =============================================================================
// In the editor: the grid, the marks, a click
// =============================================================================

namespace {

/// The caret's rectangle at a place
QRect caretAt(BookEditor& editor, const CursorPosition& position) {
    editor.setCursorPosition(position);
    return editor.inputMethodQuery(Qt::ImCursorRectangle).toRect();
}

/// A mouse press and release at a point
void click(BookEditor& editor, const QPointF& point) {
    QMouseEvent pressed(QEvent::MouseButtonPress, point, editor.mapToGlobal(point),
                        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&editor, &pressed);
    QMouseEvent released(QEvent::MouseButtonRelease, point, editor.mapToGlobal(point),
                         Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&editor, &released);
}

}  // anonymous namespace

TEST_CASE("Tables: the cells of a row on one line in the editor, a click in a cell",
          "[editor][tables][view]") {
    auto editor = editorWith(TABLE_KML);
    // Row 1: "Anna" (paragraph 3) and "Narrator" (4) side by side
    const QRect anna = caretAt(*editor, {3, 0});
    const QRect narrator = caretAt(*editor, {4, 0});
    CHECK(anna.top() == narrator.top());
    CHECK(narrator.left() > anna.left() + 100);
    const QRect second = caretAt(*editor, {5, 0});
    CHECK(second.left() == narrator.left());
    CHECK(second.top() > narrator.top());

    // A click in a cell puts the cursor there
    editor->setCursorPosition({0, 0});
    click(*editor, QPointF(narrator.center()) + QPointF(1.0, 0.0));
    CHECK(editor->cursorPosition().paragraph == 4);
    click(*editor, QPointF(anna.center()) + QPointF(1.0, 0.0));
    CHECK(editor->cursorPosition().paragraph == 3);
    click(*editor, QPointF(second.center()) + QPointF(1.0, 0.0));
    CHECK(editor->cursorPosition().paragraph == 5);
}

TEST_CASE("Tables: the grid of a table as the view draws it", "[editor][tables][view]") {
    auto editor = editorWith(TABLE_KML);
    QTextDocumentSource source(editor->textDocument());
    const std::vector<TableGrid> grids =
        source.tablesIn(0, static_cast<size_t>(editor->textDocument()->blockCount() - 1));
    REQUIRE(grids.size() == 1);
    const TableGrid& grid = grids.front();
    REQUIRE(grid.columnEdges.size() == 3);
    const double width = editor->textDocument()->textWidth();
    CHECK(grid.columnEdges[1] == std::round(width * 0.4));
    CHECK(grid.columnEdges[2] == std::round(width));
    REQUIRE(grid.rowTops.size() == 3);
    CHECK(grid.rowTops[1] == grid.rowTops[0] + grid.rowHeights[0]);
    CHECK(grid.headerRows == 1);
    REQUIRE(grid.cells.size() == 5);
    CHECK(grid.cells[4].row == 2);
    CHECK(grid.cells[4].columns == 2);

    // The whole view is drawn with the table
    const QImage image = editor->grab().toImage();
    CHECK_FALSE(image.isNull());
}

TEST_CASE("Tables: the end of a cell has ¤ for a formatting mark", "[editor][tables][view]") {
    auto editor = editorWith(TABLE_KML);
    const QTextDocument& document = *editor->textDocument();
    const auto lastMark = [&document](int number) {
        const QTextBlock block = document.findBlockByNumber(number);
        KalahariTextDocumentLayout::blockLayout(block);
        const std::vector<FormattingMark> marks = formattingMarksOf(block);
        return marks.empty() ? QChar() : marks.back().glyph;
    };
    CHECK(lastMark(0) == QChar(PARAGRAPH_MARK));
    CHECK(lastMark(1) == QChar(CELL_MARK));
    CHECK(lastMark(4) == QChar(PARAGRAPH_MARK));  // "Narrator", a second paragraph follows
    CHECK(lastMark(5) == QChar(CELL_MARK));
    CHECK(lastMark(7) == QChar(PARAGRAPH_MARK));
}


// =============================================================================
// Keys: Tab, the arrows, Backspace and Delete, selections over a table
// =============================================================================

namespace {

void press(BookEditor& editor, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QCoreApplication::sendEvent(&editor, &event);
}

/// Type the text as the keyboard does, a character at a time
void type(BookEditor& editor, const QString& text) {
    for (const QChar ch : text) {
        QKeyEvent event(QEvent::KeyPress, 0, Qt::NoModifier, QString(ch));
        QCoreApplication::sendEvent(&editor, &event);
    }
}

/// Select from @p from to @p to, the cursor at @p to
void select(BookEditor& editor, CursorPosition from, CursorPosition to) {
    editor.setCursorPosition(to);
    editor.setSelection({from, to});
}

/// The paragraphs of the editor's text, as paragraphsOf() gives them, in one line
std::string textOf(const BookEditor& editor) {
    return paragraphsOf(*editor.textDocument()).join(QLatin1Char('|')).toStdString();
}

const std::string TABLE_TEXT = "Before|0,0:Who|0,1:Role|1,0:Anna|1,1:Narrator|1,1:Second|2,0:Both|After";

}  // anonymous namespace

TEST_CASE("Tables: Tab and Shift+Tab go from cell to cell", "[editor][tables][keys]") {
    auto editor = editorWith(TABLE_KML);
    editor->setCursorPosition({1, 2});
    press(*editor, Qt::Key_Tab);
    CHECK(editor->cursorPosition() == CursorPosition{2, 0});
    press(*editor, Qt::Key_Tab);
    CHECK(editor->cursorPosition() == CursorPosition{3, 0});
    editor->setCursorPosition({5, 3});  // the second paragraph of a cell
    press(*editor, Qt::Key_Tab);
    CHECK(editor->cursorPosition() == CursorPosition{6, 0});

    // Back: to the start of the cell's first paragraph
    press(*editor, Qt::Key_Backtab, Qt::ShiftModifier);
    CHECK(editor->cursorPosition() == CursorPosition{4, 0});
    editor->setCursorPosition({1, 2});
    press(*editor, Qt::Key_Backtab, Qt::ShiftModifier);
    CHECK(editor->cursorPosition() == CursorPosition{1, 2});  // the first cell has none before

    // A selection goes with the cursor
    select(*editor, {3, 0}, {3, 2});
    press(*editor, Qt::Key_Tab);
    CHECK_FALSE(editor->hasSelection());
    CHECK(editor->cursorPosition() == CursorPosition{4, 0});
    CHECK(textOf(*editor) == TABLE_TEXT);
}

TEST_CASE("Tables: Tab in the last cell adds a row, one undo step", "[editor][tables][keys]") {
    auto editor = editorWith(TABLE_KML);
    const QTextTable* table = tableOf(editor->textDocument()->findBlockByNumber(1));
    REQUIRE(table != nullptr);
    editor->setCursorPosition({6, 4});
    press(*editor, Qt::Key_Tab);
    CHECK(table->rows() == 4);
    CHECK(textOf(*editor) == TABLE_TEXT.substr(0, TABLE_TEXT.size() - 6) + "|3,0:|3,1:|After");
    CHECK(editor->cursorPosition() == CursorPosition{7, 0});
    type(*editor, QStringLiteral("x"));
    CHECK(textOf(*editor).find("3,0:x|3,1:|After") != std::string::npos);

    editor->undo();  // the typing
    editor->undo();  // the row
    CHECK(table->rows() == 3);
    CHECK(editor->toKml().toStdString() == TABLE_KML.toStdString());

    SECTION("not in a read-only text") {
        editor->setReadOnly(true);
        editor->setCursorPosition({6, 4});
        press(*editor, Qt::Key_Tab);
        CHECK(table->rows() == 3);
        CHECK(editor->cursorPosition() == CursorPosition{6, 4});
    }
}

TEST_CASE("Tables: Up and Down go to the cell above and below", "[editor][tables][keys]") {
    auto editor = editorWith(TABLE_KML);
    editor->setCursorPosition({0, 0});
    const auto down = [&editor] {
        editor->moveCursorDown();
        return editor->cursorPosition().paragraph;
    };
    const auto up = [&editor] {
        editor->moveCursorUp();
        return editor->cursorPosition().paragraph;
    };
    CHECK(down() == 1);  // Who
    CHECK(down() == 3);  // Anna, not Role beside it
    CHECK(down() == 6);  // Both, spanning both columns
    CHECK(down() == 7);  // After
    CHECK(up() == 6);
    CHECK(up() == 3);
    CHECK(up() == 1);
    CHECK(up() == 0);

    // In the second column, the place across the rows kept
    editor->setCursorPosition({2, 0});
    CHECK(down() == 4);  // Narrator
    CHECK(down() == 5);  // Second, in the same cell
    CHECK(down() == 6);
    CHECK(down() == 7);
    CHECK(up() == 6);
    CHECK(up() == 5);  // the last paragraph of the cell in the column of the place
    CHECK(up() == 4);
    CHECK(up() == 2);
}

TEST_CASE("Tables: Backspace and Delete at the edges of cells", "[editor][tables][keys]") {
    auto editor = editorWith(TABLE_KML);
    const auto check = [&editor](CursorPosition at, int key, CursorPosition after) {
        editor->setCursorPosition(at);
        press(*editor, key);
        CHECK(editor->cursorPosition() == after);
        CHECK(textOf(*editor) == TABLE_TEXT);
    };
    // Cells do not join
    check({1, 0}, Qt::Key_Backspace, {1, 0});
    check({1, 3}, Qt::Key_Delete, {1, 3});
    check({2, 0}, Qt::Key_Backspace, {2, 0});
    check({6, 4}, Qt::Key_Delete, {6, 4});
    // The paragraphs next to the table do not join it: the cursor goes into the table
    check({0, 6}, Qt::Key_Delete, {1, 0});
    check({7, 0}, Qt::Key_Backspace, {6, 4});
    CHECK_FALSE(editor->textDocument()->isUndoAvailable());

    // Ctrl+Backspace and Ctrl+Delete take words, not the table
    editor->setCursorPosition({7, 0});
    press(*editor, Qt::Key_Backspace, Qt::ControlModifier);
    CHECK(textOf(*editor) == TABLE_TEXT);
    editor->setCursorPosition({0, 6});
    press(*editor, Qt::Key_Delete, Qt::ControlModifier);
    CHECK(textOf(*editor) == TABLE_TEXT);

    // Paragraphs in a cell join as anywhere
    editor->setCursorPosition({5, 0});
    press(*editor, Qt::Key_Backspace);
    CHECK(editor->cursorPosition() == CursorPosition{4, 8});
    CHECK(textOf(*editor) == "Before|0,0:Who|0,1:Role|1,0:Anna|1,1:NarratorSecond|2,0:Both|After");
}

TEST_CASE("Tables: an empty paragraph next to a table", "[editor][tables][keys]") {
    const QString table = QStringLiteral("<table><tr><td><p>a</p></td><td><p>b</p></td></tr></table>");
    const auto kml = [](const QString& content) {
        return QStringLiteral("<kml>") + content + QStringLiteral("</kml>");
    };

    SECTION("after it, Backspace takes it; the paragraph after it keeps its style") {
        auto editor = editorWith(kml(table + QStringLiteral("<p></p><h2>After</h2>")));
        editor->setCursorPosition({3, 0});
        press(*editor, Qt::Key_Backspace);
        CHECK(editor->toKml().toStdString() == kml(QStringLiteral("<p></p>") + table +
                                                   QStringLiteral("<h2>After</h2>")).toStdString());
        CHECK(editor->cursorPosition() == CursorPosition{2, 1});
        editor->undo();
        CHECK(textOf(*editor) == "|0,0:a|0,1:b||After");
    }

    SECTION("before it, Delete takes it") {
        auto editor = editorWith(kml(QStringLiteral("<p>Before</p><p></p>") + table));
        editor->setCursorPosition({1, 0});
        press(*editor, Qt::Key_Delete);
        CHECK(editor->toKml().toStdString() ==
              kml(QStringLiteral("<p>Before</p>") + table + QStringLiteral("<p></p>")).toStdString());
        CHECK(editor->cursorPosition() == CursorPosition{1, 0});
    }

    SECTION("the table needs it at the start and the end of the text, and between tables") {
        auto editor = editorWith(kml(table + table));
        const std::string text = textOf(*editor);
        CHECK(text == "|0,0:a|0,1:b||0,0:a|0,1:b|");
        editor->setCursorPosition({0, 0});
        press(*editor, Qt::Key_Delete);
        CHECK(editor->cursorPosition() == CursorPosition{1, 0});
        editor->setCursorPosition({3, 0});
        press(*editor, Qt::Key_Backspace);
        CHECK(editor->cursorPosition() == CursorPosition{2, 1});
        editor->setCursorPosition({3, 0});
        press(*editor, Qt::Key_Delete);
        CHECK(editor->cursorPosition() == CursorPosition{4, 0});
        editor->setCursorPosition({6, 0});
        press(*editor, Qt::Key_Backspace);
        CHECK(editor->cursorPosition() == CursorPosition{5, 1});
        CHECK(textOf(*editor) == text);
    }

    SECTION("an object before a table goes, the table stays") {
        auto editor = editorWith(kml(QStringLiteral("<p>x</p><hr/>") + table));
        editor->setCursorPosition({1, 1});
        press(*editor, Qt::Key_Backspace);
        CHECK(editor->toKml().toStdString() ==
              kml(QStringLiteral("<p>x</p>") + table + QStringLiteral("<p></p>")).toStdString());
    }
}

TEST_CASE("Tables: a selection going into or out of a table", "[editor][tables][keys]") {
    auto editor = editorWith(TABLE_KML);

    SECTION("from the text before it: the text goes, the cells lose their text") {
        select(*editor, {0, 2}, {3, 2});
        press(*editor, Qt::Key_Delete);
        CHECK(textOf(*editor) == "Be|0,0:|0,1:|1,0:na|1,1:Narrator|1,1:Second|2,0:Both|After");
        CHECK(editor->cursorPosition() == CursorPosition{0, 2});
        editor->undo();
        CHECK(editor->toKml().toStdString() == TABLE_KML.toStdString());
    }

    SECTION("into the text after it") {
        select(*editor, {4, 2}, {7, 2});
        press(*editor, Qt::Key_Backspace);
        CHECK(textOf(*editor) == "Before|0,0:Who|0,1:Role|1,0:Anna|1,1:Na|2,0:|ter");
        CHECK(editor->cursorPosition() == CursorPosition{4, 2});
    }

    SECTION("from cell to cell") {
        select(*editor, {3, 2}, {4, 2});
        press(*editor, Qt::Key_Delete);
        CHECK(textOf(*editor) == "Before|0,0:Who|0,1:Role|1,0:An|1,1:rrator|1,1:Second|2,0:Both|After");
    }

    SECTION("text typed in its place goes at its start") {
        select(*editor, {0, 2}, {3, 2});
        type(*editor, QStringLiteral("x"));
        CHECK(textOf(*editor) == "Bex|0,0:|0,1:|1,0:na|1,1:Narrator|1,1:Second|2,0:Both|After");
        CHECK(editor->cursorPosition() == CursorPosition{0, 3});
    }

    SECTION("over the whole table: the table goes with it") {
        select(*editor, {0, 2}, {7, 2});
        press(*editor, Qt::Key_Delete);
        CHECK(textOf(*editor) == "Beter");
    }
}

TEST_CASE("Tables: a cell holds only text; Enter starts a paragraph in it", "[editor][tables][keys]") {
    auto editor = editorWith(TABLE_KML);
    editor->setCursorPosition({0, 1});
    CHECK_FALSE(editor->isInTable());
    editor->setCursorPosition({3, 2});
    CHECK(editor->isInTable());
    select(*editor, {0, 1}, {3, 2});
    CHECK_FALSE(editor->isInTable());  // inserted, it goes at the start of the selection

    editor->clearSelection();
    editor->setCursorPosition({3, 2});
    editor->insertTextObject(TextObject::HorizontalRule);
    CHECK(textOf(*editor) == TABLE_TEXT);

    press(*editor, Qt::Key_Return);
    CHECK(textOf(*editor) ==
          "Before|0,0:Who|0,1:Role|1,0:An|1,0:na|1,1:Narrator|1,1:Second|2,0:Both|After");
    CHECK(editor->cursorPosition() == CursorPosition{4, 0});
}

// =============================================================================
// Insert > Table
// =============================================================================

TEST_CASE("Tables: Insert > Table puts a table at the cursor, one undo step", "[editor][tables][insert]") {
    const auto kml = [](const QString& content) {
        return QStringLiteral("<kml>") + content + QStringLiteral("</kml>");
    };
    const QString row = QStringLiteral("<tr><td><p></p></td><td><p></p></td></tr>");

    SECTION("in an empty paragraph it takes its place") {
        const QString text = kml(QStringLiteral("<p>One</p><p></p><p>Two</p>"));
        auto editor = editorWith(text);
        editor->setCursorPosition({1, 0});
        editor->insertTable(2, 2, false);
        CHECK(textOf(*editor) == "One|0,0:|0,1:|1,0:|1,1:|Two");
        CHECK(editor->cursorPosition() == CursorPosition{1, 0});
        CHECK(editor->isModified());
        editor->undo();
        CHECK(editor->toKml().toStdString() == text.toStdString());
    }

    SECTION("in the middle of a paragraph it splits it, both parts in its style") {
        auto editor = editorWith(kml(QStringLiteral("<h2>Ab</h2>")));
        editor->setCursorPosition({0, 1});
        editor->insertTable(2, 1, false);
        CHECK(editor->toKml().toStdString() ==
              kml(QStringLiteral("<h2>A</h2><table>") + row + QStringLiteral("</table><h2>b</h2>"))
                  .toStdString());
    }

    SECTION("at the end of the text an empty paragraph stays after it") {
        auto editor = editorWith(kml(QStringLiteral("<p>One</p>")));
        editor->setCursorPosition({0, 3});
        editor->insertTable(2, 1, true);
        CHECK(textOf(*editor) == "One|0,0:|0,1:|");
        const QTextTable* table = tableOf(editor->textDocument()->findBlockByNumber(1));
        REQUIRE(table != nullptr);
        CHECK(table->format().headerRowCount() == 1);
    }

    SECTION("in place of the selection") {
        auto editor = editorWith(kml(QStringLiteral("<p>One two</p>")));
        select(*editor, {0, 3}, {0, 7});
        editor->insertTable(2, 1, false);
        CHECK(textOf(*editor) == "One|0,0:|0,1:|");
    }

    SECTION("not in a table, not in a read-only text") {
        auto editor = editorWith(TABLE_KML);
        editor->setCursorPosition({3, 1});
        editor->insertTable(2, 2, false);
        CHECK(textOf(*editor) == TABLE_TEXT);
        editor->setReadOnly(true);
        editor->setCursorPosition({0, 1});
        editor->insertTable(2, 2, false);
        CHECK(textOf(*editor) == TABLE_TEXT);
    }
}

// =============================================================================
// The clipboard
// =============================================================================

namespace {

/// The KML Kalahari puts on the clipboard
std::string kmlOf(const QMimeData& data) {
    return QString::fromUtf8(data.data(QString::fromLatin1(MIME_KML))).toStdString();
}

/// Kalahari's own content of the clipboard
std::unique_ptr<QMimeData> kmlData(const QString& kml) {
    auto data = std::make_unique<QMimeData>();
    data->setData(QString::fromLatin1(MIME_KML), kml.toUtf8());
    return data;
}

}  // anonymous namespace

TEST_CASE("Tables: copied, a part of a table is the rows from the start's cell to the end's",
          "[editor][tables][clipboard]") {
    auto editor = editorWith(TABLE_KML);
    const auto copied = [&editor](CursorPosition from, CursorPosition to) {
        select(*editor, from, to);
        std::unique_ptr<QMimeData> data = editor->createMimeDataFromSelection();
        REQUIRE(data);
        return data;
    };

    SECTION("from cell to cell: the selected text, the other cells of its rows empty") {
        const std::unique_ptr<QMimeData> data = copied({4, 2}, {6, 2});
        CHECK(kmlOf(*data) == "<kml><table><col width=\"40%\"/><col width=\"60%\"/>"
                              "<tr><td><p></p></td><td><p>rrator</p><p>Second</p></td></tr>"
                              "<tr><td colspan=\"2\"><p>Bo</p></td></tr></table></kml>");
        CHECK(data->text().toStdString() == "\trrator Second\nBo");
    }

    SECTION("from the text before it into a cell") {
        const std::unique_ptr<QMimeData> data = copied({0, 3}, {3, 2});
        CHECK(kmlOf(*data) == "<kml><p>ore</p><table><col width=\"40%\"/><col width=\"60%\"/>"
                              "<tr><th><p>Who</p></th><th><p>Role</p></th></tr>"
                              "<tr><td><p>An</p></td><td><p></p></td></tr></table></kml>");
        CHECK(data->text().toStdString() == "ore\nWho\tRole\nAn\t");
    }

    SECTION("inside one cell: its text") {
        const std::unique_ptr<QMimeData> data = copied({4, 2}, {5, 3});
        CHECK(kmlOf(*data) == "<kml><p>rrator</p><p>Sec</p></kml>");
        CHECK(data->text().toStdString() == "rrator\nSec");
    }
}

TEST_CASE("Tables: other programs get a table of HTML and its text with tabs",
          "[editor][tables][clipboard]") {
    auto editor = editorWith(TABLE_KML);
    editor->selectAll();
    const std::unique_ptr<QMimeData> data = editor->createMimeDataFromSelection();
    REQUIRE(data);
    // The cells of a row separated by tabs, the paragraphs of a cell by spaces
    CHECK(data->text().toStdString() == "Before\nWho\tRole\nAnna\tNarrator Second\nBoth\nAfter");
    // As wide as the text with a thin grid, the header row in a thead, the widths on the
    // cells of the first row
    CHECK(data->html().toStdString() ==
          ClipboardHandler::htmlDocument(QStringLiteral(
              "<p>Before</p>"
              "<table style=\"border-collapse:collapse\" border=\"1\" cellspacing=\"0\" "
              "cellpadding=\"4\" width=\"100%\">"
              "<thead><tr><th width=\"40%\" valign=\"top\"><p>Who</p></th>"
              "<th width=\"60%\" valign=\"top\"><p>Role</p></th></tr></thead>"
              "<tr><td valign=\"top\"><p>Anna</p></td>"
              "<td valign=\"top\"><p>Narrator</p><p>Second</p></td></tr>"
              "<tr><td colspan=\"2\" valign=\"top\"><p>Both</p></td></tr></table>"
              "<p>After</p>")).toStdString());
    // Pasted back from the HTML alone, as from another program: the same table
    CHECK(ClipboardHandler::htmlToKml(data->html()).toStdString() == TABLE_KML.toStdString());

    // A row under a cell spanning rows keeps its columns: a tab for the cell above
    CHECK(ClipboardHandler::kmlToText(QStringLiteral(
                                          "<kml><table><tr><td rowspan=\"2\"><p>a</p></td>"
                                          "<td><p>b</p></td></tr><tr><td><p>c</p></td></tr>"
                                          "</table></kml>"))
              .toStdString() == "a\tb\n\tc");
}

TEST_CASE("Tables: a table pasted in the text, and in a cell as its text",
          "[editor][tables][clipboard]") {
    const QString table = QStringLiteral(
        "<table><tr><th><p>a</p></th><th><p>b</p></th></tr>"
        "<tr><td colspan=\"2\"><p>c</p></td></tr></table>");
    const auto kml = [](const QString& content) {
        return QStringLiteral("<kml>") + content + QStringLiteral("</kml>");
    };

    SECTION("in the middle of a paragraph it splits it, both parts in its style; one undo step") {
        const QString text = kml(QStringLiteral("<h2>Ab</h2>"));
        auto editor = editorWith(text);
        editor->setCursorPosition({0, 1});
        editor->insertFromMimeData(kmlData(kml(table)).get());
        const QString expected =
            kml(QStringLiteral("<h2>A</h2>") + table + QStringLiteral("<h2>b</h2>"));
        CHECK(editor->toKml().toStdString() == expected.toStdString());
        CHECK(editor->cursorPosition() == CursorPosition{4, 0});
        editor->undo();
        CHECK(editor->toKml().toStdString() == text.toStdString());
    }

    SECTION("in an empty paragraph it takes its place") {
        auto editor = editorWith(kml(QStringLiteral("<p>One</p><p></p><p>Two</p>")));
        editor->setCursorPosition({1, 0});
        editor->insertFromMimeData(kmlData(kml(table)).get());
        const QString expected =
            kml(QStringLiteral("<p>One</p>") + table + QStringLiteral("<p>Two</p>"));
        CHECK(editor->toKml().toStdString() == expected.toStdString());
        CHECK(editor->cursorPosition() == CursorPosition{4, 0});
    }

    SECTION("a part of a table copied: a smaller table") {
        auto editor = editorWith(TABLE_KML);
        select(*editor, {4, 2}, {6, 2});
        const std::unique_ptr<QMimeData> copied = editor->createMimeDataFromSelection();
        REQUIRE(copied);
        editor->clearSelection();
        editor->setCursorPosition({7, 5});
        editor->insertFromMimeData(copied.get());
        CHECK(textOf(*editor) == TABLE_TEXT + "|0,0:|0,1:rrator|0,1:Second|1,0:Bo|");
    }

    SECTION("in a cell: a table is its rows of text, a line an empty paragraph") {
        auto editor = editorWith(TABLE_KML);
        editor->setCursorPosition({3, 4});
        const QString pasted =
            kml(QStringLiteral("<p>x</p>") + table + QStringLiteral("<hr/><p>y</p>"));
        editor->insertFromMimeData(kmlData(pasted).get());
        CHECK(textOf(*editor) == "Before|0,0:Who|0,1:Role|1,0:Annax|1,0:a\tb|1,0:c|1,0:|1,0:y|"
                                 "1,1:Narrator|1,1:Second|2,0:Both|After");
        editor->undo();
        CHECK(textOf(*editor) == TABLE_TEXT);
    }

    SECTION("in a cell: a row under a cell spanning rows keeps its columns") {
        auto editor = editorWith(TABLE_KML);
        editor->setCursorPosition({6, 4});
        editor->insertFromMimeData(
            kmlData(kml(QStringLiteral("<table><tr><td rowspan=\"2\"><p>a</p></td>"
                                       "<td><p>b</p></td></tr><tr><td><p>c</p></td></tr></table>")))
                .get());
        CHECK(textOf(*editor) == "Before|0,0:Who|0,1:Role|1,0:Anna|1,1:Narrator|1,1:Second|"
                                 "2,0:Botha\tb|2,0:\tc|After");
    }

    SECTION("HTML of another program: a table of the editor, its merged cells kept") {
        auto editor = editorWith(kml(QStringLiteral("<p>One</p>")));
        editor->setCursorPosition({0, 3});
        QMimeData pasted;
        pasted.setText(QStringLiteral("a\nb\tc"));
        pasted.setHtml(QStringLiteral("<table><tr><td colspan=\"2\">a</td></tr>"
                                      "<tr><td>b</td><td><i>c</i></td></tr></table>"));
        editor->insertFromMimeData(&pasted);
        const QString expected =
            kml(QStringLiteral("<p>One</p><table><tr><td colspan=\"2\"><p>a</p></td></tr>"
                               "<tr><td><p>b</p></td><td><p><i>c</i></p></td></tr></table>"
                               "<p></p>"));
        CHECK(editor->toKml().toStdString() == expected.toStdString());
    }
}

// =============================================================================
// The tools of the text in cells
// =============================================================================

TEST_CASE("Tables: search and Replace All find the text of the cells", "[editor][tables][search]") {
    auto editor = editorWith(TABLE_KML);
    SearchEngine engine;
    engine.setDocument(editor->textDocument());
    engine.setSearchText(QStringLiteral("o"));
    // Before, Who, Role, Narrator, Second, Both: each paragraph its own matches
    std::vector<int> paragraphs;
    for (const SearchMatch& match : engine.findAll()) {
        paragraphs.push_back(match.paragraph);
    }
    CHECK(paragraphs == std::vector<int>{0, 1, 2, 4, 5, 6});

    engine.setSearchText(QStringLiteral("Second"));
    engine.setReplaceText(QStringLiteral("Third"));
    CHECK(engine.replaceAll() == 1);
    CHECK(textOf(*editor) ==
          "Before|0,0:Who|0,1:Role|1,0:Anna|1,1:Narrator|1,1:Third|2,0:Both|After");
    editor->undo();
    CHECK(textOf(*editor) == TABLE_TEXT);
}

TEST_CASE("Tables: the words of the cells count", "[editor][tables][statistics]") {
    auto editor = editorWith(TABLE_KML);
    CHECK(editor->wordCount() == 8);
    CHECK(editor->textCounts().paragraphs == 8);
}

TEST_CASE("Tables: a misspelled word in a cell gets its wave", "[editor][tables][spelling]") {
    auto spelling = std::make_unique<SpellCheckService>();
    REQUIRE(spelling->loadDictionary(QStringLiteral("en_US")));
    auto editor = editorWith(QStringLiteral(
        "<kml><p>Before</p><table><tr><td><p>Anna</p></td><td><p>The narator</p></td></tr>"
        "</table><p>After</p></kml>"));
    editor->setSpellCheckService(spelling.get());
    editor->show();
    REQUIRE(waitUntil([&editor]() { return !editor->isSpellCheckPending(); }, DICTIONARY_WAIT_MS));

    // The highlights of the paragraph of the second cell
    QStringList wavy;
    const QString text = editor->textDocument()->findBlockByNumber(2).text();
    for (const TextHighlight& highlight :
         QTextDocumentSource(editor->textDocument()).paragraphHighlights(2)) {
        if (highlight.kind == HighlightKind::Spelling) {
            wavy.append(text.mid(highlight.start, highlight.length));
        }
    }
    CHECK(wavy == QStringList{QStringLiteral("narator")});
}
