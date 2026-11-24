// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map.cpp
/// @brief A mind map of the Workshop: its tree of nodes, its loose nodes, its connections and
/// its frames, and their changes

#include <kalahari/mindmap/mind_map.h>

#include <algorithm>
#include <array>

namespace kalahari::mindmap {

QString MindMapNode::kindOrIdea(bool root) const {
    if (!kind.isEmpty()) {
        return kind;
    }
    return QString::fromLatin1(root ? TOPIC_KIND : IDEA_KIND);
}

bool MindMapNode::isOwn(bool root) const {
    static const std::array<QString, 4> own{QString::fromLatin1(TOPIC_KIND),
                                            QString::fromLatin1(IDEA_KIND),
                                            QString::fromLatin1(QUESTION_KIND),
                                            QString::fromLatin1(NOTE_KIND)};
    return std::find(own.cbegin(), own.cend(), kindOrIdea(root)) != own.cend();
}

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
    for (MindMapFrame& frame : m_frames) {
        frame.nodes.removeIf([&removed](const QString& in) { return removed.contains(in); });
    }
    m_frames.removeIf([](const MindMapFrame& frame) { return frame.nodes.isEmpty(); });
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
    const qsizetype at = index < 0 || index > target->children.size() ? target->children.size()
                                                                      : index;
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
    if (connectionOf(from, to) >= 0) {
        return false;
    }
    m_connections.append(MindMapConnection{from, to, label, QJsonObject()});
    return true;
}

qsizetype MindMap::connectionOf(const QString& from, const QString& to, bool eitherWay) const {
    for (qsizetype i = 0; i < m_connections.size(); ++i) {
        const MindMapConnection& connection = m_connections.at(i);
        if ((connection.from == from && connection.to == to) ||
            (eitherWay && connection.from == to && connection.to == from)) {
            return i;
        }
    }
    return -1;
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

// =============================================================================
// The map of nodes
// =============================================================================

QString MindMap::addNode(const QString& kind, const QString& text, const QPointF& place,
                         const QString& parent) {
    QString id;
    if (parent.isEmpty()) {
        id = addLoose(text, QPointF());
        findNode(id)->loosePlace.reset();
    } else {
        id = addChild(parent, text);
    }
    if (MindMapNode* own = findNode(id)) {
        own->kind = kind == QLatin1String(MindMapNode::IDEA_KIND) ? QString() : kind;
        own->place = place;
    }
    return id;
}

bool MindMap::showsRoot() const {
    const MindMapNode* main = node(m_root);
    return !m_relationMap || (main != nullptr && !main->children.isEmpty());
}

bool MindMap::setPlace(const QString& id, const QPointF& place) {
    MindMapNode* own = findNode(id);
    if (own == nullptr) {
        return false;
    }
    own->place = place;
    return true;
}

bool MindMap::removeAlone(const QString& id) {
    MindMapNode* own = findNode(id);
    if (id == m_root || own == nullptr) {
        return false;
    }
    const QStringList children = own->children;
    for (const QString& child : children) {
        detach(child);
        m_loose.append(child);
    }
    own->children.clear();
    remove(id);
    return true;
}

QString MindMap::topOf(const QString& id) const {
    if (node(id) == nullptr) {
        return QString();
    }
    QString at = id;
    while (!m_parents.value(at).isEmpty()) {
        at = m_parents.value(at);
    }
    return at;
}

QString MindMap::nodeOfElement(const QString& elementId) const {
    if (elementId.isEmpty()) {
        return QString();
    }
    for (const QString& id : nodeIds()) {
        const MindMapNode* own = node(id);
        if (own->link == elementId && own->kind == QLatin1String(MindMapNode::ELEMENT_KIND)) {
            return id;
        }
    }
    return QString();
}

const MindMapFrame* MindMap::frame(const QString& id) const {
    const auto it = std::find_if(m_frames.cbegin(), m_frames.cend(),
                                 [&id](const MindMapFrame& frame) { return frame.id == id; });
    return it != m_frames.cend() ? &*it : nullptr;
}

const MindMapFrame* MindMap::frameOf(const QString& nodeId) const {
    const auto it = std::find_if(m_frames.cbegin(), m_frames.cend(),
                                 [&nodeId](const MindMapFrame& frame) {
                                     return frame.nodes.contains(nodeId);
                                 });
    return it != m_frames.cend() ? &*it : nullptr;
}

QString MindMap::newFrameId() const {
    for (qsizetype number = m_frames.size() + 1;; ++number) {
        const QString id = QStringLiteral("f%1").arg(number);
        if (frame(id) == nullptr) {
            return id;
        }
    }
}

QString MindMap::addFrame(const QString& title, const QStringList& nodes) {
    MindMapFrame added;
    added.id = newFrameId();
    added.title = title;
    added.nodes = nodes;
    return setFrame(added) ? added.id : QString();
}

bool MindMap::setFrame(const MindMapFrame& frame) {
    MindMapFrame own = frame;
    QStringList nodes;
    for (const QString& id : frame.nodes) {
        if (node(id) != nullptr && !nodes.contains(id)) {
            nodes.append(id);
        }
    }
    own.nodes = nodes;
    if (own.id.isEmpty() || own.nodes.isEmpty()) {
        return false;
    }
    // Each node in one frame at most
    for (MindMapFrame& other : m_frames) {
        if (other.id != own.id) {
            other.nodes.removeIf([&nodes](const QString& id) { return nodes.contains(id); });
        }
    }
    const auto it = std::find_if(m_frames.begin(), m_frames.end(),
                                 [&own](const MindMapFrame& other) { return other.id == own.id; });
    if (it != m_frames.end()) {
        *it = own;
    } else {
        m_frames.append(own);
    }
    m_frames.removeIf([](const MindMapFrame& other) { return other.nodes.isEmpty(); });
    return true;
}

bool MindMap::removeFrame(const QString& id) {
    return m_frames.removeIf([&id](const MindMapFrame& frame) { return frame.id == id; }) > 0;
}

bool MindMap::setFrameOf(const QString& nodeId, const QString& frameId) {
    if (node(nodeId) == nullptr || (!frameId.isEmpty() && frame(frameId) == nullptr)) {
        return false;
    }
    if (!frameId.isEmpty()) {
        MindMapFrame changed = *frame(frameId);
        if (!changed.nodes.contains(nodeId)) {
            changed.nodes.append(nodeId);
        }
        return setFrame(changed);
    }
    for (MindMapFrame& own : m_frames) {
        own.nodes.removeAll(nodeId);
    }
    m_frames.removeIf([](const MindMapFrame& own) { return own.nodes.isEmpty(); });
    return true;
}

QStringList MindMap::attributeNames() const {
    QStringList names;
    for (const QString& id : nodeIds()) {
        for (const NodeAttribute& attribute : node(id)->attributes) {
            if (!attribute.name.isEmpty() && !names.contains(attribute.name)) {
                names.append(attribute.name);
            }
        }
    }
    return names;
}

bool MindMap::hasNodesWithoutPlace() const {
    return std::any_of(m_nodes.cbegin(), m_nodes.cend(),
                       [](const MindMapNode& own) { return !own.place; });
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
        // A node of the element, of its scene or of its thread is an idea now
        if (!own.isOwn(own.id == m_root)) {
            own.kind.clear();
            own.rows.reset();
            own.scene = -1;
            own.planned = false;
            own.thread.clear();
            if (own.image == QLatin1String(MindMapNode::PORTRAIT)) {
                own.image.clear();
            }
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

}  // namespace kalahari::mindmap
