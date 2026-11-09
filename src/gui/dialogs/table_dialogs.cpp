/// @file table_dialogs.cpp
/// @brief The windows of the Table menu: Split Cells..., Sort..., Text to Table..., Table to
///        Text... and Table Properties...

#include "kalahari/gui/dialogs/table_dialogs.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/text_tables.h"
#include "kalahari/gui/dialogs/color_palettes.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/widgets/color_menu.h"
#include "kalahari/gui/widgets/length_spin_box.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFontMetrics>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <numeric>
#include <optional>
#include <utility>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

// The pictures: their width, the height of a row, and the rows they show at most
constexpr int PREVIEW_WIDTH = 320;
constexpr int SPLIT_PREVIEW_WIDTH = 240;
constexpr int SPLIT_ROW_HEIGHT = 16;
constexpr int SPLIT_MAX_ROWS = 4;      ///< Of the split cell; more rows share their height
constexpr int PREVIEW_MAX_ROWS = 4;
constexpr int PREVIEW_LINES = 6;       ///< Of the text of Table to Text...
constexpr int CELL_TEXT_PADDING = 3;
constexpr int BORDERS_PREVIEW_WIDTH = 150;
constexpr int BORDERS_PREVIEW_HEIGHT = 90;
constexpr int BORDERS_CLICK_DISTANCE = 6;  ///< How near a line a click toggles it
constexpr int TILE_ICON_WIDTH = 32;
constexpr int TILE_ICON_HEIGHT = 24;

constexpr int DEFAULT_SPLIT_COLUMNS = 2;
constexpr int DEFAULT_SPLIT_ROWS = 1;
constexpr int DEFAULT_PARAGRAPH_COLUMNS = 2;
constexpr double DEFAULT_MIN_HEIGHT = 0.8 / 2.54 * 72.0;  ///< 0.8 cm, in points
constexpr double SAME_WIDTH = 0.005;                       ///< Points

/// The widths of the lines a table can have, in points (as Word has them)
constexpr double LINE_WIDTHS[] = {0.25, 0.5, 0.75, 1.0, 1.5, 2.25, 3.0, 4.5, 6.0};

/// The lines of the borders, in the order of their boxes
constexpr editor::TableBorderLine BORDER_LINES[] = {
    editor::BORDER_TOP,  editor::BORDER_BOTTOM, editor::BORDER_LEFT,
    editor::BORDER_RIGHT, editor::BORDER_ROWS,  editor::BORDER_COLUMNS};

/// The tiles of the setting of the borders
enum BorderSetting { SettingNone, SettingBox, SettingGrid, SettingCustom };

/// A cell's text in one line: its paragraphs joined by spaces
QString oneLine(QString text)
{
    text.replace(QChar(QChar::LineSeparator), QLatin1Char(' '));
    text.replace(QLatin1Char('\t'), QLatin1Char(' '));
    return text;
}

/// A swatch for a color button
QIcon swatchIcon(const QColor& color, const QColor& border, int side)
{
    QPixmap pixmap(side, side);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    if (color.isValid()) {
        painter.fillRect(pixmap.rect().adjusted(1, 1, -1, -1), color);
    }
    painter.setPen(border);
    painter.drawRect(pixmap.rect().adjusted(1, 1, -2, -2));
    return QIcon(pixmap);
}

/// Draw a line of a border: its style, its width and its color
void drawBorderLine(QPainter& painter, const QLineF& line, editor::BorderStyle style, qreal width,
                    const QColor& color)
{
    QPen pen(color);
    pen.setWidthF(style == editor::BorderStyle::Double ? 1.0 : width);
    pen.setCapStyle(Qt::FlatCap);
    pen.setStyle(style == editor::BorderStyle::Dashed   ? Qt::DashLine
                 : style == editor::BorderStyle::Dotted ? Qt::DotLine
                                                        : Qt::SolidLine);
    painter.setPen(pen);
    if (style != editor::BorderStyle::Double) {
        painter.drawLine(line);
        return;
    }
    // Two thin lines, as far apart as the width
    const QPointF normal = line.dx() == 0.0 ? QPointF(1.0, 0.0) : QPointF(0.0, 1.0);
    const qreal apart = std::max<qreal>(1.0, width / 2.0);
    painter.drawLine(line.translated(normal * apart));
    painter.drawLine(line.translated(-normal * apart));
}

/// The icon of a tile of the borders: the lines of a grid of 3 by 3 cells
QIcon settingIcon(int lines, const QColor& color, const QColor& faint)
{
    QPixmap pixmap(TILE_ICON_WIDTH, TILE_ICON_HEIGHT);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    const QRectF box(2.5, 2.5, TILE_ICON_WIDTH - 5.0, TILE_ICON_HEIGHT - 5.0);
    const qreal columnWidth = box.width() / 3.0;
    const qreal rowHeight = box.height() / 3.0;
    const auto draw = [&painter, &color, &faint](const QLineF& line, bool shown) {
        QPen pen(shown ? color : faint);
        pen.setStyle(shown ? Qt::SolidLine : Qt::DotLine);
        painter.setPen(pen);
        painter.drawLine(line);
    };
    draw(QLineF(box.topLeft(), box.topRight()), (lines & editor::BORDER_TOP) != 0);
    draw(QLineF(box.bottomLeft(), box.bottomRight()), (lines & editor::BORDER_BOTTOM) != 0);
    draw(QLineF(box.topLeft(), box.bottomLeft()), (lines & editor::BORDER_LEFT) != 0);
    draw(QLineF(box.topRight(), box.bottomRight()), (lines & editor::BORDER_RIGHT) != 0);
    for (int i = 1; i < 3; ++i) {
        const qreal y = box.top() + rowHeight * i;
        const qreal x = box.left() + columnWidth * i;
        draw(QLineF(box.left(), y, box.right(), y), (lines & editor::BORDER_ROWS) != 0);
        draw(QLineF(x, box.top(), x, box.bottom()), (lines & editor::BORDER_COLUMNS) != 0);
    }
    return QIcon(pixmap);
}

/// The setting of the lines: None, Box, Grid, or Custom for any other lines
BorderSetting settingOf(int lines)
{
    return lines == editor::BORDER_NONE  ? SettingNone
           : lines == editor::BORDER_BOX ? SettingBox
           : lines == editor::BORDER_GRID ? SettingGrid
                                          : SettingCustom;
}

/// A width of a line, in the way of the language: "0,5 pt"
QString lineWidthText(double points)
{
    //: The width of a line of a table's borders, in points: "0.5 pt"
    return QCoreApplication::translate("kalahari::gui::dialogs::TablePropertiesDialog", "%1 pt")
        .arg(QLocale().toString(points, 'g', 3));
}

/// A button for a radio group, with its label, for keys of its own
QRadioButton* radio(const QString& text, QButtonGroup* group, int id, QWidget* parent)
{
    auto* button = new QRadioButton(text, parent);
    group->addButton(button, id);
    return button;
}

/// Previous and Next of a row or a column, after its name
QHBoxLayout* navigationRow(QLabel* name, QPushButton* previous, QPushButton* next)
{
    auto* row = new QHBoxLayout();
    row->addWidget(name, 1);
    row->addWidget(previous);
    row->addWidget(next);
    return row;
}

} // namespace

// =============================================================================
// The pictures
// =============================================================================

/// @brief The cell split: a table of 3 by 3 cells, the middle one split into its new
///        cells, highlighted, with the marks of the text they get
class SplitCellsPreview : public QWidget {
public:
    explicit SplitCellsPreview(QWidget* parent)
        : QWidget(parent)
    {
        setFixedSize(SPLIT_PREVIEW_WIDTH + 1, SPLIT_ROW_HEIGHT * (SPLIT_MAX_ROWS + 2) + 1);
    }

