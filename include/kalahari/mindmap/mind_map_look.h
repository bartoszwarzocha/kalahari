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
#include <cstddef>
#include <map>
#include <nlohmann/json.hpp>
#include <string>

namespace kalahari::mindmap {

/// @brief The shape of the nodes
enum class NodeShape {
    Rounded,   ///< Rounded corners
    Rectangle  ///< Square corners
};

/// @brief The lines between the nodes: the branches and the other links
enum class LineStyle {
    Curved,    ///< Curves
    Straight,  ///< Straight lines
    Elbow      ///< Lines with right angles
};

/// @brief The colors of the branches
enum class BranchColors {
    EachBranch,  ///< Each main branch its own color of the palette
    One,         ///< The first color of the palette for all
    None         ///< The color of the sockets of the branches for all
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
    Map,   ///< The map
    List,  ///< The list
    Last   ///< The view it was left on
};

/// @brief The settings of the mind maps, under "mindMaps."
struct MindMapSettings {
    static constexpr const char* NEW_LAYOUT = "mindMaps.newLayout";    ///< MindMap::nameOf()
    static constexpr const char* OPEN_ON = "mindMaps.openOn";          ///< "map", "list", "last"
    static constexpr const char* SHAPE = "mindMaps.nodes";             ///< "rounded"...
    static constexpr const char* LINES = "mindMaps.lines";             ///< "curved"...
    static constexpr const char* COLORS = "mindMaps.branchColors";     ///< "each", "one", "none"
    static constexpr const char* BACKGROUND = "mindMaps.background";   ///< "theme"...
    static constexpr const char* EDITOR_FONT = "mindMaps.editorFont";  ///< bool
    static constexpr const char* TEXT_SIZE = "mindMaps.textSize";      ///< "small"...
    static constexpr const char* PASTE_TREE = "mindMaps.pasteTree";    ///< bool
    /// The nodes of the cards show the main portraits of their cards (bool)
    static constexpr const char* PORTRAITS = "mindMaps.portraits";
    /// A new node shows only its header (bool)
    static constexpr const char* NEW_FOLDED = "mindMaps.newNodesFolded";
    /// The first relation of a card offers a map of relations (bool)
    static constexpr const char* ASK_RELATION_MAP = "mindMaps.askRelationMap";

