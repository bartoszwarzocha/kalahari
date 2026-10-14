/// @file dialog_parts.h
/// @brief Parts that the program's own dialogs share: a list as high as its rows, a note with
/// an icon, and the Copy button

#pragma once

#include <QSize>
#include <QString>
#include <QTreeWidget>

#include <functional>

class QLabel;
class QPushButton;
class QWidget;

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief A list of a dialog, as high as its rows up to MAXIMUM_ROWS; the other rows scroll,
/// and on a small screen the list keeps MINIMUM_ROWS
class RowList : public QTreeWidget {
public:
    static constexpr int MAXIMUM_ROWS = 8;  ///< Rows the list shows at most
    static constexpr int MINIMUM_ROWS = 3;  ///< Rows the list keeps on a small screen

    using QTreeWidget::QTreeWidget;

    /// @brief Set the number of rows the list has; its height follows it
    void setRows(int rows);

    /// @brief As high as its rows, up to MAXIMUM_ROWS
    QSize sizeHint() const override;

    /// @brief As high as its rows, up to MINIMUM_ROWS
    QSize minimumSizeHint() const override;

private:
    /// @brief The height of @p rows rows, with the frame
    int heightFor(int rows) const;

    int m_rows = 0;
};

/// @brief A list of rows of two columns, whose rows cannot be selected: a title with its icon,
/// and at the right a text in grey (the name of a file, a status) that the caller sets
/// @param parent Parent widget
/// @param accessibleName Name of the list for screen readers
RowList* newRowList(QWidget* parent, const QString& accessibleName);

/// @brief A note of a dialog: an icon, and a sentence beside it, which can be selected
/// @param parent Parent widget
/// @param icon Gets the label of the icon, which setNoteIcon() sets
/// @param text Gets the label of the sentence
/// @return The note, for the layout of the dialog
QWidget* newNote(QWidget* parent, QLabel** icon, QLabel** text);

/// @brief Set the icon of a note
/// @param icon The label of the icon that newNote() gave
/// @param iconId ArtProvider id (e.g. "common.warning")
void setNoteIcon(QLabel* icon, const QString& iconId);

/// @brief Make @p button the Copy button of a dialog: it puts @p text() on the clipboard and
/// says so for a while
void setUpCopyButton(QPushButton* button, const std::function<QString()>& text);

} // namespace dialogs
} // namespace gui
} // namespace kalahari