    void showSplit(int columns, int rows, int texts)
    {
        m_columns = std::max(1, columns);
        m_rows = std::max(1, rows);
        m_texts = std::max(0, texts);
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        const QPalette& colors = palette();
        const int width = SPLIT_PREVIEW_WIDTH;
        const int splitHeight = SPLIT_ROW_HEIGHT * SPLIT_MAX_ROWS;
        const int height = SPLIT_ROW_HEIGHT * 2 + splitHeight;
        const int left = width / 4;
        const int right = width - width / 4;
        const int top = SPLIT_ROW_HEIGHT;
        const int bottom = top + splitHeight;

        painter.fillRect(QRect(0, 0, width, height), colors.base());
        painter.fillRect(QRect(0, 0, width, SPLIT_ROW_HEIGHT), colors.button());
        QColor highlight = colors.highlight().color();
        highlight.setAlpha(70);
        painter.fillRect(QRect(left, top, right - left, splitHeight), highlight);

        // The marks of the text: in the other cells, and in the new cells that get some
        const auto mark = [&painter](const QRect& cell, const QColor& color) {
            if (cell.width() > CELL_TEXT_PADDING * 3 && cell.height() >= 4) {
                const int length = std::min(cell.width() - CELL_TEXT_PADDING * 2, 28);
                painter.fillRect(QRect(cell.left() + CELL_TEXT_PADDING,
                                       cell.center().y() - 1, length, 2),
                                 color);
            }
        };
        const QColor other = colors.mid().color();
        for (const int y : {0, bottom}) {
            mark(QRect(0, y, left, SPLIT_ROW_HEIGHT), other);
            mark(QRect(left, y, right - left, SPLIT_ROW_HEIGHT), other);
            mark(QRect(right, y, width - right, SPLIT_ROW_HEIGHT), other);
        }
        mark(QRect(0, top, left, SPLIT_ROW_HEIGHT), other);
        mark(QRect(right, top, width - right, SPLIT_ROW_HEIGHT), other);
        const auto columnEdge = [this, left, right](int column) {
            return left + (right - left) * column / m_columns;
        };
        const auto rowEdge = [this, top, splitHeight](int row) {
            return top + splitHeight * row / m_rows;
        };
        for (int index = 0; index < std::min(m_texts, m_columns * m_rows); ++index) {
            const int row = index / m_columns;
            const int column = index % m_columns;
            mark(QRect(columnEdge(column), rowEdge(row), columnEdge(column + 1) - columnEdge(column),
                       rowEdge(row + 1) - rowEdge(row)),
                 colors.text().color());
        }

        // The lines: of the table, and of the new cells
        painter.setPen(colors.mid().color());
        for (const int y : {0, top, bottom, height}) {
            painter.drawLine(0, y, width, y);
        }
        for (const int x : {0, left, right, width}) {
            painter.drawLine(x, 0, x, height);
        }
        painter.setPen(colors.highlight().color());
        for (int row = 1; row < m_rows; ++row) {
            painter.drawLine(left, rowEdge(row), right, rowEdge(row));
        }
        for (int column = 1; column < m_columns; ++column) {
            painter.drawLine(columnEdge(column), top, columnEdge(column), bottom);
        }
    }

private:
    int m_columns = DEFAULT_SPLIT_COLUMNS;
    int m_rows = DEFAULT_SPLIT_ROWS;
    int m_texts = 1;
};

/// @brief A table with its texts: its first rows, the header row shaded
class CellsPreview : public QWidget {
public:
    explicit CellsPreview(QWidget* parent)
        : QWidget(parent)
    {
        setFixedSize(PREVIEW_WIDTH + 1, PREVIEW_MAX_ROWS * rowHeight() + 1);
    }

    void showCells(std::vector<QStringList> rows, bool headerRow)
    {
        m_rows = std::move(rows);
        m_headerRow = headerRow;
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        const QPalette& colors = palette();
        const int rowHeight = this->rowHeight();
        const int shown = std::min(static_cast<int>(m_rows.size()), PREVIEW_MAX_ROWS);
        int columns = 1;
        for (const QStringList& row : m_rows) {
            columns = std::max(columns, static_cast<int>(row.size()));
        }
        const int height = shown * rowHeight;
        painter.fillRect(QRect(0, 0, PREVIEW_WIDTH, height), colors.base());
        if (m_headerRow && shown > 0) {
            painter.fillRect(QRect(0, 0, PREVIEW_WIDTH, rowHeight), colors.button());
        }
        const auto edge = [columns](int column) { return column * PREVIEW_WIDTH / columns; };
        painter.setPen(colors.text().color());
        for (int row = 0; row < shown; ++row) {
            const QStringList& texts = m_rows[static_cast<size_t>(row)];
            for (int column = 0; column < static_cast<int>(texts.size()); ++column) {
                const QRect cell(edge(column) + CELL_TEXT_PADDING, row * rowHeight,
                                 edge(column + 1) - edge(column) - CELL_TEXT_PADDING * 2,
                                 rowHeight);
                if (cell.width() > 0) {
                    painter.drawText(cell, Qt::AlignLeft | Qt::AlignVCenter,
                                     fontMetrics().elidedText(oneLine(texts.at(column)),
                                                              Qt::ElideRight, cell.width()));
                }
            }
        }
        painter.setPen(colors.mid().color());
        for (int row = 0; row <= shown; ++row) {
            painter.drawLine(0, row * rowHeight, PREVIEW_WIDTH, row * rowHeight);
        }
        for (int column = 0; column <= columns && shown > 0; ++column) {
            painter.drawLine(edge(column), 0, edge(column), height);
        }
    }

private:
    int rowHeight() const { return fontMetrics().height() + 4; }

    std::vector<QStringList> m_rows;
    bool m_headerRow = true;
};

/// @brief The borders of a table of 3 by 3 cells: the lines chosen in their style, width and
///        color, the others faint; a click near a line takes it or gives it
class BordersPreview : public QWidget {
public:
    explicit BordersPreview(QWidget* parent)
        : QWidget(parent)
    {
        setFixedSize(BORDERS_PREVIEW_WIDTH, BORDERS_PREVIEW_HEIGHT);
        setCursor(Qt::PointingHandCursor);
    }

    void showBorders(const editor::TableBorders& borders, const QColor& color)
    {
        m_borders = borders;
        m_color = color;
        update();
    }

    /// @brief Called with the line clicked
    std::function<void(editor::TableBorderLine)> lineClicked;

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        const QPalette& colors = palette();
        painter.fillRect(rect(), colors.base());
        QColor faint = colors.mid().color();
        faint.setAlpha(110);
        const qreal width = std::clamp(m_borders.width * 1.5, 1.0, 6.0);
        for (const editor::TableBorderLine line : BORDER_LINES) {
            for (const QLineF& segment : segments(line)) {
                if ((m_borders.lines & line) != 0) {
                    drawBorderLine(painter, segment, m_borders.style, width, m_color);
                } else {
                    QPen pen(faint);
                    pen.setStyle(Qt::DotLine);
                    painter.setPen(pen);
                    painter.drawLine(segment);
                }
            }
        }
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton || !lineClicked) {
            return;
        }
        // The nearest line, if near enough
        const QPointF point = event->position();
        qreal nearest = BORDERS_CLICK_DISTANCE;
        std::optional<editor::TableBorderLine> clicked;
        for (const editor::TableBorderLine line : BORDER_LINES) {
            for (const QLineF& segment : segments(line)) {
                const QRectF box = QRectF(segment.p1(), segment.p2()).normalized();
                const qreal distance = segment.dx() == 0.0
                                           ? (point.y() >= box.top() && point.y() <= box.bottom()
                                                  ? std::abs(point.x() - segment.x1())
                                                  : BORDERS_CLICK_DISTANCE)
                                           : (point.x() >= box.left() && point.x() <= box.right()
                                                  ? std::abs(point.y() - segment.y1())
                                                  : BORDERS_CLICK_DISTANCE);
                if (distance < nearest) {
                    nearest = distance;
                    clicked = line;
                }
            }
        }
        if (clicked) {
            lineClicked(*clicked);
        }
    }

private:
    /// The table drawn: within the widget, with room for the widest line
    QRectF table() const { return QRectF(rect()).adjusted(8.5, 8.5, -8.5, -8.5); }

    /// The pieces of a line: one for an edge, two for the lines between rows or columns
    std::vector<QLineF> segments(editor::TableBorderLine line) const
    {
        const QRectF box = table();
        switch (line) {
        case editor::BORDER_TOP:
            return {QLineF(box.topLeft(), box.topRight())};
        case editor::BORDER_BOTTOM:
            return {QLineF(box.bottomLeft(), box.bottomRight())};
        case editor::BORDER_LEFT:
            return {QLineF(box.topLeft(), box.bottomLeft())};
        case editor::BORDER_RIGHT:
            return {QLineF(box.topRight(), box.bottomRight())};
        case editor::BORDER_ROWS: {
            std::vector<QLineF> lines;
            for (int i = 1; i < 3; ++i) {
                const qreal y = std::round(box.top() + box.height() * i / 3.0) + 0.5;
                lines.emplace_back(box.left(), y, box.right(), y);
            }
            return lines;
        }
        case editor::BORDER_COLUMNS: {
            std::vector<QLineF> lines;
            for (int i = 1; i < 3; ++i) {
                const qreal x = std::round(box.left() + box.width() * i / 3.0) + 0.5;
                lines.emplace_back(x, box.top(), x, box.bottom());
            }
            return lines;
        }
        }
        return {};
    }

    editor::TableBorders m_borders;
    QColor m_color;
};

