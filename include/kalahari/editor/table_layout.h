/// @file table_layout.h
/// @brief Table layout engine for KmlTable (OpenSpec #00042 Phase 2.6/2.7)
///
/// TableLayout manages the layout of a KmlTable, computing cell sizes,
/// column widths, and row heights. Each cell contains a ParagraphLayout
/// for its content.
///
/// Key responsibilities:
/// - Cell size calculation based on content
/// - Column width distribution (equal or content-based)
/// - Row height calculation (based on tallest cell)
/// - Layout coordination across all cells
/// - Support for colspan/rowspan (basic)
/// - Drawing with cell borders, backgrounds, and content (Phase 2.7)

#pragma once

#include <kalahari/editor/kml_table.h>
#include <kalahari/editor/paragraph_layout.h>
#include <QFont>
#include <QRectF>
#include <QMarginsF>
#include <QColor>
#include <QPainter>
#include <vector>
#include <memory>

namespace kalahari::editor {

/// @brief Layout information for a single table cell
struct CellLayoutInfo {
    int row;                    ///< Row index (0-based)
    int column;                 ///< Column index (0-based)
    int colspan;                ///< Number of columns spanned
    int rowspan;                ///< Number of rows spanned
    QRectF rect;                ///< Bounding rectangle (position + size)
    qreal contentWidth;         ///< Natural content width (without constraints)
    qreal contentHeight;        ///< Natural content height (at current column width)
    ParagraphLayout layout;     ///< Layout for cell content

    CellLayoutInfo()
        : row(0), column(0), colspan(1), rowspan(1)
        , rect(), contentWidth(0), contentHeight(0), layout() {}
};

/// @brief Column width distribution strategy
enum class ColumnWidthMode {
    Equal,          ///< All columns have equal width
    ContentBased,   ///< Width based on content (proportional)
    Fixed           ///< Explicit fixed widths
};

/// @brief Table layout engine wrapping KmlTable with cell layouts
///
/// TableLayout manages the complete layout of a table including:
/// - Computing natural content widths for all cells
/// - Distributing available width across columns
/// - Computing row heights based on cell content
/// - Managing ParagraphLayout instances for each cell
///
/// Usage:
/// @code
/// KmlTable table;
/// // ... populate table ...
///
/// TableLayout layout;
/// layout.setTable(&table);
/// layout.setFont(QFont("Serif", 12));
/// layout.setCellPadding(QMarginsF(5, 3, 5, 3));
/// qreal height = layout.doLayout(800.0);  // Layout at 800px width
/// @endcode
///
/// Thread safety: Not thread-safe. Use from GUI thread only.
class TableLayout {
public:
    /// @brief Construct an empty table layout
    TableLayout();

    /// @brief Destructor
    ~TableLayout();

    /// @brief Copy constructor
    TableLayout(const TableLayout& other);

    /// @brief Move constructor
    TableLayout(TableLayout&& other) noexcept;

    /// @brief Copy assignment
    TableLayout& operator=(const TableLayout& other);

    /// @brief Move assignment
    TableLayout& operator=(TableLayout&& other) noexcept;

    // =========================================================================
    // Table and Font
    // =========================================================================

    /// @brief Set the table to layout
    /// @param table Pointer to the table (not owned, must outlive layout)
    /// @note Marks the layout as dirty
    void setTable(const KmlTable* table);

    /// @brief Get the current table
    /// @return Pointer to the table, or nullptr if not set
    const KmlTable* table() const;

    /// @brief Set the font for all cells
    /// @param font The font to use
    /// @note Marks the layout as dirty
    void setFont(const QFont& font);

    /// @brief Get the current font
    /// @return The font used for layout
    QFont font() const;

    /// @brief Set font for header cells (optional)
    /// @param font The font for header cells
    /// @note If not set, uses regular font with bold weight
    void setHeaderFont(const QFont& font);

    /// @brief Get header font
    QFont headerFont() const;

    /// @brief Check if custom header font is set
    bool hasCustomHeaderFont() const;

    // =========================================================================
    // Layout Configuration
    // =========================================================================

    /// @brief Set cell padding (internal margins)
    /// @param padding Margins applied inside each cell
    /// @note Marks the layout as dirty
    void setCellPadding(const QMarginsF& padding);

