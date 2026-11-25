// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_card_data.cpp
/// @brief The cards of the Workshop (characters, locations, items): the fields of their kinds,
/// their data in the book file, their colors, and their changes in an open book

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_package.h>
#include <kalahari/core/book_type_registry.h>
#include <kalahari/core/card_data.h>
#include <kalahari/core/material_kinds.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/settings_manager.h>

#include "kalahari_tests/test_support/window_parts.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

using kalahari::core::BookPlace;
using kalahari::core::BookProject;
using kalahari::core::BookTypeRegistry;
using kalahari::core::CardColors;
using kalahari::core::CardData;
using kalahari::core::CardField;
using kalahari::core::CardFieldChoice;
using kalahari::core::CardFieldType;
using kalahari::core::CardPortrait;
using kalahari::core::ElementField;
using kalahari::core::ElementForm;
using kalahari::core::KindRef;
using kalahari::core::MaterialKinds;
using kalahari::core::OwnRelation;
using kalahari::core::ProjectElement;
using kalahari::core::ProjectManager;
using kalahari::core::SettingsManager;
using kalahari::core::WorkshopGroup;

namespace {

QJsonObject jsonOf(const QByteArray& text) {
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(text, &error);
    INFO(error.errorString().toStdString());
    REQUIRE(document.isObject());
    return document.object();
}

/// A project with the Workshop elements @p elements (JSON)
QByteArray projectWith(const QByteArray& elements) {
    return R"({ "format": 2, "id": "p",
                "books": [ { "id": "b", "folder": "book" } ],
                "workshop": { "grouped": false, "elements": [ )" +
           elements + "] } }";
}

/// The built-in book types of the source tree
const BookTypeRegistry& builtIn() {
    static BookTypeRegistry registry;
    if (registry.packages().isEmpty()) {
        registry.load({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    }
    REQUIRE(registry.problems().isEmpty());
    return registry;
}

/// Ids of @p fields, joined with ", "
std::string idsOf(const QList<CardField>& fields) {
    QStringList ids;
    for (const CardField& field : fields) {
        ids << field.id;
    }
    return ids.join(QStringLiteral(", ")).toStdString();
}

/// Ids of the fields of @p fields shown on the full card, joined with ", "
std::string shownOnCard(const QList<CardField>& fields) {
    QList<CardField> shown;
    for (const CardField& field : fields) {
        if (field.onCard) {
            shown << field;
        }
    }
    return idsOf(shown);
}

/// The palette of the Settings changed for a test, and set back afterwards
class ChangedPalette {
public:
    explicit ChangedPalette(const std::vector<std::string>& palette)
        : m_before(SettingsManager::getInstance().get<std::vector<std::string>>(
              CardColors::PALETTE_SETTING)) {
        SettingsManager::getInstance().set(CardColors::PALETTE_SETTING, palette);
    }
    ~ChangedPalette() {
        try {
            SettingsManager::getInstance().set(CardColors::PALETTE_SETTING, m_before);
        } catch (...) {
            // A destructor may not throw: the later tests then find the palette of this one
            static_cast<void>(
                std::fputs("ChangedPalette: the palette of the test stays\n", stderr));
        }
    }

    ChangedPalette(const ChangedPalette&) = delete;
    ChangedPalette& operator=(const ChangedPalette&) = delete;
    ChangedPalette(ChangedPalette&&) = delete;
    ChangedPalette& operator=(ChangedPalette&&) = delete;

private:
    std::vector<std::string> m_before;
};

}  // namespace

TEST_CASE("Cards: the kinds of cards of the Base and their fields", "[core][cards]") {
    const KindRef character = builtIn().findKind(QStringLiteral("kalahari.base"),
                                                 QStringLiteral("character"));
    const KindRef location = builtIn().findKind(QStringLiteral("kalahari.base"),
                                                QStringLiteral("location"));
    const KindRef item = builtIn().findKind(QStringLiteral("kalahari.base"),
                                            QStringLiteral("item"));
    REQUIRE(character);
    REQUIRE(location);
    REQUIRE(item);

    // A card is a text whose text is its long description, opened in the card editor
    for (const KindRef& kind : {character, location, item}) {
        CHECK(kind.kind->form == ElementForm::Text);
        CHECK(kind.kind->isCard());
        CHECK_FALSE(kind.kind->isMaterial());
        CHECK(kind.kind->workshopGroup == WorkshopGroup::Libraries);
    }
    const KindRef note = builtIn().findKind(QStringLiteral("kalahari.base"),
                                            QStringLiteral("work_note"));
    REQUIRE(note);
    CHECK_FALSE(note.kind->isCard());
    CHECK(note.kind->fields.isEmpty());
    CHECK(note.kind->subtitleField() == nullptr);

    // The fields in the order of the card; some are hidden until the writer shows them
    CHECK(idsOf(character.kind->fields) ==
          "role, gender, age, occupation, appearance, personality, goal, origin, motivation, "
          "secret, change, speech");
    CHECK(shownOnCard(character.kind->fields) ==
          "role, gender, age, occupation, appearance, personality, goal");
    CHECK(idsOf(location.kind->fields) ==
          "type, parent, appearance, senses, mood, ruler, climate, history");
    CHECK(shownOnCard(location.kind->fields) == "type, parent, appearance, senses, mood");
    CHECK(idsOf(item.kind->fields) ==
          "type, owner, place, appearance, meaning, origin, power");
    CHECK(shownOnCard(item.kind->fields) == "type, owner, place, appearance, meaning");

    // The role stands beside the kind, chosen from a list
    const CardField* role = character.kind->subtitleField();
    REQUIRE(role != nullptr);
    CHECK(role->id == QStringLiteral("role"));
    CHECK(role->type == CardFieldType::Choice);
    CHECK(role->onCard);
    CHECK(role->inProperties);
    CHECK(role->inList);
    REQUIRE(role->choices.size() == 4);
    CHECK(role->choices.first().id == QStringLiteral("main"));
    CHECK(role->choices.first().name.text(QStringLiteral("pl")) == QStringLiteral("główna"));
    CHECK(role->choice(QStringLiteral("minor"))->name.text(QStringLiteral("en")) ==
          QStringLiteral("minor"));
    CHECK(role->choice(QStringLiteral("hero")) == nullptr);
    CHECK(character.kind->field(QStringLiteral("role")) == role);
    CHECK(character.kind->field(QStringLiteral("race")) == nullptr);

    // A field of a card names another card
    const CardField* origin = character.kind->field(QStringLiteral("origin"));
    REQUIRE(origin != nullptr);
    CHECK(origin->type == CardFieldType::Card);
    CHECK(origin->cardKind == QStringLiteral("location"));
    CHECK_FALSE(origin->onCard);
    CHECK(item.kind->field(QStringLiteral("owner"))->cardKind == QStringLiteral("character"));
    CHECK(location.kind->field(QStringLiteral("parent"))->cardKind == QStringLiteral("location"));
    CHECK(character.kind->field(QStringLiteral("appearance"))->type == CardFieldType::LongText);
    CHECK(character.kind->field(QStringLiteral("age"))->type == CardFieldType::Text);
    CHECK(item.kind->subtitleField()->type == CardFieldType::Text);

    // Every field and every value of a list has a Polish and an English name
    for (const KindRef& kind : {character, location, item}) {
        for (const CardField& field : kind.kind->fields) {
            INFO(field.id.toStdString());
            CHECK(field.name.values.contains(QStringLiteral("pl")));
            CHECK(field.name.values.contains(QStringLiteral("en")));
            for (const CardFieldChoice& choice : field.choices) {
                CHECK(choice.name.values.contains(QStringLiteral("pl")));
                CHECK(choice.name.values.contains(QStringLiteral("en")));
            }
        }
    }

    // The names of the types in packages
    for (const CardFieldType type : {CardFieldType::Text, CardFieldType::LongText,
                                     CardFieldType::Choice, CardFieldType::Card}) {
        CHECK(CardField::typeFromName(CardField::typeName(type)) == type);
    }
    CHECK_FALSE(CardField::typeFromName(QStringLiteral("number")).has_value());
}

TEST_CASE("Cards: their data in the book file", "[core][cards]") {
    const QByteArray elements = R"(
        { "id": "e1", "kind": "kalahari.base:character", "title": "Anna Wolska",
          "file": "workshop/character_001.kchapter", "status": "draft",
          "description": "Kartografka z Warszawy.",
          "card": { "values": { "role": "main", "age": "29", "race": "człowiek" },
                    "color": "#c0504d",
                    "portraits": [ { "image": "e2", "label": "główny" },
                                   { "image": "e3", "frame": { "x": 10 } } ],
                    "board": { "column": 2 } } },
        { "id": "e2", "kind": "kalahari.base:picture", "title": "Anna",
          "file": "workshop/anna.png" },
        { "id": "e3", "kind": "kalahari.base:picture", "title": "Anna, portret",
          "file": "workshop/anna-portret.png" })";
    QStringList problems;
    const std::optional<BookProject> project =
        BookProject::fromJson(jsonOf(projectWith(elements)), problems);
    INFO(problems.join(QStringLiteral("; ")).toStdString());
    REQUIRE(project.has_value());
    REQUIRE(problems.isEmpty());

    const ProjectElement& anna = project->workshop.elements.at(0);
    CHECK(anna.description == QStringLiteral("Kartografka z Warszawy."));
    const CardData& card = anna.card;
    CHECK(card.values.size() == 3);
    CHECK(card.value(QStringLiteral("role")) == QStringLiteral("main"));
    CHECK(card.value(QStringLiteral("age")) == QStringLiteral("29"));
    // A value of a field the kind does not have stays: the writer's own field of the book
    CHECK(card.value(QStringLiteral("race")) == QStringLiteral("człowiek"));
    CHECK(card.value(QStringLiteral("goal")).isEmpty());
    CHECK(card.color == QStringLiteral("#c0504d"));
    REQUIRE(card.portraits.size() == 2);
    CHECK(card.portraits.at(0).image == QStringLiteral("e2"));
    CHECK(card.portraits.at(0).label == QStringLiteral("główny"));
    CHECK(card.portraits.at(1).label.isEmpty());
    CHECK(card.portraitImage() == QStringLiteral("e2"));
    CHECK(card.hasPortrait(QStringLiteral("e3")));
    CHECK_FALSE(card.hasPortrait(QStringLiteral("e1")));
    // Fields this version does not know are kept, and saved back
    CHECK(card.portraits.at(1).extra.keys() == QStringList{QStringLiteral("frame")});
    CHECK(card.extra.keys() == QStringList{QStringLiteral("board")});
    CHECK(anna.extra.isEmpty());

    // Another element has none
    CHECK(project->workshop.elements.at(1).card.isEmpty());
    CHECK(project->workshop.elements.at(1).card.portraitImage().isEmpty());

    // The book is saved as it was read
    CHECK(project->toJson()[QStringLiteral("workshop")] ==
          jsonOf(projectWith(elements))[QStringLiteral("workshop")]);

    // A change of the data changes the project; data emptied leaves no "card"
    BookProject changed = *project;
    changed.workshop.elements[0].card.values.insert(QStringLiteral("goal"), QStringLiteral("x"));
    CHECK_FALSE(changed == *project);
    changed.workshop.elements[0].card = CardData{};
    CHECK(changed.workshop.elements[0].card.isEmpty());
    const QJsonObject written = changed.toJson()[QStringLiteral("workshop")]
                                    [QStringLiteral("elements")][0].toObject();
    CHECK_FALSE(written.contains(QStringLiteral("card")));
    changed.workshop.elements[0].card.color = QStringLiteral("#123456");
    const QJsonObject colored = changed.toJson()[QStringLiteral("workshop")]
                                    [QStringLiteral("elements")][0][QStringLiteral("card")]
                                        .toObject();
    CHECK(colored.keys() == QStringList{QStringLiteral("color")});
}

TEST_CASE("Cards: problems of their data", "[core][cards]") {
    const auto problemOf = [](const QByteArray& element) {
        QStringList problems;
        BookProject::fromJson(jsonOf(projectWith(element)), problems);
        return problems.join(QStringLiteral("; "));
    };
    CHECK(problemOf(R"({ "id": "e1", "kind": "kalahari.base:character", "card": [] })") ==
          QStringLiteral("workshop.elements[0].card: must be an object"));
    CHECK(problemOf(R"({ "id": "e1", "kind": "kalahari.base:character",
                         "card": { "values": ["main"] } })") ==
          QStringLiteral("workshop.elements[0].card.values: must be an object of fields and "
                         "their values"));
    CHECK(problemOf(R"({ "id": "e1", "kind": "kalahari.base:character",
                         "card": { "values": { "age": 29 } } })") ==
          QStringLiteral("workshop.elements[0].card.values: 'age' must be a text"));
    CHECK(problemOf(R"({ "id": "e1", "kind": "kalahari.base:character",
                         "card": { "color": "red" } })") ==
          QStringLiteral("workshop.elements[0].card.color: must be a color such as "
                         "\"#c0392b\""));
    CHECK(problemOf(R"({ "id": "e1", "kind": "kalahari.base:character",
                         "card": { "portraits": [ "e2" ] } })") ==
          QStringLiteral("workshop.elements[0].card.portraits[0]: must be an object with "
                         "\"image\" or \"avatar\""));
    CHECK(problemOf(R"({ "id": "e1", "kind": "kalahari.base:character",
                         "card": { "portraits": [ { "label": "young" } ] } })") ==
          QStringLiteral("workshop.elements[0].card.portraits[0]: must have \"image\" or "
                         "\"avatar\""));
    CHECK(problemOf(R"({ "id": "e1", "kind": "kalahari.base:character",
                         "card": { "portraits": [ { "avatar": "oval" } ] } })") ==
          QStringLiteral("workshop.elements[0].card.portraits[0].avatar: must be an object of "
                         "the parts and colors; workshop.elements[0].card.portraits[0]: must "
                         "have \"image\" or \"avatar\""));
}

