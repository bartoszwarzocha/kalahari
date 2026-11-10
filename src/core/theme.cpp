/// @file theme.cpp
/// @brief Implementation of Theme data structure

#include "kalahari/core/theme.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace kalahari {
namespace core {

namespace {

/// @brief Steps of the search for a readable color (each halves the distance)
constexpr int READABLE_STEPS = 12;

/// @brief The palette colors that are made from the theme's color when the theme file's
/// palette does not list them (Theme::followThemeColor())
constexpr std::array<const char*, 4> THEME_COLOR_PALETTE = {"highlight", "highlightedText",
                                                            "link", "linkVisited"};

/// @brief The contrast the links have with the base at least (WCAG, normal text)
constexpr double MIN_LINK_CONTRAST = 4.5;

/// @brief How far a visited link is from the link's color toward the text
constexpr double VISITED_LINK_SHARE = 0.35;

/// @brief The relative luminance of a color (WCAG), from 0 (black) to 1 (white)
double luminance(const QColor& color) {
    const auto linear = [](double channel) {
        return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) +
           0.0722 * linear(color.blueF());
}

}  // namespace

// ============================================================================
// Colors readable on others
// ============================================================================

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
    const bool towardBlack =
        contrastRatio(Qt::black, background) >= contrastRatio(Qt::white, background);
    const QColor target = towardBlack ? QColor(Qt::black) : QColor(Qt::white);
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

// ============================================================================
// Helper Functions
// ============================================================================

/// @brief Parse QColor from hex string
static QColor parseColor(const std::string& hexColor) {
    QColor color(QString::fromStdString(hexColor));
    if (!color.isValid()) {
        throw std::runtime_error("Invalid color format: " + hexColor);
    }
    return color;
}

/// @brief Convert QColor to hex string
static std::string colorToHex(const QColor& color) {
    return color.name(QColor::HexRgb).toStdString();
}

/// @brief Check if background color is dark
static bool isDarkBackground(const QColor& bg) {
    // Using relative luminance formula
    int luminance = (bg.red() * 299 + bg.green() * 587 + bg.blue() * 114) / 1000;
    return luminance < 128;
}

// ============================================================================
// Theme::Palette Implementation
// ============================================================================

QPalette Theme::Palette::toQPalette() const {
    QPalette pal;

    // Set colors for all three color groups (Active, Inactive, Disabled)
    auto setColorForAllGroups = [&pal](QPalette::ColorRole role, const QColor& color) {
        pal.setColor(QPalette::Active, role, color);
        pal.setColor(QPalette::Inactive, role, color);
        // Disabled state: half transparent over the background
        QColor disabledColor = color;
        disabledColor.setAlpha(128);
        pal.setColor(QPalette::Disabled, role, disabledColor);
    };

    // A disabled background is opaque, halfway between its color and the window. Half
    // transparent, it would show what is under it, and a list paints its background over
    // nothing: a disabled list came out dark grey.
    auto setBackgroundForAllGroups = [&pal, this](QPalette::ColorRole role,
                                                  const QColor& color) {
        pal.setColor(QPalette::Active, role, color);
        pal.setColor(QPalette::Inactive, role, color);
        pal.setColor(QPalette::Disabled, role,
                     QColor((color.red() + window.red()) / 2,
                            (color.green() + window.green()) / 2,
                            (color.blue() + window.blue()) / 2));
    };

    setBackgroundForAllGroups(QPalette::Window, window);
    setColorForAllGroups(QPalette::WindowText, windowText);
    setBackgroundForAllGroups(QPalette::Base, base);
    setBackgroundForAllGroups(QPalette::AlternateBase, alternateBase);
    setColorForAllGroups(QPalette::Text, text);
    setBackgroundForAllGroups(QPalette::Button, button);
    setColorForAllGroups(QPalette::ButtonText, buttonText);
    setBackgroundForAllGroups(QPalette::Highlight, highlight);
    setColorForAllGroups(QPalette::HighlightedText, highlightedText);
    setColorForAllGroups(QPalette::Light, light);
    setColorForAllGroups(QPalette::Midlight, midlight);
    setColorForAllGroups(QPalette::Mid, mid);
    setColorForAllGroups(QPalette::Dark, dark);
    setColorForAllGroups(QPalette::Shadow, shadow);
    setColorForAllGroups(QPalette::Link, link);
    setColorForAllGroups(QPalette::LinkVisited, linkVisited);

    // Additional QPalette roles (OpenSpec #00028)
    setBackgroundForAllGroups(QPalette::ToolTipBase, toolTipBase);
    setColorForAllGroups(QPalette::ToolTipText, toolTipText);
    setColorForAllGroups(QPalette::PlaceholderText, placeholderText);
    setColorForAllGroups(QPalette::BrightText, brightText);

    return pal;
}

// ============================================================================
// Theme::fromJson
// ============================================================================

Theme Theme::fromJson(const nlohmann::json& json) {
    Theme theme;

    // Parse metadata
    theme.name = json.value("name", "Unnamed");
    theme.version = json.value("version", "1.0");
    theme.author = json.value("author", "Unknown");
    theme.description = json.value("description", "");

    // Parse main colors
    if (json.contains("colors") && json["colors"].is_object()) {
        const auto& colors = json["colors"];
        theme.colors.primary = parseColor(colors.value("primary", "#000000"));
        theme.colors.secondary = parseColor(colors.value("secondary", "#666666"));
        theme.colors.accent = parseColor(colors.value("accent", "#0078D4"));
        theme.colors.background = parseColor(colors.value("background", "#FFFFFF"));
        theme.colors.text = parseColor(colors.value("text", "#000000"));
        theme.colors.infoHeader = parseColor(colors.value("infoHeader", "#4A7A9E"));
        theme.colors.infoBarBorder = parseColor(colors.value("infoBarBorder", "#785E1E"));
        theme.colors.infoBarBackground = parseColor(colors.value("infoBarBackground", "#3E321E"));
    } else {
        throw std::runtime_error("Theme JSON missing 'colors' object");
    }

    // Parse Qt Palette colors (optional - auto-generate from main colors if missing)
    if (json.contains("palette") && json["palette"].is_object()) {
        const auto& pal = json["palette"];
        theme.palette.window = parseColor(pal.value("window", "#ffffff"));
        theme.palette.windowText = parseColor(pal.value("windowText", "#000000"));
        theme.palette.base = parseColor(pal.value("base", "#ffffff"));
        theme.palette.alternateBase = parseColor(pal.value("alternateBase", "#f5f5f5"));
        theme.palette.text = parseColor(pal.value("text", "#000000"));
        theme.palette.button = parseColor(pal.value("button", "#e0e0e0"));
        theme.palette.buttonText = parseColor(pal.value("buttonText", "#000000"));
        theme.palette.highlight = parseColor(pal.value("highlight", "#0078d4"));
        theme.palette.highlightedText = parseColor(pal.value("highlightedText", "#ffffff"));
        theme.palette.light = parseColor(pal.value("light", "#ffffff"));
        theme.palette.midlight = parseColor(pal.value("midlight", "#e0e0e0"));
        theme.palette.mid = parseColor(pal.value("mid", "#a0a0a0"));
        theme.palette.dark = parseColor(pal.value("dark", "#606060"));
        theme.palette.shadow = parseColor(pal.value("shadow", "#000000"));
        theme.palette.link = parseColor(pal.value("link", "#0078d4"));
        theme.palette.linkVisited = parseColor(pal.value("linkVisited", "#551a8b"));

        // Additional QPalette roles with backward-compatible defaults (OpenSpec #00028)
        bool isDark = isDarkBackground(theme.palette.window);
        theme.palette.toolTipBase = parseColor(pal.value("toolTipBase",
            isDark ? "#3c3c3c" : "#ffffdc"));
        theme.palette.toolTipText = parseColor(pal.value("toolTipText",
            isDark ? "#e0e0e0" : "#000000"));
        theme.palette.placeholderText = parseColor(pal.value("placeholderText",
            isDark ? "#808080" : "#a0a0a0"));
        theme.palette.brightText = parseColor(pal.value("brightText", "#ffffff"));

        // The selection and the links the file does not list are in the theme's color
        for (const char* key : THEME_COLOR_PALETTE) {
            if (!pal.contains(key)) {
                theme.themeColorPalette.insert(key);
            }
        }
        theme.followThemeColor();
    } else {
        // Auto-generate palette from main colors (backward compatibility)
        bool isDark = isDarkBackground(theme.colors.background);

        theme.palette.window = theme.colors.background;
        theme.palette.windowText = theme.colors.text;
        theme.palette.base = isDark
            ? theme.colors.background.lighter(120)
            : theme.colors.background;
        theme.palette.alternateBase = isDark
            ? theme.colors.background.lighter(110)
            : theme.colors.background.darker(105);
        theme.palette.text = theme.colors.text;
        theme.palette.button = isDark
            ? theme.colors.background.lighter(140)
            : theme.colors.background.darker(110);
        theme.palette.buttonText = theme.colors.text;
        theme.palette.highlight = theme.colors.accent;
        theme.palette.highlightedText = isDark ? QColor("#000000") : QColor("#ffffff");
        theme.palette.light = isDark
            ? theme.colors.background.lighter(180)
            : theme.colors.background.lighter(120);
        theme.palette.midlight = isDark
            ? theme.colors.background.lighter(150)
            : theme.colors.background.darker(105);
        theme.palette.mid = isDark
            ? theme.colors.background.lighter(130)
            : theme.colors.background.darker(130);
        theme.palette.dark = isDark
            ? theme.colors.background.darker(120)
            : theme.colors.background.darker(160);
        theme.palette.shadow = QColor("#000000");
        theme.palette.link = theme.colors.accent;
        theme.palette.linkVisited = theme.colors.accent.darker(120);

        // Additional QPalette roles (OpenSpec #00028)
        theme.palette.toolTipBase = isDark
            ? theme.colors.background.lighter(120)
            : QColor("#ffffdc");
        theme.palette.toolTipText = theme.colors.text;
        theme.palette.placeholderText = isDark
            ? QColor("#808080")
            : QColor("#a0a0a0");
        theme.palette.brightText = QColor("#ffffff");

        // A theme without a palette lists none of the selection and the links: they are in
        // the theme's color too
        theme.themeColorPalette.insert(THEME_COLOR_PALETTE.cbegin(), THEME_COLOR_PALETTE.cend());
        theme.followThemeColor();
    }

    // Parse log colors (optional, use defaults if missing)
    if (json.contains("log") && json["log"].is_object()) {
        const auto& log = json["log"];
        bool isDark = isDarkBackground(theme.colors.background);

        // Colors for all log levels
        theme.log.trace = parseColor(log.value("trace", isDark ? "#FF66FF" : "#CC00CC"));     // Magenta
        theme.log.debug = parseColor(log.value("debug", isDark ? "#FF66FF" : "#CC00CC"));     // Magenta
        theme.log.info = parseColor(log.value("info", isDark ? "#FFFFFF" : "#000000"));       // Default text
        theme.log.warning = parseColor(log.value("warning", isDark ? "#FFA500" : "#FF8C00")); // Orange
        theme.log.error = parseColor(log.value("error", isDark ? "#FF4444" : "#CC0000"));     // Red
        theme.log.critical = parseColor(log.value("critical", isDark ? "#FF4444" : "#CC0000")); // Red (same as error)
        theme.log.background = parseColor(log.value("background", isDark ? "#252525" : "#F5F5F5"));
    } else {
        // Default log colors based on theme type
        bool isDark = isDarkBackground(theme.colors.background);
        theme.log.trace = isDark ? QColor("#FF66FF") : QColor("#CC00CC");     // Magenta
        theme.log.debug = isDark ? QColor("#FF66FF") : QColor("#CC00CC");     // Magenta
        theme.log.info = isDark ? QColor("#FFFFFF") : QColor("#000000");       // Default text
        theme.log.warning = isDark ? QColor("#FFA500") : QColor("#FF8C00");   // Orange
        theme.log.error = isDark ? QColor("#FF4444") : QColor("#CC0000");     // Red
        theme.log.critical = isDark ? QColor("#FF4444") : QColor("#CC0000");  // Red (same as error)
        theme.log.background = isDark ? QColor("#252525") : QColor("#f5f5f5");
    }

    // Editor colors (open-ended list)
    if (json.contains("editor") && json["editor"].is_object()) {
        for (const auto& entry : json["editor"].items()) {
            if (entry.value().is_string()) {
                theme.editor[entry.key()] = parseColor(entry.value().get<std::string>());
            }
        }
    }

    return theme;
}

// ============================================================================
// Theme::followThemeColor
// ============================================================================

void Theme::followThemeColor(const std::set<std::string>& kept) {
    const QColor& themeColor = colors.primary;
    if (!themeColor.isValid()) {
        return;
    }
    const auto follows = [this, &kept](const char* key) {
        return themeColorPalette.count(key) != 0 && kept.count(key) == 0;
    };
    if (follows("highlight")) {
        palette.highlight = themeColor;
    }
    if (follows("highlightedText")) {
        const bool black = contrastRatio(Qt::black, palette.highlight) >=
                           contrastRatio(Qt::white, palette.highlight);
        palette.highlightedText = black ? QColor(Qt::black) : QColor(Qt::white);
    }
    if (follows("link")) {
        palette.link = readableColor(themeColor, palette.base, MIN_LINK_CONTRAST);
    }
    if (follows("linkVisited")) {
        palette.linkVisited = readableColor(
            mixedColor(palette.link, palette.text, VISITED_LINK_SHARE), palette.base,
            MIN_LINK_CONTRAST);
    }
}

// ============================================================================
// Theme::toJson
// ============================================================================

nlohmann::json Theme::toJson() const {
    nlohmann::json json;

    // Metadata
    json["name"] = name;
    json["version"] = version;
    json["author"] = author;
    json["description"] = description;

    // Main colors
    json["colors"] = {
        {"primary", colorToHex(colors.primary)},
        {"secondary", colorToHex(colors.secondary)},
        {"accent", colorToHex(colors.accent)},
        {"background", colorToHex(colors.background)},
        {"text", colorToHex(colors.text)},
        {"infoHeader", colorToHex(colors.infoHeader)},
        {"infoBarBorder", colorToHex(colors.infoBarBorder)},
        {"infoBarBackground", colorToHex(colors.infoBarBackground)}
    };

    // Qt Palette colors
    json["palette"] = {
        {"window", colorToHex(palette.window)},
        {"windowText", colorToHex(palette.windowText)},
        {"base", colorToHex(palette.base)},
        {"alternateBase", colorToHex(palette.alternateBase)},
        {"text", colorToHex(palette.text)},
        {"button", colorToHex(palette.button)},
        {"buttonText", colorToHex(palette.buttonText)},
        {"highlight", colorToHex(palette.highlight)},
        {"highlightedText", colorToHex(palette.highlightedText)},
        {"light", colorToHex(palette.light)},
        {"midlight", colorToHex(palette.midlight)},
        {"mid", colorToHex(palette.mid)},
        {"dark", colorToHex(palette.dark)},
        {"shadow", colorToHex(palette.shadow)},
        {"link", colorToHex(palette.link)},
        {"linkVisited", colorToHex(palette.linkVisited)},
        // Additional QPalette roles (OpenSpec #00028)
        {"toolTipBase", colorToHex(palette.toolTipBase)},
        {"toolTipText", colorToHex(palette.toolTipText)},
        {"placeholderText", colorToHex(palette.placeholderText)},
        {"brightText", colorToHex(palette.brightText)}
    };
    // What follows the theme's color stays so: it is made again when the theme is read
    for (const std::string& key : themeColorPalette) {
        json["palette"].erase(key);
    }

    // Log colors
    json["log"] = {
        {"trace", colorToHex(log.trace)},
        {"debug", colorToHex(log.debug)},
        {"info", colorToHex(log.info)},
        {"warning", colorToHex(log.warning)},
        {"error", colorToHex(log.error)},
        {"critical", colorToHex(log.critical)},
        {"background", colorToHex(log.background)}
    };

    if (!editor.empty()) {
        json["editor"] = nlohmann::json::object();
        for (const auto& [key, color] : editor) {
            json["editor"][key] = colorToHex(color);
        }
    }

    return json;
}

} // namespace core
} // namespace kalahari
