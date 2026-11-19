// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_volume_materials.cpp
/// @brief The materials of the cover and the texts for publishers of the volumes of a series:
/// each for a volume or for the whole series, chosen and in the set of each volume apart

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_registry.h>
#include <kalahari/core/cover_material.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/publishing_texts.h>
#include <kalahari/core/volume_materials.h>

#include "kalahari_tests/test_support/window_parts.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include <algorithm>
#include <optional>

using kalahari::core::BookProject;
using kalahari::core::CoverMaterial;
using kalahari::core::KindRef;
using kalahari::core::NewBook;
using kalahari::core::NewMaterial;
using kalahari::core::ProjectBook;
using kalahari::core::ProjectElement;
using kalahari::core::ProjectManager;
using kalahari::core::PublishingSet;
using kalahari::core::PublishingSetPart;
using kalahari::core::PublishingTexts;
using kalahari::core::VolumeMaterials;

namespace {

/// A series of two volumes, "b1" and "b2", with the Workshop elements @p elements (JSON)
BookProject seriesWith(const QByteArray& elements) {
    const QByteArray text = R"({ "format": 2, "id": "p",
        "books": [ { "id": "b1", "title": "Piasek", "folder": "book" },
                   { "id": "b2", "title": "Wydmy", "folder": "book2" } ],
        "workshop": { "grouped": false, "elements": [ )" +
                            elements + "] } }";
    QStringList problems;
    std::optional<BookProject> project =
        BookProject::fromJson(QJsonDocument::fromJson(text).object(), problems);
    INFO(problems.join(QStringLiteral("; ")).toStdString());
    REQUIRE(project.has_value());
    REQUIRE(problems.isEmpty());
    return *project;
}

/// The ids of @p elements, in order
QStringList idsOf(const QList<const ProjectElement*>& elements) {
    QStringList ids;
    for (const ProjectElement* element : elements) {
        ids << element->id;
    }
    return ids;
}

/// The element of the set of @p parts for @p part; empty when it has none
QString elementOf(const QList<PublishingSetPart>& parts, const char* part) {
    for (const PublishingSetPart& each : parts) {
        if (each.part == QLatin1String(part)) {
            return each.elementId;
        }
    }
    return {};
}

/// The element @p id of the Workshop of @p project
const ProjectElement& elementIn(const BookProject& project, const char* id) {
    const QList<ProjectElement>& elements = project.workshop.elements;
    const auto found = std::find_if(elements.cbegin(), elements.cend(), [id](const auto& each) {
        return each.id == QLatin1String(id);
    });
    INFO("The element " << id);
    REQUIRE(found != elements.cend());
    return *found;
}

ProjectManager& projects() {
    auto& pm = kalahari::test::book();
    ProjectManager::loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(pm.bookTypes().problems().isEmpty());
    return pm;
}

/// A description of the cover for the back cover, in the current volume
QString addBlurb(ProjectManager& pm, const char* title) {
    const KindRef cover =
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QStringLiteral("cover"));
    REQUIRE(cover);
    NewMaterial blurb;
    blurb.title = QString::fromUtf8(title);
    blurb.description = QString::fromUtf8(title);
    blurb.cover.part = QString::fromLatin1(CoverMaterial::BLURB);
    blurb.cover.place = QString::fromLatin1(CoverMaterial::BACK);
    const QString id = pm.addMaterial(blurb, cover);
    REQUIRE_FALSE(id.isEmpty());
    return id;
}

/// A sentence about the book, in the current volume
QString addPitch(ProjectManager& pm, const char* title) {
    const QString id =
        pm.addPublishingText(QString::fromLatin1(PublishingTexts::PITCH), QString(),
                             QString::fromUtf8(title));
    REQUIRE_FALSE(id.isEmpty());
    return id;
}

}  // namespace

