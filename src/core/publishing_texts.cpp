// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file publishing_texts.cpp
/// @brief The texts for publishers: their data, templates, lengths and the set of a book

#include <kalahari/core/publishing_texts.h>

#include <kalahari/core/book_project.h>
#include <kalahari/core/cover_material.h>
#include <kalahari/core/package_json.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/core/volume_materials.h>

#include <QByteArray>
#include <QChar>
#include <QJsonDocument>
#include <QJsonValue>
#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>
#include <QSet>
#include <QStringView>
#include <QXmlStreamReader>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>

namespace kalahari::core {

namespace {

using package_json::addProblem;
using package_json::checkKeys;
using package_json::isIdentifier;
using package_json::isWholeNumber;
using package_json::readText;

/// In place of an empty field of a pattern: "…"
constexpr char16_t ELLIPSIS = u'\u2026';

/// The field of a pattern: {hero}
const QRegularExpression& fieldExpression() {
    static const QRegularExpression expression(QStringLiteral("\\{([a-z][a-z0-9_]*)\\}"));
    return expression;
}

QString pathOf(const QString& field, const char* key) {
    return field.isEmpty() ? QString::fromLatin1(key)
                           : field + QLatin1Char('.') + QLatin1String(key);
}

QString pathOf(const QString& field, qsizetype index) {
    return QStringLiteral("%1[%2]").arg(field).arg(index);
}

/// Text of @p key, taken out of @p rest; empty when it has none
QString takeString(QJsonObject& rest, const char* key, const QString& field,
                   QStringList& problems) {
    const QJsonValue value = rest.take(QLatin1String(key));
    if (value.isUndefined()) {
        return {};
    }
    if (!value.isString()) {
        addProblem(problems, pathOf(field, key), QStringLiteral("must be a text"));
        return {};
    }
    return value.toString();
}

/// Length of @p key, taken out of @p rest: a whole number, 0 or more; 0 when it has none
int takeLength(QJsonObject& rest, const char* key, const QString& field,
               QStringList& problems) {
    const QJsonValue value = rest.take(QLatin1String(key));
    if (value.isUndefined()) {
        return 0;
    }
    const int length = isWholeNumber(value) ? value.toInt(-1) : -1;
    if (length < 0) {
        addProblem(problems, pathOf(field, key),
                   QStringLiteral("must be a whole number, 0 or more"));
        return 0;
    }
    return length;
}

/// Texts of the list @p key, taken out of @p rest
QStringList takeTexts(QJsonObject& rest, const char* key, const QString& field,
                      QStringList& problems) {
    const QJsonValue value = rest.take(QLatin1String(key));
    if (value.isUndefined()) {
        return {};
    }
    QStringList texts;
    const QJsonArray array = value.toArray();
    const bool allTexts = std::all_of(array.begin(), array.end(),
                                      [](const QJsonValue& each) { return each.isString(); });
    if (!value.isArray() || !allTexts) {
        addProblem(problems, pathOf(field, key), QStringLiteral("must be a list of texts"));
        return {};
    }
    for (const auto& each : array) {
        texts << each.toString();
    }
    return texts;
}

/// Texts of the object @p key by their names, taken out of @p rest
QMap<QString, QString> takeTextMap(QJsonObject& rest, const char* key, const QString& field,
                                   QStringList& problems) {
    const QJsonValue value = rest.take(QLatin1String(key));
    QMap<QString, QString> texts;
    if (value.isUndefined()) {
        return texts;
    }
    const QJsonObject object = value.toObject();
    const bool allTexts = std::all_of(object.begin(), object.end(),
                                      [](const QJsonValue& each) { return each.isString(); });
    if (!value.isObject() || !allTexts) {
        addProblem(problems, pathOf(field, key),
                   QStringLiteral("must be an object of texts by their names"));
        return texts;
    }
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        texts.insert(it.key(), it.value().toString());
    }
    return texts;
}

void setOrRemove(QJsonObject& json, const char* key, const QString& value) {
    if (value.isEmpty()) {
        json.remove(QLatin1String(key));
    } else {
        json.insert(QLatin1String(key), value);
    }
}

void setOrRemove(QJsonObject& json, const char* key, int value) {
    if (value > 0) {
        json.insert(QLatin1String(key), value);
    } else {
        json.remove(QLatin1String(key));
    }
}

void setOrRemove(QJsonObject& json, const char* key, const QStringList& values) {
    if (values.isEmpty()) {
        json.remove(QLatin1String(key));
    } else {
        json.insert(QLatin1String(key), QJsonArray::fromStringList(values));
    }
}

void setOrRemove(QJsonObject& json, const char* key, const QMap<QString, QString>& values) {
    if (values.isEmpty()) {
        json.remove(QLatin1String(key));
        return;
    }
    QJsonObject object;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        object.insert(it.key(), it.value());
    }
    json.insert(QLatin1String(key), object);
}

