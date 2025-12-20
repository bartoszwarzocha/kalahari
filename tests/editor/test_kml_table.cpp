/// @file test_kml_table.cpp
/// @brief Unit tests for KML Table elements (OpenSpec #00042 Phase 1.12)

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/kml_table.h>
#include <kalahari/editor/kml_parser.h>
#include <kalahari/editor/kml_text_run.h>
#include <kalahari/editor/kml_inline_elements.h>

using namespace kalahari::editor;

// =============================================================================
// KmlTableCell Tests
// =============================================================================

TEST_CASE("KmlTableCell construction", "[editor][kml_table]") {
    SECTION("Default construction creates empty data cell") {
        KmlTableCell cell;
        REQUIRE(cell.isEmpty());
        REQUIRE(!cell.isHeader());
        REQUIRE(cell.colspan() == 1);
        REQUIRE(cell.rowspan() == 1);
        REQUIRE(!cell.hasSpanning());
        REQUIRE(cell.plainText().isEmpty());
    }

    SECTION("Construction with text") {
        KmlTableCell cell("Hello");
        REQUIRE(!cell.isEmpty());
        REQUIRE(cell.plainText() == "Hello");
        REQUIRE(!cell.isHeader());
    }

    SECTION("Construction as header cell") {
        KmlTableCell cell("Header", true);
        REQUIRE(cell.plainText() == "Header");
        REQUIRE(cell.isHeader());
    }
}

TEST_CASE("KmlTableCell copy semantics", "[editor][kml_table]") {
    SECTION("Copy constructor") {
        KmlTableCell original("Content", true);
        original.setColspan(2);
        original.setRowspan(3);

        KmlTableCell copy(original);
        REQUIRE(copy.plainText() == "Content");
        REQUIRE(copy.isHeader());
        REQUIRE(copy.colspan() == 2);
        REQUIRE(copy.rowspan() == 3);
    }

    SECTION("Copy assignment") {
        KmlTableCell original("Source");
        KmlTableCell target("Target");

        target = original;
        REQUIRE(target.plainText() == "Source");
    }

    SECTION("Clone method") {
        KmlTableCell original("Cloned", true);
        original.setColspan(4);

        auto clone = original.clone();
        REQUIRE(clone != nullptr);
        REQUIRE(clone->plainText() == "Cloned");
        REQUIRE(clone->isHeader());
        REQUIRE(clone->colspan() == 4);
    }
}

TEST_CASE("KmlTableCell move semantics", "[editor][kml_table]") {
    SECTION("Move constructor") {
        KmlTableCell original("Moving", true);
        original.setColspan(2);

        KmlTableCell moved(std::move(original));
        REQUIRE(moved.plainText() == "Moving");
        REQUIRE(moved.isHeader());
        REQUIRE(moved.colspan() == 2);
    }

    SECTION("Move assignment") {
        KmlTableCell original("Source");
        KmlTableCell target("Target");

        target = std::move(original);
        REQUIRE(target.plainText() == "Source");
    }
}

TEST_CASE("KmlTableCell header type", "[editor][kml_table]") {
    SECTION("Set header flag") {
        KmlTableCell cell("Data");
        REQUIRE(!cell.isHeader());

        cell.setHeader(true);
        REQUIRE(cell.isHeader());

        cell.setHeader(false);
        REQUIRE(!cell.isHeader());
    }
}

TEST_CASE("KmlTableCell content methods", "[editor][kml_table]") {
    SECTION("Access mutable content") {
        KmlTableCell cell;
        cell.content().addElement(std::make_unique<KmlTextRun>("Added text"));
        REQUIRE(cell.plainText() == "Added text");
    }

    SECTION("Set content with paragraph") {
        KmlTableCell cell;
        auto para = std::make_unique<KmlParagraph>("New content");
        cell.setContent(std::move(para));
        REQUIRE(cell.plainText() == "New content");
    }

    SECTION("Content with formatting") {
        KmlTableCell cell;
        auto bold = std::make_unique<KmlBold>();
        bold->appendChild(std::make_unique<KmlTextRun>("Bold text"));
        cell.content().addElement(std::move(bold));
        REQUIRE(cell.plainText() == "Bold text");
    }
}

