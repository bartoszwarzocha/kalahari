/// @file table_layout.cpp
/// @brief Implementation of TableLayout (OpenSpec #00042 Phase 2.6/2.7)

#include <kalahari/editor/table_layout.h>
#include <kalahari/editor/format_converter.h>
#include <QFontMetricsF>
#include <algorithm>
#include <cmath>

namespace kalahari::editor {

// =============================================================================
// Constructors / Destructor
// =============================================================================

TableLayout::TableLayout()
    : m_table(nullptr)
    , m_font()
    , m_headerFont()
    , m_hasCustomHeaderFont(false)
    , m_cellPadding(5.0, 3.0, 5.0, 3.0)  // Default padding: left, top, right, bottom
    , m_cellSpacing(1.0)
    , m_columnWidthMode(ColumnWidthMode::Equal)
    , m_minColumnWidth(20.0)
    , m_width(0.0)
    , m_height(0.0)
    , m_dirty(true)
    , m_rowCount(0)
    , m_columnCount(0)
    , m_columnWidths()
    , m_rowHeights()
    , m_columnPositions()
    , m_rowPositions()
    , m_cellLayouts()
    , m_borderColor(QColor(180, 180, 180))      // Default: light gray borders
    , m_borderWidth(1.0)
    , m_backgroundColor(Qt::white)               // Default: white background
    , m_headerBackgroundColor(QColor(240, 240, 240))  // Default: light gray header
    , m_textColor(Qt::black)                     // Default: black text
    , m_headerTextColor(Qt::black)               // Default: black header text
{
}

TableLayout::~TableLayout() = default;

TableLayout::TableLayout(const TableLayout& other)
    : m_table(other.m_table)
    , m_font(other.m_font)
    , m_headerFont(other.m_headerFont)
    , m_hasCustomHeaderFont(other.m_hasCustomHeaderFont)
    , m_cellPadding(other.m_cellPadding)
    , m_cellSpacing(other.m_cellSpacing)
    , m_columnWidthMode(other.m_columnWidthMode)
    , m_minColumnWidth(other.m_minColumnWidth)
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_dirty(true)  // Force re-layout after copy
    , m_rowCount(other.m_rowCount)
    , m_columnCount(other.m_columnCount)
    , m_columnWidths(other.m_columnWidths)
    , m_rowHeights(other.m_rowHeights)
    , m_columnPositions(other.m_columnPositions)
    , m_rowPositions(other.m_rowPositions)
    , m_cellLayouts(other.m_cellLayouts)
    , m_borderColor(other.m_borderColor)
    , m_borderWidth(other.m_borderWidth)
    , m_backgroundColor(other.m_backgroundColor)
    , m_headerBackgroundColor(other.m_headerBackgroundColor)
    , m_textColor(other.m_textColor)
    , m_headerTextColor(other.m_headerTextColor)
{
}

TableLayout::TableLayout(TableLayout&& other) noexcept
    : m_table(other.m_table)
    , m_font(std::move(other.m_font))
    , m_headerFont(std::move(other.m_headerFont))
    , m_hasCustomHeaderFont(other.m_hasCustomHeaderFont)
    , m_cellPadding(other.m_cellPadding)
    , m_cellSpacing(other.m_cellSpacing)
    , m_columnWidthMode(other.m_columnWidthMode)
    , m_minColumnWidth(other.m_minColumnWidth)
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_dirty(other.m_dirty)
    , m_rowCount(other.m_rowCount)
    , m_columnCount(other.m_columnCount)
    , m_columnWidths(std::move(other.m_columnWidths))
    , m_rowHeights(std::move(other.m_rowHeights))
    , m_columnPositions(std::move(other.m_columnPositions))
    , m_rowPositions(std::move(other.m_rowPositions))
    , m_cellLayouts(std::move(other.m_cellLayouts))
    , m_borderColor(std::move(other.m_borderColor))
    , m_borderWidth(other.m_borderWidth)
    , m_backgroundColor(std::move(other.m_backgroundColor))
    , m_headerBackgroundColor(std::move(other.m_headerBackgroundColor))
    , m_textColor(std::move(other.m_textColor))
    , m_headerTextColor(std::move(other.m_headerTextColor))
{
    // Reset other to valid empty state
    other.m_table = nullptr;
    other.m_font = QFont();
    other.m_headerFont = QFont();
    other.m_hasCustomHeaderFont = false;
    other.m_cellPadding = QMarginsF(5.0, 3.0, 5.0, 3.0);
    other.m_cellSpacing = 1.0;
    other.m_columnWidthMode = ColumnWidthMode::Equal;
    other.m_minColumnWidth = 20.0;
    other.m_width = 0.0;
    other.m_height = 0.0;
    other.m_dirty = true;
    other.m_rowCount = 0;
    other.m_columnCount = 0;
    other.m_columnWidths.clear();
    other.m_rowHeights.clear();
    other.m_columnPositions.clear();
    other.m_rowPositions.clear();
    other.m_cellLayouts.clear();
    other.m_borderColor = QColor(180, 180, 180);
    other.m_borderWidth = 1.0;
    other.m_backgroundColor = Qt::white;
    other.m_headerBackgroundColor = QColor(240, 240, 240);
    other.m_textColor = Qt::black;
    other.m_headerTextColor = Qt::black;
}

TableLayout& TableLayout::operator=(const TableLayout& other)
{
    if (this != &other) {
        m_table = other.m_table;
        m_font = other.m_font;
        m_headerFont = other.m_headerFont;
        m_hasCustomHeaderFont = other.m_hasCustomHeaderFont;
        m_cellPadding = other.m_cellPadding;
        m_cellSpacing = other.m_cellSpacing;
        m_columnWidthMode = other.m_columnWidthMode;
        m_minColumnWidth = other.m_minColumnWidth;
        m_width = other.m_width;
        m_height = other.m_height;
        m_dirty = true;  // Force re-layout after copy
        m_rowCount = other.m_rowCount;
        m_columnCount = other.m_columnCount;
        m_columnWidths = other.m_columnWidths;
        m_rowHeights = other.m_rowHeights;
        m_columnPositions = other.m_columnPositions;
        m_rowPositions = other.m_rowPositions;
        m_cellLayouts = other.m_cellLayouts;
        m_borderColor = other.m_borderColor;
        m_borderWidth = other.m_borderWidth;
        m_backgroundColor = other.m_backgroundColor;
        m_headerBackgroundColor = other.m_headerBackgroundColor;
        m_textColor = other.m_textColor;
        m_headerTextColor = other.m_headerTextColor;
    }
    return *this;
}

TableLayout& TableLayout::operator=(TableLayout&& other) noexcept
{
    if (this != &other) {
        m_table = other.m_table;
        m_font = std::move(other.m_font);
        m_headerFont = std::move(other.m_headerFont);
        m_hasCustomHeaderFont = other.m_hasCustomHeaderFont;
        m_cellPadding = other.m_cellPadding;
        m_cellSpacing = other.m_cellSpacing;
        m_columnWidthMode = other.m_columnWidthMode;
        m_minColumnWidth = other.m_minColumnWidth;
        m_width = other.m_width;
        m_height = other.m_height;
        m_dirty = other.m_dirty;
        m_rowCount = other.m_rowCount;
        m_columnCount = other.m_columnCount;
        m_columnWidths = std::move(other.m_columnWidths);
        m_rowHeights = std::move(other.m_rowHeights);
        m_columnPositions = std::move(other.m_columnPositions);
        m_rowPositions = std::move(other.m_rowPositions);
        m_cellLayouts = std::move(other.m_cellLayouts);
        m_borderColor = std::move(other.m_borderColor);
        m_borderWidth = other.m_borderWidth;
        m_backgroundColor = std::move(other.m_backgroundColor);
        m_headerBackgroundColor = std::move(other.m_headerBackgroundColor);
        m_textColor = std::move(other.m_textColor);
        m_headerTextColor = std::move(other.m_headerTextColor);

        // Reset other to valid empty state
        other.m_table = nullptr;
        other.m_font = QFont();
        other.m_headerFont = QFont();
        other.m_hasCustomHeaderFont = false;
        other.m_cellPadding = QMarginsF(5.0, 3.0, 5.0, 3.0);
        other.m_cellSpacing = 1.0;
        other.m_columnWidthMode = ColumnWidthMode::Equal;
        other.m_minColumnWidth = 20.0;
        other.m_width = 0.0;
        other.m_height = 0.0;
        other.m_dirty = true;
        other.m_rowCount = 0;
        other.m_columnCount = 0;
        other.m_columnWidths.clear();
        other.m_rowHeights.clear();
        other.m_columnPositions.clear();
        other.m_rowPositions.clear();
        other.m_cellLayouts.clear();
        other.m_borderColor = QColor(180, 180, 180);
        other.m_borderWidth = 1.0;
        other.m_backgroundColor = Qt::white;
        other.m_headerBackgroundColor = QColor(240, 240, 240);
        other.m_textColor = Qt::black;
        other.m_headerTextColor = Qt::black;
    }
    return *this;
}

// =============================================================================
// Table and Font
// =============================================================================

void TableLayout::setTable(const KmlTable* table)
{
    if (m_table != table) {
        m_table = table;
        invalidate();
    }
}

const KmlTable* TableLayout::table() const
{
    return m_table;
}

void TableLayout::setFont(const QFont& font)
{
    if (m_font != font) {
        m_font = font;
        invalidate();
    }
}

QFont TableLayout::font() const
{
    return m_font;
}

void TableLayout::setHeaderFont(const QFont& font)
{
    m_headerFont = font;
    m_hasCustomHeaderFont = true;
    invalidate();
}

QFont TableLayout::headerFont() const
{
    if (m_hasCustomHeaderFont) {
        return m_headerFont;
    }
    // Default: bold version of regular font
    QFont boldFont = m_font;
    boldFont.setBold(true);
    return boldFont;
}

bool TableLayout::hasCustomHeaderFont() const
{
    return m_hasCustomHeaderFont;
}

// =============================================================================
// Layout Configuration
// =============================================================================

void TableLayout::setCellPadding(const QMarginsF& padding)
{
    if (m_cellPadding != padding) {
        m_cellPadding = padding;
        invalidate();
    }
}

QMarginsF TableLayout::cellPadding() const
{
    return m_cellPadding;
}

void TableLayout::setCellSpacing(qreal spacing)
{
    if (!qFuzzyCompare(m_cellSpacing, spacing)) {
        m_cellSpacing = std::max(0.0, spacing);
        invalidate();
    }
}

qreal TableLayout::cellSpacing() const
{
    return m_cellSpacing;
}

void TableLayout::setColumnWidthMode(ColumnWidthMode mode)
{
    if (m_columnWidthMode != mode) {
        m_columnWidthMode = mode;
        invalidate();
    }
}

ColumnWidthMode TableLayout::columnWidthMode() const
{
    return m_columnWidthMode;
}

void TableLayout::setMinColumnWidth(qreal width)
{
    if (!qFuzzyCompare(m_minColumnWidth, width)) {
        m_minColumnWidth = std::max(1.0, width);
        invalidate();
    }
}

qreal TableLayout::minColumnWidth() const
{
    return m_minColumnWidth;
}

// =============================================================================
// Layout Operations
// =============================================================================

qreal TableLayout::doLayout(qreal width)
{
    // Skip if already laid out at this width and not dirty
    if (!m_dirty && qFuzzyCompare(m_width, width)) {
        return m_height;
    }

    // Handle null or empty table
    if (!m_table || m_table->isEmpty()) {
        m_width = width;
        m_height = 0.0;
        m_rowCount = 0;
        m_columnCount = 0;
        m_columnWidths.clear();
        m_rowHeights.clear();
        m_columnPositions.clear();
        m_rowPositions.clear();
        m_cellLayouts.clear();
        m_dirty = false;
        return m_height;
    }

    m_width = width;

    // Step 1: Initialize cell layouts from table
    initializeCellLayouts();

    // Step 2: Calculate natural content widths
    calculateContentWidths();

    // Step 3: Distribute available width across columns
    qreal totalSpacing = (m_columnCount > 1) ? (m_columnCount - 1) * m_cellSpacing : 0.0;
    qreal availableWidth = width - totalSpacing;
    distributeColumnWidths(availableWidth);

    // Step 4: Layout all cells at their assigned widths
    layoutCells();

    // Step 5: Calculate row heights
    calculateRowHeights();

    // Step 6: Position all cells
    positionCells();

    // Calculate total height
    m_height = 0.0;
    if (!m_rowHeights.empty()) {
        for (qreal h : m_rowHeights) {
            m_height += h;
        }
        if (m_rowCount > 1) {
            m_height += (m_rowCount - 1) * m_cellSpacing;
        }
    }

    m_dirty = false;
    return m_height;
}

qreal TableLayout::layoutWidth() const
{
    return m_width;
}

bool TableLayout::isDirty() const
{
    return m_dirty;
}

void TableLayout::invalidate()
{
    m_dirty = true;
}

void TableLayout::clear()
{
    m_table = nullptr;
    m_font = QFont();
    m_headerFont = QFont();
    m_hasCustomHeaderFont = false;
    m_width = 0.0;
    m_height = 0.0;
    m_dirty = true;
    m_rowCount = 0;
    m_columnCount = 0;
    m_columnWidths.clear();
    m_rowHeights.clear();
    m_columnPositions.clear();
    m_rowPositions.clear();
    m_cellLayouts.clear();
}

// =============================================================================
// Geometry
// =============================================================================

qreal TableLayout::height() const
{
    return m_height;
}

QRectF TableLayout::boundingRect() const
{
    if (m_dirty) {
        return QRectF();
    }
    return QRectF(0, 0, m_width, m_height);
}

int TableLayout::rowCount() const
{
    return m_rowCount;
}

int TableLayout::columnCount() const
{
    return m_columnCount;
}

qreal TableLayout::rowHeight(int rowIndex) const
{
    if (rowIndex < 0 || rowIndex >= static_cast<int>(m_rowHeights.size())) {
        return 0.0;
    }
    return m_rowHeights[rowIndex];
}

qreal TableLayout::columnWidth(int colIndex) const
{
    if (colIndex < 0 || colIndex >= static_cast<int>(m_columnWidths.size())) {
        return 0.0;
    }
    return m_columnWidths[colIndex];
}

qreal TableLayout::rowY(int rowIndex) const
{
    if (rowIndex < 0 || rowIndex >= static_cast<int>(m_rowPositions.size())) {
        return 0.0;
    }
    return m_rowPositions[rowIndex];
}

qreal TableLayout::columnX(int colIndex) const
{
    if (colIndex < 0 || colIndex >= static_cast<int>(m_columnPositions.size())) {
        return 0.0;
    }
    return m_columnPositions[colIndex];
}

const CellLayoutInfo* TableLayout::cellLayout(int row, int column) const
{
    for (const auto& cell : m_cellLayouts) {
        if (cell.row == row && cell.column == column) {
            return &cell;
        }
    }
    return nullptr;
}

CellLayoutInfo* TableLayout::cellLayout(int row, int column)
{
    for (auto& cell : m_cellLayouts) {
        if (cell.row == row && cell.column == column) {
            return &cell;
        }
    }
    return nullptr;
}

const std::vector<CellLayoutInfo>& TableLayout::cellLayouts() const
{
    return m_cellLayouts;
}

// =============================================================================
// Private Methods
// =============================================================================

void TableLayout::initializeCellLayouts()
{
    m_cellLayouts.clear();
    m_rowCount = m_table->rowCount();
    m_columnCount = m_table->columnCount();

    m_columnWidths.assign(m_columnCount, 0.0);
    m_rowHeights.assign(m_rowCount, 0.0);
    m_columnPositions.assign(m_columnCount, 0.0);
    m_rowPositions.assign(m_rowCount, 0.0);

    // Create cell layout info for each cell
    for (int rowIdx = 0; rowIdx < m_rowCount; ++rowIdx) {
        const KmlTableRow* row = m_table->row(rowIdx);
        if (!row) continue;

        int colIdx = 0;
        for (int cellIdx = 0; cellIdx < row->cellCount(); ++cellIdx) {
            const KmlTableCell* cell = row->cell(cellIdx);
            if (!cell) continue;

            CellLayoutInfo info;
            info.row = rowIdx;
            info.column = colIdx;
            info.colspan = cell->colspan();
            info.rowspan = cell->rowspan();

            // Set up paragraph layout with cell content
            const KmlParagraph& content = cell->content();
            info.layout.setText(content.plainText());
            info.layout.setFont(fontForCell(cell));

            // Build format ranges from paragraph content
            QList<QTextLayout::FormatRange> ranges =
                FormatConverter::buildFormatRanges(content, fontForCell(cell));
            if (!ranges.isEmpty()) {
                info.layout.setFormats(ranges);
            }

            m_cellLayouts.push_back(std::move(info));

            // Advance column index by colspan
            colIdx += cell->colspan();
        }
    }
}

void TableLayout::calculateContentWidths()
{
    // Use a large width to get natural content width
    const qreal measureWidth = 10000.0;
    qreal paddingWidth = m_cellPadding.left() + m_cellPadding.right();

    // Track maximum natural width per column
    std::vector<qreal> maxWidths(m_columnCount, m_minColumnWidth);

    for (auto& cellInfo : m_cellLayouts) {
        // Layout with large width to get natural width
        cellInfo.layout.doLayout(measureWidth);

        QRectF bounds = cellInfo.layout.boundingRect();
        cellInfo.contentWidth = bounds.width() + paddingWidth;

        // For cells spanning single column, update max width
        if (cellInfo.colspan == 1) {
            if (cellInfo.column >= 0 && cellInfo.column < m_columnCount) {
                maxWidths[cellInfo.column] = std::max(
                    maxWidths[cellInfo.column],
                    cellInfo.contentWidth
                );
            }
        }
    }

    // Store natural widths for content-based distribution
    m_columnWidths = maxWidths;
}

void TableLayout::distributeColumnWidths(qreal availableWidth)
{
    if (m_columnCount == 0) return;

    switch (m_columnWidthMode) {
        case ColumnWidthMode::Equal: {
            qreal equalWidth = std::max(m_minColumnWidth, availableWidth / m_columnCount);
            m_columnWidths.assign(m_columnCount, equalWidth);
            break;
        }

        case ColumnWidthMode::ContentBased: {
            // Calculate total natural width
            qreal totalNatural = 0.0;
            for (qreal w : m_columnWidths) {
                totalNatural += w;
            }

            if (totalNatural <= 0.0) {
                // Fallback to equal distribution
                qreal equalWidth = std::max(m_minColumnWidth, availableWidth / m_columnCount);
                m_columnWidths.assign(m_columnCount, equalWidth);
            } else if (totalNatural <= availableWidth) {
                // Content fits - use natural widths plus distribute extra space
                qreal extra = availableWidth - totalNatural;
                qreal extraPerColumn = extra / m_columnCount;
                for (qreal& w : m_columnWidths) {
                    w = std::max(m_minColumnWidth, w + extraPerColumn);
                }
            } else {
                // Content doesn't fit - shrink proportionally
                qreal scale = availableWidth / totalNatural;
                for (qreal& w : m_columnWidths) {
                    w = std::max(m_minColumnWidth, w * scale);
                }
            }
            break;
        }

        case ColumnWidthMode::Fixed:
            // Keep current widths (set externally), ensure minimum
            for (qreal& w : m_columnWidths) {
                w = std::max(m_minColumnWidth, w);
            }
            break;
    }
}

void TableLayout::layoutCells()
{
    qreal paddingWidth = m_cellPadding.left() + m_cellPadding.right();

    for (auto& cellInfo : m_cellLayouts) {
        // Calculate available width for this cell's content
        qreal cellWidth = 0.0;

        // Sum up widths of spanned columns
        for (int c = cellInfo.column; c < cellInfo.column + cellInfo.colspan && c < m_columnCount; ++c) {
            cellWidth += m_columnWidths[c];
        }

        // Add spacing between spanned columns
        if (cellInfo.colspan > 1) {
            cellWidth += (cellInfo.colspan - 1) * m_cellSpacing;
        }

        // Subtract padding to get content width
        qreal contentWidth = std::max(1.0, cellWidth - paddingWidth);

        // Layout at the computed width
        cellInfo.layout.invalidate();
        qreal contentHeight = cellInfo.layout.doLayout(contentWidth);
        cellInfo.contentHeight = contentHeight + m_cellPadding.top() + m_cellPadding.bottom();
    }
}

void TableLayout::calculateRowHeights()
{
    m_rowHeights.assign(m_rowCount, 0.0);

    // First pass: consider cells that span only one row
    for (const auto& cellInfo : m_cellLayouts) {
        if (cellInfo.rowspan == 1) {
            if (cellInfo.row >= 0 && cellInfo.row < m_rowCount) {
                m_rowHeights[cellInfo.row] = std::max(
                    m_rowHeights[cellInfo.row],
                    cellInfo.contentHeight
                );
            }
        }
    }

    // Second pass: handle cells that span multiple rows
    for (const auto& cellInfo : m_cellLayouts) {
        if (cellInfo.rowspan > 1) {
            // Calculate current total height of spanned rows
            qreal spanHeight = 0.0;
            int endRow = std::min(cellInfo.row + cellInfo.rowspan, m_rowCount);
            for (int r = cellInfo.row; r < endRow; ++r) {
                spanHeight += m_rowHeights[r];
            }
            // Add spacing between rows
            if (cellInfo.rowspan > 1) {
                spanHeight += (cellInfo.rowspan - 1) * m_cellSpacing;
            }

            // If content is taller, distribute extra height
            if (cellInfo.contentHeight > spanHeight) {
                qreal extra = cellInfo.contentHeight - spanHeight;
                qreal extraPerRow = extra / cellInfo.rowspan;
                for (int r = cellInfo.row; r < endRow; ++r) {
                    m_rowHeights[r] += extraPerRow;
                }
            }
        }
    }

    // Ensure minimum row height
    QFontMetricsF fm(m_font);
    qreal minRowHeight = fm.height() + m_cellPadding.top() + m_cellPadding.bottom();
    for (qreal& h : m_rowHeights) {
        h = std::max(h, minRowHeight);
    }
}

void TableLayout::positionCells()
{
    // Calculate column positions
    qreal x = 0.0;
    for (int c = 0; c < m_columnCount; ++c) {
        m_columnPositions[c] = x;
        x += m_columnWidths[c];
        if (c < m_columnCount - 1) {
            x += m_cellSpacing;
        }
    }

    // Calculate row positions
    qreal y = 0.0;
    for (int r = 0; r < m_rowCount; ++r) {
        m_rowPositions[r] = y;
        y += m_rowHeights[r];
        if (r < m_rowCount - 1) {
            y += m_cellSpacing;
        }
    }

    // Position each cell
    for (auto& cellInfo : m_cellLayouts) {
        qreal cellX = (cellInfo.column >= 0 && cellInfo.column < m_columnCount)
                      ? m_columnPositions[cellInfo.column] : 0.0;
        qreal cellY = (cellInfo.row >= 0 && cellInfo.row < m_rowCount)
                      ? m_rowPositions[cellInfo.row] : 0.0;

        // Calculate cell width (sum of spanned columns + spacing)
        qreal cellWidth = 0.0;
        for (int c = cellInfo.column; c < cellInfo.column + cellInfo.colspan && c < m_columnCount; ++c) {
            cellWidth += m_columnWidths[c];
        }
        if (cellInfo.colspan > 1) {
            cellWidth += (cellInfo.colspan - 1) * m_cellSpacing;
        }

        // Calculate cell height (sum of spanned rows + spacing)
        qreal cellHeight = 0.0;
        int endRow = std::min(cellInfo.row + cellInfo.rowspan, m_rowCount);
        for (int r = cellInfo.row; r < endRow; ++r) {
            cellHeight += m_rowHeights[r];
        }
        if (cellInfo.rowspan > 1) {
            cellHeight += (cellInfo.rowspan - 1) * m_cellSpacing;
        }

        cellInfo.rect = QRectF(cellX, cellY, cellWidth, cellHeight);
    }
}

QFont TableLayout::fontForCell(const KmlTableCell* cell) const
{
    if (cell && cell->isHeader()) {
        return headerFont();
    }
    return m_font;
}

// =============================================================================
// Drawing (Phase 2.7)
// =============================================================================

void TableLayout::draw(QPainter* painter, const QPointF& position)
{
    if (!painter || m_dirty || m_cellLayouts.empty()) {
        return;
    }

    painter->save();

    // Draw in order: backgrounds, borders, content
    drawBackgrounds(painter, position);
    drawBorders(painter, position);
    drawContent(painter, position);

    painter->restore();
}

void TableLayout::setBorderColor(const QColor& color)
{
    m_borderColor = color;
}

QColor TableLayout::borderColor() const
{
    return m_borderColor;
}

void TableLayout::setBorderWidth(qreal width)
{
    m_borderWidth = std::max(0.0, width);
}

qreal TableLayout::borderWidth() const
{
    return m_borderWidth;
}

void TableLayout::setBackgroundColor(const QColor& color)
{
    m_backgroundColor = color;
}

QColor TableLayout::backgroundColor() const
{
    return m_backgroundColor;
}

void TableLayout::setHeaderBackgroundColor(const QColor& color)
{
    m_headerBackgroundColor = color;
}

QColor TableLayout::headerBackgroundColor() const
{
    return m_headerBackgroundColor;
}

void TableLayout::setTextColor(const QColor& color)
{
    m_textColor = color;
}

QColor TableLayout::textColor() const
{
    return m_textColor;
}

void TableLayout::setHeaderTextColor(const QColor& color)
{
    m_headerTextColor = color;
}

QColor TableLayout::headerTextColor() const
{
    return m_headerTextColor;
}

void TableLayout::drawBackgrounds(QPainter* painter, const QPointF& position)
{
    painter->setPen(Qt::NoPen);

    for (const auto& cellInfo : m_cellLayouts) {
        QRectF cellRect = cellInfo.rect.translated(position);

        // Choose background color based on whether it's a header cell
        QColor bgColor = isHeaderCell(cellInfo.row, cellInfo.column)
                         ? m_headerBackgroundColor
                         : m_backgroundColor;

        painter->setBrush(bgColor);
        painter->drawRect(cellRect);
    }
}

void TableLayout::drawBorders(QPainter* painter, const QPointF& position)
{
    if (m_borderWidth <= 0.0 || !m_borderColor.isValid()) {
        return;
    }

    QPen borderPen(m_borderColor);
    borderPen.setWidthF(m_borderWidth);
    painter->setPen(borderPen);
    painter->setBrush(Qt::NoBrush);

    for (const auto& cellInfo : m_cellLayouts) {
        QRectF cellRect = cellInfo.rect.translated(position);
        painter->drawRect(cellRect);
    }
}

void TableLayout::drawContent(QPainter* painter, const QPointF& position)
{
    for (auto& cellInfo : m_cellLayouts) {
        // Calculate content position (with padding)
        QPointF contentPos = position;
        contentPos.rx() += cellInfo.rect.x() + m_cellPadding.left();
        contentPos.ry() += cellInfo.rect.y() + m_cellPadding.top();

        // Set text color based on header status
        QColor textColor = isHeaderCell(cellInfo.row, cellInfo.column)
                           ? m_headerTextColor
                           : m_textColor;

        painter->setPen(textColor);

        // Draw the paragraph layout
        cellInfo.layout.draw(painter, contentPos);
    }
}

bool TableLayout::isHeaderCell(int row, int column) const
{
    if (!m_table) {
        return false;
    }

    if (row < 0 || row >= m_table->rowCount()) {
        return false;
    }

    const KmlTableRow* tableRow = m_table->row(row);
    if (!tableRow) {
        return false;
    }

    // Find the cell at the specified column
    int colIdx = 0;
    for (int cellIdx = 0; cellIdx < tableRow->cellCount(); ++cellIdx) {
        const KmlTableCell* cell = tableRow->cell(cellIdx);
        if (!cell) continue;

        if (colIdx == column) {
            return cell->isHeader();
        }

        // Skip columns covered by colspan
        colIdx += cell->colspan();
        if (colIdx > column) {
            // This cell spans over our target column
            return cell->isHeader();
        }
    }

    return false;
}

}  // namespace kalahari::editor
