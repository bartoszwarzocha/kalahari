/// @file character_grid.cpp
/// @brief The characters of Insert > Special Character... and of the quick palette in cells,
///        in the font of the text they go into

#include "kalahari/gui/widgets/character_grid.h"

#include <QApplication>
#include <QFontMetrics>
#include <QPainter>
#include <QRawFont>
#include <QStyle>
#include <QStyledItemDelegate>

#include <cmath>

namespace kalahari {
namespace gui {

namespace {

constexpr qreal CELL_SCALE = 1.6;   ///< The characters of a cell against the window's font
constexpr qreal CELL_SIDE = 1.7;    ///< The side of a cell, in heights of its font
constexpr qreal GLYPH_SCALE = 2.8;  ///< A character shown large
constexpr qreal MARK_SCALE = 0.9;   ///< The marks of the characters that cannot be seen

constexpr int MISSING_ROLE = Qt::UserRole + 1;  ///< The font of the text has it not
constexpr int MARK_ROLE = Qt::UserRole + 2;     ///< A mark shown for a character not seen

} // namespace

// ============================================================================
// CharacterFonts
// ============================================================================

void CharacterFonts::setFamily(const QString& family, const QFont& base)
{
    m_family = family;
    m_cell = family.isEmpty() ? base : QFont(family);
    m_cell.setPointSizeF(base.pointSizeF() * CELL_SCALE);
    m_glyph = m_cell;
    m_glyph.setPointSizeF(base.pointSizeF() * GLYPH_SCALE);
    m_mark = base;
    m_mark.setPointSizeF(base.pointSizeF() * MARK_SCALE);
    m_has.clear();
}

int CharacterFonts::cellSide() const
{
    return static_cast<int>(std::ceil(QFontMetrics(m_cell).height() * CELL_SIDE));
}

bool CharacterFonts::has(char32_t code) const
{
    if (m_family.isEmpty() || code == 0) {
        return true;
    }
    const auto known = m_has.constFind(code);
    if (known != m_has.constEnd()) {
        return *known;
    }
    // The font itself, not the fonts the system would show the character in
    QFont font(m_family);
    font.setStyleStrategy(QFont::NoFontMerging);
    const bool has = QRawFont::fromFont(font).supportsCharacter(static_cast<uint>(code));
    m_has.insert(code, has);
    return has;
}

QString shownCharacterText(const SpecialCharacter& character)
{
    return character.mark.isEmpty() ? character.text() : character.mark;
}

// ============================================================================
// CharacterCellDelegate
// ============================================================================

/// @brief Draws a character in the middle of its cell, in the font of the grid: greyed out
///        where the font of the text has it not, a mark small in a dotted frame
class CharacterCellDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void setCellSize(const QSize& size) { m_cell = size; }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        return m_cell.isValid() ? m_cell : QStyledItemDelegate::sizeHint(option, index);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        QStyleOptionViewItem cell = option;
        initStyleOption(&cell, index);
        const QWidget* widget = cell.widget;
        const QStyle* style = widget != nullptr ? widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &cell, painter, widget);

        const bool selected = cell.state.testFlag(QStyle::State_Selected);
        const QPalette::ColorGroup group =
            !cell.state.testFlag(QStyle::State_Enabled) ? QPalette::Disabled
            : cell.state.testFlag(QStyle::State_Active) ? QPalette::Normal
                                                        : QPalette::Inactive;
        QColor color = cell.palette.color(group, selected ? QPalette::HighlightedText
                                                          : QPalette::Text);
        const bool mark = index.data(MARK_ROLE).toBool();
        if (!selected && (mark || index.data(MISSING_ROLE).toBool())) {
            color = cell.palette.color(group, QPalette::PlaceholderText);
        }

        painter->save();
        painter->setPen(color);
        painter->setFont(cell.font);
        if (mark) {
            const QFontMetrics metrics(cell.font);
            QRect frame = metrics.boundingRect(cell.text).adjusted(-3, -2, 3, 2);
            frame.moveCenter(cell.rect.center());
            painter->setPen(QPen(color, 1, Qt::DotLine));
            painter->drawRect(frame);
            painter->setPen(color);
        }
        painter->drawText(cell.rect, Qt::AlignCenter, cell.text);
        painter->restore();

