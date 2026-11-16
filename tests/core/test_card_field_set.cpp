// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_card_field_set.cpp
/// @brief The fields of the cards of a kind as a book has them: the set read and written, the
/// fields of the kind in the writer's order, the fields the writer added, and those that new
/// books start with

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_package.h>
#include <kalahari/core/book_type_registry.h>
#include <kalahari/core/card_data.h>
#include <kalahari/core/card_field_set.h>
#include <kalahari/core/card_relations.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/settings_manager.h>

#include "test_support/window_parts.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QTemporaryDir>

#include <algorithm>

using kalahari::core::BookCardField;
using kalahari::core::BookPlace;
using kalahari::core::CardField;
using kalahari::core::CardFieldChoice;
using kalahari::core::CardFieldSets;
using kalahari::core::CardFieldType;
using kalahari::core::ElementField;
using kalahari::core::KindRef;
using kalahari::core::LocalizedText;
using kalahari::core::OwnRelation;
using kalahari::core::ProjectManager;
using kalahari::core::SettingsManager;
namespace card_fields = kalahari::core::card_fields;

namespace {

/// The kind of the cards of the characters of the Base
const char* const CHARACTER = "kalahari.base:character";

/// The built-in book types, loaded once
void loadBookTypes() {
    ProjectManager::loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(ProjectManager::installedBookTypes().problems().isEmpty());
}

/// A new book in @p dir open in @p pm, with the built-in book types
void openBook(ProjectManager& pm, const QTemporaryDir& dir) {
    loadBookTypes();
    REQUIRE(pm.createProject(dir.path(), "Cards", "Anna", "pl", true, "kalahari.novel"));
}

/// The kind @p kindId of the cards of the Base
KindRef baseKind(const ProjectManager& pm, const char* kindId) {
    const KindRef kind =
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QString::fromLatin1(kindId));
    REQUIRE(kind);
    return kind;
}

/// A new card of kind @p kindId of the Base titled @p title
QString addCard(ProjectManager& pm, const char* kindId, const QString& title) {
    const QString id = pm.addElement(baseKind(pm, kindId), title, BookPlace::Workshop);
    REQUIRE_FALSE(id.isEmpty());
    return id;
}

/// The book saved, closed and open again
void reopen(ProjectManager& pm) {
    const QString manifest = pm.getManifestPath();
    REQUIRE(pm.closeProject(false));
    REQUIRE(pm.openProject(manifest));
}

/// The ids of @p fields in their order
QStringList idsOf(const QList<CardField>& fields) {
    QStringList ids;
    for (const CardField& field : fields) {
        ids << field.id;
    }
    return ids;
}

/// The ids of the fields of @p fields shown on the full card, in their order
QStringList onCardOf(const QList<CardField>& fields) {
    QStringList ids;
    for (const CardField& field : fields) {
        if (field.onCard) {
            ids << field.id;
        }
    }
    return ids;
}

/// The entry of a field of the kind with id @p id, shown on the full card alone
BookCardField kindField(const char* id) {
    BookCardField field;
    field.id = QString::fromLatin1(id);
    field.onCard = true;
    return field;
}

/// A field of the writer with id @p id named @p name, of type @p type, shown on the full card
BookCardField ownField(const char* id, const QString& name, CardFieldType type) {
    BookCardField field;
    field.id = QString::fromLatin1(id);
    field.name = name;
    field.type = type;
    field.onCard = true;
    return field;
}

/// The relation of the writer with id @p id in the open book; nullptr when there is none
const OwnRelation* relationOf(const ProjectManager& pm, const QString& id) {
    const QList<OwnRelation>& relations = pm.project()->workshop.relations;
    const auto found = std::find_if(relations.cbegin(), relations.cend(),
                                    [&id](const OwnRelation& each) { return each.id == id; });
    return found == relations.cend() ? nullptr : &*found;
}

/// The entry with id @p id of the set of kind @p kind of the open book
BookCardField entryOf(const ProjectManager& pm, const KindRef& kind, const QString& id) {
    const QList<BookCardField> set = pm.cardFieldSet(kind);
    const auto found = std::find_if(set.cbegin(), set.cend(),
                                    [&id](const BookCardField& each) { return each.id == id; });
    REQUIRE(found != set.cend());
    return *found;
}

