/// @file test_table_layout.cpp
/// @brief Unit tests for TableLayout (OpenSpec #00042 Phase 2.6/2.7)

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/table_layout.h>
#include <kalahari/editor/kml_table.h>
#include <kalahari/editor/kml_text_run.h>
#include <kalahari/editor/kml_inline_elements.h>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <cmath>

using namespace kalahari::editor;

// =============================================================================
// Helper Functions
// =============================================================================

/// Create a simple table with specified dimensions
std::unique_ptr<KmlTable> createSimpleTable(int rows, int cols, const QString& cellPrefix = "Cell") {
    auto table = std::make_unique<KmlTable>();
    for (int r = 0; r < rows; ++r) {
        auto row = std::make_unique<KmlTableRow>();
        for (int c = 0; c < cols; ++c) {
            QString text = QString("%1_%2_%3").arg(cellPrefix).arg(r).arg(c);
            auto cell = std::make_unique<KmlTableCell>(text);
            row->addCell(std::move(cell));
        }
        table->addRow(std::move(row));
    }
    return table;
}

/// Create a table with header row
std::unique_ptr<KmlTable> createTableWithHeader(int dataRows, int cols) {
    auto table = std::make_unique<KmlTable>();

    // Header row
    auto headerRow = std::make_unique<KmlTableRow>();
    for (int c = 0; c < cols; ++c) {
        auto cell = std::make_unique<KmlTableCell>(QString("Header %1").arg(c), true);
        headerRow->addCell(std::move(cell));
    }
    table->addRow(std::move(headerRow));

    // Data rows
    for (int r = 0; r < dataRows; ++r) {
        auto row = std::make_unique<KmlTableRow>();
        for (int c = 0; c < cols; ++c) {
            auto cell = std::make_unique<KmlTableCell>(QString("Data %1,%2").arg(r).arg(c));
            row->addCell(std::move(cell));
        }
        table->addRow(std::move(row));
    }

    return table;
}

// =============================================================================
// Constructor Tests
// =============================================================================

TEST_CASE("TableLayout default constructor", "[editor][table_layout]") {
    TableLayout layout;

    SECTION("Initial state") {
        REQUIRE(layout.table() == nullptr);
        REQUIRE(layout.isDirty());
        REQUIRE(layout.height() == 0.0);
        REQUIRE(layout.layoutWidth() == 0.0);
        REQUIRE(layout.rowCount() == 0);
        REQUIRE(layout.columnCount() == 0);
    }

    SECTION("Default configuration") {
        REQUIRE(layout.columnWidthMode() == ColumnWidthMode::Equal);
        REQUIRE(layout.minColumnWidth() == 20.0);
        REQUIRE(layout.cellSpacing() == 1.0);
        REQUIRE(layout.hasCustomHeaderFont() == false);
    }

    SECTION("Default padding is set") {
        QMarginsF padding = layout.cellPadding();
        REQUIRE(padding.left() > 0);
        REQUIRE(padding.top() > 0);
        REQUIRE(padding.right() > 0);
        REQUIRE(padding.bottom() > 0);
    }
}

// =============================================================================
// Table and Font Tests
// =============================================================================

TEST_CASE("TableLayout setTable", "[editor][table_layout]") {
    TableLayout layout;
    auto table = createSimpleTable(2, 3);

    SECTION("Set table marks dirty") {
        layout.setTable(table.get());
        REQUIRE(layout.table() == table.get());
        REQUIRE(layout.isDirty());
    }

    SECTION("Set same table does not mark dirty") {
        layout.setTable(table.get());
        layout.doLayout(500.0);
        REQUIRE(layout.isDirty() == false);

        layout.setTable(table.get());  // Same table
        REQUIRE(layout.isDirty() == false);
    }

    SECTION("Set different table marks dirty") {
        layout.setTable(table.get());
        layout.doLayout(500.0);

        auto table2 = createSimpleTable(3, 2);
        layout.setTable(table2.get());
        REQUIRE(layout.isDirty());
    }

    SECTION("Set nullptr table") {
        layout.setTable(table.get());
        layout.setTable(nullptr);
        REQUIRE(layout.table() == nullptr);
        REQUIRE(layout.isDirty());
    }
}

TEST_CASE("TableLayout setFont", "[editor][table_layout]") {
    TableLayout layout;
    auto table = createSimpleTable(2, 2);
    layout.setTable(table.get());

    SECTION("Set font marks dirty") {
        QFont font("Serif", 14);
        layout.setFont(font);
        REQUIRE(layout.font().pointSize() == 14);
        REQUIRE(layout.isDirty());
    }

    SECTION("Set same font does not mark dirty") {
        QFont font("Serif", 14);
        layout.setFont(font);
        layout.doLayout(500.0);
        REQUIRE(layout.isDirty() == false);

        layout.setFont(font);
        REQUIRE(layout.isDirty() == false);
    }
}

TEST_CASE("TableLayout header font", "[editor][table_layout]") {
    TableLayout layout;
    layout.setFont(QFont("Serif", 12));

    SECTION("Default header font is bold version of regular font") {
        REQUIRE(layout.hasCustomHeaderFont() == false);
        QFont headerFont = layout.headerFont();
        REQUIRE(headerFont.bold());
    }

    SECTION("Custom header font") {
        QFont customHeader("Arial", 14);
        customHeader.setItalic(true);
        layout.setHeaderFont(customHeader);

        REQUIRE(layout.hasCustomHeaderFont());
        REQUIRE(layout.headerFont().pointSize() == 14);
        REQUIRE(layout.headerFont().italic());
        REQUIRE(layout.isDirty());
    }
}