TEST_CASE("Cards: portraits put together from parts, in the book file", "[core][cards]") {
    const QByteArray element = R"({ "id": "e1", "kind": "kalahari.base:character",
        "card": { "portraits": [
            { "label": "main", "style": "line", "since": 3,
              "avatar": { "face": "oval", "hair": "mohawk", "skin": "#eec6a6", "wings": 2 } },
            { "image": "e31", "label": "youth" } ] } })";
    QStringList problems;
    const auto project = BookProject::fromJson(jsonOf(projectWith(element)), problems);
    REQUIRE(project);
    CHECK(problems.isEmpty());
    const CardData& card = project->workshop.elements[0].card;
    REQUIRE(card.portraits.size() == 2);
    const CardPortrait& main = card.portraits[0];
    CHECK(main.isComposed());
    CHECK(main.label == QStringLiteral("main"));
    CHECK(main.style == QStringLiteral("line"));
    // The parts this version does not know, and the values, come back as they were
    CHECK(main.avatar.value(QStringLiteral("hair")).toString() == QStringLiteral("mohawk"));
    CHECK(main.avatar.value(QStringLiteral("wings")).toInt() == 2);
    CHECK(main.extra.value(QStringLiteral("since")).toInt() == 3);
    CHECK(card.mainPortrait() == card.portraits.data());
    // The main portrait has no picture; the second one is a picture
    CHECK(card.portraitImage().isEmpty());
    CHECK_FALSE(card.portraits[1].isComposed());
    CHECK(card.hasPortrait(QStringLiteral("e31")));

    const QJsonArray written =
        project->toJson()[QStringLiteral("workshop")][QStringLiteral("elements")][0]
                         [QStringLiteral("card")][QStringLiteral("portraits")]
                             .toArray();
    REQUIRE(written.size() == 2);
    const QJsonObject first = written[0].toObject();
    CHECK_FALSE(first.contains(QStringLiteral("image")));
    CHECK(first[QStringLiteral("style")].toString() == QStringLiteral("line"));
    CHECK(first[QStringLiteral("avatar")].toObject() == main.avatar);
    CHECK(first[QStringLiteral("since")].toInt() == 3);
    const QJsonObject second = written[1].toObject();
    CHECK(second[QStringLiteral("image")].toString() == QStringLiteral("e31"));
    CHECK_FALSE(second.contains(QStringLiteral("avatar")));
    CHECK_FALSE(second.contains(QStringLiteral("style")));
}