    /// @brief The settings and their default values, for core::settings_schema::addDefaults()
    [[nodiscard]] static std::map<std::string, nlohmann::json> defaults();
};

/// @brief The kinds of the sockets of the nodes, which links of the same kind join
enum class SocketKind {
    Branch,     ///< A branch: the output of a node, the input of its node
    Family,     ///< The family of a card
    Relations,  ///< The other relations between cards of a kind
    Places,     ///< The relations with places
    Items       ///< The relations with items
};

/// @brief The colors of the headers of the nodes, by what they are
enum class NodeColor {
    Topic,      ///< The main topic
    Idea,       ///< An idea
    Question,   ///< A question
    Note,       ///< A note
    Character,  ///< A card of a character
    Place,      ///< A card of a place
    Item,       ///< A card of an item
    Text,       ///< A text of the book: a chapter
    Scene,      ///< A scene of a text
    Thread,     ///< A thread of a timeline
    Element     ///< Another element of the book: another card, a material
};

/// @brief The keys of the colors of the theme ("editor" section of the theme files)
struct MindMapColorKeys {
    /// The palette of the branches, shared with the timelines: six colors
    static constexpr std::array<const char*, 6> PALETTE{"workshopPalette1", "workshopPalette2",
                                                        "workshopPalette3", "workshopPalette4",
                                                        "workshopPalette5", "workshopPalette6"};
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
    static constexpr std::array<const char*, 5> MARKERS{"mindMapQuestion", "mindMapImportant",
                                                        "mindMapIdea", "mindMapDone",
                                                        "mindMapCheck"};
    static constexpr const char* GRID = "mindMapGrid";               ///< The fine lines
    static constexpr const char* GRID_MAJOR = "mindMapGridMajor";    ///< Every fifth line
    static constexpr const char* NODE_LINE = "mindMapNodeLine";      ///< The edge of a node
    static constexpr const char* HEADER_TEXT = "mindMapHeaderText";  ///< Its header's text
    static constexpr const char* FRAME = "mindMapFrame";             ///< A frame
    static constexpr const char* FRAME_LINE = "mindMapFrameLine";    ///< Its edge
    /// The headers, in the order of NodeColor
    static constexpr std::array<const char*, 11> KINDS{
        "mindMapKindTopic",     "mindMapKindIdea",   "mindMapKindQuestion", "mindMapKindNote",
        "mindMapKindCharacter", "mindMapKindPlace",  "mindMapKindItem",     "mindMapKindText",
        "mindMapKindScene",     "mindMapKindThread", "mindMapKindElement"};
    /// The sockets and their links, in the order of SocketKind
    static constexpr std::array<const char*, 5> SOCKETS{
        "mindMapSocketBranch", "mindMapSocketFamily", "mindMapSocketRelations",
        "mindMapSocketPlaces", "mindMapSocketItems"};
};

/// @brief The icons of the mind maps (core::IconRegistry), registered by
/// registerMindMapModule()
struct MindMapIcons {
    // The kinds of the nodes, in their headers
    static constexpr const char* KIND_TOPIC = "mindmap.kind.topic";
    static constexpr const char* KIND_IDEA = "mindmap.kind.idea";
    static constexpr const char* KIND_QUESTION = "mindmap.kind.question";
    static constexpr const char* KIND_NOTE = "mindmap.kind.note";
    static constexpr const char* KIND_SCENE = "mindmap.kind.scene";
    static constexpr const char* KIND_THREAD = "mindmap.kind.thread";
    static constexpr const char* KIND_ELEMENT = "mindmap.kind.element";
    // The commands of the bar and of the menus
    static constexpr const char* ADD = "mindmap.add";
    static constexpr const char* BRANCH = "mindmap.branch";
    static constexpr const char* BELOW = "mindmap.below";
    static constexpr const char* RENAME = "mindmap.rename";
    static constexpr const char* FOLD = "mindmap.fold";
    static constexpr const char* UNFOLD = "mindmap.unfold";
    static constexpr const char* FRAME = "mindmap.frame";
    static constexpr const char* ARRANGE = "mindmap.arrange";
    static constexpr const char* REMOVE = "mindmap.delete";
    static constexpr const char* CONNECT = "mindmap.connect";
    static constexpr const char* PROPERTIES = "mindmap.properties";
    static constexpr const char* OPEN = "mindmap.open";
    static constexpr const char* COPY = "mindmap.copy";
    static constexpr const char* PASTE = "mindmap.paste";
    static constexpr const char* REVERSE = "mindmap.reverse";
    static constexpr const char* LABEL = "mindmap.label";
    static constexpr const char* ZOOM_IN = "mindmap.zoomIn";
    static constexpr const char* ZOOM_OUT = "mindmap.zoomOut";
    static constexpr const char* FIT = "mindmap.fit";
    static constexpr const char* RELATION_MAP = "mindmap.relationMap";
    static constexpr const char* UNDO = "edit.undo";
    static constexpr const char* REDO = "edit.redo";
    static constexpr const char* SETTINGS = "dock.settings";
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
    BranchColors colors = BranchColors::None;
    MapBackground background = MapBackground::Grid;
    bool editorFont = false;  ///< The font of the editor, else the font of the program
    double textScale = 1.0;   ///< 0.92 small, 1 medium, 1.1 large
    bool portraits = true;    ///< The nodes of the cards show their main portraits
    bool newFolded = false;   ///< A new node shows only its header

    QColor mapBackground;  ///< The background
    QColor dots;           ///< Its dots and grid
    QColor ownBackground;  ///< The writer's own background
    QColor root;           ///< The main topic
    QColor rootText;       ///< Its text
    QColor node;           ///< The other nodes
    QColor text;           ///< Their text
    QColor mutedText;      ///< The frame of a loose node, the hints
    QColor mid;            ///< Mixed into the frames of the deeper nodes
    QColor connection;     ///< The connections and their labels
    QColor plain;          ///< The lines without colors
    QColor selection;      ///< The frame of the selected node
    QColor drop;           ///< Where a dragged node goes
    QColor markerText;     ///< The signs in the circles of the markers
    QColor grid;           ///< The fine lines of the grid
    QColor gridMajor;      ///< Its fifth lines
    QColor nodeLine;       ///< The edge of a node
    QColor headerText;     ///< The text of the headers
    QColor frame;          ///< A frame
    QColor frameLine;      ///< Its edge and its title
    std::array<QColor, 6> palette;
    std::array<QColor, 5> markers;
    std::array<QColor, 11> kinds;   ///< By NodeColor
    std::array<QColor, 5> sockets;  ///< By SocketKind

    /// @brief The color of the header of a node of @p color
    [[nodiscard]] QColor kindColor(NodeColor color) const {
        return kinds.at(static_cast<std::size_t>(color));
    }

    /// @brief The color of the sockets and the links of @p kind
    [[nodiscard]] QColor socketColor(SocketKind kind) const {
        return sockets.at(static_cast<std::size_t>(kind));
    }

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
