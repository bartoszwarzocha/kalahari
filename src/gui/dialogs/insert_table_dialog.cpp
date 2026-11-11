/// @file insert_table_dialog.cpp
/// @brief Insert > Table...: the columns and rows of a new table

#include "kalahari/gui/dialogs/insert_table_dialog.h"

#include "kalahari/editor/book_editor.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr int DEFAULT_COLUMNS = 3;
constexpr int DEFAULT_ROWS = 3;

// The picture of the table: its width, the height of a row, and the rows it shows at most
// (a longer table goes on below it)
constexpr int PREVIEW_WIDTH = 240;
constexpr int PREVIEW_ROW_HEIGHT = 14;
constexpr int PREVIEW_MAX_ROWS = 6;

} // namespace

/// @brief The grid of the new table: its columns across the width, its first rows, the header
///        row shaded
class TablePreview : public QWidget {
public:
    explicit TablePreview(QWidget* parent)
        : QWidget(parent)
    {
        setFixedSize(PREVIEW_WIDTH + 1, PREVIEW_MAX_ROWS * PREVIEW_ROW_HEIGHT + 1);
    }

    void showTable(int columns, int rows, bool headerRow)
    {
        m_columns = std::max(1, columns);
        m_rows = std::max(1, rows);
        m_headerRow = headerRow;
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        const QPalette& colors = palette();
        const int shown = std::min(m_rows, PREVIEW_MAX_ROWS);
        const int height = shown * PREVIEW_ROW_HEIGHT;
        painter.fillRect(QRect(0, 0, PREVIEW_WIDTH, height), colors.base());
        if (m_headerRow) {
            painter.fillRect(QRect(0, 0, PREVIEW_WIDTH, PREVIEW_ROW_HEIGHT), colors.button());
        }
        painter.setPen(colors.mid().color());
        for (int row = 0; row <= shown; ++row) {
            painter.drawLine(0, row * PREVIEW_ROW_HEIGHT, PREVIEW_WIDTH, row * PREVIEW_ROW_HEIGHT);
        }
        for (int column = 0; column <= m_columns; ++column) {
            const int x = column * PREVIEW_WIDTH / m_columns;
            painter.drawLine(x, 0, x, height);
        }
    }

private:
    int m_columns = DEFAULT_COLUMNS;
    int m_rows = DEFAULT_ROWS;
    bool m_headerRow = true;
};

InsertTableDialog::InsertTableDialog(QWidget* parent)
    : KalahariDialog(parent)
{
    setHeading(tr("Insert Table"), tr("The table is as wide as the text, its columns equal."));
    setHeadingIcon(QStringLiteral("insert.table"));
    setAcceptText(tr("Insert"));

    // The columns and the rows side by side, each with its label before it
    m_columns = new QSpinBox(this);
    m_columns->setRange(1, editor::BookEditor::MAX_INSERTED_TABLE_COLUMNS);
    m_columns->setValue(DEFAULT_COLUMNS);
    m_rows = new QSpinBox(this);
    m_rows->setRange(1, editor::BookEditor::MAX_INSERTED_TABLE_ROWS);
    m_rows->setValue(DEFAULT_ROWS);
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

    m_headerRow = new QCheckBox(tr("&Header row"), this);
    m_headerRow->setChecked(true);
    contentLayout()->addWidget(m_headerRow);

    m_preview = new TablePreview(this);
    m_preview->setAccessibleName(tr("Preview of the table"));
    contentLayout()->addWidget(m_preview);
    contentLayout()->addStretch(1);

    connect(m_columns, &QSpinBox::valueChanged, this, &InsertTableDialog::updatePreview);
    connect(m_rows, &QSpinBox::valueChanged, this, &InsertTableDialog::updatePreview);
    connect(m_headerRow, &QCheckBox::toggled, this, &InsertTableDialog::updatePreview);
    updatePreview();

    m_columns->setFocus();
    m_columns->selectAll();
}

int InsertTableDialog::columns() const
{
    return m_columns->value();
}

int InsertTableDialog::rows() const
{
    return m_rows->value();
}

bool InsertTableDialog::headerRow() const
{
    return m_headerRow->isChecked();
}

void InsertTableDialog::updatePreview()
{
    m_preview->showTable(columns(), rows(), headerRow());
    // What the picture shows, for a screen reader
    const QString columnsText = tr("%n columns", nullptr, columns());
    const QString rowsText = tr("%n rows", nullptr, rows());
    //: %1 is the columns ("3 columns"), %2 the rows ("3 rows")
    m_preview->setAccessibleDescription(headerRow()
                                            ? tr("%1 and %2, with a header row").arg(columnsText, rowsText)
                                            : tr("%1 and %2").arg(columnsText, rowsText));
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