TEST_CASE("KmlTableCell spanning attributes", "[editor][kml_table]") {
    SECTION("Default span values") {
        KmlTableCell cell;
        REQUIRE(cell.colspan() == 1);
        REQUIRE(cell.rowspan() == 1);
        REQUIRE(!cell.hasSpanning());
    }

    SECTION("Set colspan") {
        KmlTableCell cell;
        cell.setColspan(3);
        REQUIRE(cell.colspan() == 3);
        REQUIRE(cell.hasSpanning());
    }

    SECTION("Set rowspan") {
        KmlTableCell cell;
        cell.setRowspan(2);
        REQUIRE(cell.rowspan() == 2);
        REQUIRE(cell.hasSpanning());
    }

    SECTION("Both colspan and rowspan") {
        KmlTableCell cell;
        cell.setColspan(2);
        cell.setRowspan(3);
        REQUIRE(cell.hasSpanning());
        REQUIRE(cell.colspan() == 2);
        REQUIRE(cell.rowspan() == 3);
    }

    SECTION("Invalid span values are clamped to 1") {
        KmlTableCell cell;
        cell.setColspan(0);
        REQUIRE(cell.colspan() == 1);

        cell.setColspan(-5);
        REQUIRE(cell.colspan() == 1);

        cell.setRowspan(0);
        REQUIRE(cell.rowspan() == 1);
    }
}

TEST_CASE("KmlTableCell toKml", "[editor][kml_table]") {
    SECTION("Simple data cell") {
        KmlTableCell cell("Hello");
        QString kml = cell.toKml();
        REQUIRE(kml.contains("<td>"));
        REQUIRE(kml.contains("</td>"));
        REQUIRE(kml.contains("Hello"));
        // Text is wrapped in <t> tags
        REQUIRE(kml.contains("<t>Hello</t>"));
    }

    SECTION("Header cell") {
        KmlTableCell cell("Header", true);
        QString kml = cell.toKml();
        REQUIRE(kml.contains("<th>"));
        REQUIRE(kml.contains("</th>"));
    }

    SECTION("Cell with colspan") {
        KmlTableCell cell("Spanning");
        cell.setColspan(2);
        QString kml = cell.toKml();
        REQUIRE(kml.contains("colspan=\"2\""));
    }

    SECTION("Cell with rowspan") {
        KmlTableCell cell("Spanning");
        cell.setRowspan(3);
        QString kml = cell.toKml();
        REQUIRE(kml.contains("rowspan=\"3\""));
    }

    SECTION("Cell with both spans") {
        KmlTableCell cell("Spanning", true);
        cell.setColspan(2);
        cell.setRowspan(3);
        QString kml = cell.toKml();
        REQUIRE(kml.contains("<th"));
        REQUIRE(kml.contains("colspan=\"2\""));
        REQUIRE(kml.contains("rowspan=\"3\""));
    }

    SECTION("Empty cell") {
        KmlTableCell cell;
        QString kml = cell.toKml();
        REQUIRE(kml == "<td></td>");
    }

    SECTION("Cell with formatted content") {
        KmlTableCell cell;
        auto bold = std::make_unique<KmlBold>();
        bold->appendChild(std::make_unique<KmlTextRun>("Bold"));
        cell.content().addElement(std::move(bold));

        QString kml = cell.toKml();
        // Bold wraps text in <t> tags inside <b>
        REQUIRE(kml.contains("<b><t>Bold</t></b>"));
    }
}

// =============================================================================
// KmlTableRow Tests
// =============================================================================

TEST_CASE("KmlTableRow construction", "[editor][kml_table]") {
    SECTION("Default construction creates empty row") {
        KmlTableRow row;
        REQUIRE(row.isEmpty());
        REQUIRE(row.cellCount() == 0);
    }
}

