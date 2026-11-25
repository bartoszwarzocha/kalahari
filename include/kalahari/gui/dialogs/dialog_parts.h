// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file dialog_parts.h
/// @brief Parts that the program's own dialogs share: a list as high as its rows, the keys of
/// a list with a filter, a note with an icon, and the Copy button

#ifndef KALAHARI_GUI_DIALOGS_DIALOG_PARTS_H
#define KALAHARI_GUI_DIALOGS_DIALOG_PARTS_H

#include <QSize>
#include <QString>
#include <QTabWidget>
#include <QTreeWidget>

#include <functional>

class QAbstractButton;
class QAbstractItemView;
class QEvent;
class QFontMetrics;
class QLabel;
class QLineEdit;
class QObject;
class QPushButton;
class QWidget;

namespace kalahari::gui::dialogs {

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
    [[nodiscard]] QSize sizeHint() const override;

    /// @brief As high as its rows, up to MINIMUM_ROWS
    [[nodiscard]] QSize minimumSizeHint() const override;

private:
    /// @brief The height of @p rows rows, with the frame
    [[nodiscard]] int heightFor(int rows) const;

    int m_rows = 0;
};

/// @brief A list of rows of two columns, whose rows cannot be selected: a title with its icon,
/// and at the right a text in grey (the name of a file, a status) that the caller sets
/// @param parent Parent widget
/// @param accessibleName Name of the list for screen readers
RowList* newRowList(QWidget* parent, const QString& accessibleName);

/// @brief The keys of list @p list with filter @p filter above it: the letters typed in the
/// list go to the filter, and Backspace takes the last one back; the arrows and the page keys
/// of the filter choose in the list, which gets the keys
/// @param watched What @p event is for: the list, the filter or another part of the dialog
/// @return Whether it took @p event
bool takeFilterKeys(const QObject* watched, const QEvent* event, QLineEdit* filter,
                    QAbstractItemView* list);

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

/// @brief The width of @p letters letters of a text in the font of @p metrics
///
/// Not averageCharWidth(): fonts give it differently (DejaVu Sans 6 pixels, Inter 8, for text
/// that takes 5.8 and 5.5), and a window would wrap its sentences into more lines in one font
/// than in another.
int widthOfLetters(const QFontMetrics& metrics, int letters);

/// @brief Tabs as high as their highest page at the width they have
///
/// A page of wrapped sentences asks for the lines its sentences take there, whichever page is
/// in front. A plain QTabWidget asks so only for the page in front, and for the others takes
/// their own hints, which wrap the sentences at a narrower width into more lines. A hidden
/// tab (the options of a format that is not chosen) does not count.
class FittingTabs : public QTabWidget {
public:
    using QTabWidget::QTabWidget;

    [[nodiscard]] bool hasHeightForWidth() const override { return true; }
    [[nodiscard]] int heightForWidth(int width) const override;
    [[nodiscard]] QSize sizeHint() const override;
};

/// @brief @p text in the label of a button: an '&' shows as itself, not as an access key
QString literal(QString text);

/// @brief A grey sentence under an option or a list
QLabel* newRemark(QWidget* parent, const QString& text);

/// @brief Button @p button (a check box, a radio button) with @p remark right under it, in
/// line with its text; @p beside, when given, on the line of the button after it (an option
/// of its own, which saves a line)
/// @return The button with its remark, for the layout of the dialog
QWidget* withRemark(QAbstractButton* button, QLabel* remark, QWidget* beside = nullptr);

} // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_DIALOG_PARTS_H