    /// @brief Get cell padding
    /// @return Current cell padding margins
    QMarginsF cellPadding() const;

    /// @brief Set cell spacing (gap between cells)
    /// @param spacing Space between adjacent cells
    /// @note Marks the layout as dirty
    void setCellSpacing(qreal spacing);

    /// @brief Get cell spacing
    qreal cellSpacing() const;

    /// @brief Set column width distribution mode
    /// @param mode The distribution strategy
    void setColumnWidthMode(ColumnWidthMode mode);

    /// @brief Get column width mode
    ColumnWidthMode columnWidthMode() const;

    /// @brief Set minimum column width
    /// @param width Minimum width for any column
    void setMinColumnWidth(qreal width);

    /// @brief Get minimum column width
    qreal minColumnWidth() const;

    // =========================================================================
    // Layout Operations
    // =========================================================================

    /// @brief Perform the layout at a given width
    /// @param width The available width for the table
    /// @return The total height of the laid-out table
    /// @note If not dirty and width matches, returns cached height
    qreal doLayout(qreal width);

    /// @brief Get the width used for the last layout
    /// @return The width passed to the last doLayout() call, or 0 if not laid out
    qreal layoutWidth() const;

    /// @brief Check if layout needs to be recalculated
    /// @return true if table or configuration changed since last doLayout()
    bool isDirty() const;

    /// @brief Mark the layout as needing recalculation
    void invalidate();

    /// @brief Clear the layout and reset to empty state
    void clear();

    // =========================================================================
    // Geometry
    // =========================================================================

    /// @brief Get the total height of the laid-out table
    /// @return The total height, or 0 if not laid out
    qreal height() const;

    /// @brief Get the bounding rectangle of the table
    /// @return The bounding rect (origin at 0,0)
    QRectF boundingRect() const;

    /// @brief Get the number of rows
    int rowCount() const;

    /// @brief Get the number of columns
    int columnCount() const;

    /// @brief Get the height of a specific row
    /// @param rowIndex Row index (0-based)
    /// @return Row height, or 0 if invalid index
    qreal rowHeight(int rowIndex) const;

    /// @brief Get the width of a specific column
    /// @param colIndex Column index (0-based)
    /// @return Column width, or 0 if invalid index
    qreal columnWidth(int colIndex) const;

    /// @brief Get the Y position of a row
    /// @param rowIndex Row index (0-based)
    /// @return Y coordinate of row top, or 0 if invalid
    qreal rowY(int rowIndex) const;

    /// @brief Get the X position of a column
    /// @param colIndex Column index (0-based)
    /// @return X coordinate of column left, or 0 if invalid
    qreal columnX(int colIndex) const;

    /// @brief Get layout info for a cell at row/column
    /// @param row Row index (0-based)
    /// @param column Column index (0-based)
    /// @return Pointer to cell layout info, or nullptr if not found
    const CellLayoutInfo* cellLayout(int row, int column) const;

    /// @brief Get mutable layout info for a cell
    /// @param row Row index (0-based)
    /// @param column Column index (0-based)
    /// @return Pointer to cell layout info, or nullptr if not found
    CellLayoutInfo* cellLayout(int row, int column);

    /// @brief Get all cell layout infos
    /// @return Const reference to cell layout vector
    const std::vector<CellLayoutInfo>& cellLayouts() const;

    // =========================================================================
    // Drawing (Phase 2.7)
    // =========================================================================

    /// @brief Draw the table at the specified position
    /// @param painter The painter to draw with
    /// @param position Top-left position for drawing
    ///
    /// Draws the complete table including:
    /// - Cell backgrounds (with distinct header background)
    /// - Cell borders
    /// - Cell content (text)
    ///
    /// The painter should have appropriate clip rect set if needed.
    /// Drawing respects the painter's current transform.
    ///
    /// Example:
    /// @code
    /// TableLayout layout;
    /// layout.setTable(&table);
    /// layout.doLayout(500.0);
    /// layout.draw(&painter, QPointF(10, 20));
    /// @endcode
    void draw(QPainter* painter, const QPointF& position);

    /// @brief Set the border color for cell borders
    /// @param color The color for cell borders
    void setBorderColor(const QColor& color);