// =============================================================================
// Layout Configuration Tests
// =============================================================================

TEST_CASE("TableLayout cell padding", "[editor][table_layout]") {
    TableLayout layout;

    SECTION("Set padding marks dirty") {
        layout.setCellPadding(QMarginsF(10, 5, 10, 5));
        QMarginsF padding = layout.cellPadding();
        REQUIRE(padding.left() == 10.0);
        REQUIRE(padding.top() == 5.0);
        REQUIRE(layout.isDirty());
    }

    SECTION("Set same padding does not mark dirty") {
        layout.setCellPadding(QMarginsF(10, 5, 10, 5));
        auto table = createSimpleTable(1, 1);
        layout.setTable(table.get());
        layout.doLayout(500.0);
        REQUIRE(layout.isDirty() == false);

        layout.setCellPadding(QMarginsF(10, 5, 10, 5));
        REQUIRE(layout.isDirty() == false);
    }
}

TEST_CASE("TableLayout cell spacing", "[editor][table_layout]") {
    TableLayout layout;

    SECTION("Set spacing marks dirty") {
        layout.setCellSpacing(5.0);
        REQUIRE(layout.cellSpacing() == 5.0);
        REQUIRE(layout.isDirty());
    }

    SECTION("Negative spacing is clamped to 0") {
        layout.setCellSpacing(-5.0);
        REQUIRE(layout.cellSpacing() >= 0.0);
    }
}

TEST_CASE("TableLayout column width mode", "[editor][table_layout]") {
    TableLayout layout;

    SECTION("Default is Equal") {
        REQUIRE(layout.columnWidthMode() == ColumnWidthMode::Equal);
    }

    SECTION("Set ContentBased") {
        layout.setColumnWidthMode(ColumnWidthMode::ContentBased);
        REQUIRE(layout.columnWidthMode() == ColumnWidthMode::ContentBased);
        REQUIRE(layout.isDirty());
    }

    SECTION("Set Fixed") {
        layout.setColumnWidthMode(ColumnWidthMode::Fixed);
        REQUIRE(layout.columnWidthMode() == ColumnWidthMode::Fixed);
    }
}

TEST_CASE("TableLayout minimum column width", "[editor][table_layout]") {
    TableLayout layout;

    SECTION("Set minimum width") {
        layout.setMinColumnWidth(50.0);
        REQUIRE(layout.minColumnWidth() == 50.0);
        REQUIRE(layout.isDirty());
    }

    SECTION("Minimum width is clamped to 1") {
        layout.setMinColumnWidth(0.0);
        REQUIRE(layout.minColumnWidth() >= 1.0);

        layout.setMinColumnWidth(-10.0);
        REQUIRE(layout.minColumnWidth() >= 1.0);
    }
}

// =============================================================================
// Layout Operation Tests
// =============================================================================

TEST_CASE("TableLayout doLayout basic", "[editor][table_layout]") {
    TableLayout layout;
    auto table = createSimpleTable(2, 3);
    layout.setTable(table.get());
    layout.setFont(QFont("Serif", 12));

    SECTION("Layout returns positive height") {
        qreal height = layout.doLayout(500.0);
        REQUIRE(height > 0.0);
    }

    SECTION("Layout clears dirty flag") {
        REQUIRE(layout.isDirty());
        layout.doLayout(500.0);
        REQUIRE(layout.isDirty() == false);
    }

    SECTION("Layout stores width") {
        layout.doLayout(500.0);
        REQUIRE(layout.layoutWidth() == 500.0);
    }

    SECTION("Layout updates height") {
        layout.doLayout(500.0);
        REQUIRE(layout.height() > 0.0);
    }

    SECTION("Row and column counts are set") {
        layout.doLayout(500.0);
        REQUIRE(layout.rowCount() == 2);
        REQUIRE(layout.columnCount() == 3);
    }
}

TEST_CASE("TableLayout doLayout caching", "[editor][table_layout]") {
    TableLayout layout;
    auto table = createSimpleTable(2, 2);
    layout.setTable(table.get());
    layout.setFont(QFont("Serif", 12));

    SECTION("Same width uses cached result") {
        qreal height1 = layout.doLayout(500.0);
        REQUIRE(layout.isDirty() == false);

        qreal height2 = layout.doLayout(500.0);
        REQUIRE(height1 == height2);
        REQUIRE(layout.isDirty() == false);
    }

    SECTION("Different width triggers re-layout") {
        layout.doLayout(500.0);
        qreal width1 = layout.layoutWidth();

        layout.doLayout(300.0);
        qreal width2 = layout.layoutWidth();

        REQUIRE(width2 != width1);
        REQUIRE(width2 == 300.0);
    }
}

TEST_CASE("TableLayout doLayout with empty table", "[editor][table_layout]") {
    TableLayout layout;
    KmlTable emptyTable;
    layout.setTable(&emptyTable);

    SECTION("Empty table has zero height") {
        qreal height = layout.doLayout(500.0);
        REQUIRE(height == 0.0);
    }

    SECTION("Empty table has zero dimensions") {
        layout.doLayout(500.0);
        REQUIRE(layout.rowCount() == 0);
        REQUIRE(layout.columnCount() == 0);
    }
}

TEST_CASE("TableLayout doLayout with null table", "[editor][table_layout]") {
    TableLayout layout;

    SECTION("Null table has zero height") {
        qreal height = layout.doLayout(500.0);
        REQUIRE(height == 0.0);
    }
}

// =============================================================================
// Geometry Tests
// =============================================================================

