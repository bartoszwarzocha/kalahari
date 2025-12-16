/// @file stylesheet.cpp
/// @brief Implementation of Qt Style Sheet generator from theme colors
///
/// Simplified architecture (OpenSpec #00028):
/// - QPalette (via Fusion style) handles ALL widget colors automatically
/// - QSS needed for tooltips and checkbox indicators

#include "kalahari/core/stylesheet.h"

namespace kalahari {
namespace core {

// ============================================================================
// Public Methods
// ============================================================================

QString StyleSheet::generate(const Theme& theme) {
    QString qss;
    qss += generateTooltipStyle(theme);
    qss += generateCheckboxStyle(theme);
    return qss;
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

QString StyleSheet::generateCheckboxStyle(const Theme& theme) {
    return QString(R"(
QCheckBox::indicator {
    border: 1px solid %1;
    border-radius: 2px;
    width: 14px;
    height: 14px;
}
QCheckBox::indicator:checked {
    background-color: %2;
}
)")
    .arg(theme.palette.midlight.name())
    .arg(theme.palette.highlight.name());
}

} // namespace core
} // namespace kalahari