TEST_CASE("Cards: the relations the writer named, in the book file", "[core][cards]") {
    // A project whose Workshop has the relations @p relations (JSON)
    const auto withRelations = [](const QByteArray& relations) {
        return jsonOf(R"({ "format": 2, "id": "p",
                           "books": [ { "id": "b", "folder": "book" } ],
                           "workshop": { "grouped": false, "elements": [],
                                         "relations": )" +
                      relations + " } }");
    };
    const QByteArray relations = R"([
        { "id": "own-1", "kind": "kalahari.base:character", "other": "kalahari.base:character",
          "name": "wierzyciel", "inverse": "own-2", "since": 2 },
        { "id": "own-2", "kind": "kalahari.base:character", "other": "kalahari.base:character",
          "name": "dłużnik", "inverse": "own-1" },
        { "id": "own-3", "kind": "kalahari.base:character", "other": "kalahari.base:location",
          "name": "kryjówka", "inverse": "own-4" },
        { "id": "own-4", "kind": "kalahari.base:location", "other": "kalahari.base:character",
          "name": "ukrywa się tu", "inverse": "own-3" },
        { "id": "own-5", "kind": "kalahari.base:character", "other": "kalahari.base:character",
          "name": "sąsiad" } ])";
    QStringList problems;
    const std::optional<BookProject> project =
        BookProject::fromJson(withRelations(relations), problems);
    INFO(problems.join(QStringLiteral("; ")).toStdString());
    REQUIRE(project.has_value());
    const QList<OwnRelation>& named = project->workshop.relations;
    REQUIRE(named.size() == 5);
    CHECK(named.at(0).id == QStringLiteral("own-1"));
    CHECK(named.at(0).kind == QStringLiteral("kalahari.base:character"));
    CHECK(named.at(2).otherKind == QStringLiteral("kalahari.base:location"));
    CHECK(named.at(1).name == QStringLiteral("dłużnik"));
    CHECK(named.at(0).inverse == QStringLiteral("own-2"));
    // Without an inverse, the same both ways
    CHECK(named.at(4).inverse == QStringLiteral("own-5"));
    // Fields this version does not know are kept
    CHECK(named.at(0).extra.keys() == QStringList{QStringLiteral("since")});
    // The book is saved as it was read
    CHECK(project->toJson()[QStringLiteral("workshop")] ==
          withRelations(relations)[QStringLiteral("workshop")]);

    const auto problemOf = [&withRelations](const QByteArray& list) {
        QStringList found;
        CHECK_FALSE(BookProject::fromJson(withRelations(list), found).has_value());
        return found.join(QStringLiteral("; "));
    };
    CHECK(problemOf(R"([ "own-1" ])") ==
          QStringLiteral("workshop.relations[0]: must be an object with \"id\""));
    CHECK(problemOf(R"([ { "id": "own-1", "kind": "kalahari.base:character",
                           "other": "kalahari.base:character" } ])") ==
          QStringLiteral("workshop.relations[0].name: must be a text that is not empty"));
    CHECK(problemOf(R"([ { "id": "friend", "kind": "kalahari.base:character",
                           "other": "kalahari.base:character", "name": "przyjaciel" } ])") ==
          QStringLiteral("workshop.relations[0].id: 'friend' must start with \"own-\""));
    CHECK(problemOf(R"([ { "id": "own-1", "kind": "kalahari.base:character",
                           "other": "kalahari.base:character", "name": "sąsiad" },
                         { "id": "own-1", "kind": "kalahari.base:character",
                           "other": "kalahari.base:character", "name": "sąsiadka" } ])") ==
          QStringLiteral("workshop.relations[1].id: 'own-1' is listed twice"));
    CHECK(problemOf(R"([ { "id": "own-1", "kind": "kalahari.base:character",
                           "other": "kalahari.base:character", "name": "wierzyciel",
                           "inverse": "own-2" } ])") ==
          QStringLiteral("workshop.relations[0].inverse: 'own-2' is no relation of the list"));
    CHECK(problemOf(R"([ { "id": "own-1", "kind": "kalahari.base:character",
                           "other": "kalahari.base:character", "name": "wierzyciel",
                           "inverse": "own-2" },
                         { "id": "own-2", "kind": "kalahari.base:character",
                           "other": "kalahari.base:character", "name": "dłużnik" } ])") ==
          QStringLiteral("workshop.relations[0].inverse: 'own-2' must be the relation between "
                         "the same kinds the other way round, with 'own-1' as its inverse"));
    CHECK(problemOf(R"([ { "id": "own-1", "kind": "kalahari.base:character",
                           "other": "kalahari.base:location", "name": "kryjówka",
                           "inverse": "own-2" },
                         { "id": "own-2", "kind": "kalahari.base:character",
                           "other": "kalahari.base:location", "name": "ukrywa się tu",
                           "inverse": "own-1" } ])") ==
          QStringLiteral("workshop.relations[0].inverse: 'own-2' must be the relation between "
                         "the same kinds the other way round, with 'own-1' as its inverse; "
                         "workshop.relations[1].inverse: 'own-1' must be the relation between "
                         "the same kinds the other way round, with 'own-2' as its inverse"));
}