TEST_CASE("KmlTableRow cell management", "[editor][kml_table]") {
    SECTION("Add cells") {
        KmlTableRow row;
        row.addCell(std::make_unique<KmlTableCell>("Cell 1"));
        row.addCell(std::make_unique<KmlTableCell>("Cell 2"));

        REQUIRE(row.cellCount() == 2);
        REQUIRE(!row.isEmpty());
        REQUIRE(row.cell(0)->plainText() == "Cell 1");
        REQUIRE(row.cell(1)->plainText() == "Cell 2");
    }

    SECTION("Insert cell at beginning") {
        KmlTableRow row;
        row.addCell(std::make_unique<KmlTableCell>("Second"));
        row.insertCell(0, std::make_unique<KmlTableCell>("First"));

        REQUIRE(row.cellCount() == 2);
        REQUIRE(row.cell(0)->plainText() == "First");
        REQUIRE(row.cell(1)->plainText() == "Second");
    }

    SECTION("Insert cell at end") {
        KmlTableRow row;
        row.addCell(std::make_unique<KmlTableCell>("First"));
        row.insertCell(100, std::make_unique<KmlTableCell>("Last")); // Beyond end

        REQUIRE(row.cellCount() == 2);
        REQUIRE(row.cell(1)->plainText() == "Last");
    }

    SECTION("Remove cell") {
        KmlTableRow row;
        row.addCell(std::make_unique<KmlTableCell>("Cell 1"));
        row.addCell(std::make_unique<KmlTableCell>("Cell 2"));

        auto removed = row.removeCell(0);
        REQUIRE(removed != nullptr);
        REQUIRE(removed->plainText() == "Cell 1");
        REQUIRE(row.cellCount() == 1);
        REQUIRE(row.cell(0)->plainText() == "Cell 2");
    }

    SECTION("Remove cell out of range returns nullptr") {
        KmlTableRow row;
        row.addCell(std::make_unique<KmlTableCell>("Cell"));

        REQUIRE(row.removeCell(-1) == nullptr);
        REQUIRE(row.removeCell(100) == nullptr);
        REQUIRE(row.cellCount() == 1);
    }

    SECTION("Clear cells") {
        KmlTableRow row;
        row.addCell(std::make_unique<KmlTableCell>("Cell 1"));
        row.addCell(std::make_unique<KmlTableCell>("Cell 2"));

        row.clearCells();
        REQUIRE(row.isEmpty());
        REQUIRE(row.cellCount() == 0);
    }

    SECTION("Cell access out of range returns nullptr") {
        KmlTableRow row;
        row.addCell(std::make_unique<KmlTableCell>("Cell"));

        REQUIRE(row.cell(-1) == nullptr);
        REQUIRE(row.cell(1) == nullptr);
        REQUIRE(row.cell(100) == nullptr);
    }
}

TEST_CASE("KmlTableRow copy semantics", "[editor][kml_table]") {
    SECTION("Copy constructor") {
        KmlTableRow original;
        original.addCell(std::make_unique<KmlTableCell>("Cell 1"));
        original.addCell(std::make_unique<KmlTableCell>("Cell 2", true));

        KmlTableRow copy(original);
        REQUIRE(copy.cellCount() == 2);
        REQUIRE(copy.cell(0)->plainText() == "Cell 1");
        REQUIRE(copy.cell(1)->isHeader());
    }

    SECTION("Clone method") {
        KmlTableRow original;
        original.addCell(std::make_unique<KmlTableCell>("Cloned"));

        auto clone = original.clone();
        REQUIRE(clone != nullptr);
        REQUIRE(clone->cellCount() == 1);
        REQUIRE(clone->cell(0)->plainText() == "Cloned");
    }
}

TEST_CASE("KmlTableRow toKml", "[editor][kml_table]") {
    SECTION("Row with cells") {
        KmlTableRow row;
        row.addCell(std::make_unique<KmlTableCell>("A"));
        row.addCell(std::make_unique<KmlTableCell>("B"));

        QString kml = row.toKml();
        REQUIRE(kml.startsWith("<tr>"));
        REQUIRE(kml.endsWith("</tr>"));
        // Text is wrapped in <t> tags
        REQUIRE(kml.contains("<td><t>A</t></td>"));
        REQUIRE(kml.contains("<td><t>B</t></td>"));
    }

    SECTION("Empty row") {
        KmlTableRow row;
        QString kml = row.toKml();
        REQUIRE(kml == "<tr></tr>");
    }

    SECTION("Row with mixed header and data cells") {
        KmlTableRow row;
        row.addCell(std::make_unique<KmlTableCell>("Header", true));
        row.addCell(std::make_unique<KmlTableCell>("Data", false));

        QString kml = row.toKml();
        // Text is wrapped in <t> tags
        REQUIRE(kml.contains("<th><t>Header</t></th>"));
        REQUIRE(kml.contains("<td><t>Data</t></td>"));
    }
}

// =============================================================================
// KmlTable Tests
// =============================================================================

TEST_CASE("KmlTable construction", "[editor][kml_table]") {
    SECTION("Default construction creates empty table") {
        KmlTable table;
        REQUIRE(table.isEmpty());
        REQUIRE(table.rowCount() == 0);
        REQUIRE(table.columnCount() == 0);
        REQUIRE(!table.hasStyle());
    }
}

