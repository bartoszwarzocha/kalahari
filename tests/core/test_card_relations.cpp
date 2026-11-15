// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_card_relations.cpp
/// @brief The relations between the cards of an open book: added on one card and shown on
/// both, the fields of the type "card" that name a card by a relation, and the relations the
/// writer names

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_package.h>
#include <kalahari/core/book_type_registry.h>
#include <kalahari/core/card_data.h>
#include <kalahari/core/card_relations.h>
#include <kalahari/core/project_manager.h>

#include "test_support/window_parts.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QTemporaryDir>

using kalahari::core::BookPlace;
using kalahari::core::CardRelations;
using kalahari::core::ElementLink;
using kalahari::core::KindRef;
using kalahari::core::OwnRelation;
using kalahari::core::ProjectElement;
using kalahari::core::ProjectManager;
using kalahari::core::RelationInfo;
using kalahari::core::ShownRelation;

namespace {

/// A new book in @p dir open in @p pm, with the built-in book types
void openBook(ProjectManager& pm, const QTemporaryDir& dir) {
    ProjectManager::loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(pm.bookTypes().problems().isEmpty());
    REQUIRE(pm.createProject(dir.path(), "Cards", "Anna", "pl", true, "kalahari.novel"));
}

/// A new card of kind @p kindId of the Base titled @p title, of gender @p gender (empty: not
/// said)
QString addCard(ProjectManager& pm, const char* kindId, const QString& title,
                const QString& gender = QString()) {
    const KindRef kind =
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QString::fromLatin1(kindId));
    REQUIRE(kind);
    const QString id = pm.addElement(kind, title, BookPlace::Workshop);
    REQUIRE_FALSE(id.isEmpty());
    if (!gender.isEmpty()) {
        REQUIRE(pm.setCardValue(id, QStringLiteral("gender"), gender));
    }
    return id;
}

/// The relations of card @p cardId as the card shows them in Polish, in their order:
/// "brat: Jan (bliźnięta)"
QStringList shownOf(const ProjectManager& pm, const QString& cardId) {
    const CardRelations relations = pm.cardRelations();
    const ProjectElement* card = pm.findElement(cardId);
    REQUIRE(card != nullptr);
    QStringList shown;
    for (const ShownRelation& relation : relations.of(cardId)) {
        const ProjectElement* other = pm.findElement(relation.otherId);
        REQUIRE(other != nullptr);
        QString text = relations.labelOf(relation.relation, relations.kindOf(*card), *other,
                                         QStringLiteral("pl")) +
                       QStringLiteral(": ") + other->title;
        if (!relation.note.isEmpty()) {
            text += QStringLiteral(" (") + relation.note + QLatin1Char(')');
        }
        shown << text;
    }
    return shown;
}

/// The ids of @p relations, in their order
QStringList idsOf(const QList<RelationInfo>& relations) {
    QStringList ids;
    for (const RelationInfo& relation : relations) {
        ids << relation.id;
    }
    return ids;
}

/// The book open in @p pm closed and opened again
void reopen(ProjectManager& pm) {
    const QString manifest = pm.getManifestPath();
    REQUIRE(pm.closeProject(false));
    REQUIRE(pm.openProject(manifest));
}

}  // namespace