/// The fields that new books start with, put back as they were when the test ends
class DefaultsKept {
public:
    DefaultsKept()
        : m_before(SettingsManager::getInstance().get<nlohmann::json>(
              card_fields::DEFAULTS_SETTING)) {}
    ~DefaultsKept() {
        SettingsManager::getInstance().set<nlohmann::json>(card_fields::DEFAULTS_SETTING,
                                                           m_before);
    }
    DefaultsKept(const DefaultsKept&) = delete;
    DefaultsKept& operator=(const DefaultsKept&) = delete;
    DefaultsKept(DefaultsKept&&) = delete;
    DefaultsKept& operator=(DefaultsKept&&) = delete;

private:
    nlohmann::json m_before;
};

/// The project file of the open book as JSON
QJsonObject manifestOf(const ProjectManager& pm) {
    QFile manifest(pm.getManifestPath());
    REQUIRE(manifest.open(QIODevice::ReadOnly));
    return QJsonDocument::fromJson(manifest.readAll()).object();
}

}  // namespace

TEST_CASE("Card fields: a set is read and written back, with what this version does not know",
          "[core][cards][card_fields]") {
    const QJsonObject saved = QJsonDocument::fromJson(R"({
        "kalahari.base:character": [
            { "id": "role", "show": ["card", "properties", "list"] },
            { "id": "own-1", "name": "Rasa", "type": "choice", "show": ["card"],
              "choices": [ { "id": "c1", "name": "elf" }, { "id": "c2", "name": "krasnolud" } ] },
            { "id": "own-2", "name": "Mentor", "type": "card", "card": "character",
              "relation": "own-3", "inverse": "Uczeń", "show": ["card", "list"] },
            { "id": "future", "show": [], "colour": "#c0392b" }
        ] })")
                                  .object();

    const CardFieldSets sets = card_fields::read(saved);
    REQUIRE(sets.size() == 1);
    const QList<BookCardField> set = sets.value(QString::fromLatin1(CHARACTER));
    REQUIRE(set.size() == 4);

    CHECK(set.at(0).id == QStringLiteral("role"));
    CHECK_FALSE(set.at(0).isOwn());
    CHECK((set.at(0).onCard && set.at(0).inProperties && set.at(0).inList));

    const BookCardField& race = set.at(1);
    CHECK(race.isOwn());
    CHECK(race.name == QStringLiteral("Rasa"));
    CHECK(race.type == CardFieldType::Choice);
    REQUIRE(race.choices.size() == 2);
    CHECK(race.choices.at(1).id == QStringLiteral("c2"));
    CHECK(race.choices.at(1).name.text(QStringLiteral("pl")) == QStringLiteral("krasnolud"));
    CHECK((race.onCard && !race.inProperties && !race.inList));

    const BookCardField& mentor = set.at(2);
    CHECK(mentor.type == CardFieldType::Card);
    CHECK(mentor.cardKind == QStringLiteral("character"));
    CHECK(mentor.relation == QStringLiteral("own-3"));
    CHECK(mentor.inverseName == QStringLiteral("Uczeń"));

    CHECK_FALSE(set.at(3).isShown());
    CHECK(set.at(3).extra.value(QStringLiteral("colour")).toString() ==
          QStringLiteral("#c0392b"));

    // Written back as it was
    CHECK(card_fields::write(sets) == saved);

    // An entry without an id, or a field of the writer without a name, is not read; a set
    // with nothing that can be read is no set
    const QJsonObject broken = QJsonDocument::fromJson(R"({
        "kalahari.base:item": [ { "show": ["card"] }, { "id": "own-1", "name": " " } ] })")
                                   .object();
    CHECK(card_fields::read(broken).isEmpty());
    CHECK(card_fields::write(CardFieldSets()).isEmpty());
}

