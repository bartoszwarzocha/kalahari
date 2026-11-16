// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_look.cpp
/// @brief How mind maps look and behave: the settings and the colors of the theme

#include <kalahari/mindmap/mind_map_look.h>

#include <kalahari/core/settings_manager.h>
#include <kalahari/core/theme_manager.h>

#include <algorithm>

namespace kalahari::mindmap {

namespace {

constexpr double SMALL_TEXT = 0.92;
constexpr double LARGE_TEXT = 1.1;

std::string setting(const char* key, const char* fallback) {
    return core::SettingsManager::getInstance().get<std::string>(key, fallback);
}

QColor themeColor(const char* key, const QColor& fallback) {
    return core::ThemeManager::getInstance().editorColor(key, fallback);
}

}  // namespace

std::map<std::string, nlohmann::json> MindMapSettings::defaults() {
    return {
        {NEW_LAYOUT, "bothSides"}, {OPEN_ON, "last"},       {SHAPE, "rounded"},
        {LINES, "curved"},         {THINNER, true},          {COLORS, "each"},
        {BACKGROUND, "dots"},      {EDITOR_FONT, false},     {TEXT_SIZE, "medium"},
        {PASTE_TREE, true},
    };
}

const std::array<const char*, 5>& markerIds() {
    static const std::array<const char*, 5> ids{MindMapNode::QUESTION, MindMapNode::IMPORTANT,
                                                MindMapNode::IDEA, MindMapNode::DONE,
                                                MindMapNode::CHECK};
    return ids;
}

QString markerSign(const QString& marker) {
    if (marker == QLatin1String(MindMapNode::QUESTION)) {
        return QStringLiteral("?");
    }
    if (marker == QLatin1String(MindMapNode::IMPORTANT)) {
        return QStringLiteral("!");
    }
    if (marker == QLatin1String(MindMapNode::IDEA)) {
        return QString(QChar(0x2605));  // ★
    }
    if (marker == QLatin1String(MindMapNode::DONE)) {
        return QString(QChar(0x2713));  // ✓
    }
    return QString(QChar(0x2315));  // ⌕
}

MindMapLook MindMapLook::current() {
    MindMapLook look;
    const std::string shape = setting(MindMapSettings::SHAPE, "rounded");
    look.shape = shape == "rectangle" ? NodeShape::Rectangle
                 : shape == "oval"    ? NodeShape::Oval
                 : shape == "underline" ? NodeShape::Underline
                                        : NodeShape::Rounded;
    const std::string lines = setting(MindMapSettings::LINES, "curved");
    look.lines = lines == "straight" ? LineStyle::Straight
                 : lines == "elbow"  ? LineStyle::Elbow
                                     : LineStyle::Curved;
    look.thinner = core::SettingsManager::getInstance().get<bool>(MindMapSettings::THINNER, true);
    const std::string colors = setting(MindMapSettings::COLORS, "each");
    look.colors = colors == "one"    ? BranchColors::One
                  : colors == "none" ? BranchColors::None
                                     : BranchColors::EachBranch;
    const std::string background = setting(MindMapSettings::BACKGROUND, "dots");
    look.background = background == "theme"  ? MapBackground::Theme
                      : background == "grid" ? MapBackground::Grid
                      : background == "own"  ? MapBackground::Own
                                             : MapBackground::Dots;
    look.editorFont =
        core::SettingsManager::getInstance().get<bool>(MindMapSettings::EDITOR_FONT, false);
    const std::string size = setting(MindMapSettings::TEXT_SIZE, "medium");
    look.textScale = size == "small" ? SMALL_TEXT : size == "large" ? LARGE_TEXT : 1.0;

    const core::Theme& theme = core::ThemeManager::getInstance().getCurrentTheme();
    look.text = theme.palette.text;
    look.mutedText = theme.palette.placeholderText.isValid() ? theme.palette.placeholderText
                                                             : theme.palette.mid;
    look.mid = theme.palette.mid;
    look.selection = theme.palette.highlight;
    look.mapBackground = themeColor(MindMapColorKeys::BACKGROUND, theme.palette.base);
    look.dots = themeColor(MindMapColorKeys::DOTS, theme.palette.midlight);
    look.ownBackground = themeColor(MindMapColorKeys::OWN_BACKGROUND, theme.palette.alternateBase);
    look.root = themeColor(MindMapColorKeys::ROOT, theme.palette.text);
    look.rootText = themeColor(MindMapColorKeys::ROOT_TEXT, theme.palette.base);
    look.node = themeColor(MindMapColorKeys::NODE, theme.palette.base);
    look.connection = themeColor(MindMapColorKeys::CONNECTION, theme.palette.text);
    look.plain = themeColor(MindMapColorKeys::PLAIN, theme.palette.mid);
    look.drop = themeColor(MindMapColorKeys::DROP, theme.palette.highlight);
    look.markerText = themeColor(MindMapColorKeys::MARKER_TEXT, theme.palette.highlightedText);
    for (std::size_t i = 0; i < look.palette.size(); ++i) {
        look.palette.at(i) = themeColor(MindMapColorKeys::PALETTE.at(i), theme.palette.highlight);
    }
    for (std::size_t i = 0; i < look.markers.size(); ++i) {
        look.markers.at(i) = themeColor(MindMapColorKeys::MARKERS.at(i), theme.palette.highlight);
    }
    return look;
}

QColor MindMapLook::paletteColor(const QString& key) const {
    bool number = false;
    const int index = key.toInt(&number) - 1;
    return number && index >= 0 && index < static_cast<int>(palette.size())
               ? palette.at(static_cast<std::size_t>(index))
               : QColor();
}

QColor MindMapLook::markerColor(const QString& marker) const {
    const std::array<const char*, 5>& ids = markerIds();
    const auto found = std::find_if(ids.cbegin(), ids.cend(),
                                    [&marker](const char* id) { return marker == QLatin1String(id); });
    return found != ids.cend() ? markers.at(static_cast<std::size_t>(found - ids.cbegin()))
                               : mutedText;
}

MindMapLayout MindMapLook::newLayout() {
    return MindMap::layoutOf(QString::fromStdString(setting(MindMapSettings::NEW_LAYOUT,
                                                            "bothSides")))
        .value_or(MindMapLayout::BothSides);
}

OpenView MindMapLook::openView() {
    const std::string view = setting(MindMapSettings::OPEN_ON, "last");
    return view == "map" ? OpenView::Map : view == "list" ? OpenView::List : OpenView::Last;
}

}  // namespace kalahari::mindmap
