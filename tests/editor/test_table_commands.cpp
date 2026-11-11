/// @file test_table_commands.cpp
/// @brief The Table menu in the text: the properties of tables in KML, the commands, sorting,
///        moving rows, the conversions, the properties of a table, the columns' edges, and
///        the header and the rows on the pages

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/table_editing.h>
#include <kalahari/editor/text_keys.h>
#include <kalahari/editor/text_sorting.h>
#include <kalahari/editor/text_tables.h>
#include "editor_test_utils.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QLocale>
#include <QMimeData>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextTable>
#include <cmath>
#include <memory>

using namespace kalahari::editor;
using namespace kalahari::test;
using Catch::Approx;

namespace {

std::unique_ptr<BookEditor> editorWith(const QString& content) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(600, 400));
    editor->fromKml(content);
    return editor;
}

/// The texts of the document's paragraphs, a cell's as "row,column:text", in one line
std::string textOf(const BookEditor& editor) {
    QStringList texts;
    for (QTextBlock block = editor.textDocument()->begin(); block.isValid(); block = block.next()) {
        if (const QTextTable* table = tableOf(block)) {
            const QTextTableCell cell = table->cellAt(block.position());
            texts << QStringLiteral("%1,%2:%3").arg(cell.row()).arg(cell.column()).arg(block.text());
        } else {
            texts << block.text();
        }
    }
    return texts.join(QLatin1Char('|')).toStdString();
}

/// The KML of the editor's text
std::string kmlOf(const BookEditor& editor) {
    return editor.toKml().toStdString();
}

/// The first table of the text
QTextTable* firstTable(const BookEditor& editor) {
    for (QTextBlock block = editor.textDocument()->begin(); block.isValid(); block = block.next()) {
        if (QTextTable* table = tableOf(block)) {
            return table;
        }
    }
    return nullptr;
}

void select(BookEditor& editor, CursorPosition from, CursorPosition to) {
    editor.setCursorPosition(to);
    editor.setSelection({from, to});
}

void press(BookEditor& editor, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QCoreApplication::sendEvent(&editor, &event);
}

QString kml(const QString& body) {
    return QStringLiteral("<kml>") + body + QStringLiteral("</kml>");
}

/// A table of 3 by 3 cells between two paragraphs: block 1 is a1, 5 is b2, 9 is c3
const QString GRID_KML = kml(QStringLiteral(
    "<p>Before</p><table>"
    "<tr><td><p>a1</p></td><td><p>b1</p></td><td><p>c1</p></td></tr>"
    "<tr><td><p>a2</p></td><td><p>b2</p></td><td><p>c2</p></td></tr>"
    "<tr><td><p>a3</p></td><td><p>b3</p></td><td><p>c3</p></td></tr>"
    "</table><p>After</p>"));
const std::string GRID_TEXT =
    "Before|0,0:a1|0,1:b1|0,2:c1|1,0:a2|1,1:b2|1,2:c2|2,0:a3|2,1:b3|2,2:c3|After";

/// A table with a header row: block 3 is Lucyna, 5 Adam, 7 Łabędź
const QString PEOPLE_KML = kml(QStringLiteral(
    "<p>Before</p><table>"
    "<tr><th><p>Name</p></th><th><p>Age</p></th></tr>"
    "<tr><td><p>Lucyna</p></td><td><p>10</p></td></tr>"
    "<tr><td><p>Adam</p></td><td><p>2</p></td></tr>"
    "<tr><td><p>Łabędź</p></td><td><p>33</p></td></tr>"
    "</table><p>After</p>"));

}  // anonymous namespace

// =============================================================================
// KML
// =============================================================================

TEST_CASE("Table commands: the properties of a table, its rows and cells in KML",
          "[editor][tables][kml]") {
    SECTION("written as they were read") {
        const QString text = kml(QStringLiteral(
            "<p>Before</p>"
            "<table width=\"200pt\" align=\"center\" borders=\"top bottom rows\" "
            "border-style=\"dashed\" border-width=\"1.5pt\" border-color=\"#336699\">"
            "<tr height=\"30pt\" split=\"true\"><th><p>A</p></th>"
            "<th valign=\"middle\" background=\"#ffee00\"><p>B</p></th></tr>"
            "<tr><td valign=\"bottom\"><p>C</p></td><td><p>D</p></td></tr>"
            "</table><p>After</p>"));
        auto editor = editorWith(text);
        CHECK(kmlOf(*editor) == text.toStdString());
        const QTextTable* table = firstTable(*editor);
        REQUIRE(table != nullptr);
        const TableProperties properties = tablePropertiesOf(table->format());
        CHECK(properties.width.value_or(0.0) == Approx(200.0));
        CHECK(properties.alignment == TableAlignment::Center);
        CHECK(properties.borders.lines == (BORDER_TOP | BORDER_BOTTOM | BORDER_ROWS));
        CHECK(properties.borders.style == BorderStyle::Dashed);
        CHECK(properties.borders.width == Approx(1.5));
        CHECK(properties.borders.color == QColor(QStringLiteral("#336699")));
        CHECK(rowPropertiesOf(*table, 0).minHeight == Approx(30.0));
        CHECK(rowPropertiesOf(*table, 0).allowSplit);
        CHECK_FALSE(rowPropertiesOf(*table, 1).allowSplit);
        CHECK(cellPropertiesOf(table->cellAt(0, 1)).alignment == CellAlignment::Middle);
        CHECK(cellPropertiesOf(table->cellAt(0, 1)).background == QColor(QStringLiteral("#ffee00")));
        CHECK(cellPropertiesOf(table->cellAt(1, 0)).alignment == CellAlignment::Bottom);
    }

    SECTION("a table at the left with an indent, two header rows, no borders") {
        const QString text = kml(QStringLiteral(
            "<p>Before</p><table indent=\"36pt\" borders=\"none\">"
            "<tr><th><p>A</p></th></tr><tr><th><p>B</p></th></tr><tr><td><p>C</p></td></tr>"
            "</table><p>After</p>"));
        auto editor = editorWith(text);
        CHECK(kmlOf(*editor) == text.toStdString());
        const QTextTable* table = firstTable(*editor);
        REQUIRE(table != nullptr);
        CHECK(table->format().headerRowCount() == 2);
        CHECK(tablePropertiesOf(table->format()).indent == Approx(36.0));
    }

    SECTION("lengths in other units, and attributes it does not know") {
        CHECK(pointsOfKmlLength(QStringLiteral("2.54cm")).value_or(0.0) == Approx(72.0));
        CHECK(pointsOfKmlLength(QStringLiteral("10mm")).value_or(0.0) == Approx(72.0 / 2.54));
        CHECK(pointsOfKmlLength(QStringLiteral("1in")).value_or(0.0) == Approx(72.0));
        CHECK(pointsOfKmlLength(QStringLiteral("12")).value_or(0.0) == Approx(12.0));
        CHECK_FALSE(pointsOfKmlLength(QStringLiteral("wide")).has_value());
        auto editor = editorWith(kml(QStringLiteral(
            "<p>x</p><table width=\"50%\" align=\"middle\" borders=\"stars\"><tr><td><p>A</p>"
            "</td></tr></table><p>y</p>")));
        const QTextTable* table = firstTable(*editor);
        REQUIRE(table != nullptr);
        const TableProperties properties = tablePropertiesOf(table->format());
        CHECK_FALSE(properties.width.has_value());
        CHECK(properties.alignment == TableAlignment::Left);
        CHECK(properties.borders.lines == BORDER_GRID);
    }
}