TEST_CASE("KmlTable row management", "[editor][kml_table]") {
    SECTION("Add rows") {
        KmlTable table;

        auto row1 = std::make_unique<KmlTableRow>();
        row1->addCell(std::make_unique<KmlTableCell>("Row 1"));

        auto row2 = std::make_unique<KmlTableRow>();
        row2->addCell(std::make_unique<KmlTableCell>("Row 2"));

        table.addRow(std::move(row1));
        table.addRow(std::move(row2));

        REQUIRE(table.rowCount() == 2);
        REQUIRE(!table.isEmpty());
    }

    SECTION("Insert row") {
        KmlTable table;

        auto row2 = std::make_unique<KmlTableRow>();
        row2->addCell(std::make_unique<KmlTableCell>("Second"));
        table.addRow(std::move(row2));

        auto row1 = std::make_unique<KmlTableRow>();
        row1->addCell(std::make_unique<KmlTableCell>("First"));
        table.insertRow(0, std::move(row1));

        REQUIRE(table.rowCount() == 2);
        REQUIRE(table.row(0)->cell(0)->plainText() == "First");
        REQUIRE(table.row(1)->cell(0)->plainText() == "Second");
    }

    SECTION("Remove row") {
        KmlTable table;

        auto row1 = std::make_unique<KmlTableRow>();
        row1->addCell(std::make_unique<KmlTableCell>("Row 1"));
        table.addRow(std::move(row1));

        auto row2 = std::make_unique<KmlTableRow>();
        row2->addCell(std::make_unique<KmlTableCell>("Row 2"));
        table.addRow(std::move(row2));

        auto removed = table.removeRow(0);
        REQUIRE(removed != nullptr);
        REQUIRE(table.rowCount() == 1);
        REQUIRE(table.row(0)->cell(0)->plainText() == "Row 2");
    }

    SECTION("Clear rows") {
        KmlTable table;
        auto row = std::make_unique<KmlTableRow>();
        row->addCell(std::make_unique<KmlTableCell>("Cell"));
        table.addRow(std::move(row));

        table.clearRows();
        REQUIRE(table.isEmpty());
    }

    SECTION("Row access out of range returns nullptr") {
        KmlTable table;
        REQUIRE(table.row(0) == nullptr);
        REQUIRE(table.row(-1) == nullptr);
        REQUIRE(table.row(100) == nullptr);
    }
}

TEST_CASE("KmlTable column count", "[editor][kml_table]") {
    SECTION("Simple table") {
        KmlTable table;

        auto row = std::make_unique<KmlTableRow>();
        row->addCell(std::make_unique<KmlTableCell>("A"));
        row->addCell(std::make_unique<KmlTableCell>("B"));
        row->addCell(std::make_unique<KmlTableCell>("C"));
        table.addRow(std::move(row));

        REQUIRE(table.columnCount() == 3);
    }

    SECTION("Table with colspan") {
        KmlTable table;

        auto row = std::make_unique<KmlTableRow>();
        auto cell = std::make_unique<KmlTableCell>("Spanning");
        cell->setColspan(3);
        row->addCell(std::move(cell));
        table.addRow(std::move(row));

        REQUIRE(table.columnCount() == 3);
    }

    SECTION("Rows with different column counts") {
        KmlTable table;

        auto row1 = std::make_unique<KmlTableRow>();
        row1->addCell(std::make_unique<KmlTableCell>("A"));
        row1->addCell(std::make_unique<KmlTableCell>("B"));
        table.addRow(std::move(row1));

        auto row2 = std::make_unique<KmlTableRow>();
        row2->addCell(std::make_unique<KmlTableCell>("A"));
        row2->addCell(std::make_unique<KmlTableCell>("B"));
        row2->addCell(std::make_unique<KmlTableCell>("C"));
        row2->addCell(std::make_unique<KmlTableCell>("D"));
        table.addRow(std::move(row2));

        REQUIRE(table.columnCount() == 4);
    }
}

TEST_CASE("KmlTable cellAt", "[editor][kml_table]") {
    SECTION("Simple table access") {
        KmlTable table;

        auto row1 = std::make_unique<KmlTableRow>();
        row1->addCell(std::make_unique<KmlTableCell>("R0C0"));
        row1->addCell(std::make_unique<KmlTableCell>("R0C1"));
        table.addRow(std::move(row1));

        auto row2 = std::make_unique<KmlTableRow>();
        row2->addCell(std::make_unique<KmlTableCell>("R1C0"));
        row2->addCell(std::make_unique<KmlTableCell>("R1C1"));
        table.addRow(std::move(row2));

        REQUIRE(table.cellAt(0, 0)->plainText() == "R0C0");
        REQUIRE(table.cellAt(0, 1)->plainText() == "R0C1");
        REQUIRE(table.cellAt(1, 0)->plainText() == "R1C0");
        REQUIRE(table.cellAt(1, 1)->plainText() == "R1C1");
    }

    SECTION("Access with colspan") {
        KmlTable table;

        auto row = std::make_unique<KmlTableRow>();
        auto cell = std::make_unique<KmlTableCell>("Spanning");
        cell->setColspan(3);
        row->addCell(std::move(cell));
        row->addCell(std::make_unique<KmlTableCell>("After"));
        table.addRow(std::move(row));

        // All three logical columns point to the spanning cell
        REQUIRE(table.cellAt(0, 0)->plainText() == "Spanning");
        REQUIRE(table.cellAt(0, 1)->plainText() == "Spanning");
        REQUIRE(table.cellAt(0, 2)->plainText() == "Spanning");
        REQUIRE(table.cellAt(0, 3)->plainText() == "After");
    }

    SECTION("Out of range returns nullptr") {
        KmlTable table;

        auto row = std::make_unique<KmlTableRow>();
        row->addCell(std::make_unique<KmlTableCell>("Cell"));
        table.addRow(std::move(row));

        REQUIRE(table.cellAt(-1, 0) == nullptr);
        REQUIRE(table.cellAt(0, -1) == nullptr);
        REQUIRE(table.cellAt(1, 0) == nullptr);
        REQUIRE(table.cellAt(0, 1) == nullptr);
    }
}

