/// @file view_modes.cpp
/// @brief View mode registry implementation (OpenSpec #00042 Phase 5)

#include <kalahari/editor/view_modes.h>
#include <QObject>

namespace kalahari::editor {

// Static member initialization
std::vector<ViewModeInfo> ViewModeRegistry::s_modes;
bool ViewModeRegistry::s_initialized = false;

// =============================================================================
// ViewModeInfo
// =============================================================================

QIcon ViewModeInfo::icon(int size) const
{
    // TODO: Get icon from ArtProvider when available
    // For now, return empty icon - will be populated when icons are designed
    Q_UNUSED(size);
    return QIcon();
}

// =============================================================================
// ViewModeRegistry
// =============================================================================

void ViewModeRegistry::initializeRegistry()
{
    if (s_initialized) {
        return;
    }

    s_modes = {
        {
            ViewMode::Continuous,
            "continuous",
            QObject::tr("Continuous"),
            QObject::tr("Continuous scrolling without page breaks. Best for drafting."),
            "view.continuous",
            QKeySequence("Ctrl+1"),
            true
        },
        {
            ViewMode::Page,
            "page",
            QObject::tr("Page Layout"),
            QObject::tr("View document as printed pages with margins and page numbers."),
            "view.page",
            QKeySequence("Ctrl+2"),
            true
        },
        {
            ViewMode::Typewriter,
            "typewriter",
            QObject::tr("Typewriter"),
            QObject::tr("Keep current line at fixed position. Classic writing feel."),
            "view.typewriter",
            QKeySequence("Ctrl+3"),
            true
        },
        {
            ViewMode::Focus,
            "focus",
            QObject::tr("Focus"),
            QObject::tr("Dim surrounding text to focus on current paragraph."),
            "view.focus",
            QKeySequence("Ctrl+4"),
            true
        },
        {
            ViewMode::DistractionFree,
            "distraction-free",
            QObject::tr("Distraction-Free"),
            QObject::tr("Fullscreen mode with hidden UI. Pure writing."),
            "view.distraction-free",
            QKeySequence("F11"),
            true
        },
        {
            ViewMode::Outline,
            "outline",
            QObject::tr("Outline"),
            QObject::tr("View document structure with collapsible sections."),
            "view.outline",
            QKeySequence("Ctrl+5"),
            false  // Not yet implemented
        },
        {
            ViewMode::Split,
            "split",
            QObject::tr("Split View"),
            QObject::tr("Two views of the document side by side."),
            "view.split",
            QKeySequence("Ctrl+\\"),
            false  // Not yet implemented
        }
    };

    s_initialized = true;
}

const ViewModeInfo& ViewModeRegistry::info(ViewMode mode)
{
    initializeRegistry();

    int index = static_cast<int>(mode);
    if (index >= 0 && index < static_cast<int>(s_modes.size())) {
        return s_modes[static_cast<size_t>(index)];
    }

    // Return first mode as fallback
    return s_modes[0];
}

const std::vector<ViewModeInfo>& ViewModeRegistry::allModes()
{
    initializeRegistry();
    return s_modes;
}

std::vector<ViewModeInfo> ViewModeRegistry::availableModes()
{
    initializeRegistry();

    std::vector<ViewModeInfo> available;
    for (const auto& mode : s_modes) {
        if (mode.available) {
            available.push_back(mode);
        }
    }
    return available;
}

ViewMode ViewModeRegistry::modeFromId(const QString& id)
{
    initializeRegistry();

    for (const auto& mode : s_modes) {
        if (mode.id == id) {
            return mode.mode;
        }
    }

    return ViewMode::Continuous;
}

QString ViewModeRegistry::idFromMode(ViewMode mode)
{
    return info(mode).id;
}

std::optional<ViewMode> ViewModeRegistry::modeFromShortcut(const QKeySequence& shortcut)
{
    initializeRegistry();

    for (const auto& mode : s_modes) {
        if (mode.shortcut == shortcut && mode.available) {
            return mode.mode;
        }
    }

    return std::nullopt;
}

ViewMode ViewModeRegistry::nextMode(ViewMode current)
{
    auto available = availableModes();
    if (available.empty()) {
        return ViewMode::Continuous;
    }

    // Find current mode in available list
    for (size_t i = 0; i < available.size(); ++i) {
        if (available[i].mode == current) {
            // Return next, wrapping around
            return available[(i + 1) % available.size()].mode;
        }
    }

    return available[0].mode;
}

ViewMode ViewModeRegistry::previousMode(ViewMode current)
{
    auto available = availableModes();
    if (available.empty()) {
        return ViewMode::Continuous;
    }

    // Find current mode in available list
    for (size_t i = 0; i < available.size(); ++i) {
        if (available[i].mode == current) {
            // Return previous, wrapping around
            size_t prev = (i == 0) ? available.size() - 1 : i - 1;
            return available[prev].mode;
        }
    }

    return available[0].mode;
}

}  // namespace kalahari::editor
