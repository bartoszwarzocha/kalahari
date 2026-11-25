// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_cover_material.cpp
/// @brief The materials of the cover: their data in the book file, their parts and places,
/// and the lengths of their texts

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/cover_material.h>
#include <kalahari/core/settings_manager.h>

#include <QJsonArray>
#include <QJsonDocument>

#include <cstdio>
#include <string>
#include <utility>

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

/// A setting changed for a test, and set back afterwards
template <typename T>
class ChangedSetting {
public:
    ChangedSetting(std::string key, const T& value)
        : m_key(std::move(key)), m_before(SettingsManager::getInstance().get<T>(m_key)) {
        SettingsManager::getInstance().set<T>(m_key, value);
    }
    ~ChangedSetting() {
        try {
            SettingsManager::getInstance().set<T>(m_key, m_before);
        } catch (...) {
            // A destructor may not throw: the later tests then find the setting of this one
            std::fputs("ChangedSetting: the setting of the test stays\n", stderr);
        }
    }

    ChangedSetting(const ChangedSetting&) = delete;
    ChangedSetting& operator=(const ChangedSetting&) = delete;

private:
    std::string m_key;
    T m_before;
};

} // namespace

TEST_CASE("Cover materials: their data in the book file", "[core][cover]") {
    const QByteArray elements = R"(
        { "id": "e1", "kind": "kalahari.base:cover", "title": "Opinia – Anna Wiśniewska",
          "description": "Proza gęsta jak pustynny upał.",
          "cover": { "part": "opinion", "place": "back", "state": "chosen",
                     "author": "Anna Wiśniewska", "role": "krytyczka literacka",
                     "source": "kwartalnik „Wydmy”, nr 3/2026", "date": "2026-10-02",
                     "consent": "granted", "targetCharacters": 180, "quoteMarks": "„”" } },
        { "id": "e2", "kind": "kalahari.base:cover", "title": "Mood",
          "file": "workshop/mood.png",
          "cover": { "part": "reference", "author": "J. Doe", "license": "CC BY 4.0",
                     "about": "colors", "targetWords": 12 } },
        { "id": "e3", "kind": "kalahari.base:quote", "title": "Quote" })";
    QStringList problems;
    const std::optional<BookProject> project =
        BookProject::fromJson(jsonOf(projectWith(elements)), problems);
    INFO(problems.join(QStringLiteral("; ")).toStdString());
    REQUIRE(project.has_value());
    REQUIRE(problems.isEmpty());

    const CoverData& opinion = project->workshop.elements.at(0).cover;
    CHECK(opinion.part == QStringLiteral("opinion"));
    CHECK(opinion.place == QStringLiteral("back"));
    CHECK(opinion.state == QStringLiteral("chosen"));
    CHECK(opinion.author == QStringLiteral("Anna Wiśniewska"));
    CHECK(opinion.role == QStringLiteral("krytyczka literacka"));
    CHECK(opinion.source == QStringLiteral("kwartalnik „Wydmy”, nr 3/2026"));
    CHECK(opinion.date == QStringLiteral("2026-10-02"));
    CHECK(opinion.consent == QStringLiteral("granted"));
    CHECK(opinion.targetCharacters == 180);
    CHECK(opinion.targetWords == 0);
    // A field this version does not know is kept, and saved back
    CHECK(opinion.extra.keys() == QStringList{QStringLiteral("quoteMarks")});
    CHECK(project->workshop.elements.at(0).fields.isEmpty());
    CHECK(project->workshop.elements.at(0).extra.isEmpty());

    const CoverData& reference = project->workshop.elements.at(1).cover;
    CHECK(reference.part == QStringLiteral("reference"));
    CHECK(reference.place.isEmpty());
    CHECK(reference.license == QStringLiteral("CC BY 4.0"));
    CHECK(reference.about == QStringLiteral("colors"));
    CHECK(reference.targetWords == 12);

    // A material of another kind has none
    CHECK(project->workshop.elements.at(2).cover.isEmpty());

    // The book is saved as it was read
    CHECK(project->toJson()[QStringLiteral("workshop")] ==
          jsonOf(projectWith(elements))[QStringLiteral("workshop")]);

    // A change of the data changes the project; data emptied leaves no "cover"
    BookProject changed = *project;
    changed.workshop.elements[0].cover.state = QStringLiteral("rejected");
    CHECK_FALSE(changed == *project);
    changed.workshop.elements[1].cover = CoverData{};
    const QJsonObject written = changed.toJson()[QStringLiteral("workshop")]
                                    [QStringLiteral("elements")][1].toObject();
    CHECK_FALSE(written.contains(QStringLiteral("cover")));
    const QJsonObject state = changed.toJson()[QStringLiteral("workshop")]
                                  [QStringLiteral("elements")][0][QStringLiteral("cover")]
                                      .toObject();
    CHECK(state.value(QStringLiteral("state")).toString() == QStringLiteral("rejected"));
}

