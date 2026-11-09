/// @file insert_table_dialog.h
/// @brief Insert > Table...: the columns and rows of a new table

#pragma once

#include "kalahari/gui/dialogs/kalahari_dialog.h"

class QCheckBox;
class QSpinBox;

namespace kalahari {
namespace gui {
namespace dialogs {

class TablePreview;

/// @brief Insert > Table...: the columns and rows of a new table, and its header row
///
/// The table is as wide as the text, its columns equal; a small picture shows its grid as
/// the numbers change. Without a gear: the dialog has no settings in the Settings dialog.
class InsertTableDialog : public KalahariDialog {
    Q_OBJECT

public:
    explicit InsertTableDialog(QWidget* parent = nullptr);

    /// @brief The columns chosen (3 at first)
    [[nodiscard]] int columns() const;

    /// @brief The rows chosen (3 at first)
    [[nodiscard]] int rows() const;

    /// @brief Whether the first row is a header row (it is at first)
    [[nodiscard]] bool headerRow() const;

private:
    /// @brief The picture follows the numbers and the header row
    void updatePreview();

    QSpinBox* m_columns = nullptr;
    QSpinBox* m_rows = nullptr;
    QCheckBox* m_headerRow = nullptr;
    TablePreview* m_preview = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