// =============================================================================
// Selected cells
// =============================================================================

TEST_CASE("Table commands: a selection from cell to cell selects the cells between",
          "[editor][tables][selection]") {
    auto editor = editorWith(GRID_KML);

    SECTION("a rectangle of whole cells, shown by its text with tabs") {
        select(*editor, {2, 1}, {4, 1});  // b1 to a2
        const std::optional<TableCellRange> cells = editor->selectedCellRange();
        REQUIRE(cells.has_value());
        CHECK(cells->firstRow == 0);
        CHECK(cells->lastRow == 1);
        CHECK(cells->firstColumn == 0);
        CHECK(cells->lastColumn == 1);
        CHECK(editor->selectedText() ==
              QStringLiteral("a1\tb1") + QChar(QChar::ParagraphSeparator) + QStringLiteral("a2\tb2"));
        CHECK(editor->canMergeTableCells());
    }

    SECTION("in one cell: text") {
        select(*editor, {5, 0}, {5, 2});
        CHECK_FALSE(editor->selectedCellRange().has_value());
        CHECK_FALSE(editor->canMergeTableCells());
        CHECK(editor->tableCells().has_value());
    }

    SECTION("Shift with the arrows goes from cell to cell") {
        editor->setCursorPosition({5, 1});
        press(*editor, Qt::Key_Right, Qt::ShiftModifier);
        press(*editor, Qt::Key_Right, Qt::ShiftModifier);
        const std::optional<TableCellRange> cells = editor->selectedCellRange();
        REQUIRE(cells.has_value());
        CHECK(cells->firstColumn == 1);
        CHECK(cells->lastColumn == 2);
        CHECK(cells->firstRow == 1);
        CHECK(cells->lastRow == 1);
    }

    SECTION("typing clears the cells and goes into the first") {
        select(*editor, {5, 1}, {9, 1});  // b2 to c3
        QKeyEvent event(QEvent::KeyPress, 0, Qt::NoModifier, QStringLiteral("x"));
        QCoreApplication::sendEvent(editor.get(), &event);
        CHECK(textOf(*editor) ==
              "Before|0,0:a1|0,1:b1|0,2:c1|1,0:a2|1,1:x|1,2:|2,0:a3|2,1:|2,2:|After");
    }
}

// =============================================================================
// Insert, delete, select
// =============================================================================

TEST_CASE("Table commands: rows and columns inserted, each one step of Undo",
          "[editor][tables][commands]") {
    auto editor = editorWith(GRID_KML);
    editor->setCursorPosition({5, 1});  // b2

    SECTION("a row below") {
        editor->insertTableRows(true);
        CHECK(textOf(*editor) == "Before|0,0:a1|0,1:b1|0,2:c1|1,0:a2|1,1:b2|1,2:c2|"
                                 "2,0:|2,1:|2,2:|3,0:a3|3,1:b3|3,2:c3|After");
        CHECK(editor->cursorPosition() == CursorPosition{5, 1});
        editor->undo();
        CHECK(kmlOf(*editor) == GRID_KML.toStdString());
        CHECK(editor->cursorPosition() == CursorPosition{5, 1});
    }

    SECTION("as many rows above as the cells selected are high") {
        select(*editor, {1, 0}, {5, 1});  // a1 to b2
        editor->insertTableRows(false);
        CHECK(textOf(*editor) == "Before|0,0:|0,1:|0,2:|1,0:|1,1:|1,2:|2,0:a1|2,1:b1|2,2:c1|"
                                 "3,0:a2|3,1:b2|3,2:c2|4,0:a3|4,1:b3|4,2:c3|After");
    }

    SECTION("a column to the right: the table keeps its width") {
        editor->insertTableColumns(true);
        CHECK(textOf(*editor) == "Before|0,0:a1|0,1:b1|0,2:|0,3:c1|1,0:a2|1,1:b2|1,2:|1,3:c2|"
                                 "2,0:a3|2,1:b3|2,2:|2,3:c3|After");
        const std::vector<double> widths = columnWidthsOf(*firstTable(*editor));
        REQUIRE(widths.size() == 4);
        CHECK(widths[0] + widths[1] + widths[2] + widths[3] == Approx(100.0));
        editor->undo();
        CHECK(kmlOf(*editor) == GRID_KML.toStdString());
    }

    SECTION("a column to the left") {
        editor->insertTableColumns(false);
        CHECK(firstTable(*editor)->columns() == 4);
        CHECK(textOf(*editor).find("1,0:a2|1,1:|1,2:b2") != std::string::npos);
    }
}

TEST_CASE("Table commands: rows, columns and the table deleted", "[editor][tables][commands]") {
    auto editor = editorWith(GRID_KML);
    editor->setCursorPosition({5, 1});  // b2

    SECTION("the cursor's row") {
        editor->deleteTableRows();
        CHECK(textOf(*editor) == "Before|0,0:a1|0,1:b1|0,2:c1|1,0:a3|1,1:b3|1,2:c3|After");
        editor->undo();
        CHECK(kmlOf(*editor) == GRID_KML.toStdString());
    }

    SECTION("the cursor's column") {
        editor->deleteTableColumns();
        CHECK(textOf(*editor) == "Before|0,0:a1|0,1:c1|1,0:a2|1,1:c2|2,0:a3|2,1:c3|After");
    }

    SECTION("the table; the cursor goes after it") {
        editor->deleteTable();
        CHECK(textOf(*editor) == "Before|After");
        CHECK(editor->cursorPosition().paragraph == 1);
        editor->undo();
        CHECK(kmlOf(*editor) == GRID_KML.toStdString());
    }

    SECTION("all of its rows: the table") {
        editor->selectTablePart(BookEditor::TablePart::Table);
        editor->deleteTableRows();
        CHECK(textOf(*editor) == "Before|After");
    }
}