TEST_CASE("Volume materials: the volume of a material in the book file, else the whole series",
          "[core][series][publishing]") {
    const BookProject project = seriesWith(R"(
        { "id": "c0", "kind": "kalahari.base:cover", "title": "Opis serii",
          "cover": { "part": "blurb", "place": "back", "state": "chosen" } },
        { "id": "c1", "kind": "kalahari.base:cover", "title": "Opis 1", "book": "b1",
          "cover": { "part": "blurb", "place": "back", "state": "chosen" } },
        { "id": "c2", "kind": "kalahari.base:cover", "title": "Opis 2", "book": "b2",
          "cover": { "part": "blurb", "place": "back" } },
        { "id": "c3", "kind": "kalahari.base:cover", "title": "Hasło 2", "book": "b2",
          "cover": { "part": "tagline", "place": "front", "state": "chosen" } },
        { "id": "x", "kind": "kalahari.base:cover", "title": "Opis tomu usuniętego",
          "book": "b9", "cover": { "part": "blurb", "place": "back" } },
        { "id": "p0", "kind": "kalahari.base:publishing", "title": "Zdanie serii",
          "publishing": { "part": "pitch", "state": "chosen" } },
        { "id": "p1", "kind": "kalahari.base:publishing", "title": "Zdanie 1", "book": "b1",
          "publishing": { "part": "pitch", "state": "chosen" } },
        { "id": "p2", "kind": "kalahari.base:publishing", "title": "Zdanie 2", "book": "b2",
          "publishing": { "part": "pitch" } },
        { "id": "q", "kind": "kalahari.base:quote", "title": "Cytat" })");
    const ProjectBook& first = project.books.at(0);
    const ProjectBook& second = project.books.at(1);

    // In the book file: the volume as it is written, none without it
    CHECK(elementIn(project, "c1").bookId == QStringLiteral("b1"));
    CHECK(elementIn(project, "c0").bookId.isEmpty());
    const QJsonObject json = project.toJson();
    const QJsonObject saved =
        json.value(QStringLiteral("workshop")).toObject().value(QStringLiteral("elements"))
            .toArray().at(1).toObject();
    CHECK(saved.value(QStringLiteral("book")).toString() == QStringLiteral("b1"));
    CHECK_FALSE(json.value(QStringLiteral("workshop"))
                    .toObject()
                    .value(QStringLiteral("elements"))
                    .toArray()
                    .at(0)
                    .toObject()
                    .contains(QStringLiteral("book")));

    // What is for a volume: a material of the cover, a text for publishers
    CHECK(VolumeMaterials::isVolumeMaterial(elementIn(project, "c0")));
    CHECK(VolumeMaterials::isVolumeMaterial(elementIn(project, "p0")));
    CHECK_FALSE(VolumeMaterials::isVolumeMaterial(elementIn(project, "q")));

    // The volume: as written, the whole series without one or for a book the project lacks
    CHECK(VolumeMaterials::volumeOf(project, elementIn(project, "c1")) == QStringLiteral("b1"));
    CHECK(VolumeMaterials::volumeOf(project, elementIn(project, "c0")).isEmpty());
    CHECK(VolumeMaterials::volumeOf(project, elementIn(project, "x")).isEmpty());
    CHECK(VolumeMaterials::isFor(project, elementIn(project, "c1"), first));
    CHECK_FALSE(VolumeMaterials::isFor(project, elementIn(project, "c1"), second));
    CHECK(VolumeMaterials::isFor(project, elementIn(project, "c0"), second));
    CHECK(VolumeMaterials::sameVolume(project, elementIn(project, "c0"), elementIn(project, "x")));
    CHECK_FALSE(
        VolumeMaterials::sameVolume(project, elementIn(project, "c1"), elementIn(project, "c2")));

    // The versions of a volume: its own and those of the whole series
    const QString blurb = QString::fromLatin1(PublishingTexts::BLURB);
    CHECK(idsOf(PublishingTexts::versionsOf(project, blurb, &first)) ==
          QStringList{QStringLiteral("c0"), QStringLiteral("c1"), QStringLiteral("x")});
    CHECK(idsOf(PublishingTexts::versionsOf(project, blurb, &second)) ==
          QStringList{QStringLiteral("c0"), QStringLiteral("c2"), QStringLiteral("x")});
    CHECK(PublishingTexts::versionsOf(project, blurb).size() == 4);

    // The set of each volume: its own versions chosen first, else those of the whole series
    const PublishingSet set = PublishingSet::defaults();
    const QList<PublishingSetPart> ofFirst = PublishingTexts::resolveSet(project, first, set);
    CHECK(elementOf(ofFirst, PublishingTexts::PITCH) == QStringLiteral("p1"));
    CHECK(elementOf(ofFirst, PublishingTexts::BLURB) == QStringLiteral("c1"));
    CHECK(elementOf(ofFirst, PublishingTexts::TAGLINE).isEmpty());
    const QList<PublishingSetPart> ofSecond = PublishingTexts::resolveSet(project, second, set);
    CHECK(elementOf(ofSecond, PublishingTexts::PITCH) == QStringLiteral("p0"));
    CHECK(elementOf(ofSecond, PublishingTexts::BLURB) == QStringLiteral("c0"));
    CHECK(elementOf(ofSecond, PublishingTexts::TAGLINE) == QStringLiteral("c3"));
    // A pick of a version of another volume is not the one of the set
    PublishingSet picked = set;
    picked.picks.insert(blurb, QStringLiteral("c1"));
    CHECK(elementOf(PublishingTexts::resolveSet(project, second, picked),
                    PublishingTexts::BLURB) == QStringLiteral("c0"));
}