// ---------------------------------------------------------------------------------------------
// Templates of the packages
// ---------------------------------------------------------------------------------------------

/// The target length of a template: { "characters": 1800, "words": 300 }, both above 0
bool readTarget(const QJsonValue& value, const QString& field, PublishingTemplate& templ,
                QStringList& problems) {
    const QJsonObject target = value.toObject();
    const QJsonValue characters = target.value(QStringLiteral("characters"));
    const QJsonValue words = target.value(QStringLiteral("words"));
    if (!value.isObject() || target.size() != 2 || !isWholeNumber(characters) ||
        !isWholeNumber(words) || characters.toInt() < 1 || words.toInt() < 1) {
        addProblem(problems, field,
                   QStringLiteral("must be the target length in characters and in words, both "
                                  "above 0, e.g. { \"characters\": 1800, \"words\": 300 }"));
        return false;
    }
    templ.targetCharacters = characters.toInt();
    templ.targetWords = words.toInt();
    return true;
}

/// The id of a point or a field: an identifier, unique among @p seen
QString readItemId(const QJsonObject& item, const QString& field, QSet<QString>& seen,
                   QStringList& problems) {
    const QString id = item.value(QStringLiteral("id")).toString();
    if (!isIdentifier(id)) {
        addProblem(problems, pathOf(field, "id"),
                   QStringLiteral("must be lowercase letters, digits and _, e.g. \"hero\""));
    } else if (seen.contains(id)) {
        addProblem(problems, pathOf(field, "id"),
                   QStringLiteral("'%1' is already the id of another one").arg(id));
    }
    seen.insert(id);
    return id;
}

/// The points of a template of a synopsis, or the fields of a template of a sentence: each
/// an id, a name and a hint
template <typename Item>
QList<Item> readItems(const QJsonValue& value, const QString& field, bool hintRequired,
                      QStringList& problems) {
    QList<Item> items;
    if (!value.isArray() || value.toArray().isEmpty()) {
        addProblem(problems, field, QStringLiteral("must be a list that is not empty"));
        return items;
    }
    const QJsonArray array = value.toArray();
    QSet<QString> seen;
    for (qsizetype i = 0; i < array.size(); ++i) {
        const QString itemField = pathOf(field, i);
        if (!array.at(i).isObject()) {
            addProblem(problems, itemField, QStringLiteral("must be an object"));
            continue;
        }
        const QJsonObject item = array.at(i).toObject();
        checkKeys(item, itemField, {"id", "name", "hint"}, problems);
        Item read;
        read.id = readItemId(item, itemField, seen, problems);
        read.name = readText(item.value(QStringLiteral("name")), pathOf(itemField, "name"), true,
                             problems);
        read.hint = readText(item.value(QStringLiteral("hint")), pathOf(itemField, "hint"),
                             hintRequired, problems);
        items << read;
    }
    return items;
}

/// Each field that a pattern names is a field of the template, and the English pattern
/// names each field
void checkPattern(const PublishingTemplate& templ, const QString& field,
                  QStringList& problems) {
    QSet<QString> fields;
    for (const PublishingTemplateField& each : templ.fields) {
        fields.insert(each.id);
    }
    for (auto it = templ.pattern.values.constBegin(); it != templ.pattern.values.constEnd();
         ++it) {
        for (const QString& named : PublishingTexts::fieldsOf(it.value())) {
            if (!fields.contains(named)) {
                addProblem(problems, pathOf(field, "pattern") + QLatin1Char('.') + it.key(),
                           QStringLiteral("names {%1}, which is not one of its fields").arg(named));
            }
        }
    }
    const QStringList english = PublishingTexts::fieldsOf(templ.pattern.text(QStringLiteral("en")));
    for (const PublishingTemplateField& each : templ.fields) {
        if (!english.contains(each.id)) {
            addProblem(problems, pathOf(field, "fields"),
                       QStringLiteral("the pattern does not name the field '%1'").arg(each.id));
        }
    }
}

/// Reports @p key of @p json, which a template of @p part does not have
void refuseKey(const QJsonObject& json, const char* key, const QString& field,
               const QString& part, QStringList& problems) {
    if (json.contains(QLatin1String(key))) {
        addProblem(problems, pathOf(field, key),
                   QStringLiteral("a template of a %1 does not have it").arg(part));
    }
}