TEST_CASE("Card fields: the fields of a kind in the order of the set, the others hidden",
          "[core][cards][card_fields]") {
    loadBookTypes();
    const KindRef kind = ProjectManager::installedBookTypes().findKind(
        QStringLiteral("kalahari.base"), QStringLiteral("character"));
    REQUIRE(kind);

    // Without a set, the fields of the package, where it shows them
    const QList<BookCardField> ofPackage = card_fields::ofKind(*kind.kind);
    REQUIRE(ofPackage.size() == kind.kind->fields.size());
    CHECK(card_fields::complete(*kind.kind, {}) == ofPackage);
    CHECK(card_fields::isKindSet(*kind.kind, {}));
    CHECK(card_fields::isKindSet(*kind.kind, ofPackage));

    // The fields the set lists in its order (a field it lists twice, or that the kind does not
    // have, once or not at all), then the other fields of the kind, hidden
    const QList<BookCardField> set{kindField("age"),
                                   ownField("own-1", QStringLiteral("Rasa"), CardFieldType::Text),
                                   kindField("age"), kindField("unknown"), kindField("role")};
    const QList<BookCardField> fields = card_fields::complete(*kind.kind, set);
    REQUIRE(fields.size() == kind.kind->fields.size() + 1);
    CHECK(fields.at(0).id == QStringLiteral("age"));
    CHECK(fields.at(1).id == QStringLiteral("own-1"));
    CHECK(fields.at(2).id == QStringLiteral("role"));
    CHECK(fields.at(3).id == QStringLiteral("gender"));
    CHECK_FALSE(fields.at(3).isShown());
    CHECK_FALSE(card_fields::isKindSet(*kind.kind, set));

    // As the cards show them: a field of the kind as the package has it, where the set shows
    // it; a field of the writer as the set has it
    const QList<CardField> shown = card_fields::effective(*kind.kind, set);
    REQUIRE(shown.size() == fields.size());
    CHECK(shown.at(0).name.text(QStringLiteral("pl")) == QStringLiteral("Wiek"));
    CHECK((shown.at(0).onCard && !shown.at(0).inProperties && !shown.at(0).inList));
    CHECK(shown.at(1).name.text(QStringLiteral("pl")) == QStringLiteral("Rasa"));
    CHECK(shown.at(1).name.text(QStringLiteral("en")) == QStringLiteral("Rasa"));
    CHECK(shown.at(1).type == CardFieldType::Text);
    CHECK(shown.at(2).type == CardFieldType::Choice);
    CHECK(shown.at(2).subtitle);
    CHECK(onCardOf(shown) ==
          QStringList{QStringLiteral("age"), QStringLiteral("own-1"), QStringLiteral("role")});

    // What the package shows, in another order, is not the set of the package
    QList<BookCardField> reordered = ofPackage;
    reordered.move(0, 1);
    CHECK_FALSE(card_fields::isKindSet(*kind.kind, reordered));

    // An entry of a newer package is kept at the end
    BookCardField future = kindField("future");
    const QList<BookCardField> kept = card_fields::keepUnknown(*kind.kind, {future, set.at(1)},
                                                               ofPackage);
    REQUIRE(kept.size() == ofPackage.size() + 1);
    CHECK(kept.last().id == QStringLiteral("future"));
}

TEST_CASE("Card fields: new ids of the fields of the writer and of the values of their lists",
          "[core][cards][card_fields]") {
    const QList<BookCardField> set{ownField("own-2", QStringLiteral("Rasa"), CardFieldType::Text),
                                   kindField("role")};
    CHECK(card_fields::newOwnId({}, {}) == QStringLiteral("own-1"));
    CHECK(card_fields::newOwnId(set, {}) == QStringLiteral("own-3"));
    CHECK(card_fields::newOwnId(set, {QStringLiteral("own-5"), QStringLiteral("age")}) ==
          QStringLiteral("own-6"));

    const QList<CardFieldChoice> choices{
        CardFieldChoice{QStringLiteral("c1"), LocalizedText::plain(QStringLiteral("elf"))},
        CardFieldChoice{QStringLiteral("c3"), LocalizedText::plain(QStringLiteral("ork"))}};
    CHECK(card_fields::newChoiceId({}) == QStringLiteral("c1"));
    CHECK(card_fields::newChoiceId(choices) == QStringLiteral("c4"));
}

