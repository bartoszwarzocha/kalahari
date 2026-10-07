/// @file view_modes.h
/// @brief View modes of BookEditor

#pragma once

namespace kalahari::editor {

// =============================================================================
// View Mode Enum
// =============================================================================

/// @brief Available view modes for the text editor
///
/// Each mode provides a different writing experience optimized for
/// specific use cases (drafting, reviewing, focused writing, etc.)
///
/// Every view shows the text as wide as the page, with the page's margins and the line
/// breaks of the printed page, and zooms it as a whole; the views other than Page show it
/// as one endless page. Focus, typewriter scrolling and Distraction-Free are toggles on top
/// of the view mode (BookEditor::setFocusModeEnabled(), BookEditor::setTypewriterEnabled(),
/// BookEditor::setDistractionFree()).
enum class ViewMode {
    /// @brief Continuous scrolling mode (default)
    ///
    /// One endless page: uninterrupted vertical scrolling, no page breaks.
    /// Best for: First drafts, quick editing, short documents.
    Continuous,

    /// @brief Page layout mode
    ///
    /// Shows document as pages with margins, page breaks, and numbers.
    /// WYSIWYG preview of printed output.
    /// Best for: Final formatting, print preview, book layout.
    Page,

    /// @brief Outline mode (future)
    ///
    /// Shows document structure with collapsible sections.
    /// Best for: Navigation, restructuring, overview.
    Outline,

    /// @brief Split view mode (future)
    ///
    /// Two views of the same or different documents.
    /// Best for: Reference, comparison, notes.
    Split,

    /// @brief Count of view modes (for iteration)
    _Count
};

}  // namespace kalahari::editor
