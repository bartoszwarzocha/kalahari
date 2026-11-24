// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_view_commands.cpp
/// @brief The commands of the map of nodes: adding, joining, folding, framing, arranging and
/// deleting, with their messages; the menus of what is selected

#include <kalahari/core/art_provider.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/gui/dialogs/relation_dialog.h>
#include <kalahari/gui/kind_names.h>
#include <kalahari/mindmap/mind_map_content.h>
#include <kalahari/mindmap/mind_map_document.h>
#include <kalahari/mindmap/mind_map_layout.h>
#include <kalahari/mindmap/mind_map_menus.h>
#include <kalahari/mindmap/mind_map_picker.h>
#include <kalahari/mindmap/mind_map_view.h>

#include <QAction>
#include <QMenu>

#include <algorithm>
#include <array>
#include <map>
#include <memory>

namespace kalahari::mindmap {

namespace {

/// The room a new node takes, until it is drawn
constexpr double NEW_HEIGHT = NodeMetrics::HEADER + 40.0;
/// Nodes stand at least this far apart, when a new one looks for its place
constexpr double FREE_ROOM = 14.0;
/// Arrange: the gaps of the layouts, times this, as the nodes are boxes
constexpr double ARRANGE_GAPS = 2.0;
/// The generations of a family: the columns this far apart
constexpr double GENERATION_GAP = 110.0;
constexpr int MOST_ROUNDS = 64;
constexpr qsizetype MOST_IN_COLUMN = 5;  ///< The places and items in a column of generations

/// A shortcut shown beside an action of the menu; the keys themselves are the map's
void showKeys(QAction* action, const QKeySequence& keys) {
    action->setShortcut(keys);
    action->setShortcutContext(Qt::WidgetShortcut);
    action->setShortcutVisibleInContextMenu(true);
}

QAction* menuAction(QMenu& menu, const char* icon, const QString& text,
                    const QKeySequence& keys = QKeySequence()) {
    QAction* action = icon != nullptr ? menu.addAction(core::ArtProvider::getInstance().getIcon(
                                                           QString::fromLatin1(icon)),
                                                       text)
                                      : menu.addAction(text);
    if (!keys.isEmpty()) {
        showKeys(action, keys);
    }
    return action;
}

/// The order of the kinds of cards whose relation a link saves: a character before an item,
/// an item before a place
int rankOf(const QString& kind) {
    if (kind.endsWith(QLatin1String(":character"))) {
        return 0;
    }
    if (kind.endsWith(QLatin1String(":item"))) {
        return 1;
    }
    if (kind.endsWith(QLatin1String(":location"))) {
        return 2;
    }
    return 3;
}

/// The relation of @p after that @p before did not have
std::optional<core::ShownRelation> newOf(const QList<core::ShownRelation>& before,
                                         const QList<core::ShownRelation>& after) {
    for (const core::ShownRelation& relation : after) {
        if (!before.contains(relation)) {
            return relation;
        }
    }
    return std::nullopt;
}

/// The heading of the relations of the sockets of @p kind, in the list of their choices
QString relationsTitle(SocketKind kind) {
    switch (kind) {
    case SocketKind::Family:
        //: The heading of the relations of a family in the list over a mind map. In Polish:
        //: Rodzina
        return QCoreApplication::translate("kalahari::mindmap::MindMapView", "Family");
    case SocketKind::Places:
        //: In Polish: Miejsca
        return QCoreApplication::translate("kalahari::mindmap::MindMapView", "Places");
    case SocketKind::Items:
        //: In Polish: Przedmioty
        return QCoreApplication::translate("kalahari::mindmap::MindMapView", "Items");
    case SocketKind::Branch:
    case SocketKind::Relations:
        break;
    }
    //: In Polish: Relacje
    return QCoreApplication::translate("kalahari::mindmap::MindMapView", "Relations");
}

}  // namespace

// =============================================================================
// Where new nodes go
// =============================================================================

QPointF MindMapView::freePlace(const QPointF& wanted, double height, bool up) const {
    QPointF at = wanted;
    if (!m_scene) {
        return at;
    }
    for (int guard = 0; guard < 2 * MOST_ROUNDS; ++guard) {
        const auto hit = std::find_if(m_scene->nodes().cbegin(), m_scene->nodes().cend(),
                                      [&at, height](const NodeBox& box) {
                                          return std::abs(box.rect.left() - at.x()) <
                                                     NodeMetrics::WIDTH &&
                                                 at.y() < box.rect.bottom() + FREE_ROOM &&
                                                 at.y() + height + FREE_ROOM > box.rect.top();
                                      });
        if (hit == m_scene->nodes().cend()) {
            break;
        }
        at.setY(up ? hit->rect.top() - height - NodeMetrics::GAP_ALONG
                   : hit->rect.bottom() + NodeMetrics::GAP_ALONG);
    }
    return at;
}

QPointF MindMapView::newPlace() const {
    return freePlace(toMap(QPointF(width() / 2.0, height() / 2.0)) -
                         QPointF(NodeMetrics::WIDTH / 2.0, NodeMetrics::HEADER / 2.0),
                     NEW_HEIGHT, false);
}

QPointF MindMapView::placeRightOf(const QString& id) const {
    const NodeBox* box = m_scene ? m_scene->box(id) : nullptr;
    const MindMapNode* node = m_document.map().node(id);
    if (box == nullptr || node == nullptr) {
        return newPlace();
    }
    // A node left of the node it leaves has its branches on the left
    const NodeBox* parent = m_scene->box(m_document.map().parentOf(id));
    const bool left = parent != nullptr && box->rect.right() < parent->rect.left();
    const double x = left ? box->rect.left() - NodeMetrics::GAP_ACROSS - NodeMetrics::WIDTH
                          : box->rect.right() + NodeMetrics::GAP_ACROSS;
    double y = box->rect.top();
    bool first = true;
    for (const QString& child : node->children) {
        if (const NodeBox* below = m_scene->box(child)) {
            y = first ? below->rect.bottom() + NodeMetrics::GAP_ALONG
                      : std::max(y, below->rect.bottom() + NodeMetrics::GAP_ALONG);
            first = false;
        }
    }
    return freePlace(QPointF(x, y), NEW_HEIGHT, false);
}

QPointF MindMapView::placeBelow(const QString& id, bool above) const {
    const NodeBox* box = m_scene ? m_scene->box(id) : nullptr;
    if (box == nullptr) {
        return newPlace();
    }
    const double y = above ? box->rect.top() - NEW_HEIGHT - NodeMetrics::GAP_ALONG
                           : box->rect.bottom() + NodeMetrics::GAP_ALONG;
    return freePlace(QPointF(box->rect.left(), y), NEW_HEIGHT, above);
}

// =============================================================================
// Adding nodes
// =============================================================================

void MindMapView::addBranch() {
    finishEditing(true);
    finishAnimations();
    // Nothing selected: a branch of the main topic; on a map of relations, which does not
    // draw it, a loose node
    QString from = m_document.selected();
    if (from.isEmpty() && m_document.map().showsRoot()) {
        from = m_document.map().root();
    }
    //: The title of a new node of a mind map, which the writer changes at once. In Polish:
    //: Nowy pomysł
    const QString added = m_document.addNode(QString::fromLatin1(MindMapNode::IDEA_KIND),
                                             tr("New idea"),
                                             from.isEmpty() ? newPlace() : placeRightOf(from),
                                             from);
    if (added.isEmpty()) {
        return;
    }
    emit told(from.isEmpty()
                  //: In the bar under a mind map. In Polish: Dodano węzeł.
                  ? tr("Added a node.")
                  //: In Polish: Dodano gałąź: nowy węzeł po prawej, połączony z wybranym.
                  : tr("Added a branch: a new node on the right, joined with the one selected."),
              true);
    startEditing(added);
}

void MindMapView::addBelow(bool above) {
    finishEditing(true);
    finishAnimations();
    const MindMap& map = m_document.map();
    const QString id = m_document.selected();
    if (id.isEmpty() || id == map.root()) {
        addBranch();
        return;
    }
    const bool branch = !map.parentOf(id).isEmpty();
    const QString added = m_document.addSiblingAt(id, placeBelow(id, above), above, tr("New idea"));
    if (added.isEmpty()) {
        return;
    }
    if (above) {
        emit told(branch
                      //: In the bar under a mind map. In Polish: Dodano węzeł powyżej, z tej
                      //: samej gałęzi.
                      ? tr("Added a node above, of the same branch.")
                      //: In Polish: Dodano węzeł powyżej.
                      : tr("Added a node above."),
                  true);
    } else {
        emit told(branch
                      //: In Polish: Dodano węzeł poniżej, z tej samej gałęzi.
                      ? tr("Added a node below, of the same branch.")
                      //: In Polish: Dodano węzeł poniżej.
                      : tr("Added a node below."),
                  true);
    }
    startEditing(added);
}

void MindMapView::openAddNode(const std::optional<QPointF>& mapPoint, const QString& from,
                              const QPoint& globalPoint) {
    finishEditing(true);
    finishAnimations();
    QString parent = from;
    QPointF place;
    if (mapPoint) {
        place = *mapPoint;
    } else if (!m_document.selected().isEmpty()) {
        parent = m_document.selected();
        place = placeRightOf(parent);
    } else {
        place = m_menuPoint.value_or(newPlace());
    }
    // The kinds of the writer's nodes, then what the book has
    const QStringList kinds{QString::fromLatin1(MindMapNode::IDEA_KIND),
                            QString::fromLatin1(MindMapNode::QUESTION_KIND),
                            QString::fromLatin1(MindMapNode::NOTE_KIND)};
    const std::array<NodeColor, 3> colors{NodeColor::Idea, NodeColor::Question, NodeColor::Note};
    QList<PickItem> items;
    for (qsizetype i = 0; i < kinds.size(); ++i) {
        //: The heading of the kinds of the new nodes in Add Node over a mind map. In Polish:
        //: Nowy
        items.append(PickItem{MindMapContent::ownKindName(kinds.at(i)), tr("New"),
                              m_look.kindColor(colors.at(static_cast<std::size_t>(i)))});
    }
    const QList<CatalogEntry> catalog = m_content.catalog();
    for (const CatalogEntry& entry : catalog) {
        items.append(PickItem{entry.title, entry.group, m_look.kindColor(entry.color)});
    }
    //: The small window over a mind map that adds a node. In Polish: Dodaj węzeł
    auto* picker = new MindMapPicker(tr("Add Node"), items, this);
    connect(picker, &MindMapPicker::picked, this, [this, kinds, catalog, place, parent](int index) {
        if (index < kinds.size()) {
            addOwnAt(kinds.at(index), place, parent);
        } else if (index - kinds.size() < catalog.size()) {
            addEntryAt(catalog.at(index - kinds.size()), place, parent);
        }
    });
    QPoint at = globalPoint;
    if (at.isNull()) {
        at = mapToGlobal(toView(place).toPoint());
    }
    picker->popUp(at);
}

void MindMapView::addOwnAt(const QString& kind, const QPointF& place, const QString& from) {
    const QString added = m_document.addNode(kind, MindMapContent::ownKindName(kind), place, from);
    if (added.isEmpty()) {
        return;
    }
    if (from.isEmpty()) {
        //: In the bar under a mind map. In Polish: Dodano węzeł.
        emit told(tr("Added a node."), true);
    } else {
        //: In Polish: Dodano węzeł połączony z „%1”.
        emit told(tr("Added a node joined with “%1”.").arg(titleOf(from)), true);
    }
    startEditing(added);
}

void MindMapView::addEntryAt(const CatalogEntry& entry, const QPointF& place, const QString& from) {
    if (entry.kind == QLatin1String(MindMapNode::ELEMENT_KIND)) {
        if (const QString there = m_document.map().nodeOfElement(entry.elementId);
            !there.isEmpty()) {
            m_document.select(there);
            //: In the bar under a mind map. In Polish: „%1” już jest na mapie.
            emit told(tr("“%1” is already on the map.").arg(entry.title), false);
            return;
        }
    }
    const qsizetype before = m_scene ? m_scene->relations().size() : 0;
    const QString added = m_document.addEntry(entry, place, from);
    if (added.isEmpty()) {
        return;
    }
    const qsizetype relations = (m_scene ? m_scene->relations().size() : 0) - before;
    if (relations > 0) {
        //: In the bar under a mind map. In Polish: Dodano „%1” z relacjami do kart na mapie:
        //: %2.
        emit told(tr("Added “%1” with its relations to the cards on the map: %2.")
                      .arg(entry.title)
                      .arg(relations),
                  true);
    } else {
        //: In Polish: Dodano „%1”.
        emit told(tr("Added “%1”.").arg(entry.title), true);
    }
}

void MindMapView::addCards(const QStringList& elementIds, const QPointF& place) {
    if (m_book == nullptr) {
        return;
    }
    QPointF at = place;
    QString already;
    int added = 0;
    QString last;
    for (const QString& id : elementIds) {
        const core::ProjectElement* element = m_book->findElement(id);
        if (element == nullptr) {
            continue;
        }
        if (const QString there = m_document.map().nodeOfElement(id); !there.isEmpty()) {
            already = there;
            continue;
        }
        CatalogEntry entry;
        entry.kind = QString::fromLatin1(MindMapNode::ELEMENT_KIND);
        entry.elementId = id;
        entry.title = element->title;
        entry.color = m_content.colorOf(*element);
        if (added == 0 && elementIds.size() == 1) {
            addEntryAt(entry, freePlace(at, NEW_HEIGHT, false), QString());
            return;
        }
        last = m_document.addEntry(entry, freePlace(at, NEW_HEIGHT, false), QString());
        if (!last.isEmpty()) {
            ++added;
            if (const NodeBox* box = m_scene ? m_scene->box(last) : nullptr) {
                at = QPointF(at.x(), box->rect.bottom() + NodeMetrics::GAP_ALONG);
            }
        }
    }
    if (added > 0) {
        //: In the bar under a mind map, after cards dragged from the Navigator. In Polish:
        //: Dodano elementy: %1.
        emit told(tr("Added elements: %1.").arg(added), true);
    } else if (!already.isEmpty()) {
        m_document.select(already);
        emit told(tr("“%1” is already on the map.").arg(titleOf(already)), false);
    }
}

void MindMapView::copySelected() {
    if (const QString message = MindMapMenus::copy(m_document, m_document.selected());
        !message.isEmpty()) {
        emit told(message, false);
    }
}

void MindMapView::paste() {
    const QString parent = m_document.selected().isEmpty() ? m_document.map().root()
                                                           : m_document.selected();
    QString message;
    MindMapMenus::paste(m_document, parent, &message);
    if (!message.isEmpty()) {
        emit told(message, true);
    }
}

// =============================================================================
// Joining nodes, the relations of the cards
// =============================================================================

void MindMapView::joinSockets(const SocketBox& from, const SocketBox& to,
                              const QPoint& globalPoint) {
    const std::optional<SocketKind> kind = MindMapScene::joinable(from, to);
    const MindMap& map = m_document.map();
    if (!kind) {
        return;
    }
    if (*kind == SocketKind::Branch) {
        const SocketBox& output = from.right ? from : to;
        const SocketBox& input = from.right ? to : from;
        const QString parent = map.parentOf(input.node);
        bool joined = false;
        if (!from.right && !parent.isEmpty() && parent != output.node &&
            !map.descendantsOf(input.node).contains(output.node)) {
            // The input of a branch dragged to another output: the branch goes there
            joined = m_document.moveNode(input.node, output.node, -1);
        } else {
            joined = m_document.joinNodes(output.node, input.node);
        }
        if (joined) {
            //: In the bar under a mind map. In Polish: Połączono „%1” z „%2”.
            emit told(tr("Connected “%1” with “%2”.").arg(titleOf(output.node), titleOf(input.node)),
                      true);
        }
        return;
    }
    // Two cards: the relation is saved on both; the card of a character has it, before an
    // item, before a place
    const MindMapNode* a = map.node(from.node);
    const MindMapNode* b = map.node(to.node);
    if (a == nullptr || b == nullptr || m_book == nullptr) {
        return;
    }
    QString cardId = a->link;
    QString otherId = b->link;
    if (rankOf(to.cardKind) < rankOf(from.cardKind)) {
        std::swap(cardId, otherId);
    }
    const QList<core::RelationInfo> choices = m_content.relationChoices(cardId, otherId, *kind);
    QList<PickItem> items;
    const QString language = gui::programLanguage();
    for (const core::RelationInfo& choice : choices) {
        items.append(PickItem{choice.choiceText(language), relationsTitle(*kind), QColor()});
    }
    //: The last item of the relations over a mind map, which opens the window Relation. In
    //: Polish: Inna relacja…
    items.append(PickItem{tr("Other relation…"), QString(), QColor()});
    //: The small window over a mind map that picks the relation of two cards. In Polish:
    //: Relacja
    auto* picker = new MindMapPicker(tr("Relation"), items, this);
    connect(picker, &MindMapPicker::picked, this, [this, choices, cardId, otherId](int index) {
        if (index < choices.size()) {
            addRelation(cardId, otherId, choices.at(index).id);
        } else {
            addOtherRelation(cardId, otherId);
        }
    });
    picker->popUp(globalPoint.isNull() ? mapToGlobal(rect().center()) : globalPoint);
}

void MindMapView::addRelation(const QString& cardId, const QString& otherId,
                              const QString& relationId) {
    core::ProjectManager* book = m_book;
    if (book == nullptr) {
        return;
    }
    auto added = std::make_shared<std::optional<core::ShownRelation>>();
    //: What Undo undoes in a mind map. In Polish: dodanie relacji
    const bool saved = m_document.changeWithBook(
        tr("adding a relation"), nullptr,
        [book, cardId, otherId, relationId, added]() {
            const QList<core::ShownRelation> before = book->cardRelations().of(cardId);
            if (!book->addRelation(cardId, otherId, relationId, QString())) {
                return false;
            }
            *added = newOf(before, book->cardRelations().of(cardId));
            return true;
        },
        [book, added]() { return !*added || book->removeRelation(**added); });
    if (!saved) {
        //: In the bar under a mind map. In Polish: Nie udało się zapisać relacji.
        emit told(tr("The relation could not be saved."), false);
        return;
    }
    const core::ProjectElement* card = book->findElement(cardId);
    const core::ProjectElement* other = book->findElement(otherId);
    //: In the bar under a mind map. In Polish: Relacja zapisana na kartach: %1 i %2.
    emit told(tr("The relation is saved on the cards: %1 and %2.")
                  .arg(card != nullptr ? card->title : QString(),
                       other != nullptr ? other->title : QString()),
              true);
}

void MindMapView::addOtherRelation(const QString& cardId, const QString& otherId) {
    core::ProjectManager* book = m_book;
    if (book == nullptr) {
        return;
    }
    gui::dialogs::RelationDialog dialog(*book, cardId, std::nullopt, this);
    dialog.setCurrentCard(otherId);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const gui::dialogs::RelationChoice choice = dialog.choice();
    auto added = std::make_shared<std::optional<core::ShownRelation>>();
    auto first = std::make_shared<bool>(true);
    const QPointer<QWidget> parent(this);
    const bool saved = m_document.changeWithBook(
        tr("adding a relation"), nullptr,
        [book, cardId, choice, added, first, parent]() {
            if (!*first) {
                // Redo: the relation saved the first time
                return *added && book->addRelation((*added)->ownerId, (*added)->linkTo,
                                                   (*added)->linkRole, (*added)->note);
            }
            *first = false;
            const QList<core::ShownRelation> before = book->cardRelations().of(cardId);
            if (gui::dialogs::saveRelation(*book, cardId, choice, std::nullopt, parent.data())
                    .isEmpty()) {
                return false;
            }
            *added = newOf(before, book->cardRelations().of(cardId));
            return true;
        },
        [book, added]() { return !*added || book->removeRelation(**added); });
    if (saved) {
        //: In the bar under a mind map. In Polish: Relacja zapisana na obu kartach.
        emit told(tr("The relation is saved on both cards."), true);
    }
}

void MindMapView::changeRelation(const core::ShownRelation& relation) {
    core::ProjectManager* book = m_book;
    if (book == nullptr) {
        return;
    }
    // The card whose node the link leaves shows it so
    const QString cardId = relation.ownerId == relation.linkTo ? relation.otherId
                                                               : relation.ownerId;
    const QString shownBy = book->cardRelations().of(cardId).contains(relation) ? cardId
                                                                                : relation.otherId;
    gui::dialogs::RelationDialog dialog(*book, shownBy, relation, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const gui::dialogs::RelationChoice choice = dialog.choice();
    auto added = std::make_shared<std::optional<core::ShownRelation>>();
    auto first = std::make_shared<bool>(true);
    const QPointer<QWidget> parent(this);
    //: What Undo undoes in a mind map. In Polish: zmiana relacji
    const bool saved = m_document.changeWithBook(
        tr("changing a relation"), nullptr,
        [book, shownBy, choice, relation, added, first, parent]() {
            if (!*first) {
                return *added && book->removeRelation(relation) &&
                       book->addRelation((*added)->ownerId, (*added)->linkTo, (*added)->linkRole,
                                         (*added)->note);
            }
            *first = false;
            const QList<core::ShownRelation> before = book->cardRelations().of(shownBy);
            if (gui::dialogs::saveRelation(*book, shownBy, choice, relation, parent.data())
                    .isEmpty()) {
                return false;
            }
            *added = newOf(before, book->cardRelations().of(shownBy));
            return true;
        },
        [book, relation, added]() {
            if (!*added) {
                return true;
            }
            return book->removeRelation(**added) &&
                   book->addRelation(relation.ownerId, relation.linkTo, relation.linkRole,
                                     relation.note);
        });
    if (saved) {
        //: In the bar under a mind map. In Polish: Zmieniono relację na obu kartach.
        emit told(tr("The relation is changed on both cards."), true);
    }
}

void MindMapView::removeRelation(const core::ShownRelation& relation) {
    core::ProjectManager* book = m_book;
    if (book == nullptr) {
        return;
    }
    //: What Undo undoes in a mind map. In Polish: usunięcie relacji
    const bool removed = m_document.changeWithBook(
        tr("deleting a relation"), nullptr,
        [book, relation]() { return book->removeRelation(relation); },
        [book, relation]() {
            return book->addRelation(relation.ownerId, relation.linkTo, relation.linkRole,
                                     relation.note);
        });
    if (removed) {
        selectLink(std::nullopt);
        //: In the bar under a mind map. In Polish: Usunięto relację z obu kart.
        emit told(tr("The relation is deleted from both cards."), true);
    }
}

void MindMapView::connectTo() {
    finishEditing(true);
    const QString from = m_document.selected();
    if (from.isEmpty() || !m_scene) {
        return;
    }
    QStringList targets;
    QList<PickItem> items;
    for (const NodeBox& box : m_scene->nodes()) {
        if (box.id != from) {
            targets.append(box.id);
            items.append(
                PickItem{box.content.title, QString(), m_look.kindColor(box.content.color)});
        }
    }
    //: The small window over a mind map that connects the node selected with another. In
    //: Polish: Połącz z
    auto* picker = new MindMapPicker(tr("Connect To"), items, this);
    connect(picker, &MindMapPicker::picked, this, [this, from, targets](int index) {
        if (index < 0 || index >= targets.size()) {
            return;
        }
        const QString to = targets.at(index);
        if (m_document.joinNodes(from, to)) {
            emit told(tr("Connected “%1” with “%2”.").arg(titleOf(from), titleOf(to)), true);
        }
    });
    const NodeBox* box = m_scene->box(from);
    picker->popUp(box != nullptr ? mapToGlobal(toView(box->rect.bottomLeft()).toPoint())
                                 : mapToGlobal(rect().center()));
}

// =============================================================================
// Folding, frames, deleting, opening
// =============================================================================

void MindMapView::foldSelected() {
    if (!m_document.selection().isEmpty()) {
        m_document.toggleFolded(m_document.selection());
    }
}

void MindMapView::frameSelected() {
    finishEditing(true);
    const QStringList nodes = m_document.selection();
    if (nodes.isEmpty()) {
        //: In the bar under a mind map. In Polish: Wybierz węzły, które ma objąć ramka.
        emit told(tr("Select the nodes to put in a frame."), false);
        return;
    }
    //: The title of a new frame of a mind map. In Polish: Nowa ramka
    const QString added = m_document.addFrame(nodes, tr("New frame"));
    if (added.isEmpty()) {
        return;
    }
    //: In the bar under a mind map. In Polish: Dodano ramkę. Przeciągnij jej tytuł, aby
    //: przesunąć ją razem z węzłami.
    emit told(tr("Added a frame. Drag its title to move it with its nodes."), true);
    startEditingFrame(added);
}

void MindMapView::removeSelected() {
    finishEditing(true);
    finishAnimations();
    const MindMap& map = m_document.map();
    if (m_link) {
        const LinkRef link = *m_link;
        bool removed = false;
        switch (link.type) {
        case LinkLine::Type::Branch:
            removed = m_document.detachBranch(link.to);
            if (removed) {
                //: In the bar under a mind map. In Polish: Odłączono gałąź; „%1” zostaje na
                //: mapie.
                emit told(tr("The branch is detached; “%1” stays on the map.").arg(titleOf(link.to)),
                          true);
            }
            break;
        case LinkLine::Type::Connection:
            removed = m_document.removeConnection(link.connection);
            if (removed) {
                //: In Polish: Usunięto połączenie.
                emit told(tr("The connection is deleted."), true);
            }
            break;
        case LinkLine::Type::Relation:
            removeRelation(link.relation);
            return;
        }
        if (removed) {
            selectLink(std::nullopt);
        }
        return;
    }
    if (!m_frame.isEmpty()) {
        if (m_document.removeFrame(m_frame)) {
            selectFrame(QString());
            //: In the bar under a mind map. In Polish: Usunięto ramkę; węzły zostają.
            emit told(tr("The frame is deleted; its nodes stay."), true);
        }
        return;
    }
    QStringList ids = m_document.selection();
    ids.removeAll(map.root());
    if (ids.isEmpty()) {
        if (!m_document.selection().isEmpty()) {
            //: In Polish: Temat główny zostaje: to tytuł mapy. Usuń jego gałęzie albo całą
            //: mapę w Nawigatorze.
            emit told(tr("The main topic stays: it is the title of the map. Delete its "
                         "branches, or the whole map in the Navigator."),
                      false);
        }
        return;
    }
    const QString title = titleOf(ids.first());
    const MindMapNode* first = map.node(ids.first());
    const bool card = first != nullptr && m_scene != nullptr &&
                      m_scene->box(first->id) != nullptr &&
                      !m_scene->box(first->id)->content.cardKind.isEmpty();
    const bool branches = std::any_of(ids.cbegin(), ids.cend(), [&map](const QString& id) {
        const MindMapNode* node = map.node(id);
        return node != nullptr && !node->children.isEmpty();
    });
    const int removed = m_document.removeNodes(ids);
    if (removed == 0) {
        return;
    }
    QString message;
    if (removed > 1) {
        //: In the bar under a mind map. In Polish: Usunięto węzły z mapy: %1.
        message = tr("Deleted nodes from the map: %1.").arg(removed);
    } else if (card) {
        //: In Polish: Usunięto „%1” z mapy; karta zostaje w Warsztacie.
        message = tr("Deleted “%1” from the map; the card stays in the Workshop.").arg(title);
    } else {
        //: In Polish: Usunięto „%1” z mapy.
        message = tr("Deleted “%1” from the map.").arg(title);
    }
    if (branches) {
        //: Added to the message of a deleted node. In Polish: Węzły jego gałęzi zostają.
        message += QLatin1Char(' ') + tr("The nodes of its branches stay.");
    }
    emit told(message, true);
}

void MindMapView::openSelected() {
    const NodeBox* box = m_scene ? m_scene->box(m_document.selected()) : nullptr;
    if (box == nullptr) {
        return;
    }
    if (box->content.elementId.isEmpty() || box->content.missing) {
        //: In Polish: Ten węzeł nie jest o żadnym elemencie książki.
        emit told(tr("This node is not about an element of the book."), false);
        return;
    }
    emit openElementRequested(box->content.elementId);
}

void MindMapView::moveSelectedBy(const QPointF& delta) {
    if (!m_document.selection().isEmpty()) {
        m_document.moveNodesBy(m_document.selection(), delta);
    }
}

void MindMapView::tellUndone(bool redo) {
    finishEditing(true);
    finishAnimations();
    const QString what = redo ? m_document.redoText() : m_document.undoText();
    if (what.isEmpty()) {
        return;
    }
    if (redo) {
        m_document.redo();
        //: In the bar under a mind map; %1 is what was done again: "dodanie węzła". In Polish:
        //: Ponowiono: %1.
        emit told(tr("Redone: %1.").arg(what), false);
    } else {
        m_document.undo();
        //: In the bar under a mind map; %1 is what was undone: "dodanie węzła". In Polish:
        //: Cofnięto: %1.
        emit told(tr("Undone: %1.").arg(what), false);
    }
}

// =============================================================================
// Arranging
// =============================================================================

void MindMapView::arrange(MindMapLayout layout) {
    finishEditing(true);
    finishAnimations();
    const MindMap& map = m_document.map();
    const NodeBox* root = m_scene ? m_scene->box(map.root()) : nullptr;
    if (root == nullptr) {
        return;
    }
    MindMapLayout used = layout;
    if (used == MindMapLayout::Free) {
        used = map.layout() == MindMapLayout::Free ? MindMapLayout::Right : map.layout();
    }
    MindMapLayoutOptions options;
    options.everyNode = true;
    options.gaps = ARRANGE_GAPS;
    options.stackLoose = true;
    const QHash<QString, MindMapPlace> laid = layOutMindMap(
        map, used,
        [this](const MindMapNode& node, int /*depth*/) { return m_scene->sizeOf(node.id); },
        options);
    if (!laid.contains(map.root())) {
        return;
    }
    // The main topic stays where it is
    const QSizeF rootSize = root->rect.size();
    const QPointF shift = root->rect.topLeft() -
                          (laid.value(map.root()).centre -
                           QPointF(rootSize.width(), rootSize.height()) / 2.0);
    QHash<QString, QPointF> places;
    for (auto it = laid.cbegin(); it != laid.cend(); ++it) {
        const QSizeF size = m_scene->sizeOf(it.key());
        places.insert(it.key(), it->centre - QPointF(size.width(), size.height()) / 2.0 + shift);
    }
    //: What Undo undoes in a mind map. In Polish: uporządkowanie mapy
    animateTo(places, tr("arranging the map"), used);
}

bool MindMapView::hasFamilies() const {
    if (!m_scene) {
        return false;
    }
    return std::count_if(m_scene->nodes().cbegin(), m_scene->nodes().cend(), [](const NodeBox& box) {
               return box.content.cardKind.endsWith(QLatin1String(":character"));
           }) >= 2;
}

void MindMapView::arrangeGenerations() {
    finishEditing(true);
    finishAnimations();
    if (!m_scene || m_book == nullptr) {
        return;
    }
    const MindMap& map = m_document.map();
    QRectF anchor;
    if (const NodeBox* root = m_scene->box(map.root()); root != nullptr) {
        anchor = root->rect;
    } else {
        // The main topic is not drawn: the cards from where they begin now
        QRectF cards;
        for (const NodeBox& box : m_scene->nodes()) {
            cards = cards.united(box.rect);
        }
        anchor = QRectF(QPointF(cards.left() - GENERATION_GAP, cards.center().y()), QSizeF());
    }
    QHash<QString, QPointF> places = generationPlaces(anchor);
    if (m_scene->box(map.root()) != nullptr) {
        places.insert(map.root(), anchor.topLeft());
    }
    animateTo(places, tr("arranging the map"), std::nullopt);
}

QHash<QString, QPointF> MindMapView::generationPlaces(const QRectF& anchor) const {
    const MindMap& map = m_document.map();
    QHash<QString, QPointF> places;
    if (!m_scene || m_book == nullptr) {
        return places;
    }
    // The characters: a parent one generation before its child; spouses, partners and
    // siblings in the same generation
    QStringList characters;
    QHash<QString, QString> nodeOfCard;
    for (const NodeBox& box : m_scene->nodes()) {
        if (box.content.cardKind.endsWith(QLatin1String(":character")) &&
            !box.content.elementId.isEmpty()) {
            characters.append(box.id);
            nodeOfCard.insert(box.content.elementId, box.id);
        }
    }
    QList<std::pair<QString, QString>> parents;  // Parent, child
    QList<std::pair<QString, QString>> same;
    const core::CardRelations relations = m_book->cardRelations();
    for (const QString& id : std::as_const(characters)) {
        const QString card = m_scene->box(id)->content.elementId;
        for (const core::ShownRelation& relation : relations.of(card)) {
            const QString other = nodeOfCard.value(relation.otherId);
            if (other.isEmpty()) {
                continue;
            }
            if (relation.relation == QLatin1String("parent")) {
                parents.append({other, id});
            } else if (relation.relation == QLatin1String("child")) {
                parents.append({id, other});
            } else if (relation.relation == QLatin1String("spouse") ||
                       relation.relation == QLatin1String("partner") ||
                       relation.relation == QLatin1String("sibling")) {
                same.append({id, other});
            }
        }
    }
    QHash<QString, int> generation;
    for (const QString& id : std::as_const(characters)) {
        generation.insert(id, 0);
    }
    for (int round = 0; round < MOST_ROUNDS; ++round) {
        bool changed = false;
        for (const auto& [parent, child] : std::as_const(parents)) {
            if (generation.value(child) < generation.value(parent) + 1) {
                generation[child] = generation.value(parent) + 1;
                changed = true;
            }
        }
        for (const auto& [a, b] : std::as_const(same)) {
            const int both = std::max(generation.value(a), generation.value(b));
            if (generation.value(a) != both || generation.value(b) != both) {
                generation[a] = both;
                generation[b] = both;
                changed = true;
            }
        }
        if (!changed) {
            break;
        }
    }
    // The columns: the main topic, the generations, then the other cards by kind, then the
    // writer's nodes
    std::map<int, QStringList> columns;
    int last = 0;
    for (const QString& id : std::as_const(characters)) {
        columns[generation.value(id)].append(id);
        last = std::max(last, generation.value(id));
    }
    // The items and the places of the characters together, by kind, in columns of a few
    QStringList others;
    QStringList own;
    for (const NodeBox& box : m_scene->nodes()) {
        if (box.id == map.root() || characters.contains(box.id)) {
            continue;
        }
        if (!box.content.cardKind.isEmpty()) {
            others.append(box.id);
        } else {
            own.append(box.id);
        }
    }
    std::stable_sort(others.begin(), others.end(), [this](const QString& a, const QString& b) {
        return rankOf(m_scene->box(a)->content.cardKind) <
               rankOf(m_scene->box(b)->content.cardKind);
    });
    int next = last + 1;
    for (qsizetype first = 0; first < others.size(); first += MOST_IN_COLUMN) {
        columns[next++] = others.mid(first, MOST_IN_COLUMN);
    }
    if (!own.isEmpty()) {
        columns[next] = own;
    }
    // A child below the parents seen before it: each column in the order of their parents
    QHash<QString, double> order;
    for (auto& [column, ids] : columns) {
        std::stable_sort(ids.begin(), ids.end(),
                         [&order, &parents](const QString& a, const QString& b) {
                             const auto mean = [&order, &parents](const QString& id) {
                                 double sum = 0.0;
                                 int count = 0;
                                 for (const auto& [parent, child] : parents) {
                                     if (child == id && order.contains(parent)) {
                                         sum += order.value(parent);
                                         ++count;
                                     }
                                 }
                                 return count > 0 ? sum / count : 1e9;
                             };
                             return mean(a) < mean(b);
                         });
        for (qsizetype i = 0; i < ids.size(); ++i) {
            order.insert(ids.at(i), static_cast<double>(i));
        }
    }
    const double left = anchor.right() + GENERATION_GAP;
    double tallest = 0.0;
    QHash<int, double> heights;
    for (const auto& [column, ids] : columns) {
        double height = 0.0;
        for (const QString& id : ids) {
            height += m_scene->sizeOf(id).height() + NodeMetrics::GAP_ALONG * ARRANGE_GAPS;
        }
        heights.insert(column, height);
        tallest = std::max(tallest, height);
    }
    const double top = anchor.center().y() - tallest / 2.0;
    for (const auto& [column, ids] : columns) {
        double y = top + (tallest - heights.value(column)) / 2.0;
        const double x = left + column * (NodeMetrics::WIDTH + GENERATION_GAP);
        for (const QString& id : ids) {
            places.insert(id, QPointF(x, y));
            y += m_scene->sizeOf(id).height() + NodeMetrics::GAP_ALONG * ARRANGE_GAPS;
        }
    }
    return places;
}

QHash<QString, QPointF> MindMapView::gridPlaces(const QPointF& topLeft) const {
    // The cards by kind (characters, items, places), each kind in the order of the Workshop,
    // in rows of about as many as there are rows
    QHash<QString, QPointF> places;
    if (!m_scene) {
        return places;
    }
    QStringList ids;
    for (const NodeBox& box : m_scene->nodes()) {
        if (box.id != m_document.map().root()) {
            ids.append(box.id);
        }
    }
    std::stable_sort(ids.begin(), ids.end(), [this](const QString& a, const QString& b) {
        return rankOf(m_scene->box(a)->content.cardKind) <
               rankOf(m_scene->box(b)->content.cardKind);
    });
    const auto across = static_cast<qsizetype>(
        std::max(1.0, std::ceil(std::sqrt(static_cast<double>(ids.size())))));
    double y = topLeft.y();
    for (qsizetype first = 0; first < ids.size(); first += across) {
        double tallest = 0.0;
        for (qsizetype i = first; i < std::min(first + across, ids.size()); ++i) {
            const QString& id = ids.at(i);
            const double x = topLeft.x() +
                             static_cast<double>(i - first) * (NodeMetrics::WIDTH + GENERATION_GAP);
            places.insert(id, QPointF(x, y));
            tallest = std::max(tallest, m_scene->sizeOf(id).height());
        }
        y += tallest + NodeMetrics::GAP_ALONG * ARRANGE_GAPS * 2.0;
    }
    return places;
}

// =============================================================================
// The menus
// =============================================================================

void MindMapView::fillMenu(QMenu& menu) {
    if (m_link) {
        addMenuOfLink(menu, *m_link);
        return;
    }
    if (!m_frame.isEmpty()) {
        addMenuOfFrame(menu, m_frame);
        return;
    }
    if (!m_document.selection().isEmpty()) {
        addMenuOfNode(menu);
        return;
    }
    addMenuOfMap(menu);
}

void MindMapView::addMenuOfLink(QMenu& menu, const LinkRef& link) {
    switch (link.type) {
    case LinkLine::Type::Connection: {
        const qsizetype index = link.connection;
        //: In the menu of a connection of a mind map. In Polish: Podpis…
        connect(menuAction(menu, MindMapIcons::LABEL, tr("Label…"), QKeySequence(Qt::Key_F2)),
                &QAction::triggered, this, [this, index]() { startEditingLabel(index); });
        //: In Polish: Odwróć kierunek
        connect(menuAction(menu, MindMapIcons::REVERSE, tr("Reverse the direction")),
                &QAction::triggered, this, [this, index]() {
                    m_document.reverseConnection(index);
                    selectLink(std::nullopt);
                });
        menu.addSeparator();
        //: In Polish: Usuń połączenie
        connect(menuAction(menu, MindMapIcons::REMOVE, tr("Delete the connection"),
                           QKeySequence(Qt::Key_Delete)),
                &QAction::triggered, this, &MindMapView::removeSelected);
        return;
    }
    case LinkLine::Type::Relation: {
        const core::ShownRelation relation = link.relation;
        //: In the menu of a relation of two cards on a mind map. In Polish: Zmień relację…
        connect(menuAction(menu, MindMapIcons::RENAME, tr("Change the relation…")),
                &QAction::triggered, this, [this, relation]() { changeRelation(relation); });
        menu.addSeparator();
        //: In Polish: Usuń relację z kart
        connect(menuAction(menu, MindMapIcons::REMOVE, tr("Delete the relation from the cards"),
                           QKeySequence(Qt::Key_Delete)),
                &QAction::triggered, this, &MindMapView::removeSelected);
        return;
    }
    case LinkLine::Type::Branch:
        //: In the menu of a branch of a mind map. In Polish: Odłącz gałąź
        connect(menuAction(menu, MindMapIcons::REMOVE, tr("Detach the branch"),
                           QKeySequence(Qt::Key_Delete)),
                &QAction::triggered, this, &MindMapView::removeSelected);
        return;
    }
}

void MindMapView::addMenuOfFrame(QMenu& menu, const QString& frameId) {
    //: In the menu of a frame of a mind map. In Polish: Zmień tytuł
    connect(menuAction(menu, MindMapIcons::RENAME, tr("Change the title"), QKeySequence(Qt::Key_F2)),
            &QAction::triggered, this, [this, frameId]() { startEditingFrame(frameId); });
    //: In the menu of a frame of a mind map. In Polish: Kolor
    QMenu* colors = menu.addMenu(tr("Color"));
    const MindMapFrame* frame = m_document.map().frame(frameId);
    const QString now = frame != nullptr ? frame->color : QString();
    //: The color of a frame: that of the frames of the theme. In Polish: Zwykły
    QAction* plain = colors->addAction(tr("Plain"));
    plain->setCheckable(true);
    plain->setChecked(now.isEmpty());
    const auto setColor = [this, frameId](const QString& color) {
        if (const MindMapFrame* own = m_document.map().frame(frameId)) {
            MindMapFrame changed = *own;
            changed.color = color;
            m_document.setFrame(changed);
        }
    };
    connect(plain, &QAction::triggered, this, [setColor]() { setColor(QString()); });
    for (std::size_t i = 0; i < m_look.palette.size(); ++i) {
        const QString key = QString::number(i + 1);
        QPixmap swatch(16, 16);
        swatch.fill(m_look.palette.at(i));
        //: A color of the palette of a frame; %1 is its number. In Polish: Kolor %1
        QAction* action = colors->addAction(QIcon(swatch), tr("Color %1").arg(key));
        action->setCheckable(true);
        action->setChecked(now == key);
        connect(action, &QAction::triggered, this, [setColor, key]() { setColor(key); });
    }
    menu.addSeparator();
    //: In Polish: Usuń ramkę
    connect(menuAction(menu, MindMapIcons::REMOVE, tr("Delete the frame"),
                       QKeySequence(Qt::Key_Delete)),
            &QAction::triggered, this, &MindMapView::removeSelected);
}

void MindMapView::addMenuOfNode(QMenu& menu) {
    const MindMap& map = m_document.map();
    const QString id = m_document.selected();
    const MindMapNode* node = map.node(id);
    if (node == nullptr) {
        return;
    }
    const bool one = m_document.selection().size() == 1;
    const bool root = id == map.root();
    //: In the menu of a node of a mind map. In Polish: Dodaj gałąź
    connect(menuAction(menu, MindMapIcons::BRANCH, tr("Add Branch"), QKeySequence(Qt::Key_Tab)),
            &QAction::triggered, this, &MindMapView::addBranch);
    if (!root) {
        //: In Polish: Dodaj poniżej
        connect(menuAction(menu, MindMapIcons::BELOW, tr("Add Below"), QKeySequence(Qt::Key_Return)),
                &QAction::triggered, this, [this]() { addBelow(false); });
        //: In Polish: Dodaj powyżej
        connect(menuAction(menu, nullptr, tr("Add Above"), QKeySequence(Qt::SHIFT | Qt::Key_Return)),
                &QAction::triggered, this, [this]() { addBelow(true); });
    }
    //: In Polish: Dodaj węzeł…
    connect(menuAction(menu, MindMapIcons::ADD, tr("Add Node…"),
                       QKeySequence(Qt::SHIFT | Qt::Key_A)),
            &QAction::triggered, this, [this]() { openAddNode(); });
    menu.addSeparator();
    if (one) {
        //: In Polish: Edytuj nazwę
        connect(menuAction(menu, MindMapIcons::RENAME, tr("Edit Name"), QKeySequence(Qt::Key_F2)),
                &QAction::triggered, this, [this, id]() { startEditing(id); });
        //: The window of a node: its note, its rows, its attributes, its picture. In Polish:
        //: Właściwości węzła…
        connect(menuAction(menu, MindMapIcons::PROPERTIES, tr("Node Properties…"),
                           QKeySequence(Qt::ALT | Qt::Key_Return)),
                &QAction::triggered, this, [this, id]() { emit nodeWindowRequested(id); });
        //: In Polish: Połącz z…
        connect(menuAction(menu, MindMapIcons::CONNECT, tr("Connect To…")), &QAction::triggered,
                this, &MindMapView::connectTo);
        const NodeBox* box = m_scene ? m_scene->box(id) : nullptr;
        if (box != nullptr && !box->content.elementId.isEmpty() && !box->content.missing) {
            //: In Polish: Otwórz element
            connect(menuAction(menu, MindMapIcons::OPEN, tr("Open the Element"),
                               QKeySequence(Qt::CTRL | Qt::Key_Return)),
                    &QAction::triggered, this, &MindMapView::openSelected);
        }
        if (node->isOwn(root)) {
            //: In Polish: Element książki…
            connect(menu.addAction(tr("Element of the book…")), &QAction::triggered, this,
                    [this, id]() { emit linkRequested(id); });
        }
        menu.addSeparator();
    }
    const bool folded = std::all_of(m_document.selection().cbegin(), m_document.selection().cend(),
                                    [&map](const QString& each) {
                                        const MindMapNode* own = map.node(each);
                                        return own != nullptr && own->folded;
                                    });
    connect(menuAction(menu, folded ? MindMapIcons::UNFOLD : MindMapIcons::FOLD,
                       folded
                           //: In Polish: Rozwiń
                           ? tr("Unfold")
                           //: In Polish: Zwiń
                           : tr("Fold"),
                       QKeySequence(Qt::Key_H)),
            &QAction::triggered, this, &MindMapView::foldSelected);
    //: In Polish: Ramka
    connect(menuAction(menu, MindMapIcons::FRAME, tr("Frame"), QKeySequence(Qt::CTRL | Qt::Key_J)),
            &QAction::triggered, this, &MindMapView::frameSelected);
    if (one) {
        MindMapMenus::addMarkers(menu, m_document, m_look, id);
        if (!root) {
            MindMapMenus::addColors(menu, m_document, m_look, id);
        }
    }
    menu.addSeparator();
    if (one) {
        //: In Polish: Kopiuj jako tekst
        connect(menuAction(menu, MindMapIcons::COPY, tr("Copy as text"),
                           QKeySequence(Qt::CTRL | Qt::Key_C)),
                &QAction::triggered, this, &MindMapView::copySelected);
    }
    QAction* pasted = menuAction(menu, MindMapIcons::PASTE, tr("Paste"),
                                 QKeySequence(Qt::CTRL | Qt::Key_V));
    pasted->setEnabled(MindMapMenus::canPaste());
    connect(pasted, &QAction::triggered, this, &MindMapView::paste);
    menu.addSeparator();
    //: In Polish: Usuń
    QAction* remove = menuAction(menu, MindMapIcons::REMOVE, tr("Delete"),
                                 QKeySequence(Qt::Key_Delete));
    remove->setEnabled(!(root && one));
    connect(remove, &QAction::triggered, this, &MindMapView::removeSelected);
}

void MindMapView::addMenuOfMap(QMenu& menu) {
    const std::optional<QPointF> at = m_menuPoint;
    connect(menuAction(menu, MindMapIcons::ADD, tr("Add Node…"),
                       QKeySequence(Qt::SHIFT | Qt::Key_A)),
            &QAction::triggered, this, [this, at]() { openAddNode(at); });
    QAction* pasted = menuAction(menu, MindMapIcons::PASTE, tr("Paste"),
                                 QKeySequence(Qt::CTRL | Qt::Key_V));
    pasted->setEnabled(MindMapMenus::canPaste());
    connect(pasted, &QAction::triggered, this, &MindMapView::paste);
    menu.addSeparator();
    //: The menu of the layouts of a mind map. In Polish: Uporządkuj
    QMenu* arranged = menu.addMenu(core::ArtProvider::getInstance().getIcon(
                                       QString::fromLatin1(MindMapIcons::ARRANGE)),
                                   tr("Arrange"));
    //: A layout of a mind map. In Polish: Na obie strony
    connect(arranged->addAction(tr("On Both Sides")), &QAction::triggered, this,
            [this]() { arrange(MindMapLayout::BothSides); });
    //: In Polish: W prawo
    connect(arranged->addAction(tr("To the Right")), &QAction::triggered, this,
            [this]() { arrange(MindMapLayout::Right); });
    //: In Polish: W dół
    connect(arranged->addAction(tr("Down")), &QAction::triggered, this,
            [this]() { arrange(MindMapLayout::Down); });
    if (hasFamilies()) {
        //: In Polish: Rodzina w pokoleniach
        connect(arranged->addAction(tr("Family in Generations")), &QAction::triggered, this,
                &MindMapView::arrangeGenerations);
    }
    //: In Polish: Cała mapa
    connect(menuAction(menu, MindMapIcons::FIT, tr("Whole Map"), QKeySequence(Qt::Key_Home)),
            &QAction::triggered, this, [this]() { showAll(); });
    //: In Polish: Powiększenie 100%
    connect(menuAction(menu, nullptr, tr("Zoom 100%"), QKeySequence(Qt::CTRL | Qt::Key_0)),
            &QAction::triggered, this, &MindMapView::resetZoom);
}

}  // namespace kalahari::mindmap