TEST_CASE("Cards: the color of a new card", "[core][cards]") {
    const QStringList palette{QStringLiteral("#111111"), QStringLiteral("#222222"),
                              QStringLiteral("#333333")};
    // The colors of the palette in order, then the one that the fewest cards have
    CHECK(CardColors::next(palette, {}) == QStringLiteral("#111111"));
    CHECK(CardColors::next(palette, {QStringLiteral("#111111")}) == QStringLiteral("#222222"));
    CHECK(CardColors::next(palette, {QStringLiteral("#111111"), QStringLiteral("#222222")}) ==
          QStringLiteral("#333333"));
    CHECK(CardColors::next(palette, {QStringLiteral("#111111"), QStringLiteral("#222222"),
                                     QStringLiteral("#333333")}) == QStringLiteral("#111111"));
    CHECK(CardColors::next(palette, {QStringLiteral("#111111"), QStringLiteral("#111111"),
                                     QStringLiteral("#222222"), QStringLiteral("#333333")}) ==
          QStringLiteral("#222222"));
    // A color chosen by the writer outside the palette counts for nothing; letters in either
    // case are one color
    CHECK(CardColors::next(palette, {QStringLiteral("#ABCDEF"), QStringLiteral("#111111")}) ==
          QStringLiteral("#222222"));
    CHECK(CardColors::next({QStringLiteral("#abcdef")}, {QStringLiteral("#ABCDEF")}) ==
          QStringLiteral("#abcdef"));
    CHECK(CardColors::next({}, {}).isEmpty());

    // The palette of the Settings, without what is no color; the program's when it has none
    CHECK(CardColors::defaultPalette().size() == 12);
    {
        const ChangedPalette changed({"#AA0000", "no color", "#00aa00"});
        CHECK(CardColors::palette() ==
              QStringList({QStringLiteral("#aa0000"), QStringLiteral("#00aa00")}));
    }
    {
        const ChangedPalette changed({});
        CHECK(CardColors::palette() == CardColors::defaultPalette());
    }
}

