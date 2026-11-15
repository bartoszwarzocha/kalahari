// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file copy_target.h
/// @brief A view that takes Copy (Ctrl+C) and Select All (Ctrl+A) of its own, such as the pages
/// of a PDF file, whose text the writer selects

#ifndef KALAHARI_GUI_COPY_TARGET_H
#define KALAHARI_GUI_COPY_TARGET_H

#include <QMetaObject>

#include <functional>

class QObject;

namespace kalahari::gui {

/// @brief A view that is no text field and has text to copy: EditCoordinator asks it, as it asks
/// a text field, whether Copy and Select All are enabled and what they do
class CopyTarget {
public:
    virtual ~CopyTarget() = default;

    /// @brief Whether Copy copies something here now: a selection
    [[nodiscard]] virtual bool canCopyHere() const = 0;

    /// @brief Copy the selection
    virtual void copyHere() = 0;

    /// @brief Whether Select All selects something here now
    [[nodiscard]] virtual bool canSelectAllHere() const = 0;

    /// @brief Select all it can: the text of the page shown
    virtual void selectAllHere() = 0;

    /// @brief Call @p update, in the thread of @p context, each time the selection changes
    virtual QMetaObject::Connection whenSelectionChanges(QObject* context,
                                                         std::function<void()> update) = 0;

protected:
    CopyTarget() = default;
    CopyTarget(const CopyTarget&) = default;
    CopyTarget& operator=(const CopyTarget&) = default;
    CopyTarget(CopyTarget&&) = default;
    CopyTarget& operator=(CopyTarget&&) = default;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_COPY_TARGET_H