TEST_CASE("Card fields: the book keeps the fields of the cards as the writer set them",
          "[core][cards][card_fields][project_manager]") {
    const QTemporaryDir dir;
    auto& pm = kalahari::test::book();
    openBook(pm, dir);
    const KindRef character = baseKind(pm, "character");
    const QString anna = addCard(pm, "character", QStringLiteral("Anna"));
    REQUIRE(pm.setCardValue(anna, QStringLiteral("age"), QStringLiteral("30")));
    QStringList changedKinds;
    const QObject listening;  // The book outlives the test
    QObject::connect(&pm, &ProjectManager::cardFieldsChanged, &listening,
                     [&changedKinds](const QString& kind) { changedKinds << kind; });

    // A new book has the fields of the package
    CHECK_FALSE(pm.hasOwnCardFields(character));
    CHECK(pm.cardFieldSet(character) == card_fields::ofKind(*character.kind));
    CHECK(idsOf(pm.cardFields(character)).first() == QStringLiteral("role"));

    // The age goes first, on the full card alone; the occupation is hidden, and keeps the
    // values of the cards
    QList<BookCardField> set = pm.cardFieldSet(character);
    set.move(2, 0);
    set[0].inProperties = false;
    set[0].inList = false;
    set[3].onCard = false;
    set[3].inProperties = false;
    set[3].inList = false;
    REQUIRE(set.at(3).id == QStringLiteral("occupation"));
    REQUIRE(pm.setCardFields(character, set));
    CHECK(changedKinds == QStringList{QString::fromLatin1(CHARACTER)});
    CHECK(pm.hasOwnCardFields(character));
    CHECK(idsOf(pm.cardFields(character)).mid(0, 2) ==
          QStringList{QStringLiteral("age"), QStringLiteral("role")});
    REQUIRE(pm.cardField(character, QStringLiteral("age")) != nullptr);
    CHECK_FALSE(pm.cardField(character, QStringLiteral("age"))->inList);
    CHECK_FALSE(pm.cardField(character, QStringLiteral("occupation"))->onCard);
    CHECK(pm.cardFieldText(anna, QStringLiteral("age")) == QStringLiteral("30"));

    // The book keeps them
    reopen(pm);
    CHECK(pm.cardFieldSet(character) == set);
    CHECK(manifestOf(pm)[QStringLiteral("workshop")][QStringLiteral("cardFields")]
              .toObject()
              .contains(QString::fromLatin1(CHARACTER)));

    // The fields of the package need no set
    REQUIRE(pm.setCardFields(character, card_fields::ofKind(*character.kind)));
    CHECK_FALSE(pm.hasOwnCardFields(character));
    CHECK_FALSE(manifestOf(pm)[QStringLiteral("workshop")].toObject().contains(
        QStringLiteral("cardFields")));
    CHECK(onCardOf(pm.cardFields(character)).contains(QStringLiteral("occupation")));

    REQUIRE(pm.closeProject(false));
}