TEST_CASE("Volume materials: one chosen takes the place of those of its volume only",
          "[core][series][publishing]") {
    BookProject project = seriesWith(R"(
        { "id": "p0", "kind": "kalahari.base:publishing",
          "publishing": { "part": "pitch", "state": "chosen" } },
        { "id": "p1", "kind": "kalahari.base:publishing", "book": "b1",
          "publishing": { "part": "pitch", "state": "chosen" } },
        { "id": "p2", "kind": "kalahari.base:publishing", "book": "b2",
          "publishing": { "part": "pitch" } },
        { "id": "p3", "kind": "kalahari.base:publishing", "book": "b2",
          "publishing": { "part": "pitch", "state": "chosen" } },
        { "id": "c1", "kind": "kalahari.base:cover", "book": "b1",
          "cover": { "part": "blurb", "place": "back", "state": "chosen" } },
        { "id": "c2", "kind": "kalahari.base:cover", "book": "b2",
          "cover": { "part": "blurb", "place": "back", "state": "chosen" } },
        { "id": "c3", "kind": "kalahari.base:cover", "book": "b2",
          "cover": { "part": "blurb", "place": "back", "state": "chosen" } })");
    const auto stateOf = [&project](const char* id) {
        const ProjectElement& element = elementIn(project, id);
        return element.publishing.part.isEmpty() ? element.cover.state
                                                 : element.publishing.state;
    };

    // A sentence of the second volume: the one chosen there becomes a draft, the first
    // volume's and the series' stay
    CHECK(PublishingTexts::setState(project, QStringLiteral("p2"), PublishingTexts::CHOSEN) ==
          QStringList{QStringLiteral("p3")});
    CHECK(stateOf("p0") == PublishingTexts::CHOSEN);
    CHECK(stateOf("p1") == PublishingTexts::CHOSEN);
    CHECK(stateOf("p2") == PublishingTexts::CHOSEN);
    CHECK(stateOf("p3").isEmpty());

    // A description of the cover, the same
    CHECK(VolumeMaterials::unchooseOthers(project, elementIn(project, "c3")) ==
          QStringList{QStringLiteral("c2")});
    CHECK(stateOf("c1") == CoverMaterial::CHOSEN);
    CHECK(stateOf("c2").isEmpty());
    // One that is not chosen takes no place
    CHECK(VolumeMaterials::unchooseOthers(project, elementIn(project, "c2")).isEmpty());
    CHECK(stateOf("c3") == CoverMaterial::CHOSEN);

    // The second volume leaves the series: its materials are of the whole series, as drafts
    VolumeMaterials::release(project, QStringLiteral("b2"));
    CHECK(elementIn(project, "p2").bookId.isEmpty());
    CHECK(stateOf("p2").isEmpty());
    CHECK(stateOf("c3").isEmpty());
    CHECK(elementIn(project, "p1").bookId == QStringLiteral("b1"));
    CHECK(stateOf("p1") == PublishingTexts::CHOSEN);

    // A book that is no series: all are its own, one chosen takes the place of all others
    project.books.removeLast();
    CHECK(VolumeMaterials::volumeOf(project, elementIn(project, "p1")).isEmpty());
    CHECK(PublishingTexts::setState(project, QStringLiteral("p2"), PublishingTexts::CHOSEN) ==
          (QStringList{QStringLiteral("p0"), QStringLiteral("p1")}));

    // ...and when it becomes a series, they are all of its first volume
    VolumeMaterials::toFirstVolume(project);
    for (const ProjectElement& element : project.workshop.elements) {
        CHECK(element.bookId == QStringLiteral("b1"));
    }
}