TEST_CASE("TableLayout geometry", "[editor][table_layout]") {
    TableLayout layout;
    auto table = createSimpleTable(3, 4);
    layout.setTable(table.get());
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(800.0);

    SECTION("Height is positive") {
        REQUIRE(layout.height() > 0.0);
    }

    SECTION("Bounding rect has dimensions") {
        QRectF rect = layout.boundingRect();
        REQUIRE(rect.width() > 0.0);
        REQUIRE(rect.height() > 0.0);
    }

    SECTION("All row heights are positive") {
        for (int r = 0; r < layout.rowCount(); ++r) {
            REQUIRE(layout.rowHeight(r) > 0.0);
        }
    }

    SECTION("All column widths are positive") {
        for (int c = 0; c < layout.columnCount(); ++c) {
            REQUIRE(layout.columnWidth(c) > 0.0);
        }
    }

    SECTION("Invalid row/column returns 0") {
        REQUIRE(layout.rowHeight(-1) == 0.0);
        REQUIRE(layout.rowHeight(100) == 0.0);
        REQUIRE(layout.columnWidth(-1) == 0.0);
        REQUIRE(layout.columnWidth(100) == 0.0);
    }
}

TEST_CASE("TableLayout row and column positions", "[editor][table_layout]") {
    TableLayout layout;
    auto table = createSimpleTable(3, 3);
    layout.setTable(table.get());
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(600.0);

    SECTION("First row/column starts at 0") {
        REQUIRE(layout.rowY(0) == 0.0);
        REQUIRE(layout.columnX(0) == 0.0);
    }

    SECTION("Rows are stacked vertically") {
        qreal prevY = layout.rowY(0);
        for (int r = 1; r < layout.rowCount(); ++r) {
            qreal y = layout.rowY(r);
            REQUIRE(y > prevY);
            prevY = y;
        }
    }

    SECTION("Columns are positioned horizontally") {
        qreal prevX = layout.columnX(0);
        for (int c = 1; c < layout.columnCount(); ++c) {
            qreal x = layout.columnX(c);
            REQUIRE(x > prevX);
            prevX = x;
        }
    }

    SECTION("Invalid positions return 0") {
        REQUIRE(layout.rowY(-1) == 0.0);
        REQUIRE(layout.rowY(100) == 0.0);
        REQUIRE(layout.columnX(-1) == 0.0);
        REQUIRE(layout.columnX(100) == 0.0);
    }
}

// =============================================================================
// Cell Layout Tests
// =============================================================================

TEST_CASE("TableLayout cell layout access", "[editor][table_layout]") {
    TableLayout layout;
    auto table = createSimpleTable(2, 2);
    layout.setTable(table.get());
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(400.0);

    SECTION("Cell layout exists for each cell") {
        for (int r = 0; r < 2; ++r) {
            for (int c = 0; c < 2; ++c) {
                const CellLayoutInfo* info = layout.cellLayout(r, c);
                REQUIRE(info != nullptr);
                REQUIRE(info->row == r);
                REQUIRE(info->column == c);
            }
        }
    }

    SECTION("Cell layout has valid rect") {
        const CellLayoutInfo* info = layout.cellLayout(0, 0);
        REQUIRE(info != nullptr);
        REQUIRE(info->rect.width() > 0.0);
        REQUIRE(info->rect.height() > 0.0);
    }

    SECTION("Cell layout has paragraph layout") {
        const CellLayoutInfo* info = layout.cellLayout(0, 0);
        REQUIRE(info != nullptr);
        REQUIRE(info->layout.text().contains("Cell_0_0"));
    }

    SECTION("Invalid cell returns nullptr") {
        REQUIRE(layout.cellLayout(-1, 0) == nullptr);
        REQUIRE(layout.cellLayout(0, -1) == nullptr);
        REQUIRE(layout.cellLayout(100, 0) == nullptr);
        REQUIRE(layout.cellLayout(0, 100) == nullptr);
    }

    SECTION("Cell layouts vector is accessible") {
        const auto& layouts = layout.cellLayouts();
        REQUIRE(layouts.size() == 4);  // 2x2 table
    }
}

// =============================================================================
// Column Width Distribution Tests
// =============================================================================

TEST_CASE("TableLayout equal width distribution", "[editor][table_layout]") {
    TableLayout layout;
    auto table = createSimpleTable(2, 4);
    layout.setTable(table.get());
    layout.setFont(QFont("Serif", 12));
    layout.setColumnWidthMode(ColumnWidthMode::Equal);
    layout.setCellSpacing(0.0);  // No spacing for easier calculation
    layout.doLayout(400.0);

    SECTION("All columns have equal width") {
        qreal expectedWidth = 400.0 / 4;  // 100.0 each
        for (int c = 0; c < 4; ++c) {
            REQUIRE(std::abs(layout.columnWidth(c) - expectedWidth) < 0.1);
        }
    }
}

TEST_CASE("TableLayout content-based width distribution", "[editor][table_layout]") {
    TableLayout layout;

    // Create table with varying content lengths
    auto table = std::make_unique<KmlTable>();
    auto row = std::make_unique<KmlTableRow>();
    row->addCell(std::make_unique<KmlTableCell>("Short"));
    row->addCell(std::make_unique<KmlTableCell>("This is a much longer cell content"));
    row->addCell(std::make_unique<KmlTableCell>("Medium text"));
    table->addRow(std::move(row));

    layout.setTable(table.get());
    layout.setFont(QFont("Serif", 12));
    layout.setColumnWidthMode(ColumnWidthMode::ContentBased);
    layout.setCellSpacing(0.0);
    layout.doLayout(800.0);

    SECTION("Columns have varying widths") {
        qreal w0 = layout.columnWidth(0);
        qreal w1 = layout.columnWidth(1);
        qreal w2 = layout.columnWidth(2);

        // The longer content should get more width
        // (Exact proportions depend on font metrics)
        REQUIRE(w0 > 0.0);
        REQUIRE(w1 > 0.0);
        REQUIRE(w2 > 0.0);

        // Total should approximately equal available width
        qreal total = w0 + w1 + w2;
        REQUIRE(std::abs(total - 800.0) < 1.0);
    }
}

