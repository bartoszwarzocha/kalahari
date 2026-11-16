// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

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

/// @brief The places of the nodes of a mind map that show: the nodes inside a collapsed node
/// do not
///
/// The layouts on both sides, to the right and down place the tree from the main topic, and
/// the loose nodes at their loose places. The free layout puts each node at its free place;
/// a node without one takes its place on both sides, moved a little, as if put there by
/// hand (MindMap::fixFreePlaces() keeps it).
[[nodiscard]] QHash<QString, MindMapPlace> layOutMindMap(const MindMap& map, MindMapLayout layout,
                                                         const MindMapSizes& sizes);

}  // namespace kalahari::mindmap

#endif  // KALAHARI_MINDMAP_MIND_MAP_LAYOUT_H
