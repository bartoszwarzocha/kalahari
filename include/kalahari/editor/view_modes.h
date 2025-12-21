/// @file view_modes.h
/// @brief View mode definitions and registry for BookEditor (OpenSpec #00042 Phase 5)
///
/// This header defines the available view modes and provides a registry system
/// that supports:
/// - Mode metadata (icon, name, description, shortcut)
/// - Easy iteration for UI (future mode carousel/wheel)
/// - Extensibility for new modes
///
/// Future: Mode wheel/carousel will use ViewModeRegistry to display
/// large icons in the editor area for quick mode switching.

#pragma once

#include <QString>
#include <QKeySequence>
#include <QIcon>
#include <vector>
#include <functional>

namespace kalahari::editor {

// =============================================================================
// View Mode Enum
// =============================================================================

/// @brief Available view modes for the text editor
///
/// Each mode provides a different writing experience optimized for
/// specific use cases (drafting, reviewing, focused writing, etc.)
enum class ViewMode {
    /// @brief Continuous scrolling mode (default)
    ///
    /// Simple, uninterrupted vertical scrolling. No page breaks.
    /// Best for: First drafts, quick editing, short documents.
    Continuous,

    /// @brief Page layout mode
    ///
    /// Shows document as pages with margins, page breaks, and numbers.
    /// WYSIWYG preview of printed output.
    /// Best for: Final formatting, print preview, book layout.
    Page,

    /// @brief Typewriter mode
    ///
    /// Keeps the current line at a fixed vertical position (typically
    /// 40% from top). Text scrolls up as you type.
    /// Best for: Long writing sessions, maintaining rhythm.
    Typewriter,

    /// @brief Focus mode
    ///
    /// Dims content outside the current paragraph/sentence.
    /// Reduces visual distractions.
    /// Best for: Deep focus, editing specific passages.
    Focus,

    /// @brief Distraction-free mode
    ///
    /// Fullscreen with hidden UI. Only the text visible.
    /// Optional: word count, clock, fade-in UI on mouse movement.
    /// Best for: Immersive writing, flow state.
    DistractionFree,

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

// =============================================================================
// View Mode Info
// =============================================================================

/// @brief Metadata for a view mode
///
/// Contains all information needed to display the mode in UI:
/// - Icon (for toolbar, menu, mode wheel)
/// - Name and description (for tooltips, settings)
/// - Keyboard shortcut
/// - Availability flag (some modes may require features not yet implemented)
struct ViewModeInfo {
    ViewMode mode;                             ///< The mode enum value
    QString id;                                ///< Unique string identifier
    QString name;                              ///< Display name (translated)
    QString description;                       ///< Longer description (translated)
    QString iconName;                          ///< Icon name (for ArtProvider)
    QKeySequence shortcut;                     ///< Default keyboard shortcut
    bool available{true};                      ///< Is this mode currently available?

    /// @brief Get icon for this mode
    /// @param size Icon size (for mode wheel, use large sizes like 64x64)
    QIcon icon(int size = 24) const;

    /// @brief Get large icon for mode wheel/carousel
    QIcon largeIcon() const { return icon(64); }
};

// =============================================================================
// View Mode Registry
// =============================================================================

/// @brief Registry of all available view modes
///
/// Provides access to view mode metadata for UI rendering.
/// Future: Mode wheel/carousel will use this to display available modes.
///
/// Usage:
/// @code
/// // Get all available modes for mode wheel
/// for (const auto& info : ViewModeRegistry::availableModes()) {
///     addModeButton(info.largeIcon(), info.name, info.shortcut);
/// }
///
/// // Switch mode by ID
/// ViewMode mode = ViewModeRegistry::modeFromId("focus");
/// @endcode
class ViewModeRegistry {
public:
    /// @brief Get info for a specific mode
    static const ViewModeInfo& info(ViewMode mode);

    /// @brief Get all registered modes
    static const std::vector<ViewModeInfo>& allModes();

    /// @brief Get only currently available modes
    static std::vector<ViewModeInfo> availableModes();

    /// @brief Get mode from string identifier
    /// @return Mode enum, or Continuous if not found
    static ViewMode modeFromId(const QString& id);

    /// @brief Get string identifier for mode
    static QString idFromMode(ViewMode mode);

    /// @brief Get mode from shortcut
    /// @return Mode if shortcut matches, nullopt otherwise
    static std::optional<ViewMode> modeFromShortcut(const QKeySequence& shortcut);

    /// @brief Get the next mode in sequence (for cycling)
    static ViewMode nextMode(ViewMode current);

    /// @brief Get the previous mode in sequence (for cycling)
    static ViewMode previousMode(ViewMode current);

private:
    static void initializeRegistry();
    static std::vector<ViewModeInfo> s_modes;
    static bool s_initialized;
};

// =============================================================================
// View Mode Changed Event
// =============================================================================

/// @brief Information about a view mode change
struct ViewModeChangedEvent {
    ViewMode previousMode;
    ViewMode newMode;
    bool animated{true};                       ///< Should transition be animated?
};

}  // namespace kalahari::editor