TEST_CASE("Cards: adding a card, its fields, color and portraits in an open book",
          "[core][cards][project_manager]") {
    const QTemporaryDir dir;
    auto& pm = kalahari::test::book();
    ProjectManager::loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(pm.bookTypes().problems().isEmpty());
    REQUIRE(pm.createProject(dir.path(), "Cards", "Anna", "pl", true, "kalahari.novel"));
    const ChangedPalette changed({"#111111", "#222222"});

    const KindRef character = pm.bookTypes().findKind(QStringLiteral("kalahari.base"),
                                                      QStringLiteral("character"));
    const KindRef location = pm.bookTypes().findKind(QStringLiteral("kalahari.base"),
                                                     QStringLiteral("location"));
    REQUIRE(character);
    REQUIRE(location);
    CHECK(ProjectManager::canAdd(character));

    // A card is a text of the Workshop with a color of the palette, the next one for each
    const QString anna = pm.addElement(character, "Anna Wolska", BookPlace::Workshop);
    REQUIRE_FALSE(anna.isEmpty());
    const ProjectElement* card = pm.findElement(anna);
    REQUIRE(card != nullptr);
    CHECK(pm.isCard(*card));
    CHECK(card->file == QStringLiteral("workshop/character_001.kchapter"));
    CHECK(card->status == QStringLiteral("draft"));
    CHECK(QFileInfo::exists(pm.filePathOf(*card)));
    CHECK(card->card.color == QStringLiteral("#111111"));
    const QString warsaw = pm.addElement(location, "Warszawa", BookPlace::Workshop);
    REQUIRE_FALSE(warsaw.isEmpty());
    CHECK(pm.findElement(warsaw)->card.color == QStringLiteral("#222222"));
    CHECK(pm.newCardColor() == QStringLiteral("#111111"));

    QStringList changes;
    const QObject listener;  // the connection goes with it, also when a check stops the test
    QObject::connect(&pm, &ProjectManager::cardChanged, &listener,
                     [&changes](const QString& elementId) { changes << elementId; });

    // Its short description is the description of the element
    REQUIRE(pm.setDescription(anna, QStringLiteral("Kartografka z Warszawy.")));
    CHECK(pm.findElement(anna)->description == QStringLiteral("Kartografka z Warszawy."));
    CHECK(changes == QStringList{anna});
    REQUIRE(pm.setDescription(anna, QStringLiteral("Kartografka z Warszawy.")));
    CHECK(changes.size() == 1);  // nothing changed
    changes.clear();

    // So are its own fields
    const QList<ElementField> fields{
        ElementField{QStringLiteral("Eyes"), QStringLiteral("grey"), {}}};
    REQUIRE(pm.setFields(anna, fields));
    CHECK(changes == QStringList{anna});
    REQUIRE(pm.setFields(anna, fields));
    CHECK(changes.size() == 1);  // nothing changed
    changes.clear();

    // The values of its fields, each saved at once, and told of
    REQUIRE(pm.setCardValue(anna, QStringLiteral("role"), QStringLiteral("main")));
    REQUIRE(pm.setCardValue(anna, QStringLiteral("age"), QStringLiteral("29")));
    CHECK(changes.count() == 2);
    CHECK(changes.at(0) == anna);
    REQUIRE(pm.setCardValue(anna, QStringLiteral("age"), QStringLiteral("29")));
    CHECK(changes.count() == 2);  // nothing changed
    REQUIRE(pm.setCardValue(anna, QStringLiteral("age"), QString()));
    CHECK_FALSE(pm.findElement(anna)->card.values.contains(QStringLiteral("age")));
    CHECK(changes.count() == 3);

    // Its color and its portrait, a picture of the materials
    const QString picture = QDir(dir.path()).filePath("anna.png");
    {
        QFile file(picture);
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write("a picture");
    }
    const QString portrait = pm.addMaterial(
        picture, pm.bookTypes().findKind(QStringLiteral("kalahari.base"),
                                         QStringLiteral("picture")),
        "Anna");
    REQUIRE_FALSE(portrait.isEmpty());
    CardData data = pm.findElement(anna)->card;
    data.color = QStringLiteral("#abcdef");
    data.portraits = {CardPortrait{portrait, QString(), QString(), {}, {}}};
    REQUIRE(pm.setCard(anna, data));
    CHECK(changes.count() == 4);

    // Not a card: nothing changes
    CHECK_FALSE(pm.setCard(portrait, data));
    CHECK_FALSE(pm.setCardValue(QStringLiteral("no such element"), QStringLiteral("age"),
                                QStringLiteral("1")));
    CHECK(pm.findElement(portrait)->card.isEmpty());

    // The book keeps it all
    const QString manifest = pm.getManifestPath();
    REQUIRE(pm.closeProject(false));
    REQUIRE(pm.openProject(manifest));
    const ProjectElement* reopened = pm.findElement(anna);
    REQUIRE(reopened != nullptr);
    CHECK(reopened->card.value(QStringLiteral("role")) == QStringLiteral("main"));
    CHECK(reopened->card.color == QStringLiteral("#abcdef"));
    CHECK(reopened->card.portraitImage() == portrait);
    CHECK(reopened->description == QStringLiteral("Kartografka z Warszawy."));

    // A picture deleted takes its portrait with it
    changes.clear();
    REQUIRE(pm.removeElement(portrait).has_value());
    CHECK(pm.findElement(anna)->card.portraits.isEmpty());
    CHECK(changes == QStringList{anna});
    CHECK(pm.findElement(anna)->card.color == QStringLiteral("#abcdef"));

    REQUIRE(pm.closeProject(false));
}