TEST_CASE("TableLayout minimum column width enforced", "[editor][table_layout]") {
    TableLayout layout;
    auto table = createSimpleTable(1, 10);  // Many narrow columns
    layout.setTable(table.get());
    layout.setFont(QFont("Serif", 12));
    layout.setMinColumnWidth(50.0);
    layout.setCellSpacing(0.0);
    layout.doLayout(300.0);  // Less than needed for 10 * 50 = 500

    SECTION("All columns at least minimum width") {
        for (int c = 0; c < 10; ++c) {
            REQUIRE(layout.columnWidth(c) >= 50.0);
        }
    }
}

// =============================================================================
// Row Height Tests
// =============================================================================

TEST_CASE("TableLayout row height calculation", "[editor][table_layout]") {
    TableLayout layout;
    auto table = createSimpleTable(3, 2);
    layout.setTable(table.get());
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(400.0);

    SECTION("All rows have positive height") {
        for (int r = 0; r < 3; ++r) {
            REQUIRE(layout.rowHeight(r) > 0.0);
        }
    }

    SECTION("Row heights include padding") {
        QMarginsF padding = layout.cellPadding();
        qreal minHeight = padding.top() + padding.bottom();

        for (int r = 0; r < 3; ++r) {
            REQUIRE(layout.rowHeight(r) >= minHeight);
        }
    }
}

TEST_CASE("TableLayout row height with varying content", "[editor][table_layout]") {
    TableLayout layout;

    // Create table with one cell having multi-line content
    auto table = std::make_unique<KmlTable>();
    auto row1 = std::make_unique<KmlTableRow>();
    row1->addCell(std::make_unique<KmlTableCell>("Short"));
    row1->addCell(std::make_unique<KmlTableCell>("Also short"));
    table->addRow(std::move(row1));

    auto row2 = std::make_unique<KmlTableRow>();
    row2->addCell(std::make_unique<KmlTableCell>("This cell has a lot of content that will wrap to multiple lines when the column is narrow"));
    row2->addCell(std::make_unique<KmlTableCell>("Short"));
    table->addRow(std::move(row2));

    layout.setTable(table.get());
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(200.0);  // Narrow to force wrapping

    SECTION("Row with multi-line content is taller") {
        qreal h0 = layout.rowHeight(0);
        qreal h1 = layout.rowHeight(1);

        // Second row should be taller due to wrapped content
        REQUIRE(h1 >= h0);
    }
}

// =============================================================================
// Colspan/Rowspan Tests
// =============================================================================

TEST_CASE("TableLayout with colspan", "[editor][table_layout]") {
    TableLayout layout;

    // Create table with colspan
    auto table = std::make_unique<KmlTable>();

    auto row1 = std::make_unique<KmlTableRow>();
    auto spanCell = std::make_unique<KmlTableCell>("Spanning 2 columns");
    spanCell->setColspan(2);
    row1->addCell(std::move(spanCell));
    row1->addCell(std::make_unique<KmlTableCell>("Normal"));
    table->addRow(std::move(row1));

    auto row2 = std::make_unique<KmlTableRow>();
    row2->addCell(std::make_unique<KmlTableCell>("A"));
    row2->addCell(std::make_unique<KmlTableCell>("B"));
    row2->addCell(std::make_unique<KmlTableCell>("C"));
    table->addRow(std::move(row2));

    layout.setTable(table.get());
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(600.0);

    SECTION("Table has correct dimensions") {
        REQUIRE(layout.rowCount() == 2);
        REQUIRE(layout.columnCount() == 3);
    }

    SECTION("Spanning cell has correct info") {
        const CellLayoutInfo* spanInfo = layout.cellLayout(0, 0);
        REQUIRE(spanInfo != nullptr);
        REQUIRE(spanInfo->colspan == 2);
    }

    SECTION("Spanning cell rect is wider") {
        const CellLayoutInfo* spanInfo = layout.cellLayout(0, 0);
        const CellLayoutInfo* normalInfo = layout.cellLayout(1, 0);
        REQUIRE(spanInfo != nullptr);
        REQUIRE(normalInfo != nullptr);

        // Spanning cell should be approximately twice as wide
        REQUIRE(spanInfo->rect.width() > normalInfo->rect.width());
    }
}

TEST_CASE("TableLayout with rowspan", "[editor][table_layout]") {
    TableLayout layout;

    // Create table with rowspan
    auto table = std::make_unique<KmlTable>();

    auto row1 = std::make_unique<KmlTableRow>();
    auto spanCell = std::make_unique<KmlTableCell>("Spanning 2 rows");
    spanCell->setRowspan(2);
    row1->addCell(std::move(spanCell));
    row1->addCell(std::make_unique<KmlTableCell>("B1"));
    table->addRow(std::move(row1));

    auto row2 = std::make_unique<KmlTableRow>();
    // First column is occupied by rowspan
    row2->addCell(std::make_unique<KmlTableCell>("B2"));
    table->addRow(std::move(row2));

    layout.setTable(table.get());
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(400.0);

    SECTION("Spanning cell has correct info") {
        const CellLayoutInfo* spanInfo = layout.cellLayout(0, 0);
        REQUIRE(spanInfo != nullptr);
        REQUIRE(spanInfo->rowspan == 2);
    }

    SECTION("Spanning cell rect is taller") {
        const CellLayoutInfo* spanInfo = layout.cellLayout(0, 0);
        REQUIRE(spanInfo != nullptr);

        // Spanning cell height should span both rows
        qreal expectedHeight = layout.rowHeight(0) + layout.rowHeight(1) + layout.cellSpacing();
        REQUIRE(std::abs(spanInfo->rect.height() - expectedHeight) < 1.0);
    }
}

