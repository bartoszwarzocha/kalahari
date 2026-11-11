/// @file deadline_dialog.h
/// @brief The small window of Add Deadline and Change Deadline, on the tab Writing Goals and
/// Deadlines

#pragma once

#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <kalahari/core/writing_goals.h>

#include <QList>
#include <QString>

class QComboBox;
class QDateEdit;
class QLineEdit;
class QSpinBox;

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief A part of the book that a deadline can be about: a group or a text of it
struct BookPart {
    QString id;     ///< The id of its element
    QString title;  ///< Its title
    int depth = 0;  ///< 0: in a part of the book, 1: in a group of it, ...

    bool operator==(const BookPart&) const = default;
};

/// @brief A deadline of a book: what is due, by which day, which part of the book and how many
/// words it should have by then
///
/// The deadline needs a name; the part is the whole book or one of @p parts, and the words
/// may be none. Enter adds or changes it, Esc gives up.
class DeadlineDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief The window of Add Deadline (@p deadline has no name) or Change Deadline
    /// @param deadline What the fields start with; a new one has no name and the day a month
    /// from today
    /// @param parts The parts of the book, in reading order
    /// @param parent Parent widget
    DeadlineDialog(const core::WritingDeadline& deadline, const QList<BookPart>& parts,
                   QWidget* parent = nullptr);

    /// @brief The deadline of the fields: its name without spaces around it
    core::WritingDeadline deadline() const;

private:
    /// @brief Add or Change only with a name
    void updateAcceptButton();

    QLineEdit* m_nameEdit = nullptr;
    QDateEdit* m_dateEdit = nullptr;
    QComboBox* m_partBox = nullptr;
    QSpinBox* m_wordsBox = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
