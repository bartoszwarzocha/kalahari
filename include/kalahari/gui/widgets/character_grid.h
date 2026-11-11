/// @file character_grid.h
/// @brief The characters of Insert > Special Character... and of the quick palette in cells,
///        in the font of the text they go into

#pragma once

#include "kalahari/gui/special_characters.h"

#include <QFont>
#include <QHash>
#include <QList>
#include <QListWidget>
#include <QSize>
#include <QString>

#include <functional>

class QListWidgetItem;

namespace kalahari {
namespace gui {

class CharacterCellDelegate;

/// @brief The fonts the characters are shown in, from the font of the text at the cursor,
///        and whether that font has a character
class CharacterFonts {
public:
    /// @brief The fonts for a text in @p family, sized from @p base (the font of the window);
    ///        an empty family: the font of the window itself
    void setFamily(const QString& family, const QFont& base);

    /// @brief The family of the text; empty: the font of the window
    [[nodiscard]] const QString& family() const { return m_family; }

    /// @brief The characters of a cell
    [[nodiscard]] const QFont& cell() const { return m_cell; }

    /// @brief A character shown large
    [[nodiscard]] const QFont& glyph() const { return m_glyph; }

    /// @brief The marks of the characters that cannot be seen (spaces, an optional hyphen)
    [[nodiscard]] const QFont& mark() const { return m_mark; }

    /// @brief The side of a cell, for the font of a cell
    [[nodiscard]] int cellSide() const;

    /// @brief Whether the font of the text has a character (true for the window's font: the
    ///        system finds a font for each)
    [[nodiscard]] bool has(char32_t code) const;

private:
    QString m_family;
    QFont m_cell;
    QFont m_glyph;
    QFont m_mark;
    mutable QHash<char32_t, bool> m_has;  ///< The characters asked for, known
};

/// @brief A character as a cell or a line shows it: itself, or the mark of one that cannot be
///        seen
[[nodiscard]] QString shownCharacterText(const SpecialCharacter& character);

/// @brief A grid of characters in square cells, each in the middle of its cell: greyed out
///        where the font of the text has it not, a mark small in a dotted frame
///
/// As large as its columns and rows of cells at first, smaller on a small screen down to its
/// minimum cells; with one row it is as high as a cell and does not scroll.
class CharacterGrid : public QListWidget {
public:
    /// @brief The code of the character of an item
    static constexpr int CODE_ROLE = Qt::UserRole;

    /// @param columns The cells of a row at first
    /// @param rows The rows at first; 1: one row, without a scroll bar
    /// @param parent Parent widget
    CharacterGrid(int columns, int rows, QWidget* parent = nullptr);

    /// @brief The fewest cells the grid shows on a small screen
    void setMinimumCells(int columns, int rows);

    /// @brief Cells of the side the fonts give
    void setCellSide(int side);

    /// @brief Show characters in the fonts of the text
    /// @param toolTipOf What a cell says of its character
    void setCharacters(const QList<char32_t>& codes, const SpecialCharacters& characters,
                       const CharacterFonts& fonts,
                       const std::function<QString(char32_t)>& toolTipOf);

    /// @brief The characters shown, in their order
    [[nodiscard]] QList<char32_t> characters() const;

    /// @brief Select a character and bring it into view; none (0) or one not shown: nothing
    ///        selected
    void selectCharacter(char32_t code);

    /// @brief The code of the character of an item; 0 for none
    [[nodiscard]] static char32_t codeOf(const QListWidgetItem* item);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

private:
    int m_columns;
    int m_rows;
    int m_minimumColumns;
    int m_minimumRows;
    QSize m_cell{32, 32};
    CharacterCellDelegate* m_delegate = nullptr;
};

} // namespace gui
} // namespace kalahari
