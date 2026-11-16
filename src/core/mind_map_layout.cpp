// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_layout.cpp
/// @brief The layouts of a mind map: on both sides, to the right, down, and free

#include <kalahari/core/mind_map_layout.h>

#include <QStringList>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace kalahari::core {

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
    Layouter(const MindMap& map, const MindMapSizes& sizes) : m_map(map), m_sizes(sizes) {}

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
        return node != nullptr && !node->collapsed ? node->children : QStringList();
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
        double total = ALONG * static_cast<double>(children.size() - 1);
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
        double total = DOWN_ALONG * static_cast<double>(children.size() - 1);
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
        double total = ALONG * static_cast<double>(children.size() - 1);
        for (const QString& child : children) {
            total += branchHeight(child);
        }
        double y = centre.y() - total / 2.0;
        for (const QString& child : children) {
            const double height = branchHeight(child);
            const double x = centre.x() + side * (own.width() / 2.0 + ACROSS +
                                                  size(child).width() / 2.0);
            this->side(child, QPointF(x, y + height / 2.0), side, depth + 1);
            y += height + ALONG;
        }
    }

    /// The main branches @p branches on side @p side of the main topic
    void group(const QStringList& branches, int side) {
        double total = MAIN_ALONG * static_cast<double>(std::max<qsizetype>(0, branches.size() - 1));
        for (const QString& branch : branches) {
            total += branchHeight(branch);
        }
        const double rootHalf = size(m_map.root()).width() / 2.0;
        double y = -total / 2.0;
        for (const QString& branch : branches) {
            const double height = branchHeight(branch);
            const double x = side * (rootHalf + ACROSS + ROOT_ROOM + size(branch).width() / 2.0);
            this->side(branch, QPointF(x, y + height / 2.0), side, 1);
            y += height + MAIN_ALONG;
        }
    }

    /// Down: a node with its top at @p top and its nodes below it
    void down(const QString& id, double centreX, double top, int depth) {
        const QSizeF own = size(id);
        m_places.insert(id,
                        MindMapPlace{QPointF(centreX, top + own.height() / 2.0), own, 0, depth});
        const QStringList children = shown(id);
        if (children.isEmpty()) {
            return;
        }
        double total = DOWN_ALONG * static_cast<double>(children.size() - 1);
        for (const QString& child : children) {
            total += branchWidth(child);
        }
        double x = centreX - total / 2.0;
        const double nextTop = top + own.height() + DOWN_ACROSS;
        for (const QString& child : children) {
            const double width = branchWidth(child);
            down(child, x + width / 2.0, nextTop, depth + 1);
            x += width + DOWN_ALONG;
        }
    }

    /// The loose nodes at their places; without one, above the tree on the right
    void placeLoose(bool downwards) {
        double top = std::numeric_limits<double>::max();
        double right = std::numeric_limits<double>::lowest();
        for (const MindMapPlace& place : std::as_const(m_places)) {
            top = std::min(top, place.centre.y() - place.size.height() / 2.0);
            right = std::max(right, place.centre.x() + place.size.width() / 2.0);
        }
        for (const QString& loose : m_map.looseNodes()) {
            const QPointF at = m_map.node(loose)->loosePlace.value_or(
                QPointF(right - size(loose).width() / 2.0, top - LOOSE_ABOVE));
            const QStringList branch = QStringList{loose} + m_map.descendantsOf(loose);
            if (downwards) {
                down(loose, at.x(), at.y() - size(loose).height() / 2.0, 1);
            } else {
                side(loose, at, 1, 1);
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
    QHash<QString, QSizeF> m_sizeCache;
    QHash<QString, MindMapPlace> m_places;
};

/// The free layout: the writer's places, else the places on both sides moved a little
QHash<QString, MindMapPlace> freeLayout(const MindMap& map, const MindMapSizes& sizes) {
    QHash<QString, MindMapPlace> places = Layouter(map, sizes).layOut(MindMapLayout::BothSides);
    constexpr int JITTER_X = 9;
    constexpr int JITTER_Y = 7;
    for (auto it = places.begin(); it != places.end(); ++it) {
        const MindMapNode* node = map.node(it.key());
        if (node->freePlace) {
            it->centre = *node->freePlace;
        } else if (it.key() == map.root()) {
            it->centre = QPointF();
        } else if (!node->loosePlace) {
            const int hash = hashOf(it.key());
            it->centre = QPointF(
                std::round(it->centre.x() * FREE_SPREAD_X + hash % JITTER_X - JITTER_X / 2),
                std::round(it->centre.y() * FREE_SPREAD_Y + hash % JITTER_Y - JITTER_Y / 2));
        }
    }
    return places;
}

}  // namespace

QHash<QString, MindMapPlace> layOutMindMap(const MindMap& map, MindMapLayout layout,
                                           const MindMapSizes& sizes) {
    if (layout == MindMapLayout::Free) {
        return freeLayout(map, sizes);
    }
    return Layouter(map, sizes).layOut(layout);
}

}  // namespace kalahari::core