TEST_CASE("Table commands: the paragraphs next to a table removed stay apart in their formats",
          "[editor][tables][commands]") {
    const QString row = QStringLiteral("<table><tr><td><p>a</p></td><td><p>c</p></td></tr></table>");
    const QString after = QStringLiteral("<p style=\"quote\">After</p>");

    SECTION("a table of one row deleted") {
        const QString text = kml(QStringLiteral("<p>Before</p>") + row + after);
        auto editor = editorWith(text);
        editor->setCursorPosition({1, 0});
        editor->deleteTable();
        CHECK(kmlOf(*editor) == kml(QStringLiteral("<p>Before</p>") + after).toStdString());
        editor->undo();
        CHECK(kmlOf(*editor) == text.toStdString());
    }

    SECTION("a table of one column made text") {
        const QString text = kml(QStringLiteral(
            "<p>Before</p><table><tr><td><p>a1</p></td></tr><tr><td><p>a2</p></td></tr></table>") +
            after);
        auto editor = editorWith(text);
        editor->setCursorPosition({1, 0});
        editor->convertTableToText(QLatin1Char('\t'));
        CHECK(kmlOf(*editor) == kml(QStringLiteral("<p>Before</p><p>a1</p><p>a2</p>") + after).toStdString());
        editor->undo();
        CHECK(kmlOf(*editor) == text.toStdString());
    }

    SECTION("a table at the start and at the end of the text") {
        auto first = editorWith(kml(row + after));
        first->setCursorPosition({1, 0});
        first->deleteTable();
        CHECK(kmlOf(*first) == kml(after).toStdString());

        auto made = editorWith(kml(row + after));
        made->setCursorPosition({1, 0});
        made->convertTableToText(QLatin1Char(';'));
        CHECK(textOf(*made) == "a; c|After");

        auto last = editorWith(kml(QStringLiteral("<p>Before</p>") + row));
        last->setCursorPosition({1, 0});
        last->convertTableToText(QChar());
        CHECK(textOf(*last) == "Before|a|c");
        last->undo();
        last->deleteTable();
        CHECK(textOf(*last) == "Before");

        auto alone = editorWith(kml(row));
        alone->setCursorPosition({1, 0});
        alone->deleteTable();
        CHECK(textOf(*alone).empty());
    }
}

TEST_CASE("Table commands: Select > Cell, Row, Column, Table", "[editor][tables][commands]") {
    auto editor = editorWith(GRID_KML);
    editor->setCursorPosition({5, 1});  // b2
    using Part = BookEditor::TablePart;

    editor->selectTablePart(Part::Cell);
    CHECK(editor->selectedText() == QStringLiteral("b2"));
    CHECK_FALSE(editor->selectedCellRange().has_value());

    editor->selectTablePart(Part::Row);
    std::optional<TableCellRange> cells = editor->selectedCellRange();
    REQUIRE(cells.has_value());
    CHECK((cells->firstRow == 1 && cells->lastRow == 1));
    CHECK((cells->firstColumn == 0 && cells->lastColumn == 2));

    // With the row selected, all its columns: the table
    editor->selectTablePart(Part::Column);
    cells = editor->selectedCellRange();
    REQUIRE(cells.has_value());
    CHECK((cells->firstColumn == 0 && cells->lastColumn == 2));

    editor->clearSelection();
    editor->setCursorPosition({5, 1});
    editor->selectTablePart(Part::Column);
    cells = editor->selectedCellRange();
    REQUIRE(cells.has_value());
    CHECK((cells->firstRow == 0 && cells->lastRow == 2));
    CHECK((cells->firstColumn == 1 && cells->lastColumn == 1));

    editor->selectTablePart(Part::Table);
    cells = editor->selectedCellRange();
    REQUIRE(cells.has_value());
    CHECK((cells->firstRow == 0 && cells->lastRow == 2));
    CHECK((cells->firstColumn == 0 && cells->lastColumn == 2));
}

// =============================================================================
// Merge, split, distribute, header
// =============================================================================

TEST_CASE("Table commands: cells merged and split", "[editor][tables][commands]") {
    auto editor = editorWith(GRID_KML);

    SECTION("merged: one cell with the paragraphs of all") {
        select(*editor, {1, 0}, {5, 1});  // a1 to b2
        editor->mergeTableCells();
        const QTextTable* table = firstTable(*editor);
        CHECK(table->cellAt(0, 0).rowSpan() == 2);
        CHECK(table->cellAt(0, 0).columnSpan() == 2);
        CHECK(textOf(*editor).find("0,0:a1") != std::string::npos);
        CHECK(textOf(*editor).find("b2") != std::string::npos);
        editor->undo();
        CHECK(kmlOf(*editor) == GRID_KML.toStdString());
    }

    SECTION("split into two columns: the other cells of its column span them") {
        editor->setCursorPosition({5, 1});  // b2
        CHECK(editor->canSplitTableCells());
        CHECK(editor->splitCellParagraphs(false) == 1);
        editor->splitTableCells(2, 1, false, false);
        const QTextTable* table = firstTable(*editor);
        CHECK(table->columns() == 4);
        CHECK(table->cellAt(0, 1).columnSpan() == 2);
        CHECK(table->cellAt(1, 1).columnSpan() == 1);
        CHECK(textOf(*editor).find("1,1:b2|1,2:|1,3:c2") != std::string::npos);
        editor->undo();
        CHECK(kmlOf(*editor) == GRID_KML.toStdString());
    }

    SECTION("merged first and split again, the paragraphs one to a cell") {
        select(*editor, {1, 0}, {2, 1});  // a1 and b1
        CHECK(editor->splitCellParagraphs(true) == 2);
        editor->splitTableCells(2, 1, true, true);
        CHECK(textOf(*editor) == GRID_TEXT);
    }
}

TEST_CASE("Table commands: rows and columns evenly", "[editor][tables][commands]") {
    auto editor = editorWith(kml(QStringLiteral(
        "<p>Before</p><table><col width=\"20%\"/><col width=\"30%\"/><col width=\"50%\"/>"
        "<tr><td><p>a</p></td><td><p>b</p></td><td><p>c</p></td></tr>"
        "<tr height=\"40pt\"><td><p>d</p></td><td><p>e</p></td><td><p>f</p></td></tr>"
        "</table><p>After</p>")));
    editor->setCursorPosition({1, 0});

    SECTION("the columns") {
        editor->distributeTableColumns();
        for (const double width : columnWidthsOf(*firstTable(*editor))) {
            CHECK(width == Approx(100.0 / 3.0));
        }
        editor->undo();
        CHECK(columnWidthsOf(*firstTable(*editor)).front() == Approx(20.0));
    }

    SECTION("the rows: each as high as the highest") {
        editor->distributeTableRows();
        const QTextTable* table = firstTable(*editor);
        CHECK(rowPropertiesOf(*table, 0).minHeight > 0.0);
        CHECK(rowPropertiesOf(*table, 0).minHeight == Approx(rowPropertiesOf(*table, 1).minHeight));
    }
}