TEST_CASE("Cover materials: problems of their data", "[core][cover]") {
    const auto problemOf = [](const QByteArray& element) {
        QStringList problems;
        BookProject::fromJson(jsonOf(projectWith(element)), problems);
        return problems.join(QStringLiteral("; "));
    };
    CHECK(problemOf(R"({ "id": "e1", "kind": "kalahari.base:cover", "cover": "opinion" })") ==
          QStringLiteral("workshop.elements[0].cover: must be an object"));
    CHECK(problemOf(R"({ "id": "e1", "kind": "kalahari.base:cover",
                         "cover": { "part": 4 } })") ==
          QStringLiteral("workshop.elements[0].cover.part: must be a text"));
    CHECK(problemOf(R"({ "id": "e1", "kind": "kalahari.base:cover",
                         "cover": { "targetWords": -3 } })") ==
          QStringLiteral("workshop.elements[0].cover.targetWords: must be a whole number, 0 or "
                         "more"));
}

TEST_CASE("Cover materials: parts and places", "[core][cover]") {
    CHECK(CoverMaterial::parts() == QStringList{"blurb", "tagline", "opinion", "bio", "design",
                                                "authorPhoto", "reference", "note"});
    CHECK(CoverMaterial::coverPlaces() ==
          QStringList{"front", "spine", "back", "frontFlap", "backFlap", "whole"});
    CHECK(CoverMaterial::jacketPlaces() ==
          QStringList{"jacketFront", "jacketSpine", "jacketBack", "jacketFrontFlap",
                      "jacketBackFlap", "jacketWhole"});
    CHECK(CoverMaterial::places() == CoverMaterial::coverPlaces() + CoverMaterial::jacketPlaces());

    // Where a new material goes
    CHECK(CoverMaterial::defaultPlaceOf(QStringLiteral("blurb")) == QStringLiteral("back"));
    CHECK(CoverMaterial::defaultPlaceOf(QStringLiteral("tagline")) == QStringLiteral("front"));
    CHECK(CoverMaterial::defaultPlaceOf(QStringLiteral("opinion")) == QStringLiteral("back"));
    CHECK(CoverMaterial::defaultPlaceOf(QStringLiteral("design")) == QStringLiteral("whole"));
    CHECK(CoverMaterial::defaultPlaceOf(QStringLiteral("reference")).isEmpty());
    CHECK(CoverMaterial::defaultPlaceOf(QStringLiteral("note")).isEmpty());

    // A note about the author and a photo of the author go on the back flap of the jacket,
    // else on the back flap of the cover, else on the back
    CoverParts parts;
    for (const char* part : {"bio", "authorPhoto"}) {
        const QString each = QString::fromLatin1(part);
        CHECK(CoverMaterial::defaultPlaceOf(each) == QStringLiteral("backFlap"));
        parts.jacket = true;
        CHECK(CoverMaterial::defaultPlaceOf(each, parts) == QStringLiteral("jacketBackFlap"));
        parts.jacket = false;
        parts.flaps = false;
        CHECK(CoverMaterial::defaultPlaceOf(each, parts) == QStringLiteral("back"));
        parts.flaps = true;
    }
    // The places of a part turned off are not on the cover; the others always are
    parts.flaps = false;
    CHECK_FALSE(CoverMaterial::isOn(QStringLiteral("frontFlap"), parts));
    CHECK(CoverMaterial::isOn(QStringLiteral("back"), parts));
    CHECK_FALSE(CoverMaterial::isOn(QStringLiteral("jacketBack"), parts));
    CHECK(CoverMaterial::isOn(QStringLiteral("belly band"), parts));
    parts.jacket = true;
    // The jacket always has its flaps
    CHECK(CoverMaterial::isOn(QStringLiteral("jacketFrontFlap"), parts));
    CHECK(CoverMaterial::isWhole(QStringLiteral("jacketWhole")));
    CHECK(CoverMaterial::isWhole(QStringLiteral("whole")));
    CHECK_FALSE(CoverMaterial::isWhole(QStringLiteral("jacketFront")));
    CHECK(CoverMaterial::isJacketPlace(QStringLiteral("jacketSpine")));
    CHECK_FALSE(CoverMaterial::isJacketPlace(QStringLiteral("spine")));

    // Texts and pictures; the texts with a target; what goes on the cover
    CHECK(CoverMaterial::isText(QStringLiteral("bio")));
    CHECK(CoverMaterial::hasTarget(QStringLiteral("bio")));
    CHECK(CoverMaterial::choosesOne(QStringLiteral("bio")));
    CHECK(CoverMaterial::isPicture(QStringLiteral("authorPhoto")));
    CHECK(CoverMaterial::goesOnCover(QStringLiteral("authorPhoto")));
    CHECK(CoverMaterial::choosesOne(QStringLiteral("authorPhoto")));
    CHECK(CoverMaterial::canChange(QStringLiteral("bio"), QStringLiteral("blurb")));
    CHECK(CoverMaterial::canChange(QStringLiteral("authorPhoto"), QStringLiteral("design")));
    for (const char* text : {"blurb", "tagline", "opinion", "note"}) {
        CHECK(CoverMaterial::isText(QString::fromLatin1(text)));
        CHECK_FALSE(CoverMaterial::isPicture(QString::fromLatin1(text)));
    }
    CHECK(CoverMaterial::isPicture(QStringLiteral("design")));
    CHECK(CoverMaterial::isPicture(QStringLiteral("reference")));
    CHECK(CoverMaterial::hasTarget(QStringLiteral("blurb")));
    CHECK(CoverMaterial::hasTarget(QStringLiteral("opinion")));
    CHECK_FALSE(CoverMaterial::hasTarget(QStringLiteral("note")));
    CHECK_FALSE(CoverMaterial::hasTarget(QStringLiteral("design")));
    CHECK(CoverMaterial::goesOnCover(QStringLiteral("design")));
    CHECK_FALSE(CoverMaterial::goesOnCover(QStringLiteral("reference")));
    CHECK_FALSE(CoverMaterial::goesOnCover(QStringLiteral("note")));
    CHECK(CoverMaterial::choosesOne(QStringLiteral("blurb")));
    CHECK(CoverMaterial::choosesOne(QStringLiteral("design")));
    CHECK_FALSE(CoverMaterial::choosesOne(QStringLiteral("opinion")));

    // A material changes its part among the texts or among the pictures
    CHECK(CoverMaterial::canChange(QStringLiteral("blurb"), QStringLiteral("opinion")));
    CHECK(CoverMaterial::canChange(QStringLiteral("design"), QStringLiteral("reference")));
    CHECK_FALSE(CoverMaterial::canChange(QStringLiteral("blurb"), QStringLiteral("design")));
    CHECK_FALSE(CoverMaterial::canChange(QStringLiteral("reference"), QStringLiteral("note")));
}