TEST_CASE("KmlTable style", "[editor][kml_table]") {
    SECTION("Default has no style") {
        KmlTable table;
        REQUIRE(!table.hasStyle());
        REQUIRE(table.styleId().isEmpty());
    }

    SECTION("Set and get style") {
        KmlTable table;
        table.setStyleId("bordered");
        REQUIRE(table.hasStyle());
        REQUIRE(table.styleId() == "bordered");
    }

    SECTION("Clear style") {
        KmlTable table;
        table.setStyleId("style");
        table.setStyleId("");
        REQUIRE(!table.hasStyle());
    }
}

TEST_CASE("KmlTable copy semantics", "[editor][kml_table]") {
    SECTION("Copy constructor") {
        KmlTable original;
        original.setStyleId("myStyle");

        auto row = std::make_unique<KmlTableRow>();
        row->addCell(std::make_unique<KmlTableCell>("Cell"));
        original.addRow(std::move(row));

        KmlTable copy(original);
        REQUIRE(copy.rowCount() == 1);
        REQUIRE(copy.styleId() == "myStyle");
        REQUIRE(copy.row(0)->cell(0)->plainText() == "Cell");
    }

    SECTION("Clone method") {
        KmlTable original;

        auto row = std::make_unique<KmlTableRow>();
        row->addCell(std::make_unique<KmlTableCell>("Cloned"));
        original.addRow(std::move(row));

        auto clone = original.clone();
        REQUIRE(clone != nullptr);
        REQUIRE(clone->rowCount() == 1);
        REQUIRE(clone->row(0)->cell(0)->plainText() == "Cloned");
    }
}

TEST_CASE("KmlTable toKml", "[editor][kml_table]") {
    SECTION("Simple table") {
        KmlTable table;

        auto row = std::make_unique<KmlTableRow>();
        row->addCell(std::make_unique<KmlTableCell>("A"));
        row->addCell(std::make_unique<KmlTableCell>("B"));
        table.addRow(std::move(row));

        QString kml = table.toKml();
        REQUIRE(kml.startsWith("<table>"));
        REQUIRE(kml.endsWith("</table>"));
        REQUIRE(kml.contains("<tr>"));
        // Text is wrapped in <t> tags
        REQUIRE(kml.contains("<td><t>A</t></td>"));
        REQUIRE(kml.contains("<td><t>B</t></td>"));
    }

    SECTION("Table with style") {
        KmlTable table;
        table.setStyleId("bordered");

        auto row = std::make_unique<KmlTableRow>();
        row->addCell(std::make_unique<KmlTableCell>("Cell"));
        table.addRow(std::move(row));

        QString kml = table.toKml();
        REQUIRE(kml.contains("style=\"bordered\""));
    }

    SECTION("Table with header row") {
        KmlTable table;

        auto headerRow = std::make_unique<KmlTableRow>();
        headerRow->addCell(std::make_unique<KmlTableCell>("Name", true));
        headerRow->addCell(std::make_unique<KmlTableCell>("Age", true));
        table.addRow(std::move(headerRow));

        auto dataRow = std::make_unique<KmlTableRow>();
        dataRow->addCell(std::make_unique<KmlTableCell>("Alice"));
        dataRow->addCell(std::make_unique<KmlTableCell>("25"));
        table.addRow(std::move(dataRow));

        QString kml = table.toKml();
        // Text is wrapped in <t> tags
        REQUIRE(kml.contains("<th><t>Name</t></th>"));
        REQUIRE(kml.contains("<th><t>Age</t></th>"));
        REQUIRE(kml.contains("<td><t>Alice</t></td>"));
        REQUIRE(kml.contains("<td><t>25</t></td>"));
    }

    SECTION("Table with spanning cells") {
        KmlTable table;

        auto row = std::make_unique<KmlTableRow>();
        auto cell = std::make_unique<KmlTableCell>("Spanning", true);
        cell->setColspan(2);
        cell->setRowspan(3);
        row->addCell(std::move(cell));
        table.addRow(std::move(row));

        QString kml = table.toKml();
        REQUIRE(kml.contains("colspan=\"2\""));
        REQUIRE(kml.contains("rowspan=\"3\""));
    }

    SECTION("Empty table") {
        KmlTable table;
        QString kml = table.toKml();
        REQUIRE(kml == "<table></table>");
    }

    SECTION("Style with special characters is escaped") {
        KmlTable table;
        table.setStyleId("style&name\"test");

        QString kml = table.toKml();
        REQUIRE(kml.contains("style=\"style&amp;name&quot;test\""));
    }
}