TEST_CASE("Table commands: Repeat Header Rows", "[editor][tables][commands]") {
    auto editor = editorWith(GRID_KML);
    const QTextTable* table = firstTable(*editor);
    editor->setCursorPosition({7, 0});  // a3: no header above it
    CHECK_FALSE(editor->canChangeTableHeader());
    editor->setCursorPosition({1, 0});  // a1
    CHECK(editor->canChangeTableHeader());
    CHECK_FALSE(editor->isTableHeader());
    editor->setTableHeader(true);
    CHECK(table->format().headerRowCount() == 1);
    CHECK(editor->isTableHeader());
    editor->setCursorPosition({4, 0});  // a2: the row after the header
    CHECK(editor->canChangeTableHeader());
    editor->setTableHeader(true);
    CHECK(table->format().headerRowCount() == 2);
    editor->setCursorPosition({1, 0});
    editor->setTableHeader(false);
    CHECK(table->format().headerRowCount() == 0);
    editor->undo();
    CHECK(table->format().headerRowCount() == 2);
}

TEST_CASE("Table commands: nothing changes in a read-only text", "[editor][tables][commands]") {
    auto editor = editorWith(GRID_KML);
    editor->setReadOnly(true);
    editor->setCursorPosition({5, 1});
    editor->insertTableRows(true);
    editor->deleteTableColumns();
    editor->mergeTableCells();
    editor->splitTableCells(2, 2, false, false);
    editor->setTableHeader(true);
    editor->distributeTableColumns();
    editor->convertTableToText(QLatin1Char('\t'));
    CHECK(textOf(*editor) == GRID_TEXT);
    CHECK_FALSE(editor->canUndo());
}

// =============================================================================
// Sorting
// =============================================================================

TEST_CASE("Table commands: the first number and the first date of a text", "[editor][tables][sort]") {
    CHECK(firstNumberIn(QStringLiteral("chapter 12, page 3")).value_or(0.0) == Approx(12.0));
    CHECK(firstNumberIn(QStringLiteral("2,5 kg")).value_or(0.0) == Approx(2.5));
    CHECK(firstNumberIn(QStringLiteral("-4 degrees")).value_or(0.0) == Approx(-4.0));
    CHECK_FALSE(firstNumberIn(QStringLiteral("none")).has_value());
    const QLocale polish(QLocale::Polish, QLocale::Poland);
    CHECK(firstDateIn(QStringLiteral("3.11.2026"), polish) == QDate(2026, 11, 3));
    CHECK(firstDateIn(QStringLiteral("w dniu 2026-11-03"), polish) == QDate(2026, 11, 3));
    CHECK(firstDateIn(QStringLiteral("3 Nov 2026"), polish) == QDate(2026, 11, 3));
    CHECK(firstDateIn(QStringLiteral("November 3, 2026"), polish) == QDate(2026, 11, 3));
    CHECK_FALSE(firstDateIn(QStringLiteral("no date"), polish).has_value());
}

TEST_CASE("Table commands: the order of rows", "[editor][tables][sort]") {
    const std::vector<QStringList> rows = {{QStringLiteral("b"), QStringLiteral("10")},
                                           {QStringLiteral("a"), QStringLiteral("2")},
                                           {QStringLiteral("c"), QStringLiteral("none")},
                                           {QStringLiteral("a"), QStringLiteral("1")}};
    SortOptions options;
    options.language = QStringLiteral("en");
    options.keys = {{1, SortType::Numbers, false}};
    CHECK(sortedOrder(rows, options) == std::vector<int>{3, 1, 0, 2});  // without a number last
    options.keys = {{1, SortType::Numbers, true}};
    CHECK(sortedOrder(rows, options) == std::vector<int>{0, 1, 3, 2});
    options.keys = {{0, SortType::Text, false}, {1, SortType::Numbers, false}};
    CHECK(sortedOrder(rows, options) == std::vector<int>{3, 1, 0, 2});
    options.keys = {{0, SortType::Text, false}};
    CHECK(sortedOrder(rows, options) == std::vector<int>{1, 3, 0, 2});  // the same keep their order
}

TEST_CASE("Table commands: Sort... in a table and of paragraphs", "[editor][tables][sort]") {
    SECTION("the rows under the header, in the order of the book's language") {
        auto editor = editorWith(PEOPLE_KML);
        editor->setCursorPosition({5, 0});  // Adam
        const std::optional<BookEditor::SortSubject> subject = editor->sortSubject();
        REQUIRE(subject.has_value());
        CHECK(subject->tableRows);
        CHECK(subject->header);
        CHECK(subject->count == 3);
        CHECK(subject->columns == QStringList{QStringLiteral("Name"), QStringLiteral("Age")});

        SortOptions options;
        options.language = QStringLiteral("pl");
        options.keys = {{0, SortType::Text, false}};
        editor->sortSelection(options);
        // Polish: Ł after L
        CHECK(textOf(*editor) == "Before|0,0:Name|0,1:Age|1,0:Adam|1,1:2|2,0:Lucyna|2,1:10|"
                                 "3,0:Łabędź|3,1:33|After");
        options.keys = {{1, SortType::Numbers, true}};
        editor->sortSelection(options);
        CHECK(textOf(*editor) == "Before|0,0:Name|0,1:Age|1,0:Łabędź|1,1:33|2,0:Lucyna|2,1:10|"
                                 "3,0:Adam|3,1:2|After");
        editor->undo();
        editor->undo();
        CHECK(kmlOf(*editor) == PEOPLE_KML.toStdString());
    }

    SECTION("the language of the text when the options have none") {
        auto editor = editorWith(PEOPLE_KML);
        editor->setTextLanguage([]() { return QStringLiteral("pl_PL"); });
        editor->setCursorPosition({5, 0});
        SortOptions options;
        options.keys = {{0, SortType::Text, true}};
        editor->sortSelection(options);
        CHECK(textOf(*editor).find("1,0:Łabędź|1,1:33|2,0:Lucyna") != std::string::npos);
    }

    SECTION("the selected paragraphs") {
        auto editor = editorWith(kml(QStringLiteral("<p>c</p><p>a</p><p>b</p><p>z</p>")));
        select(*editor, {0, 0}, {2, 1});
        const std::optional<BookEditor::SortSubject> subject = editor->sortSubject();
        REQUIRE(subject.has_value());
        CHECK_FALSE(subject->tableRows);
        CHECK(subject->count == 3);
        SortOptions options;
        options.keys = {{0, SortType::Text, false}};
        editor->sortSelection(options);
        CHECK(textOf(*editor) == "a|b|c|z");
    }

    SECTION("nothing to sort, and why") {
        auto editor = editorWith(kml(QStringLiteral(
            "<p>One</p><table><tr><th><p>H</p></th></tr><tr><td><p>x</p></td></tr></table><p>Two</p>")));
        editor->setCursorPosition({2, 0});
        CHECK_FALSE(editor->sortSubject().has_value());
        CHECK(editor->sortRefusal() == QStringLiteral("The table has no rows to sort under its header"));
        editor->setCursorPosition({0, 1});
        CHECK_FALSE(editor->sortSubject().has_value());
        CHECK(editor->sortRefusal() ==
              QStringLiteral("Select the paragraphs to sort, or put the cursor in a list"));
    }
}

