/// @file annotation_colors.cpp
/// @brief The colors of the annotations' cards, readable on the panel and on the paper

#include "kalahari/gui/panels/annotation_colors.h"

#include <algorithm>

namespace kalahari::gui {

namespace {

/// @brief How much of the kind's color a card has on a light and on a dark surface
constexpr double LIGHT_TINT = 0.16;
constexpr double DARK_TINT = 0.24;

/// @brief The most of the kind's color a card has, whatever the tint asked for
constexpr double MAX_TINT = 0.6;

/// @brief How far the less important text is from the text, toward the card
constexpr double SECONDARY_SHARE = 0.38;

}  // namespace

QColor shownColor(const QColor& color, const QColor& background, bool optimize) {
    if (!optimize || !color.isValid()) {
        return color;
    }
    return readableColor(color, background, MIN_MARK_CONTRAST);
}

AnnotationCardColors annotationCardColors(const QColor& kindColor, const QColor& surface,
                                          const QColor& text, double tint) {
    AnnotationCardColors colors;
    colors.kind = kindColor;
    const double share = (surface.lightness() < 128 ? DARK_TINT : LIGHT_TINT) * std::max(0.0, tint);
    colors.background = mixedColor(surface, kindColor, std::min(share, MAX_TINT));
    colors.kindName = readableColor(kindColor, colors.background, MIN_TEXT_CONTRAST);
    colors.text = readableColor(text, colors.background, MIN_TEXT_CONTRAST);
    colors.secondary = readableColor(mixedColor(colors.text, colors.background, SECONDARY_SHARE),
                                     colors.background, MIN_TEXT_CONTRAST);
    return colors;
}

}  // namespace kalahari::gui
