// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_card_data.cpp
/// @brief The cards of the Workshop (characters, locations, items): the fields of their kinds,
/// their data in the book file, their colors, and their changes in an open book

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/card_data.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/settings_manager.h>

#include "test_support/window_parts.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <string>
#include <vector>

using namespace kalahari::core;

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
    ~ChangedPalette() { SettingsManager::getInstance().set(CardColors::PALETTE_SETTING, m_before); }

    ChangedPalette(const ChangedPalette&) = delete;
    ChangedPalette& operator=(const ChangedPalette&) = delete;

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
                                   { "image": "e3", "avatar": { "face": "oval" } } ],
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
    CHECK(card.portraits.at(1).extra.keys() == QStringList{QStringLiteral("avatar")});
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
                         "\"image\""));
    CHECK(problemOf(R"({ "id": "e1", "kind": "kalahari.base:character",
                         "card": { "portraits": [ { "label": "young" } ] } })") ==
          QStringLiteral("workshop.elements[0].card.portraits[0].image: must be a text that is "
                         "not empty"));
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
    QTemporaryDir dir;
    auto& pm = kalahari::test::book();
    pm.loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
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

    // Its short description is the description of the element
    REQUIRE(pm.setDescription(anna, QStringLiteral("Kartografka z Warszawy.")));
    CHECK(pm.findElement(anna)->description == QStringLiteral("Kartografka z Warszawy."));

    // The values of its fields, each saved at once, and told of
    QStringList changes;
    const QObject listener;  // the connection goes with it, also when a check stops the test
    QObject::connect(&pm, &ProjectManager::cardChanged, &listener,
                     [&changes](const QString& elementId) { changes << elementId; });
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
    data.portraits = {CardPortrait{portrait, QString(), {}}};
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
