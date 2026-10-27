/// @file test_tables.cpp
/// @brief Tables in the text: the model, KML, the layout of the cells, the keys

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/kml_document_model.h>
#include <kalahari/editor/kml_serializer.h>
#include <kalahari/editor/text_tables.h>
#include "editor_test_utils.h"

#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextTable>
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
