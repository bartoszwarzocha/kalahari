/// @file table_dialogs.h
/// @brief The windows of the Table menu: Split Cells..., Sort..., Text to Table..., Table to
///        Text... and Table Properties...

#pragma once

#include "kalahari/editor/table_editing.h"
#include "kalahari/editor/text_sorting.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QChar>
#include <QColor>
#include <QStringList>

#include <functional>
#include <vector>

class QAbstractButton;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QTabWidget;
class QToolButton;

namespace kalahari {
namespace gui {

class ColorMenu;
class LengthSpinBox;

namespace dialogs {

class BordersPreview;
class CellsPreview;
class SplitCellsPreview;

// =============================================================================
// The separators of the conversions
// =============================================================================

/// @brief The choice of the character that divides the text into cells (Text to Table...)
///        or the cells of a row (Table to Text...): a tab, a semicolon, a comma, a paragraph
///        break or another character
class SeparatorChoice : public QObject {
public:
    /// @brief The kinds of separators, in the order of the radio buttons
    enum Kind { Tab, Semicolon, Comma, Paragraphs, Other };

    /// @brief The buttons in a group with its title
    /// @param labels The labels of the five buttons, with their access keys, and the
    ///        accessible name of the field of the other character
    /// @param parent The dialog, which owns the group
    SeparatorChoice(const QString& title, const QStringList& labels, QWidget* parent);

    /// @brief The group, for the layout of the dialog
    [[nodiscard]] QGroupBox* group() const { return m_group; }

    /// @brief The kind chosen
    [[nodiscard]] Kind kind() const;

    /// @brief Choose a kind
    void setKind(Kind kind);

    /// @brief The character: a null QChar for paragraphs and for another character not
    ///        given
    [[nodiscard]] QChar separator() const;

    /// @brief Whether a character is chosen: always, unless another one is not given
    [[nodiscard]] bool isComplete() const;

    /// @brief Calls @p changed when the kind or the other character changes
    void onChanged(const std::function<void()>& changed);

private:
    QGroupBox* m_group = nullptr;
    QButtonGroup* m_buttons = nullptr;
    QLineEdit* m_other = nullptr;
};

// =============================================================================
// Split Cells...
// =============================================================================

/// @brief Table > Split Cells...: the columns and rows the cell is split into
///
/// The cells selected can be merged first; the paragraphs of the cell can go one to each
/// new cell. The picture shows the cell split, its new cells highlighted and the text in
/// them.
class SplitCellsDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @param cellsSelected Several cells are selected: they can be merged first
    /// @param paragraphs The paragraphs of the cell split: of the selected cells merged,
    ///        when its argument is true (BookEditor::splitCellParagraphs())
    /// @param parent Parent widget
    SplitCellsDialog(bool cellsSelected, std::function<int(bool)> paragraphs,
                     QWidget* parent = nullptr);

    /// @brief The columns chosen (2 at first)
    [[nodiscard]] int columns() const;

    /// @brief The rows chosen (1 at first)
    [[nodiscard]] int rows() const;

    /// @brief Whether the selected cells are merged first (they are at first)
    [[nodiscard]] bool mergeFirst() const;

    /// @brief Whether the paragraphs go one to each new cell (only for a cell of several)
    [[nodiscard]] bool distribute() const;

private:
    /// @brief The fields and the picture follow the numbers and the boxes
    void updateFields();

    std::function<int(bool)> m_paragraphs;
    QSpinBox* m_columns = nullptr;
    QSpinBox* m_rows = nullptr;
    QCheckBox* m_mergeFirst = nullptr;
    QCheckBox* m_distribute = nullptr;
    SplitCellsPreview* m_preview = nullptr;
};

// =============================================================================
// Sort...
// =============================================================================

/// @brief Table > Sort...: the rows of a table by one or two columns, or the selected
///        paragraphs, as text, numbers or dates, ascending or descending
class SortDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @param tableRows The rows of a table are sorted, else paragraphs
    /// @param columns The names of the table's columns: the texts of its header row (an
    ///        empty one: "Column N")
    /// @param header The table has a header row, which stays at the top
    /// @param language The name of the language of the book, which gives the order
    /// @param parent Parent widget
    SortDialog(bool tableRows, const QStringList& columns, bool header, const QString& language,
               QWidget* parent = nullptr);

    /// @brief The keys chosen and the case; the language is the editor's
    [[nodiscard]] editor::SortOptions options() const;

private:
    /// @brief The type and the order of "Then by" only with a column
    void updateSecondKey();

    bool m_tableRows;
    QComboBox* m_firstColumn = nullptr;
    QComboBox* m_firstType = nullptr;
    QButtonGroup* m_firstOrder = nullptr;
    QComboBox* m_secondColumn = nullptr;
    QComboBox* m_secondType = nullptr;
    QButtonGroup* m_secondOrder = nullptr;
    QCheckBox* m_caseSensitive = nullptr;
};

// =============================================================================
// Text to Table... and Table to Text...
// =============================================================================

/// @brief Table > Convert > Text to Table...: the selected paragraphs become the rows of a
///        table, divided into cells at a character
///
/// The character in most of the paragraphs is chosen at first, and the columns are those
/// of the longest row; only for paragraphs (each a cell) the columns are given.
class TextToTableDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @param paragraphs The texts of the paragraphs (BookEditor::paragraphsForTable())
    /// @param parent Parent widget
    explicit TextToTableDialog(const QStringList& paragraphs, QWidget* parent = nullptr);

    /// @brief The character chosen; a null QChar: each paragraph a cell
    [[nodiscard]] QChar separator() const;

