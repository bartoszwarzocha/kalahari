/// @file stylesheet.h
/// @brief Qt Style Sheet generator from theme colors
///
/// Simplified architecture (OpenSpec #00028):
/// - QPalette (via Fusion style) handles ALL widget colors automatically
/// - QSS is only needed for tooltips (QPalette alone doesn't fully work)
/// - This minimizes code complexity and avoids breaking native widget rendering

#pragma once

#include "kalahari/core/theme.h"
#include <QString>
#include <QColor>

namespace kalahari {
namespace core {

/// @brief Generates minimal Qt Style Sheets (QSS) from theme colors
///
/// Most styling is handled by QPalette + Fusion style automatically.
/// This class only generates QSS for elements that require it (tooltips).
class StyleSheet {
public:
    /// @brief Generate QSS from theme (only essential styles)
    /// @param theme Theme object containing color definitions
    /// @return QSS string (currently only tooltip styling)
    static QString generate(const Theme& theme);

    // ========================================================================
    // Color utility functions (kept for potential future use)
    // ========================================================================

    /// @brief Make color darker by specified percentage
    static QColor darken(const QColor& color, int percent);

    /// @brief Make color lighter by specified percentage
    static QColor lighten(const QColor& color, int percent);

    /// @brief Create color with specified alpha value
    static QColor withAlpha(const QColor& color, int alpha);

private:
    /// @brief Generate QSS for tooltip styling (the only essential QSS)
    static QString generateTooltipStyle(const Theme& theme);

    /// @brief Get border color from theme
    static QColor getBorderColor(const Theme& theme);
};

} // namespace core
} // namespace kalahari