TEST_CASE("Card relations: added on one card, shown on both, changed and taken out",
          "[core][cards][project_manager]") {
    const QTemporaryDir dir;
    auto& pm = kalahari::test::book();
    openBook(pm, dir);
    const QString anna = addCard(pm, "character", QStringLiteral("Anna"), QStringLiteral("woman"));
    const QString jan = addCard(pm, "character", QStringLiteral("Jan"), QStringLiteral("man"));
    const QString alex = addCard(pm, "character", QStringLiteral("Alex"));
    const QString krakow = addCard(pm, "location", QStringLiteral("Kraków"));

    QStringList changes;
    int linkChanges = 0;
    const QObject listener;  // the connections go with it, also when a check stops the test
    QObject::connect(&pm, &ProjectManager::cardChanged, &listener,
                     [&changes](const QString& elementId) { changes << elementId; });
    QObject::connect(&pm, &ProjectManager::linksChanged, &listener,
                     [&linkChanges]() { ++linkChanges; });

    // Jan is the brother of Anna, and she is his sister: one link, of the card it was added to
    REQUIRE(pm.addRelation(anna, jan, QStringLiteral("sibling"), QStringLiteral("bliźnięta")));
    CHECK(shownOf(pm, anna) == QStringList{QStringLiteral("brat: Jan (bliźnięta)")});
    CHECK(shownOf(pm, jan) == QStringList{QStringLiteral("siostra: Anna (bliźnięta)")});
    CHECK(pm.findElement(anna)->links.size() == 1);
    CHECK(pm.findElement(jan)->links.isEmpty());
    CHECK(changes == QStringList{anna, jan});
    CHECK(linkChanges == 1);

    // A card whose gender is not said: the name of the relation
    REQUIRE(pm.addRelation(alex, anna, QStringLiteral("friend"), QString()));
    CHECK(shownOf(pm, alex) == QStringList{QStringLiteral("przyjaciółka: Anna")});
    CHECK(shownOf(pm, anna) == QStringList{QStringLiteral("brat: Jan (bliźnięta)"),
                                           QStringLiteral("przyjaciel: Alex")});

    // The same relation added on the other card: still one, with the new note
    REQUIRE(pm.addRelation(jan, anna, QStringLiteral("sibling"), QStringLiteral("przyrodnie")));
    CHECK(shownOf(pm, anna) == QStringList{QStringLiteral("brat: Jan (przyrodnie)"),
                                           QStringLiteral("przyjaciel: Alex")});
    CHECK(pm.findElement(jan)->links.isEmpty());

    // A relation with its inverse: Anna is the mentor of Jan, and he is her student
    REQUIRE(pm.addRelation(jan, anna, QStringLiteral("mentor"), QString()));
    CHECK(shownOf(pm, jan) == QStringList{QStringLiteral("mentorka: Anna"),
                                          QStringLiteral("siostra: Anna (przyrodnie)")});
    CHECK(shownOf(pm, anna) == QStringList{QStringLiteral("brat: Jan (przyrodnie)"),
                                           QStringLiteral("uczeń: Jan"),
                                           QStringLiteral("przyjaciel: Alex")});

    // None between other kinds, with the card itself, with no card, or unknown
    CHECK_FALSE(pm.addRelation(anna, krakow, QStringLiteral("sibling"), QString()));
    CHECK_FALSE(pm.addRelation(krakow, anna, QStringLiteral("lives_in"), QString()));
    CHECK_FALSE(pm.addRelation(anna, anna, QStringLiteral("friend"), QString()));
    CHECK_FALSE(pm.addRelation(anna, QStringLiteral("no such card"), QStringLiteral("friend"),
                               QString()));
    CHECK_FALSE(pm.addRelation(anna, jan, QStringLiteral("no_such_relation"), QString()));

    // Between a character and a location
    REQUIRE(pm.addRelation(anna, krakow, QStringLiteral("lives_in"), QString()));
    CHECK(shownOf(pm, krakow) == QStringList{QStringLiteral("mieszka tu: Anna")});
    CHECK(shownOf(pm, anna) == QStringList{QStringLiteral("brat: Jan (przyrodnie)"),
                                           QStringLiteral("miejsce zamieszkania: Kraków"),
                                           QStringLiteral("uczeń: Jan"),
                                           QStringLiteral("przyjaciel: Alex")});

    // Changed on the other card, in one save: another relation with another note
    const ShownRelation sibling = pm.cardRelations().of(jan).at(1);
    CHECK(sibling.ownerId == anna);
    CHECK_FALSE(pm.replaceRelation(jan, sibling, krakow, QStringLiteral("friend"), QString()));
    CHECK(pm.cardRelations().of(jan).at(1) == sibling);
    changes.clear();
    REQUIRE(pm.replaceRelation(jan, sibling, anna, QStringLiteral("friend"),
                               QStringLiteral("dawniej")));
    CHECK(shownOf(pm, jan) == QStringList{QStringLiteral("mentorka: Anna"),
                                          QStringLiteral("przyjaciółka: Anna (dawniej)")});
    CHECK(shownOf(pm, anna) == QStringList{QStringLiteral("miejsce zamieszkania: Kraków"),
                                           QStringLiteral("uczeń: Jan"),
                                           QStringLiteral("przyjaciel: Jan (dawniej)"),
                                           QStringLiteral("przyjaciel: Alex")});
    CHECK(changes == QStringList{jan, anna});

    // Taken out on the card that does not have its link
    const ShownRelation student = pm.cardRelations().of(anna).at(1);
    CHECK(student.relation == QStringLiteral("student"));
    CHECK(student.ownerId == jan);
    REQUIRE(pm.removeRelation(student));
    CHECK(shownOf(pm, jan) == QStringList{QStringLiteral("przyjaciółka: Anna (dawniej)")});
    const QStringList annaShows{QStringLiteral("miejsce zamieszkania: Kraków"),
                                QStringLiteral("przyjaciel: Jan (dawniej)"),
                                QStringLiteral("przyjaciel: Alex")};
    CHECK(shownOf(pm, anna) == annaShows);

    // The book keeps them
    reopen(pm);
    CHECK(shownOf(pm, anna) == annaShows);

    // Saved with the other links of a card: each relation with a card once
    ElementLink rivalry;
    rivalry.to = anna;
    rivalry.role = QStringLiteral("rival");
    const ElementLink friendship = pm.findElement(alex)->links.at(0);
    REQUIRE(pm.setLinks(alex, QList<ElementLink>{friendship, rivalry, friendship}));
    CHECK(pm.findElement(alex)->links == (QList<ElementLink>{friendship, rivalry}));
    CHECK(shownOf(pm, alex) == QStringList{QStringLiteral("przyjaciółka: Anna"),
                                           QStringLiteral("rywalka: Anna")});
    REQUIRE(pm.setLinks(alex, QList<ElementLink>{friendship}));

    // A card deleted takes its relations with it
    REQUIRE(pm.removeElement(jan).has_value());
    CHECK(shownOf(pm, anna) == QStringList{QStringLiteral("miejsce zamieszkania: Kraków"),
                                           QStringLiteral("przyjaciel: Alex")});

    REQUIRE(pm.closeProject(false));
}