    /// @brief The columns: of the longest row, or the ones given for paragraphs
    [[nodiscard]] int columns() const;

    /// @brief Whether the first row is a header row (it is at first)
    [[nodiscard]] bool headerRow() const;

    /// @brief The cells of the table, row by row (for the picture and the tests)
    [[nodiscard]] std::vector<QStringList> cells() const;

private:
    /// @brief The columns, the size and the picture follow the choice
    void updateTable();

    QStringList m_paragraphs;
    SeparatorChoice* m_separator = nullptr;
    QSpinBox* m_columns = nullptr;
    QLabel* m_size = nullptr;
    QCheckBox* m_headerRow = nullptr;
    CellsPreview* m_preview = nullptr;
};

/// @brief Table > Convert > Table to Text...: each row of the table with the cursor becomes
///        a paragraph, its cells separated by a character (a tab at first), or each cell
///        a paragraph
class TableToTextDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @param rows The texts of the cells, row by row; a cell's paragraphs separated by
    ///        line separators (BookEditor::tableTexts())
    /// @param parent Parent widget
    explicit TableToTextDialog(const std::vector<QStringList>& rows, QWidget* parent = nullptr);

    /// @brief The character chosen; a null QChar: each cell a paragraph
    [[nodiscard]] QChar separator() const;

    /// @brief The paragraphs the table becomes (for the preview and the tests)
    [[nodiscard]] QStringList paragraphs() const;

private:
    /// @brief The note and the preview follow the choice
    void updateText();

    std::vector<QStringList> m_rows;
    SeparatorChoice* m_separator = nullptr;
    QLabel* m_hint = nullptr;
    QPlainTextEdit* m_preview = nullptr;
};

// =============================================================================
// Table Properties...
// =============================================================================

/// @brief Table > Table Properties...: the look of the table with the cursor, of its rows,
///        columns and cells, in five tabs: Table, Row, Column, Cell and Borders
///
/// The Row and Column tabs go through the rows and columns with Previous and Next; the
/// changes of all of them are made with OK, as one step of Undo. The Cell tab sets the
/// selected cells (without a selection, the cell with the cursor). The gear opens the
/// colors of the editor, where the automatic color of the lines and the header's are set.
class TablePropertiesDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @param data The properties of the table now (BookEditor::tablePropertiesData())
    /// @param automaticLines The color of the lines without a color of their own (the
    ///        editor's settings)
    /// @param parent Parent widget
    TablePropertiesDialog(const editor::TablePropertiesData& data, const QColor& automaticLines,
                          QWidget* parent = nullptr);

    /// @brief The properties chosen
    [[nodiscard]] editor::TablePropertiesData data() const;

    /// @brief The tabs (for the tests)
    [[nodiscard]] QTabWidget* tabs() const { return m_tabs; }

    /// @brief Show a row or a column in its tab (for the tests)
    void showRow(int row);
    void showColumn(int column);

private:
    QWidget* createTableTab();
    QWidget* createRowTab();
    QWidget* createColumnTab();
    QWidget* createCellTab();
    QWidget* createBordersTab();

    /// @brief The width of the table in points, as the layout makes it: never wider than
    ///        the text
    [[nodiscard]] double tableWidth() const;

    /// @brief The fields of the Table tab into the data
    void readTableTab();

    /// @brief The tab of the row or column shown
    void updateRowTab();
    void updateColumnTab();

    /// @brief The width of the column shown: the next column (the one before the last)
    ///        gives or takes the difference
    void setColumnWidth(double width);

    /// @brief The tiles, the lines and the picture follow the lines chosen
    void updateBorders();

    /// @brief A color button shows the color: its swatch and name
    void showColor(QPushButton* button, const QColor& color, bool background);

    editor::TablePropertiesData m_data;
    QColor m_automaticLines;
    std::vector<double> m_columnShares;  ///< The columns' widths, in proportion
    int m_row = 0;
    int m_column = 0;
    bool m_updating = false;  ///< The program sets the fields: they change no data

    QTabWidget* m_tabs = nullptr;

    // Table
    QRadioButton* m_textWidth = nullptr;
    QRadioButton* m_fixedWidth = nullptr;
    LengthSpinBox* m_width = nullptr;
    QButtonGroup* m_alignment = nullptr;
    LengthSpinBox* m_indent = nullptr;

    // Row
    QLabel* m_rowName = nullptr;
    QPushButton* m_previousRow = nullptr;
    QPushButton* m_nextRow = nullptr;
    QRadioButton* m_autoHeight = nullptr;
    QRadioButton* m_minHeight = nullptr;
    LengthSpinBox* m_height = nullptr;
    QCheckBox* m_headerRow = nullptr;
    QCheckBox* m_allowSplit = nullptr;

    // Column
    QLabel* m_columnName = nullptr;
    QPushButton* m_previousColumn = nullptr;
    QPushButton* m_nextColumn = nullptr;
    LengthSpinBox* m_columnWidth = nullptr;

    // Cell
    QButtonGroup* m_cellAlignment = nullptr;
    QPushButton* m_backgroundButton = nullptr;
    ColorMenu* m_backgroundMenu = nullptr;

    // Borders
    QButtonGroup* m_setting = nullptr;
    std::vector<QCheckBox*> m_lines;  ///< In the order of the lines (TableBorderLine)
    QComboBox* m_lineStyle = nullptr;
    QComboBox* m_lineWidth = nullptr;
    QPushButton* m_lineColorButton = nullptr;
    ColorMenu* m_lineColorMenu = nullptr;
    BordersPreview* m_bordersPreview = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