TEST_CASE("Card fields: a field of a newer package stays in the book",
          "[core][cards][card_fields][project_manager]") {
    const QTemporaryDir dir;
    auto& pm = kalahari::test::book();
    openBook(pm, dir);
    const KindRef character = baseKind(pm, "character");
    QList<BookCardField> set = pm.cardFieldSet(character);
    set.move(0, 1);
    REQUIRE(pm.setCardFields(character, set));

    // A newer version of the program added a field to the kind
    const QString path = pm.getManifestPath();
    REQUIRE(pm.closeProject(false));
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    QJsonObject manifest = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    QJsonObject workshop = manifest[QStringLiteral("workshop")].toObject();
    QJsonObject sets = workshop[QStringLiteral("cardFields")].toObject();
    QJsonArray fields = sets[QString::fromLatin1(CHARACTER)].toArray();
    fields.append(QJsonObject{{QStringLiteral("id"), QStringLiteral("future")},
                              {QStringLiteral("show"), QJsonArray{QStringLiteral("card")}}});
    sets[QString::fromLatin1(CHARACTER)] = fields;
    workshop[QStringLiteral("cardFields")] = sets;
    manifest[QStringLiteral("workshop")] = workshop;
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(QJsonDocument(manifest).toJson());
    file.close();
    REQUIRE(pm.openProject(path));

    // This version does not show it, and keeps it when the writer changes the fields, even
    // back to those of the package
    CHECK_FALSE(idsOf(pm.cardFields(character)).contains(QStringLiteral("future")));
    REQUIRE(pm.setCardFields(character, card_fields::ofKind(*character.kind)));
    CHECK(pm.hasOwnCardFields(character));
    const QJsonArray saved = manifestOf(pm)[QStringLiteral("workshop")][QStringLiteral(
                                 "cardFields")][QString::fromLatin1(CHARACTER)]
                                 .toArray();
    REQUIRE_FALSE(saved.isEmpty());
    CHECK(saved.last()[QStringLiteral("id")].toString() == QStringLiteral("future"));

    REQUIRE(pm.closeProject(false));
}