TEST_CASE("KmlTable complex structures", "[editor][kml_table]") {
    SECTION("3x3 table") {
        KmlTable table;

        for (int r = 0; r < 3; ++r) {
            auto row = std::make_unique<KmlTableRow>();
            for (int c = 0; c < 3; ++c) {
                row->addCell(std::make_unique<KmlTableCell>(
                    QString("R%1C%2").arg(r).arg(c)));
            }
            table.addRow(std::move(row));
        }

        REQUIRE(table.rowCount() == 3);
        REQUIRE(table.columnCount() == 3);
        REQUIRE(table.cellAt(1, 1)->plainText() == "R1C1");
        REQUIRE(table.cellAt(2, 2)->plainText() == "R2C2");
    }

    SECTION("Table with formatted cell content") {
        KmlTable table;

        auto row = std::make_unique<KmlTableRow>();
        auto cell = std::make_unique<KmlTableCell>();

        auto bold = std::make_unique<KmlBold>();
        bold->appendChild(std::make_unique<KmlTextRun>("Important"));
        cell->content().addElement(std::move(bold));
        cell->content().addElement(std::make_unique<KmlTextRun>(" note"));

        row->addCell(std::move(cell));
        table.addRow(std::move(row));

        REQUIRE(table.row(0)->cell(0)->plainText() == "Important note");

        QString kml = table.toKml();
        // Text is wrapped in <t> tags
        REQUIRE(kml.contains("<b><t>Important</t></b>"));
        REQUIRE(kml.contains("<t> note</t>"));
    }
}

// =============================================================================
// KmlParser Table Tests
// =============================================================================

TEST_CASE("KmlParser parseTable basic", "[editor][kml_table][parser]") {
    KmlParser parser;

    SECTION("Simple table") {
        QString kml = "<table><tr><td>Cell</td></tr></table>";
        auto result = parser.parseTable(kml);
        REQUIRE(result);
        REQUIRE(result.result->rowCount() == 1);
        REQUIRE(result.result->row(0)->cellCount() == 1);
        REQUIRE(result.result->cellAt(0, 0)->plainText() == "Cell");
    }

    SECTION("Table with multiple cells") {
        QString kml = "<table><tr><td>A</td><td>B</td><td>C</td></tr></table>";
        auto result = parser.parseTable(kml);
        REQUIRE(result);
        REQUIRE(result.result->row(0)->cellCount() == 3);
        REQUIRE(result.result->cellAt(0, 0)->plainText() == "A");
        REQUIRE(result.result->cellAt(0, 1)->plainText() == "B");
        REQUIRE(result.result->cellAt(0, 2)->plainText() == "C");
    }

    SECTION("Table with multiple rows") {
        QString kml =
            "<table>"
            "<tr><td>R0C0</td><td>R0C1</td></tr>"
            "<tr><td>R1C0</td><td>R1C1</td></tr>"
            "</table>";
        auto result = parser.parseTable(kml);
        REQUIRE(result);
        REQUIRE(result.result->rowCount() == 2);
        REQUIRE(result.result->cellAt(0, 0)->plainText() == "R0C0");
        REQUIRE(result.result->cellAt(1, 1)->plainText() == "R1C1");
    }

    SECTION("Empty table") {
        QString kml = "<table></table>";
        auto result = parser.parseTable(kml);
        REQUIRE(result);
        REQUIRE(result.result->isEmpty());
    }
}

