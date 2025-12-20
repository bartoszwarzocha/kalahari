/// @file kml_table.h
/// @brief KML Table elements - table, row, cell (OpenSpec #00042 Phase 1.12)
///
/// KML Tables provide structured content organization for documents.
/// Tables consist of rows, which contain cells. Each cell can contain
/// a paragraph with inline formatting.
///
/// Supported elements:
/// - KmlTable (<table>) - Table container
/// - KmlTableRow (<tr>) - Table row
/// - KmlTableCell (<td>, <th>) - Table cell (data or header)
///
/// Example KML:
/// @code
/// <table>
///   <tr>
///     <th>Header 1</th>
///     <th>Header 2</th>
///   </tr>
///   <tr>
///     <td>Cell 1</td>
///     <td colspan="2">Spanning cell</td>
///   </tr>
/// </table>
/// @endcode

#pragma once

#include <kalahari/editor/kml_paragraph.h>
#include <vector>
#include <memory>
#include <QString>

namespace kalahari::editor {

// Forward declarations
class KmlTableRow;
class KmlTableCell;

/// @brief Table cell element (<td> or <th>)
///
/// Represents a single cell in a table row. Cells can contain a paragraph
/// with inline formatting. Cells support colspan and rowspan attributes
/// for spanning multiple columns or rows.
///
/// Example KML:
/// @code
/// <td>Simple cell content</td>
/// <td colspan="2">Spanning two columns</td>
/// <th rowspan="3">Header spanning rows</th>
/// @endcode
class KmlTableCell {
public:
    /// @brief Construct an empty data cell
    KmlTableCell();

    /// @brief Construct a cell with text content
    /// @param text Initial text content
    /// @param isHeader True for header cell (<th>), false for data cell (<td>)
    explicit KmlTableCell(const QString& text, bool isHeader = false);

    /// @brief Destructor
    ~KmlTableCell();

    /// @brief Copy constructor
    KmlTableCell(const KmlTableCell& other);

    /// @brief Move constructor
    KmlTableCell(KmlTableCell&& other) noexcept;

    /// @brief Copy assignment
    KmlTableCell& operator=(const KmlTableCell& other);

    /// @brief Move assignment
    KmlTableCell& operator=(KmlTableCell&& other) noexcept;

    // =========================================================================
    // Cell type
    // =========================================================================

    /// @brief Check if this is a header cell (<th>)
    /// @return True for header cell, false for data cell
    bool isHeader() const;

    /// @brief Set whether this is a header cell
    /// @param header True for header cell (<th>), false for data cell (<td>)
    void setHeader(bool header);

    // =========================================================================
    // Content methods
    // =========================================================================

    /// @brief Get the cell content paragraph
    /// @return Const reference to the paragraph
    const KmlParagraph& content() const;

    /// @brief Get the cell content paragraph (mutable)
    /// @return Mutable reference to the paragraph
    KmlParagraph& content();

    /// @brief Set the cell content
    /// @param paragraph The new content (ownership transferred)
    void setContent(std::unique_ptr<KmlParagraph> paragraph);

    /// @brief Get plain text content
    /// @return Plain text from the paragraph
    QString plainText() const;

    /// @brief Check if the cell is empty
    /// @return True if the cell has no content
    bool isEmpty() const;

    // =========================================================================
    // Spanning attributes
    // =========================================================================

    /// @brief Get the column span
    /// @return Number of columns this cell spans (default: 1)
    int colspan() const;

    /// @brief Set the column span
    /// @param span Number of columns to span (must be >= 1)
    void setColspan(int span);

    /// @brief Get the row span
    /// @return Number of rows this cell spans (default: 1)
    int rowspan() const;

    /// @brief Set the row span
    /// @param span Number of rows to span (must be >= 1)
    void setRowspan(int span);

    /// @brief Check if this cell has spanning attributes
    /// @return True if colspan > 1 or rowspan > 1
    bool hasSpanning() const;

    // =========================================================================
    // Serialization
    // =========================================================================

    /// @brief Serialize this cell to KML format
    /// @return QString containing valid KML markup (<td> or <th>)
    QString toKml() const;

    /// @brief Create a deep copy of this cell
    /// @return unique_ptr to a new cell with same content
    std::unique_ptr<KmlTableCell> clone() const;

private:
    std::unique_ptr<KmlParagraph> m_content;  ///< Cell content
    int m_colspan;                            ///< Column span (default: 1)
    int m_rowspan;                            ///< Row span (default: 1)
    bool m_isHeader;                          ///< True if header cell (<th>)
};

/// @brief Table row element (<tr>)
///
/// Represents a single row in a table. Rows contain cells.
///
/// Example KML:
/// @code
/// <tr>
///   <td>Cell 1</td>
///   <td>Cell 2</td>
/// </tr>
/// @endcode
class KmlTableRow {
public:
    /// @brief Construct an empty row
    KmlTableRow();

    /// @brief Destructor
    ~KmlTableRow();

    /// @brief Copy constructor
    KmlTableRow(const KmlTableRow& other);

    /// @brief Move constructor
    KmlTableRow(KmlTableRow&& other) noexcept;

    /// @brief Copy assignment
    KmlTableRow& operator=(const KmlTableRow& other);

    /// @brief Move assignment
    KmlTableRow& operator=(KmlTableRow&& other) noexcept;

    // =========================================================================
    // Cell container methods
    // =========================================================================

    /// @brief Get the number of cells in this row
    /// @return Number of cells
    int cellCount() const;

