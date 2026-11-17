// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_look.h
/// @brief How mind maps look and behave: the settings of Settings > Mind Maps and the colors
/// of the theme

#ifndef KALAHARI_MINDMAP_MIND_MAP_LOOK_H
#define KALAHARI_MINDMAP_MIND_MAP_LOOK_H

#include <kalahari/mindmap/mind_map.h>

#include <QColor>
#include <QString>

#include <array>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

namespace kalahari::mindmap {

/// @brief The shape of the nodes
enum class NodeShape {
    Rounded,    ///< Rounded rectangles
    Rectangle,  ///< Rectangles
    Oval,       ///< Ovals
    Underline   ///< The text over a line in the color of its branch
};

/// @brief The lines between a node and its nodes
enum class LineStyle {
    Curved,    ///< Curves
    Straight,  ///< Straight lines
    Elbow      ///< Lines with right angles
};

/// @brief The colors of the branches
enum class BranchColors {
    EachBranch,  ///< Each main branch its own color of the palette
    One,         ///< The first color of the palette for all
    None         ///< A plain color for all
};

/// @brief The background of the maps
enum class MapBackground {
    Theme,  ///< The color of the map's background
    Dots,   ///< With dots
    Grid,   ///< With a grid
    Own     ///< The writer's own color
};

/// @brief The view a map opens on
enum class OpenView {
    Map,  ///< The map
    List, ///< The list
    Last  ///< The view it was left on
};

/// @brief The settings of the mind maps, under "mindMaps."
struct MindMapSettings {
    static constexpr const char* NEW_LAYOUT = "mindMaps.newLayout";   ///< MindMap::nameOf()
    static constexpr const char* OPEN_ON = "mindMaps.openOn";         ///< "map", "list", "last"
    static constexpr const char* SHAPE = "mindMaps.nodes";            ///< "rounded"...
    static constexpr const char* LINES = "mindMaps.lines";            ///< "curved"...
    static constexpr const char* THINNER = "mindMaps.thinnerBranches";  ///< bool
    static constexpr const char* COLORS = "mindMaps.branchColors";    ///< "each", "one", "none"
    static constexpr const char* BACKGROUND = "mindMaps.background";  ///< "theme"...
    static constexpr const char* EDITOR_FONT = "mindMaps.editorFont";  ///< bool
    static constexpr const char* TEXT_SIZE = "mindMaps.textSize";     ///< "small"...
    static constexpr const char* PASTE_TREE = "mindMaps.pasteTree";   ///< bool

    /// @brief The settings and their default values, for core::settings_schema::addDefaults()
    [[nodiscard]] static std::map<std::string, nlohmann::json> defaults();
};

/// @brief The keys of the colors of the theme ("editor" section of the theme files)
struct MindMapColorKeys {
    /// The palette of the branches, shared with the timelines: six colors
    static constexpr std::array<const char*, 6> PALETTE{
        "workshopPalette1", "workshopPalette2", "workshopPalette3",
        "workshopPalette4", "workshopPalette5", "workshopPalette6"};
    static constexpr const char* BACKGROUND = "mindMapBackground";
    static constexpr const char* DOTS = "mindMapDots";
    static constexpr const char* OWN_BACKGROUND = "mindMapOwnBackground";
    static constexpr const char* ROOT = "mindMapRoot";
    static constexpr const char* ROOT_TEXT = "mindMapRootText";
    static constexpr const char* NODE = "mindMapNode";
    static constexpr const char* CONNECTION = "mindMapConnection";
    static constexpr const char* PLAIN = "mindMapPlain";
    static constexpr const char* DROP = "mindMapDrop";
    static constexpr const char* MARKER_TEXT = "mindMapMarkerText";
    /// The markers, in the order of markerIds()
    static constexpr std::array<const char*, 5> MARKERS{
        "mindMapQuestion", "mindMapImportant", "mindMapIdea", "mindMapDone", "mindMapCheck"};
};

/// @brief The markers of the nodes, in the order of the menus: QUESTION, IMPORTANT, IDEA, DONE,
/// CHECK
[[nodiscard]] const std::array<const char*, 5>& markerIds();

/// @brief The sign drawn in the circle of marker @p marker: "?", "!", "★", "✓", "⌕"
[[nodiscard]] QString markerSign(const QString& marker);

/// @brief How the maps look now: the settings and the colors of the theme in use
struct MindMapLook {
    NodeShape shape = NodeShape::Rounded;
    LineStyle lines = LineStyle::Curved;
    bool thinner = true;  ///< The lines are thinner deeper in the branches
    BranchColors colors = BranchColors::EachBranch;
    MapBackground background = MapBackground::Dots;
    bool editorFont = false;  ///< The font of the editor, else the font of the program
    double textScale = 1.0;   ///< 0.92 small, 1 medium, 1.1 large

    QColor mapBackground;   ///< The background
    QColor dots;            ///< Its dots and grid
    QColor ownBackground;   ///< The writer's own background
    QColor root;            ///< The main topic
    QColor rootText;        ///< Its text
    QColor node;            ///< The other nodes
    QColor text;            ///< Their text
    QColor mutedText;       ///< The frame of a loose node, the hints
    QColor mid;             ///< Mixed into the frames of the deeper nodes
    QColor connection;      ///< The connections and their labels
    QColor plain;           ///< The lines without colors
    QColor selection;       ///< The frame of the selected node
    QColor drop;            ///< Where a dragged node goes
    QColor markerText;      ///< The signs in the circles of the markers
    std::array<QColor, 6> palette;
    std::array<QColor, 5> markers;

    /// @brief The look of the settings and of the theme in use
    [[nodiscard]] static MindMapLook current();

    /// @brief The color of palette key @p key ("1"..."6"); invalid for another key
    [[nodiscard]] QColor paletteColor(const QString& key) const;

    /// @brief The color of marker @p marker
    [[nodiscard]] QColor markerColor(const QString& marker) const;

    /// @brief The layout of a new map, as the settings say
    [[nodiscard]] static MindMapLayout newLayout();

    /// @brief The view a map opens on, as the settings say
    [[nodiscard]] static OpenView openView();
};

}  // namespace kalahari::mindmap

#endif  // KALAHARI_MINDMAP_MIND_MAP_LOOK_H