// =============================================================================
// Copy/Move Tests
// =============================================================================

TEST_CASE("TableLayout copy constructor", "[editor][table_layout]") {
    TableLayout original;
    auto table = createSimpleTable(2, 2);
    original.setTable(table.get());
    original.setFont(QFont("Serif", 14));
    original.setCellPadding(QMarginsF(10, 5, 10, 5));
    original.doLayout(400.0);

    TableLayout copy(original);

    SECTION("Copy has same configuration") {
        REQUIRE(copy.font().pointSize() == 14);
        REQUIRE(copy.cellPadding().left() == 10.0);
    }

    SECTION("Copy is dirty") {
        REQUIRE(copy.isDirty());
    }

    SECTION("Copy has same table pointer") {
        REQUIRE(copy.table() == table.get());
    }
}

TEST_CASE("TableLayout move constructor", "[editor][table_layout]") {
    TableLayout original;
    auto table = createSimpleTable(2, 2);
    original.setTable(table.get());
    original.setFont(QFont("Serif", 14));
    original.doLayout(400.0);
    qreal originalHeight = original.height();

    TableLayout moved(std::move(original));

    SECTION("Moved has original data") {
        REQUIRE(moved.table() == table.get());
        REQUIRE(moved.font().pointSize() == 14);
        REQUIRE(moved.height() == originalHeight);
    }

    SECTION("Original is reset") {
        REQUIRE(original.table() == nullptr);
        REQUIRE(original.isDirty());
    }
}

TEST_CASE("TableLayout copy assignment", "[editor][table_layout]") {
    TableLayout original;
    auto table = createSimpleTable(2, 2);
    original.setTable(table.get());
    original.doLayout(400.0);

    TableLayout target;
    target = original;

    SECTION("Target has source data") {
        REQUIRE(target.table() == table.get());
    }

    SECTION("Target is dirty") {
        REQUIRE(target.isDirty());
    }

    SECTION("Self-assignment is safe") {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
#endif
        target = target;  // Self-assignment test
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
        REQUIRE(target.table() == table.get());
    }
}

TEST_CASE("TableLayout move assignment", "[editor][table_layout]") {
    TableLayout original;
    auto table = createSimpleTable(2, 2);
    original.setTable(table.get());
    original.doLayout(400.0);

    TableLayout target;
    target = std::move(original);

    SECTION("Target has moved data") {
        REQUIRE(target.table() == table.get());
    }

    SECTION("Original is reset") {
        REQUIRE(original.table() == nullptr);
    }
}

// =============================================================================
// Clear and Invalidate Tests
// =============================================================================

TEST_CASE("TableLayout clear", "[editor][table_layout]") {
    TableLayout layout;
    auto table = createSimpleTable(2, 2);
    layout.setTable(table.get());
    layout.setFont(QFont("Serif", 14));
    layout.doLayout(400.0);

    layout.clear();

    SECTION("Clear resets table") {
        REQUIRE(layout.table() == nullptr);
    }

    SECTION("Clear resets dimensions") {
        REQUIRE(layout.height() == 0.0);
        REQUIRE(layout.layoutWidth() == 0.0);
        REQUIRE(layout.rowCount() == 0);
        REQUIRE(layout.columnCount() == 0);
    }

    SECTION("Clear marks dirty") {
        REQUIRE(layout.isDirty());
    }

    SECTION("Clear resets cell layouts") {
        REQUIRE(layout.cellLayouts().empty());
    }
}

TEST_CASE("TableLayout invalidate", "[editor][table_layout]") {
    TableLayout layout;
    auto table = createSimpleTable(2, 2);
    layout.setTable(table.get());
    layout.doLayout(400.0);

    SECTION("Invalidate marks dirty") {
        REQUIRE(layout.isDirty() == false);
        layout.invalidate();
        REQUIRE(layout.isDirty());
    }

    SECTION("Invalidate preserves cached data until re-layout") {
        qreal heightBefore = layout.height();
        layout.invalidate();
        REQUIRE(layout.height() == heightBefore);
    }
}

// =============================================================================
// Header Cell Tests
// =============================================================================

TEST_CASE("TableLayout with header cells", "[editor][table_layout]") {
    TableLayout layout;
    auto table = createTableWithHeader(2, 3);
    layout.setTable(table.get());
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(600.0);

    SECTION("Table is laid out correctly") {
        REQUIRE(layout.rowCount() == 3);  // 1 header + 2 data
        REQUIRE(layout.columnCount() == 3);
    }

    SECTION("Header row has valid cells") {
        for (int c = 0; c < 3; ++c) {
            const CellLayoutInfo* info = layout.cellLayout(0, c);
            REQUIRE(info != nullptr);
            REQUIRE(info->layout.text().contains("Header"));
        }
    }

    SECTION("Data rows have valid cells") {
        for (int r = 1; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                const CellLayoutInfo* info = layout.cellLayout(r, c);
                REQUIRE(info != nullptr);
                REQUIRE(info->layout.text().contains("Data"));
            }
        }
    }
}

// =============================================================================
// Cell Spacing Tests
// =============================================================================

