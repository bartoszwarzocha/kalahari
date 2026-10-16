/// @file color_palettes.cpp
/// @brief The palettes of the color window

#include "kalahari/gui/dialogs/color_palettes.h"

#include <QCoreApplication>

#include <array>
#include <limits>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr const char* CONTEXT = "ColorPalettes";

/// @brief A family of colors: its name, and its hue and saturation in degrees and percent
struct Family {
    const char* name;
    int hue;
    int saturation;
};

/// @brief A palette as data: its families in its lightness, from light to dark, in percent.
/// Basic has greys from white to black in its first column.
struct PaletteData {
    const char* id;
    const char* name;
    const char* description;
    bool greys;
    std::array<int, PALETTE_SHADES> lightness;
    std::array<Family, PALETTE_FAMILIES> families;
};

/// @brief The lightness of the greys, from white to black
constexpr std::array<int, PALETTE_SHADES> GREY_LIGHTNESS = {100, 80, 60, 40, 20, 0};

// The greys of Basic have names of their own; the first family is only its column
const std::array<const char*, PALETTE_SHADES> GREY_NAMES = {
    QT_TRANSLATE_NOOP("ColorPalettes", "White"),
    QT_TRANSLATE_NOOP("ColorPalettes", "Light grey"),
    QT_TRANSLATE_NOOP("ColorPalettes", "Grey"),
    QT_TRANSLATE_NOOP("ColorPalettes", "Dark grey"),
    QT_TRANSLATE_NOOP("ColorPalettes", "Very dark grey"),
    QT_TRANSLATE_NOOP("ColorPalettes", "Black")};

//: The name of a color of a palette in its shade; %1: the name of its family. In Polish:
//: %1 – bardzo jasny odcień (and so on for the other shades)
const std::array<const char*, PALETTE_SHADES> SHADE_NAMES = {
    QT_TRANSLATE_NOOP("ColorPalettes", "%1, very light"),
    QT_TRANSLATE_NOOP("ColorPalettes", "%1, light"),
    QT_TRANSLATE_NOOP("ColorPalettes", "%1, medium light"),
    QT_TRANSLATE_NOOP("ColorPalettes", "%1, medium dark"),
    QT_TRANSLATE_NOOP("ColorPalettes", "%1, dark"),
    QT_TRANSLATE_NOOP("ColorPalettes", "%1, very dark")};

const std::array<PaletteData, 6> PALETTES = {{
    {"basic", QT_TRANSLATE_NOOP("ColorPalettes", "Basic"),
     QT_TRANSLATE_NOOP("ColorPalettes", "Plain, clear colors and greys"), true,
     {88, 76, 62, 48, 34, 22},
     {{{QT_TRANSLATE_NOOP("ColorPalettes", "Grey"), 0, 0},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Red"), 0, 70},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Orange"), 30, 80},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Yellow"), 50, 80},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Green"), 120, 50},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Turquoise"), 170, 55},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Sky blue"), 200, 65},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Blue"), 220, 65},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Violet"), 270, 50},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Pink"), 320, 55}}}},
    {"kalahari", QT_TRANSLATE_NOOP("ColorPalettes", "Kalahari"),
     QT_TRANSLATE_NOOP("ColorPalettes",
                       "Stone, sand, ochre and terracotta of the desert, its grasses and sky"),
     false,
     {88, 76, 63, 50, 38, 26},
     {{{QT_TRANSLATE_NOOP("ColorPalettes", "Stone"), 30, 10},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Sand"), 42, 50},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Dry grass"), 52, 40},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Ochre"), 36, 72},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Dune"), 24, 70},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Terracotta"), 12, 52},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Pink dusk"), 345, 35},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Acacia"), 75, 25},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Desert sky"), 205, 50},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Desert night"), 225, 35}}}},
    {"okavango", QT_TRANSLATE_NOOP("ColorPalettes", "Okavango"),
     QT_TRANSLATE_NOOP("ColorPalettes", "Greens, azures and blues of the Okavango Delta"), false,
     {88, 74, 60, 47, 35, 23},
     {{{QT_TRANSLATE_NOOP("ColorPalettes", "Marsh"), 90, 12},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Papyrus"), 70, 45},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Reed"), 95, 40},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Leaves"), 115, 45},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Emerald"), 150, 50},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Lagoon"), 175, 55},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Azure"), 192, 70},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Clear sky"), 205, 65},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Deep water"), 218, 60},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Water lily"), 255, 35}}}},
    {"pastel", QT_TRANSLATE_NOOP("ColorPalettes", "Pastel"),
     QT_TRANSLATE_NOOP("ColorPalettes", "Soft, whitened colors"), false,
     {95, 91, 87, 83, 78, 73},
     {{{QT_TRANSLATE_NOOP("ColorPalettes", "Pearl"), 30, 15},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Powder pink"), 340, 60},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Peach"), 20, 75},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Apricot"), 35, 80},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Vanilla"), 50, 75},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Pistachio"), 90, 45},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Mint"), 150, 45},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Pale blue"), 195, 60},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Forget-me-not"), 215, 65},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Lavender"), 260, 45}}}},
    {"ink", QT_TRANSLATE_NOOP("ColorPalettes", "Ink and Paper"),
     QT_TRANSLATE_NOOP("ColorPalettes", "Colors of paper, ink and pencil"), false,
     {94, 82, 67, 52, 37, 22},
     {{{QT_TRANSLATE_NOOP("ColorPalettes", "Paper"), 45, 35},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Parchment"), 38, 50},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Sepia"), 28, 40},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Iron gall ink"), 20, 25},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Graphite"), 210, 6},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Navy ink"), 225, 55},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Blue ink"), 210, 70},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Violet ink"), 275, 40},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Red pencil"), 2, 65},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Green ink"), 160, 45}}}},
    {"savannaDusk", QT_TRANSLATE_NOOP("ColorPalettes", "Savanna Dusk"),
     QT_TRANSLATE_NOOP("ColorPalettes", "Gold, pinks and violets of the setting sun"), false,
     {86, 74, 62, 50, 38, 26},
     {{{QT_TRANSLATE_NOOP("ColorPalettes", "Ash"), 260, 8},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Golden hour"), 42, 85},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Tangerine"), 25, 85},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Coral"), 8, 75},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Raspberry"), 345, 60},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Evening pink"), 325, 45},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Plum"), 300, 30},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Dusk violet"), 275, 40},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Indigo"), 245, 40},
       {QT_TRANSLATE_NOOP("ColorPalettes", "Night sky"), 225, 45}}}},
}};

