// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file mind_map_layout.cpp
/// @brief The layouts of a mind map: on both sides, to the right, down, and free

#include <kalahari/mindmap/mind_map_layout.h>

#include <QStringList>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace kalahari::mindmap {

namespace {

/// Gap between a node and the nodes inside it, across the branches
constexpr double ACROSS = 34.0;
/// Gap between nodes beside each other, along the branches
constexpr double ALONG = 9.0;
/// The main branches stand further apart
constexpr double MAIN_ALONG = ALONG * 1.6;
/// Extra room between the main topic and its branches
constexpr double ROOT_ROOM = 12.0;
/// Down: gap between nodes beside each other
constexpr double DOWN_ALONG = 14.0;
/// Down: gap between a node and the nodes inside it
constexpr double DOWN_ACROSS = 38.0;
/// A loose node without a place stands this far above the tree
constexpr double LOOSE_ABOVE = 70.0;
/// Loose branches stacked under the tree stand this far apart
constexpr double LOOSE_BELOW = 40.0;

/// The free layout spreads the nodes of the layout on both sides a little more
constexpr double FREE_SPREAD_X = 1.12;
constexpr double FREE_SPREAD_Y = 1.18;

/// A small number from the id of a node, the same each time
int hashOf(const QString& id) {
    constexpr int START = 7;
    constexpr int FACTOR = 31;
    constexpr int MODULUS = 9973;
    int hash = START;
    for (const QChar character : id) {
        hash = (hash * FACTOR + character.unicode()) % MODULUS;
    }
    return hash;
}

class Layouter {
public:
    Layouter(const MindMap& map, const MindMapSizes& sizes, const MindMapLayoutOptions& options)
        : m_map(map),
          m_sizes(sizes),
          m_options(options),
          m_across(ACROSS * options.gaps),
          m_along(ALONG * options.gaps),
          m_mainAlong(MAIN_ALONG * options.gaps),
          m_downAlong(DOWN_ALONG * options.gaps),
          m_downAcross(DOWN_ACROSS * options.gaps) {}

    QHash<QString, MindMapPlace> layOut(MindMapLayout layout) {
        const QStringList mainBranches = shown(m_map.root());
        if (layout == MindMapLayout::Down) {
            down(m_map.root(), 0.0, 0.0, 0);
        } else {
            m_places.insert(m_map.root(), MindMapPlace{QPointF(), size(m_map.root()), 0, 0});
            if (layout == MindMapLayout::Right) {
                group(mainBranches, 1);
            } else {
                const qsizetype half = (mainBranches.size() + 1) / 2;
                group(mainBranches.mid(0, half), 1);
                group(mainBranches.mid(half), -1);
            }
        }
        placeLoose(layout == MindMapLayout::Down);
        return m_places;
    }

private:
    /// The nodes inside node @p id that show
    QStringList shown(const QString& id) const {
        const MindMapNode* node = m_map.node(id);
        return node != nullptr && (!node->collapsed || m_options.everyNode) ? node->children
                                                                            : QStringList();
    }

    QSizeF size(const QString& id) {
        if (!m_sizeCache.contains(id)) {
            const MindMapNode* node = m_map.node(id);
            m_sizeCache.insert(id, node != nullptr ? m_sizes(*node, m_map.depthOf(id)) : QSizeF());
        }
        return m_sizeCache.value(id);
    }

    /// The height a branch takes, across: its node or its nodes with their gaps
    double branchHeight(const QString& id) {
        const QStringList children = shown(id);
        if (children.isEmpty()) {
            return size(id).height();
        }
        double total = m_along * static_cast<double>(children.size() - 1);
        for (const QString& child : children) {
            total += branchHeight(child);
        }
        return std::max(size(id).height(), total);
    }

    /// Down: the width a branch takes
    double branchWidth(const QString& id) {
        const QStringList children = shown(id);
        if (children.isEmpty()) {
            return size(id).width();
        }
        double total = m_downAlong * static_cast<double>(children.size() - 1);
        for (const QString& child : children) {
            total += branchWidth(child);
        }
        return std::max(size(id).width(), total);
    }

    /// A node at @p centre and its nodes on side @p side
    void side(const QString& id, const QPointF& centre, int side, int depth) {
        const QSizeF own = size(id);
        m_places.insert(id, MindMapPlace{centre, own, side, depth});
        const QStringList children = shown(id);
        if (children.isEmpty()) {
            return;
        }
        double total = m_along * static_cast<double>(children.size() - 1);
        for (const QString& child : children) {
            total += branchHeight(child);
        }
        double y = centre.y() - total / 2.0;
        for (const QString& child : children) {
            const double height = branchHeight(child);
            const double x = centre.x() +
                             side * (own.width() / 2.0 + m_across + size(child).width() / 2.0);
            this->side(child, QPointF(x, y + height / 2.0), side, depth + 1);
            y += height + m_along;
        }
    }

    /// The main branches @p branches on side @p side of the main topic
    void group(const QStringList& branches, int side) {
        double total = m_mainAlong *
                       static_cast<double>(std::max<qsizetype>(0, branches.size() - 1));
        for (const QString& branch : branches) {
            total += branchHeight(branch);
        }
        const double rootHalf = size(m_map.root()).width() / 2.0;
        double y = -total / 2.0;
        for (const QString& branch : branches) {
            const double height = branchHeight(branch);
            const double x = side * (rootHalf + m_across + ROOT_ROOM * m_options.gaps +
                                     size(branch).width() / 2.0);
            this->side(branch, QPointF(x, y + height / 2.0), side, 1);
            y += height + m_mainAlong;
        }
    }