TEST_CASE("KmlParser parseTable header cells", "[editor][kml_table][parser]") {
    KmlParser parser;

    SECTION("Header cells are marked") {
        QString kml =
            "<table>"
            "<tr><th>Header 1</th><th>Header 2</th></tr>"
            "<tr><td>Data 1</td><td>Data 2</td></tr>"
            "</table>";

        auto result = parser.parseTable(kml);
        REQUIRE(result);
        REQUIRE(result.result->rowCount() == 2);

        // First row - headers
        REQUIRE(result.result->row(0)->cell(0)->isHeader());
        REQUIRE(result.result->row(0)->cell(1)->isHeader());
        REQUIRE(result.result->row(0)->cell(0)->plainText() == "Header 1");

        // Second row - data
        REQUIRE(!result.result->row(1)->cell(0)->isHeader());
        REQUIRE(!result.result->row(1)->cell(1)->isHeader());
    }

    SECTION("Mixed header and data in same row") {
        QString kml = "<table><tr><th>Header</th><td>Data</td></tr></table>";
        auto result = parser.parseTable(kml);
        REQUIRE(result);
        REQUIRE(result.result->row(0)->cell(0)->isHeader());
        REQUIRE(!result.result->row(0)->cell(1)->isHeader());
    }
}

TEST_CASE("KmlParser parseTable spanning attributes", "[editor][kml_table][parser]") {
    KmlParser parser;

    SECTION("Cell with colspan") {
        QString kml = "<table><tr><td colspan=\"3\">Spanning</td></tr></table>";
        auto result = parser.parseTable(kml);
        REQUIRE(result);
        REQUIRE(result.result->cellAt(0, 0)->colspan() == 3);
        REQUIRE(result.result->cellAt(0, 0)->rowspan() == 1);  // default
    }

    SECTION("Cell with rowspan") {
        QString kml = "<table><tr><td rowspan=\"2\">Spanning</td></tr></table>";
        auto result = parser.parseTable(kml);
        REQUIRE(result);
        REQUIRE(result.result->cellAt(0, 0)->rowspan() == 2);
        REQUIRE(result.result->cellAt(0, 0)->colspan() == 1);  // default
    }

    SECTION("Cell with both spans") {
        QString kml = "<table><tr><th colspan=\"2\" rowspan=\"3\">Big cell</th></tr></table>";
        auto result = parser.parseTable(kml);
        REQUIRE(result);
        REQUIRE(result.result->cellAt(0, 0)->colspan() == 2);
        REQUIRE(result.result->cellAt(0, 0)->rowspan() == 3);
        REQUIRE(result.result->cellAt(0, 0)->isHeader());
    }
}

TEST_CASE("KmlParser parseTable styled", "[editor][kml_table][parser]") {
    KmlParser parser;

    SECTION("Table with style attribute") {
        QString kml = "<table style=\"bordered\"><tr><td>Cell</td></tr></table>";
        auto result = parser.parseTable(kml);
        REQUIRE(result);
        REQUIRE(result.result->styleId() == "bordered");
    }
}

TEST_CASE("KmlParser parseTable formatted content", "[editor][kml_table][parser]") {
    KmlParser parser;

    SECTION("Cell with bold text") {
        QString kml = "<table><tr><td><b>Bold</b></td></tr></table>";
        auto result = parser.parseTable(kml);
        REQUIRE(result);
        REQUIRE(result.result->cellAt(0, 0)->plainText() == "Bold");
    }

    SECTION("Cell with mixed formatting") {
        QString kml = "<table><tr><td>Normal <b>bold</b> and <i>italic</i></td></tr></table>";
        auto result = parser.parseTable(kml);
        REQUIRE(result);
        REQUIRE(result.result->cellAt(0, 0)->plainText() == "Normal bold and italic");
    }

    SECTION("Cell with subscript and superscript") {
        QString kml = "<table><tr><td>H<sub>2</sub>O and x<sup>2</sup></td></tr></table>";
        auto result = parser.parseTable(kml);
        REQUIRE(result);
        REQUIRE(result.result->cellAt(0, 0)->plainText() == "H2O and x2");
    }
}

TEST_CASE("KmlParser parseTable errors", "[editor][kml_table][parser]") {
    KmlParser parser;

    SECTION("Empty input") {
        auto result = parser.parseTable("");
        REQUIRE(!result);
        REQUIRE(!result.errorMessage.isEmpty());
    }

    SECTION("Non-table element") {
        auto result = parser.parseTable("<p>Not a table</p>");
        REQUIRE(!result);
        REQUIRE(result.errorMessage.contains("<table>"));
    }
}