// =============================================================================
// The separators
// =============================================================================

SeparatorChoice::SeparatorChoice(const QString& title, const QStringList& labels,
                                 QWidget* parent)
    : QObject(parent)
{
    // Two columns: tabs and semicolons, commas and paragraphs, and another character
    m_group = new QGroupBox(title, parent);
    auto* layout = new QGridLayout(m_group);
    m_buttons = new QButtonGroup(this);
    for (int kind = Tab; kind <= Other; ++kind) {
        auto* button = radio(labels.value(kind), m_buttons, kind, m_group);
        if (kind != Other) {
            layout->addWidget(button, kind / 2, kind % 2);
            continue;
        }
        m_other = new QLineEdit(QStringLiteral("|"), m_group);
        m_other->setMaxLength(1);
        m_other->setAccessibleName(labels.value(Other + 1));
        m_other->setFixedWidth(m_other->fontMetrics().averageCharWidth() * 4 +
                               m_other->fontMetrics().height());
        m_other->setEnabled(false);
        auto* row = new QHBoxLayout();
        row->addWidget(button);
        row->addWidget(m_other);
        row->addStretch(1);
        layout->addLayout(row, kind / 2, 0, 1, 2);
    }
    layout->setColumnStretch(2, 1);
    m_buttons->button(Tab)->setChecked(true);
    connect(m_buttons, &QButtonGroup::idToggled, this, [this](int id, bool checked) {
        if (id == Other) {
            m_other->setEnabled(checked);
            if (checked) {
                m_other->setFocus();
                m_other->selectAll();
            }
        }
    });
}

SeparatorChoice::Kind SeparatorChoice::kind() const
{
    return static_cast<Kind>(std::max(0, m_buttons->checkedId()));
}

void SeparatorChoice::setKind(Kind kind)
{
    if (QAbstractButton* button = m_buttons->button(kind)) {
        button->setChecked(true);
    }
}

QChar SeparatorChoice::separator() const
{
    switch (kind()) {
    case Tab:
        return QLatin1Char('\t');
    case Semicolon:
        return QLatin1Char(';');
    case Comma:
        return QLatin1Char(',');
    case Paragraphs:
        return QChar();
    case Other:
        return m_other->text().isEmpty() ? QChar() : m_other->text().at(0);
    }
    return QChar();
}

bool SeparatorChoice::isComplete() const
{
    return kind() != Other || !m_other->text().isEmpty();
}

void SeparatorChoice::onChanged(const std::function<void()>& changed)
{
    connect(m_buttons, &QButtonGroup::idToggled, this, [changed](int /*id*/, bool checked) {
        if (checked) {
            changed();
        }
    });
    connect(m_other, &QLineEdit::textChanged, this, [changed]() { changed(); });
}

// =============================================================================
// Split Cells...
// =============================================================================

SplitCellsDialog::SplitCellsDialog(bool cellsSelected, std::function<int(bool)> paragraphs,
                                   QWidget* parent)
    : KalahariDialog(parent)
    , m_paragraphs(std::move(paragraphs))
{
    setHeading(tr("Split Cells"), cellsSelected
                                      ? tr("The selected cells are split into smaller cells.")
                                      : tr("The cell with the cursor is split into smaller cells."));
    setHeadingIcon(QStringLiteral("table.splitCells"));
    setAcceptText(tr("Split"));

    m_columns = new QSpinBox(this);
    m_columns->setRange(1, editor::MAX_SPLIT_CELLS);
    m_columns->setValue(DEFAULT_SPLIT_COLUMNS);
    m_rows = new QSpinBox(this);
    m_rows->setRange(1, editor::MAX_SPLIT_CELLS);
    m_rows->setValue(DEFAULT_SPLIT_ROWS);
    auto* numbers = new QHBoxLayout();
    for (const auto& [label, field] :
         {std::pair{tr("&Columns"), m_columns}, std::pair{tr("&Rows"), m_rows}}) {
        auto* fieldLabel = new QLabel(label, this);
        fieldLabel->setBuddy(field);
        numbers->addWidget(fieldLabel);
        numbers->addWidget(field);
        numbers->addSpacing(fontMetrics().averageCharWidth() * 2);
    }
    numbers->addStretch(1);
    contentLayout()->addLayout(numbers);

    m_mergeFirst = new QCheckBox(tr("&Merge the selected cells first"), this);
    m_mergeFirst->setChecked(true);
    m_mergeFirst->setEnabled(cellsSelected);
    if (!cellsSelected) {
        m_mergeFirst->setToolTip(tr("Select several cells to merge them before they are split"));
    }
    contentLayout()->addWidget(m_mergeFirst);
    m_distribute = new QCheckBox(tr("&Distribute the cell's paragraphs to the new cells"), this);
    m_distribute->setChecked(true);
    contentLayout()->addWidget(m_distribute);

    auto* caption = new QLabel(tr("Preview – the new cells are highlighted"), this);
    caption->setForegroundRole(QPalette::PlaceholderText);
    contentLayout()->addWidget(caption);
    m_preview = new SplitCellsPreview(this);
    m_preview->setAccessibleName(tr("Preview of the table split"));
    contentLayout()->addWidget(m_preview);
    contentLayout()->addStretch(1);

    connect(m_columns, &QSpinBox::valueChanged, this, &SplitCellsDialog::updateFields);
    connect(m_rows, &QSpinBox::valueChanged, this, &SplitCellsDialog::updateFields);
    connect(m_mergeFirst, &QCheckBox::toggled, this, &SplitCellsDialog::updateFields);
    connect(m_distribute, &QCheckBox::toggled, this, &SplitCellsDialog::updateFields);
    updateFields();

    m_columns->setFocus();
    m_columns->selectAll();
}

int SplitCellsDialog::columns() const
{
    return m_columns->value();
}

int SplitCellsDialog::rows() const
{
    return m_rows->value();
}

bool SplitCellsDialog::mergeFirst() const
{
    return m_mergeFirst->isEnabled() && m_mergeFirst->isChecked();
}

bool SplitCellsDialog::distribute() const
{
    return m_distribute->isEnabled() && m_distribute->isChecked();
}

void SplitCellsDialog::updateFields()
{
    // The paragraphs go one to a cell only from a cell of several
    const int paragraphs = m_paragraphs ? m_paragraphs(mergeFirst()) : 1;
    m_distribute->setEnabled(paragraphs > 1);
    m_distribute->setToolTip(paragraphs > 1
                                 ? QString()
                                 : tr("The cell has one paragraph: it stays in the first new cell"));
    const int texts = distribute() ? paragraphs : std::min(paragraphs, 1);
    m_preview->showSplit(columns(), rows(), texts);
    // 1 x 1 does not split
    acceptButton()->setEnabled(columns() * rows() > 1 || mergeFirst());

    const QString columnsText = tr("%n columns", nullptr, columns());
    const QString rowsText = tr("%n rows", nullptr, rows());
    //: %1 is the columns ("2 columns"), %2 the rows ("1 rows")
    m_preview->setAccessibleDescription(tr("The cell split into %1 and %2").arg(columnsText, rowsText));
}

// =============================================================================
// Sort...
// =============================================================================