TEST_CASE("Card fields: a field of the writer that names a card has a relation named after it",
          "[core][cards][card_fields][links][project_manager]") {
    const QTemporaryDir dir;
    auto& pm = kalahari::test::book();
    openBook(pm, dir);
    const KindRef character = baseKind(pm, "character");
    const QString anna = addCard(pm, "character", QStringLiteral("Anna"));
    const QString jan = addCard(pm, "character", QStringLiteral("Jan"));
    addCard(pm, "character", QStringLiteral("Ola"));

    BookCardField mentor = ownField("own-1", QStringLiteral("Mentor"), CardFieldType::Card);
    mentor.cardKind = QStringLiteral("character");
    mentor.inverseName = QStringLiteral("Uczeń");
    QList<BookCardField> set = pm.cardFieldSet(character);
    set.prepend(mentor);
    REQUIRE(pm.setCardFields(character, set));

    const QString relationId = entryOf(pm, character, QStringLiteral("own-1")).relation;
    REQUIRE_FALSE(relationId.isEmpty());
    const OwnRelation* relation = relationOf(pm, relationId);
    REQUIRE(relation != nullptr);
    CHECK(relation->name == QStringLiteral("Mentor"));
    CHECK(relation->kind == QString::fromLatin1(CHARACTER));
    CHECK(relation->otherKind == QString::fromLatin1(CHARACTER));
    const QString inverseId = relation->inverse;
    REQUIRE(relationOf(pm, inverseId) != nullptr);
    CHECK(relationOf(pm, inverseId)->name == QStringLiteral("Uczeń"));

    // The field names a card by the relation
    REQUIRE(pm.setCardFieldText(anna, QStringLiteral("own-1"), QStringLiteral("Jan")));
    CHECK(pm.cardFieldText(anna, QStringLiteral("own-1")) == QStringLiteral("Jan"));
    REQUIRE(pm.findElement(anna)->linkTo(jan) != nullptr);
    CHECK(pm.findElement(anna)->linkTo(jan)->role == relationId);

    // Renamed, the field renames its relation, which keeps its links
    set = pm.cardFieldSet(character);
    set[0].name = QStringLiteral("Nauczyciel");
    set[0].inverseName = QStringLiteral("Wychowanek");
    REQUIRE(pm.setCardFields(character, set));
    CHECK(entryOf(pm, character, QStringLiteral("own-1")).relation == relationId);
    CHECK(relationOf(pm, relationId)->name == QStringLiteral("Nauczyciel"));
    CHECK(relationOf(pm, inverseId)->name == QStringLiteral("Wychowanek"));
    CHECK(pm.cardFieldText(anna, QStringLiteral("own-1")) == QStringLiteral("Jan"));

    // Named the same both ways, it keeps its relations
    set = pm.cardFieldSet(character);
    set[0].name = QStringLiteral("Druh");
    set[0].inverseName.clear();
    REQUIRE(pm.setCardFields(character, set));
    CHECK(entryOf(pm, character, QStringLiteral("own-1")).relation == relationId);
    CHECK(relationOf(pm, inverseId)->name == QStringLiteral("Druh"));

    // A relation the same both ways that gets another inverse name is another relation; the
    // links of the cards on both sides move to it
    BookCardField friendField = ownField("own-2", QStringLiteral("Przyjaciel"),
                                         CardFieldType::Card);
    friendField.cardKind = QStringLiteral("character");
    set = pm.cardFieldSet(character);
    set.insert(1, friendField);
    REQUIRE(pm.setCardFields(character, set));
    const QString friendId = entryOf(pm, character, QStringLiteral("own-2")).relation;
    REQUIRE(relationOf(pm, friendId) != nullptr);
    CHECK(relationOf(pm, friendId)->inverse == friendId);
    REQUIRE(pm.setCardFieldText(anna, QStringLiteral("own-2"), QStringLiteral("Ola")));
    REQUIRE(pm.setCardFieldText(jan, QStringLiteral("own-2"), QStringLiteral("Anna")));
    set = pm.cardFieldSet(character);
    set[1].inverseName = QStringLiteral("Przyjaciółka");
    REQUIRE(pm.setCardFields(character, set));
    const QString pair = entryOf(pm, character, QStringLiteral("own-2")).relation;
    REQUIRE(relationOf(pm, pair) != nullptr);
    CHECK(pair != friendId);
    CHECK(relationOf(pm, relationOf(pm, pair)->inverse)->name == QStringLiteral("Przyjaciółka"));
    CHECK(pm.cardFieldText(anna, QStringLiteral("own-2")) == QStringLiteral("Ola"));
    CHECK(pm.cardFieldText(jan, QStringLiteral("own-2")) == QStringLiteral("Anna"));

    // Named the same both ways again, it keeps the relations it has
    set = pm.cardFieldSet(character);
    set[1].inverseName.clear();
    REQUIRE(pm.setCardFields(character, set));
    CHECK(entryOf(pm, character, QStringLiteral("own-2")).relation == pair);
    CHECK(relationOf(pm, relationOf(pm, pair)->inverse)->name == QStringLiteral("Przyjaciel"));
    CHECK(pm.cardFieldText(anna, QStringLiteral("own-2")) == QStringLiteral("Ola"));
    CHECK(pm.cardFieldText(jan, QStringLiteral("own-2")) == QStringLiteral("Anna"));

    // Of another kind of cards, it has another relation; the cards it named keep theirs
    set = pm.cardFieldSet(character);
    set[0].cardKind = QStringLiteral("location");
    REQUIRE(pm.setCardFields(character, set));
    const QString places = entryOf(pm, character, QStringLiteral("own-1")).relation;
    REQUIRE_FALSE(places.isEmpty());
    CHECK(places != relationId);
    CHECK(relationOf(pm, places)->otherKind == QStringLiteral("kalahari.base:location"));
    CHECK(pm.findElement(anna)->linkTo(jan)->role == relationId);
    CHECK(pm.cardFieldText(anna, QStringLiteral("own-1")).isEmpty());

    reopen(pm);
    CHECK(entryOf(pm, character, QStringLiteral("own-1")).relation == places);
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("Card fields: an own field of a card made a field of all the cards of its kind",
          "[core][cards][card_fields][project_manager]") {
    const QTemporaryDir dir;
    auto& pm = kalahari::test::book();
    openBook(pm, dir);
    const KindRef character = baseKind(pm, "character");
    const QString anna = addCard(pm, "character", QStringLiteral("Anna"));
    const QString jan = addCard(pm, "character", QStringLiteral("Jan"));
    const QString krakow = addCard(pm, "location", QStringLiteral("Kraków"));
    const auto own = [](const char* name, const char* value) {
        return ElementField{QString::fromUtf8(name), QString::fromUtf8(value), {}};
    };
    REQUIRE(pm.setFields(anna, {own("Rasa", "elf"), own("Sposób mówienia", "cicho")}));
    REQUIRE(pm.setFields(jan, {own("Broń", "topór"), own(" rasa ", "krasnolud")}));
    REQUIRE(pm.setFields(krakow, {own("Rasa", "ludzie")}));

    // A name no field has: a new field of the writer, which takes the values of the fields of
    // that name of all the cards of the kind
    const QString race = pm.addFieldToKind(anna, 0);
    CHECK(race == QStringLiteral("own-1"));
    const CardField* field = pm.cardField(character, race);
    REQUIRE(field != nullptr);
    CHECK(field->name.text(QStringLiteral("pl")) == QStringLiteral("Rasa"));
    CHECK(field->type == CardFieldType::Text);
    CHECK(field->onCard);
    CHECK(pm.findElement(anna)->card.value(race) == QStringLiteral("elf"));
    CHECK(pm.findElement(jan)->card.value(race) == QStringLiteral("krasnolud"));
    REQUIRE(pm.findElement(anna)->fields.size() == 1);
    CHECK(pm.findElement(jan)->fields == QList<ElementField>{own("Broń", "topór")});
    // A card of another kind keeps its own field
    CHECK(pm.findElement(krakow)->fields.size() == 1);

    // The name of a field of the kind, hidden: shown on the card, with the values
    const QString speech = pm.addFieldToKind(anna, 0);
    CHECK(speech == QStringLiteral("speech"));
    CHECK(pm.cardField(character, speech)->onCard);
    CHECK(pm.findElement(anna)->card.value(speech) == QStringLiteral("cicho"));
    CHECK(pm.findElement(anna)->fields.isEmpty());

    // A value with several lines gives a longer text; the id of a field that went is not
    // taken again, since the cards keep its values
    QList<BookCardField> set = pm.cardFieldSet(character);
    set.erase(std::remove_if(set.begin(), set.end(),
                             [&race](const BookCardField& each) { return each.id == race; }),
              set.end());
    REQUIRE(pm.setCardFields(character, set));
    REQUIRE(pm.setFields(jan, {own("Przeszłość", "Wojna.\nPokój.")}));
    const QString past = pm.addFieldToKind(jan, 0);
    CHECK(past == QStringLiteral("own-2"));
    CHECK(pm.cardField(character, past)->type == CardFieldType::LongText);

    // Not a field that is not there, nor one without a name
    CHECK(pm.addFieldToKind(jan, 0).isEmpty());
    REQUIRE(pm.setFields(jan, {own(" ", "nic")}));
    CHECK(pm.addFieldToKind(jan, 0).isEmpty());
    CHECK(pm.addFieldToKind(krakow, 3).isEmpty());

    REQUIRE(pm.closeProject(false));
}

TEST_CASE("Card fields: a new book starts with the fields set in the Settings",
          "[core][cards][card_fields][project_manager]") {
    const DefaultsKept kept;

    BookCardField mentor = ownField("own-1", QStringLiteral("Mentor"), CardFieldType::Card);
    mentor.cardKind = QStringLiteral("character");
    mentor.relation = QStringLiteral("own-9");  // Of the book the set was made in
    CardFieldSets defaults;
    defaults.insert(QString::fromLatin1(CHARACTER), {kindField("age"), mentor});
    card_fields::setDefaults(defaults);
    CHECK(card_fields::defaults() == defaults);

    const QTemporaryDir dir;
    auto& pm = kalahari::test::book();
    openBook(pm, dir);
    const KindRef character = baseKind(pm, "character");

    CHECK(pm.hasOwnCardFields(character));
    CHECK(onCardOf(pm.cardFields(character)) ==
          QStringList{QStringLiteral("age"), QStringLiteral("own-1")});
    // The field has a relation of the new book
    const QString relationId = entryOf(pm, character, QStringLiteral("own-1")).relation;
    const OwnRelation* relation = relationOf(pm, relationId);
    REQUIRE(relation != nullptr);
    CHECK(relation->name == QStringLiteral("Mentor"));
    // The same both ways, when it has no inverse name
    CHECK(relation->inverse == relation->id);

    REQUIRE(pm.closeProject(false));
}
