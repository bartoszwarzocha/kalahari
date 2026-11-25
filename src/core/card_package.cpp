// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file card_package.cpp
/// @brief What a package of book types says of the cards: reading the fields of the kinds of
/// cards and the relations between the cards

#include "kalahari/core/card_package.h"

#include "kalahari/core/book_type_package.h"
#include "kalahari/core/link_roles.h"
#include "kalahari/core/package_json.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <optional>

namespace kalahari::core {

namespace {

using package_json::addProblem;
using package_json::checkKeys;
using package_json::isIdentifier;
using package_json::readIdList;
using package_json::readText;

/// The values of a field of the cards with a list: [{"id": "main", "name": {...}}]
QList<CardFieldChoice> readChoices(const QJsonValue& value, const QString& field,
                                   QStringList& problems) {
    QList<CardFieldChoice> choices;
    if (!value.isArray() || value.toArray().isEmpty()) {
        addProblem(problems, field,
                   QStringLiteral("a field with a list needs the values to choose from, e.g. "
                                  "[{\"id\": \"main\", \"name\": {\"en\": \"main\"}}]"));
        return choices;
    }
    const QJsonArray array = value.toArray();
    for (qsizetype i = 0; i < array.size(); ++i) {
        const QString choiceField = QStringLiteral("%1[%2]").arg(field).arg(i);
        const QJsonObject object = array.at(i).toObject();
        if (!array.at(i).isObject()) {
            addProblem(problems, choiceField, QStringLiteral("must be an object"));
            continue;
        }
        checkKeys(object, choiceField, {"id", "name"}, problems);
        CardFieldChoice choice;
        choice.id = object.value(QStringLiteral("id")).toString();
        choice.name = readText(object.value(QStringLiteral("name")),
                               choiceField + QStringLiteral(".name"), true, problems);
        const bool taken = std::any_of(choices.cbegin(), choices.cend(),
                                       [&choice](const CardFieldChoice& each) {
                                           return each.id == choice.id;
                                       });
        if (!isIdentifier(choice.id)) {
            addProblem(problems, choiceField + QStringLiteral(".id"),
                       QStringLiteral("must be an id: lowercase letters, digits and _"));
        } else if (taken) {
            addProblem(problems, choiceField + QStringLiteral(".id"),
                       QStringLiteral("'%1' is listed twice").arg(choice.id));
        } else {
            choices << choice;
        }
    }
    return choices;
}

/// Where a field of the cards is shown: ["card", "properties", "list"]
void readShown(const QJsonValue& value, const QString& field, CardField& cardField,
               QStringList& problems) {
    if (value.isUndefined()) {
        return;
    }
    const QStringList places =
        readIdList(value, field, isIdentifier, QStringLiteral("place of a field"), true, problems);
    for (const QString& place : places) {
        if (place == QLatin1String("card")) {
            cardField.onCard = true;
        } else if (place == QLatin1String("properties")) {
            cardField.inProperties = true;
        } else if (place == QLatin1String("list")) {
            cardField.inList = true;
        } else {
            addProblem(problems, field,
                       QStringLiteral("'%1' is not \"card\", \"properties\" or \"list\"")
                           .arg(place));
        }
    }
}

/// The type of a field of the cards: "text" when it says none
void readFieldType(const QJsonValue& value, const QString& field, CardField& cardField,
                   QStringList& problems) {
    const auto type = value.isUndefined() ? std::optional<CardFieldType>(CardFieldType::Text)
                                          : CardField::typeFromName(value.toString());
    if (type) {
        cardField.type = *type;
        return;
    }
    addProblem(problems, field,
               QStringLiteral("must be \"text\", \"longText\", \"choice\" or \"card\""));
}

/// Whether a field of the cards is the subtitle of the card: true or false
void readSubtitle(const QJsonValue& value, const QString& field, CardField& cardField,
                  QStringList& problems) {
    if (!value.isUndefined() && !value.isBool()) {
        addProblem(problems, field, QStringLiteral("must be true or false"));
    }
    cardField.subtitle = value.toBool();
}

/// The values to choose from, which only a field of the type "choice" has
void readFieldChoices(const QJsonValue& value, const QString& field, CardField& cardField,
                      QStringList& problems) {
    if (cardField.type == CardFieldType::Choice) {
        cardField.choices = readChoices(value, field, problems);
    } else if (!value.isUndefined()) {
        addProblem(problems, field,
                   QStringLiteral("only a field of the type \"choice\" has values to choose "
                                  "from"));
    }
}

/// The kind of the cards that a field of the type "card" names, which only such a field has
void readCardKind(const QJsonValue& value, const QString& field, CardField& cardField,
                  QStringList& problems) {
    if (cardField.type != CardFieldType::Card) {
        if (!value.isUndefined()) {
            addProblem(problems, field,
                       QStringLiteral("only a field of the type \"card\" names a kind of "
                                      "cards"));
        }
        return;
    }
    if (!value.isString() || !isIdentifier(value.toString())) {
        addProblem(problems, field,
                   QStringLiteral("a field of the type \"card\" needs the id of the kind of the "
                                  "cards it names, e.g. \"location\""));
        return;
    }
    cardField.cardKind = value.toString();
}

/// The relation that the card a field of the type "card" names has with the card, which only
/// such a field has
void readFieldRelation(const QJsonValue& value, const QString& field, CardField& cardField,
                       QStringList& problems) {
    if (value.isUndefined()) {
        return;
    }
    if (cardField.type != CardFieldType::Card) {
        addProblem(problems, field,
                   QStringLiteral("only a field of the type \"card\" makes a relation"));
    } else if (!value.isString() || !isIdentifier(value.toString())) {
        addProblem(problems, field,
                   QStringLiteral("must be the id of a relation of the package, e.g. \"owner\""));
    } else {
        cardField.relation = value.toString();
    }
}

/// A field of the cards of a kind
std::optional<CardField> readCardField(const QJsonValue& value, const QString& field,
                                       QStringList& problems) {
    if (!value.isObject()) {
        addProblem(problems, field, QStringLiteral("must be an object"));
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();
    checkKeys(object, field,
              {"id", "type", "name", "show", "subtitle", "choices", "card", "relation"},
              problems);
    CardField cardField;
    cardField.id = object.value(QStringLiteral("id")).toString();
    if (!isIdentifier(cardField.id)) {
        addProblem(problems, field + QStringLiteral(".id"),
                   QStringLiteral("must be an id: lowercase letters, digits and _"));
        return std::nullopt;
    }
    cardField.name =
        readText(object.value(QStringLiteral("name")), field + QStringLiteral(".name"), true,
                 problems);
    readFieldType(object.value(QStringLiteral("type")), field + QStringLiteral(".type"),
                  cardField, problems);
    readShown(object.value(QStringLiteral("show")), field + QStringLiteral(".show"), cardField,
              problems);
    readSubtitle(object.value(QStringLiteral("subtitle")), field + QStringLiteral(".subtitle"),
                 cardField, problems);
    readFieldChoices(object.value(QStringLiteral("choices")), field + QStringLiteral(".choices"),
                     cardField, problems);
    readCardKind(object.value(QStringLiteral("card")), field + QStringLiteral(".card"), cardField,
                 problems);
    readFieldRelation(object.value(QStringLiteral("relation")),
                      field + QStringLiteral(".relation"), cardField, problems);
    return cardField;
}

/// The names of a relation by the gender of the other card: {"woman": {...}, "man": {...}}
QMap<QString, LocalizedText> readForms(const QJsonValue& value, const QString& field,
                                       QStringList& problems) {
    QMap<QString, LocalizedText> forms;
    if (value.isUndefined()) {
        return forms;
    }
    if (!value.isObject()) {
        addProblem(problems, field,
                   QStringLiteral("must be names by the gender of the other card, e.g. "
                                  "{\"woman\": {\"en\": \"sister\"}}"));
        return forms;
    }
    const QJsonObject object = value.toObject();
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        const QString formField = field + QLatin1Char('.') + it.key();
        if (!isIdentifier(it.key())) {
            addProblem(problems, formField,
                       QStringLiteral("must be the id of a value of the field \"%1\"")
                           .arg(QLatin1String(CardRelation::GENDER_FIELD)));
            continue;
        }
        forms.insert(it.key(), readText(it.value(), formField, true, problems));
    }
    return forms;
}

/// A kind of the cards of a relation: "character", or "package:kind"
KindReference readRelationKind(const QJsonValue& value, const QString& field,
                               QStringList& problems) {
    const std::optional<KindReference> kind = KindReference::parse(value.toString());
    if (!value.isString() || !kind) {
        addProblem(problems, field,
                   QStringLiteral("must be the id of a kind of cards, e.g. \"character\""));
        return {};
    }
    return *kind;
}

/// The inverse of relation @p id: the relation the other card has; its own id when missing
QString readInverse(const QJsonValue& value, const QString& field, const QString& id,
                    QStringList& problems) {
    if (value.isUndefined()) {
        return id;
    }
    if (!value.isString() || !isIdentifier(value.toString())) {
        addProblem(problems, field, QStringLiteral("must be the id of a relation"));
        return id;
    }
    return value.toString();
}

/// A relation between cards
std::optional<CardRelation> readRelation(const QJsonValue& value, const QString& field,
                                         QStringList& problems) {
    if (!value.isObject()) {
        addProblem(problems, field, QStringLiteral("must be an object"));
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();
    checkKeys(object, field,
              {"id", "kind", "other", "inverse", "group", "choice", "name", "forms"}, problems);
    CardRelation relation;
    relation.id = object.value(QStringLiteral("id")).toString();
    if (!isIdentifier(relation.id)) {
        addProblem(problems, field + QStringLiteral(".id"),
                   QStringLiteral("must be an id: lowercase letters, digits and _"));
        return std::nullopt;
    }
    relation.kind =
        readRelationKind(object.value(QStringLiteral("kind")), field + QStringLiteral(".kind"),
                         problems);
    relation.otherKind = readRelationKind(object.value(QStringLiteral("other")),
                                          field + QStringLiteral(".other"), problems);
    relation.inverse = readInverse(object.value(QStringLiteral("inverse")),
                                   field + QStringLiteral(".inverse"), relation.id, problems);
    relation.group = readText(object.value(QStringLiteral("group")),
                              field + QStringLiteral(".group"), false, problems);
    relation.choice = readText(object.value(QStringLiteral("choice")),
                               field + QStringLiteral(".choice"), false, problems);
    relation.name = readText(object.value(QStringLiteral("name")),
                             field + QStringLiteral(".name"), true, problems);
    relation.forms = readForms(object.value(QStringLiteral("forms")),
                               field + QStringLiteral(".forms"), problems);
    return relation;
}

/// Whether @p kind names kind @p kindId of package @p packageId: by its id alone in that
/// package, or with that package
bool isKind(const KindReference& kind, const QString& packageId, const QString& kindId) {
    return (kind.packageId.isEmpty() || kind.packageId == packageId) && kind.kindId == kindId;
}

/// Whether @p kind names a kind of the cards of @p package, or a kind of another package,
/// which the program does not check here
bool namesCards(const KindReference& kind, const BookTypePackage& package) {
    if (!kind.packageId.isEmpty() && kind.packageId != package.id) {
        return true;
    }
    const ElementKind* own = package.ownKind(kind.kindId);
    return own != nullptr && own->isCard();
}

/// The problems of the names of relation @p relation (@p field) by the gender of the other
/// card: each a value of the field CardRelation::GENDER_FIELD of the other kind, when the
/// package has that kind
void checkForms(const CardRelation& relation, const BookTypePackage& package,
                const QString& field, QStringList& problems) {
    const KindReference& other = relation.otherKind;
    const ElementKind* kind = other.packageId.isEmpty() || other.packageId == package.id
                                  ? package.ownKind(other.kindId)
                                  : nullptr;
    if (kind == nullptr) {
        return;
    }
    const CardField* gender = kind->field(QString::fromLatin1(CardRelation::GENDER_FIELD));
    for (auto it = relation.forms.cbegin(); it != relation.forms.cend(); ++it) {
        const QString& value = it.key();
        const bool known =
            gender != nullptr &&
            std::any_of(gender->choices.cbegin(), gender->choices.cend(),
                        [&value](const CardFieldChoice& choice) { return choice.id == value; });
        if (!known) {
            addProblem(problems, field + QStringLiteral(".forms.") + value,
                       QStringLiteral("'%1' is no value of the field \"%2\" of kind '%3'")
                           .arg(value, QLatin1String(CardRelation::GENDER_FIELD), kind->id));
        }
    }
}

/// The problems of relation @p relation of @p package (@p field): its kinds and its inverse
void checkRelation(const CardRelation& relation, const BookTypePackage& package,
                   const QString& field, QStringList& problems) {
    for (const KindReference* kind : {&relation.kind, &relation.otherKind}) {
        if (!kind->kindId.isEmpty() && !namesCards(*kind, package)) {
            addProblem(problems, field,
                       QStringLiteral("'%1' is no kind of cards of the package")
                           .arg(kind->toString()));
        }
    }
    checkForms(relation, package, field, problems);
    const CardRelation* inverse = package.relation(relation.inverse);
    if (inverse == nullptr) {
        addProblem(problems, field + QStringLiteral(".inverse"),
                   QStringLiteral("'%1' is no relation of the package").arg(relation.inverse));
        return;
    }
    const bool reversed = isKind(inverse->kind, package.id, relation.otherKind.kindId) &&
                          isKind(inverse->otherKind, package.id, relation.kind.kindId);
    if (!reversed || inverse->inverse != relation.id) {
        addProblem(problems, field + QStringLiteral(".inverse"),
                   QStringLiteral("'%1' must be the relation between the same kinds the other "
                                  "way round, with '%2' as its inverse")
                       .arg(relation.inverse, relation.id));
    }
}

/// The problems of field @p cardField of kind @p kind (@p field) that makes a relation
void checkFieldRelation(const CardField& cardField, const ElementKind& kind,
                        const BookTypePackage& package, const QString& field,
                        QStringList& problems) {
    const CardRelation* relation = package.relation(cardField.relation);
    const bool fits = relation != nullptr && isKind(relation->kind, package.id, kind.id) &&
                      isKind(relation->otherKind, package.id, cardField.cardKind);
    if (!fits) {
        addProblem(problems, field,
                   QStringLiteral("'%1' must be a relation of the package between the kinds "
                                  "'%2' and '%3'")
                       .arg(cardField.relation, kind.id, cardField.cardKind));
    }
}

}  // namespace

namespace card_package {

void readCardKind(const QJsonObject& object, const QString& field, ElementKind& kind,
                  QStringList& problems) {
    const QJsonValue fields = object.value(QStringLiteral("fields"));
    if (!fields.isUndefined()) {
        if (kind.isCard()) {
            kind.fields = readCardFields(fields, field + QStringLiteral(".fields"), problems);
        } else {
            addProblem(problems, field + QStringLiteral(".fields"),
                       QStringLiteral("only a kind of cards (a text kind with the editor "
                                      "\"card\") has fields"));
        }
    }
    const QJsonValue roles = object.value(QStringLiteral("chapterRoles"));
    if (roles.isUndefined()) {
        return;
    }
    const QString rolesField = field + QStringLiteral(".chapterRoles");
    if (!kind.isCard()) {
        addProblem(problems, rolesField,
                   QStringLiteral("only a kind of cards (a text kind with the editor \"card\") "
                                  "has roles in the chapters"));
        return;
    }
    kind.chapterRoles = readIdList(
        roles, rolesField,
        [](const QString& id) { return LinkRoles::ofCards().contains(id); },
        QStringLiteral("chapter role"), true, problems);
    if (roles.isArray() && roles.toArray().isEmpty()) {
        addProblem(problems, rolesField,
                   QStringLiteral("needs a role at least; without it, a card appears or is "
                                  "mentioned"));
    }
}

QList<CardField> readCardFields(const QJsonValue& value, const QString& field,
                                QStringList& problems) {
    QList<CardField> fields;
    if (!value.isArray()) {
        addProblem(problems, field, QStringLiteral("must be a list of fields"));
        return fields;
    }
    const QJsonArray array = value.toArray();
    for (qsizetype i = 0; i < array.size(); ++i) {
        const QString fieldName = QStringLiteral("%1[%2]").arg(field).arg(i);
        std::optional<CardField> cardField = readCardField(array.at(i), fieldName, problems);
        if (!cardField) {
            continue;
        }
        const QString& fieldId = cardField->id;
        if (std::any_of(fields.cbegin(), fields.cend(),
                        [&fieldId](const CardField& each) { return each.id == fieldId; })) {
            addProblem(problems, fieldName + QStringLiteral(".id"),
                       QStringLiteral("'%1' is listed twice").arg(fieldId));
            continue;
        }
        if (cardField->subtitle &&
            std::any_of(fields.cbegin(), fields.cend(),
                        [](const CardField& each) { return each.subtitle; })) {
            addProblem(problems, fieldName + QStringLiteral(".subtitle"),
                       QStringLiteral("only one field of a kind is the subtitle"));
            cardField->subtitle = false;
        }
        fields << *cardField;
    }
    return fields;
}

QList<CardRelation> readRelations(const QJsonValue& value, const QString& field,
                                  QStringList& problems) {
    QList<CardRelation> relations;
    if (value.isUndefined()) {
        return relations;
    }
    if (!value.isArray()) {
        addProblem(problems, field, QStringLiteral("must be a list of relations"));
        return relations;
    }
    const QJsonArray array = value.toArray();
    for (qsizetype i = 0; i < array.size(); ++i) {
        const QString relationField = QStringLiteral("%1[%2]").arg(field).arg(i);
        std::optional<CardRelation> relation = readRelation(array.at(i), relationField, problems);
        if (!relation) {
            continue;
        }
        const QString& id = relation->id;
        if (std::any_of(relations.cbegin(), relations.cend(),
                        [&id](const CardRelation& each) { return each.id == id; })) {
            addProblem(problems, relationField + QStringLiteral(".id"),
                       QStringLiteral("'%1' is listed twice").arg(id));
            continue;
        }
        relations << *relation;
    }
    return relations;
}

void checkRelations(const BookTypePackage& package, QStringList& problems) {
    for (qsizetype i = 0; i < package.relations.size(); ++i) {
        checkRelation(package.relations.at(i), package, QStringLiteral("relations[%1]").arg(i),
                      problems);
    }
    for (const ElementKind& kind : package.kinds) {
        for (qsizetype i = 0; i < kind.fields.size(); ++i) {
            const CardField& cardField = kind.fields.at(i);
            if (!cardField.relation.isEmpty()) {
                checkFieldRelation(cardField, kind, package,
                                   QStringLiteral("kinds.%1.fields[%2].relation")
                                       .arg(kind.id)
                                       .arg(i),
                                   problems);
            }
        }
    }
}

}  // namespace card_package

QString CardRelation::nameFor(const QString& gender, const QString& language) const {
    const auto form = forms.constFind(gender);
    return form != forms.cend() && !form->isEmpty() ? form->text(language) : name.text(language);
}

QString CardRelation::choiceText(const QString& language) const {
    return choice.isEmpty() ? name.text(language) : choice.text(language);
}

const CardRelation* BookTypePackage::relation(const QString& relationId) const {
    const auto found =
        std::find_if(relations.cbegin(), relations.cend(),
                     [&relationId](const CardRelation& each) { return each.id == relationId; });
    return found == relations.cend() ? nullptr : &*found;
}

}  // namespace kalahari::core