SortDialog::SortDialog(bool tableRows, const QStringList& columns, bool header,
                       const QString& language, QWidget* parent)
    : KalahariDialog(parent)
    , m_tableRows(tableRows)
{
    setHeading(tr("Sort"), tableRows ? tr("The rows of the table in the order of the column chosen.")
                                     : tr("The selected paragraphs in the order of their text."));
    setHeadingIcon(QStringLiteral("table.sort"));
    setAcceptText(tr("Sort"));

    auto* order = new QGroupBox(tr("Order"), this);
    auto* grid = new QGridLayout(order);
    const QStringList types = {tr("Text"), tr("Numbers"), tr("Dates")};
    const auto addKey = [this, order, grid, &types](int row, const QString& label,
                                                    QComboBox*& column, QComboBox*& type,
                                                    QButtonGroup*& direction,
                                                    const QString& ascending,
                                                    const QString& descending) {
        auto* columnLabel = new QLabel(label, order);
        column = new QComboBox(order);
        columnLabel->setBuddy(column);
        //: The type of the values a key of sorting has: text, numbers or dates
        auto* typeLabel = new QLabel(tr("Type"), order);
        type = new QComboBox(order);
        typeLabel->setBuddy(type);
        type->addItems(types);
        type->setAccessibleName(tr("Type"));
        direction = new QButtonGroup(this);
        auto* up = radio(ascending, direction, 0, order);
        auto* down = radio(descending, direction, 1, order);
        up->setChecked(true);
        grid->addWidget(columnLabel, row, 0);
        grid->addWidget(column, row, 1, 1, 3);
        grid->addWidget(typeLabel, row + 1, 0);
        grid->addWidget(type, row + 1, 1);
        grid->addWidget(up, row + 1, 2);
        grid->addWidget(down, row + 1, 3);
    };
    addKey(0, tr("&Sort by"), m_firstColumn, m_firstType, m_firstOrder, tr("&Ascending"),
           tr("&Descending"));
    addKey(2, tr("&Then by"), m_secondColumn, m_secondType, m_secondOrder, tr("Ascending"),
           tr("Descending"));
    grid->setColumnStretch(4, 1);
    contentLayout()->addWidget(order);

    // The columns by the texts of the header row, else by their numbers
    //: Sort: the second key is not used
    m_secondColumn->addItem(tr("(none)"));
    if (tableRows) {
        for (qsizetype column = 0; column < columns.size(); ++column) {
            const QString name = columns.at(column).trimmed();
            //: Sort: a column of a table without a header row; %1 is its number
            const QString shown = name.isEmpty() ? tr("Column %1").arg(column + 1) : name;
            m_firstColumn->addItem(shown, static_cast<int>(column));
            m_secondColumn->addItem(shown, static_cast<int>(column));
        }
    } else {
        m_firstColumn->addItem(tr("Paragraphs"), 0);
        m_firstColumn->setEnabled(false);
        m_secondColumn->setEnabled(false);
    }

    m_caseSensitive = new QCheckBox(tr("&Match case"), this);
    contentLayout()->addWidget(m_caseSensitive);

    //: Sort: %1 is the language of the book ("Polish")
    QString hint = tr("The order of the alphabet: %1, the language of the book.").arg(language);
    if (tableRows && header) {
        hint += QLatin1Char(' ') + tr("The header row stays at the top.");
    } else if (!tableRows) {
        hint += QLatin1Char(' ') + tr("List items are sorted with their paragraphs.");
    }
    auto* hintLabel = new QLabel(hint, this);
    hintLabel->setWordWrap(true);
    hintLabel->setForegroundRole(QPalette::PlaceholderText);
    contentLayout()->addWidget(hintLabel);
    contentLayout()->addStretch(1);

    connect(m_secondColumn, &QComboBox::currentIndexChanged, this, &SortDialog::updateSecondKey);
    updateSecondKey();
    (tableRows ? static_cast<QWidget*>(m_firstColumn) : m_firstType)->setFocus();
}

editor::SortOptions SortDialog::options() const
{
    editor::SortOptions options;
    const auto key = [](const QComboBox* column, const QComboBox* type, const QButtonGroup* order) {
        editor::SortKey key;
        key.column = std::max(0, column->currentData().toInt());
        key.type = static_cast<editor::SortType>(std::max(0, type->currentIndex()));
        key.descending = order->checkedId() == 1;
        return key;
    };
    options.keys.push_back(key(m_firstColumn, m_firstType, m_firstOrder));
    if (m_tableRows && m_secondColumn->currentIndex() > 0) {
        options.keys.push_back(key(m_secondColumn, m_secondType, m_secondOrder));
    }
    options.caseSensitive = m_caseSensitive->isChecked();
    return options;
}

void SortDialog::updateSecondKey()
{
    const bool used = m_tableRows && m_secondColumn->currentIndex() > 0;
    m_secondType->setEnabled(used);
    for (QAbstractButton* button : m_secondOrder->buttons()) {
        button->setEnabled(used);
    }
}

// =============================================================================
// Text to Table...
// =============================================================================

TextToTableDialog::TextToTableDialog(const QStringList& paragraphs, QWidget* parent)
    : KalahariDialog(parent)
    , m_paragraphs(paragraphs)
{
    setHeading(tr("Text to Table"), tr("The selected paragraphs become the rows of a table."));
    setHeadingIcon(QStringLiteral("table.textToTable"));
    setCompactHeading(true);
    setAcceptText(tr("Convert to Table"));

    //: Text to Table: divide the text into cells at the tabs, semicolons...
    m_separator = new SeparatorChoice(tr("Divide the text into cells at"),
                                      {tr("&Tabs"), tr("S&emicolons"), tr("Co&mmas"),
                                       tr("&Paragraphs"), tr("&Other character:"),
                                       tr("Other character")},
                                      this);
    contentLayout()->addWidget(m_separator->group());

    m_columns = new QSpinBox(this);
    m_columns->setRange(1, editor::BookEditor::MAX_INSERTED_TABLE_COLUMNS);
    auto* columnsLabel = new QLabel(tr("&Columns"), this);
    columnsLabel->setBuddy(m_columns);
    m_size = new QLabel(this);
    m_size->setForegroundRole(QPalette::PlaceholderText);
    auto* row = new QHBoxLayout();
    row->addWidget(columnsLabel);
    row->addWidget(m_columns);
    row->addSpacing(fontMetrics().averageCharWidth() * 2);
    row->addWidget(m_size, 1);
    contentLayout()->addLayout(row);

    m_headerRow = new QCheckBox(tr("&Header row"), this);
    m_headerRow->setChecked(true);
    contentLayout()->addWidget(m_headerRow);

    auto* caption = new QLabel(tr("Preview"), this);
    caption->setForegroundRole(QPalette::PlaceholderText);
    contentLayout()->addWidget(caption);
    m_preview = new CellsPreview(this);
    m_preview->setAccessibleName(tr("Preview of the table"));
    contentLayout()->addWidget(m_preview);
    contentLayout()->addStretch(1);

    // The character most of the paragraphs have (a tab without one)
    const QChar likeliest = editor::likeliestSeparator(paragraphs);
    m_separator->setKind(likeliest == QLatin1Char(';')   ? SeparatorChoice::Semicolon
                         : likeliest == QLatin1Char(',') ? SeparatorChoice::Comma
                                                         : SeparatorChoice::Tab);
    m_separator->onChanged([this]() {
        if (m_separator->kind() == SeparatorChoice::Paragraphs) {
            m_columns->setValue(DEFAULT_PARAGRAPH_COLUMNS);
        }
        updateTable();
    });
    connect(m_columns, &QSpinBox::valueChanged, this, [this]() {
        if (m_separator->kind() == SeparatorChoice::Paragraphs) {
            updateTable();
        }
    });
    connect(m_headerRow, &QCheckBox::toggled, this, &TextToTableDialog::updateTable);
    updateTable();
}

QChar TextToTableDialog::separator() const
{
    return m_separator->separator();
}

int TextToTableDialog::columns() const
{
    return m_columns->value();
}

bool TextToTableDialog::headerRow() const
{
    return m_headerRow->isChecked();
}

std::vector<QStringList> TextToTableDialog::cells() const
{
    std::vector<QStringList> rows;
    if (m_separator->kind() == SeparatorChoice::Paragraphs) {
        const int columns = std::max(1, m_columns->value());
        for (qsizetype i = 0; i < m_paragraphs.size(); ++i) {
            if (i % columns == 0) {
                rows.emplace_back();
            }
            rows.back() << m_paragraphs.at(i);
        }
    } else {
        const QChar separator = m_separator->separator();
        for (const QString& paragraph : m_paragraphs) {
            QStringList texts = editor::cellTexts(paragraph, separator);
            if (separator != QLatin1Char('\t')) {
                for (QString& text : texts) {
                    text = text.trimmed();
                }
            }
            rows.push_back(texts);
        }
    }
    int columns = 1;
    for (const QStringList& texts : rows) {
        columns = std::max(columns, static_cast<int>(texts.size()));
    }
    columns = std::min(columns, editor::MAX_TABLE_SPAN);
    for (QStringList& texts : rows) {
        while (texts.size() < columns) {
            texts << QString();
        }
        texts = texts.mid(0, columns);
    }
    return rows;
}

