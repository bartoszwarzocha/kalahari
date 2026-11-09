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
    CHECK(CoverMaterial::parts() ==
          QStringList{"blurb", "tagline", "opinion", "design", "reference", "note"});
    CHECK(CoverMaterial::places() ==
          QStringList{"front", "spine", "back", "frontFlap", "backFlap", "whole"});

    // Where a new material goes
    CHECK(CoverMaterial::defaultPlaceOf(QStringLiteral("blurb")) == QStringLiteral("back"));
    CHECK(CoverMaterial::defaultPlaceOf(QStringLiteral("tagline")) == QStringLiteral("front"));
    CHECK(CoverMaterial::defaultPlaceOf(QStringLiteral("opinion")) == QStringLiteral("back"));
    CHECK(CoverMaterial::defaultPlaceOf(QStringLiteral("design")) == QStringLiteral("whole"));
    CHECK(CoverMaterial::defaultPlaceOf(QStringLiteral("reference")).isEmpty());
    CHECK(CoverMaterial::defaultPlaceOf(QStringLiteral("note")).isEmpty());

    // Texts and pictures; the texts with a target; what goes on the cover
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
