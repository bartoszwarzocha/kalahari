/// @file kml_table.cpp
/// @brief Implementation of KML Table elements (OpenSpec #00042 Phase 1.12)

#include <kalahari/editor/kml_table.h>

namespace kalahari::editor {

// =============================================================================
// KmlTableCell Implementation
// =============================================================================

KmlTableCell::KmlTableCell()
    : m_content(std::make_unique<KmlParagraph>())
    , m_colspan(1)
    , m_rowspan(1)
    , m_isHeader(false)
{
}

KmlTableCell::KmlTableCell(const QString& text, bool isHeader)
    : m_content(std::make_unique<KmlParagraph>(text))
    , m_colspan(1)
    , m_rowspan(1)
    , m_isHeader(isHeader)
{
}

KmlTableCell::~KmlTableCell() = default;

KmlTableCell::KmlTableCell(const KmlTableCell& other)
    : m_content(other.m_content ? other.m_content->clone() : std::make_unique<KmlParagraph>())
    , m_colspan(other.m_colspan)
    , m_rowspan(other.m_rowspan)
    , m_isHeader(other.m_isHeader)
{
}

KmlTableCell::KmlTableCell(KmlTableCell&& other) noexcept
    : m_content(std::move(other.m_content))
    , m_colspan(other.m_colspan)
    , m_rowspan(other.m_rowspan)
    , m_isHeader(other.m_isHeader)
{
    other.m_colspan = 1;
    other.m_rowspan = 1;
    other.m_isHeader = false;
}

KmlTableCell& KmlTableCell::operator=(const KmlTableCell& other)
{
    if (this != &other) {
        m_content = other.m_content ? other.m_content->clone() : std::make_unique<KmlParagraph>();
        m_colspan = other.m_colspan;
        m_rowspan = other.m_rowspan;
        m_isHeader = other.m_isHeader;
    }
    return *this;
}

KmlTableCell& KmlTableCell::operator=(KmlTableCell&& other) noexcept
{
    if (this != &other) {
        m_content = std::move(other.m_content);
        m_colspan = other.m_colspan;
        m_rowspan = other.m_rowspan;
        m_isHeader = other.m_isHeader;
        other.m_colspan = 1;
        other.m_rowspan = 1;
        other.m_isHeader = false;
    }
    return *this;
}

bool KmlTableCell::isHeader() const
{
    return m_isHeader;
}

void KmlTableCell::setHeader(bool header)
{
    m_isHeader = header;
}

const KmlParagraph& KmlTableCell::content() const
{
    static KmlParagraph empty;
    return m_content ? *m_content : empty;
}

KmlParagraph& KmlTableCell::content()
{
    if (!m_content) {
        m_content = std::make_unique<KmlParagraph>();
    }
    return *m_content;
}

void KmlTableCell::setContent(std::unique_ptr<KmlParagraph> paragraph)
{
    m_content = std::move(paragraph);
}

QString KmlTableCell::plainText() const
{
    return m_content ? m_content->plainText() : QString();
}

bool KmlTableCell::isEmpty() const
{
    return !m_content || m_content->isEmpty();
}

int KmlTableCell::colspan() const
{
    return m_colspan;
}

void KmlTableCell::setColspan(int span)
{
    m_colspan = (span >= 1) ? span : 1;
}

int KmlTableCell::rowspan() const
{
    return m_rowspan;
}

void KmlTableCell::setRowspan(int span)
{
    m_rowspan = (span >= 1) ? span : 1;
}

bool KmlTableCell::hasSpanning() const
{
    return m_colspan > 1 || m_rowspan > 1;
}

QString KmlTableCell::toKml() const
{
    QString result;
    QString tag = m_isHeader ? QStringLiteral("th") : QStringLiteral("td");

    result += QStringLiteral("<") + tag;

    if (m_colspan > 1) {
        result += QStringLiteral(" colspan=\"") + QString::number(m_colspan) + QStringLiteral("\"");
    }
    if (m_rowspan > 1) {
        result += QStringLiteral(" rowspan=\"") + QString::number(m_rowspan) + QStringLiteral("\"");
    }

    result += QStringLiteral(">");

    // Serialize cell content (just the paragraph content, not <p> tags)
    if (m_content) {
        // Get the inner content of the paragraph (elements only)
        for (const auto& element : m_content->elements()) {
            if (element) {
                result += element->toKml();
            }
        }
    }

    result += QStringLiteral("</") + tag + QStringLiteral(">");

    return result;
}

std::unique_ptr<KmlTableCell> KmlTableCell::clone() const
{
    return std::make_unique<KmlTableCell>(*this);
}

// =============================================================================
// KmlTableRow Implementation
// =============================================================================

KmlTableRow::KmlTableRow()
    : m_cells()
{
}

KmlTableRow::~KmlTableRow() = default;

KmlTableRow::KmlTableRow(const KmlTableRow& other)
    : m_cells()
{
    m_cells.reserve(other.m_cells.size());
    for (const auto& cell : other.m_cells) {
        if (cell) {
            m_cells.push_back(cell->clone());
        }
    }
}

KmlTableRow::KmlTableRow(KmlTableRow&& other) noexcept
    : m_cells(std::move(other.m_cells))
{
}

KmlTableRow& KmlTableRow::operator=(const KmlTableRow& other)
{
    if (this != &other) {
        m_cells.clear();
        m_cells.reserve(other.m_cells.size());
        for (const auto& cell : other.m_cells) {
            if (cell) {
                m_cells.push_back(cell->clone());
            }
        }
    }
    return *this;
}

KmlTableRow& KmlTableRow::operator=(KmlTableRow&& other) noexcept
{
    if (this != &other) {
        m_cells = std::move(other.m_cells);
    }
    return *this;
}

int KmlTableRow::cellCount() const
{
    return static_cast<int>(m_cells.size());
}

const KmlTableCell* KmlTableRow::cell(int index) const
{
    if (index < 0 || static_cast<size_t>(index) >= m_cells.size()) {
        return nullptr;
    }
    return m_cells[static_cast<size_t>(index)].get();
}

KmlTableCell* KmlTableRow::cell(int index)
{
    if (index < 0 || static_cast<size_t>(index) >= m_cells.size()) {
        return nullptr;
    }
    return m_cells[static_cast<size_t>(index)].get();
}

void KmlTableRow::addCell(std::unique_ptr<KmlTableCell> cell)
{
    if (cell) {
        m_cells.push_back(std::move(cell));
    }
}

void KmlTableRow::insertCell(int index, std::unique_ptr<KmlTableCell> cell)
{
    if (cell) {
        if (index < 0) {
            index = 0;
        }
        if (static_cast<size_t>(index) >= m_cells.size()) {
            m_cells.push_back(std::move(cell));
        } else {
            m_cells.insert(m_cells.begin() + index, std::move(cell));
        }
    }
}

std::unique_ptr<KmlTableCell> KmlTableRow::removeCell(int index)
{
    if (index < 0 || static_cast<size_t>(index) >= m_cells.size()) {
        return nullptr;
    }
    auto cell = std::move(m_cells[static_cast<size_t>(index)]);
    m_cells.erase(m_cells.begin() + index);
    return cell;
}

void KmlTableRow::clearCells()
{
    m_cells.clear();
}

const std::vector<std::unique_ptr<KmlTableCell>>& KmlTableRow::cells() const
{
    return m_cells;
}

bool KmlTableRow::isEmpty() const
{
    return m_cells.empty();
}

QString KmlTableRow::toKml() const
{
    QString result = QStringLiteral("<tr>");

    for (const auto& cell : m_cells) {
        if (cell) {
            result += cell->toKml();
        }
    }

    result += QStringLiteral("</tr>");
    return result;
}

std::unique_ptr<KmlTableRow> KmlTableRow::clone() const
{
    return std::make_unique<KmlTableRow>(*this);
}

// =============================================================================
// KmlTable Implementation
// =============================================================================

KmlTable::KmlTable()
    : m_rows()
    , m_styleId()
{
}

KmlTable::~KmlTable() = default;

KmlTable::KmlTable(const KmlTable& other)
    : m_rows()
    , m_styleId(other.m_styleId)
{
    m_rows.reserve(other.m_rows.size());
    for (const auto& row : other.m_rows) {
        if (row) {
            m_rows.push_back(row->clone());
        }
    }
}

KmlTable::KmlTable(KmlTable&& other) noexcept
    : m_rows(std::move(other.m_rows))
    , m_styleId(std::move(other.m_styleId))
{
}

KmlTable& KmlTable::operator=(const KmlTable& other)
{
    if (this != &other) {
        m_rows.clear();
        m_rows.reserve(other.m_rows.size());
        for (const auto& row : other.m_rows) {
            if (row) {
                m_rows.push_back(row->clone());
            }
        }
        m_styleId = other.m_styleId;
    }
    return *this;
}

KmlTable& KmlTable::operator=(KmlTable&& other) noexcept
{
    if (this != &other) {
        m_rows = std::move(other.m_rows);
        m_styleId = std::move(other.m_styleId);
    }
    return *this;
}

int KmlTable::rowCount() const
{
    return static_cast<int>(m_rows.size());
}

const KmlTableRow* KmlTable::row(int index) const
{
    if (index < 0 || static_cast<size_t>(index) >= m_rows.size()) {
        return nullptr;
    }
    return m_rows[static_cast<size_t>(index)].get();
}

KmlTableRow* KmlTable::row(int index)
{
    if (index < 0 || static_cast<size_t>(index) >= m_rows.size()) {
        return nullptr;
    }
    return m_rows[static_cast<size_t>(index)].get();
}

void KmlTable::addRow(std::unique_ptr<KmlTableRow> row)
{
    if (row) {
        m_rows.push_back(std::move(row));
    }
}

void KmlTable::insertRow(int index, std::unique_ptr<KmlTableRow> row)
{
    if (row) {
        if (index < 0) {
            index = 0;
        }
        if (static_cast<size_t>(index) >= m_rows.size()) {
            m_rows.push_back(std::move(row));
        } else {
            m_rows.insert(m_rows.begin() + index, std::move(row));
        }
    }
}

std::unique_ptr<KmlTableRow> KmlTable::removeRow(int index)
{
    if (index < 0 || static_cast<size_t>(index) >= m_rows.size()) {
        return nullptr;
    }
    auto row = std::move(m_rows[static_cast<size_t>(index)]);
    m_rows.erase(m_rows.begin() + index);
    return row;
}

void KmlTable::clearRows()
{
    m_rows.clear();
}

const std::vector<std::unique_ptr<KmlTableRow>>& KmlTable::rows() const
{
    return m_rows;
}

bool KmlTable::isEmpty() const
{
    return m_rows.empty();
}

int KmlTable::columnCount() const
{
    int maxCols = 0;
    for (const auto& row : m_rows) {
        if (row) {
            int colCount = 0;
            for (const auto& cell : row->cells()) {
                if (cell) {
                    colCount += cell->colspan();
                }
            }
            if (colCount > maxCols) {
                maxCols = colCount;
            }
        }
    }
    return maxCols;
}

const KmlTableCell* KmlTable::cellAt(int rowIndex, int colIndex) const
{
    const KmlTableRow* r = row(rowIndex);
    if (!r) {
        return nullptr;
    }

    // Find cell at logical column index (considering colspan)
    int currentCol = 0;
    for (const auto& cell : r->cells()) {
        if (cell) {
            if (colIndex >= currentCol && colIndex < currentCol + cell->colspan()) {
                return cell.get();
            }
            currentCol += cell->colspan();
        }
    }
    return nullptr;
}

KmlTableCell* KmlTable::cellAt(int rowIndex, int colIndex)
{
    KmlTableRow* r = row(rowIndex);
    if (!r) {
        return nullptr;
    }

    // Find cell at logical column index (considering colspan)
    int currentCol = 0;
    for (auto& cell : const_cast<std::vector<std::unique_ptr<KmlTableCell>>&>(r->cells())) {
        if (cell) {
            if (colIndex >= currentCol && colIndex < currentCol + cell->colspan()) {
                return cell.get();
            }
            currentCol += cell->colspan();
        }
    }
    return nullptr;
}

const QString& KmlTable::styleId() const
{
    return m_styleId;
}

void KmlTable::setStyleId(const QString& styleId)
{
    m_styleId = styleId;
}

bool KmlTable::hasStyle() const
{
    return !m_styleId.isEmpty();
}

QString KmlTable::toKml() const
{
    QString result = QStringLiteral("<table");

    if (!m_styleId.isEmpty()) {
        // Escape special characters in style attribute
        QString escapedStyle = m_styleId;
        escapedStyle.replace(QStringLiteral("&"), QStringLiteral("&amp;"));
        escapedStyle.replace(QStringLiteral("\""), QStringLiteral("&quot;"));
        escapedStyle.replace(QStringLiteral("<"), QStringLiteral("&lt;"));
        escapedStyle.replace(QStringLiteral(">"), QStringLiteral("&gt;"));
        result += QStringLiteral(" style=\"") + escapedStyle + QStringLiteral("\"");
    }

    result += QStringLiteral(">");

    for (const auto& row : m_rows) {
        if (row) {
            result += row->toKml();
        }
    }

    result += QStringLiteral("</table>");
    return result;
}

std::unique_ptr<KmlTable> KmlTable::clone() const
{
    return std::make_unique<KmlTable>(*this);
}

}  // namespace kalahari::editor