void TextToTableDialog::updateTable()
{
    const bool paragraphs = m_separator->kind() == SeparatorChoice::Paragraphs;
    const std::vector<QStringList> rows = cells();
    const int columns = rows.empty() ? 1 : static_cast<int>(rows.front().size());
    // The columns of the longest row; given only for paragraphs
    if (!paragraphs) {
        const QSignalBlocker blocker(m_columns);
        m_columns->setRange(1, std::max(columns, editor::BookEditor::MAX_INSERTED_TABLE_COLUMNS));
        m_columns->setValue(columns);
    } else {
        m_columns->setRange(1, editor::BookEditor::MAX_INSERTED_TABLE_COLUMNS);
    }
    m_columns->setEnabled(paragraphs);
    const int rowCount = static_cast<int>(rows.size());
    //: Text to Table: the size of the table made, "3 × 5 (columns × rows)"
    m_size->setText(tr("The table: %1 × %2 (columns × rows)").arg(columns).arg(rowCount));
    m_preview->showCells(rows, headerRow());
    m_preview->setAccessibleDescription(m_size->text());
    acceptButton()->setEnabled(m_separator->isComplete() && rowCount > 0);
}

// =============================================================================
// Table to Text...
// =============================================================================

TableToTextDialog::TableToTextDialog(const std::vector<QStringList>& rows, QWidget* parent)
    : KalahariDialog(parent)
    , m_rows(rows)
{
    setHeading(tr("Table to Text"), tr("Each row of the table becomes a paragraph."));
    setHeadingIcon(QStringLiteral("table.tableToText"));
    setCompactHeading(true);
    setAcceptText(tr("Convert to Text"));

    //: Table to Text: separate the cells with tabs, semicolons...
    m_separator = new SeparatorChoice(tr("Separate the cells with"),
                                      {tr("&Tabs"), tr("S&emicolons"), tr("Co&mmas"),
                                       tr("&Paragraphs"), tr("&Other character:"),
                                       tr("Other character")},
                                      this);
    contentLayout()->addWidget(m_separator->group());

    m_hint = new QLabel(this);
    m_hint->setWordWrap(true);
    m_hint->setForegroundRole(QPalette::PlaceholderText);
    contentLayout()->addWidget(m_hint);

    auto* caption = new QLabel(tr("Preview"), this);
    caption->setForegroundRole(QPalette::PlaceholderText);
    contentLayout()->addWidget(caption);
    m_preview = new QPlainTextEdit(this);
    m_preview->setReadOnly(true);
    m_preview->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_preview->setFocusPolicy(Qt::TabFocus);
    m_preview->setAccessibleName(tr("Preview of the text"));
    m_preview->setFixedHeight(m_preview->fontMetrics().lineSpacing() * PREVIEW_LINES +
                              m_preview->frameWidth() * 2 +
                              static_cast<int>(m_preview->document()->documentMargin() * 2));
    contentLayout()->addWidget(m_preview);
    contentLayout()->addStretch(1);

    m_separator->onChanged([this]() { updateText(); });
    updateText();
}

QChar TableToTextDialog::separator() const
{
    return m_separator->separator();
}

QStringList TableToTextDialog::paragraphs() const
{
    QStringList paragraphs;
    const QChar separator = m_separator->separator();
    const bool spaced = separator == QLatin1Char(';') || separator == QLatin1Char(',');
    const QString between = spaced ? QString(separator) + QLatin1Char(' ') : QString(separator);
    for (const QStringList& row : m_rows) {
        if (m_separator->kind() == SeparatorChoice::Paragraphs) {
            for (const QString& cell : row) {
                paragraphs << cell.split(QChar(QChar::LineSeparator));
            }
            continue;
        }
        if (row.isEmpty()) {
            continue;
        }
        QStringList cells;
        for (const QString& cell : row) {
            cells << QString(cell).replace(QChar(QChar::LineSeparator), QLatin1Char(' '));
        }
        paragraphs << cells.join(between);
    }
    return paragraphs;
}

void TableToTextDialog::updateText()
{
    const QStringList paragraphs = this->paragraphs();
    const QChar separator = m_separator->separator();
    QString hint;
    if (m_separator->kind() == SeparatorChoice::Paragraphs) {
        //: Table to Text: %1 is the paragraphs ("12 paragraphs")
        hint = tr("Each cell becomes a paragraph: %1 in all.")
                   .arg(tr("%n paragraphs", nullptr, static_cast<int>(paragraphs.size())));
    } else if (!separator.isNull()) {
        // A cell with the character: back to a table, it divides otherwise
        QString found;
        for (const QStringList& row : m_rows) {
            for (const QString& cell : row) {
                if (found.isEmpty() && cell.contains(separator)) {
                    found = oneLine(cell);
                }
            }
        }
        if (!found.isEmpty()) {
            //: Table to Text: %1 is the text of a cell with the separator chosen
            hint = tr("Note: the character is also in the text of a cell (“%1”), so turning "
                      "the text back into a table divides it otherwise.")
                       .arg(found);
        } else if (separator == QLatin1Char('\t')) {
            hint = tr("The arrow in the preview stands for a tab.");
        } else if (separator == QLatin1Char(';') || separator == QLatin1Char(',')) {
            hint = tr("A space follows each semicolon and comma.");
        }
    }
    m_hint->setText(hint);
    m_hint->setVisible(!hint.isEmpty());

    // A tab shown as an arrow; the first rows
    QStringList shown = paragraphs.mid(0, PREVIEW_LINES * 4);
    for (QString& line : shown) {
        line.replace(QLatin1Char('\t'), QStringLiteral("→\t"));
    }
    m_preview->setPlainText(shown.join(QLatin1Char('\n')));
    acceptButton()->setEnabled(m_separator->isComplete());
}

// =============================================================================
// Table Properties...
// =============================================================================

TablePropertiesDialog::TablePropertiesDialog(const editor::TablePropertiesData& data,
                                             const QColor& automaticLines, QWidget* parent)
    : KalahariDialog(parent)
    , m_data(data)
    , m_automaticLines(automaticLines)
{
    setHeading(tr("Table Properties"),
               tr("The look of the selected table, its rows, columns and cells."));
    setHeadingIcon(QStringLiteral("table.properties"));
    setCompactHeading(true);
    setAcceptText(tr("OK"));
    // The automatic color of the lines and the header's are the colors of the editor
    addSettingsButton(QString::fromLatin1(SettingsDialog::EDITOR_COLORS_PAGE),
                      tr("Table Color Settings"));

    const int rows = static_cast<int>(m_data.rows.size());
    const int columns = static_cast<int>(m_data.columnWidths.size());
    m_row = std::clamp(m_data.row, 0, std::max(0, rows - 1));
    m_column = std::clamp(m_data.column, 0, std::max(0, columns - 1));
    m_columnShares = m_data.columnWidths;
    if (std::accumulate(m_columnShares.begin(), m_columnShares.end(), 0.0) <= 0.0) {
        std::fill(m_columnShares.begin(), m_columnShares.end(), 1.0);
    }

    m_tabs = new QTabWidget(this);
    //: Table Properties: the tabs. In Polish: &Tabela, &Wiersz, &Kolumna, Ko&mórka, &Obramowanie
    m_tabs->addTab(createTableTab(), tr("T&able"));
    m_tabs->addTab(createRowTab(), tr("Ro&w"));
    m_tabs->addTab(createColumnTab(), tr("Colu&mn"));
    m_tabs->addTab(createCellTab(), tr("C&ell"));
    m_tabs->addTab(createBordersTab(), tr("&Borders"));
    contentLayout()->addWidget(m_tabs, 1);

    // The width of the table gives the columns' widths in points
    connect(m_tabs, &QTabWidget::currentChanged, this, [this]() {
        readTableTab();
        updateColumnTab();
    });
    updateRowTab();
    updateColumnTab();
    updateBorders();
}