/// The points of a template of a synopsis, or the hint of the point of each text
void readSynopsisTemplate(const QJsonObject& json, const QString& field,
                          PublishingTemplate& templ, QStringList& problems) {
    for (const char* key : {"pattern", "fields"}) {
        refuseKey(json, key, field, templ.part, problems);
    }
    const QJsonValue eachText = json.value(QStringLiteral("eachText"));
    if (!eachText.isUndefined() && !eachText.isBool()) {
        addProblem(problems, pathOf(field, "eachText"), QStringLiteral("must be true or false"));
    }
    templ.eachText = eachText.toBool(false);
    if (templ.eachText) {
        refuseKey(json, "points", field, QStringLiteral("synopsis of each text"), problems);
        templ.hint = readText(json.value(QStringLiteral("hint")), pathOf(field, "hint"), true,
                              problems);
        return;
    }
    refuseKey(json, "hint", field, QStringLiteral("synopsis with its own points"), problems);
    templ.points = readItems<PublishingTemplatePoint>(json.value(QStringLiteral("points")),
                                                      pathOf(field, "points"), false, problems);
}

/// The pattern of a template of a sentence and its fields
void readPitchTemplate(const QJsonObject& json, const QString& field, PublishingTemplate& templ,
                       QStringList& problems) {
    for (const char* key : {"eachText", "hint", "points"}) {
        refuseKey(json, key, field, templ.part, problems);
    }
    templ.pattern = readText(json.value(QStringLiteral("pattern")), pathOf(field, "pattern"), true,
                             problems);
    templ.fields = readItems<PublishingTemplateField>(json.value(QStringLiteral("fields")),
                                                      pathOf(field, "fields"), true, problems);
    checkPattern(templ, field, problems);
}

// ---------------------------------------------------------------------------------------------
// The writer's own templates
// ---------------------------------------------------------------------------------------------

/// One text of the writer, kept as the English text: shown in every language
LocalizedText ownText(const QJsonValue& value) {
    LocalizedText text;
    const QString plain = value.toString().trimmed();
    if (!plain.isEmpty()) {
        text.values.insert(QStringLiteral("en"), plain);
    }
    return text;
}

QString plainText(const LocalizedText& text) {
    return text.values.value(QStringLiteral("en"));
}

template <typename Item>
QList<Item> readOwnItems(const QJsonValue& value) {
    QList<Item> items;
    for (const auto& each : value.toArray()) {
        const QJsonObject item = each.toObject();
        const QString id = item.value(QStringLiteral("id")).toString();
        if (!id.isEmpty()) {
            items << Item{id, ownText(item.value(QStringLiteral("name"))),
                          ownText(item.value(QStringLiteral("hint")))};
        }
    }
    return items;
}

template <typename Item>
QJsonArray ownItemsToJson(const QList<Item>& items) {
    QJsonArray array;
    for (const Item& item : items) {
        QJsonObject object{{QStringLiteral("id"), item.id},
                           {QStringLiteral("name"), plainText(item.name)}};
        if (!item.hint.isEmpty()) {
            object.insert(QStringLiteral("hint"), plainText(item.hint));
        }
        array.append(object);
    }
    return array;
}

// ---------------------------------------------------------------------------------------------
// The set
// ---------------------------------------------------------------------------------------------

/// @p visit for each element of @p elements and of its groups, in order (a group before its
/// elements), without recursion; @p List is const for the elements not to change
template <typename List, typename Visit>
void forEachIn(List& elements, const Visit& visit) {
    QList<std::pair<List*, qsizetype>> lists{{&elements, 0}};
    while (!lists.isEmpty()) {
        auto& [list, next] = lists.last();
        if (next >= list->size()) {
            lists.removeLast();
            continue;
        }
        auto& element = (*list)[next];
        ++next;
        visit(element);
        if (!element.elements.isEmpty()) {
            lists.append({&element.elements, 0});
        }
    }
}

/// Each element of @p elements and of its groups for which @p wanted is true, in order
void collect(const QList<ProjectElement>& elements,
             const std::function<bool(const ProjectElement&)>& wanted,
             QList<const ProjectElement*>& found) {
    forEachIn(elements, [&wanted, &found](const ProjectElement& element) {
        if (wanted(element)) {
            found << &element;
        }
    });
}

/// Each element of @p elements and of its groups, in order, to change
void collectAll(QList<ProjectElement>& elements, QList<ProjectElement*>& found) {
    forEachIn(elements, [&found](ProjectElement& element) { found << &element; });
}

/// The version of a sentence or a synopsis chosen for the set of @p book: of its volume
/// first, else of the whole series
const ProjectElement* chosenText(const BookProject& project, const ProjectBook& book,
                                 const QString& part) {
    const ProjectElement* ofSeries = nullptr;
    for (const ProjectElement* text : PublishingTexts::versionsOf(project, part, &book)) {
        if (text->publishing.state != QLatin1String(PublishingTexts::CHOSEN)) {
            continue;
        }
        if (!VolumeMaterials::volumeOf(project, *text).isEmpty()) {
            return text;
        }
        if (ofSeries == nullptr) {
            ofSeries = text;
        }
    }
    return ofSeries;
}