// =============================================================================
// Moving rows and paragraphs
// =============================================================================

TEST_CASE("Table commands: Alt+Shift+Up and Down move rows and paragraphs",
          "[editor][tables][move]") {
    // Control+Shift+the arrows on macOS (Qt::MetaModifier there), as in Word for macOS
    const Qt::KeyboardModifiers MOVE = (isMacOS() ? Qt::MetaModifier : Qt::AltModifier) | Qt::ShiftModifier;
    SECTION("a row of a table, with the cursor") {
        auto editor = editorWith(GRID_KML);
        editor->setCursorPosition({5, 1});  // b2
        press(*editor, Qt::Key_Up, MOVE);
        CHECK(textOf(*editor) == "Before|0,0:a2|0,1:b2|0,2:c2|1,0:a1|1,1:b1|1,2:c1|"
                                 "2,0:a3|2,1:b3|2,2:c3|After");
        CHECK(editor->cursorPosition() == CursorPosition{2, 1});
        CHECK_FALSE(editor->moveParagraphsOrRows(false));  // the first row
        press(*editor, Qt::Key_Down, MOVE);
        press(*editor, Qt::Key_Down, MOVE);
        CHECK(textOf(*editor) == "Before|0,0:a1|0,1:b1|0,2:c1|1,0:a3|1,1:b3|1,2:c3|"
                                 "2,0:a2|2,1:b2|2,2:c2|After");
        editor->undo();
        editor->undo();
        editor->undo();
        CHECK(kmlOf(*editor) == GRID_KML.toStdString());
    }

    SECTION("not past the header") {
        auto editor = editorWith(PEOPLE_KML);
        editor->setCursorPosition({3, 0});  // Lucyna, under the header
        CHECK_FALSE(editor->moveParagraphsOrRows(false));
        CHECK(kmlOf(*editor) == PEOPLE_KML.toStdString());
    }

    SECTION("paragraphs, and past a table") {
        auto editor = editorWith(kml(QStringLiteral("<p>One</p><p>Two</p><p>Three</p>")));
        editor->setCursorPosition({1, 1});
        press(*editor, Qt::Key_Down, MOVE);
        CHECK(textOf(*editor) == "One|Three|Two");
        CHECK(editor->cursorPosition() == CursorPosition{2, 1});
        CHECK_FALSE(editor->moveParagraphsOrRows(true));  // the last

        auto table = editorWith(GRID_KML);
        table->setCursorPosition({0, 0});
        CHECK(table->moveParagraphsOrRows(true));
        // The empty paragraph at the start is the one Qt keeps before a table
        CHECK(textOf(*table) == "|0,0:a1|0,1:b1|0,2:c1|1,0:a2|1,1:b2|1,2:c2|2,0:a3|2,1:b3|2,2:c3|"
                                "Before|After");
        table->undo();
        CHECK(kmlOf(*table) == GRID_KML.toStdString());
    }

    SECTION("not with a selection going into or out of a table") {
        auto editor = editorWith(GRID_KML);
        select(*editor, {0, 2}, {5, 1});  // from Before to b2
        CHECK_FALSE(editor->moveParagraphsOrRows(true));
        CHECK_FALSE(editor->moveParagraphsOrRows(false));
        select(*editor, {5, 1}, {10, 2});  // from b2 to After
        CHECK_FALSE(editor->moveParagraphsOrRows(true));
        CHECK_FALSE(editor->moveParagraphsOrRows(false));
        CHECK(kmlOf(*editor) == GRID_KML.toStdString());
    }
}

// =============================================================================
// Text to table and back
// =============================================================================

TEST_CASE("Table commands: text to a table and back", "[editor][tables][convert]") {
    const QString text = kml(QStringLiteral("<p>Before</p><p>Name; Age</p><p>Adam; 2</p><p>After</p>"));

    SECTION("the paragraphs divided at a character, the spaces next to it going") {
        auto editor = editorWith(text);
        select(*editor, {1, 0}, {2, 3});
        CHECK(editor->canConvertTextToTable());
        CHECK(editor->paragraphsForTable() ==
              QStringList{QStringLiteral("Name; Age"), QStringLiteral("Adam; 2")});
        CHECK(likeliestSeparator(editor->paragraphsForTable()) == QLatin1Char(';'));
        editor->convertTextToTable(QLatin1Char(';'), 0, true);
        CHECK(textOf(*editor) == "Before|0,0:Name|0,1:Age|1,0:Adam|1,1:2|After");
        CHECK(firstTable(*editor)->format().headerRowCount() == 1);
        CHECK(editor->isInTable());
        CHECK_FALSE(editor->canConvertTextToTable());

        // Back, with a space after the semicolon
        CHECK(editor->tableTexts() == std::vector<QStringList>{
                                          {QStringLiteral("Name"), QStringLiteral("Age")},
                                          {QStringLiteral("Adam"), QStringLiteral("2")}});
        editor->convertTableToText(QLatin1Char(';'));
        CHECK(textOf(*editor) == "Before|Name; Age|Adam; 2|After");
        editor->undo();
        editor->undo();
        CHECK(kmlOf(*editor) == text.toStdString());
    }

    SECTION("each paragraph a cell, row by row") {
        auto editor = editorWith(kml(QStringLiteral("<p>Before</p><p>a</p><p>b</p><p>c</p><p>After</p>")));
        select(*editor, {1, 0}, {3, 1});
        editor->convertTextToTable(QChar(), 2, false);
        CHECK(textOf(*editor) == "Before|0,0:a|0,1:b|1,0:c|1,1:|After");
        editor->convertTableToText(QChar());
        CHECK(textOf(*editor) == "Before|a|b|c||After");
    }

    SECTION("a cell's paragraphs joined by a space, tabs between the cells") {
        auto editor = editorWith(kml(QStringLiteral(
            "<p>Before</p><table><tr><td><p>a</p><p>b</p></td><td><p>c</p></td></tr></table>"
            "<p>After</p>")));
        editor->setCursorPosition({1, 0});
        editor->convertTableToText(QLatin1Char('\t'));
        CHECK(textOf(*editor) == "Before|a b\tc|After");
    }

    SECTION("not with an object or a table among the paragraphs") {
        auto editor = editorWith(kml(QStringLiteral("<p>a</p><hr/><p>b</p>")));
        select(*editor, {0, 0}, {2, 1});
        CHECK_FALSE(editor->canConvertTextToTable());
        CHECK(editor->paragraphsForTable().isEmpty());
    }
}