    /// @brief Get the border color
    /// @return Current border color
    QColor borderColor() const;

    /// @brief Set the border width
    /// @param width Width of cell borders in pixels
    void setBorderWidth(qreal width);

    /// @brief Get the border width
    /// @return Current border width
    qreal borderWidth() const;

    /// @brief Set the default background color for data cells
    /// @param color Background color for non-header cells
    void setBackgroundColor(const QColor& color);

    /// @brief Get the default background color
    /// @return Current background color for data cells
    QColor backgroundColor() const;

    /// @brief Set the background color for header cells
    /// @param color Background color for header cells
    void setHeaderBackgroundColor(const QColor& color);

    /// @brief Get the header background color
    /// @return Current background color for header cells
    QColor headerBackgroundColor() const;

    /// @brief Set the text color for data cells
    /// @param color Text color for non-header cells
    void setTextColor(const QColor& color);

    /// @brief Get the text color for data cells
    /// @return Current text color for data cells
    QColor textColor() const;

    /// @brief Set the text color for header cells
    /// @param color Text color for header cells
    void setHeaderTextColor(const QColor& color);

    /// @brief Get the header text color
    /// @return Current text color for header cells
    QColor headerTextColor() const;

private:
    /// @brief Initialize cell layouts from table
    void initializeCellLayouts();

    /// @brief Calculate natural content widths for all cells
    void calculateContentWidths();

    /// @brief Distribute available width across columns
    /// @param availableWidth Total width available for columns
    void distributeColumnWidths(qreal availableWidth);

    /// @brief Layout all cells at their assigned widths
    void layoutCells();

    /// @brief Calculate row heights based on cell content
    void calculateRowHeights();

    /// @brief Position all cells
    void positionCells();

    /// @brief Get effective font for a cell
    QFont fontForCell(const KmlTableCell* cell) const;

    /// @brief Draw cell backgrounds
    /// @param painter The painter
    /// @param position Drawing offset
    void drawBackgrounds(QPainter* painter, const QPointF& position);

    /// @brief Draw cell borders
    /// @param painter The painter
    /// @param position Drawing offset
    void drawBorders(QPainter* painter, const QPointF& position);

    /// @brief Draw cell content (text)
    /// @param painter The painter
    /// @param position Drawing offset
    void drawContent(QPainter* painter, const QPointF& position);

    /// @brief Check if a cell is a header cell
    /// @param row Row index
    /// @param column Column index
    /// @return true if the cell is a header
    bool isHeaderCell(int row, int column) const;

    const KmlTable* m_table;                    ///< Table being laid out (not owned)
    QFont m_font;                               ///< Default cell font
    QFont m_headerFont;                         ///< Header cell font
    bool m_hasCustomHeaderFont;                 ///< Whether custom header font is set
    QMarginsF m_cellPadding;                    ///< Cell internal padding
    qreal m_cellSpacing;                        ///< Gap between cells
    ColumnWidthMode m_columnWidthMode;          ///< Width distribution strategy
    qreal m_minColumnWidth;                     ///< Minimum column width

    qreal m_width;                              ///< Layout width
    qreal m_height;                             ///< Computed table height
    bool m_dirty;                               ///< Whether layout needs recalculation

    int m_rowCount;                             ///< Number of rows
    int m_columnCount;                          ///< Number of columns
    std::vector<qreal> m_columnWidths;          ///< Width of each column
    std::vector<qreal> m_rowHeights;            ///< Height of each row
    std::vector<qreal> m_columnPositions;       ///< X position of each column
    std::vector<qreal> m_rowPositions;          ///< Y position of each row
    std::vector<CellLayoutInfo> m_cellLayouts;  ///< Layout info for each cell

    // Drawing properties (Phase 2.7)
    QColor m_borderColor;                       ///< Color for cell borders
    qreal m_borderWidth;                        ///< Width of cell borders
    QColor m_backgroundColor;                   ///< Background color for data cells
    QColor m_headerBackgroundColor;             ///< Background color for header cells
    QColor m_textColor;                         ///< Text color for data cells
    QColor m_headerTextColor;                   ///< Text color for header cells
};

}  // namespace kalahari::editor
