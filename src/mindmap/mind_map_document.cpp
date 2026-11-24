// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_document.cpp
/// @brief A mind map open on its tab: its file, its changes and their undoing

#include <kalahari/mindmap/mind_map_document.h>

#include <kalahari/core/logger.h>

#include <QFileInfo>

#include <algorithm>
#include <utility>

namespace kalahari::mindmap {

MindMapDocument::MindMapDocument(const QString& path, QObject* parent)
    : QObject(parent), m_path(path), m_selected(m_map.root()), m_selection{m_map.root()} {}

bool MindMapDocument::load(QString* error) {
    MindMap loaded;
    if (!QFileInfo::exists(m_path) || !loaded.load(m_path, error)) {
        core::Logger::getInstance().warn("MindMapDocument: {} cannot be read; the map is empty",
                                         m_path.toStdString());
        return false;
    }
    m_map = loaded;
    m_selected = m_map.root();
    m_selection = {m_selected};
    m_undo.clear();
    m_redo.clear();
    emit changed();
    return true;
}

QString MindMapDocument::shownText(const MindMapNode& node) const {
    return m_shownText ? m_shownText(node) : node.text;
}

QString MindMapDocument::shownText(const QString& id) const {
    const MindMapNode* node = m_map.node(id);
    return node != nullptr ? shownText(*node) : QString();
}

void MindMapDocument::pushStep(Step step) {
    m_undo.append(std::move(step));
    if (m_undo.size() > MOST_STEPS) {
        m_undo.removeFirst();
    }
    m_redo.clear();
}

void MindMapDocument::keepSelection() {
    m_selection.removeIf([this](const QString& id) { return m_map.node(id) == nullptr; });
    if (!m_selected.isEmpty() && m_map.node(m_selected) == nullptr) {
        m_selected = m_selection.isEmpty() ? m_map.root() : m_selection.first();
    }
    if (!m_selected.isEmpty() && !m_selection.contains(m_selected)) {
        m_selection.prepend(m_selected);
    }
    if (m_selectedConnection >= m_map.connections().size()) {
        m_selectedConnection = -1;
    }
}

bool MindMapDocument::change(const QString& what, const std::function<bool(MindMap&)>& change) {
    Step before{m_map.toJson(), m_selected, what, nullptr, nullptr};
    if (!change(m_map)) {
        m_map.fromJson(before.map);
        return false;
    }
    if (m_map.toJson() == before.map) {
        return false;
    }
    pushStep(std::move(before));
    keepSelection();
    save();
    emit changed();
    return true;
}

bool MindMapDocument::changeWithBook(const QString& what,
                                     const std::function<bool(MindMap&)>& change,
                                     const std::function<bool()>& doBook,
                                     const std::function<bool()>& undoBook) {
    Step before{m_map.toJson(), m_selected, what, undoBook, doBook};
    if (change && !change(m_map)) {
        m_map.fromJson(before.map);
        return false;
    }
    if (!doBook()) {
        m_map.fromJson(before.map);
        return false;
    }
    const bool mapChanged = m_map.toJson() != before.map;
    pushStep(std::move(before));
    keepSelection();
    if (mapChanged) {
        save();
    }
    emit changed();
    return true;
}

void MindMapDocument::changeView(const std::function<void(MindMap&)>& change) {
    const QJsonObject before = m_map.toJson();
    change(m_map);
    if (m_map.toJson() != before) {
        save();
        emit viewChanged();
    }
}

QString MindMapDocument::undoText() const {
    return m_undo.isEmpty() ? QString() : m_undo.last().what;
}

QString MindMapDocument::redoText() const {
    return m_redo.isEmpty() ? QString() : m_redo.last().what;
}

void MindMapDocument::restore(const Step& step) {
    // The view of the map (its zoom, its mode) stays as it is now
    const MindMapMode mode = m_map.mode();
    const double zoom = m_map.zoom();
    const std::optional<QPointF> centre = m_map.centre();
    m_map.fromJson(step.map);
    m_map.setMode(mode);
    m_map.setZoom(zoom);
    m_map.setCentre(centre);
    // Nothing was selected: nothing is
    m_selected = step.selected.isEmpty() || m_map.node(step.selected) != nullptr ? step.selected
                                                                                 : m_map.root();
    m_selection = m_selected.isEmpty() ? QStringList() : QStringList{m_selected};
    m_selectedConnection = -1;
    save();
    emit changed();
    emit selectionChanged();
}

void MindMapDocument::undo() {
    if (m_undo.isEmpty()) {
        return;
    }
    const Step step = m_undo.takeLast();
    m_redo.append(Step{m_map.toJson(), m_selected, step.what, step.undoBook, step.redoBook});
    if (step.undoBook && !step.undoBook()) {
        emit bookStepFailed(step.what);
    }
    restore(step);
}

void MindMapDocument::redo() {
    if (m_redo.isEmpty()) {
        return;
    }
    const Step step = m_redo.takeLast();
    m_undo.append(Step{m_map.toJson(), m_selected, step.what, step.undoBook, step.redoBook});
    if (step.redoBook && !step.redoBook()) {
        emit bookStepFailed(step.what);
    }
    restore(step);
}

void MindMapDocument::select(const QString& id) {
    if (m_map.node(id) == nullptr ||
        (id == m_selected && m_selectedConnection < 0 && m_selection.size() == 1)) {
        return;
    }
    m_selected = id;
    m_selection = {id};
    m_selectedConnection = -1;
    emit selectionChanged();
}

void MindMapDocument::clearSelection() {
    if (m_selected.isEmpty() && m_selection.isEmpty() && m_selectedConnection < 0) {
        return;
    }
    m_selected.clear();
    m_selection.clear();
    m_selectedConnection = -1;
    emit selectionChanged();
}

void MindMapDocument::setSelection(const QStringList& ids) {
    QStringList known;
    for (const QString& id : ids) {
        if (m_map.node(id) != nullptr && !known.contains(id)) {
            known.append(id);
        }
    }
    if (known.isEmpty() || (known == m_selection && m_selectedConnection < 0)) {
        return;
    }
    m_selection = known;
    m_selected = known.first();
    m_selectedConnection = -1;
    emit selectionChanged();
}

void MindMapDocument::toggleSelection(const QString& id) {
    if (m_map.node(id) == nullptr) {
        return;
    }
    QStringList changed = m_selection;
    if (changed.contains(id)) {
        if (changed.size() == 1) {
            clearSelection();
            return;
        }
        changed.removeAll(id);
    } else {
        changed.prepend(id);
    }
    m_selection = changed;
    m_selected = changed.first();
    m_selectedConnection = -1;
    emit selectionChanged();
}

void MindMapDocument::selectConnection(qsizetype index) {
    if (index < 0 || index >= m_map.connections().size() || index == m_selectedConnection) {
        return;
    }
    m_selectedConnection = index;
    emit selectionChanged();
}

// =============================================================================
// The changes of both views
// =============================================================================

QString MindMapDocument::addChild(const QString& parent) {
    QString added;
    //: What Undo undoes in a mind map. In Polish: dodanie węzła
    change(tr("adding a node"), [&parent, &added](MindMap& map) {
        added = map.addChild(parent, QString());
        return !added.isEmpty();
    });
    select(added);
    return added;
}

QString MindMapDocument::addSibling(const QString& id, bool before) {
    QString added;
    change(tr("adding a node"), [&id, before, &added](MindMap& map) {
        added = map.addSibling(id, QString(), before);
        return !added.isEmpty();
    });
    select(added);
    return added;
}

QString MindMapDocument::addLoose(const QPointF& place) {
    QString added;
    //: What Undo undoes in a mind map. In Polish: dodanie luźnego węzła
    change(tr("adding a loose node"), [&place, &added](MindMap& map) {
        added = map.addLoose(QString(), place);
        return !added.isEmpty();
    });
    select(added);
    return added;
}

int MindMapDocument::removeNode(const QString& id) {
    const QString parent = m_map.parentOf(id);
    int removed = 0;
    //: What Undo undoes in a mind map. In Polish: usunięcie węzła
    change(tr("deleting a node"), [&id, &removed](MindMap& map) {
        removed = static_cast<int>(map.remove(id).size());
        return removed > 0;
    });
    if (removed > 0) {
        select(m_map.node(parent) != nullptr ? parent : m_map.root());
    }
    return removed;
}

bool MindMapDocument::setText(const QString& id, const QString& text) {
    const MindMapNode* node = m_map.node(id);
    if (node == nullptr) {
        return false;
    }
    MindMapNode changed = *node;
    changed.text.clear();
    changed.text = text.trimmed() == shownText(changed) ? QString() : text.trimmed();
    //: What Undo undoes in a mind map. In Polish: zmiana tekstu węzła
    return change(tr("changing the text of a node"),
                  [&changed](MindMap& map) { return map.setNode(changed); });
}

bool MindMapDocument::toggleCollapsed(const QString& id) {
    const MindMapNode* node = m_map.node(id);
    if (node == nullptr || node->children.isEmpty()) {
        return false;
    }
    MindMapNode changed = *node;
    changed.collapsed = !changed.collapsed;
    const bool done = change(changed.collapsed
                                 //: What Undo undoes in a mind map. In Polish: zwinięcie gałęzi
                                 ? tr("collapsing a branch")
                                 //: What Undo undoes in a mind map. In Polish: rozwinięcie
                                 //: gałęzi
                                 : tr("expanding a branch"),
                             [&changed](MindMap& map) { return map.setNode(changed); });
    // A node inside the branch just collapsed is no more seen: its branch is selected
    if (done && changed.collapsed && m_map.descendantsOf(id).contains(m_selected)) {
        select(id);
    }
    return done;
}

bool MindMapDocument::toggleMarker(const QString& id, const QString& marker) {
    const MindMapNode* node = m_map.node(id);
    if (node == nullptr) {
        return false;
    }
    MindMapNode changed = *node;
    if (!changed.markers.removeAll(marker)) {
        changed.markers.append(marker);
    }
    //: What Undo undoes in a mind map. In Polish: zmiana znaczników
    return change(tr("changing the markers"),
                  [&changed](MindMap& map) { return map.setNode(changed); });
}

bool MindMapDocument::setColor(const QString& id, const QString& color) {
    const MindMapNode* node = m_map.node(id);
    if (node == nullptr) {
        return false;
    }
    MindMapNode changed = *node;
    changed.color = color;
    //: What Undo undoes in a mind map. In Polish: zmiana koloru gałęzi
    return change(tr("changing the color of a branch"),
                  [&changed](MindMap& map) { return map.setNode(changed); });
}

bool MindMapDocument::setNote(const QString& id, const QString& note) {
    const MindMapNode* node = m_map.node(id);
    if (node == nullptr) {
        return false;
    }
    MindMapNode changed = *node;
    changed.note = note.trimmed();
    //: What Undo undoes in a mind map. In Polish: zmiana notatki
    return change(tr("changing the note"),
                  [&changed](MindMap& map) { return map.setNode(changed); });
}

bool MindMapDocument::setLink(const QString& id, const QString& elementId) {
    const MindMapNode* node = m_map.node(id);
    if (node == nullptr) {
        return false;
    }
    MindMapNode changed = *node;
    changed.link = elementId;
    //: What Undo undoes in a mind map. In Polish: zmiana powiązania z elementem
    return change(tr("changing the linked element"),
                  [&changed](MindMap& map) { return map.setNode(changed); });
}

bool MindMapDocument::moveNode(const QString& id, const QString& parent, qsizetype index) {
    //: What Undo undoes in a mind map. In Polish: przeniesienie węzła
    return change(tr("moving a node"),
                  [&id, &parent, index](MindMap& map) { return map.move(id, parent, index); });
}

bool MindMapDocument::moveAmongSiblings(const QString& id, int steps) {
    return change(tr("moving a node"),
                  [&id, steps](MindMap& map) { return map.moveAmongSiblings(id, steps); });
}

bool MindMapDocument::indent(const QString& id) {
    return change(tr("moving a node"), [&id](MindMap& map) { return map.indent(id); });
}

bool MindMapDocument::outdent(const QString& id) {
    return change(tr("moving a node"), [&id](MindMap& map) { return map.outdent(id); });
}

bool MindMapDocument::makeLoose(const QString& id, const QPointF& place) {
    //: What Undo undoes in a mind map. In Polish: odłączenie węzła od gałęzi
    return change(tr("detaching a node"),
                  [&id, &place](MindMap& map) { return map.makeLoose(id, place); });
}

bool MindMapDocument::movePlaces(const QHash<QString, QPointF>& places, bool free) {
    return change(tr("moving a node"), [&places, free](MindMap& map) {
        for (auto it = places.cbegin(); it != places.cend(); ++it) {
            const MindMapNode* node = map.node(it.key());
            if (node == nullptr) {
                return false;
            }
            MindMapNode moved = *node;
            if (free) {
                moved.freePlace = it.value();
            } else {
                moved.loosePlace = it.value();
            }
            map.setNode(moved);
        }
        return true;
    });
}

bool MindMapDocument::connectNodes(const QString& from, const QString& to) {
    //: What Undo undoes in a mind map. In Polish: połączenie węzłów
    const bool done = change(tr("connecting nodes"),
                             [&from, &to](MindMap& map) { return map.connect(from, to); });
    if (done) {
        selectConnection(m_map.connections().size() - 1);
    }
    return done;
}

bool MindMapDocument::setConnectionLabel(qsizetype index, const QString& label) {
    if (index < 0 || index >= m_map.connections().size()) {
        return false;
    }
    MindMapConnection changed = m_map.connections().at(index);
    changed.label = label.trimmed();
    //: What Undo undoes in a mind map. In Polish: zmiana podpisu połączenia
    return change(tr("changing the label of a connection"),
                  [index, &changed](MindMap& map) { return map.setConnection(index, changed); });
}

bool MindMapDocument::reverseConnection(qsizetype index) {
    if (index < 0 || index >= m_map.connections().size()) {
        return false;
    }
    MindMapConnection changed = m_map.connections().at(index);
    std::swap(changed.from, changed.to);
    //: What Undo undoes in a mind map. In Polish: odwrócenie połączenia
    return change(tr("reversing a connection"),
                  [index, &changed](MindMap& map) { return map.setConnection(index, changed); });
}

bool MindMapDocument::removeConnection(qsizetype index) {
    //: What Undo undoes in a mind map. In Polish: usunięcie połączenia
    const bool done = change(tr("deleting a connection"),
                             [index](MindMap& map) { return map.disconnect(index); });
    if (done) {
        m_selectedConnection = -1;
        emit selectionChanged();
    }
    return done;
}

bool MindMapDocument::setLayout(MindMapLayout layout) {
    //: What Undo undoes in a mind map. In Polish: zmiana układu mapy
    return change(tr("changing the layout"), [layout](MindMap& map) {
        map.setLayout(layout);
        return true;
    });
}

QStringList MindMapDocument::pasteOutline(const QString& parent, qsizetype index,
                                          const QString& text) {
    QStringList added;
    //: What Undo undoes in a mind map. In Polish: wklejenie
    change(tr("pasting"), [&parent, index, &text, &added](MindMap& map) {
        added = map.addOutline(parent, text, index);
        return !added.isEmpty();
    });
    if (!added.isEmpty()) {
        select(added.first());
    }
    return added;
}

// =============================================================================
// The changes of the map of nodes
// =============================================================================

QString MindMapDocument::addNode(const QString& kind, const QString& text, const QPointF& place,
                                 const QString& parent) {
    QString added;
    const bool folded = m_newFolded;
    //: What Undo undoes in a mind map. In Polish: dodanie węzła
    change(tr("adding a node"), [&](MindMap& map) {
        added = map.addNode(kind, text, place, parent);
        const MindMapNode* own = map.node(added);
        if (own == nullptr) {
            return false;
        }
        MindMapNode changed = *own;
        changed.folded = folded;
        return map.setNode(changed);
    });
    select(added);
    return added;
}

QString MindMapDocument::addEntry(const CatalogEntry& entry, const QPointF& place,
                                  const QString& parent) {
    QString added;
    const bool folded = m_newFolded;
    change(tr("adding a node"), [&](MindMap& map) {
        added = map.addNode(entry.kind, QString(), place, parent);
        const MindMapNode* own = map.node(added);
        if (own == nullptr) {
            return false;
        }
        MindMapNode changed = *own;
        changed.link = entry.elementId;
        changed.scene = entry.scene;
        changed.planned = entry.planned;
        changed.thread = entry.thread;
        changed.folded = folded;
        return map.setNode(changed);
    });
    select(added);
    return added;
}

QString MindMapDocument::addSiblingAt(const QString& id, const QPointF& place, bool before,
                                      const QString& text) {
    QString added;
    const bool folded = m_newFolded;
    change(tr("adding a node"), [&](MindMap& map) {
        added = map.addSibling(id, text, before);
        const MindMapNode* own = map.node(added);
        if (own == nullptr) {
            return false;
        }
        MindMapNode changed = *own;
        changed.place = place;
        changed.loosePlace.reset();
        changed.folded = folded;
        return map.setNode(changed);
    });
    select(added);
    return added;
}

int MindMapDocument::removeNodes(const QStringList& ids) {
    int removed = 0;
    QString near;
    for (const QString& id : ids) {
        if (id != m_map.root() && m_map.node(id) != nullptr && near.isEmpty()) {
            near = m_map.parentOf(id);
        }
    }
    //: What Undo undoes in a mind map. In Polish: usunięcie węzła
    change(tr("deleting a node"), [&ids, &removed](MindMap& map) {
        for (const QString& id : ids) {
            if (map.removeAlone(id)) {
                ++removed;
            }
        }
        return removed > 0;
    });
    if (removed > 0) {
        select(m_map.node(near) != nullptr ? near : m_map.root());
    }
    return removed;
}

bool MindMapDocument::moveNodesBy(const QStringList& ids, const QPointF& delta) {
    //: What Undo undoes in a mind map. In Polish: przesunięcie węzła
    return change(tr("moving a node"), [&ids, &delta](MindMap& map) {
        bool moved = false;
        for (const QString& id : ids) {
            const MindMapNode* node = map.node(id);
            if (node != nullptr) {
                moved = map.setPlace(id, node->place.value_or(QPointF()) + delta) || moved;
            }
        }
        return moved;
    });
}

bool MindMapDocument::setPlaces(const QHash<QString, QPointF>& places, const QString& what) {
    return change(what, [&places](MindMap& map) {
        for (auto it = places.cbegin(); it != places.cend(); ++it) {
            map.setPlace(it.key(), it.value());
        }
        return true;
    });
}

bool MindMapDocument::toggleFolded(const QStringList& ids) {
    const bool anyFolded = std::any_of(ids.cbegin(), ids.cend(), [this](const QString& id) {
        const MindMapNode* node = m_map.node(id);
        return node != nullptr && node->folded;
    });
    return change(anyFolded
                      //: What Undo undoes in a mind map. In Polish: rozwinięcie węzła
                      ? tr("unfolding a node")
                      //: What Undo undoes in a mind map. In Polish: zwinięcie węzła
                      : tr("folding a node"),
                  [&ids, anyFolded](MindMap& map) {
                      for (const QString& id : ids) {
                          if (const MindMapNode* node = map.node(id)) {
                              MindMapNode changed = *node;
                              changed.folded = !anyFolded;
                              map.setNode(changed);
                          }
                      }
                      return true;
                  });
}

bool MindMapDocument::joinNodes(const QString& from, const QString& to) {
    if (m_map.node(from) == nullptr || m_map.node(to) == nullptr || from == to) {
        return false;
    }
    // A loose node joins the tree as a branch of the node; a node of a branch, or one above the
    // node, is joined by a connection
    const bool branch = to != m_map.root() && m_map.parentOf(to).isEmpty() &&
                        !m_map.descendantsOf(to).contains(from);
    if (branch) {
        //: What Undo undoes in a mind map. In Polish: połączenie węzłów
        return change(tr("connecting nodes"), [&](MindMap& map) {
            const std::optional<QPointF> place = map.node(to)->place;
            if (!map.move(to, from, -1)) {
                return false;
            }
            if (place) {
                map.setPlace(to, *place);
            }
            return true;
        });
    }
    return connectNodes(from, to);
}

bool MindMapDocument::detachBranch(const QString& id) {
    if (m_map.parentOf(id).isEmpty()) {
        return false;
    }
    //: What Undo undoes in a mind map. In Polish: odłączenie węzła od gałęzi
    return change(tr("detaching a node"), [&id](MindMap& map) {
        const std::optional<QPointF> place = map.node(id)->place;
        if (!map.makeLoose(id, QPointF())) {
            return false;
        }
        MindMapNode changed = *map.node(id);
        changed.loosePlace.reset();
        changed.place = place;
        return map.setNode(changed);
    });
}

QString MindMapDocument::addFrame(const QStringList& nodes, const QString& title) {
    QString added;
    //: What Undo undoes in a mind map. In Polish: dodanie ramki
    change(tr("adding a frame"), [&](MindMap& map) {
        added = map.addFrame(title, nodes);
        return !added.isEmpty();
    });
    return added;
}

bool MindMapDocument::setFrame(const MindMapFrame& frame) {
    //: What Undo undoes in a mind map. In Polish: zmiana ramki
    return change(tr("changing a frame"), [&frame](MindMap& map) { return map.setFrame(frame); });
}

bool MindMapDocument::removeFrame(const QString& id) {
    //: What Undo undoes in a mind map. In Polish: usunięcie ramki
    return change(tr("deleting a frame"), [&id](MindMap& map) { return map.removeFrame(id); });
}

bool MindMapDocument::setNode(const MindMapNode& node, const QString& what) {
    return change(what, [&node](MindMap& map) { return map.setNode(node); });
}

void MindMapDocument::save() {
    QString error;
    m_saveFailed = !m_map.save(m_path, &error);
    if (m_saveFailed) {
        core::Logger::getInstance().error("MindMapDocument: {} cannot be written: {}",
                                          m_path.toStdString(), error.toStdString());
        emit saveError(error);
    }
}

}  // namespace kalahari::mindmap