/// The description or the tagline chosen for the cover of @p book: of its volume first, else
/// of the whole series; of each, for @p place first, else for another place
const ProjectElement* chosenForCover(const BookProject& project, const ProjectBook& book,
                                     const QString& part, const char* place) {
    const ProjectElement* best = nullptr;
    int bestRank = 0;
    for (const ProjectElement* material : PublishingTexts::versionsOf(project, part, &book)) {
        if (material->cover.state != QLatin1String(CoverMaterial::CHOSEN)) {
            continue;
        }
        const int rank = (VolumeMaterials::volumeOf(project, *material).isEmpty() ? 2 : 0) +
                         (material->cover.place == QLatin1String(place) ? 0 : 1);
        if (best == nullptr || rank < bestRank) {
            best = material;
            bestRank = rank;
        }
    }
    return best;
}

/// The first note about the author in the back part of @p book
const ProjectElement* firstAboutAuthorOf(const ProjectBook& book) {
    QList<const ProjectElement*> notes;
    collect(book.backElements,
            [](const ProjectElement& element) {
                return element.kind.kindId == QLatin1String(PublishingTexts::ABOUT_AUTHOR_KIND);
            },
            notes);
    return notes.isEmpty() ? nullptr : notes.first();
}

/// The version of @p part chosen for the set, not its pick
const ProjectElement* chosenVersion(const BookProject& project, const ProjectBook& book,
                                    const QString& part) {
    if (part == QLatin1String(PublishingTexts::TAGLINE)) {
        return chosenForCover(project, book, part, CoverMaterial::FRONT);
    }
    if (part == QLatin1String(PublishingTexts::BLURB)) {
        return chosenForCover(project, book, part, CoverMaterial::BACK);
    }
    if (part == QLatin1String(PublishingTexts::ABOUT_AUTHOR)) {
        return firstAboutAuthorOf(book);
    }
    return chosenText(project, book, part);
}

/// Markdown that a paragraph would start with: a heading, a list, a quote
bool startsMarkdown(const QString& paragraph) {
    static const QRegularExpression start(QStringLiteral("^(#|>|[-+*] |\\d+[.)] )"));
    return start.match(paragraph).hasMatch();
}

QJsonArray toQtJson(const nlohmann::json& json) {
    return QJsonDocument::fromJson(QByteArray::fromStdString(json.dump())).array();
}

nlohmann::json toSettingsJson(const QJsonArray& array) {
    const QByteArray text = QJsonDocument(array).toJson(QJsonDocument::Compact);
    return nlohmann::json::parse(text.constBegin(), text.constEnd(), nullptr, false);
}

}  // namespace

// =============================================================================================
// PublishingData
// =============================================================================================

bool PublishingData::isEmpty() const {
    return part.isEmpty() && state.isEmpty() && templateId.isEmpty() && fields.isEmpty() &&
           !manual && done.isEmpty() && targetCharacters == 0 && targetWords == 0 &&
           extra.isEmpty();
}

QJsonObject PublishingData::toJson() const {
    if (isEmpty()) {
        return {};
    }
    QJsonObject json = extra;
    setOrRemove(json, "part", part);
    setOrRemove(json, "state", state);
    setOrRemove(json, "template", templateId);
    setOrRemove(json, "fields", fields);
    if (manual) {
        json.insert(QStringLiteral("manual"), true);
    } else {
        json.remove(QStringLiteral("manual"));
    }
    setOrRemove(json, "done", done);
    setOrRemove(json, "targetCharacters", targetCharacters);
    setOrRemove(json, "targetWords", targetWords);
    return json;
}

PublishingData PublishingData::fromJson(const QJsonObject& json, const QString& field,
                                        QStringList& problems) {
    QJsonObject rest = json;
    PublishingData data;
    data.part = takeString(rest, "part", field, problems);
    data.state = takeString(rest, "state", field, problems);
    data.templateId = takeString(rest, "template", field, problems);
    data.fields = takeTextMap(rest, "fields", field, problems);
    const QJsonValue manual = rest.take(QStringLiteral("manual"));
    if (!manual.isUndefined() && !manual.isBool()) {
        addProblem(problems, pathOf(field, "manual"), QStringLiteral("must be true or false"));
    }
    data.manual = manual.toBool(false);
    data.done = takeTexts(rest, "done", field, problems);
    data.targetCharacters = takeLength(rest, "targetCharacters", field, problems);
    data.targetWords = takeLength(rest, "targetWords", field, problems);
    data.extra = rest;
    return data;
}

// =============================================================================================
// PublishingTemplate
// =============================================================================================