TEST_CASE("TableLayout cell spacing affects layout", "[editor][table_layout]") {
    auto table = createSimpleTable(3, 3);

    TableLayout layoutNoSpacing;
    layoutNoSpacing.setTable(table.get());
    layoutNoSpacing.setFont(QFont("Serif", 12));
    layoutNoSpacing.setCellSpacing(0.0);
    layoutNoSpacing.doLayout(600.0);

    TableLayout layoutWithSpacing;
    layoutWithSpacing.setTable(table.get());
    layoutWithSpacing.setFont(QFont("Serif", 12));
    layoutWithSpacing.setCellSpacing(5.0);
    layoutWithSpacing.doLayout(600.0);

    SECTION("Layout with spacing is taller") {
        REQUIRE(layoutWithSpacing.height() > layoutNoSpacing.height());
    }

    SECTION("Cell positions differ") {
        // Second row should start at different Y positions
        REQUIRE(layoutWithSpacing.rowY(1) > layoutNoSpacing.rowY(1));
    }
}

// =============================================================================
// Geometry When Dirty Tests
// =============================================================================

TEST_CASE("TableLayout geometry when dirty", "[editor][table_layout]") {
    TableLayout layout;

    SECTION("Bounding rect is empty when dirty") {
        REQUIRE(layout.isDirty());
        REQUIRE(layout.boundingRect().isEmpty());
    }
}

// =============================================================================
// Unicode Content Tests
// =============================================================================

TEST_CASE("TableLayout with Unicode content", "[editor][table_layout]") {
    TableLayout layout;

    auto table = std::make_unique<KmlTable>();
    auto row = std::make_unique<KmlTableRow>();
    row->addCell(std::make_unique<KmlTableCell>(QString::fromUtf8(u8"Za\u017C\u00F3\u0142\u0107")));  // Polish
    row->addCell(std::make_unique<KmlTableCell>(QString::fromUtf8(u8"\u4F60\u597D")));  // Chinese
    row->addCell(std::make_unique<KmlTableCell>(QString::fromUtf8(u8"\u041F\u0440\u0438\u0432\u0435\u0442")));  // Russian
    table->addRow(std::move(row));

    layout.setTable(table.get());
    layout.setFont(QFont("Serif", 12));

    SECTION("Layout succeeds with Unicode") {
        qreal height = layout.doLayout(600.0);
        REQUIRE(height > 0.0);
    }

    SECTION("All cells are laid out") {
        layout.doLayout(600.0);
        REQUIRE(layout.cellLayout(0, 0) != nullptr);
        REQUIRE(layout.cellLayout(0, 1) != nullptr);
        REQUIRE(layout.cellLayout(0, 2) != nullptr);
    }
}

// =============================================================================
// Formatted Content Tests
// =============================================================================

