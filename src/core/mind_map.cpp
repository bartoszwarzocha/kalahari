// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map.cpp
/// @brief A mind map of the Workshop: its tree of nodes, its loose nodes and its connections,
/// and their changes

#include <kalahari/core/mind_map.h>

#include <algorithm>

namespace kalahari::core {

namespace {

constexpr double MIN_ZOOM = 0.25;
constexpr double MAX_ZOOM = 4.0;

}  // namespace

MindMap::MindMap() : m_root(QStringLiteral("n1")) {
    MindMapNode root;
    root.id = m_root;
    m_nodes.insert(m_root, root);
}

const MindMapNode* MindMap::node(const QString& id) const {
    const auto it = m_nodes.constFind(id);
    return it != m_nodes.cend() ? &*it : nullptr;
}

MindMapNode* MindMap::findNode(const QString& id) {
    const auto it = m_nodes.find(id);
    return it != m_nodes.end() ? &*it : nullptr;
}

bool MindMap::setNode(const MindMapNode& node) {
    MindMapNode* own = findNode(node.id);
    if (own == nullptr) {
        return false;
    }
    const QStringList children = own->children;
    const std::optional<QPointF> loosePlace = own->loosePlace;
    *own = node;
    own->children = children;
    // Only a loose node has a loose place
    if (!m_loose.contains(node.id)) {
        own->loosePlace.reset();
    } else if (!node.loosePlace) {
        own->loosePlace = loosePlace;
    }
    return true;
}

QStringList MindMap::nodeIds() const {
    QStringList ids{m_root};
    ids.append(descendantsOf(m_root));
    for (const QString& loose : m_loose) {
        ids.append(loose);
        ids.append(descendantsOf(loose));
    }
    return ids;
}

QString MindMap::parentOf(const QString& id) const {
    return m_parents.value(id);
}

int MindMap::depthOf(const QString& id) const {
    int depth = 0;
    QString at = id;
    while (!m_parents.value(at).isEmpty()) {
        at = m_parents.value(at);
        ++depth;
    }
    // The top of a loose branch stands one level below the main topic
    return m_loose.contains(at) ? depth + 1 : depth;
}

QStringList MindMap::descendantsOf(const QString& id) const {
    QStringList ids;
    const MindMapNode* parent = node(id);
    if (parent == nullptr) {
        return ids;
    }
    for (const QString& child : parent->children) {
        ids.append(child);
        ids.append(descendantsOf(child));
    }
    return ids;
}

QString MindMap::branchOf(const QString& id) const {
    if (id == m_root || node(id) == nullptr) {
        return QString();
    }
    QString at = id;
    while (!m_parents.value(at).isEmpty() && m_parents.value(at) != m_root) {
        at = m_parents.value(at);
    }
    return at;
}

bool MindMap::isInLooseBranch(const QString& id) const {
    return m_loose.contains(branchOf(id));
}

QString MindMap::newId() const {
    for (qsizetype number = m_nodes.size() + 1;; ++number) {
        const QString id = QStringLiteral("n%1").arg(number);
        if (!m_nodes.contains(id)) {
            return id;
        }
    }
}

// =============================================================================
// Changes
// =============================================================================

QString MindMap::addChild(const QString& parent, const QString& text, qsizetype index) {
    MindMapNode* own = findNode(parent);
    if (own == nullptr) {
        return QString();
    }
    MindMapNode child;
    child.id = newId();
    child.text = text;
    const qsizetype at = index < 0 || index > own->children.size() ? own->children.size() : index;
    own->children.insert(at, child.id);
    own->collapsed = false;
    m_parents.insert(child.id, parent);
    m_nodes.insert(child.id, child);
    return child.id;
}

QString MindMap::addSibling(const QString& id, const QString& text, bool before) {
    if (node(id) == nullptr) {
        return QString();
    }
    if (id == m_root) {
        return addChild(id, text);
    }
    const QString parent = parentOf(id);
    if (parent.isEmpty()) {
        const QPointF place = node(id)->loosePlace.value_or(QPointF());
        constexpr double STEP = 46.0;
        const QString added = addLoose(text, place + QPointF(0.0, before ? -STEP : STEP));
        // Beside it in the list of the loose nodes too
        m_loose.removeAll(added);
        m_loose.insert(m_loose.indexOf(id) + (before ? 0 : 1), added);
        return added;
    }
    const qsizetype at = node(parent)->children.indexOf(id);
    return addChild(parent, text, before ? at : at + 1);
}

QString MindMap::addLoose(const QString& text, const QPointF& place) {
    MindMapNode loose;
    loose.id = newId();
    loose.text = text;
    loose.loosePlace = place;
    m_nodes.insert(loose.id, loose);
    m_loose.append(loose.id);
    return loose.id;
}

void MindMap::detach(const QString& id) {
    const QString parent = parentOf(id);
    if (MindMapNode* own = findNode(parent)) {
        own->children.removeAll(id);
    }
    m_parents.remove(id);
    m_loose.removeAll(id);
}

QStringList MindMap::remove(const QString& id) {
    if (id == m_root || node(id) == nullptr) {
        return QStringList();
    }
    QStringList removed{id};
    removed.append(descendantsOf(id));
    detach(id);
    for (const QString& gone : removed) {
        m_nodes.remove(gone);
        m_parents.remove(gone);
    }
    m_connections.removeIf([&removed](const MindMapConnection& connection) {
        return removed.contains(connection.from) || removed.contains(connection.to);
    });
    return removed;
}

bool MindMap::move(const QString& id, const QString& parent, qsizetype index) {
    if (id == m_root || node(id) == nullptr || node(parent) == nullptr || id == parent ||
        descendantsOf(id).contains(parent)) {
        return false;
    }
    // A place in the list of the parent counts without the node itself
    MindMapNode* own = findNode(id);
    if (parentOf(id) == parent) {
        const qsizetype was = node(parent)->children.indexOf(id);
        if (index > was) {
            --index;
        }
    }
    detach(id);
    own->loosePlace.reset();
    MindMapNode* target = findNode(parent);
    const qsizetype at =
        index < 0 || index > target->children.size() ? target->children.size() : index;
    target->children.insert(at, id);
    target->collapsed = false;
    m_parents.insert(id, parent);
    return true;
}

bool MindMap::makeLoose(const QString& id, const QPointF& place) {
    MindMapNode* own = findNode(id);
    if (id == m_root || own == nullptr) {
        return false;
    }
    detach(id);
    own->loosePlace = place;
    m_loose.append(id);
    return true;
}

bool MindMap::moveAmongSiblings(const QString& id, int steps) {
    const QString parent = parentOf(id);
    QStringList* list = parent.isEmpty() ? &m_loose : &findNode(parent)->children;
    const qsizetype at = list->indexOf(id);
    const qsizetype to = at + steps;
    if (at < 0 || to < 0 || to >= list->size() || steps == 0) {
        return false;
    }
    list->move(at, to);
    return true;
}

bool MindMap::indent(const QString& id) {
    const QString parent = parentOf(id);
    if (parent.isEmpty()) {
        return false;
    }
    const QStringList& siblings = node(parent)->children;
    const qsizetype at = siblings.indexOf(id);
    if (at <= 0) {
        return false;
    }
    const QString before = siblings.at(at - 1);
    return move(id, before, node(before)->children.size());
}

bool MindMap::outdent(const QString& id) {
    const QString parent = parentOf(id);
    const QString grandparent = parentOf(parent);
    if (parent.isEmpty() || grandparent.isEmpty()) {
        return false;
    }
    return move(id, grandparent, node(grandparent)->children.indexOf(parent) + 1);
}

bool MindMap::connect(const QString& from, const QString& to, const QString& label) {
    if (from == to || node(from) == nullptr || node(to) == nullptr) {
        return false;
    }
    const bool known = std::any_of(m_connections.cbegin(), m_connections.cend(),
                                   [&from, &to](const MindMapConnection& connection) {
                                       return (connection.from == from && connection.to == to) ||
                                              (connection.from == to && connection.to == from);
                                   });
    if (known) {
        return false;
    }
    m_connections.append(MindMapConnection{from, to, label, QJsonObject()});
    return true;
}

bool MindMap::setConnection(qsizetype index, const MindMapConnection& connection) {
    if (index < 0 || index >= m_connections.size() || node(connection.from) == nullptr ||
        node(connection.to) == nullptr || connection.from == connection.to) {
        return false;
    }
    m_connections[index] = connection;
    return true;
}

bool MindMap::disconnect(qsizetype index) {
    if (index < 0 || index >= m_connections.size()) {
        return false;
    }
    m_connections.removeAt(index);
    return true;
}

bool MindMap::forgetElement(const QString& elementId, const QString& title) {
    bool changed = false;
    for (MindMapNode& own : m_nodes) {
        if (own.link != elementId) {
            continue;
        }
        own.link.clear();
        if (own.text.isEmpty()) {
            own.text = title;
        }
        changed = true;
    }
    return changed;
}

void MindMap::setZoom(double zoom) {
    m_zoom = std::clamp(zoom, MIN_ZOOM, MAX_ZOOM);
}

void MindMap::fixFreePlaces(const QHash<QString, QPointF>& places) {
    for (auto it = places.cbegin(); it != places.cend(); ++it) {
        MindMapNode* own = findNode(it.key());
        if (own != nullptr && !own->freePlace) {
            own->freePlace = it.value();
        }
    }
}

}  // namespace kalahari::core