QString PublishingTemplate::reference() const {
    return (isOwn() ? QString::fromLatin1(PublishingTexts::OWN_TEMPLATE_PREFIX) : package) +
           QLatin1Char(':') + id;
}

bool PublishingTemplate::isFor(const QString& typeId) const {
    return !isOwn() || types.isEmpty() || types.contains(typeId);
}

QList<PublishingTemplatePoint> PublishingTemplate::pointsFor(
    const QList<std::pair<QString, QString>>& texts) const {
    if (!eachText) {
        return points;
    }
    QList<PublishingTemplatePoint> each;
    for (const auto& [textId, title] : texts) {
        LocalizedText pointName;
        pointName.values.insert(QStringLiteral("en"), title);
        each << PublishingTemplatePoint{textId, pointName, hint};
    }
    return each;
}

std::optional<PublishingTemplate> PublishingTemplate::fromPackageJson(const QJsonObject& json,
                                                                      const QString& packageId,
                                                                      const QString& field,
                                                                      QStringList& problems) {
    const qsizetype problemsBefore = problems.size();
    checkKeys(json, field,
              {"id", "part", "name", "target", "eachText", "hint", "points", "pattern", "fields"},
              problems);
    PublishingTemplate templ;
    templ.package = packageId;
    templ.id = json.value(QStringLiteral("id")).toString();
    if (!isIdentifier(templ.id)) {
        addProblem(problems, pathOf(field, "id"),
                   QStringLiteral("must be lowercase letters, digits and _, e.g. \"one_page\""));
    }
    templ.name = readText(json.value(QStringLiteral("name")), pathOf(field, "name"), true,
                          problems);
    readTarget(json.value(QStringLiteral("target")), pathOf(field, "target"), templ, problems);
    templ.part = json.value(QStringLiteral("part")).toString();
    if (templ.part == QLatin1String(PublishingTexts::SYNOPSIS)) {
        readSynopsisTemplate(json, field, templ, problems);
    } else if (templ.part == QLatin1String(PublishingTexts::PITCH)) {
        readPitchTemplate(json, field, templ, problems);
    } else {
        addProblem(problems, pathOf(field, "part"),
                   QStringLiteral("must be \"pitch\" or \"synopsis\""));
    }
    if (problems.size() > problemsBefore) {
        return std::nullopt;
    }
    return templ;
}

QList<PublishingTemplate> PublishingTemplate::listFromPackageJson(const QJsonValue& value,
                                                                  const QString& packageId,
                                                                  QStringList& problems) {
    QList<PublishingTemplate> templates;
    const QString field = QStringLiteral("publishingTemplates");
    if (value.isUndefined()) {
        return templates;
    }
    if (!value.isArray()) {
        addProblem(problems, field, QStringLiteral("must be a list of templates"));
        return templates;
    }
    const QJsonArray array = value.toArray();
    for (qsizetype i = 0; i < array.size(); ++i) {
        const QString each = pathOf(field, i);
        if (!array.at(i).isObject()) {
            addProblem(problems, each, QStringLiteral("must be an object"));
            continue;
        }
        std::optional<PublishingTemplate> templ =
            fromPackageJson(array.at(i).toObject(), packageId, each, problems);
        if (!templ) {
            continue;
        }
        const QString id = templ->id;
        if (std::any_of(templates.cbegin(), templates.cend(),
                        [&id](const PublishingTemplate& other) { return other.id == id; })) {
            addProblem(problems, pathOf(each, "id"),
                       QStringLiteral("'%1' is already the id of another template").arg(id));
            continue;
        }
        templates << *templ;
    }
    return templates;
}

std::optional<PublishingTemplate> PublishingTemplate::fromOwnJson(const QJsonObject& json) {
    QJsonObject rest = json;
    PublishingTemplate templ;
    templ.id = rest.take(QStringLiteral("id")).toString();
    templ.part = rest.take(QStringLiteral("part")).toString();
    if (templ.id.isEmpty() || !PublishingTexts::parts().contains(templ.part)) {
        return std::nullopt;
    }
    templ.name = ownText(rest.take(QStringLiteral("name")));
    for (const auto& type : rest.take(QStringLiteral("types")).toArray()) {
        if (type.isString() && !templ.types.contains(type.toString())) {
            templ.types << type.toString();
        }
    }
    const QJsonObject target = rest.take(QStringLiteral("target")).toObject();
    templ.targetCharacters = std::max(0, target.value(QStringLiteral("characters")).toInt());
    templ.targetWords = std::max(0, target.value(QStringLiteral("words")).toInt());
    templ.eachText = rest.take(QStringLiteral("eachText")).toBool(false);
    templ.hint = ownText(rest.take(QStringLiteral("hint")));
    templ.points = readOwnItems<PublishingTemplatePoint>(rest.take(QStringLiteral("points")));
    templ.pattern = ownText(rest.take(QStringLiteral("pattern")));
    templ.fields = readOwnItems<PublishingTemplateField>(rest.take(QStringLiteral("fields")));
    templ.extra = rest;
    return templ;
}