TEST_CASE("Card relations: a field of the type card names its card by a relation",
          "[core][cards][project_manager]") {
    const QTemporaryDir dir;
    auto& pm = kalahari::test::book();
    openBook(pm, dir);
    const QString anna = addCard(pm, "character", QStringLiteral("Anna"), QStringLiteral("woman"));
    const QString sabre = addCard(pm, "item", QStringLiteral("Szabla"));
    const QString krakow = addCard(pm, "location", QStringLiteral("Kraków"));
    const QString poland = addCard(pm, "location", QStringLiteral("Polska"));
    const QString lesser = addCard(pm, "location", QStringLiteral("Małopolska"));
    const QString owner = QStringLiteral("owner");
    const QString parent = QStringLiteral("parent");

    // The title of a card of its kind: the relation with that card, and no value
    REQUIRE(pm.setCardFieldText(sabre, owner, QStringLiteral("  anna ")));
    CHECK(pm.cardFieldText(sabre, owner) == QStringLiteral("Anna"));
    CHECK_FALSE(pm.findElement(sabre)->card.values.contains(owner));
    CHECK(shownOf(pm, sabre) == QStringList{QStringLiteral("właścicielka: Anna")});
    CHECK(shownOf(pm, anna) == QStringList{QStringLiteral("własność: Szabla")});

    // Another text: its value, and the card it named loses the relation
    REQUIRE(pm.setCardFieldText(sabre, owner, QStringLiteral("Nieznajomy")));
    CHECK(pm.cardFieldText(sabre, owner) == QStringLiteral("Nieznajomy"));
    CHECK(pm.findElement(sabre)->card.value(owner) == QStringLiteral("Nieznajomy"));
    CHECK(shownOf(pm, anna).isEmpty());

    // A card again: the value goes
    REQUIRE(pm.setCardFieldText(sabre, owner, QStringLiteral("Anna")));
    CHECK_FALSE(pm.findElement(sabre)->card.values.contains(owner));
    CHECK(shownOf(pm, anna) == QStringList{QStringLiteral("własność: Szabla")});

    // The title of a card of another kind is a text
    REQUIRE(pm.setCardFieldText(sabre, QStringLiteral("place"), QStringLiteral("Anna")));
    CHECK(pm.findElement(sabre)->card.value(QStringLiteral("place")) == QStringLiteral("Anna"));
    CHECK(shownOf(pm, sabre) == QStringList{QStringLiteral("właścicielka: Anna")});

    // No text: neither a card nor a value
    REQUIRE(pm.setCardFieldText(sabre, owner, QString()));
    CHECK(pm.cardFieldText(sabre, owner).isEmpty());
    CHECK_FALSE(pm.findElement(sabre)->card.values.contains(owner));
    CHECK(shownOf(pm, anna).isEmpty());

    // The inverse relation of the other card names it too: Polska takes in Kraków, so Kraków
    // lies in Polska
    REQUIRE(pm.addRelation(poland, krakow, QStringLiteral("contains"), QString()));
    CHECK(pm.cardFieldText(krakow, parent) == QStringLiteral("Polska"));
    CHECK(shownOf(pm, krakow) == QStringList{QStringLiteral("miejsce nadrzędne: Polska")});

    // Another place in its stead: the relation of the other card goes, and all three tell
    QStringList changes;
    const QObject listener;
    QObject::connect(&pm, &ProjectManager::cardChanged, &listener,
                     [&changes](const QString& elementId) { changes << elementId; });
    REQUIRE(pm.setCardFieldText(krakow, parent, QStringLiteral("Małopolska")));
    CHECK(pm.cardFieldText(krakow, parent) == QStringLiteral("Małopolska"));
    CHECK(shownOf(pm, krakow) == QStringList{QStringLiteral("miejsce nadrzędne: Małopolska")});
    CHECK(shownOf(pm, lesser) == QStringList{QStringLiteral("miejsce podrzędne: Kraków")});
    CHECK(shownOf(pm, poland).isEmpty());
    CHECK(changes == QStringList{krakow, poland, lesser});

    // A field without a relation, or one the kind does not have, keeps the text as its value
    REQUIRE(pm.setCardFieldText(anna, QStringLiteral("occupation"),
                                QStringLiteral("kartografka")));
    CHECK(pm.findElement(anna)->card.value(QStringLiteral("occupation")) ==
          QStringLiteral("kartografka"));
    CHECK(pm.cardFieldText(anna, QStringLiteral("occupation")) == QStringLiteral("kartografka"));
    REQUIRE(pm.setCardFieldText(anna, QStringLiteral("nickname"), QStringLiteral("Ania")));
    CHECK(pm.cardFieldText(anna, QStringLiteral("nickname")) == QStringLiteral("Ania"));
    CHECK(pm.cardFieldText(QStringLiteral("no such card"), owner).isEmpty());

    // The book keeps them
    reopen(pm);
    CHECK(pm.cardFieldText(krakow, parent) == QStringLiteral("Małopolska"));

    REQUIRE(pm.closeProject(false));
}

