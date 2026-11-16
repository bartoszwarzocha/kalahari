// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_field_set.cpp
/// @brief The fields of the cards of a kind as a book has them

#include "kalahari/core/card_field_set.h"

#include "kalahari/core/settings_manager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <optional>
#include <string>

namespace kalahari::core::card_fields {

namespace {

/// The places a field is shown in, as the packages name them
constexpr const char* ON_CARD = "card";
constexpr const char* IN_PROPERTIES = "properties";
constexpr const char* IN_LIST = "list";

/// The start of the ids of the values of a field of the writer with a list: "c1"
constexpr const char* CHOICE_PREFIX = "c";

/// The keys of an entry that this version reads; the others are kept in its extra
const QStringList& knownKeys() {
    static const QStringList keys{
        QStringLiteral("id"),   QStringLiteral("show"),     QStringLiteral("name"),
        QStringLiteral("type"), QStringLiteral("choices"),  QStringLiteral("card"),
        QStringLiteral("relation"), QStringLiteral("inverse")};
    return keys;
}

/// Where @p field is shown, from ["card", "properties", "list"]
void readShown(const QJsonValue& value, BookCardField& field) {
    for (const QJsonValue& place : value.toArray()) {
        const QString name = place.toString();
        field.onCard = field.onCard || name == QLatin1String(ON_CARD);
        field.inProperties = field.inProperties || name == QLatin1String(IN_PROPERTIES);
        field.inList = field.inList || name == QLatin1String(IN_LIST);
    }
}

/// ["card", "properties", "list"] of where @p field is shown
QJsonArray shownOf(const BookCardField& field) {
    QJsonArray places;
    if (field.onCard) {
        places.append(QLatin1String(ON_CARD));
    }
    if (field.inProperties) {
        places.append(QLatin1String(IN_PROPERTIES));
    }
    if (field.inList) {
        places.append(QLatin1String(IN_LIST));
    }
    return places;
}

/// The values of a field of the writer with a list: [{"id": "c1", "name": "elf"}]
QList<CardFieldChoice> readChoices(const QJsonValue& value) {
    QList<CardFieldChoice> choices;
    for (const QJsonValue& each : value.toArray()) {
        const QJsonObject object = each.toObject();
        const QString id = object.value(QStringLiteral("id")).toString();
        const QString name = object.value(QStringLiteral("name")).toString().trimmed();
        if (!id.isEmpty() && !name.isEmpty()) {
            choices << CardFieldChoice{id, LocalizedText::plain(name)};
        }
    }
    return choices;
}

/// The values of @p choices as [{"id": "c1", "name": "elf"}]
QJsonArray choicesOf(const QList<CardFieldChoice>& choices) {
    QJsonArray array;
    for (const CardFieldChoice& choice : choices) {
        QJsonObject each;
        each.insert(QLatin1String("id"), choice.id);
        each.insert(QLatin1String("name"), choice.name.text(QString()));
        array.append(each);
    }
    return array;
}

/// What a field of the writer is: its name, its type and, by its type, its values or the kind
/// and the relation of the cards it names
void readOwn(const QJsonObject& object, BookCardField& field) {
    field.name = object.value(QStringLiteral("name")).toString().trimmed();
    field.type = CardField::typeFromName(object.value(QStringLiteral("type")).toString())
                     .value_or(CardFieldType::Text);
    if (field.type == CardFieldType::Choice) {
        field.choices = readChoices(object.value(QStringLiteral("choices")));
    } else if (field.type == CardFieldType::Card) {
        field.cardKind = object.value(QStringLiteral("card")).toString();
        field.relation = object.value(QStringLiteral("relation")).toString();
        field.inverseName = object.value(QStringLiteral("inverse")).toString().trimmed();
    }
}

/// An entry of a set; nullopt for one without an id, or a field of the writer without a name
std::optional<BookCardField> readEntry(const QJsonValue& value) {
    const QJsonObject object = value.toObject();
    BookCardField field;
    field.id = object.value(QStringLiteral("id")).toString();
    if (field.id.isEmpty()) {
        return std::nullopt;
    }
    readShown(object.value(QStringLiteral("show")), field);
    if (field.isOwn()) {
        readOwn(object, field);
        if (field.name.isEmpty()) {
            return std::nullopt;
        }
    }
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (!knownKeys().contains(it.key())) {
            field.extra.insert(it.key(), it.value());
        }
    }
    return field;
}

/// The entry of @p field
QJsonObject entryOf(const BookCardField& field) {
    QJsonObject entry = field.extra;
    entry.insert(QLatin1String("id"), field.id);
    entry.insert(QLatin1String("show"), shownOf(field));
    if (!field.isOwn()) {
        return entry;
    }
    entry.insert(QLatin1String("name"), field.name);
    entry.insert(QLatin1String("type"), CardField::typeName(field.type));
    if (field.type == CardFieldType::Choice) {
        entry.insert(QLatin1String("choices"), choicesOf(field.choices));
    } else if (field.type == CardFieldType::Card) {
        entry.insert(QLatin1String("card"), field.cardKind);
        if (!field.relation.isEmpty()) {
            entry.insert(QLatin1String("relation"), field.relation);
        }
        if (!field.inverseName.isEmpty()) {
            entry.insert(QLatin1String("inverse"), field.inverseName);
        }
    }
    return entry;
}

/// The field of the cards that entry @p field of a set gives, with field @p ofKind of the kind
/// (nullptr: a field of the writer)
CardField cardFieldOf(const BookCardField& field, const CardField* ofKind) {
    CardField shown;
    if (ofKind != nullptr) {
        shown = *ofKind;
    } else {
        shown.id = field.id;
        shown.name = LocalizedText::plain(field.name);
        shown.type = field.type;
        shown.choices = field.choices;
        shown.cardKind = field.cardKind;
        shown.relation = field.relation;
    }
    shown.onCard = field.onCard;
    shown.inProperties = field.inProperties;
    shown.inList = field.inList;
    return shown;
}

/// The number after @p prefix in @p id: "own-12" -> 12; 0 for another id
int numberOf(const QString& id, const QString& prefix) {
    if (!id.startsWith(prefix)) {
        return 0;
    }
    bool isNumber = false;
    const int number = id.mid(prefix.size()).toInt(&isNumber);
    return isNumber ? number : 0;
}

/// A JSON value of the program's settings as Qt's, and back
QJsonValue toQt(const nlohmann::json& value) {
    const QByteArray text = QByteArray::fromStdString(value.dump());
    const QJsonDocument document = QJsonDocument::fromJson(text);
    return document.isObject() ? QJsonValue(document.object()) : QJsonValue();
}

nlohmann::json fromQt(const QJsonObject& object) {
    const QByteArray text = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return nlohmann::json::parse(text.toStdString(), nullptr, false);
}

}  // namespace