QJsonObject PublishingTemplate::toOwnJson() const {
    QJsonObject json = extra;
    json.insert(QStringLiteral("id"), id);
    json.insert(QStringLiteral("part"), part);
    json.insert(QStringLiteral("name"), plainText(name));
    setOrRemove(json, "types", types);
    json.insert(QStringLiteral("target"),
                QJsonObject{{QStringLiteral("characters"), targetCharacters},
                            {QStringLiteral("words"), targetWords}});
    if (eachText) {
        json.insert(QStringLiteral("eachText"), true);
    } else {
        json.remove(QStringLiteral("eachText"));
    }
    setOrRemove(json, "hint", plainText(hint));
    const QJsonArray ownPoints = ownItemsToJson(points);
    const QJsonArray ownFields = ownItemsToJson(fields);
    for (const auto& [key, array] :
         {std::pair{"points", ownPoints}, std::pair{"fields", ownFields}}) {
        if (array.isEmpty()) {
            json.remove(QLatin1String(key));
        } else {
            json.insert(QLatin1String(key), array);
        }
    }
    setOrRemove(json, "pattern", plainText(pattern));
    return json;
}

// =============================================================================================
// PublishingSet
// =============================================================================================

QStringList PublishingSet::parts() const {
    QStringList parts;
    for (const QString& part : order) {
        if (PublishingTexts::setParts().contains(part) && !parts.contains(part)) {
            parts << part;
        }
    }
    for (const QString& part : PublishingTexts::setParts()) {
        if (!parts.contains(part)) {
            parts << part;
        }
    }
    return parts;
}

bool PublishingSet::includes(const QString& part) const {
    return !off.contains(part);
}

PublishingSet PublishingSet::withPart(const QString& part, bool on) const {
    PublishingSet set = *this;
    set.off.removeAll(part);
    if (!on && PublishingTexts::setParts().contains(part)) {
        set.off << part;
    }
    return set;
}

PublishingSet PublishingSet::withPartMoved(const QString& part, int step) const {
    PublishingSet set = *this;
    set.order = parts();
    const qsizetype from = set.order.indexOf(part);
    const qsizetype to = from + step;
    if (from < 0 || to < 0 || to >= set.order.size()) {
        return *this;
    }
    set.order.move(from, to);
    return set;
}

PublishingSet PublishingSet::defaults() {
    const auto chosen = SettingsManager::getInstance().get<nlohmann::json>(
        PublishingTexts::SET_SETTING, nlohmann::json::array());
    PublishingSet set;
    for (const auto& part : toQtJson(chosen)) {
        set.order << part.toString();
    }
    const QStringList listed = set.order;
    set.order = set.parts();
    for (const QString& part : PublishingTexts::setParts()) {
        if (!listed.contains(part)) {
            set.off << part;
        }
    }
    return set;
}

QJsonObject PublishingSet::toJson() const {
    QJsonObject json = extra;
    json.insert(QStringLiteral("order"), QJsonArray::fromStringList(order));
    setOrRemove(json, "off", off);
    setOrRemove(json, "picks", picks);
    return json;
}

PublishingSet PublishingSet::fromJson(const QJsonObject& json, const QString& field,
                                      QStringList& problems) {
    QJsonObject rest = json;
    PublishingSet set;
    set.order = takeTexts(rest, "order", field, problems);
    set.off = takeTexts(rest, "off", field, problems);
    set.picks = takeTextMap(rest, "picks", field, problems);
    set.extra = rest;
    return set;
}

// =============================================================================================
// PublishingTexts
// =============================================================================================

const QStringList& PublishingTexts::parts() {
    static const QStringList parts{QString::fromLatin1(PITCH), QString::fromLatin1(SYNOPSIS)};
    return parts;
}

const QStringList& PublishingTexts::setParts() {
    static const QStringList parts{QString::fromLatin1(PITCH), QString::fromLatin1(TAGLINE),
                                   QString::fromLatin1(BLURB), QString::fromLatin1(SYNOPSIS),
                                   QString::fromLatin1(ABOUT_AUTHOR)};
    return parts;
}

bool PublishingTexts::hasPoints(const QString& part) {
    return part == QLatin1String(SYNOPSIS);
}

bool PublishingTexts::hasPattern(const QString& part) {
    return part == QLatin1String(PITCH);
}

bool PublishingTexts::showsPages() {
    return SettingsManager::getInstance().get<bool>(PAGES_SETTING, true);
}

bool PublishingTexts::showsPointBars() {
    return SettingsManager::getInstance().get<bool>(POINT_BARS_SETTING, true);
}

