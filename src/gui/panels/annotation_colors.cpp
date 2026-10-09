/// @file annotation_colors.cpp
/// @brief The colors of the annotations' cards, readable on the panel and on the paper

#include "kalahari/gui/panels/annotation_colors.h"

#include <algorithm>
#include <cmath>

namespace kalahari::gui {

namespace {

/// @brief How much of the kind's color a card has on a light and on a dark surface
constexpr double LIGHT_TINT = 0.16;
constexpr double DARK_TINT = 0.24;

/// @brief How far the less important text is from the text, toward the card
constexpr double SECONDARY_SHARE = 0.38;

/// @brief Steps of the search for a readable color (each halves the distance)
constexpr int READABLE_STEPS = 12;

/// @brief The relative luminance of a color (WCAG), from 0 (black) to 1 (white)
double luminance(const QColor& color) {
    const auto linear = [](double channel) {
        return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) +
           0.0722 * linear(color.blueF());
}

}  // namespace

double contrastRatio(const QColor& a, const QColor& b) {
    const double la = luminance(a);
    const double lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

QColor mixedColor(const QColor& under, const QColor& over, double amount) {
    const auto channel = [amount](int a, int b) {
        return static_cast<int>(std::lround(a + (b - a) * amount));
    };
    return QColor(channel(under.red(), over.red()), channel(under.green(), over.green()),
                  channel(under.blue(), over.blue()));
}

QColor readableColor(const QColor& color, const QColor& background, double ratio) {
    if (contrastRatio(color, background) >= ratio) {
        return color;
    }

    // Toward the end of the scale far from the background, as little as will do
    const QColor target = contrastRatio(Qt::black, background) >= contrastRatio(Qt::white, background)
                              ? QColor(Qt::black)
                              : QColor(Qt::white);
    double low = 0.0;
    double high = 1.0;
    for (int i = 0; i < READABLE_STEPS; ++i) {
        const double middle = (low + high) / 2.0;
        if (contrastRatio(mixedColor(color, target, middle), background) >= ratio) {
            high = middle;
        } else {
            low = middle;
        }
    }
    return mixedColor(color, target, high);
}

AnnotationCardColors annotationCardColors(const QColor& kindColor, const QColor& surface,
                                          const QColor& text) {
    AnnotationCardColors colors;
    colors.kind = kindColor;
    colors.background =
        mixedColor(surface, kindColor, surface.lightness() < 128 ? DARK_TINT : LIGHT_TINT);
    colors.kindName = readableColor(kindColor, colors.background, MIN_TEXT_CONTRAST);
    colors.text = readableColor(text, colors.background, MIN_TEXT_CONTRAST);
    colors.secondary = readableColor(mixedColor(colors.text, colors.background, SECONDARY_SHARE),
                                     colors.background, MIN_TEXT_CONTRAST);
    return colors;
}

}  // namespace kalahari::gui