TEST_CASE("Cards: the names that a field of a card suggests, and the pictures of a portrait",
          "[core][cards][project_manager]") {
    const QTemporaryDir dir;
    auto& pm = kalahari::test::book();
    ProjectManager::loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});

    // Without a book: none
    REQUIRE_FALSE(pm.isProjectOpen());
    CHECK(pm.cardTitles(QStringLiteral("kalahari.base"), QStringLiteral("character")).isEmpty());
    CHECK(pm.pictureMaterials().isEmpty());

    REQUIRE(pm.createProject(dir.path(), "Cards", "Anna", "pl", true, "kalahari.novel"));
    const KindRef character = pm.bookTypes().findKind(QStringLiteral("kalahari.base"),
                                                      QStringLiteral("character"));
    const KindRef location = pm.bookTypes().findKind(QStringLiteral("kalahari.base"),
                                                     QStringLiteral("location"));
    REQUIRE(character);
    REQUIRE(location);

    // The cards of the kind, each name once, sorted without regard to case, the spaces
    // around them left out
    const QString zofia = pm.addElement(character, "Zofia", BookPlace::Workshop);
    const QString anna = pm.addElement(character, " anna ", BookPlace::Workshop);
    const QString bolek = pm.addElement(character, "Bolek", BookPlace::Workshop);
    const QString again = pm.addElement(character, "Bolek", BookPlace::Workshop);
    const QString warsaw = pm.addElement(location, "Warszawa", BookPlace::Workshop);
    for (const QString& id : {zofia, anna, bolek, again, warsaw}) {
        REQUIRE_FALSE(id.isEmpty());
    }
    CHECK(pm.cardTitles(QStringLiteral("kalahari.base"), QStringLiteral("character")) ==
          QStringList{QStringLiteral("anna"), QStringLiteral("Bolek"), QStringLiteral("Zofia")});
    // Without the card that asks
    CHECK(pm.cardTitles(QStringLiteral("kalahari.base"), QStringLiteral("character"), zofia) ==
          QStringList{QStringLiteral("anna"), QStringLiteral("Bolek")});
    // Another card of the same name keeps it
    CHECK(pm.cardTitles(QStringLiteral("kalahari.base"), QStringLiteral("character"), bolek) ==
          QStringList{QStringLiteral("anna"), QStringLiteral("Bolek"), QStringLiteral("Zofia")});
    CHECK(pm.cardTitles(QStringLiteral("kalahari.base"), QStringLiteral("location")) ==
          QStringList{QStringLiteral("Warszawa")});
    CHECK(pm.cardTitles(QStringLiteral("kalahari.base"), QStringLiteral("item")).isEmpty());
    CHECK(pm.cardTitles(QStringLiteral("other.package"), QStringLiteral("character")).isEmpty());

    // The materials whose file is a picture, in the order of the Workshop
    const auto material = [&pm, &dir](const QString& name, const QString& title) {
        const QString path = QDir(dir.path()).filePath(name);
        QFile file(path);
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write("data");
        file.close();
        const QString id = pm.addMaterial(
            path, pm.bookTypes().findKind(QStringLiteral("kalahari.base"),
                                          MaterialKinds::ofFile(path)),
            title);
        REQUIRE_FALSE(id.isEmpty());
        return id;
    };
    CHECK(pm.pictureMaterials().isEmpty());
    const QString map = material(QStringLiteral("map.png"), QStringLiteral("Map"));
    material(QStringLiteral("notes.pdf"), QStringLiteral("Notes"));
    const QString face = material(QStringLiteral("face.JPG"), QStringLiteral("Face"));
    QStringList pictures;
    for (const ProjectElement* picture : pm.pictureMaterials()) {
        pictures << picture->id;
    }
    CHECK(pictures == QStringList{map, face});

    REQUIRE(pm.closeProject(false));
}