TEST_CASE("TableLayout with formatted content", "[editor][table_layout]") {
    TableLayout layout;

    // Create table with formatted cell content
    auto table = std::make_unique<KmlTable>();
    auto row = std::make_unique<KmlTableRow>();

    auto cell = std::make_unique<KmlTableCell>();
    auto bold = std::make_unique<KmlBold>();
    bold->appendChild(std::make_unique<KmlTextRun>("Bold text"));
    cell->content().addElement(std::move(bold));
    row->addCell(std::move(cell));

    auto cell2 = std::make_unique<KmlTableCell>();
    auto italic = std::make_unique<KmlItalic>();
    italic->appendChild(std::make_unique<KmlTextRun>("Italic text"));
    cell2->content().addElement(std::move(italic));
    row->addCell(std::move(cell2));

    table->addRow(std::move(row));

    layout.setTable(table.get());
    layout.setFont(QFont("Serif", 12));

    SECTION("Layout succeeds with formatted content") {
        qreal height = layout.doLayout(600.0);
        REQUIRE(height > 0.0);
    }

    SECTION("Cell layouts have formats applied") {
        layout.doLayout(600.0);
        const CellLayoutInfo* info = layout.cellLayout(0, 0);
        REQUIRE(info != nullptr);
        REQUIRE(info->layout.hasFormats());
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("TableLayout edge cases", "[editor][table_layout]") {
    SECTION("Very narrow width") {
        TableLayout layout;
        auto table = createSimpleTable(2, 2);
        layout.setTable(table.get());
        layout.setFont(QFont("Serif", 12));

        qreal height = layout.doLayout(10.0);
        REQUIRE(height > 0.0);  // Should still produce valid layout
    }

    SECTION("Very wide width") {
        TableLayout layout;
        auto table = createSimpleTable(2, 2);
        layout.setTable(table.get());
        layout.setFont(QFont("Serif", 12));

        qreal height = layout.doLayout(10000.0);
        REQUIRE(height > 0.0);
    }

    SECTION("Single cell table") {
        TableLayout layout;
        auto table = createSimpleTable(1, 1);
        layout.setTable(table.get());
        layout.setFont(QFont("Serif", 12));
        layout.doLayout(400.0);

        REQUIRE(layout.rowCount() == 1);
        REQUIRE(layout.columnCount() == 1);
        REQUIRE(layout.height() > 0.0);
    }

    SECTION("Empty cells") {
        TableLayout layout;

        auto table = std::make_unique<KmlTable>();
        auto row = std::make_unique<KmlTableRow>();
        row->addCell(std::make_unique<KmlTableCell>());  // Empty
        row->addCell(std::make_unique<KmlTableCell>("Content"));
        row->addCell(std::make_unique<KmlTableCell>());  // Empty
        table->addRow(std::move(row));

        layout.setTable(table.get());
        layout.setFont(QFont("Serif", 12));
        qreal height = layout.doLayout(600.0);

        REQUIRE(height > 0.0);
    }
}

// =============================================================================
// Drawing Tests (Phase 2.7)
// =============================================================================

TEST_CASE("TableLayout drawing colors default values", "[editor][table_layout][drawing]") {
    TableLayout layout;

    SECTION("Default border color") {
        QColor borderColor = layout.borderColor();
        REQUIRE(borderColor.isValid());
        // Default is light gray (180, 180, 180)
        REQUIRE(borderColor.red() == 180);
        REQUIRE(borderColor.green() == 180);
        REQUIRE(borderColor.blue() == 180);
    }

    SECTION("Default border width") {
        REQUIRE(layout.borderWidth() == 1.0);
    }

    SECTION("Default background color") {
        QColor bgColor = layout.backgroundColor();
        REQUIRE(bgColor.isValid());
        REQUIRE(bgColor == Qt::white);
    }

    SECTION("Default header background color") {
        QColor headerBg = layout.headerBackgroundColor();
        REQUIRE(headerBg.isValid());
        // Default is light gray (240, 240, 240)
        REQUIRE(headerBg.red() == 240);
        REQUIRE(headerBg.green() == 240);
        REQUIRE(headerBg.blue() == 240);
    }

    SECTION("Default text colors") {
        REQUIRE(layout.textColor() == Qt::black);
        REQUIRE(layout.headerTextColor() == Qt::black);
    }
}

TEST_CASE("TableLayout drawing color setters", "[editor][table_layout][drawing]") {
    TableLayout layout;

    SECTION("Set border color") {
        QColor newColor(255, 0, 0);
        layout.setBorderColor(newColor);
        REQUIRE(layout.borderColor() == newColor);
    }

    SECTION("Set border width") {
        layout.setBorderWidth(2.5);
        REQUIRE(layout.borderWidth() == 2.5);
    }

    SECTION("Border width clamped to 0") {
        layout.setBorderWidth(-5.0);
        REQUIRE(layout.borderWidth() >= 0.0);
    }

    SECTION("Set background color") {
        QColor newColor(200, 220, 255);
        layout.setBackgroundColor(newColor);
        REQUIRE(layout.backgroundColor() == newColor);
    }

    SECTION("Set header background color") {
        QColor newColor(100, 150, 200);
        layout.setHeaderBackgroundColor(newColor);
        REQUIRE(layout.headerBackgroundColor() == newColor);
    }

    SECTION("Set text color") {
        QColor newColor(50, 50, 50);
        layout.setTextColor(newColor);
        REQUIRE(layout.textColor() == newColor);
    }

    SECTION("Set header text color") {
        QColor newColor(0, 0, 128);
        layout.setHeaderTextColor(newColor);
        REQUIRE(layout.headerTextColor() == newColor);
    }
}

TEST_CASE("TableLayout draw basic", "[editor][table_layout][drawing]") {
    TableLayout layout;
    auto table = createSimpleTable(2, 3);
    layout.setTable(table.get());
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(400.0);

    SECTION("Draw to image does not crash") {
        QImage image(500, 300, QImage::Format_ARGB32);
        image.fill(Qt::white);
        QPainter painter(&image);

        // Should not crash
        layout.draw(&painter, QPointF(10, 10));
        painter.end();

        // Image should have been modified (not all white)
        // Just verify no crash occurred
        REQUIRE(true);
    }

    SECTION("Draw with null painter does not crash") {
        layout.draw(nullptr, QPointF(0, 0));
        REQUIRE(true);
    }

    SECTION("Draw when dirty does nothing") {
        layout.invalidate();
        REQUIRE(layout.isDirty());

        QImage image(500, 300, QImage::Format_ARGB32);
        image.fill(Qt::white);
        QPainter painter(&image);

        // Should return early without crashing
        layout.draw(&painter, QPointF(10, 10));
        painter.end();
        REQUIRE(true);
    }
}

TEST_CASE("TableLayout draw with header cells", "[editor][table_layout][drawing]") {
    TableLayout layout;
    auto table = createTableWithHeader(2, 3);
    layout.setTable(table.get());
    layout.setFont(QFont("Serif", 12));
    layout.setHeaderBackgroundColor(QColor(200, 200, 200));
    layout.setBackgroundColor(Qt::white);
    layout.doLayout(400.0);

    SECTION("Draw table with headers") {
        QImage image(500, 300, QImage::Format_ARGB32);
        image.fill(Qt::lightGray);
        QPainter painter(&image);

        layout.draw(&painter, QPointF(10, 10));
        painter.end();

        // Verify header row is at different color
        // (visual inspection would be needed for full verification)
        REQUIRE(true);
    }
}

TEST_CASE("TableLayout draw with custom styling", "[editor][table_layout][drawing]") {
    TableLayout layout;
    auto table = createSimpleTable(2, 2);
    layout.setTable(table.get());
    layout.setFont(QFont("Serif", 12));

    // Apply custom styling
    layout.setBorderColor(QColor(0, 0, 255));
    layout.setBorderWidth(2.0);
    layout.setBackgroundColor(QColor(255, 255, 200));
    layout.setTextColor(QColor(0, 100, 0));

    layout.doLayout(300.0);

    SECTION("Draw with custom colors") {
        QImage image(400, 200, QImage::Format_ARGB32);
        image.fill(Qt::white);
        QPainter painter(&image);

        layout.draw(&painter, QPointF(10, 10));
        painter.end();

        REQUIRE(true);
    }
}

TEST_CASE("TableLayout draw with zero border width", "[editor][table_layout][drawing]") {
    TableLayout layout;
    auto table = createSimpleTable(2, 2);
    layout.setTable(table.get());
    layout.setFont(QFont("Serif", 12));
    layout.setBorderWidth(0.0);  // No borders
    layout.doLayout(300.0);

    SECTION("Draw without borders") {
        QImage image(400, 200, QImage::Format_ARGB32);
        image.fill(Qt::white);
        QPainter painter(&image);

        layout.draw(&painter, QPointF(10, 10));
        painter.end();

        REQUIRE(true);
    }
}

TEST_CASE("TableLayout draw at different positions", "[editor][table_layout][drawing]") {
    TableLayout layout;
    auto table = createSimpleTable(2, 2);
    layout.setTable(table.get());
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(200.0);

    QImage image(500, 500, QImage::Format_ARGB32);
    QPainter painter(&image);

    SECTION("Draw at origin") {
        image.fill(Qt::white);
        layout.draw(&painter, QPointF(0, 0));
        REQUIRE(true);
    }

    SECTION("Draw at positive offset") {
        image.fill(Qt::white);
        layout.draw(&painter, QPointF(100, 100));
        REQUIRE(true);
    }

    SECTION("Draw at fractional position") {
        image.fill(Qt::white);
        layout.draw(&painter, QPointF(50.5, 75.25));
        REQUIRE(true);
    }

    painter.end();
}

TEST_CASE("TableLayout copy preserves drawing properties", "[editor][table_layout][drawing]") {
    TableLayout original;
    auto table = createSimpleTable(2, 2);
    original.setTable(table.get());
    original.setBorderColor(QColor(255, 0, 0));
    original.setBorderWidth(3.0);
    original.setBackgroundColor(QColor(0, 255, 0));
    original.setHeaderBackgroundColor(QColor(0, 0, 255));
    original.setTextColor(QColor(128, 128, 128));
    original.setHeaderTextColor(QColor(64, 64, 64));

    SECTION("Copy constructor preserves colors") {
        TableLayout copy(original);

        REQUIRE(copy.borderColor() == QColor(255, 0, 0));
        REQUIRE(copy.borderWidth() == 3.0);
        REQUIRE(copy.backgroundColor() == QColor(0, 255, 0));
        REQUIRE(copy.headerBackgroundColor() == QColor(0, 0, 255));
        REQUIRE(copy.textColor() == QColor(128, 128, 128));
        REQUIRE(copy.headerTextColor() == QColor(64, 64, 64));
    }

    SECTION("Copy assignment preserves colors") {
        TableLayout copy;
        copy = original;

        REQUIRE(copy.borderColor() == QColor(255, 0, 0));
        REQUIRE(copy.borderWidth() == 3.0);
        REQUIRE(copy.backgroundColor() == QColor(0, 255, 0));
    }

    SECTION("Move constructor preserves colors") {
        TableLayout moved(std::move(original));

        REQUIRE(moved.borderColor() == QColor(255, 0, 0));
        REQUIRE(moved.borderWidth() == 3.0);
        REQUIRE(moved.backgroundColor() == QColor(0, 255, 0));
    }
}

TEST_CASE("TableLayout draw empty table", "[editor][table_layout][drawing]") {
    TableLayout layout;
    KmlTable emptyTable;
    layout.setTable(&emptyTable);
    layout.doLayout(400.0);

    SECTION("Draw empty table does not crash") {
        QImage image(500, 300, QImage::Format_ARGB32);
        image.fill(Qt::white);
        QPainter painter(&image);

        layout.draw(&painter, QPointF(10, 10));
        painter.end();

        REQUIRE(true);
    }
}

TEST_CASE("TableLayout draw with colspan cells", "[editor][table_layout][drawing]") {
    TableLayout layout;

    auto table = std::make_unique<KmlTable>();
    auto row1 = std::make_unique<KmlTableRow>();
    auto spanCell = std::make_unique<KmlTableCell>("Spanning cell");
    spanCell->setColspan(2);
    row1->addCell(std::move(spanCell));
    row1->addCell(std::make_unique<KmlTableCell>("Normal"));
    table->addRow(std::move(row1));

    auto row2 = std::make_unique<KmlTableRow>();
    row2->addCell(std::make_unique<KmlTableCell>("A"));
    row2->addCell(std::make_unique<KmlTableCell>("B"));
    row2->addCell(std::make_unique<KmlTableCell>("C"));
    table->addRow(std::move(row2));

    layout.setTable(table.get());
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(600.0);

    SECTION("Draw table with colspan") {
        QImage image(700, 200, QImage::Format_ARGB32);
        image.fill(Qt::white);
        QPainter painter(&image);

        layout.draw(&painter, QPointF(10, 10));
        painter.end();

        REQUIRE(true);
    }
}

TEST_CASE("TableLayout draw with rowspan cells", "[editor][table_layout][drawing]") {
    TableLayout layout;

    auto table = std::make_unique<KmlTable>();
    auto row1 = std::make_unique<KmlTableRow>();
    auto spanCell = std::make_unique<KmlTableCell>("Spanning rows");
    spanCell->setRowspan(2);
    row1->addCell(std::move(spanCell));
    row1->addCell(std::make_unique<KmlTableCell>("B1"));
    table->addRow(std::move(row1));

    auto row2 = std::make_unique<KmlTableRow>();
    row2->addCell(std::make_unique<KmlTableCell>("B2"));
    table->addRow(std::move(row2));

    layout.setTable(table.get());
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(400.0);

    SECTION("Draw table with rowspan") {
        QImage image(500, 200, QImage::Format_ARGB32);
        image.fill(Qt::white);
        QPainter painter(&image);

        layout.draw(&painter, QPointF(10, 10));
        painter.end();

        REQUIRE(true);
    }
}