TEST_CASE("Cover materials: lengths and targets", "[core][cover]") {
    // Characters with spaces, as publishers count them, or words
    const QString text = QStringLiteral("Trzy dni drogi. Jedna studnia.");
    CHECK(CoverMaterial::lengthOf(text, false) == 30);
    CHECK(CoverMaterial::lengthOf(text, true) == 5);
    CHECK(CoverMaterial::lengthOf(QStringLiteral("Dwa\nwiersze"), false) == 10);

    // The defaults of the settings, in the unit of the settings
    const ChangedSetting<std::string> unit(CoverMaterial::UNIT_SETTING, "characters");
    CHECK_FALSE(CoverMaterial::countsWords());
    CHECK(CoverMaterial::defaultTargetOf(QStringLiteral("blurb"), false) == 600);
    CHECK(CoverMaterial::defaultTargetOf(QStringLiteral("tagline"), false) == 60);
    CHECK(CoverMaterial::defaultTargetOf(QStringLiteral("opinion"), false) == 200);
    CHECK(CoverMaterial::defaultTargetOf(QStringLiteral("bio"), false) == 500);
    CHECK(CoverMaterial::defaultTargetOf(QStringLiteral("bio"), true) == 80);
    CHECK(CoverMaterial::defaultTargetOf(QStringLiteral("blurb"), true) == 100);
    CHECK(CoverMaterial::defaultTargetOf(QStringLiteral("note"), false) == 0);
    {
        const ChangedSetting<int> blurb(CoverMaterial::targetSettingOf(QStringLiteral("blurb"),
                                                                       false),
                                        750);
        CHECK(CoverMaterial::defaultTargetOf(QStringLiteral("blurb"), false) == 750);
    }
    {
        const ChangedSetting<std::string> words(CoverMaterial::UNIT_SETTING, "words");
        CHECK(CoverMaterial::countsWords());
    }

    // A material's own target, else the default of its part
    CoverData cover;
    cover.part = QStringLiteral("tagline");
    CHECK(CoverMaterial::targetOf(cover, false) == 60);
    cover.targetCharacters = 45;
    CHECK(CoverMaterial::targetOf(cover, false) == 45);
    CHECK(CoverMaterial::targetOf(cover, true) == 10);
    cover.part = QStringLiteral("note");
    CHECK(CoverMaterial::targetOf(cover, false) == 0);
}

