/// @file rename_element_dialog.h
/// @brief Dialog for a new name of an element of the book in the Navigator

#pragma once

#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QString>

class QLineEdit;

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief Dialog for a new name of an element of the book (a chapter, a part, an item)
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
    QString name() const;

private:
    /// @brief The dialog can be accepted only with a name
    void updateAcceptButton();

    QLineEdit* m_nameEdit = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