bool PublishingTexts::warnsOfPoints() {
    return SettingsManager::getInstance().get<bool>(POINT_WARNINGS_SETTING, true);
}

QSet<QString> PublishingTexts::linkedPointsOf(const QString& kml) {
    // The tag and its attribute as editor::TEXT_POINTS_TAG and TEXT_POINTS_IDS name them
    QSet<QString> linked;
    QStringList open;  // the ids of each <points> the reader is in
    QXmlStreamReader reader(QStringLiteral("<root>") + kml + QStringLiteral("</root>"));
    while (!reader.atEnd()) {
        switch (reader.readNext()) {
        case QXmlStreamReader::StartElement:
            if (reader.name() == QLatin1String("points")) {
                open << reader.attributes().value(QLatin1String("ids")).toString();
            }
            break;
        case QXmlStreamReader::EndElement:
            if (reader.name() == QLatin1String("points") && !open.isEmpty()) {
                open.removeLast();
            }
            break;
        case QXmlStreamReader::Characters:
            if (!open.isEmpty() && !reader.text().trimmed().isEmpty()) {
                for (const QString& ids : std::as_const(open)) {
                    for (const QString& id : ids.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
                        linked.insert(id);
                    }
                }
            }
            break;
        default:
            break;
        }
    }
    return linked;
}

std::string PublishingTexts::targetSettingOf(const QString& part, bool words) {
    return std::string(TARGETS_SETTING) + '.' + part.toStdString() +
           (words ? ".words" : ".characters");
}

int PublishingTexts::defaultTargetOf(const QString& part, bool words) {
    if (!parts().contains(part)) {
        return 0;
    }
    const int target = SettingsManager::getInstance().get<int>(targetSettingOf(part, words), 0);
    return std::max(target, 0);
}

int PublishingTexts::targetOf(const PublishingData& data, const PublishingTemplate* templ,
                              int texts, bool words) {
    const int own = words ? data.targetWords : data.targetCharacters;
    if (own > 0) {
        return own;
    }
    if (templ != nullptr) {
        const int target = words ? templ->targetWords : templ->targetCharacters;
        if (target > 0) {
            return templ->eachText ? target * std::max(texts, 1) : target;
        }
    }
    return defaultTargetOf(data.part, words);
}

double PublishingTexts::pagesOf(int characters) {
    constexpr double TENTHS = 10.0;
    return std::round(characters * TENTHS / PAGE_CHARACTERS) / TENTHS;
}

QString PublishingTexts::composePitch(const QString& pattern,
                                      const QMap<QString, QString>& fields) {
    QString sentence;
    qsizetype from = 0;
    QRegularExpressionMatchIterator it = fieldExpression().globalMatch(pattern);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        sentence += pattern.mid(from, match.capturedStart() - from);
        const QString value = fields.value(match.captured(1)).trimmed();
        sentence += value.isEmpty() ? QString(QChar(ELLIPSIS)) : value;
        from = match.capturedEnd();
    }
    sentence += pattern.mid(from);
    sentence = sentence.simplified();
    // The first letter a capital one, after the quotation marks or the bracket that open it
    static const QString OPENING = QStringLiteral("„“‘«\"'(");
    qsizetype first = 0;
    while (first < sentence.size() && OPENING.contains(sentence.at(first))) {
        ++first;
    }
    if (first < sentence.size() && sentence.at(first).isLetter()) {
        sentence[first] = sentence.at(first).toUpper();
    }
    return sentence;
}

QStringList PublishingTexts::fieldsOf(const QString& pattern) {
    QStringList fields;
    QRegularExpressionMatchIterator it = fieldExpression().globalMatch(pattern);
    while (it.hasNext()) {
        const QString field = it.next().captured(1);
        if (!fields.contains(field)) {
            fields << field;
        }
    }
    return fields;
}

QList<PublishingTemplate> PublishingTexts::ownTemplates() {
    const auto stored = SettingsManager::getInstance().get<nlohmann::json>(
        TEMPLATES_SETTING, nlohmann::json::array());
    QList<PublishingTemplate> templates;
    QSet<QString> ids;
    for (const auto& each : toQtJson(stored)) {
        std::optional<PublishingTemplate> templ = PublishingTemplate::fromOwnJson(each.toObject());
        if (templ && !ids.contains(templ->id)) {
            ids.insert(templ->id);
            templates << *templ;
        }
    }
    return templates;
}

void PublishingTexts::setOwnTemplates(const QList<PublishingTemplate>& templates) {
    QJsonArray array;
    for (const PublishingTemplate& templ : templates) {
        array.append(templ.toOwnJson());
    }
    SettingsManager::getInstance().set<nlohmann::json>(TEMPLATES_SETTING, toSettingsJson(array));
}