// =============================================================================
// Table Properties...
// =============================================================================

TEST_CASE("Table commands: Table Properties... give the table, rows, columns and cells theirs",
          "[editor][tables][properties]") {
    auto editor = editorWith(GRID_KML);
    editor->setCursorPosition({5, 1});  // b2
    const std::optional<TablePropertiesData> before = editor->tablePropertiesData();
    REQUIRE(before.has_value());
    CHECK(before->rows.size() == 3);
    CHECK(before->columnWidths.size() == 3);
    CHECK(before->row == 1);
    CHECK(before->column == 1);
    CHECK(before->textWidth > 0.0);

    SECTION("nothing changed: no step of Undo") {
        editor->setTablePropertiesData(*before);
        CHECK_FALSE(editor->canUndo());
    }

    SECTION("all of them, in one step of Undo") {
        TablePropertiesData data = *before;
        data.table.alignment = TableAlignment::Center;
        data.table.width = 200.0;
        data.table.borders.lines = BORDER_BOX;
        data.rows[0].minHeight = 30.0;
        data.rows[2].allowSplit = true;
        data.headerRows = 1;
        data.columnWidths = {1.0, 1.0, 2.0};
        data.cell.background = QColor(QStringLiteral("#ffee00"));
        data.cell.alignment = CellAlignment::Bottom;
        data.cellChanged = true;
        editor->setTablePropertiesData(data);
        const QTextTable* table = firstTable(*editor);
        const TableProperties properties = tablePropertiesOf(table->format());
        CHECK(properties.alignment == TableAlignment::Center);
        CHECK(properties.width.value_or(0.0) == Approx(200.0));
        CHECK(properties.borders.lines == BORDER_BOX);
        CHECK(table->format().headerRowCount() == 1);
        CHECK(rowPropertiesOf(*table, 0).minHeight == Approx(30.0));
        CHECK(rowPropertiesOf(*table, 2).allowSplit);
        CHECK(columnWidthsOf(*table)[2] == Approx(50.0));
        CHECK(cellPropertiesOf(table->cellAt(1, 1)).background == QColor(QStringLiteral("#ffee00")));
        CHECK(cellPropertiesOf(table->cellAt(1, 1)).alignment == CellAlignment::Bottom);
        CHECK_FALSE(cellPropertiesOf(table->cellAt(0, 0)).background.isValid());
        CHECK(editor->cursorPosition() == CursorPosition{5, 1});
        editor->undo();
        CHECK(kmlOf(*editor) == GRID_KML.toStdString());
    }

    SECTION("the cells of a selection") {
        select(*editor, {1, 0}, {5, 1});  // a1 to b2
        TablePropertiesData data = *editor->tablePropertiesData();
        data.cell.background = QColor(QStringLiteral("#336699"));
        data.cellChanged = true;
        editor->setTablePropertiesData(data);
        const QTextTable* table = firstTable(*editor);
        for (const auto& [row, column] : {std::pair{0, 0}, {0, 1}, {1, 0}, {1, 1}}) {
            CHECK(cellPropertiesOf(table->cellAt(row, column)).background == QColor(QStringLiteral("#336699")));
        }
        CHECK_FALSE(cellPropertiesOf(table->cellAt(2, 2)).background.isValid());
    }
}

TEST_CASE("Table commands: the edge between two columns dragged", "[editor][tables][properties]") {
    auto editor = editorWith(kml(QStringLiteral(
        "<p>Before</p><table><tr><td><p>a</p></td><td><p>b</p></td></tr></table><p>After</p>")));
    QTextTable* table = firstTable(*editor);
    const auto* layout = qobject_cast<KalahariTextDocumentLayout*>(editor->textDocument()->documentLayout());
    REQUIRE(layout != nullptr);
    const std::vector<qreal> edges = layout->tableColumnEdges(*table);
    REQUIRE(edges.size() == 3);
    editor->setColumnEdge(*table, 1, edges[0] + (edges[2] - edges[0]) / 4.0);
    const std::vector<double> widths = columnWidthsOf(*table);
    CHECK(widths[0] == Approx(25.0).margin(0.5));
    CHECK(widths[1] == Approx(75.0).margin(0.5));
    // Not narrower than the narrowest column
    editor->setColumnEdge(*table, 1, edges[0] - 100.0);
    CHECK(columnWidthsOf(*table)[0] > 0.5);
    editor->undo();
    editor->undo();
    CHECK(columnWidthsOf(*table)[0] == Approx(50.0));
}

// =============================================================================
// On the pages
// =============================================================================

namespace {

/// A table of @p rows rows of 2 columns in a document of its own, laid out in pages
struct PagedTable {
    std::unique_ptr<QTextDocument> doc = std::make_unique<QTextDocument>();
    KalahariTextDocumentLayout* layout = nullptr;
    QTextTable* table = nullptr;

    PagedTable(int rows, int headerRows, const std::function<QString(int)>& text) {
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
        TableShape shape;
        shape.rows = rows;
        shape.columns = 2;
        shape.headerRows = headerRows;
        table = insertTable(cursor, shape);
        for (int row = 0; row < rows; ++row) {
            table->cellAt(row, 0).firstCursorPosition().insertText(text(row));
        }
    }

    void inPages(qreal pitch, qreal textHeight) {
        layout->setPageFlow(PageFlow{true, pitch, textHeight});
        layout->layoutPendingBlocks();
    }
};

QString words(int count) {
    QStringList list;
    for (int i = 0; i < count; ++i) {
        list << QStringLiteral("word");
    }
    return list.join(QLatin1Char(' '));
}

}  // anonymous namespace