TEST_CASE("KmlParser parseTable round-trip", "[editor][kml_table][parser]") {
    KmlParser parser;

    SECTION("Simple table round-trip") {
        QString original = "<table><tr><td>Cell</td></tr></table>";
        auto result1 = parser.parseTable(original);
        REQUIRE(result1);

        QString serialized = result1.result->toKml();
        auto result2 = parser.parseTable(serialized);
        REQUIRE(result2);

        REQUIRE(result2.result->rowCount() == result1.result->rowCount());
        REQUIRE(result2.result->cellAt(0, 0)->plainText() == "Cell");
    }

    SECTION("Complex table round-trip") {
        QString original =
            "<table style=\"bordered\">"
            "<tr><th colspan=\"2\">Header</th></tr>"
            "<tr><td>A</td><td>B</td></tr>"
            "</table>";

        auto result1 = parser.parseTable(original);
        REQUIRE(result1);

        QString serialized = result1.result->toKml();
        auto result2 = parser.parseTable(serialized);
        REQUIRE(result2);

        REQUIRE(result2.result->styleId() == "bordered");
        REQUIRE(result2.result->rowCount() == 2);
        REQUIRE(result2.result->row(0)->cell(0)->isHeader());
        REQUIRE(result2.result->row(0)->cell(0)->colspan() == 2);
    }

    SECTION("Table with formatted cells round-trip") {
        KmlTable table;
        auto row = std::make_unique<KmlTableRow>();
        auto cell = std::make_unique<KmlTableCell>();
        auto bold = std::make_unique<KmlBold>();
        bold->appendChild(std::make_unique<KmlTextRun>("Bold text"));
        cell->content().addElement(std::move(bold));
        row->addCell(std::move(cell));
        table.addRow(std::move(row));

        QString serialized = table.toKml();
        auto result = parser.parseTable(serialized);
        REQUIRE(result);
        REQUIRE(result.result->cellAt(0, 0)->plainText() == "Bold text");
    }
}

// =============================================================================
// Round-Trip Tests
// =============================================================================

TEST_CASE("KmlTable round-trip", "[editor][kml_table]") {
    SECTION("Simple table serialization stability") {
        KmlTable table;

        auto row = std::make_unique<KmlTableRow>();
        row->addCell(std::make_unique<KmlTableCell>("Cell 1"));
        row->addCell(std::make_unique<KmlTableCell>("Cell 2"));
        table.addRow(std::move(row));

        QString kml1 = table.toKml();

        // Clone and serialize again
        auto clone = table.clone();
        QString kml2 = clone->toKml();

        REQUIRE(kml1 == kml2);
    }

    SECTION("Complex table serialization stability") {
        KmlTable table;
        table.setStyleId("bordered");

        // Header row
        auto headerRow = std::make_unique<KmlTableRow>();
        auto headerCell = std::make_unique<KmlTableCell>("Header", true);
        headerCell->setColspan(2);
        headerRow->addCell(std::move(headerCell));
        table.addRow(std::move(headerRow));

        // Data row
        auto dataRow = std::make_unique<KmlTableRow>();
        dataRow->addCell(std::make_unique<KmlTableCell>("A"));
        dataRow->addCell(std::make_unique<KmlTableCell>("B"));
        table.addRow(std::move(dataRow));

        QString kml1 = table.toKml();
        auto clone = table.clone();
        QString kml2 = clone->toKml();

        REQUIRE(kml1 == kml2);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("KmlTable edge cases", "[editor][kml_table]") {
    SECTION("Very large colspan") {
        KmlTableCell cell("Wide");
        cell.setColspan(100);
        REQUIRE(cell.colspan() == 100);

        QString kml = cell.toKml();
        REQUIRE(kml.contains("colspan=\"100\""));
    }

    SECTION("Null content handling") {
        KmlTableCell cell;
        cell.setContent(nullptr);

        // Should not crash
        REQUIRE(cell.isEmpty());
        REQUIRE(cell.plainText().isEmpty());
    }

    SECTION("Deeply nested row operations") {
        KmlTable table;

        // Add 100 rows
        for (int i = 0; i < 100; ++i) {
            auto row = std::make_unique<KmlTableRow>();
            row->addCell(std::make_unique<KmlTableCell>(QString::number(i)));
            table.addRow(std::move(row));
        }

        REQUIRE(table.rowCount() == 100);
        REQUIRE(table.cellAt(50, 0)->plainText() == "50");

        // Remove every other row
        for (int i = 49; i >= 0; --i) {
            table.removeRow(i * 2);
        }

        REQUIRE(table.rowCount() == 50);
    }

    SECTION("Mutable cell access modification") {
        KmlTable table;
        auto row = std::make_unique<KmlTableRow>();
        row->addCell(std::make_unique<KmlTableCell>("Original"));
        table.addRow(std::move(row));

        // Modify through mutable access
        KmlTableCell* cell = table.cellAt(0, 0);
        REQUIRE(cell != nullptr);
        cell->content().clearElements();
        cell->content().addElement(std::make_unique<KmlTextRun>("Modified"));

        REQUIRE(table.cellAt(0, 0)->plainText() == "Modified");
    }
}