    /// @brief Get a cell by index
    /// @param index The index (0-based)
    /// @return Pointer to the cell, or nullptr if index out of range
    const KmlTableCell* cell(int index) const;

    /// @brief Get a mutable cell by index
    /// @param index The index (0-based)
    /// @return Pointer to the cell, or nullptr if index out of range
    KmlTableCell* cell(int index);

    /// @brief Add a cell to the end of the row
    /// @param cell The cell to add (ownership transferred)
    void addCell(std::unique_ptr<KmlTableCell> cell);

    /// @brief Insert a cell at a specific index
    /// @param index The insertion position
    /// @param cell The cell to insert (ownership transferred)
    void insertCell(int index, std::unique_ptr<KmlTableCell> cell);

    /// @brief Remove a cell by index
    /// @param index The index to remove
    /// @return The removed cell, or nullptr if index out of range
    std::unique_ptr<KmlTableCell> removeCell(int index);

    /// @brief Remove all cells
    void clearCells();

    /// @brief Get direct access to cells (for iteration)
    /// @return Const reference to the cell vector
    const std::vector<std::unique_ptr<KmlTableCell>>& cells() const;

    /// @brief Check if the row is empty (no cells)
    /// @return True if no cells
    bool isEmpty() const;

    // =========================================================================
    // Serialization
    // =========================================================================

    /// @brief Serialize this row to KML format
    /// @return QString containing valid KML markup
    QString toKml() const;

    /// @brief Create a deep copy of this row
    /// @return unique_ptr to a new row with same content
    std::unique_ptr<KmlTableRow> clone() const;

private:
    std::vector<std::unique_ptr<KmlTableCell>> m_cells;  ///< Cell storage
};

/// @brief Table element (<table>)
///
/// Represents a complete table structure. Tables contain rows,
/// which contain cells.
///
/// Example KML:
/// @code
/// <table>
///   <tr>
///     <th>Name</th>
///     <th>Age</th>
///   </tr>
///   <tr>
///     <td>Alice</td>
///     <td>25</td>
///   </tr>
/// </table>
/// @endcode
class KmlTable {
public:
    /// @brief Construct an empty table
    KmlTable();

    /// @brief Destructor
    ~KmlTable();

    /// @brief Copy constructor
    KmlTable(const KmlTable& other);

    /// @brief Move constructor
    KmlTable(KmlTable&& other) noexcept;

    /// @brief Copy assignment
    KmlTable& operator=(const KmlTable& other);

    /// @brief Move assignment
    KmlTable& operator=(KmlTable&& other) noexcept;

    // =========================================================================
    // Row container methods
    // =========================================================================

    /// @brief Get the number of rows in this table
    /// @return Number of rows
    int rowCount() const;

    /// @brief Get a row by index
    /// @param index The index (0-based)
    /// @return Pointer to the row, or nullptr if index out of range
    const KmlTableRow* row(int index) const;

    /// @brief Get a mutable row by index
    /// @param index The index (0-based)
    /// @return Pointer to the row, or nullptr if index out of range
    KmlTableRow* row(int index);

    /// @brief Add a row to the end of the table
    /// @param row The row to add (ownership transferred)
    void addRow(std::unique_ptr<KmlTableRow> row);

    /// @brief Insert a row at a specific index
    /// @param index The insertion position
    /// @param row The row to insert (ownership transferred)
    void insertRow(int index, std::unique_ptr<KmlTableRow> row);

    /// @brief Remove a row by index
    /// @param index The index to remove
    /// @return The removed row, or nullptr if index out of range
    std::unique_ptr<KmlTableRow> removeRow(int index);

    /// @brief Remove all rows
    void clearRows();

    /// @brief Get direct access to rows (for iteration)
    /// @return Const reference to the row vector
    const std::vector<std::unique_ptr<KmlTableRow>>& rows() const;

    /// @brief Check if the table is empty (no rows)
    /// @return True if no rows
    bool isEmpty() const;

    // =========================================================================
    // Table metrics
    // =========================================================================

    /// @brief Get the maximum number of columns in any row
    /// @return Maximum column count (considering colspan)
    int columnCount() const;

    /// @brief Get a cell at specified row and column
    /// @param rowIndex Row index (0-based)
    /// @param colIndex Column index (0-based)
    /// @return Pointer to the cell, or nullptr if out of range
    const KmlTableCell* cellAt(int rowIndex, int colIndex) const;

    /// @brief Get a mutable cell at specified row and column
    /// @param rowIndex Row index (0-based)
    /// @param colIndex Column index (0-based)
    /// @return Pointer to the cell, or nullptr if out of range
    KmlTableCell* cellAt(int rowIndex, int colIndex);

    // =========================================================================
    // Style methods
    // =========================================================================

    /// @brief Get the table style ID
    /// @return The style ID (empty string for default style)
    const QString& styleId() const;

    /// @brief Set the table style ID
    /// @param styleId The new style ID (empty for default)
    void setStyleId(const QString& styleId);

    /// @brief Check if this table has a custom style
    /// @return True if styleId is not empty
    bool hasStyle() const;

    // =========================================================================
    // Serialization
    // =========================================================================

    /// @brief Serialize this table to KML format
    /// @return QString containing valid KML markup
    QString toKml() const;

    /// @brief Create a deep copy of this table
    /// @return unique_ptr to a new table with same content
    std::unique_ptr<KmlTable> clone() const;

private:
    std::vector<std::unique_ptr<KmlTableRow>> m_rows;  ///< Row storage
    QString m_styleId;                                 ///< Table style ID
};

}  // namespace kalahari::editor
