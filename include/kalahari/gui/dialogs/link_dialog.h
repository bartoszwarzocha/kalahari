/// @file link_dialog.h
/// @brief Insert > Link... (Ctrl+K): the text of a link and its address

#pragma once

#include "kalahari/gui/dialogs/kalahari_dialog.h"

class QLabel;
class QLineEdit;

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief Insert > Link... (Ctrl+K): the text a link shows and the address it opens
///
/// On a link of the text it edits it, and Remove Link takes it off the text. OK is enabled
/// once an address is written. The gear opens the colors of the editor in the Settings
/// dialog, where the color of links is chosen.
class LinkDialog : public KalahariDialog {
    Q_OBJECT

public:
    explicit LinkDialog(QWidget* parent = nullptr);

    /// @brief Show a link, or what becomes one
    /// @param text The text it shows
    /// @param address Its address; empty for a new link
    /// @param existing A link of the text: the dialog edits it and can remove it
    /// @param textEditable false: the text stays as it is (a selection over paragraphs)
    void setLink(const QString& text, const QString& address, bool existing, bool textEditable);

    /// @brief The text written in Text to Display
    [[nodiscard]] QString text() const;

    /// @brief The address written in Address, as written
    [[nodiscard]] QString address() const;

    /// @brief Whether Remove Link closed the dialog
    [[nodiscard]] bool removeAsked() const { return m_removeAsked; }

private:
    /// @brief OK only with an address
    void updateAcceptButton();

    QLineEdit* m_text = nullptr;
    QLineEdit* m_address = nullptr;
    QPushButton* m_remove = nullptr;
    bool m_removeAsked = false;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