QString translated(const char* text)
{
    return QCoreApplication::translate(CONTEXT, text);
}

} // namespace

QList<ColorPalette> colorPalettes()
{
    QList<ColorPalette> palettes;
    for (const PaletteData& data : PALETTES) {
        ColorPalette palette{QString::fromLatin1(data.id), translated(data.name),
                             translated(data.description), {}};
        for (int shade = 0; shade < PALETTE_SHADES; ++shade) {
            const auto row = static_cast<size_t>(shade);
            for (size_t column = 0; column < data.families.size(); ++column) {
                const Family& family = data.families.at(column);
                if (data.greys && column == 0) {
                    palette.colors.append({colorFromHsl(0, 0, GREY_LIGHTNESS.at(row)),
                                           translated(GREY_NAMES.at(row))});
                } else {
                    palette.colors.append(
                        {colorFromHsl(family.hue, family.saturation, data.lightness.at(row)),
                         translated(SHADE_NAMES.at(row)).arg(translated(family.name))});
                }
            }
        }
        palettes.append(palette);
    }
    return palettes;
}

QColor colorFromHsl(int hue, int saturation, int lightness)
{
    constexpr float DEGREES = 360.0F;
    constexpr float PERCENT = 100.0F;
    return QColor::fromRgb(QColor::fromHslF(static_cast<float>(hue) / DEGREES,
                                            static_cast<float>(saturation) / PERCENT,
                                            static_cast<float>(lightness) / PERCENT)
                               .rgb());
}

QString paletteColorName(const QList<ColorPalette>& palettes, const QColor& color)
{
    for (const ColorPalette& palette : palettes) {
        for (const NamedColor& named : palette.colors) {
            if (named.color.rgb() == color.rgb()) {
                return named.name;
            }
        }
    }
    return {};
}

QString closestPaletteColorName(const QList<ColorPalette>& palettes, const QColor& color)
{
    // The eye tells green apart best and blue least: a distance that weighs them so
    constexpr int RED_WEIGHT = 2;
    constexpr int GREEN_WEIGHT = 4;
    constexpr int BLUE_WEIGHT = 3;
    QString closest;
    long long shortest = std::numeric_limits<long long>::max();
    for (const ColorPalette& palette : palettes) {
        for (const NamedColor& named : palette.colors) {
            const long long red = named.color.red() - color.red();
            const long long green = named.color.green() - color.green();
            const long long blue = named.color.blue() - color.blue();
            const long long distance = RED_WEIGHT * red * red + GREEN_WEIGHT * green * green +
                                       BLUE_WEIGHT * blue * blue;
            if (distance < shortest) {
                shortest = distance;
                closest = named.name;
            }
        }
    }
    return closest;
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
