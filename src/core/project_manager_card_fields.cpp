// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file project_manager_card_fields.cpp
/// @brief ProjectManager: the fields of the cards of the open book, their sets and the own
/// fields of a card made fields of all the cards of its kind

#include "kalahari/core/project_manager.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/card_data.h"
#include "kalahari/core/card_field_set.h"
#include "kalahari/core/card_relations.h"

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <utility>

namespace kalahari::core {

namespace {

/// Each element of @p elements and of the groups inside them, with @p visit
void forEachElement(QList<ProjectElement>& elements,
                    const std::function<void(ProjectElement&)>& visit) {
    for (ProjectElement& element : elements) {
        visit(element);
        forEachElement(element.elements, visit);
    }
}

/// The relation of the writer @p id in @p relations; nullptr when there is none
OwnRelation* ownRelationOf(QList<OwnRelation>& relations, const QString& id) {
    const auto found = std::find_if(relations.begin(), relations.end(),
                                    [&id](const OwnRelation& each) { return each.id == id; });
    return found == relations.end() ? nullptr : &*found;
}

/// The links of the elements of @p project with relation @p before get relation @p after
void moveLinks(BookProject& project, const QString& before, const QString& after) {
    forEachElement(project.workshop.elements, [&before, &after](ProjectElement& element) {
        for (ElementLink& link : element.links) {
            if (link.role == before) {
                link.role = after;
            }
        }
    });
}

/// Whether relation @p relation can take the names @p name and @p inverse in place: of kinds
/// @p kind and @p otherKind, the same both ways only while the names are the same
bool renamesInPlace(const OwnRelation& relation, const QString& kind, const QString& otherKind,
                    const QString& name, const QString& inverse) {
    const bool bothWays = relation.inverse == relation.id;
    return relation.kind == kind && relation.otherKind == otherKind &&
           (!bothWays || name == inverse);
}

/// Give field @p field of the writer, which names a card, of the cards of kind @p kind of
/// package @p packageId, the relation of the writer named after it: its own relation renamed,
/// else one of the Workshop with these names, or a new one. The links of its relation move to
/// it while the kinds of the cards are the same; of cards of another kind, they stay where
/// they are, relations of the cards
void giveRelation(BookProject& project, const QString& kind, const QString& packageId,
                  BookCardField& field) {
    const QString otherKind = packageId + QLatin1Char(':') + field.cardKind;
    const QString name = field.name.trimmed();
    const QString inverse =
        field.inverseName.trimmed().isEmpty() ? name : field.inverseName.trimmed();
    QList<OwnRelation>& relations = project.workshop.relations;
    OwnRelation* mine = ownRelationOf(relations, field.relation);
    if (mine != nullptr && renamesInPlace(*mine, kind, otherKind, name, inverse)) {
        mine->name = name;
        if (OwnRelation* other = ownRelationOf(relations, mine->inverse); other != mine &&
                                                                          other != nullptr) {
            other->name = inverse;
        }
        return;
    }
    const bool sameKinds =
        mine != nullptr && mine->kind == kind && mine->otherKind == otherKind;
    // Not renamed in place, its relation is the same both ways: its links are those of the
    // cards on both sides
    const QString before = field.relation;
    field.relation = card_links::ownRelation(relations, kind, otherKind, name, inverse);
    if (sameKinds && before != field.relation) {
        moveLinks(project, before, field.relation);
    }
}

/// The ids of the values of the cards of kind @p kind in @p project: those of the fields that
/// went stay, so a new field does not take them
QStringList valueIds(const BookProject& project, const KindReference& kind) {
    QSet<QString> ids;
    std::function<void(const QList<ProjectElement>&)> visit =
        [&](const QList<ProjectElement>& elements) {
            for (const ProjectElement& element : elements) {
                if (element.kind == kind) {
                    for (auto it = element.card.values.cbegin(); it != element.card.values.cend();
                         ++it) {
                        ids.insert(it.key());
                    }
                }
                visit(element.elements);
            }
        };
    visit(project.workshop.elements);
    return {ids.cbegin(), ids.cend()};
}

/// Whether field @p field of the cards of kind @p kind is named @p name in any language, and
/// takes a text
bool isTextFieldNamed(const ElementKind& kind, const BookCardField& field, const QString& name) {
    if (field.isOwn()) {
        return (field.type == CardFieldType::Text || field.type == CardFieldType::LongText) &&
               field.name.compare(name, Qt::CaseInsensitive) == 0;
    }
    const CardField* ofKind = kind.field(field.id);
    if (ofKind == nullptr ||
        (ofKind->type != CardFieldType::Text && ofKind->type != CardFieldType::LongText)) {
        return false;
    }
    const QList<QString> names = ofKind->name.values.values();
    return std::any_of(names.cbegin(), names.cend(), [&name](const QString& each) {
        return each.compare(name, Qt::CaseInsensitive) == 0;
    });
}

/// The own field named @p name of each card of kind @p kind gives its value to field
/// @p fieldId of the card, and goes; the cards changed
QStringList takeOwnFields(BookProject& project, const KindReference& kind, const QString& name,
                          const QString& fieldId) {
    QStringList changed;
    forEachElement(project.workshop.elements, [&](ProjectElement& element) {
        if (!(element.kind == kind)) {
            return;
        }
        for (qsizetype i = element.fields.size() - 1; i >= 0; --i) {
            const ElementField& own = element.fields.at(i);
            if (own.name.trimmed().compare(name, Qt::CaseInsensitive) != 0) {
                continue;
            }
            if (element.card.values.value(fieldId).isEmpty() && !own.value.isEmpty()) {
                element.card.values.insert(fieldId, own.value);
            }
            element.fields.removeAt(i);
            changed << element.id;
        }
    });
    return changed;
}

}  // namespace

const QList<CardField>& ProjectManager::cardFields(const KindRef& kind) const {
    static const QList<CardField> none;
    if (!kind || !kind.kind->isCard()) {
        return none;
    }
    const QString reference = kind.reference();
    const QList<BookCardField> set =
        m_project ? m_project->workshop.cardFields.value(reference) : QList<BookCardField>();
    ShownCardFields& shown = m_cardFields[reference];
    if (shown.kind != kind.kind || shown.set != set || shown.fields.isEmpty()) {
        shown.kind = kind.kind;
        shown.set = set;
        shown.fields = card_fields::effective(*kind.kind, set);
    }
    return shown.fields;
}

const CardField* ProjectManager::cardField(const KindRef& kind, const QString& fieldId) const {
    const QList<CardField>& fields = cardFields(kind);
    const auto found = std::find_if(fields.cbegin(), fields.cend(),
                                    [&fieldId](const CardField& each) {
                                        return each.id == fieldId;
                                    });
    return found == fields.cend() ? nullptr : &*found;
}

const CardField* ProjectManager::subtitleField(const KindRef& kind) const {
    const QList<CardField>& fields = cardFields(kind);
    const auto found =
        std::find_if(fields.cbegin(), fields.cend(), [](const CardField& each) {
            return each.subtitle && (each.onCard || each.inProperties || each.inList);
        });
    return found == fields.cend() ? nullptr : &*found;
}

QList<BookCardField> ProjectManager::cardFieldSet(const KindRef& kind) const {
    if (!kind || !kind.kind->isCard()) {
        return {};
    }
    const QList<BookCardField> set =
        m_project ? m_project->workshop.cardFields.value(kind.reference())
                  : QList<BookCardField>();
    return card_fields::complete(*kind.kind, set);
}

bool ProjectManager::hasOwnCardFields(const KindRef& kind) const {
    return kind && m_project && m_project->workshop.cardFields.contains(kind.reference());
}

bool ProjectManager::setCardFields(const KindRef& kind, const QList<BookCardField>& fields) {
    if (!kind || !kind.kind->isCard() || kind.package == nullptr || !m_project) {
        return false;
    }
    const QString reference = kind.reference();
    const QList<BookCardField> before = m_project->workshop.cardFields.value(reference);
    // The fields of the package need no set, unless the book keeps fields of a newer one
    const bool ofPackage = card_fields::isKindSet(*kind.kind, fields);
    QList<BookCardField> set = card_fields::keepUnknown(
        *kind.kind, before,
        ofPackage ? card_fields::ofKind(*kind.kind) : card_fields::complete(*kind.kind, fields));
    if (ofPackage && set.size() == kind.kind->fields.size()) {
        set.clear();
    }
    const bool saved = changeProject("setCardFields", [&](BookProject& project) {
        for (BookCardField& field : set) {
            if (field.isOwn() && field.type == CardFieldType::Card) {
                giveRelation(project, reference, kind.package->id, field);
            }
        }
        if (set.isEmpty()) {
            project.workshop.cardFields.remove(reference);
        } else {
            project.workshop.cardFields.insert(reference, set);
        }
    });
    if (saved) {
        emit cardFieldsChanged(reference);
        emit linksChanged();
    }
    return saved;
}

QString ProjectManager::addFieldToKind(const QString& cardId, int index) {
    const ProjectElement* card = findElement(cardId);
    const KindRef kind = card != nullptr ? kindOf(*card) : KindRef();
    if (!kind || !kind.kind->isCard() || index < 0 || index >= card->fields.size()) {
        return {};
    }
    const QString name = card->fields.at(index).name.trimmed();
    if (name.isEmpty()) {
        return {};
    }
    QList<BookCardField> set = cardFieldSet(kind);
    const auto named = std::find_if(set.cbegin(), set.cend(),
                                    [&kind, &name](const BookCardField& field) {
                                        return isTextFieldNamed(*kind.kind, field, name);
                                    });
    QString fieldId;
    if (named != set.cend()) {
        // Shown on the card, with the values it takes
        BookCardField& taking = set[std::distance(set.cbegin(), named)];
        taking.onCard = true;
        fieldId = taking.id;
    } else {
        BookCardField field;
        field.id = card_fields::newOwnId(set, valueIds(*m_project, card->kind));
        field.name = name;
        field.type = card->fields.at(index).value.contains(QLatin1Char('\n'))
                         ? CardFieldType::LongText
                         : CardFieldType::Text;
        field.onCard = true;
        set << field;
        fieldId = field.id;
    }
    QStringList changed;
    const QString reference = kind.reference();
    const KindReference cardKind = card->kind;
    const bool saved = changeProject("addFieldToKind", [&](BookProject& project) {
        project.workshop.cardFields.insert(reference, set);
        changed = takeOwnFields(project, cardKind, name, fieldId);
    });
    if (!saved) {
        return {};
    }
    emit cardFieldsChanged(reference);
    for (const QString& id : std::as_const(changed)) {
        emit cardChanged(id);
    }
    return fieldId;
}

void ProjectManager::giveDefaultCardFields(BookProject& project) {
    const CardFieldSets defaults = card_fields::defaults();
    for (auto it = defaults.cbegin(); it != defaults.cend(); ++it) {
        const std::optional<KindReference> kind = KindReference::parse(it.key());
        if (!kind || kind->packageId.isEmpty()) {
            continue;
        }
        QList<BookCardField> set = it.value();
        for (BookCardField& field : set) {
            if (field.isOwn() && field.type == CardFieldType::Card) {
                field.relation.clear();
                giveRelation(project, it.key(), kind->packageId, field);
            }
        }
        project.workshop.cardFields.insert(it.key(), set);
    }
}

}  // namespace kalahari::core