TEST_CASE("Card relations: the relations the writer names", "[core][cards][project_manager]") {
    const QTemporaryDir dir;
    auto& pm = kalahari::test::book();
    openBook(pm, dir);
    const QString anna = addCard(pm, "character", QStringLiteral("Anna"), QStringLiteral("woman"));
    const QString jan = addCard(pm, "character", QStringLiteral("Jan"), QStringLiteral("man"));
    const QString krakow = addCard(pm, "location", QStringLiteral("Kraków"));
    const auto ownRelations = [&pm]() { return pm.project()->workshop.relations; };
    const QString character = QStringLiteral("kalahari.base:character");
    const QString location = QStringLiteral("kalahari.base:location");

    // A relation with its inverse, saved in the Workshop
    const QString creditor = pm.ownRelation(character, character, QStringLiteral("wierzyciel"),
                                            QStringLiteral("dłużnik"));
    CHECK(creditor == QStringLiteral("own-1"));
    REQUIRE(ownRelations().size() == 2);
    CHECK(ownRelations().at(0) == OwnRelation{QStringLiteral("own-1"), character, character,
                                              QStringLiteral("wierzyciel"),
                                              QStringLiteral("own-2"), {}});
    CHECK(ownRelations().at(1) == OwnRelation{QStringLiteral("own-2"), character, character,
                                              QStringLiteral("dłużnik"), QStringLiteral("own-1"),
                                              {}});

    // The same names: the same relation, either way round
    CHECK(pm.ownRelation(character, character, QStringLiteral(" wierzyciel "),
                         QStringLiteral("dłużnik")) == creditor);
    CHECK(pm.ownRelation(character, character, QStringLiteral("dłużnik"),
                         QStringLiteral("wierzyciel")) == QStringLiteral("own-2"));
    CHECK(ownRelations().size() == 2);

    // One name both ways between cards of one kind: one relation, its own inverse
    const QString neighbor =
        pm.ownRelation(character, character, QStringLiteral("sąsiad"), QStringLiteral("sąsiad"));
    CHECK(neighbor == QStringLiteral("own-3"));
    REQUIRE(ownRelations().size() == 3);
    CHECK(ownRelations().at(2).inverse == neighbor);

    // Between two kinds: one for each of them, also of one name
    const QString hideout = pm.ownRelation(character, location, QStringLiteral("kryjówka"),
                                           QStringLiteral("ukrywa się tu"));
    CHECK(hideout == QStringLiteral("own-4"));
    CHECK(pm.ownRelation(character, location, QStringLiteral("bywa"), QStringLiteral("bywa")) ==
          QStringLiteral("own-6"));
    REQUIRE(ownRelations().size() == 7);
    CHECK(ownRelations().at(4) == OwnRelation{QStringLiteral("own-5"), location, character,
                                              QStringLiteral("ukrywa się tu"), hideout, {}});
    CHECK(ownRelations().at(6).inverse == QStringLiteral("own-6"));

    // Without a name: none
    CHECK(pm.ownRelation(character, character, QStringLiteral(" "), QStringLiteral("dłużnik"))
              .isEmpty());
    CHECK(ownRelations().size() == 7);

    // Offered between their kinds, after those of the packages
    const CardRelations relations = pm.cardRelations();
    const QStringList characters = idsOf(relations.between(*pm.findElement(anna),
                                                           *pm.findElement(jan)));
    REQUIRE(characters.size() > 3);
    CHECK(characters.first() == QStringLiteral("parent"));
    CHECK(characters.mid(characters.size() - 3) ==
          QStringList{creditor, QStringLiteral("own-2"), neighbor});
    CHECK(idsOf(relations.between(*pm.findElement(anna), *pm.findElement(krakow))) ==
          QStringList{QStringLiteral("lives_in"), QStringLiteral("origin"),
                      QStringLiteral("works_in"), QStringLiteral("visits"),
                      QStringLiteral("realm"), hideout, QStringLiteral("own-6")});
    CHECK(idsOf(relations.between(*pm.findElement(krakow), *pm.findElement(anna))) ==
          QStringList{QStringLiteral("resident"), QStringLiteral("native"),
                      QStringLiteral("worker"), QStringLiteral("visitor"),
                      QStringLiteral("ruler"), QStringLiteral("own-5"),
                      QStringLiteral("own-7")});

    // Added like the others, and shown by their names
    REQUIRE(pm.addRelation(anna, jan, creditor, QString()));
    CHECK(shownOf(pm, anna) == QStringList{QStringLiteral("wierzyciel: Jan")});
    CHECK(shownOf(pm, jan) == QStringList{QStringLiteral("dłużnik: Anna")});
    REQUIRE(pm.addRelation(anna, krakow, hideout, QString()));
    CHECK(shownOf(pm, krakow) == QStringList{QStringLiteral("ukrywa się tu: Anna")});
    CHECK_FALSE(pm.addRelation(krakow, anna, creditor, QString()));

    // The book keeps them; a relation that is its own inverse is saved without it
    const QList<OwnRelation> named = ownRelations();
    reopen(pm);
    CHECK(ownRelations() == named);
    CHECK(shownOf(pm, jan) == QStringList{QStringLiteral("dłużnik: Anna")});
    QFile manifest(pm.getManifestPath());
    REQUIRE(manifest.open(QIODevice::ReadOnly));
    const QJsonArray saved = QJsonDocument::fromJson(manifest.readAll())
                                 .object()[QStringLiteral("workshop")][QStringLiteral("relations")]
                                 .toArray();
    REQUIRE(saved.size() == 7);
    CHECK(saved.at(0)[QStringLiteral("inverse")].toString() == QStringLiteral("own-2"));
    CHECK_FALSE(saved.at(2).toObject().contains(QStringLiteral("inverse")));

    REQUIRE(pm.closeProject(false));
}