    /// Down: a node with its top at @p top and its nodes below it
    void down(const QString& id, double centreX, double top, int depth) {
        const QSizeF own = size(id);
        m_places.insert(id, MindMapPlace{QPointF(centreX, top + own.height() / 2.0), own, 0, depth});
        const QStringList children = shown(id);
        if (children.isEmpty()) {
            return;
        }
        double total = m_downAlong * static_cast<double>(children.size() - 1);
        for (const QString& child : children) {
            total += branchWidth(child);
        }
        double x = centreX - total / 2.0;
        const double nextTop = top + own.height() + m_downAcross;
        for (const QString& child : children) {
            const double width = branchWidth(child);
            down(child, x + width / 2.0, nextTop, depth + 1);
            x += width + m_downAlong;
        }
    }

    /// The loose nodes at their places; without one, above the tree on the right; stacked
    /// under the tree when the options say so
    void placeLoose(bool downwards) {
        double top = std::numeric_limits<double>::max();
        double right = std::numeric_limits<double>::lowest();
        double left = std::numeric_limits<double>::max();
        double bottom = std::numeric_limits<double>::lowest();
        for (const MindMapPlace& place : std::as_const(m_places)) {
            top = std::min(top, place.centre.y() - place.size.height() / 2.0);
            right = std::max(right, place.centre.x() + place.size.width() / 2.0);
            left = std::min(left, place.centre.x() - place.size.width() / 2.0);
            bottom = std::max(bottom, place.centre.y() + place.size.height() / 2.0);
        }
        for (const QString& loose : m_map.looseNodes()) {
            const QStringList branch = QStringList{loose} + m_map.descendantsOf(loose);
            if (m_options.stackLoose) {
                const double y = bottom + LOOSE_BELOW * m_options.gaps;
                if (downwards) {
                    down(loose, left + branchWidth(loose) / 2.0, y, 1);
                } else {
                    side(loose,
                         QPointF(left + size(loose).width() / 2.0, y + branchHeight(loose) / 2.0),
                         1, 1);
                }
                for (const QString& id : branch) {
                    if (m_places.contains(id)) {
                        const MindMapPlace& place = m_places.value(id);
                        bottom = std::max(bottom, place.centre.y() + place.size.height() / 2.0);
                    }
                }
            } else {
                const std::optional<QPointF> own = m_map.node(loose)->loosePlace;
                const QPointF at = own ? QPointF(own->x() * m_options.placeScale.x(),
                                                 own->y() * m_options.placeScale.y())
                                       : QPointF(right - size(loose).width() / 2.0,
                                                 top - LOOSE_ABOVE);
                if (downwards) {
                    down(loose, at.x(), at.y() - size(loose).height() / 2.0, 1);
                } else {
                    side(loose, at, 1, 1);
                }
            }
            for (const QString& id : branch) {
                if (m_places.contains(id)) {
                    m_places[id].loose = true;
                }
            }
        }
    }

    const MindMap& m_map;
    const MindMapSizes& m_sizes;
    const MindMapLayoutOptions& m_options;
    double m_across;
    double m_along;
    double m_mainAlong;
    double m_downAlong;
    double m_downAcross;
    QHash<QString, QSizeF> m_sizeCache;
    QHash<QString, MindMapPlace> m_places;
};

/// The free layout: the writer's places, else the places on both sides moved a little
QHash<QString, MindMapPlace> freeLayout(const MindMap& map, const MindMapSizes& sizes,
                                        const MindMapLayoutOptions& options) {
    QHash<QString, MindMapPlace> places =
        Layouter(map, sizes, options).layOut(MindMapLayout::BothSides);
    constexpr int JITTER_X = 9;
    constexpr int JITTER_Y = 7;
    constexpr int HALF_JITTER_X = JITTER_X / 2;
    constexpr int HALF_JITTER_Y = JITTER_Y / 2;
    for (auto it = places.begin(); it != places.end(); ++it) {
        const MindMapNode* node = map.node(it.key());
        if (node->freePlace) {
            it->centre = QPointF(node->freePlace->x() * options.placeScale.x(),
                                 node->freePlace->y() * options.placeScale.y());
        } else if (it.key() == map.root()) {
            it->centre = QPointF();
        } else if (!node->loosePlace) {
            const int hash = hashOf(it.key());
            it->centre = QPointF(
                std::round(it->centre.x() * FREE_SPREAD_X + hash % JITTER_X - HALF_JITTER_X),
                std::round(it->centre.y() * FREE_SPREAD_Y + hash % JITTER_Y - HALF_JITTER_Y));
        }
    }
    return places;
}

}  // namespace

QHash<QString, MindMapPlace> layOutMindMap(const MindMap& map, MindMapLayout layout,
                                           const MindMapSizes& sizes,
                                           const MindMapLayoutOptions& options) {
    if (layout == MindMapLayout::Free) {
        return freeLayout(map, sizes, options);
    }
    return Layouter(map, sizes, options).layOut(layout);
}

}  // namespace kalahari::mindmap
