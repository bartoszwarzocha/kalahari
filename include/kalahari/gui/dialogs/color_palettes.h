/// @file color_palettes.h
/// @brief The palettes of the color window: ten families of named colors in six shades

#pragma once

#include <QColor>
#include <QList>
#include <QString>

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief A color of a palette with its name, e.g. "Ochre, medium light"
struct NamedColor {
    QColor color;
    QString name;
};

/// @brief A palette: ten families of colors (its columns) in six shades (its rows), from the
/// lightest to the darkest
struct ColorPalette {
    QString id;               ///< Kept in the settings: "basic", "kalahari", ...
    QString name;             ///< Shown in the list of palettes
    QString description;      ///< Shown under the list
    QList<NamedColor> colors; ///< Row by row: the lightest shade of each family first
};

constexpr int PALETTE_FAMILIES = 10;  ///< The columns of a palette
constexpr int PALETTE_SHADES = 6;     ///< The rows of a palette

/// @brief The palettes, named in the language of the program: Basic, Kalahari, Okavango,
/// Pastel, Ink and Paper, Savanna Dusk
QList<ColorPalette> colorPalettes();

/// @brief A color from hue, saturation and lightness in degrees and percent, rounded to whole
/// 0-255 values as HEX and RGB show it, so the same color always compares equal
QColor colorFromHsl(int hue, int saturation, int lightness);

/// @brief The name of @p color in @p palettes, those first that come first; empty when no
/// palette has it
QString paletteColorName(const QList<ColorPalette>& palettes, const QColor& color);

/// @brief The name of the color of @p palettes that looks closest to @p color
QString closestPaletteColorName(const QList<ColorPalette>& palettes, const QColor& color);

} // namespace dialogs
} // namespace gui
} // namespace kalahari
