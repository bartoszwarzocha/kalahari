/// @file stylesheet.cpp
/// @brief Implementation of Qt Style Sheet generator from theme colors
///
/// Simplified architecture (OpenSpec #00028):
/// - QPalette (via Fusion style) handles ALL widget colors automatically
/// - QSS is only needed for tooltips (QPalette alone doesn't fully work)
/// - Removed 300+ lines of unnecessary widget styling code

#include "kalahari/core/stylesheet.h"

namespace kalahari {
namespace core {

// ============================================================================
// Public Methods
// ============================================================================

QString StyleSheet::generate(const Theme& theme) {
    // Only tooltip styling is necessary - everything else is handled by QPalette + Fusion
    return generateTooltipStyle(theme);
}

// ============================================================================
// Color Utility Functions
// ============================================================================

QColor StyleSheet::darken(const QColor& color, int percent) {
    return color.darker(100 + percent);
}

QColor StyleSheet::lighten(const QColor& color, int percent) {
    return color.lighter(100 + percent);
}

QColor StyleSheet::withAlpha(const QColor& color, int alpha) {
    QColor result = color;
    result.setAlpha(alpha);
    return result;
}

// ============================================================================
// Private Methods
// ============================================================================

QColor StyleSheet::getBorderColor(const Theme& theme) {
    return theme.palette.mid;
}

QString StyleSheet::generateTooltipStyle(const Theme& theme) {
    // Tooltips require QSS because QPalette::ToolTipBase/ToolTipText
    // don't fully apply without explicit stylesheet
    return QString(R"(
QToolTip {
    background-color: %1;
    color: %2;
    border: 1px solid %3;
    padding: 0px 2px;
    margin: 0px;
    border-radius: 0px;
}
)")
    .arg(theme.palette.toolTipBase.name())
    .arg(theme.palette.toolTipText.name())
    .arg(getBorderColor(theme).name());
}

} // namespace core
} // namespace kalahari