editor::TablePropertiesData TablePropertiesDialog::data() const
{
    editor::TablePropertiesData data = m_data;
    // The Table tab, and the columns in points of the table's width
    data.table.width = m_fixedWidth->isChecked() ? std::optional<double>(m_width->storedValue())
                                                 : std::nullopt;
    data.table.alignment = static_cast<editor::TableAlignment>(std::max(0, m_alignment->checkedId()));
    data.table.indent = data.table.alignment == editor::TableAlignment::Left ? m_indent->storedValue() : 0.0;
    const double total = std::accumulate(m_columnShares.begin(), m_columnShares.end(), 0.0);
    if (total > 0.0 && m_columnShares.size() == m_data.columnWidths.size()) {
        const double before =
            std::accumulate(m_data.columnWidths.begin(), m_data.columnWidths.end(), 0.0);
        for (size_t column = 0; column < m_columnShares.size(); ++column) {
            data.columnWidths[column] = m_columnShares[column] / total * before;
        }
    }
    return data;
}

void TablePropertiesDialog::showRow(int row)
{
    m_row = std::clamp(row, 0, std::max(0, static_cast<int>(m_data.rows.size()) - 1));
    updateRowTab();
}

void TablePropertiesDialog::showColumn(int column)
{
    m_column = std::clamp(column, 0, std::max(0, static_cast<int>(m_columnShares.size()) - 1));
    updateColumnTab();
}

QWidget* TablePropertiesDialog::createTableTab()
{
    auto* tab = new QWidget(m_tabs);
    auto* layout = new QVBoxLayout(tab);
    const double narrowest = editor::MIN_COLUMN_WIDTH * static_cast<double>(std::max<size_t>(1, m_columnShares.size()));
    const double textWidth = std::max(m_data.textWidth, narrowest);

    // The width: of the text, or fixed
    auto* widthGroup = new QGroupBox(tr("Width"), tab);
    auto* widthLayout = new QGridLayout(widthGroup);
    auto* width = new QButtonGroup(this);
    m_textWidth = radio(tr("&Text width"), width, 0, widthGroup);
    m_fixedWidth = radio(tr("&Fixed"), width, 1, widthGroup);
    m_width = new LengthSpinBox(LengthUnit::Points, narrowest, textWidth, widthGroup);
    m_width->setAccessibleName(tr("Width of the table"));
    m_width->setStoredValue(m_data.table.width.value_or(textWidth));
    (m_data.table.width ? m_fixedWidth : m_textWidth)->setChecked(true);
    m_width->setEnabled(m_fixedWidth->isChecked());
    connect(m_fixedWidth, &QRadioButton::toggled, m_width, &QWidget::setEnabled);
    widthLayout->addWidget(m_textWidth, 0, 0, 1, 2);
    widthLayout->addWidget(m_fixedWidth, 1, 0);
    widthLayout->addWidget(m_width, 1, 1);
    widthLayout->setColumnStretch(2, 1);
    layout->addWidget(widthGroup);

    // The alignment, and the indent of a table at the left
    auto* alignmentGroup = new QGroupBox(tr("Alignment"), tab);
    auto* alignmentLayout = new QGridLayout(alignmentGroup);
    m_alignment = new QButtonGroup(this);
    //: Table Properties: where the table is; in Polish: Do &lewej, Do ś&rodka, Do &prawej
    const QString alignments[] = {tr("&Left", "table alignment"), tr("&Center", "table alignment"),
                                  tr("&Right", "table alignment")};
    for (int alignment = 0; alignment < 3; ++alignment) {
        alignmentLayout->addWidget(radio(alignments[alignment], m_alignment, alignment, alignmentGroup),
                                   0, alignment);
    }
    m_alignment->button(static_cast<int>(m_data.table.alignment))->setChecked(true);
    auto* indentLabel = new QLabel(tr("&Indent from left"), alignmentGroup);
    m_indent = new LengthSpinBox(LengthUnit::Points, 0.0, std::max(0.0, textWidth - narrowest),
                                 alignmentGroup);
    m_indent->setStoredValue(m_data.table.indent);
    indentLabel->setBuddy(m_indent);
    const auto leftAligned = [this, indentLabel]() {
        const bool left = m_alignment->checkedId() == static_cast<int>(editor::TableAlignment::Left);
        m_indent->setEnabled(left);
        indentLabel->setEnabled(left);
    };
    connect(m_alignment, &QButtonGroup::idToggled, this, leftAligned);
    leftAligned();
    alignmentLayout->addWidget(indentLabel, 1, 0);
    alignmentLayout->addWidget(m_indent, 1, 1, 1, 2);
    alignmentLayout->setColumnStretch(3, 1);
    layout->addWidget(alignmentGroup);

    auto* note = new QLabel(tr("A table is never wider than the text."), tab);
    note->setWordWrap(true);
    note->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(note);
    layout->addStretch(1);
    return tab;
}

QWidget* TablePropertiesDialog::createRowTab()
{
    auto* tab = new QWidget(m_tabs);
    auto* layout = new QVBoxLayout(tab);
    m_rowName = new QLabel(tab);
    m_previousRow = new QPushButton(tr("&Previous", "row"), tab);
    m_nextRow = new QPushButton(tr("&Next", "row"), tab);
    m_previousRow->setAutoDefault(false);
    m_nextRow->setAutoDefault(false);
    connect(m_previousRow, &QPushButton::clicked, this, [this]() { showRow(m_row - 1); });
    connect(m_nextRow, &QPushButton::clicked, this, [this]() { showRow(m_row + 1); });
    layout->addLayout(navigationRow(m_rowName, m_previousRow, m_nextRow));

    // The height: of its text, or at least a length
    auto* heightGroup = new QGroupBox(tr("Height"), tab);
    auto* heightLayout = new QGridLayout(heightGroup);
    auto* height = new QButtonGroup(this);
    m_autoHeight = radio(tr("A&utomatic"), height, 0, heightGroup);
    m_minHeight = radio(tr("At &least"), height, 1, heightGroup);
    m_height = new LengthSpinBox(LengthUnit::Points, 1.0, editor::MAX_ROW_HEIGHT, heightGroup);
    m_height->setAccessibleName(tr("Height of the row"));
    heightLayout->addWidget(m_autoHeight, 0, 0, 1, 2);
    heightLayout->addWidget(m_minHeight, 1, 0);
    heightLayout->addWidget(m_height, 1, 1);
    heightLayout->setColumnStretch(2, 1);
    layout->addWidget(heightGroup);

    auto* pagesGroup = new QGroupBox(tr("On the pages"), tab);
    auto* pagesLayout = new QVBoxLayout(pagesGroup);
    m_headerRow = new QCheckBox(tr("Repeat as &header row at the top of each page"), pagesGroup);
    m_allowSplit = new QCheckBox(tr("Allow the row to &split across pages"), pagesGroup);
    pagesLayout->addWidget(m_headerRow);
    pagesLayout->addWidget(m_allowSplit);
    layout->addWidget(pagesGroup);
    layout->addStretch(1);

    const auto row = [this]() -> editor::TableRowProperties& {
        return m_data.rows[static_cast<size_t>(m_row)];
    };
    connect(m_minHeight, &QRadioButton::toggled, this, [this, row](bool checked) {
        m_height->setEnabled(checked);
        if (!m_updating && !m_data.rows.empty()) {
            row().minHeight = checked ? m_height->storedValue() : 0.0;
        }
    });
    connect(m_height, &QDoubleSpinBox::valueChanged, this, [this, row]() {
        if (!m_updating && !m_data.rows.empty() && m_minHeight->isChecked()) {
            row().minHeight = m_height->storedValue();
        }
    });
    // The header rows are the first ones: a row joins them or leaves with those after it
    connect(m_headerRow, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_updating) {
            m_data.headerRows = checked ? m_row + 1 : m_row;
            updateRowTab();
        }
    });
    connect(m_allowSplit, &QCheckBox::toggled, this, [this, row](bool checked) {
        if (!m_updating && !m_data.rows.empty()) {
            row().allowSplit = checked;
        }
    });
    return tab;
}

