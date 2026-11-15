// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file edit_target.h
/// @brief A view that does commands of the Edit menu itself, such as the plot board, which
/// undoes its own changes and marks a card with Cut

#ifndef KALAHARI_GUI_EDIT_TARGET_H
#define KALAHARI_GUI_EDIT_TARGET_H

#include <QMetaObject>

#include <functional>

class QObject;

namespace kalahari::gui {

/// @brief The commands of the Edit menu, in its order
enum class EditCommand { Undo, Redo, Cut, Copy, Paste, Delete, SelectAll };

/// @brief A view with commands of the Edit menu of its own: EditCoordinator asks it, as it
/// asks a text field, whether a command is enabled and what it does
///
/// It does them with the keys anywhere in it but in a field being written in (that field's
/// own Undo, Cut, Paste...); the commands it does not take work there as on any element. It
/// says itself what a command did.
class EditTarget {
public:
    virtual ~EditTarget() = default;

    /// @brief Whether it does @p command itself
    [[nodiscard]] virtual bool takesEdit(EditCommand command) const = 0;

    /// @brief Whether @p command, one it takes, does something here now
    [[nodiscard]] virtual bool canEditHere(EditCommand command) const = 0;

    /// @brief Do @p command, one that canEditHere() allows
    virtual void editHere(EditCommand command) = 0;

    /// @brief Call @p changed whenever what canEditHere() says may have changed, while
    /// @p receiver is there
    /// @return The connection, to be broken when the view is no more followed
    virtual QMetaObject::Connection followEditStates(QObject* receiver,
                                                     std::function<void()> changed) = 0;

protected:
    EditTarget() = default;
    EditTarget(const EditTarget&) = default;
    EditTarget& operator=(const EditTarget&) = default;
    EditTarget(EditTarget&&) = default;
    EditTarget& operator=(EditTarget&&) = default;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_EDIT_TARGET_H