TEST_CASE("ProjectManager: the cover and the texts for publishers of each volume of a series",
          "[core][series][publishing]") {
    const QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Piasek", "Ewa Lis", "pl", true, "kalahari.novel"));
    const QString first = pm.book()->id;
    const QString blurb1 = addBlurb(pm, "Trzy dni drogi.");
    const QString pitch1 = addPitch(pm, "Gdy studnia wysycha.");
    const KindRef quoteKind =
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QStringLiteral("quote"));
    const QString quote = pm.addMaterial(QString(), quoteKind, QStringLiteral("Cytat"));
    REQUIRE_FALSE(quote.isEmpty());
    // A book that is no series: no volume
    CHECK(pm.findElement(blurb1)->bookId.isEmpty());
    REQUIRE(pm.setCoverState(blurb1, CoverMaterial::CHOSEN));
    REQUIRE(pm.setPublishingState(pitch1, PublishingTexts::CHOSEN));

    // The book becomes a series: its materials are for its first volume
    const QString second =
        pm.addBook(NewBook{QStringLiteral("Wydmy"), QStringLiteral("Ewa Lis"), QString()});
    REQUIRE_FALSE(second.isEmpty());
    CHECK(pm.findElement(blurb1)->bookId == first);
    CHECK(pm.findElement(pitch1)->bookId == first);
    CHECK(pm.findElement(quote)->bookId.isEmpty());

    // New ones are for the current volume, chosen there apart from the first volume's
    REQUIRE(pm.setCurrentBook(second));
    const QString blurb2 = addBlurb(pm, "Wydmy idą.");
    const QString pitch2 = addPitch(pm, "Gdy wydma zasypuje oazę.");
    CHECK(pm.findElement(blurb2)->bookId == second);
    CHECK(pm.findElement(pitch2)->bookId == second);
    QStringList drafts;
    REQUIRE(pm.setCoverState(blurb2, CoverMaterial::CHOSEN, &drafts));
    CHECK(drafts.isEmpty());
    REQUIRE(pm.setPublishingState(pitch2, PublishingTexts::CHOSEN, &drafts));
    CHECK(drafts.isEmpty());
    CHECK(pm.findElement(blurb1)->cover.state == CoverMaterial::CHOSEN);
    CHECK(elementOf(pm.publishingSetParts(), PublishingTexts::PITCH) == pitch2);
    CHECK(elementOf(pm.publishingSetParts(), PublishingTexts::BLURB) == blurb2);
    CHECK(pm.setBookOf(pitch1)->id == first);
    CHECK(pm.setBookOf(quote)->id == second);
    CHECK(elementOf(pm.publishingSetPartsOf(pitch1), PublishingTexts::PITCH) == pitch1);
    CHECK(elementOf(pm.publishingSetPartsOf(pitch1), PublishingTexts::BLURB) == blurb1);

    // A sentence of the whole series is in the set of a volume without its own chosen
    REQUIRE(pm.setMaterialBook(pitch1, QString(), &drafts));
    CHECK(drafts.isEmpty());
    CHECK(pm.findElement(pitch1)->bookId.isEmpty());
    CHECK(pm.setBookOf(pitch1)->id == second);
    CHECK(elementOf(pm.publishingSetParts(), PublishingTexts::PITCH) == pitch2);
    REQUIRE(pm.setCurrentBook(first));
    CHECK(elementOf(pm.publishingSetParts(), PublishingTexts::PITCH) == pitch1);

    // Given to the second volume, chosen: the one chosen there becomes a draft
    REQUIRE(pm.setMaterialBook(pitch1, second, &drafts));
    CHECK(drafts == QStringList{pitch2});
    CHECK(pm.findElement(pitch2)->publishing.state.isEmpty());
    CHECK(pm.findElement(pitch1)->publishing.state == PublishingTexts::CHOSEN);
    CHECK(elementOf(pm.publishingSetParts(), PublishingTexts::PITCH).isEmpty());
    CHECK_FALSE(pm.setMaterialBook(pitch1, QStringLiteral("none")));
    CHECK_FALSE(pm.setMaterialBook(quote, second));
    CHECK_FALSE(pm.setMaterialBook(QStringLiteral("none"), second));

    // Picked for the set of its own volume, not of the current one
    REQUIRE(pm.pickForPublishingSet(pitch2));
    REQUIRE(pm.project()->books.at(1).publishingSet.has_value());
    CHECK(pm.project()->books.at(1).publishingSet->picks.value(
              QString::fromLatin1(PublishingTexts::PITCH)) == pitch2);
    CHECK_FALSE(pm.project()->books.at(0).publishingSet.has_value());
    // Chosen again: the pick of its volume goes
    REQUIRE(pm.setPublishingState(pitch2, PublishingTexts::CHOSEN, &drafts));
    CHECK(drafts == QStringList{pitch1});
    CHECK(pm.project()->books.at(1).publishingSet->picks.isEmpty());

    // All of it in the book file
    const QString manifest = pm.getManifestPath();
    REQUIRE(pm.closeProject(false));
    REQUIRE(pm.openProject(manifest));
    CHECK(pm.findElement(blurb1)->bookId == first);
    CHECK(pm.findElement(blurb2)->bookId == second);
    CHECK(pm.findElement(pitch1)->bookId == second);

    // The second volume removed: its materials are of the whole series, as drafts
    REQUIRE(pm.removeBook(second).has_value());
    CHECK(pm.findElement(blurb2)->bookId.isEmpty());
    CHECK(pm.findElement(blurb2)->cover.state.isEmpty());
    CHECK(pm.findElement(pitch2)->publishing.state.isEmpty());
    CHECK(pm.findElement(blurb1)->cover.state == CoverMaterial::CHOSEN);
    CHECK(elementOf(pm.publishingSetParts(), PublishingTexts::BLURB) == blurb1);
    REQUIRE(pm.closeProject(false));
}
