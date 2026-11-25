// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file mind_map_layout.h
/// @brief Where the nodes of a mind map stand in each of its layouts

#ifndef KALAHARI_MINDMAP_MIND_MAP_LAYOUT_H
#define KALAHARI_MINDMAP_MIND_MAP_LAYOUT_H

#include <kalahari/mindmap/mind_map.h>

#include <QHash>
#include <QPointF>
#include <QSizeF>
#include <QString>

#include <functional>

namespace kalahari::mindmap {

/// @brief Where a node of a mind map stands
struct MindMapPlace {
    QPointF centre;      ///< Its centre, from the main topic at 0,0
    QSizeF size;         ///< Its size
    int side = 0;        ///< 1 right of the main topic, -1 left, 0 the main topic or a tree
                         ///< going down
    int depth = 0;       ///< 0 the main topic, 1 a main branch or a loose node
    bool loose = false;  ///< In a loose branch

    bool operator==(const MindMapPlace&) const = default;
};

/// @brief The size a node takes, drawn at depth @p depth (0 the main topic)
using MindMapSizes = std::function<QSizeF(const MindMapNode& node, int depth)>;

/// @brief How a layout places the nodes
struct MindMapLayoutOptions {
    bool everyNode = false;        ///< The nodes inside collapsed nodes too (the map of
                                   ///< nodes, where the list's collapsing hides none)
    double gaps = 1.0;             ///< The gaps between the nodes, times this
    QPointF placeScale{1.0, 1.0};  ///< The places of format 1 (free, loose), times this
    bool stackLoose = false;       ///< The loose branches under the tree, one under another,
                                   ///< not at their loose places
};

/// @brief The places of the nodes of a mind map that show: the nodes inside a collapsed node
/// do not, unless @p options say every node
///
/// The layouts on both sides, to the right and down place the tree from the main topic, and
/// the loose nodes at their loose places. The free layout puts each node at its free place;
/// a node without one takes its place on both sides, moved a little, as if put there by
/// hand (MindMap::fixFreePlaces() keeps it).
[[nodiscard]] QHash<QString, MindMapPlace> layOutMindMap(
    const MindMap& map, MindMapLayout layout, const MindMapSizes& sizes,
    const MindMapLayoutOptions& options = MindMapLayoutOptions());

}  // namespace kalahari::mindmap

#endif  // KALAHARI_MINDMAP_MIND_MAP_LAYOUT_H
