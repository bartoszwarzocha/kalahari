/// @file annotation_colors.h
/// @brief The colors of the annotations' cards, readable on the panel and on the paper

#pragma once

#include <QColor>

namespace kalahari::gui {

/// @brief The contrast every text of a card has with the card at least (WCAG, normal text)
constexpr double MIN_TEXT_CONTRAST = 4.5;

/// @brief The colors of a card of one kind of annotation: in the Annotations panel, or the
/// frame its text is written in
///
/// Every color of text has at least MIN_TEXT_CONTRAST with the card's background.
struct AnnotationCardColors {
    QColor kind;        ///< The kind's own color: the bar on the left
    QColor background;  ///< The card: what it lies on, tinted with the kind's color
    QColor kindName;    ///< The kind's name: its color, darker or lighter to be read
    QColor text;        ///< The annotation's text
    QColor secondary;   ///< Less important text: the chapter, the date, a hint, a done one

    bool operator==(const AnnotationCardColors& other) const = default;
};

/// @brief The contrast ratio of two colors, from 1 (none) to 21 (black on white)
double contrastRatio(const QColor& a, const QColor& b);

/// @brief A color between two others
/// @param amount How much of @p over there is (0 to 1)
QColor mixedColor(const QColor& under, const QColor& over, double amount);

/// @brief The color nearest to @p color with at least @p ratio contrast with @p background
///
/// @p color itself when it has it; else it is moved toward black on a light background and
/// toward white on a dark one.
QColor readableColor(const QColor& color, const QColor& background, double ratio);

/// @brief The colors of the cards of a kind
/// @param kindColor The kind's color (from the theme)
/// @param surface What the cards lie on: the panel's base, or the paper
/// @param text The color of the text on that surface
AnnotationCardColors annotationCardColors(const QColor& kindColor, const QColor& surface,
                                          const QColor& text);

}  // namespace kalahari::gui
