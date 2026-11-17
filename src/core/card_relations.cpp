// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_relations.cpp
/// @brief The relations between the cards of a book

#include "kalahari/core/card_relations.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/card_data.h"
#include "kalahari/core/project_manager.h"

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <optional>
#include <ranges>

namespace kalahari::core {

namespace {

/// The cards among @p elements and the groups inside them, in their order: each before those
/// inside it
QList<const ProjectElement*> cardsOf(const ProjectManager& book,
                                     const QList<ProjectElement>& elements) {
    QList<const ProjectElement*> cards;
    QList<const ProjectElement*> waiting;  // the next one last
    for (const ProjectElement& each : std::views::reverse(elements)) {
        waiting << &each;
    }
    while (!waiting.isEmpty()) {
        const ProjectElement* element = waiting.takeLast();
        if (book.isCard(*element)) {
            cards << element;
        }
        for (const ProjectElement& each : std::views::reverse(element->elements)) {
            waiting << &each;
        }
    }
    return cards;
}

/// @p kind with its package: "character" of @p package is "kalahari.base:character"
QString qualified(const KindReference& kind, const BookTypePackage& package) {
    return kind.packageId.isEmpty() ? package.id + QLatin1Char(':') + kind.kindId
                                    : kind.toString();
}

/// What the program knows of relation @p relation of @p package
RelationInfo infoOf(const CardRelation& relation, const BookTypePackage& package) {
    RelationInfo info;
    info.id = relation.id;
    info.inverse = relation.inverse;
    info.kind = qualified(relation.kind, package);
    info.otherKind = qualified(relation.otherKind, package);
    info.relation = &relation;
    return info;
}

/// What the program knows of relation @p relation of the Workshop
RelationInfo infoOf(const OwnRelation& relation) {
    RelationInfo info;
    info.id = relation.id;
    info.inverse = relation.inverse;
    info.kind = relation.kind;
    info.otherKind = relation.otherKind;
    info.own = &relation;
    return info;
}

/// The cards of @p above (the card above, by the card) whose way up comes back to them: those
/// in a circle
QStringList inCircles(const QHash<QString, QString>& above) {
    QStringList circle;
    for (auto it = above.cbegin(); it != above.cend(); ++it) {
        QSet<QString> passed;
        QString at = it.value();
        while (at != it.key() && above.contains(at) && !passed.contains(at)) {
            passed.insert(at);
            at = above.value(at);
        }
        if (at == it.key()) {
            circle << at;
        }
    }
    return circle;
}

}  // namespace

QString RelationInfo::choiceText(const QString& language) const {
    if (own != nullptr) {
        return own->name;
    }
    return relation != nullptr ? relation->choiceText(language) : id;
}

QString RelationInfo::groupText(const QString& language) const {
    return relation != nullptr ? relation->group.text(language) : QString();
}

QString RelationInfo::nameFor(const QString& gender, const QString& language) const {
    if (own != nullptr) {
        return own->name;
    }
    return relation != nullptr ? relation->nameFor(gender, language) : id;
}

CardRelations::CardRelations(const ProjectManager& book) : m_book(&book) {
    if (const BookProject* project = book.project()) {
        m_cards = cardsOf(book, project->workshop.elements);
    }
}

std::optional<RelationInfo> CardRelations::find(const QString& relationId,
                                                const QString& kind) const {
    if (const BookProject* project = m_book->project()) {
        for (const OwnRelation& own : project->workshop.relations) {
            if (own.id == relationId && own.kind == kind) {
                return infoOf(own);
            }
        }
    }
    return findInPackages(relationId, kind);
}

std::optional<RelationInfo> CardRelations::findInPackages(const QString& relationId,
                                                          const QString& kind) const {
    // The package of the kind first: the relations of its cards are there
    const QString packageId = kind.section(QLatin1Char(':'), 0, 0);
    QList<const BookTypePackage*> packages = m_book->bookTypes().packages();
    std::stable_partition(packages.begin(), packages.end(),
                          [&packageId](const BookTypePackage* each) {
                              return each->id == packageId;
                          });
    for (const BookTypePackage* package : packages) {
        const CardRelation* relation = package->relation(relationId);
        if (relation != nullptr && qualified(relation->kind, *package) == kind) {
            return infoOf(*relation, *package);
        }
    }
    return std::nullopt;
}

QList<RelationInfo> CardRelations::between(const ProjectElement& card,
                                           const ProjectElement& other) const {
    return between(kindOf(card), kindOf(other));
}

QList<RelationInfo> CardRelations::between(const QString& kind, const QString& otherKind) const {
    QList<RelationInfo> relations;
    for (const BookTypePackage* package : m_book->bookTypes().packages()) {
        for (const CardRelation& relation : package->relations) {
            if (qualified(relation.kind, *package) == kind &&
                qualified(relation.otherKind, *package) == otherKind) {
                relations << infoOf(relation, *package);
            }
        }
    }
    if (const BookProject* project = m_book->project()) {
        for (const OwnRelation& own : project->workshop.relations) {
            if (own.kind == kind && own.otherKind == otherKind) {
                relations << infoOf(own);
            }
        }
    }
    return relations;
}

std::optional<RelationInfo> CardRelations::offered(const QString& cardId, const QString& otherId,
                                                   const QString& relationId) const {
    const ProjectElement* card = m_book->findElement(cardId);
    const ProjectElement* other = m_book->findElement(otherId);
    if (card == nullptr || other == nullptr || cardId == otherId || !m_book->isCard(*card) ||
        !m_book->isCard(*other)) {
        return std::nullopt;
    }
    const QList<RelationInfo> relations = between(*card, *other);
    const auto found = std::find_if(relations.cbegin(), relations.cend(),
                                    [&relationId](const RelationInfo& each) {
                                        return each.id == relationId;
                                    });
    return found == relations.cend() ? std::nullopt : std::optional<RelationInfo>(*found);
}

std::optional<FieldChange> CardRelations::fieldChange(const ProjectElement& card,
                                                      const QString& fieldId,
                                                      const QString& text) const {
    const KindRef kind = m_book->kindOf(card);
    const CardField* field = m_book->cardField(kind, fieldId);
    if (field == nullptr || field->relation.isEmpty() || kind.package == nullptr) {
        return std::nullopt;
    }
    const std::optional<RelationInfo> relation = find(field->relation, kind.reference());
    if (!relation) {
        return std::nullopt;
    }
    FieldChange change;
    change.relation = *relation;
    if (const ProjectElement* before = fieldCard(card, *field)) {
        change.before = before->id;
    }
    const QString cardKind = kind.package->id + QLatin1Char(':') + field->cardKind;
    if (const ProjectElement* named = cardTitled(text, cardKind)) {
        change.named = named->id;
    } else {
        change.value = text.trimmed();
    }
    return change;
}

QString CardRelations::labelOf(const QString& relationId, const QString& kind,
                               const ProjectElement& other, const QString& language) const {
    const std::optional<RelationInfo> relation = find(relationId, kind);
    return relation ? relation->nameFor(genderOf(other), language) : relationId;
}

QList<ShownRelation> CardRelations::of(const QString& cardId) const {
    QList<ShownRelation> shown;
    const auto found = std::find_if(m_cards.cbegin(), m_cards.cend(),
                                    [&cardId](const ProjectElement* each) {
                                        return each->id == cardId;
                                    });
    if (found == m_cards.cend()) {
        return shown;
    }
    const ProjectElement& card = **found;
    const auto isCardId = [this](const QString& id) {
        return std::any_of(m_cards.cbegin(), m_cards.cend(),
                           [&id](const ProjectElement* each) { return each->id == id; });
    };
    for (const ElementLink& link : card.links) {
        if (isCardId(link.to)) {
            shown << ShownRelation{link.to, link.role, link.note, card.id, link.to, link.role};
        }
    }
    // The links of the other cards with it, as their inverse relations
    for (const ProjectElement* other : m_cards) {
        for (const ElementLink& link : other->links) {
            if (link.to != cardId || other->id == cardId) {
                continue;
            }
            const std::optional<RelationInfo> relation = find(link.role, kindOf(*other));
            shown << ShownRelation{other->id, relation ? relation->inverse : link.role, link.note,
                                   other->id, cardId, link.role};
        }
    }
    return shown;
}

const ProjectElement* CardRelations::fieldCard(const ProjectElement& card,
                                               const CardField& field) const {
    if (field.relation.isEmpty()) {
        return nullptr;
    }
    for (const ShownRelation& relation : of(card.id)) {
        if (relation.relation == field.relation) {
            return m_book->findElement(relation.otherId);
        }
    }
    return nullptr;
}

const CardField* CardRelations::treeField(const ElementKind& kind) {
    const auto found =
        std::find_if(kind.fields.cbegin(), kind.fields.cend(), [&kind](const CardField& field) {
            return field.type == CardFieldType::Card && field.cardKind == kind.id &&
                   !field.relation.isEmpty();
        });
    return found == kind.fields.cend() ? nullptr : &*found;
}

QHash<QString, QString> CardRelations::treeOf(const QList<const ProjectElement*>& cards,
                                              const CardField& field) const {
    QSet<QString> ids;
    for (const ProjectElement* card : cards) {
        ids.insert(card->id);
    }
    QHash<QString, QString> above;
    for (const ProjectElement* card : cards) {
        const ProjectElement* named = fieldCard(*card, field);
        if (named != nullptr && named->id != card->id && ids.contains(named->id)) {
            above.insert(card->id, named->id);
        }
    }

    for (const QString& id : inCircles(above)) {
        above.remove(id);
    }
    return above;
}

const ProjectElement* CardRelations::cardTitled(const QString& title, const QString& kind) const {
    const QString wanted = title.trimmed();
    if (wanted.isEmpty()) {
        return nullptr;
    }
    const auto found = std::find_if(m_cards.cbegin(), m_cards.cend(),
                                    [this, &wanted, &kind](const ProjectElement* each) {
                                        return kindOf(*each) == kind &&
                                               each->title.trimmed().compare(
                                                   wanted, Qt::CaseInsensitive) == 0;
                                    });
    return found == m_cards.cend() ? nullptr : *found;
}

QString CardRelations::kindOf(const ProjectElement& card) const {
    return m_book->kindOf(card).reference();
}

QString CardRelations::genderOf(const ProjectElement& card) {
    return card.card.value(QString::fromLatin1(CardRelation::GENDER_FIELD));
}

namespace card_links {

namespace {

/// The elements of the Workshop of @p project and of the groups inside it
QList<ProjectElement*> workshopOf(BookProject& project) {
    QList<ProjectElement*> all;
    QList<ProjectElement*> waiting;
    for (ProjectElement& element : project.workshop.elements) {
        waiting << &element;
    }
    while (!waiting.isEmpty()) {
        ProjectElement* element = waiting.takeLast();
        all << element;
        for (ProjectElement& inside : element->elements) {
            waiting << &inside;
        }
    }
    return all;
}

/// The name of own relation @p id of @p relations; empty when there is none
QString ownName(const QList<OwnRelation>& relations, const QString& id) {
    const auto found = std::find_if(relations.cbegin(), relations.cend(),
                                    [&id](const OwnRelation& each) { return each.id == id; });
    return found == relations.cend() ? QString() : found->name;
}

/// The own relation of @p relations of a card of kind @p kind with a card of kind
/// @p otherKind named @p name, whose inverse is named @p inverseName; empty when there is none
QString sameOwn(const QList<OwnRelation>& relations, const QString& kind,
                const QString& otherKind, const QString& name, const QString& inverseName) {
    const auto found =
        std::find_if(relations.cbegin(), relations.cend(), [&](const OwnRelation& each) {
            return each.kind == kind && each.otherKind == otherKind && each.name == name &&
                   ownName(relations, each.inverse) == inverseName;
        });
    return found == relations.cend() ? QString() : found->id;
}

}  // namespace

void addRelation(BookProject& project, const QString& cardId, const QString& otherId,
                 const RelationInfo& relation, const QString& note) {
    ProjectElement* card = project.findElement(cardId);
    ProjectElement* other = project.findElement(otherId);
    if (card == nullptr || other == nullptr) {
        return;
    }
    const auto same = [](const QString& to, const QString& role) {
        return [to, role](const ElementLink& link) { return link.to == to && link.role == role; };
    };
    const auto own = std::find_if(card->links.begin(), card->links.end(),
                                  same(otherId, relation.id));
    if (own != card->links.end()) {
        own->note = note;
        return;
    }
    const auto inverse = std::find_if(other->links.begin(), other->links.end(),
                                      same(cardId, relation.inverse));
    if (inverse != other->links.end()) {
        inverse->note = note;
        return;
    }
    ElementLink link;
    link.to = otherId;
    link.role = relation.id;
    link.note = note;
    card->links << link;
}

void removeRelation(BookProject& project, const ShownRelation& relation) {
    if (ProjectElement* owner = project.findElement(relation.ownerId)) {
        owner->links.removeIf([&relation](const ElementLink& link) {
            return link.to == relation.linkTo && link.role == relation.linkRole;
        });
    }
}

void setFieldCard(BookProject& project, const QString& cardId, const RelationInfo& relation,
                  const QString& otherId) {
    for (ProjectElement* element : workshopOf(project)) {
        const bool isCard = element->id == cardId;
        element->links.removeIf([&cardId, &relation, isCard](const ElementLink& link) {
            return (isCard && link.role == relation.id) ||
                   (link.to == cardId && link.role == relation.inverse);
        });
    }
    if (ProjectElement* card = project.findElement(cardId); card != nullptr && !otherId.isEmpty()) {
        ElementLink link;
        link.to = otherId;
        link.role = relation.id;
        card->links << link;
    }
}

void setValue(BookProject& project, const QString& cardId, const QString& fieldId,
              const QString& value) {
    ProjectElement* card = project.findElement(cardId);
    if (card == nullptr) {
        return;
    }
    if (value.isEmpty()) {
        card->card.values.remove(fieldId);
    } else {
        card->card.values.insert(fieldId, value);
    }
}

QString ownRelation(QList<OwnRelation>& relations, const QString& kind, const QString& otherKind,
                    const QString& name, const QString& inverseName) {
    const QString forward = name.trimmed();
    const QString backward = inverseName.trimmed();
    if (const QString known = sameOwn(relations, kind, otherKind, forward, backward);
        !known.isEmpty()) {
        return known;
    }
    // The next free numbers
    int number = 1;
    const auto nextId = [&relations, &number]() {
        QString id = QLatin1String(OwnRelation::ID_PREFIX) + QString::number(number);
        while (!ownName(relations, id).isEmpty()) {
            id = QLatin1String(OwnRelation::ID_PREFIX) + QString::number(++number);
        }
        ++number;
        return id;
    };
    OwnRelation relation{nextId(), kind, otherKind, forward, QString(), {}};
    const bool same = kind == otherKind && forward == backward;
    relation.inverse = same ? relation.id : nextId();
    relations << relation;
    if (!same) {
        relations << OwnRelation{relation.inverse, otherKind, kind, backward, relation.id, {}};
    }
    return relation.id;
}

}  // namespace card_links

}  // namespace kalahari::core