QList<BookCardField> ofKind(const ElementKind& kind) {
    QList<BookCardField> set;
    for (const CardField& field : kind.fields) {
        BookCardField entry;
        entry.id = field.id;
        entry.onCard = field.onCard;
        entry.inProperties = field.inProperties;
        entry.inList = field.inList;
        set << entry;
    }
    return set;
}

QList<BookCardField> complete(const ElementKind& kind, const QList<BookCardField>& set) {
    if (set.isEmpty()) {
        return ofKind(kind);
    }
    QList<BookCardField> fields;
    QSet<QString> listed;
    for (const BookCardField& field : set) {
        if ((field.isOwn() || kind.field(field.id) != nullptr) && !listed.contains(field.id)) {
            fields << field;
            listed.insert(field.id);
        }
    }
    for (const CardField& field : kind.fields) {
        if (!listed.contains(field.id)) {
            BookCardField hidden;
            hidden.id = field.id;
            fields << hidden;
        }
    }
    return fields;
}

QList<CardField> effective(const ElementKind& kind, const QList<BookCardField>& set) {
    QList<CardField> fields;
    for (const BookCardField& field : complete(kind, set)) {
        fields << cardFieldOf(field, field.isOwn() ? nullptr : kind.field(field.id));
    }
    return fields;
}

bool isKindSet(const ElementKind& kind, const QList<BookCardField>& set) {
    const QList<BookCardField> fields = complete(kind, set);
    const QList<BookCardField> ofPackage = ofKind(kind);
    if (fields.size() != ofPackage.size()) {
        return false;
    }
    for (qsizetype i = 0; i < fields.size(); ++i) {
        const BookCardField& field = fields.at(i);
        const BookCardField& original = ofPackage.at(i);
        if (field.id != original.id || field.onCard != original.onCard ||
            field.inProperties != original.inProperties || field.inList != original.inList) {
            return false;
        }
    }
    return true;
}

QList<BookCardField> keepUnknown(const ElementKind& kind, const QList<BookCardField>& before,
                                 const QList<BookCardField>& after) {
    QList<BookCardField> fields = after;
    for (const BookCardField& field : before) {
        const bool known = field.isOwn() || kind.field(field.id) != nullptr;
        const bool listed = std::any_of(fields.cbegin(), fields.cend(),
                                        [&field](const BookCardField& each) {
                                            return each.id == field.id;
                                        });
        if (!known && !listed) {
            fields << field;
        }
    }
    return fields;
}

QString newOwnId(const QList<BookCardField>& set, const QStringList& taken) {
    const QString prefix = QLatin1String(BookCardField::OWN_PREFIX);
    int highest = 0;
    for (const BookCardField& field : set) {
        highest = std::max(highest, numberOf(field.id, prefix));
    }
    for (const QString& id : taken) {
        highest = std::max(highest, numberOf(id, prefix));
    }
    return prefix + QString::number(highest + 1);
}

QString newChoiceId(const QList<CardFieldChoice>& choices) {
    const QString prefix = QLatin1String(CHOICE_PREFIX);
    int highest = 0;
    for (const CardFieldChoice& choice : choices) {
        highest = std::max(highest, numberOf(choice.id, prefix));
    }
    return prefix + QString::number(highest + 1);
}

CardFieldSets read(const QJsonValue& value) {
    CardFieldSets sets;
    const QJsonObject object = value.toObject();
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        QList<BookCardField> fields;
        for (const QJsonValue& entry : it.value().toArray()) {
            if (std::optional<BookCardField> field = readEntry(entry)) {
                fields << *field;
            }
        }
        if (!fields.isEmpty()) {
            sets.insert(it.key(), fields);
        }
    }
    return sets;
}

QJsonObject write(const CardFieldSets& sets) {
    QJsonObject object;
    for (auto it = sets.constBegin(); it != sets.constEnd(); ++it) {
        if (it.value().isEmpty()) {
            continue;
        }
        QJsonArray fields;
        for (const BookCardField& field : it.value()) {
            fields.append(entryOf(field));
        }
        object.insert(it.key(), fields);
    }
    return object;
}

CardFieldSets fromSetting(const nlohmann::json& value) {
    return read(toQt(value));
}

nlohmann::json toSetting(const CardFieldSets& sets) {
    return fromQt(write(sets));
}

CardFieldSets defaults() {
    return fromSetting(SettingsManager::getInstance().get<nlohmann::json>(DEFAULTS_SETTING));
}

void setDefaults(const CardFieldSets& sets) {
    SettingsManager::getInstance().set<nlohmann::json>(DEFAULTS_SETTING, toSetting(sets));
}

}  // namespace kalahari::core::card_fields