TEST_CASE("Cover parts: the flaps, the jacket and the endpapers in the book file",
          "[core][cover]") {
    // A book without them has the flaps of the cover only
    const CoverParts none;
    CHECK(none.flaps);
    CHECK_FALSE(none.jacket);
    CHECK_FALSE(none.endpapers);

    const QByteArray text = R"({ "format": 2, "id": "p",
        "books": [ { "id": "b", "folder": "book",
                     "coverParts": { "flaps": false, "jacket": true, "endpapers": true,
                                     "frontEndpaper": { "fill": "pattern", "pattern": "diagonal",
                                                        "colors": ["#9a5a2e", "#efe2c2"] },
                                     "backEndpaper": { "fill": "tile", "picture": "m-17",
                                                       "scale": 40, "sameAsFront": true,
                                                       "gloss": "matte" },
                                     "spineWidth": 22 } } ],
        "workshop": { "grouped": false, "elements": [] } })";
    QStringList problems;
    const std::optional<BookProject> project = BookProject::fromJson(jsonOf(text), problems);
    INFO(problems.join(QStringLiteral("; ")).toStdString());
    REQUIRE(project.has_value());
    REQUIRE(problems.isEmpty());
    REQUIRE(project->books.at(0).coverParts.has_value());
    const CoverParts& parts = *project->books.at(0).coverParts;
    CHECK_FALSE(parts.flaps);
    CHECK(parts.jacket);
    CHECK(parts.endpapers);
    CHECK(parts.front.fill == QStringLiteral("pattern"));
    CHECK(parts.front.pattern == QStringLiteral("diagonal"));
    CHECK(parts.front.patternColor == QStringLiteral("#9a5a2e"));
    CHECK(parts.front.paperColor == QStringLiteral("#efe2c2"));
    CHECK(parts.front.scale == Endpaper::DEFAULT_SCALE);
    CHECK(parts.back.fill == QStringLiteral("tile"));
    CHECK(parts.back.picture == QStringLiteral("m-17"));
    CHECK(parts.back.scale == 40);
    CHECK(parts.back.sameAsFront);
    // The back endpaper as the front one shows the front's fill, and keeps its own
    CHECK(parts.backShown().fill == QStringLiteral("pattern"));
    // Fields this version does not know are kept, and saved back
    CHECK(parts.extra.keys() == QStringList{QStringLiteral("spineWidth")});
    CHECK(parts.back.extra.keys() == QStringList{QStringLiteral("gloss")});
    const auto partsOf = [](const QJsonObject& json) {
        return json.value(QStringLiteral("books"))
            .toArray()
            .at(0)
            .toObject()
            .value(QStringLiteral("coverParts"));
    };
    INFO(QJsonDocument(partsOf(project->toJson()).toObject()).toJson().toStdString());
    CHECK(partsOf(project->toJson()) == partsOf(jsonOf(text)));

    // An unknown pattern shows dots
    Endpaper odd;
    odd.pattern = QStringLiteral("tartan");
    CHECK(odd.shownPattern() == QStringLiteral("dots"));

    // A book without parts of its own writes none
    BookProject changed = *project;
    changed.books[0].coverParts.reset();
    CHECK_FALSE(changed.toJson()[QStringLiteral("books")][0].toObject().contains(
        QStringLiteral("coverParts")));
}

TEST_CASE("Cover parts: problems of their data", "[core][cover]") {
    const auto problemOf = [](const QByteArray& parts) {
        QStringList problems;
        BookProject::fromJson(
            jsonOf(R"({ "format": 2, "id": "p", "books": [ { "id": "b", "folder": "book", )"
                   R"("coverParts": )" +
                   parts + R"( } ], "workshop": { "grouped": false, "elements": [] } })"),
            problems);
        return problems.join(QStringLiteral("; "));
    };
    CHECK(problemOf("true") == QStringLiteral("books[0].coverParts: must be an object"));
    CHECK(problemOf(R"({ "jacket": "yes" })") ==
          QStringLiteral("books[0].coverParts.jacket: must be true or false"));
    CHECK(problemOf(R"({ "frontEndpaper": { "colors": ["red", "#ffffff"] } })") ==
          QStringLiteral("books[0].coverParts.frontEndpaper.colors: must be a list of two "
                         "colors: \"#RRGGBB\""));
    CHECK(problemOf(R"({ "backEndpaper": { "scale": 200 } })") ==
          QStringLiteral("books[0].coverParts.backEndpaper.scale: must be a whole number from 5 "
                         "to 100"));
    CHECK(problemOf(R"({ "backEndpaper": { "color": "#12345" } })") ==
          QStringLiteral("books[0].coverParts.backEndpaper.color: must be a color: \"#RRGGBB\""));
}