QString PublishingTexts::newOwnTemplateId(const QList<PublishingTemplate>& templates) {
    for (int number = 1;; ++number) {
        const QString id = QStringLiteral("t%1").arg(number);
        const bool used = std::any_of(templates.cbegin(), templates.cend(),
                                      [&id](const PublishingTemplate& each) {
                                          return each.id == id;
                                      });
        if (!used) {
            return id;
        }
    }
}

QList<const ProjectElement*> PublishingTexts::textsOf(const BookProject& project,
                                                      const QString& part) {
    QList<const ProjectElement*> texts;
    collect(project.workshop.elements,
            [&part](const ProjectElement& element) { return element.publishing.part == part; },
            texts);
    return texts;
}

QStringList PublishingTexts::setState(BookProject& project, const QString& elementId,
                                     const QString& state) {
    QList<ProjectElement*> workshop;
    collectAll(project.workshop.elements, workshop);
    const auto text = std::find_if(workshop.begin(), workshop.end(),
                                   [&elementId](const ProjectElement* element) {
                                       return element->id == elementId;
                                   });
    if (text == workshop.end()) {
        return {};
    }
    (*text)->publishing.state = state;
    if (state != QLatin1String(CHOSEN)) {
        return {};
    }
    return VolumeMaterials::unchooseOthers(project, **text);
}

QList<const ProjectElement*> PublishingTexts::versionsOf(const BookProject& project,
                                                         const QString& part,
                                                         const ProjectBook* book) {
    QList<const ProjectElement*> versions;
    if (part == QLatin1String(TAGLINE) || part == QLatin1String(BLURB)) {
        collect(project.workshop.elements,
                [&part](const ProjectElement& element) { return element.cover.part == part; },
                versions);
    } else if (parts().contains(part)) {
        versions = textsOf(project, part);
    }
    if (book != nullptr) {
        versions.removeIf([&project, book](const ProjectElement* version) {
            return !VolumeMaterials::isFor(project, *version, *book);
        });
    }
    return versions;
}

const ProjectElement* PublishingTexts::aboutAuthorOf(const ProjectBook& book) {
    return firstAboutAuthorOf(book);
}

QList<PublishingSetPart> PublishingTexts::resolveSet(const BookProject& project,
                                                     const ProjectBook& book,
                                                     const PublishingSet& set) {
    QList<PublishingSetPart> resolved;
    for (const QString& part : set.parts()) {
        PublishingSetPart each{part, {}, set.includes(part), false};
        const QString pick = set.picks.value(part);
        const QList<const ProjectElement*> versions = versionsOf(project, part, &book);
        const bool pickIsVersion =
            !pick.isEmpty() &&
            std::any_of(versions.cbegin(), versions.cend(),
                        [&pick](const ProjectElement* version) { return version->id == pick; });
        if (pickIsVersion) {
            each.elementId = pick;
            each.picked = true;
        } else if (const ProjectElement* chosen = chosenVersion(project, book, part)) {
            each.elementId = chosen->id;
        }
        resolved << each;
    }
    return resolved;
}

QString PublishingTexts::setToPlainText(const QString& title, const QString& subtitle,
                                        const QList<PublishingSetText>& parts) {
    QStringList head;
    for (const QString& line : {title.trimmed(), subtitle.trimmed()}) {
        if (!line.isEmpty()) {
            head << line;
        }
    }
    QStringList blocks;
    if (!head.isEmpty()) {
        blocks << head.join(QLatin1Char('\n'));
    }
    for (const PublishingSetText& part : parts) {
        blocks << part.heading.toUpper() + QLatin1Char('\n') + part.text.trimmed();
    }
    return blocks.join(QStringLiteral("\n\n")) + QLatin1Char('\n');
}

QString PublishingTexts::setToMarkdown(const QString& title, const QString& subtitle,
                                       const QList<PublishingSetText>& parts) {
    QStringList blocks;
    if (!title.trimmed().isEmpty()) {
        blocks << QStringLiteral("# ") + title.trimmed();
    }
    if (!subtitle.trimmed().isEmpty()) {
        blocks << subtitle.trimmed();
    }
    for (const PublishingSetText& part : parts) {
        blocks << QStringLiteral("## ") + part.heading.trimmed();
        for (const QString& line : part.text.split(QLatin1Char('\n'))) {
            const QString paragraph = line.trimmed();
            if (!paragraph.isEmpty()) {
                blocks << (startsMarkdown(paragraph) ? QStringLiteral("\\") + paragraph
                                                     : paragraph);
            }
        }
    }
    return blocks.join(QStringLiteral("\n\n")) + QLatin1Char('\n');
}

int PublishingTexts::lengthOf(QStringView text, bool words) {
    return CoverMaterial::lengthOf(text, words);
}

}  // namespace kalahari::core
