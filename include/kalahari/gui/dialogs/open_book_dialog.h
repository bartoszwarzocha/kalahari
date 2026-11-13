/// @file open_book_dialog.h
/// @brief The question "Opening a Book": where a book goes when another one is open in the
/// window

#pragma once

#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QString>

class QCheckBox;
class QPushButton;

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief Where the book chosen goes while another one is open in the window: in a new window
/// (the default button, Enter), in this window in place of the open one, or nowhere (Cancel)
///
/// "Remember my choice" keeps the answer in the settings (Settings > General > Windows), so the
/// question is not asked again; the gear opens that page.
class OpenBookDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief What the writer chose
    enum class Choice {
        NewWindow,   ///< The book opens in a new window; the open one stays
        ThisWindow,  ///< The open book closes and the book opens in its place
        Cancel       ///< Nothing opens
    };

    /// @brief The question
    /// @param openBook The title of the book open in the window
    /// @param otherBook The name of the book chosen
    /// @param parent The window
    OpenBookDialog(const QString& openBook, const QString& otherBook, QWidget* parent = nullptr);

    /// @brief What the writer chose; Cancel while the window is open
    Choice choice() const { return m_choice; }

    /// @brief Whether the choice is to be remembered, so that it is not asked again
    bool remembersChoice() const;

    /// @brief The button In This Window
    QPushButton* thisWindowButton() const { return m_thisWindowButton; }

    /// @brief The checkbox Remember my choice
    QCheckBox* rememberBox() const { return m_rememberBox; }

private:
    QPushButton* m_thisWindowButton = nullptr;
    QCheckBox* m_rememberBox = nullptr;
    Choice m_choice = Choice::Cancel;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