QWidget* TablePropertiesDialog::createColumnTab()
{
    auto* tab = new QWidget(m_tabs);
    auto* layout = new QVBoxLayout(tab);
    m_columnName = new QLabel(tab);
    m_previousColumn = new QPushButton(tr("&Previous", "column"), tab);
    m_nextColumn = new QPushButton(tr("&Next", "column"), tab);
    m_previousColumn->setAutoDefault(false);
    m_nextColumn->setAutoDefault(false);
    connect(m_previousColumn, &QPushButton::clicked, this, [this]() { showColumn(m_column - 1); });
    connect(m_nextColumn, &QPushButton::clicked, this, [this]() { showColumn(m_column + 1); });
    layout->addLayout(navigationRow(m_columnName, m_previousColumn, m_nextColumn));

    auto* widthGroup = new QGroupBox(tr("Width"), tab);
    auto* widthLayout = new QGridLayout(widthGroup);
    auto* widthLabel = new QLabel(tr("Column w&idth"), widthGroup);
    m_columnWidth = new LengthSpinBox(LengthUnit::Points, editor::MIN_COLUMN_WIDTH,
                                      std::max(m_data.textWidth, editor::MIN_COLUMN_WIDTH),
                                      widthGroup);
    widthLabel->setBuddy(m_columnWidth);
    auto* distribute = new QPushButton(tr("&Distribute Columns Evenly"), widthGroup);
    distribute->setAutoDefault(false);
    widthLayout->addWidget(widthLabel, 0, 0);
    widthLayout->addWidget(m_columnWidth, 0, 1);
    widthLayout->addWidget(distribute, 1, 0, 1, 2, Qt::AlignLeft);
    widthLayout->setColumnStretch(2, 1);
    layout->addWidget(widthGroup);
    if (m_columnShares.size() < 2) {
        m_columnWidth->setEnabled(false);
        distribute->setEnabled(false);
        m_columnWidth->setToolTip(tr("The only column is as wide as the table: the Table tab"));
    }

    auto* note = new QLabel(tr("Dragging the edge of a column with the mouse also changes its width."), tab);
    note->setWordWrap(true);
    note->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(note);
    layout->addStretch(1);

    connect(m_columnWidth, &QDoubleSpinBox::valueChanged, this, [this]() {
        if (!m_updating) {
            setColumnWidth(m_columnWidth->storedValue());
        }
    });
    connect(distribute, &QPushButton::clicked, this, [this]() {
        std::fill(m_columnShares.begin(), m_columnShares.end(), 1.0);
        updateColumnTab();
    });
    return tab;
}

QWidget* TablePropertiesDialog::createCellTab()
{
    auto* tab = new QWidget(m_tabs);
    auto* layout = new QVBoxLayout(tab);

    auto* alignmentGroup = new QGroupBox(tr("Vertical alignment"), tab);
    auto* alignmentLayout = new QHBoxLayout(alignmentGroup);
    m_cellAlignment = new QButtonGroup(this);
    //: Table Properties: where the text of a cell is; in Polish: Do &góry, Do ś&rodka, Do &dołu
    const QString alignments[] = {tr("&Top", "cell alignment"), tr("&Center", "cell alignment"),
                                  tr("B&ottom", "cell alignment")};
    for (int alignment = 0; alignment < 3; ++alignment) {
        alignmentLayout->addWidget(
            radio(alignments[alignment], m_cellAlignment, alignment, alignmentGroup));
    }
    alignmentLayout->addStretch(1);
    m_cellAlignment->button(static_cast<int>(m_data.cell.alignment))->setChecked(true);
    connect(m_cellAlignment, &QButtonGroup::idToggled, this, [this](int id, bool checked) {
        if (checked) {
            m_data.cell.alignment = static_cast<editor::CellAlignment>(id);
            m_data.cellChanged = true;
        }
    });
    layout->addWidget(alignmentGroup);

    auto* backgroundGroup = new QGroupBox(tr("Background"), tab);
    auto* backgroundLayout = new QHBoxLayout(backgroundGroup);
    auto* backgroundLabel = new QLabel(tr("Back&ground color"), backgroundGroup);
    m_backgroundButton = new QPushButton(backgroundGroup);
    m_backgroundButton->setAutoDefault(false);
    backgroundLabel->setBuddy(m_backgroundButton);
    m_backgroundMenu = new ColorMenu(
        ColorMenu::Kind::Highlight, [this]() { return m_data.cell.background; }, this);
    m_backgroundButton->setMenu(m_backgroundMenu);
    connect(m_backgroundMenu, &ColorMenu::colorChosen, this, [this](const QColor& color) {
        m_data.cell.background = color;
        m_data.cellChanged = true;
        showColor(m_backgroundButton, color, true);
    });
    showColor(m_backgroundButton, m_data.cell.background, true);
    backgroundLayout->addWidget(backgroundLabel);
    backgroundLayout->addWidget(m_backgroundButton);
    backgroundLayout->addStretch(1);
    layout->addWidget(backgroundGroup);

    auto* note = new QLabel(tr("For the selected cells; without a selection, the cell with the cursor."), tab);
    note->setWordWrap(true);
    note->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(note);
    layout->addStretch(1);
    return tab;
}

QWidget* TablePropertiesDialog::createBordersTab()
{
    auto* tab = new QWidget(m_tabs);
    auto* layout = new QVBoxLayout(tab);
    editor::TableBorders& borders = m_data.table.borders;

    // The setting: tiles of the usual lines, and Custom
    auto* settingGroup = new QGroupBox(tr("Setting"), tab);
    auto* settingLayout = new QHBoxLayout(settingGroup);
    m_setting = new QButtonGroup(this);
    const QColor color = palette().color(QPalette::Text);
    const QColor faint = palette().color(QPalette::Mid);
    const std::pair<QString, int> tiles[] = {
        {tr("&None"), editor::BORDER_NONE},
        {tr("Bo&x"), editor::BORDER_BOX},
        {tr("&Grid"), editor::BORDER_GRID},
        {tr("C&ustom"), editor::BORDER_TOP | editor::BORDER_BOTTOM | editor::BORDER_ROWS}};
    for (int setting = SettingNone; setting <= SettingCustom; ++setting) {
        auto* tile = new QToolButton(settingGroup);
        tile->setText(tiles[setting].first);
        tile->setIcon(settingIcon(tiles[setting].second, color, faint));
        tile->setIconSize(QSize(TILE_ICON_WIDTH, TILE_ICON_HEIGHT));
        tile->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        tile->setCheckable(true);
        tile->setAutoRaise(true);
        m_setting->addButton(tile, setting);
        settingLayout->addWidget(tile);
    }
    settingLayout->addStretch(1);
    layout->addWidget(settingGroup);

    // The lines, one by one
    auto* linesGroup = new QGroupBox(tr("Lines"), tab);
    auto* linesLayout = new QGridLayout(linesGroup);
    //: Table Properties: the lines of the borders; in Polish: &Górna, &Dolna, L&ewa, &Prawa
    const QString lineNames[] = {tr("&Top", "border line"),  tr("B&ottom", "border line"),
                                 tr("&Left", "border line"), tr("&Right", "border line"),
                                 tr("Inside hori&zontal"),   tr("Inside &vertical")};
    for (size_t line = 0; line < std::size(BORDER_LINES); ++line) {
        auto* box = new QCheckBox(lineNames[line], linesGroup);
        m_lines.push_back(box);
        linesLayout->addWidget(box, static_cast<int>(line % 2), static_cast<int>(line / 2));
        connect(box, &QCheckBox::toggled, this, [this, line](bool checked) {
            if (!m_updating) {
                int& lines = m_data.table.borders.lines;
                lines = checked ? lines | BORDER_LINES[line] : lines & ~BORDER_LINES[line];
                updateBorders();
            }
        });
    }
    linesLayout->setColumnStretch(3, 1);
    layout->addWidget(linesGroup);

    // The style, the width and the color of the lines
    auto* lineGrid = new QGridLayout();
    m_lineStyle = new QComboBox(tab);
    m_lineStyle->addItems({tr("Solid line"), tr("Dashed line"), tr("Dotted line"), tr("Double line")});
    m_lineStyle->setCurrentIndex(static_cast<int>(borders.style));
    m_lineWidth = new QComboBox(tab);
    for (const double width : LINE_WIDTHS) {
        m_lineWidth->addItem(lineWidthText(width), width);
    }
    int widthIndex = -1;
    for (int i = 0; i < m_lineWidth->count(); ++i) {
        if (std::abs(m_lineWidth->itemData(i).toDouble() - borders.width) < SAME_WIDTH) {
            widthIndex = i;
        }
    }
    if (widthIndex < 0) {
        // A width of its own, among the others in its place
        widthIndex = static_cast<int>(std::count_if(std::begin(LINE_WIDTHS), std::end(LINE_WIDTHS),
                                                    [&borders](double width) { return width < borders.width; }));
        m_lineWidth->insertItem(widthIndex, lineWidthText(borders.width), borders.width);
    }
    m_lineWidth->setCurrentIndex(widthIndex);
    m_lineColorButton = new QPushButton(tab);
    m_lineColorButton->setAutoDefault(false);
    m_lineColorMenu = new ColorMenu(
        ColorMenu::Kind::Text, [this]() { return m_data.table.borders.color; }, this);
    m_lineColorButton->setMenu(m_lineColorMenu);
    const std::pair<QString, QWidget*> fields[] = {{tr("&Style"), m_lineStyle},
                                                   {tr("W&idth"), m_lineWidth},
                                                   {tr("&Color"), m_lineColorButton}};
    for (int field = 0; field < 3; ++field) {
        auto* label = new QLabel(fields[field].first, tab);
        label->setBuddy(fields[field].second);
        lineGrid->addWidget(label, field, 0);
        lineGrid->addWidget(fields[field].second, field, 1);
    }
    m_bordersPreview = new BordersPreview(tab);
    m_bordersPreview->setAccessibleName(tr("Preview of the borders"));
    m_bordersPreview->setToolTip(tr("A click on a line takes it away or draws it"));
    m_bordersPreview->lineClicked = [this](editor::TableBorderLine line) {
        m_data.table.borders.lines ^= line;
        updateBorders();
    };
    lineGrid->addWidget(m_bordersPreview, 0, 2, 3, 1, Qt::AlignRight | Qt::AlignTop);
    lineGrid->setColumnStretch(1, 1);
    layout->addLayout(lineGrid);
    layout->addStretch(1);

    connect(m_setting, &QButtonGroup::idClicked, this, [this](int setting) {
        if (setting != SettingCustom) {
            m_data.table.borders.lines = setting == SettingNone  ? editor::BORDER_NONE
                                         : setting == SettingBox ? editor::BORDER_BOX
                                                                 : editor::BORDER_GRID;
        } else if (settingOf(m_data.table.borders.lines) != SettingCustom) {
            // Custom from the lines as they are, or the lines of its tile
            m_data.table.borders.lines = editor::BORDER_TOP | editor::BORDER_BOTTOM | editor::BORDER_ROWS;
        }
        updateBorders();
    });
    connect(m_lineStyle, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_data.table.borders.style = static_cast<editor::BorderStyle>(std::max(0, index));
        updateBorders();
    });
    connect(m_lineWidth, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_data.table.borders.width = m_lineWidth->itemData(index).toDouble();
        updateBorders();
    });
    connect(m_lineColorMenu, &ColorMenu::colorChosen, this, [this](const QColor& color) {
        m_data.table.borders.color = color;
        updateBorders();
    });
    return tab;
}

