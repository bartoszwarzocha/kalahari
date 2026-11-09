/// @file paste_target.h
/// @brief A view that takes Paste (Ctrl+V) of its own, such as the Navigator, which adds the
/// materials on the clipboard

#pragma once

namespace kalahari {
namespace gui {

/// @brief A list or a tree that takes Paste of its own: EditCoordinator asks it, as it asks a
/// text field, whether Paste is enabled and what it does
class PasteTarget {
public:
    virtual ~PasteTarget() = default;

    /// @brief Whether Paste does something here now
    virtual bool canPasteHere() const = 0;

    /// @brief Paste what the clipboard has
    virtual void pasteHere() = 0;

protected:
    PasteTarget() = default;
    PasteTarget(const PasteTarget&) = default;
    PasteTarget& operator=(const PasteTarget&) = default;
};

} // namespace gui
} // namespace kalahari