        if (cell.state.testFlag(QStyle::State_HasFocus)) {
            QStyleOptionFocusRect focus;
            focus.QStyleOption::operator=(cell);
            focus.rect = cell.rect.adjusted(1, 1, -1, -1);
            focus.backgroundColor = cell.palette.color(
                group, selected ? QPalette::Highlight : QPalette::Base);
            style->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, painter, widget);
        }
    }

private:
    QSize m_cell;
};

// ============================================================================
// CharacterGrid
// ============================================================================

CharacterGrid::CharacterGrid(int columns, int rows, QWidget* parent)
    : QListWidget(parent)
    , m_columns(columns)
    , m_rows(rows)
    , m_minimumColumns(1)
    , m_minimumRows(1)
{
    const bool oneRow = rows == 1;
    setViewMode(QListView::IconMode);
    setFlow(QListView::LeftToRight);
    setWrapping(!oneRow);
    setResizeMode(QListView::Adjust);
    setMovement(QListView::Static);
    setDragDropMode(QAbstractItemView::NoDragDrop);
    setDragEnabled(false);
    setUniformItemSizes(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSpacing(0);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(oneRow ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    m_delegate = new CharacterCellDelegate(this);
    setItemDelegate(m_delegate);
}

void CharacterGrid::setMinimumCells(int columns, int rows)
{
    m_minimumColumns = columns;
    m_minimumRows = rows;
    updateGeometry();
}

void CharacterGrid::setCellSide(int side)
{
    m_cell = QSize(side, side);
    m_delegate->setCellSize(m_cell);
    setGridSize(m_cell);
    if (m_rows == 1) {
        setFixedHeight(m_cell.height() + 2 * frameWidth());
    }
    updateGeometry();
}

void CharacterGrid::setCharacters(const QList<char32_t>& codes,
                                  const SpecialCharacters& characters,
                                  const CharacterFonts& fonts,
                                  const std::function<QString(char32_t)>& toolTipOf)
{
    const QSignalBlocker blocker(this);
    clear();
    for (const char32_t code : codes) {
        const SpecialCharacter character = characters.character(code);
        const bool mark = !character.mark.isEmpty();
        auto* item = new QListWidgetItem(shownCharacterText(character), this);
        item->setData(CODE_ROLE, static_cast<uint>(code));
        item->setData(MARK_ROLE, mark);
        item->setData(MISSING_ROLE, !fonts.has(code));
        item->setData(Qt::FontRole, mark ? fonts.mark() : fonts.cell());
        item->setData(Qt::AccessibleTextRole, character.name);
        if (toolTipOf) {
            item->setToolTip(toolTipOf(code));
        }
    }
}

QList<char32_t> CharacterGrid::characters() const
{
    QList<char32_t> codes;
    codes.reserve(count());
    for (int row = 0; row < count(); ++row) {
        codes.append(codeOf(item(row)));
    }
    return codes;
}

void CharacterGrid::selectCharacter(char32_t code)
{
    const QSignalBlocker blocker(this);
    QListWidgetItem* selected = nullptr;
    for (int row = 0; row < count() && code != 0; ++row) {
        if (codeOf(item(row)) == code) {
            selected = item(row);
            break;
        }
    }
    setCurrentItem(selected);
    if (selected == nullptr) {
        clearSelection();
    } else {
        scrollToItem(selected);
    }
}

char32_t CharacterGrid::codeOf(const QListWidgetItem* item)
{
    return item != nullptr ? static_cast<char32_t>(item->data(CODE_ROLE).toUInt()) : 0;
}

QSize CharacterGrid::sizeHint() const
{
    const int scrollBar = style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, this);
    const int frame = 2 * frameWidth();
    if (m_rows == 1) {
        return {m_columns * m_cell.width() + frame, m_cell.height() + frame};
    }
    return {m_columns * m_cell.width() + scrollBar + frame, m_rows * m_cell.height() + frame};
}

QSize CharacterGrid::minimumSizeHint() const
{
    const int scrollBar = style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, this);
    const int frame = 2 * frameWidth();
    if (m_rows == 1) {
        return {m_minimumColumns * m_cell.width() + frame, m_cell.height() + frame};
    }
    return {m_minimumColumns * m_cell.width() + scrollBar + frame,
            m_minimumRows * m_cell.height() + frame};
}

} // namespace gui
} // namespace kalahari
