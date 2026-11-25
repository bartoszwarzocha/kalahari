// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file mind_map_content.cpp
/// @brief What the nodes of a mind map show of the book, and what the book has to put on a map

#include <kalahari/mindmap/mind_map_content.h>

#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_registry.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/text_statistics.h>
#include <kalahari/gui/avatar.h>
#include <kalahari/gui/kind_names.h>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSize>

#include <algorithm>
#include <functional>
#include <tuple>

namespace kalahari::mindmap {

namespace {

/// The kinds of the cards of the base package that the nodes have colors and sockets for
const QString CHARACTER = QStringLiteral("character");
const QString LOCATION = QStringLiteral("location");
const QString ITEM = QStringLiteral("item");
const QString FAMILY = QStringLiteral("family");
const QString TIMELINE_EDITOR = QStringLiteral("timeline");

/// The size of a picture in a node, in pixels: twice the size it is drawn, so that it stays
/// sharp when the map is zoomed in
constexpr int IMAGE_WIDTH = 352;
constexpr int IMAGE_HEIGHT = 240;

/// The id of the kind in "package:kind"
QString kindIdOf(const QString& kind) {
    const qsizetype colon = kind.lastIndexOf(QLatin1Char(':'));
    return colon >= 0 ? kind.mid(colon + 1) : kind;
}

/// The order of the kinds whose relations a card shows on its right edge: a character has its
/// places and its items there, an item its places
int rankOf(const QString& kind) {
    const QString id = kindIdOf(kind);
    if (id == CHARACTER) {
        return 0;
    }
    if (id == ITEM) {
        return 1;
    }
    if (id == LOCATION) {
        return 2;
    }
    return 3;
}

/// The order of the rows of the relations: the family and the others first, then the places,
/// the items and the other kinds
int rowRankOf(const QString& kind, const QString& otherKind) {
    if (kind == otherKind) {
        return 0;
    }
    const QString id = kindIdOf(otherKind);
    if (id == CHARACTER) {
        return 1;
    }
    if (id == LOCATION) {
        return 2;
    }
    if (id == ITEM) {
        return 3;
    }
    return 4;
}

/// A row of a field and its value, or of a text across the node (@p wide)
NodeRow rowOf(const QString& id, const QString& label, const QString& value, bool wide = false) {
    NodeRow row;
    row.id = id;
    row.label = label;
    row.value = value;
    row.wide = wide;
    return row;
}

QString language() {
    return gui::programLanguage();
}

core::KindRef kindRefOf(const core::ProjectManager& book, const QString& kind) {
    const std::optional<core::KindReference> reference = core::KindReference::parse(kind);
    return reference ? book.bookTypes().findKind(reference->packageId, reference->kindId)
                     : core::KindRef();
}

/// The text of a field of the plot of a scene or of a text, by its row
QString plotValue(const core::ScenePlot& plot, const QString& row) {
    if (row == QLatin1String(NodeRows::DESCRIPTION)) {
        return plot.description;
    }
    if (row == QLatin1String(NodeRows::POV)) {
        return plot.pov;
    }
    if (row == QLatin1String(NodeRows::PLACE)) {
        return plot.place;
    }
    if (row == QLatin1String(NodeRows::TIME)) {
        return plot.time;
    }
    return QString();
}

}  // namespace

QString NodeRows::relationsRow(const QString& kind, const QString& otherKind, const QString& group) {
    return QString::fromLatin1(RELATIONS) + kind + QLatin1Char('|') + otherKind + QLatin1Char('|') +
           group;
}

MindMapContent::MindMapContent(const core::ProjectManager* book) : m_book(book) {}

void MindMapContent::forget() {
    m_timelines.clear();
    m_scenes.clear();
    m_images.clear();
    m_relationRows.clear();
    m_cardKinds.reset();
}

// =============================================================================
// Names
// =============================================================================

QString MindMapContent::ownKindName(const QString& kind) {
    if (kind == QLatin1String(MindMapNode::TOPIC_KIND)) {
        //: The kind of a node of a mind map. In Polish: Temat główny
        return tr("Main topic");
    }
    if (kind == QLatin1String(MindMapNode::QUESTION_KIND)) {
        //: The kind of a node of a mind map. In Polish: Pytanie
        return tr("Question");
    }
    if (kind == QLatin1String(MindMapNode::NOTE_KIND)) {
        //: The kind of a node of a mind map. In Polish: Notatka
        return tr("Note");
    }
    //: The kind of a node of a mind map. In Polish: Pomysł
    return tr("Idea");
}

QString MindMapContent::statusName(const QString& status) {
    if (status == QLatin1String("final")) {
        //: The status of a text on a node of a mind map. In Polish: Gotowy
        return tr("Final");
    }
    if (status == QLatin1String("revision")) {
        //: The status of a text on a node of a mind map. In Polish: Poprawki
        return tr("Revision");
    }
    //: The status of a text on a node of a mind map. In Polish: Szkic
    return tr("Draft");
}

QString MindMapContent::cardKindOf(const QString& elementId) const {
    if (m_book == nullptr || elementId.isEmpty()) {
        return QString();
    }
    const core::ProjectElement* element = m_book->findElement(elementId);
    return element != nullptr && m_book->isCard(*element) ? element->kind.toString() : QString();
}

QString MindMapContent::pluralOf(const QString& kind) const {
    const core::KindRef reference = m_book != nullptr ? kindRefOf(*m_book, kind) : core::KindRef();
    if (!reference) {
        return kindIdOf(kind);
    }
    const QString plural = reference.kind->plural.text(language());
    return plural.isEmpty() ? reference.kind->name.text(language()) : plural;
}

NodeColor MindMapContent::colorOf(const core::ProjectElement& element) const {
    if (m_book == nullptr) {
        return NodeColor::Element;
    }
    if (m_book->isCard(element)) {
        const QString id = element.kind.kindId;
        return id == CHARACTER  ? NodeColor::Character
               : id == LOCATION ? NodeColor::Place
               : id == ITEM     ? NodeColor::Item
                                : NodeColor::Element;
    }
    const core::KindRef kind = m_book->kindOf(element);
    if (kind && kind.kind->form == core::ElementForm::Text) {
        return NodeColor::Text;
    }
    return NodeColor::Element;
}

QString MindMapContent::sceneTitle(const QString& textId, int scene, bool planned) const {
    const core::ProjectElement* text = m_book != nullptr ? m_book->findElement(textId) : nullptr;
    if (text == nullptr) {
        return QString();
    }
    QString title;
    if (planned) {
        if (scene >= 0 && scene < text->plot.planned.size()) {
            title = text->plot.planned.at(scene).plot.title;
        }
        //: The title of a node of a mind map: a scene in the plan of a text, without a title
        //: of its own. %1: its number. In Polish: Scena w planie %1
        return title.isEmpty() ? tr("Planned scene %1").arg(scene + 1) : title;
    }
    const QList<core::KmlScene>& scenes = scenesOf(textId);
    if (scene == 0) {
        title = text->plot.opening.title;
    } else if (scene > 0 && scene < scenes.size()) {
        title = scenes.at(scene).plot.title;
    }
    //: The title of a node of a mind map: a scene of a text without a title of its own. %1:
    //: its number. In Polish: Scena %1
    return title.isEmpty() ? tr("Scene %1").arg(scene + 1) : title;
}

QString MindMapContent::titleOf(const MindMap& map, const MindMapNode& node,
                                const QString& mapTitle) const {
    if (!node.text.isEmpty()) {
        return node.text;
    }
    if (node.id == map.root()) {
        return mapTitle;
    }
    if (m_book == nullptr || node.link.isEmpty()) {
        return QString();
    }
    const core::ProjectElement* element = m_book->findElement(node.link);
    if (element == nullptr) {
        //: The title of a node of a mind map whose element is not in the book any more. In
        //: Polish: (usunięty element)
        return tr("(deleted element)");
    }
    if (node.kind == QLatin1String(MindMapNode::SCENE_KIND)) {
        return sceneTitle(node.link, node.scene, node.planned);
    }
    if (node.kind == QLatin1String(MindMapNode::THREAD_KIND)) {
        for (const auto& [id, thread] : threadsOf(node.link)) {
            if (id == node.thread) {
                return thread.name;
            }
        }
        //: The title of a node of a mind map whose thread is not in the timeline any more.
        //: In Polish: (usunięty wątek)
        return tr("(deleted thread)");
    }
    return element->title;
}

// =============================================================================
// The book read
// =============================================================================

const QList<core::KmlScene>& MindMapContent::scenesOf(const QString& textId) const {
    auto it = m_scenes.find(textId);
    if (it == m_scenes.end()) {
        it = m_scenes.insert(textId, m_book != nullptr ? m_book->scenesOf(textId)
                                                       : QList<core::KmlScene>());
    }
    return *it;
}

QList<std::pair<QString, MindMapContent::ThreadInfo>> MindMapContent::threadsOf(
    const QString& timelineId) const {
    QList<std::pair<QString, ThreadInfo>> threads;
    const core::ProjectElement* timeline = m_book != nullptr ? m_book->findElement(timelineId)
                                                             : nullptr;
    if (timeline == nullptr) {
        return threads;
    }
    auto it = m_timelines.find(timelineId);
    if (it == m_timelines.end()) {
        QFile file(m_book->filePathOf(*timeline));
        QJsonObject json;
        if (file.open(QIODevice::ReadOnly)) {
            json = QJsonDocument::fromJson(file.readAll()).object();
        }
        it = m_timelines.insert(timelineId, json);
    }
    QHash<QString, int> events;
    for (const auto& value : it->value(QStringLiteral("events")).toArray()) {
        events[value.toObject().value(QStringLiteral("thread")).toString()] += 1;
    }
    for (const auto& value : it->value(QStringLiteral("threads")).toArray()) {
        const QJsonObject thread = value.toObject();
        const QString id = thread.value(QStringLiteral("id")).toString();
        if (!id.isEmpty()) {
            threads.append({id, ThreadInfo{thread.value(QStringLiteral("name")).toString(),
                                           events.value(id)}});
        }
    }
    return threads;
}

QStringList MindMapContent::cardKinds() const {
    if (m_cardKinds) {
        return *m_cardKinds;
    }
    QStringList kinds;
    if (m_book != nullptr) {
        for (const core::KindRef& kind : m_book->kindsFor(core::BookPlace::Workshop)) {
            if (kind.kind->isCard() && !kinds.contains(kind.reference())) {
                kinds.append(kind.reference());
            }
        }
        const core::CardRelations relations = m_book->cardRelations();
        for (const core::ProjectElement* card : relations.cards()) {
            const QString kind = card->kind.toString();
            if (!kinds.contains(kind)) {
                kinds.append(kind);
            }
        }
    }
    m_cardKinds = kinds;
    return kinds;
}

bool MindMapContent::shows(const RelationMapOptions& options, const QString& kind,
                           const QString& otherKind, const QString& group) {
    if (kind != otherKind) {
        return options.across;
    }
    return group == FAMILY ? options.family : options.others;
}

QString MindMapContent::groupOf(const QString& relationId, const QString& kind) const {
    if (m_book == nullptr) {
        return QString();
    }
    const std::optional<core::RelationInfo> info = m_book->cardRelations().find(relationId, kind);
    return info && info->groupText(QStringLiteral("en")).compare(FAMILY, Qt::CaseInsensitive) == 0
               ? FAMILY
               : QString();
}

SocketKind MindMapContent::socketOf(const QString& kind, const QString& otherKind,
                                    const QString& group) {
    if (kind == otherKind) {
        return group == FAMILY ? SocketKind::Family : SocketKind::Relations;
    }
    const QString a = kindIdOf(kind);
    const QString b = kindIdOf(otherKind);
    if (a == LOCATION || b == LOCATION) {
        return SocketKind::Places;
    }
    if (a == ITEM || b == ITEM) {
        return SocketKind::Items;
    }
    return SocketKind::Relations;
}

QList<NodeRow> MindMapContent::relationRows(const QString& kind) const {
    if (const auto it = m_relationRows.constFind(kind); it != m_relationRows.cend()) {
        return *it;
    }
    QList<NodeRow> rows;
    if (m_book != nullptr) {
        const core::CardRelations relations = m_book->cardRelations();
        QStringList others = cardKinds();
        std::stable_sort(others.begin(), others.end(), [&kind](const QString& a, const QString& b) {
            return rowRankOf(kind, a) < rowRankOf(kind, b);
        });
        for (const QString& other : std::as_const(others)) {
            const QList<core::RelationInfo> between = relations.between(kind, other);
            if (between.isEmpty()) {
                continue;
            }
            QStringList groups;
            if (other == kind) {
                const bool family = std::any_of(
                    between.cbegin(), between.cend(), [](const core::RelationInfo& info) {
                        return info.groupText(QStringLiteral("en"))
                                   .compare(FAMILY, Qt::CaseInsensitive) == 0;
                    });
                if (family) {
                    groups.append(FAMILY);
                }
                if (std::any_of(between.cbegin(), between.cend(),
                                [](const core::RelationInfo& info) {
                                    return info.groupText(QStringLiteral("en"))
                                               .compare(FAMILY, Qt::CaseInsensitive) != 0;
                                })) {
                    groups.append(QString());
                }
            } else {
                groups.append(QString());
            }
            for (const QString& group : std::as_const(groups)) {
                NodeRow row;
                row.id = NodeRows::relationsRow(kind, other, group);
                row.otherKind = other;
                row.group = group;
                row.socket = socketOf(kind, other, group);
                if (other == kind) {
                    row.left = true;
                    row.right = true;
                    row.label = group == FAMILY ?
                                                //: A row of a node of a card on a mind map:
                                                //: its family, with its socket. In Polish:
                                                //: Rodzina
                                    tr("Family")
                                                :
                                                //: A row of a node of a card on a mind map:
                                                //: its other relations with cards of its
                                                //: kind. In Polish: Relacje
                                    tr("Relations");
                } else {
                    const bool right = rankOf(kind) < rankOf(other) ||
                                       (rankOf(kind) == rankOf(other) && kind < other);
                    row.right = right;
                    row.left = !right;
                    row.label = pluralOf(other);
                }
                rows.append(row);
            }
        }
    }
    m_relationRows.insert(kind, rows);
    return rows;
}

// =============================================================================
// The rows
// =============================================================================

QList<RowChoice> MindMapContent::rowChoices(const MindMapNode& node) const {
    QList<RowChoice> choices;
    if (m_book == nullptr || node.link.isEmpty()) {
        return choices;
    }
    const core::ProjectElement* element = m_book->findElement(node.link);
    if (element == nullptr) {
        return choices;
    }
    //: A row of a node of a mind map: the short description of its element. In Polish: W
    //: skrócie
    const QString inShort = tr("In short");
    //: A row of a node of a mind map. In Polish: Stan
    const QString status = tr("Status");
    //: A row of a node of a mind map. In Polish: Słowa
    const QString words = tr("Words");
    //: A row of a node of a mind map: the point of view. In Polish: Punkt widzenia
    const QString pov = tr("Point of view");
    //: A row of a node of a mind map: the place of the action. In Polish: Miejsce akcji
    const QString place = tr("Place of the action");
    //: A row of a node of a mind map: the time of the action. In Polish: Czas akcji
    const QString time = tr("Time of the action");
    if (node.kind == QLatin1String(MindMapNode::SCENE_KIND)) {
        //: A row of a node of a scene on a mind map. In Polish: Opis
        choices = {{QString::fromLatin1(NodeRows::DESCRIPTION), tr("Description")},
                   {QString::fromLatin1(NodeRows::POV), pov},
                   {QString::fromLatin1(NodeRows::PLACE), place},
                   {QString::fromLatin1(NodeRows::TIME), time},
                   {QString::fromLatin1(NodeRows::STATUS), status},
                   {QString::fromLatin1(NodeRows::WORDS), words}};
        return choices;
    }
    if (node.kind == QLatin1String(MindMapNode::THREAD_KIND)) {
        //: A row of a node of a thread of a timeline on a mind map: how many events it has.
        //: In Polish: Wydarzenia
        choices = {{QString::fromLatin1(NodeRows::EVENTS), tr("Events")}};
        return choices;
    }
    if (m_book->isCard(*element)) {
        const core::KindRef kind = m_book->kindOf(*element);
        if (kind) {
            for (const core::CardField& field : m_book->cardFields(kind)) {
                choices.append(
                    {QString::fromLatin1(NodeRows::FIELD) + field.id, field.name.text(language())});
            }
        }
        choices.append({QString::fromLatin1(NodeRows::DESCRIPTION), inShort});
        for (const NodeRow& row : relationRows(element->kind.toString())) {
            choices.append({row.id, row.label});
        }
        return choices;
    }
    if (colorOf(*element) == NodeColor::Text) {
        choices = {{QString::fromLatin1(NodeRows::STATUS), status},
                   {QString::fromLatin1(NodeRows::WORDS), words},
                   {QString::fromLatin1(NodeRows::POV), pov},
                   {QString::fromLatin1(NodeRows::PLACE), place},
                   {QString::fromLatin1(NodeRows::TIME), time},
                   {QString::fromLatin1(NodeRows::DESCRIPTION), inShort}};
        return choices;
    }
    choices.append({QString::fromLatin1(NodeRows::DESCRIPTION), inShort});
    return choices;
}

QStringList MindMapContent::defaultRows(const MindMapNode& node) const {
    QStringList rows;
    if (m_book == nullptr || node.link.isEmpty()) {
        return rows;
    }
    if (node.kind == QLatin1String(MindMapNode::SCENE_KIND)) {
        return {QString::fromLatin1(NodeRows::DESCRIPTION), QString::fromLatin1(NodeRows::POV),
                QString::fromLatin1(NodeRows::PLACE)};
    }
    if (node.kind == QLatin1String(MindMapNode::THREAD_KIND)) {
        return {QString::fromLatin1(NodeRows::EVENTS)};
    }
    const core::ProjectElement* element = m_book->findElement(node.link);
    if (element == nullptr) {
        return rows;
    }
    if (m_book->isCard(*element)) {
        const core::KindRef kind = m_book->kindOf(*element);
        const core::CardField* shown = kind ? m_book->subtitleField(kind) : nullptr;
        if (shown == nullptr && kind) {
            const QList<core::CardField>& fields = m_book->cardFields(kind);
            const auto first = std::find_if(fields.cbegin(), fields.cend(),
                                            [](const core::CardField& field) {
                                                return field.inList;
                                            });
            shown = first != fields.cend() ? &*first : nullptr;
        }
        if (shown != nullptr) {
            rows.append(QString::fromLatin1(NodeRows::FIELD) + shown->id);
        }
        for (const NodeRow& row : relationRows(element->kind.toString())) {
            rows.append(row.id);
        }
        return rows;
    }
    if (colorOf(*element) == NodeColor::Text) {
        return {QString::fromLatin1(NodeRows::STATUS), QString::fromLatin1(NodeRows::WORDS)};
    }
    return {QString::fromLatin1(NodeRows::DESCRIPTION)};
}

void MindMapContent::addCardRows(const MindMapNode& node, const core::ProjectElement& card,
                                 const QStringList& rows, NodeContent& content) const {
    Q_UNUSED(node)
    const core::KindRef kind = m_book->kindOf(card);
    const QList<NodeRow> relations = relationRows(card.kind.toString());
    for (const QString& id : rows) {
        if (id.startsWith(QLatin1String(NodeRows::FIELD))) {
            const QString fieldId = id.mid(static_cast<qsizetype>(qstrlen(NodeRows::FIELD)));
            const core::CardField* field = kind ? m_book->cardField(kind, fieldId) : nullptr;
            if (field == nullptr) {
                continue;
            }
            QString value = m_book->cardFieldText(card.id, fieldId);
            if (field->type == core::CardFieldType::Choice) {
                if (const core::CardFieldChoice* choice = field->choice(value)) {
                    value = choice->name.text(language());
                }
            }
            NodeRow row;
            row.id = id;
            row.label = field->name.text(language());
            row.value = value;
            row.wide = field->type == core::CardFieldType::LongText;
            content.rows.append(row);
        } else if (id == QLatin1String(NodeRows::DESCRIPTION)) {
            if (!card.description.isEmpty()) {
                content.rows.append(rowOf(id, QString(), card.description, true));
            }
        } else if (id.startsWith(QLatin1String(NodeRows::RELATIONS))) {
            const auto found = std::find_if(relations.cbegin(), relations.cend(),
                                            [&id](const NodeRow& row) { return row.id == id; });
            if (found != relations.cend()) {
                content.rows.append(*found);
            }
        }
    }
}

void MindMapContent::addTextRows(const MindMapNode& node, const core::ProjectElement& text,
                                 const QStringList& rows, NodeContent& content) const {
    Q_UNUSED(node)
    const QList<RowChoice> choices = rowChoices(node);
    const auto labelOf = [&choices](const QString& id) {
        const auto found = std::find_if(choices.cbegin(), choices.cend(),
                                        [&id](const RowChoice& choice) { return choice.id == id; });
        return found != choices.cend() ? found->label : id;
    };
    for (const QString& id : rows) {
        NodeRow row;
        row.id = id;
        row.label = labelOf(id);
        if (id == QLatin1String(NodeRows::STATUS)) {
            row.value = statusName(core::ProjectManager::statusOf(text));
        } else if (id == QLatin1String(NodeRows::WORDS)) {
            row.value = QString::number(m_book->wordCount(text.id));
        } else if (id == QLatin1String(NodeRows::POV)) {
            row.value = text.plot.pov;
        } else if (id == QLatin1String(NodeRows::PLACE)) {
            row.value = text.plot.place;
        } else if (id == QLatin1String(NodeRows::TIME)) {
            row.value = text.plot.time;
        } else if (id == QLatin1String(NodeRows::DESCRIPTION)) {
            row.label.clear();
            row.value = text.description;
            row.wide = true;
        } else {
            continue;
        }
        if (!row.value.isEmpty() || !row.wide) {
            content.rows.append(row);
        }
    }
}

void MindMapContent::addSceneRows(const MindMapNode& node, const QStringList& rows,
                                  NodeContent& content) const {
    const core::ProjectElement* text = m_book->findElement(node.link);
    core::ScenePlot plot;
    int words = 0;
    bool found = false;
    if (node.planned) {
        if (node.scene >= 0 && node.scene < text->plot.planned.size()) {
            plot = text->plot.planned.at(node.scene).plot;
            found = true;
        }
    } else {
        const QList<core::KmlScene>& scenes = scenesOf(node.link);
        if (node.scene >= 0 && node.scene < scenes.size()) {
            plot = node.scene == 0 ? text->plot.opening : scenes.at(node.scene).plot;
            words = scenes.at(node.scene).words.wordsUnder(core::wordCountRules());
            found = true;
        }
    }
    if (!found) {
        content.missing = true;
        return;
    }
    const QList<RowChoice> choices = rowChoices(node);
    for (const QString& id : rows) {
        const auto choice = std::find_if(choices.cbegin(), choices.cend(),
                                         [&id](const RowChoice& each) { return each.id == id; });
        if (choice == choices.cend()) {
            continue;
        }
        NodeRow row;
        row.id = id;
        row.label = choice->label;
        if (id == QLatin1String(NodeRows::STATUS)) {
            row.value = node.planned ?
                                     //: The status of a scene in the plan on a node of a mind
                                     //: map. In Polish: W planie
                            tr("In the plan")
                                     : statusName(plot.status.isEmpty()
                                                      ? core::ProjectManager::statusOf(*text)
                                                      : plot.status);
        } else if (id == QLatin1String(NodeRows::WORDS)) {
            row.value = QString::number(words);
        } else if (id == QLatin1String(NodeRows::DESCRIPTION)) {
            row.label.clear();
            row.value = plot.description;
            row.wide = true;
            if (row.value.isEmpty()) {
                continue;
            }
        } else {
            row.value = plotValue(plot, id);
        }
        content.rows.append(row);
    }
}

void MindMapContent::addThreadRows(const MindMapNode& node, const QStringList& rows,
                                   NodeContent& content) const {
    bool found = false;
    int events = 0;
    for (const auto& [id, thread] : threadsOf(node.link)) {
        if (id == node.thread) {
            found = true;
            events = thread.events;
        }
    }
    if (!found) {
        content.missing = true;
        return;
    }
    if (rows.contains(QLatin1String(NodeRows::EVENTS))) {
        //: A row of a node of a thread of a timeline on a mind map: how many events it has.
        //: In Polish: Wydarzenia
        content.rows.append(
            rowOf(QString::fromLatin1(NodeRows::EVENTS), tr("Events"), QString::number(events)));
    }
}

QImage MindMapContent::imageOf(const MindMapNode& node, const MindMapLook& look) const {
    if (m_book == nullptr) {
        return QImage();
    }
    QString image = node.image;
    if (image.isEmpty()) {
        image = look.portraits ? QString::fromLatin1(MindMapNode::PORTRAIT)
                               : QString::fromLatin1(MindMapNode::NO_IMAGE);
    }
    if (image == QLatin1String(MindMapNode::NO_IMAGE)) {
        return QImage();
    }
    const auto picture = [this](const QString& materialId) {
        const core::ProjectElement* material = m_book->findElement(materialId);
        if (material == nullptr) {
            return QImage();
        }
        const QString key = QStringLiteral("m:") + materialId;
        if (!m_images.contains(key)) {
            QImage read(m_book->filePathOf(*material));
            if (!read.isNull()) {
                read = read.scaled(QSize(IMAGE_WIDTH, IMAGE_HEIGHT), Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
            }
            m_images.insert(key, read);
        }
        return m_images.value(key);
    };
    if (image != QLatin1String(MindMapNode::PORTRAIT)) {
        return picture(image);
    }
    const core::ProjectElement* card = m_book->findElement(node.link);
    if (card == nullptr || !m_book->isCard(*card) ||
        node.kind != QLatin1String(MindMapNode::ELEMENT_KIND)) {
        return QImage();
    }
    const core::CardPortrait* portrait = card->card.mainPortrait();
    if (portrait == nullptr) {
        return QImage();
    }
    if (!portrait->isComposed()) {
        return picture(portrait->image);
    }
    const QString key = QStringLiteral("p:") + card->id;
    if (!m_images.contains(key)) {
        m_images.insert(key, gui::Avatar::imageOf(*portrait, gui::AvatarFrame::Bust,
                                                  QColor(card->card.color),
                                                  core::CardRelations::genderOf(*card),
                                                  QSize(IMAGE_HEIGHT, IMAGE_HEIGHT)));
    }
    return m_images.value(key);
}

NodeContent MindMapContent::contentOf(const MindMap& map, const MindMapNode& node,
                                      const QString& mapTitle, const MindMapLook& look) const {
    NodeContent content;
    const bool root = node.id == map.root();
    content.title = titleOf(map, node, mapTitle);
    const QString kind = node.kindOrIdea(root);
    const core::ProjectElement* element = m_book != nullptr && !node.link.isEmpty()
                                              ? m_book->findElement(node.link)
                                              : nullptr;
    if (node.isOwn(root)) {
        content.kindName = ownKindName(kind);
        content.color = kind == QLatin1String(MindMapNode::TOPIC_KIND)      ? NodeColor::Topic
                        : kind == QLatin1String(MindMapNode::QUESTION_KIND) ? NodeColor::Question
                        : kind == QLatin1String(MindMapNode::NOTE_KIND)     ? NodeColor::Note
                                                                            : NodeColor::Idea;
        content.icon = kind == QLatin1String(MindMapNode::TOPIC_KIND)
                           ? QStringLiteral("mindmap.kind.topic")
                       : kind == QLatin1String(MindMapNode::QUESTION_KIND)
                           ? QStringLiteral("mindmap.kind.question")
                       : kind == QLatin1String(MindMapNode::NOTE_KIND)
                           ? QStringLiteral("mindmap.kind.note")
                           : QStringLiteral("mindmap.kind.idea");
        content.elementId = element != nullptr ? element->id : QString();
        if (!node.link.isEmpty()) {
            //: A row of a node of a mind map: the element of the book it is about. In Polish:
            //: Element
            content.rows.append(
                rowOf(QString::fromLatin1(NodeRows::LINK), tr("Element"),
                      element != nullptr ? element->title : tr("(deleted element)")));
        }
    } else if (element == nullptr) {
        content.missing = true;
        content.color = NodeColor::Element;
        content.icon = QStringLiteral("mindmap.kind.idea");
    } else {
        content.elementId = element->id;
        const QStringList rows = node.rows.value_or(defaultRows(node));
        if (kind == QLatin1String(MindMapNode::SCENE_KIND)) {
            //: The kind of a node of a mind map: a scene of a text. In Polish: Scena
            content.kindName = tr("Scene");
            content.color = NodeColor::Scene;
            content.icon = QStringLiteral("mindmap.kind.scene");
            addSceneRows(node, rows, content);
        } else if (kind == QLatin1String(MindMapNode::THREAD_KIND)) {
            //: The kind of a node of a mind map: a thread of a timeline. In Polish: Wątek
            content.kindName = tr("Thread");
            content.color = NodeColor::Thread;
            content.icon = QStringLiteral("mindmap.kind.thread");
            addThreadRows(node, rows, content);
        } else {
            content.color = colorOf(*element);
            const core::KindRef elementKind = m_book->kindOf(*element);
            content.kindName = elementKind ? elementKind.kind->name.text(language())
                                           : element->kind.kindId;
            content.icon = elementKind && !elementKind.kind->icon.isEmpty()
                               ? elementKind.kind->icon
                               : QStringLiteral("mindmap.kind.element");
            if (m_book->isCard(*element)) {
                content.cardKind = element->kind.toString();
                addCardRows(node, *element, rows, content);
            } else if (content.color == NodeColor::Text) {
                addTextRows(node, *element, rows, content);
            } else if (rows.contains(QLatin1String(NodeRows::DESCRIPTION)) &&
                       !element->description.isEmpty()) {
                content.rows.append(rowOf(QString::fromLatin1(NodeRows::DESCRIPTION), QString(),
                                          element->description, true));
            }
        }
    }
    if (!node.note.isEmpty()) {
        content.rows.append(rowOf(QString::fromLatin1(NodeRows::NOTE), QString(), node.note, true));
    }
    for (const NodeAttribute& attribute : node.attributes) {
        content.rows.append(rowOf(QString::fromLatin1(NodeRows::ATTRIBUTE) + attribute.name,
                                  attribute.name, attribute.value));
    }
    if (!node.markers.isEmpty()) {
        NodeRow markers;
        markers.id = QString::fromLatin1(NodeRows::MARKERS);
        markers.markers = node.markers;
        content.rows.append(markers);
    }
    content.image = imageOf(node, look);
    return content;
}

// =============================================================================
// The relations
// =============================================================================

QList<RelationLink> MindMapContent::relationLinks(const MindMap& map) const {
    QList<RelationLink> links;
    if (m_book == nullptr) {
        return links;
    }
    QHash<QString, QString> nodes;  // Card -> its node
    for (const QString& id : map.nodeIds()) {
        const MindMapNode* node = map.node(id);
        if (node->kind == QLatin1String(MindMapNode::ELEMENT_KIND) && !node->link.isEmpty() &&
            !nodes.contains(node->link) && !cardKindOf(node->link).isEmpty()) {
            nodes.insert(node->link, id);
        }
    }
    if (nodes.size() < 2) {
        return links;
    }
    const core::CardRelations relations = m_book->cardRelations();
    const std::optional<RelationMapOptions>& options = map.relationMap();
    for (auto it = nodes.cbegin(); it != nodes.cend(); ++it) {
        const core::ProjectElement* card = m_book->findElement(it.key());
        const QString kind = card->kind.toString();
        for (const core::ShownRelation& relation : relations.of(it.key())) {
            if (relation.ownerId != it.key() || !nodes.contains(relation.otherId)) {
                continue;
            }
            const core::ProjectElement* other = m_book->findElement(relation.otherId);
            const QString otherKind = other->kind.toString();
            const QString group = groupOf(relation.linkRole, kind);
            if (options && !shows(*options, kind, otherKind, group)) {
                continue;
            }
            RelationLink link;
            link.from = it.value();
            link.to = nodes.value(relation.otherId);
            link.fromRow = NodeRows::relationsRow(kind, otherKind, group);
            link.toRow = NodeRows::relationsRow(otherKind, kind, group);
            link.kind = socketOf(kind, otherKind, group);
            link.relation = relation;
            // What each card is to the other: "dziadek – wnuczka"
            const QString otherIs = relations.labelOf(relation.linkRole, kind, *other, language());
            const std::optional<core::RelationInfo> info = relations.find(relation.linkRole, kind);
            const QString cardIs = info ? relations.labelOf(info->inverse, otherKind, *card,
                                                            language())
                                        : QString();
            link.label = cardIs.isEmpty() || cardIs == otherIs
                             ? otherIs
                             : QStringLiteral("%1 – %2").arg(cardIs, otherIs);
            links.append(link);
        }
    }
    // The same order each time: by the nodes
    std::sort(links.begin(), links.end(), [](const RelationLink& a, const RelationLink& b) {
        return std::tie(a.from, a.to, a.relation.linkRole) <
               std::tie(b.from, b.to, b.relation.linkRole);
    });
    return links;
}

QList<core::RelationInfo> MindMapContent::relationChoices(const QString& cardId,
                                                          const QString& otherId,
                                                          SocketKind kind) const {
    QList<core::RelationInfo> choices;
    if (m_book == nullptr) {
        return choices;
    }
    const core::ProjectElement* card = m_book->findElement(cardId);
    const core::ProjectElement* other = m_book->findElement(otherId);
    if (card == nullptr || other == nullptr) {
        return choices;
    }
    const core::CardRelations relations = m_book->cardRelations();
    for (const core::RelationInfo& info : relations.between(*card, *other)) {
        const bool family =
            info.groupText(QStringLiteral("en")).compare(FAMILY, Qt::CaseInsensitive) == 0;
        if ((kind == SocketKind::Family && !family) || (kind == SocketKind::Relations && family)) {
            continue;
        }
        choices.append(info);
    }
    return choices;
}

// =============================================================================
// What the book has to put on a map
// =============================================================================

QList<CatalogEntry> MindMapContent::catalog() const {
    QList<CatalogEntry> entries;
    if (m_book == nullptr || m_book->project() == nullptr) {
        return entries;
    }
    const QString element = QString::fromLatin1(MindMapNode::ELEMENT_KIND);
    // The cards, by their kinds
    const core::CardRelations relations = m_book->cardRelations();
    for (const QString& kind : cardKinds()) {
        for (const core::ProjectElement* card : relations.cards()) {
            if (card->kind.toString() == kind) {
                entries.append(CatalogEntry{element, card->id, -1, false, QString(), card->title,
                                            pluralOf(kind), colorOf(*card)});
            }
        }
    }
    // The texts of the book and their scenes
    //: A group of the menu Add Node of a mind map. In Polish: Teksty
    const QString texts = tr("Texts");
    //: A group of the menu Add Node of a mind map. In Polish: Sceny
    const QString scenes = tr("Scenes");
    QList<CatalogEntry> sceneEntries;
    for (const core::ProjectElement* text :
         m_book->project()->readingOrder(m_book->currentBookIndex())) {
        const core::KindRef kind = m_book->kindOf(*text);
        if (!kind || kind.kind->form != core::ElementForm::Text || m_book->isCard(*text)) {
            continue;
        }
        entries.append(CatalogEntry{element, text->id, -1, false, QString(), text->title, texts,
                                    NodeColor::Text});
        const qsizetype written = scenesOf(text->id).size();
        if (written > 1) {
            for (int scene = 0; scene < written; ++scene) {
                sceneEntries.append(CatalogEntry{
                    QString::fromLatin1(MindMapNode::SCENE_KIND), text->id, scene, false, QString(),
                    QStringLiteral("%1 › %2").arg(text->title, sceneTitle(text->id, scene, false)),
                    scenes, NodeColor::Scene});
            }
        }
        for (int scene = 0; scene < text->plot.planned.size(); ++scene) {
            sceneEntries.append(CatalogEntry{
                QString::fromLatin1(MindMapNode::SCENE_KIND), text->id, scene, true, QString(),
                QStringLiteral("%1 › %2").arg(text->title, sceneTitle(text->id, scene, true)),
                scenes, NodeColor::Scene});
        }
    }
    entries.append(sceneEntries);
    // The threads of the timelines
    //: A group of the menu Add Node of a mind map: the threads of the timelines. In Polish:
    //: Wątki
    const QString threads = tr("Threads");
    for (const core::ProjectElement& each :
         m_book->project()->elementsIn(core::BookPlace::Workshop)) {
        const std::function<void(const core::ProjectElement&)> add =
            [&](const core::ProjectElement& item) {
                const core::KindRef kind = m_book->kindOf(item);
                if (kind && kind.kind->editor == TIMELINE_EDITOR) {
                    for (const auto& [id, thread] : threadsOf(item.id)) {
                        entries.append(CatalogEntry{
                            QString::fromLatin1(MindMapNode::THREAD_KIND), item.id, -1, false, id,
                            QStringLiteral("%1 › %2").arg(item.title, thread.name), threads,
                            NodeColor::Thread});
                    }
                }
                for (const core::ProjectElement& inside : item.elements) {
                    add(inside);
                }
            };
        add(each);
    }
    return entries;
}

}  // namespace kalahari::mindmap