TEST_CASE("Table commands: the header rows at the top of each page the table goes on to",
          "[editor][tables][pages]") {
    PagedTable paged(40, 1, [](int row) { return QStringLiteral("Row %1").arg(row); });
    constexpr qreal pitch = 400.0;
    paged.inPages(pitch, 300.0);
    const std::vector<qreal> repeats = paged.layout->headerRepeats(*paged.table);
    REQUIRE_FALSE(repeats.empty());
    for (const qreal top : repeats) {
        CHECK(std::fmod(top, pitch) == Approx(0.0).margin(0.01));
    }
    // The first row of a page is under the copy of the header
    const std::vector<KalahariTextDocumentLayout::TableRow> rows = paged.layout->tableRows(*paged.table);
    for (size_t i = 1; i < rows.size(); ++i) {
        CHECK(std::fmod(rows[i].top, pitch) > 1.0);
    }
}

TEST_CASE("Table commands: a row higher than a page is split, another goes to the next page",
          "[editor][tables][pages]") {
    constexpr qreal pitch = 400.0;
    constexpr qreal textHeight = 300.0;

    SECTION("higher than a page: split") {
        PagedTable paged(2, 0, [](int row) { return row == 1 ? words(400) : QStringLiteral("a"); });
        paged.inPages(pitch, textHeight);
        const std::vector<KalahariTextDocumentLayout::TableRow> rows = paged.layout->tableRows(*paged.table);
        REQUIRE(rows.size() == 2);
        CHECK(rows[1].parts.size() >= 2);
    }

    SECTION("not higher than a page: whole on the next one, or split when it may be") {
        const auto text = [](int row) { return row % 2 == 0 ? QStringLiteral("a") : words(90); };
        PagedTable whole(8, 0, text);
        whole.inPages(pitch, textHeight);
        bool moved = false;
        for (const auto& row : whole.layout->tableRows(*whole.table)) {
            CHECK(row.parts.size() <= 1);
            moved = moved || std::fmod(row.top, pitch) < 0.01;
        }
        CHECK(moved);

        PagedTable split(8, 0, text);
        QTextCursor cursor(split.doc.get());
        for (int row = 0; row < 8; ++row) {
            TableRowProperties properties;
            properties.allowSplit = true;
            setRowProperties(cursor, *split.table, row, properties);
        }
        split.inPages(pitch, textHeight);
        bool parted = false;
        for (const auto& row : split.layout->tableRows(*split.table)) {
            parted = parted || row.parts.size() >= 2;
        }
        CHECK(parted);
    }
}

// =============================================================================
// The rows laid out again after a change: as the whole text laid out anew
// =============================================================================

namespace {

struct LaidOutLine {
    qreal x = 0;
    qreal y = 0;
    qreal width = 0;
    int start = 0;
    int length = 0;
};

struct LaidOutBlock {
    qreal y = 0;
    qreal height = 0;
    std::vector<LaidOutLine> lines;
};

/// The places of the blocks and their lines, and of the copies of the headers of tables
struct LaidOut {
    std::vector<LaidOutBlock> blocks;
    std::vector<qreal> repeats;
    qreal height = 0;
};

KalahariTextDocumentLayout* layoutOf(const BookEditor& editor) {
    return qobject_cast<KalahariTextDocumentLayout*>(editor.textDocument()->documentLayout());
}

LaidOut laidOut(const BookEditor& editor) {
    KalahariTextDocumentLayout* layout = layoutOf(editor);
    layout->layoutPendingBlocks();
    LaidOut result;
    const QTextTable* lastTable = nullptr;
    for (QTextBlock block = editor.textDocument()->begin(); block.isValid(); block = block.next()) {
        LaidOutBlock entry;
        entry.y = layout->blockY(block.blockNumber());
        entry.height = layout->blockHeight(block.blockNumber());
        if (const QTextLayout* lines = block.layout()) {
            for (int i = 0; i < lines->lineCount(); ++i) {
                const QTextLine line = lines->lineAt(i);
                entry.lines.push_back({line.x(), line.y(), line.width(), line.textStart(), line.textLength()});
            }
        }
        result.blocks.push_back(entry);
        if (const QTextTable* table = tableOf(block); table && table != lastTable) {
            lastTable = table;
            for (const qreal top : layout->headerRepeats(*table)) {
                result.repeats.push_back(top);
            }
        }
    }
    result.height = layout->documentSize().height();
    return result;
}

/// The text laid out after the change is laid out as the same text laid out anew
void checkLaidOutAsAnew(const BookEditor& editor, const std::string& change) {
    INFO(change);
    const LaidOut changed = laidOut(editor);
    layoutOf(editor)->layoutAllBlocks();
    const LaidOut anew = laidOut(editor);
    REQUIRE(changed.blocks.size() == anew.blocks.size());
    for (size_t i = 0; i < anew.blocks.size(); ++i) {
        INFO("block " << i);
        const LaidOutBlock& a = changed.blocks[i];
        const LaidOutBlock& b = anew.blocks[i];
        CHECK(a.y == Approx(b.y).margin(0.01));
        CHECK(a.height == Approx(b.height).margin(0.01));
        REQUIRE(a.lines.size() == b.lines.size());
        for (size_t k = 0; k < b.lines.size(); ++k) {
            INFO("line " << k);
            CHECK(a.lines[k].x == Approx(b.lines[k].x).margin(0.01));
            CHECK(a.lines[k].y == Approx(b.lines[k].y).margin(0.01));
            CHECK(a.lines[k].width == Approx(b.lines[k].width).margin(0.01));
            CHECK(a.lines[k].start == b.lines[k].start);
            CHECK(a.lines[k].length == b.lines[k].length);
        }
    }
    REQUIRE(changed.repeats.size() == anew.repeats.size());
    for (size_t i = 0; i < anew.repeats.size(); ++i) {
        CHECK(changed.repeats[i] == Approx(anew.repeats[i]).margin(0.01));
    }
    CHECK(changed.height == Approx(anew.height).margin(0.01));
}

void typeText(BookEditor& editor, const QString& text) {
    for (const QChar c : text) {
        QKeyEvent event(QEvent::KeyPress, 0, Qt::NoModifier, QString(c));
        QCoreApplication::sendEvent(&editor, &event);
    }
}

/// A table with a header row, cells spanning two rows, a cell of two paragraphs, a cell in
/// the middle of its row and a row of its least height; blocks: 1-3 the header, 4-6 row 1
/// (4 spanning rows 1 and 2), 7-8 row 2, 9-12 row 3 (10 and 11 one cell), 13-15 row 4,
/// 16-18 row 5, 19-21 row 6 (21 spanning rows 6 and 7, higher than both, at their bottom),
/// 22-23 row 7, 24-29 rows 8 and 9
QString mixedTableKml() {
    QString rows = QStringLiteral(
        "<tr><th><p>Name</p></th><th><p>Text</p></th><th><p>Note</p></th></tr>"
        "<tr><td rowspan=\"2\"><p>Tall</p></td><td><p>b1</p></td><td><p>c1</p></td></tr>"
        "<tr><td><p>b2</p></td><td><p>c2</p></td></tr>"
        "<tr><td><p>a3</p></td><td><p>First</p><p>Second</p></td><td valign=\"middle\"><p>c3</p></td></tr>"
        "<tr height=\"60pt\"><td><p>a4</p></td><td><p>b4</p></td><td><p>c4</p></td></tr>");
    for (int row = 5; row < 10; ++row) {
        const QString last = row == 6   ? QStringLiteral("<td rowspan=\"2\" valign=\"bottom\"><p>c6 %1</p></td>")
                                              .arg(words(90))
                             : row == 7 ? QString()
                                        : QStringLiteral("<td><p>c%1</p></td>").arg(row);
        rows += QStringLiteral("<tr><td><p>a%1</p></td><td><p>%2</p></td>%3</tr>")
                    .arg(row)
                    .arg(words(row == 6 || row == 7 ? 4 : row * 6), last);
    }
    return kml(QStringLiteral("<p>Before</p><table>") + rows + QStringLiteral("</table><p>After</p>"));
}

}  // anonymous namespace

