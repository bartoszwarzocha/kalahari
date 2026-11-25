// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file rename_element_dialog.h
/// @brief Dialog for a new name of an element or a section of the book in the Navigator

#ifndef KALAHARI_GUI_DIALOGS_RENAME_ELEMENT_DIALOG_H
#define KALAHARI_GUI_DIALOGS_RENAME_ELEMENT_DIALOG_H

#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QString>

class QLineEdit;

namespace kalahari::gui::dialogs {

/// @brief Dialog for a new name of an element of the book (a chapter, a part, an item) or of
/// a section of the book
///
/// Shows the current name and starts with it in the field, selected, so typing replaces
/// it. The dialog cannot be accepted with an empty name.
class RenameElementDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief Create the dialog for an element
    /// @param currentName The element's current name
    /// @param iconId ArtProvider id of the element's icon (as in the Navigator)
    /// @param parent Parent widget
    explicit RenameElementDialog(const QString& currentName, const QString& iconId,
                                 QWidget* parent = nullptr);

    /// @brief The new name, without spaces around it
    [[nodiscard]] QString name() const;

private:
    /// @brief The dialog can be accepted only with a name
    void updateAcceptButton();

    QLineEdit* m_nameEdit = nullptr;
};

} // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_RENAME_ELEMENT_DIALOG_H
