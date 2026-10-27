/// @file quick_insert_item_dialog.h
/// @brief The window of a text fragment or a typographic character of quick insert (Settings >
///        Editor > Quick Insert: Add..., Edit...)

#pragma once

#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <kalahari/editor/quick_insert.h>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief The name, the text and the kind of a fragment or a character of quick insert
///
/// The name is what the writer types after @ or # to find it; the text is what it inserts,
/// also in several paragraphs. The window cannot be accepted without both.
class QuickInsertItemDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief The window of a new item (Add...) or of an item to change (Edit...)
    /// @param item The item to change; none (an empty name): a new text fragment
    /// @param parent Parent widget
    explicit QuickInsertItemDialog(const editor::QuickInsertItem& item, QWidget* parent = nullptr);

    /// @brief The item as the window has it: the name without spaces around it
    [[nodiscard]] editor::QuickInsertItem item() const;

private:
    /// @brief The window can be accepted only with a name and a text
    void updateAcceptButton();

    QLineEdit* m_name = nullptr;
    QPlainTextEdit* m_text = nullptr;
    QComboBox* m_kind = nullptr;
};

}  // namespace dialogs
}  // namespace gui
}  // namespace kalahari