double TablePropertiesDialog::tableWidth() const
{
    // As the layout makes it (tableColumnEdges())
    const double textWidth = m_data.textWidth;
    const bool left = m_alignment->checkedId() == static_cast<int>(editor::TableAlignment::Left);
    const double indent = left ? std::clamp(m_indent->storedValue(), 0.0, textWidth) : 0.0;
    const double width = m_fixedWidth->isChecked() ? m_width->storedValue() : textWidth - indent;
    return std::max(0.0, std::min(width, textWidth - indent));
}

void TablePropertiesDialog::readTableTab()
{
    m_data.table.width = m_fixedWidth->isChecked() ? std::optional<double>(m_width->storedValue())
                                                   : std::nullopt;
    m_data.table.alignment = static_cast<editor::TableAlignment>(std::max(0, m_alignment->checkedId()));
    m_data.table.indent =
        m_data.table.alignment == editor::TableAlignment::Left ? m_indent->storedValue() : 0.0;
}

void TablePropertiesDialog::updateRowTab()
{
    const int rows = static_cast<int>(m_data.rows.size());
    if (rows == 0) {
        return;
    }
    m_updating = true;
    const bool header = m_row < m_data.headerRows;
    //: Table Properties: the row shown, "Row 1 of 4"
    const QString name = tr("Row %1 of %2").arg(m_row + 1).arg(rows);
    //: Table Properties: the row shown is a header row, "Row 1 of 4 (header)"
    m_rowName->setText(header ? tr("%1 (header)").arg(name) : name);
    m_previousRow->setEnabled(m_row > 0);
    m_nextRow->setEnabled(m_row + 1 < rows);
    const editor::TableRowProperties& row = m_data.rows[static_cast<size_t>(m_row)];
    (row.minHeight > 0.0 ? m_minHeight : m_autoHeight)->setChecked(true);
    m_height->setEnabled(row.minHeight > 0.0);
    m_height->setStoredValue(row.minHeight > 0.0 ? row.minHeight : DEFAULT_MIN_HEIGHT);
    // A row after the header rows can join them; the rows under them cannot
    m_headerRow->setChecked(header);
    m_headerRow->setEnabled(m_row <= m_data.headerRows);
    m_headerRow->setToolTip(m_row <= m_data.headerRows
                                ? QString()
                                : tr("The header rows are the first rows of the table: make the "
                                     "rows above it header rows first"));
    m_allowSplit->setChecked(row.allowSplit);
    m_updating = false;
}

void TablePropertiesDialog::updateColumnTab()
{
    const int columns = static_cast<int>(m_columnShares.size());
    if (columns == 0) {
        return;
    }
    m_updating = true;
    //: Table Properties: the column shown, "Column 2 of 3"
    m_columnName->setText(tr("Column %1 of %2").arg(m_column + 1).arg(columns));
    m_previousColumn->setEnabled(m_column > 0);
    m_nextColumn->setEnabled(m_column + 1 < columns);
    const double total = std::accumulate(m_columnShares.begin(), m_columnShares.end(), 0.0);
    m_columnWidth->setStoredValue(total > 0.0 ? m_columnShares[static_cast<size_t>(m_column)] / total * tableWidth()
                                              : 0.0);
    m_updating = false;
}

void TablePropertiesDialog::setColumnWidth(double width)
{
    const size_t columns = m_columnShares.size();
    if (columns < 2) {
        return;
    }
    const double total = std::accumulate(m_columnShares.begin(), m_columnShares.end(), 0.0);
    const double table = tableWidth();
    if (total <= 0.0 || table <= 0.0) {
        return;
    }
    // In points; the neighbour keeps at least the narrowest width
    const auto column = static_cast<size_t>(m_column);
    const size_t neighbour = column + 1 < columns ? column + 1 : column - 1;
    std::vector<double> widths;
    for (const double share : m_columnShares) {
        widths.push_back(share / total * table);
    }
    const double both = widths[column] + widths[neighbour];
    const double narrowest = std::min(editor::MIN_COLUMN_WIDTH, both / 2.0);
    widths[column] = std::clamp(width, narrowest, both - narrowest);
    widths[neighbour] = both - widths[column];
    m_columnShares = widths;
    if (std::abs(widths[column] - width) > SAME_WIDTH) {
        updateColumnTab();  // the field shows the width it got
    }
}

void TablePropertiesDialog::updateBorders()
{
    const editor::TableBorders& borders = m_data.table.borders;
    m_updating = true;
    m_setting->button(settingOf(borders.lines))->setChecked(true);
    for (size_t line = 0; line < m_lines.size(); ++line) {
        m_lines[line]->setChecked((borders.lines & BORDER_LINES[line]) != 0);
    }
    m_updating = false;
    const bool any = borders.lines != editor::BORDER_NONE;
    m_lineStyle->setEnabled(any);
    m_lineWidth->setEnabled(any);
    m_lineColorButton->setEnabled(any);
    showColor(m_lineColorButton, borders.color, false);
    m_bordersPreview->showBorders(borders, borders.color.isValid() ? borders.color : m_automaticLines);
}

void TablePropertiesDialog::showColor(QPushButton* button, const QColor& color, bool background)
{
    const int side = button->fontMetrics().height();
    const QColor border = palette().color(QPalette::Mid);
    if (!color.isValid()) {
        button->setIcon(background ? core::ArtProvider::getInstance().getIcon(
                                         QStringLiteral("format.noColor"))
                                   : swatchIcon(m_automaticLines, border, side));
        //: The color of a cell's background: none; of the lines: the editor's
        button->setText(background ? tr("No Color") : tr("Automatic"));
        return;
    }
    button->setIcon(swatchIcon(color, border, side));
    const QString name = paletteColorName(colorPalettes(), color);
    button->setText(name.isEmpty() ? color.name().toUpper() : name);
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
