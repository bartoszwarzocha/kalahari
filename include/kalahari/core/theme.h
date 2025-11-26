/// @file theme.h
/// @brief Theme data structure for Kalahari appearance system

#pragma once

#include <string>
#include <QColor>
#include <QPalette>
#include <nlohmann/json.hpp>

namespace kalahari {
namespace core {

/// @brief Theme configuration data structure
///
/// Contains all color definitions for Kalahari's appearance system.
/// The `palette` section maps directly to QPalette color roles for
/// native Qt widget styling via QApplication::setPalette().
struct Theme {
    /// @brief Theme metadata
    std::string name;        ///< Theme name (e.g., "Light", "Dark")
    std::string version;     ///< Theme version (e.g., "1.0")
    std::string author;      ///< Theme author
    std::string description; ///< Theme description

    /// @brief Main application colors (for icons, custom drawing)
    struct Colors {
        QColor primary;     ///< Primary color (main UI elements, icons)
        QColor secondary;   ///< Secondary color (accents, borders)
        QColor accent;      ///< Accent color (buttons, links)
        QColor background;  ///< Background color (windows, panels)
        QColor text;        ///< Text color (labels, content)
    } colors;

    /// @brief Qt Palette colors (for native widget styling)
    ///
    /// These colors are applied via QApplication::setPalette()
    /// and Qt Fusion style uses them for all native widgets.
    struct Palette {
        QColor window;          ///< General background color
        QColor windowText;      ///< General foreground color
        QColor base;            ///< Background for text entry widgets
        QColor alternateBase;   ///< Alternate background for views
        QColor text;            ///< Foreground for text entry widgets
        QColor button;          ///< Button background color
        QColor buttonText;      ///< Button foreground color
        QColor highlight;       ///< Selection/focus highlight color
        QColor highlightedText; ///< Text color when highlighted
        QColor light;           ///< Lighter than button color
        QColor midlight;        ///< Between button and light
        QColor mid;             ///< Between button and dark
        QColor dark;            ///< Darker than button color
        QColor shadow;          ///< Very dark, for shadows
        QColor link;            ///< Hyperlink color
        QColor linkVisited;     ///< Visited hyperlink color

        /// @brief Convert to QPalette object
        /// @return QPalette with all color roles set
        QPalette toQPalette() const;
    } palette;

    /// @brief Log panel specific colors
    struct LogColors {
        QColor info;       ///< INFO message color
        QColor debug;      ///< DEBUG message color
        QColor background; ///< Log panel background color
    } log;

    /// @brief Load Theme from JSON object
    /// @param json JSON object from theme file
    /// @return Parsed Theme struct
    /// @throws std::runtime_error if JSON is malformed
    static Theme fromJson(const nlohmann::json& json);

    /// @brief Convert Theme to JSON object
    /// @return JSON representation of theme
    nlohmann::json toJson() const;
};

} // namespace core
} // namespace kalahari