TEST_CASE("Card relations: a card in the chapters, beside its relations",
          "[core][cards][project_manager]") {
    const QTemporaryDir dir;
    auto& pm = kalahari::test::book();
    openBook(pm, dir);
    const QString anna = addCard(pm, "character", QStringLiteral("Anna"), QStringLiteral("woman"));
    const QString jan = addCard(pm, "character", QStringLiteral("Jan"), QStringLiteral("man"));
    const QString krakow = addCard(pm, "location", QStringLiteral("Kraków"));
    const QString storm = pm.addElement(pm.chapterKindFor(BookPlace::Main),
                                        QStringLiteral("Burza"), BookPlace::Main);
    REQUIRE_FALSE(storm.isEmpty());

    // A new link of a card with a chapter has the role its kind suggests; with a card, none
    CHECK(pm.newLink(anna, storm).role == QStringLiteral("appears"));
    CHECK(pm.newLink(krakow, storm).role == QStringLiteral("setting"));
    CHECK(pm.newLink(anna, jan).role.isEmpty());

    // The chapters and the relations of a card are its links; each is shown where it belongs
    REQUIRE(pm.addRelation(anna, jan, QStringLiteral("sibling"), QString()));
    QList<ElementLink> links = pm.findElement(anna)->links;
    ElementLink chapter = pm.newLink(anna, storm);
    chapter.role = QStringLiteral("pov");
    chapter.note = QStringLiteral("cała");
    links << chapter;
    REQUIRE(pm.setLinks(anna, links));
    CHECK(shownOf(pm, anna) == QStringList{QStringLiteral("brat: Jan")});
    const ElementLink* kept = pm.findElement(anna)->linkTo(storm);
    REQUIRE(kept != nullptr);
    CHECK(kept->role == QStringLiteral("pov"));
    CHECK(kept->note == QStringLiteral("cała"));
    CHECK(pm.materialsLinkedTo(storm).isEmpty());

    // The book keeps both
    reopen(pm);
    CHECK(shownOf(pm, anna) == QStringList{QStringLiteral("brat: Jan")});
    REQUIRE(pm.findElement(anna)->linkTo(storm) != nullptr);
    CHECK(pm.findElement(anna)->linkTo(storm)->role == QStringLiteral("pov"));

    REQUIRE(pm.closeProject(false));
}