TEST_CASE("Table commands: the rows laid out again after a change are as the text laid out anew",
          "[editor][tables][layout]") {
    const bool pages = GENERATE(false, true);
    INFO((pages ? "in pages" : "without pages"));
    auto editor = editorWith(mixedTableKml());
    if (pages) {
        layoutOf(*editor)->setPageFlow(PageFlow{true, 260.0, 200.0});
    }
    layoutOf(*editor)->layoutPendingBlocks();
    REQUIRE(firstTable(*editor)->format().headerRowCount() == 1);
    REQUIRE(firstTable(*editor)->cellAt(1, 0).rowSpan() == 2);
    REQUIRE(firstTable(*editor)->cellAt(7, 2).row() == 6);
    REQUIRE(textOf(*editor).find("|7,0:a7|") != std::string::npos);

    const auto change = [&](const std::string& name, const std::function<void()>& command) {
        command();
        checkLaidOutAsAnew(*editor, name);
        editor->undo();
        checkLaidOutAsAnew(*editor, name + ", undone");
    };

    change("typed in a cell of a row in the middle", [&] {
        editor->setCursorPosition({13, 2});
        typeText(*editor, QStringLiteral(" and more words that make the cell wrap to more lines"));
    });
    change("typed in the header", [&] {
        editor->setCursorPosition({2, 4});
        typeText(*editor, QStringLiteral(" of the column, so long that the header row gets higher"));
    });
    change("typed in the cell spanning two rows", [&] {
        editor->setCursorPosition({4, 4});
        typeText(*editor, QStringLiteral(" cell with words enough to be higher than both its rows"));
    });
    change("typed in a row a cell spans from the row above", [&] {
        editor->setCursorPosition({22, 2});
        typeText(*editor, QStringLiteral(" and the words that make the row higher than before"));
    });
    change("typed in a cell spanning two rows below the head", [&] {
        editor->setCursorPosition({21, 2});
        typeText(*editor, QStringLiteral(" spans two rows and gets higher than both of them now"));
    });
    change("typed in the first row below the header", [&] {
        editor->setCursorPosition({5, 2});
        typeText(*editor, QStringLiteral(" grows higher with these words in it, and more"));
    });
    change("typed in the last row", [&] {
        const int last = static_cast<int>(editor->paragraphCount()) - 2;
        editor->setCursorPosition({last, 0});
        typeText(*editor, QStringLiteral("the last cell of the table, now on more lines "));
    });
    change("Enter in a cell", [&] {
        editor->setCursorPosition({10, 2});
        press(*editor, Qt::Key_Return);
        typeText(*editor, QStringLiteral("new"));
    });
    change("Backspace joins the paragraphs of a cell", [&] {
        editor->setCursorPosition({11, 0});
        press(*editor, Qt::Key_Backspace);
    });
    change("rows inserted below the header", [&] {
        editor->setCursorPosition({1, 0});
        editor->insertTableRows(true);
    });
    change("rows inserted above a row in the middle", [&] {
        editor->setCursorPosition({12, 0});
        editor->insertTableRows(false);
    });
    change("a row deleted", [&] {
        editor->setCursorPosition({12, 0});
        editor->deleteTableRows();
    });
    change("the first row below the header deleted", [&] {
        editor->setCursorPosition({5, 0});
        editor->deleteTableRows();
    });
    change("a column inserted", [&] {
        editor->setCursorPosition({12, 0});
        editor->insertTableColumns(true);
    });
    change("a column deleted", [&] {
        editor->setCursorPosition({13, 0});
        editor->deleteTableColumns();
    });
    change("cells merged across rows", [&] {
        select(*editor, {12, 0}, {16, 0});  // a4 to b5
        editor->mergeTableCells();
    });
    change("the spanning cell split", [&] {
        editor->setCursorPosition({4, 0});
        editor->splitTableCells(1, 2, false, false);
    });
    change("rows moved down", [&] {
        editor->setCursorPosition({12, 0});
        editor->moveParagraphsOrRows(true);
    });
    change("a second header row", [&] {
        editor->setCursorPosition({5, 0});
        editor->setTableHeader(true);
    });
    change("selected cells cleared", [&] {
        select(*editor, {12, 0}, {19, 0});
        press(*editor, Qt::Key_Delete);
    });
    change("rows sorted", [&] {
        select(*editor, {12, 0}, {24, 0});
        SortOptions options;
        options.keys = {{1, SortType::Text, true}};
        editor->sortSelection(options);
    });
    change("the properties of a row and of a cell", [&] {
        editor->setCursorPosition({13, 0});
        std::optional<TablePropertiesData> data = editor->tablePropertiesData();
        REQUIRE(data);
        TableRowProperties& row = data->rows[static_cast<size_t>(data->row)];
        row.minHeight = 90.0;
        row.allowSplit = true;
        data->cell.alignment = CellAlignment::Bottom;
        data->cellChanged = true;
        editor->setTablePropertiesData(*data);
    });

    // Changes one after another, laid out with what the ones before left
    editor->setCursorPosition({13, 2});
    typeText(*editor, QStringLiteral(" more"));
    editor->setCursorPosition({2, 4});
    typeText(*editor, QStringLiteral(" header words to wrap the header to another line"));
    editor->setCursorPosition({1, 0});
    editor->insertTableRows(true);
    editor->setCursorPosition({30, 0});
    typeText(*editor, QStringLiteral("text "));
    checkLaidOutAsAnew(*editor, "one change after another");
}
