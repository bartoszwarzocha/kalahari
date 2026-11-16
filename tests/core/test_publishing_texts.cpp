// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_publishing_texts.cpp
/// @brief The texts for publishers: their data in the book file, their templates and lengths,
/// and the set of a book

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_registry.h>
#include <kalahari/core/cover_material.h>
#include <kalahari/core/localized_text.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/publishing_texts.h>
#include <kalahari/core/settings_manager.h>

#include "test_support/window_parts.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <nlohmann/json_fwd.hpp>

#include <cstdio>
#include <optional>
#include <string>
#include <utility>

using kalahari::core::BookProject;
using kalahari::core::BookTypeRegistry;
using kalahari::core::CoverMaterial;
using kalahari::core::KindRef;
using kalahari::core::LocalizedText;
using kalahari::core::NewMaterial;
using kalahari::core::ProjectBook;
using kalahari::core::ProjectElement;
using kalahari::core::ProjectManager;
using kalahari::core::PublishingData;
using kalahari::core::PublishingSet;
using kalahari::core::PublishingSetPart;
using kalahari::core::PublishingSetText;
using kalahari::core::PublishingTemplate;
using kalahari::core::PublishingTemplatePoint;
using kalahari::core::PublishingTexts;
using kalahari::core::SettingsManager;

namespace {

QJsonObject jsonOf(const QByteArray& text) {
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(text, &error);
    INFO(error.errorString().toStdString());
    REQUIRE(document.isObject());
    return document.object();
}

/// A project with the Workshop elements @p elements and the back part @p back (JSON)
QByteArray projectWith(const QByteArray& elements, const QByteArray& back = "",
                       const QByteArray& bookFields = "") {
    return R"({ "format": 2, "id": "p",
                "books": [ { "id": "b", "folder": "book", "back": [ )" +
           back + "] " + bookFields + R"( } ],
                "workshop": { "grouped": false, "elements": [ )" +
           elements + "] } }";
}

BookProject projectOf(const QByteArray& text) {
    QStringList problems;
    std::optional<BookProject> project = BookProject::fromJson(jsonOf(text), problems);
    INFO(problems.join(QStringLiteral("; ")).toStdString());
    REQUIRE(project.has_value());
    REQUIRE(problems.isEmpty());
    return *project;
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
            static_cast<void>(
                std::fputs("ChangedSetting: the setting of the test stays\n", stderr));
        }
    }

    ChangedSetting(const ChangedSetting&) = delete;
    ChangedSetting& operator=(const ChangedSetting&) = delete;
    ChangedSetting(ChangedSetting&&) = delete;
    ChangedSetting& operator=(ChangedSetting&&) = delete;

private:
    std::string m_key;
    T m_before;
};

/// The problems of the template @p json of package "test.type"
QString templateProblems(const QByteArray& json) {
    QStringList problems;
    PublishingTemplate::fromPackageJson(jsonOf(json), QStringLiteral("test.type"),
                                        QStringLiteral("publishingTemplates[0]"), problems);
    return problems.join(QStringLiteral("; "));
}

LocalizedText english(const char* text) {
    LocalizedText localized;
    localized.values.insert(QStringLiteral("en"), QString::fromUtf8(text));
    return localized;
}

ProjectManager& projects() {
    auto& pm = kalahari::test::book();
    ProjectManager::loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(pm.bookTypes().problems().isEmpty());
    return pm;
}

}  // namespace

TEST_CASE("Texts for publishers: their data in the book file", "[core][publishing]") {
    const QByteArray elements = R"(
        { "id": "e1", "kind": "kalahari.base:work_note", "title": "Zdanie 1",
          "description": "Gdy studnia wysycha, Lejla musi odkryć, kto kłamie.",
          "publishing": { "part": "pitch", "state": "chosen",
                          "template": "kalahari.novel:when_must",
                          "fields": { "event": "studnia wysycha", "hero": "Lejla" },
                          "manual": true, "targetCharacters": 180, "tone": "light" } },
        { "id": "e2", "kind": "kalahari.base:work_note", "title": "Streszczenie",
          "publishing": { "part": "synopsis", "template": "user:t1",
                          "done": ["hero", "event"], "targetWords": 250 } },
        { "id": "e3", "kind": "kalahari.base:quote", "title": "Quote" })";
    const BookProject project = projectOf(projectWith(elements));

    const PublishingData& pitch = project.workshop.elements.at(0).publishing;
    CHECK(pitch.part == PublishingTexts::PITCH);
    CHECK(pitch.state == PublishingTexts::CHOSEN);
    CHECK(pitch.templateId == QStringLiteral("kalahari.novel:when_must"));
    CHECK(pitch.fields.value(QStringLiteral("hero")) == QStringLiteral("Lejla"));
    CHECK(pitch.manual);
    CHECK(pitch.targetCharacters == 180);
    CHECK(pitch.targetWords == 0);
    // A field this version does not know is kept, and saved back
    CHECK(pitch.extra.keys() == QStringList{QStringLiteral("tone")});
    CHECK(project.workshop.elements.at(0).extra.isEmpty());

    const PublishingData& synopsis = project.workshop.elements.at(1).publishing;
    CHECK(synopsis.part == PublishingTexts::SYNOPSIS);
    CHECK(synopsis.state.isEmpty());
    CHECK(synopsis.done == QStringList{QStringLiteral("hero"), QStringLiteral("event")});
    CHECK_FALSE(synopsis.manual);
    CHECK(synopsis.targetWords == 250);

    // Another element has none
    CHECK(project.workshop.elements.at(2).publishing.isEmpty());

    // The book is saved as it was read
    CHECK(project.toJson()[QStringLiteral("workshop")] ==
          jsonOf(projectWith(elements))[QStringLiteral("workshop")]);

    // A change of the data changes the project; data emptied leaves no "publishing"
    BookProject changed = project;
    changed.workshop.elements[0].publishing.manual = false;
    CHECK_FALSE(changed == project);
    const QJsonObject unmarked = changed.toJson()[QStringLiteral("workshop")]
                                     [QStringLiteral("elements")][0][QStringLiteral("publishing")]
                                         .toObject();
    CHECK_FALSE(unmarked.contains(QStringLiteral("manual")));
    changed.workshop.elements[1].publishing = PublishingData{};
    const QJsonObject written = changed.toJson()[QStringLiteral("workshop")]
                                    [QStringLiteral("elements")][1].toObject();
    CHECK_FALSE(written.contains(QStringLiteral("publishing")));
    CHECK(PublishingData{}.toJson().isEmpty());
}

TEST_CASE("Texts for publishers: problems of their data", "[core][publishing]") {
    const auto problemOf = [](const QByteArray& publishing) {
        QStringList problems;
        BookProject::fromJson(
            jsonOf(projectWith(R"({ "id": "e1", "kind": "kalahari.base:work_note",
                                     "publishing": )" + publishing + " }")),
            problems);
        return problems.join(QStringLiteral("; "));
    };
    const QString field = QStringLiteral("workshop.elements[0].publishing");
    CHECK(problemOf(R"("pitch")") == field + QStringLiteral(": must be an object"));
    CHECK(problemOf(R"({ "part": 4 })") == field + QStringLiteral(".part: must be a text"));
    CHECK(problemOf(R"({ "fields": ["a"] })") ==
          field + QStringLiteral(".fields: must be an object of texts by their names"));
    CHECK(problemOf(R"({ "fields": { "hero": 1 } })") ==
          field + QStringLiteral(".fields: must be an object of texts by their names"));
    CHECK(problemOf(R"({ "done": "hero" })") ==
          field + QStringLiteral(".done: must be a list of texts"));
    CHECK(problemOf(R"({ "done": [1] })") ==
          field + QStringLiteral(".done: must be a list of texts"));
    CHECK(problemOf(R"({ "manual": "yes" })") ==
          field + QStringLiteral(".manual: must be true or false"));
    CHECK(problemOf(R"({ "targetWords": -3 })") ==
          field + QStringLiteral(".targetWords: must be a whole number, 0 or more"));
    CHECK(problemOf(R"({ "targetCharacters": 1.5 })") ==
          field + QStringLiteral(".targetCharacters: must be a whole number, 0 or more"));
}

TEST_CASE("Texts for publishers: the set of a book in the book file", "[core][publishing]") {
    const BookProject project = projectOf(projectWith(
        "", "", R"(, "publishingSet": { "order": ["synopsis", "pitch"], "off": ["tagline"],
                                        "picks": { "blurb": "e9" }, "title": "Do Znaku" })"));
    const ProjectBook& book = project.books.first();
    REQUIRE(book.publishingSet.has_value());
    const PublishingSet& set = *book.publishingSet;
    CHECK(set.order == QStringList{QStringLiteral("synopsis"), QStringLiteral("pitch")});
    CHECK(set.off == QStringList{QStringLiteral("tagline")});
    CHECK(set.picks.value(QStringLiteral("blurb")) == QStringLiteral("e9"));
    CHECK(set.extra.keys() == QStringList{QStringLiteral("title")});
    const QJsonObject saved = project.toJson()[QStringLiteral("books")][0]
                                  [QStringLiteral("publishingSet")].toObject();
    CHECK(saved == set.toJson());
    CHECK(saved.value(QStringLiteral("title")).toString() == QStringLiteral("Do Znaku"));

    // A book without its own set has none in the file
    BookProject without = project;
    without.books.first().publishingSet.reset();
    CHECK_FALSE(without.toJson()[QStringLiteral("books")][0].toObject().contains(
        QStringLiteral("publishingSet")));
    // Without a pick or a part left out, the file has neither
    const QJsonObject plain = PublishingSet{{QStringLiteral("pitch")}, {}, {}, {}}.toJson();
    CHECK(plain.keys() == QStringList{QStringLiteral("order")});

    QStringList problems;
    BookProject::fromJson(jsonOf(projectWith("", "", R"(, "publishingSet": ["pitch"])")),
                          problems);
    CHECK(problems == QStringList{QStringLiteral("books[0].publishingSet: must be an object")});
    problems.clear();
    BookProject::fromJson(
        jsonOf(projectWith("", "", R"(, "publishingSet": { "order": "pitch" })")), problems);
    CHECK(problems ==
          QStringList{QStringLiteral("books[0].publishingSet.order: must be a list of texts")});
}

TEST_CASE("Texts for publishers: the parts of a set", "[core][publishing]") {
    PublishingSet set;
    set.order = {QStringLiteral("synopsis"), QStringLiteral("letter"), QStringLiteral("pitch"),
                 QStringLiteral("synopsis")};
    set.off = {QStringLiteral("blurb")};
    // The parts the program knows in the writer's order, each once, then the others
    CHECK(set.parts() == QStringList{QStringLiteral("synopsis"), QStringLiteral("pitch"),
                                     QStringLiteral("tagline"), QStringLiteral("blurb"),
                                     QStringLiteral("aboutAuthor")});
    CHECK(set.includes(QStringLiteral("pitch")));
    CHECK_FALSE(set.includes(QStringLiteral("blurb")));

    // A new book: all parts, in the order of the Settings
    const PublishingSet fresh = PublishingSet::defaults();
    CHECK(fresh.order == PublishingTexts::setParts());
    CHECK(fresh.off.isEmpty());
    CHECK(fresh.picks.isEmpty());

    // The parts the Settings leave out are off, after those they have
    const ChangedSetting<nlohmann::json> chosen(PublishingTexts::SET_SETTING,
                                                nlohmann::json::array({"synopsis", "pitch"}));
    const PublishingSet some = PublishingSet::defaults();
    CHECK(some.order == QStringList{QStringLiteral("synopsis"), QStringLiteral("pitch"),
                                    QStringLiteral("tagline"), QStringLiteral("blurb"),
                                    QStringLiteral("aboutAuthor")});
    CHECK(some.off == QStringList{QStringLiteral("tagline"), QStringLiteral("blurb"),
                                  QStringLiteral("aboutAuthor")});
}

TEST_CASE("Texts for publishers: a sentence made of the fields of its pattern",
          "[core][publishing]") {
    const QString pattern = QStringLiteral("Gdy {event}, {hero} musi {goal}, zanim {stakes}.");
    CHECK(PublishingTexts::fieldsOf(pattern) ==
          QStringList{QStringLiteral("event"), QStringLiteral("hero"), QStringLiteral("goal"),
                      QStringLiteral("stakes")});
    CHECK(PublishingTexts::fieldsOf(QStringLiteral("{a} and {a} or {B} {b_2}")) ==
          QStringList{QStringLiteral("a"), QStringLiteral("b_2")});

    QMap<QString, QString> fields{{QStringLiteral("event"), QStringLiteral(" studnia wysycha ")},
                                  {QStringLiteral("hero"), QStringLiteral("Lejla")},
                                  {QStringLiteral("goal"), QStringLiteral("odkryć,  kto kłamie")}};
    // An empty field is "…", the spaces run together
    CHECK(PublishingTexts::composePitch(pattern, fields) ==
          QStringLiteral("Gdy studnia wysycha, Lejla musi odkryć, kto kłamie, zanim …."));
    // The sentence starts with a capital letter, also after the quotation mark that opens it
    CHECK(PublishingTexts::composePitch(QStringLiteral("{hero}, kartografka."), fields) ==
          QStringLiteral("Lejla, kartografka."));
    fields.insert(QStringLiteral("hero"), QStringLiteral("młoda kartografka"));
    CHECK(PublishingTexts::composePitch(QStringLiteral("{hero} szuka wody."), fields) ==
          QStringLiteral("Młoda kartografka szuka wody."));
    CHECK(PublishingTexts::composePitch(QStringLiteral("„{hero}” w drodze."), fields) ==
          QStringLiteral("„Młoda kartografka” w drodze."));
    CHECK(PublishingTexts::composePitch(QStringLiteral("{nothing}, dalej."), fields) ==
          QStringLiteral("…, dalej."));
    CHECK(PublishingTexts::composePitch(QString(), fields).isEmpty());
}

TEST_CASE("Texts for publishers: lengths and targets", "[core][publishing]") {
    // The defaults of the Settings
    CHECK(PublishingTexts::defaultTargetOf(QStringLiteral("pitch"), false) == 200);
    CHECK(PublishingTexts::defaultTargetOf(QStringLiteral("pitch"), true) == 35);
    CHECK(PublishingTexts::defaultTargetOf(QStringLiteral("synopsis"), false) == 1800);
    CHECK(PublishingTexts::defaultTargetOf(QStringLiteral("synopsis"), true) == 300);
    CHECK(PublishingTexts::defaultTargetOf(QStringLiteral("blurb"), false) == 0);
    CHECK(PublishingTexts::targetSettingOf(QStringLiteral("synopsis"), true) ==
          "materials.publishing.targets.synopsis.words");
    {
        const ChangedSetting<int> changed("materials.publishing.targets.synopsis.characters",
                                          -5);
        CHECK(PublishingTexts::defaultTargetOf(QStringLiteral("synopsis"), false) == 0);
    }

    // Its own target, else its template's (of each text, times the texts), else the default
    PublishingData data;
    data.part = QString::fromLatin1(PublishingTexts::SYNOPSIS);
    PublishingTemplate templ;
    templ.targetCharacters = 1000;
    templ.targetWords = 170;
    CHECK(PublishingTexts::targetOf(data, nullptr, 7, false) == 1800);
    CHECK(PublishingTexts::targetOf(data, &templ, 7, false) == 1000);
    CHECK(PublishingTexts::targetOf(data, &templ, 7, true) == 170);
    templ.eachText = true;
    templ.targetCharacters = 600;
    CHECK(PublishingTexts::targetOf(data, &templ, 7, false) == 4200);
    CHECK(PublishingTexts::targetOf(data, &templ, 0, false) == 600);
    data.targetCharacters = 2500;
    CHECK(PublishingTexts::targetOf(data, &templ, 7, false) == 2500);
    CHECK(PublishingTexts::targetOf(data, &templ, 7, true) == 170 * 7);
    templ.targetWords = 0;
    CHECK(PublishingTexts::targetOf(data, &templ, 7, true) == 300);

    // Standard pages, to one decimal
    CHECK(PublishingTexts::pagesOf(1105) == 0.6);
    CHECK(PublishingTexts::pagesOf(1800) == 1.0);
    CHECK(PublishingTexts::pagesOf(5400) == 3.0);
    CHECK(PublishingTexts::pagesOf(0) == 0.0);

    // The length in the unit of the cover's texts
    CHECK(PublishingTexts::lengthOf(u"Trzy dni drogi.", true) == 3);
    CHECK(PublishingTexts::lengthOf(u"Trzy dni drogi.", false) == 15);

    // Where the points of a template are, and the pages
    CHECK_FALSE(PublishingTexts::pointsInText());
    CHECK(PublishingTexts::showsPages());
    {
        const ChangedSetting<std::string> inText(PublishingTexts::POINTS_SETTING, "inText");
        const ChangedSetting<bool> pages(PublishingTexts::PAGES_SETTING, false);
        CHECK(PublishingTexts::pointsInText());
        CHECK_FALSE(PublishingTexts::showsPages());
    }
    CHECK(PublishingTexts::hasPoints(QStringLiteral("synopsis")));
    CHECK_FALSE(PublishingTexts::hasPoints(QStringLiteral("pitch")));
    CHECK(PublishingTexts::hasPattern(QStringLiteral("pitch")));
    CHECK_FALSE(PublishingTexts::hasPattern(QStringLiteral("synopsis")));
}

TEST_CASE("Texts for publishers: templates of a package", "[core][publishing]") {
    QStringList problems;
    const std::optional<PublishingTemplate> synopsis = PublishingTemplate::fromPackageJson(
        jsonOf(R"({ "id": "one_page", "part": "synopsis",
                    "name": { "pl": "Jedna strona", "en": "One page" },
                    "target": { "characters": 1800, "words": 300 },
                    "points": [ { "id": "hero", "name": { "en": "Hero" },
                                  "hint": { "en": "Who the hero is." } },
                                { "id": "ending", "name": { "en": "Ending" } } ] })"),
        QStringLiteral("test.type"), QStringLiteral("publishingTemplates[0]"), problems);
    INFO(problems.join(QStringLiteral("; ")).toStdString());
    REQUIRE(synopsis.has_value());
    CHECK(synopsis->package == QStringLiteral("test.type"));
    CHECK(synopsis->reference() == QStringLiteral("test.type:one_page"));
    CHECK_FALSE(synopsis->isOwn());
    CHECK(synopsis->isFor(QStringLiteral("other.type")));
    CHECK(synopsis->name.text(QStringLiteral("pl")) == QStringLiteral("Jedna strona"));
    CHECK(synopsis->targetCharacters == 1800);
    CHECK(synopsis->targetWords == 300);
    REQUIRE(synopsis->points.size() == 2);
    CHECK(synopsis->points.at(0).id == QStringLiteral("hero"));
    CHECK(synopsis->points.at(0).hint.text(QStringLiteral("en")) ==
          QStringLiteral("Who the hero is."));
    CHECK(synopsis->points.at(1).hint.isEmpty());
    // Its own points, whatever the texts of the book
    CHECK(synopsis->pointsFor({{QStringLiteral("c1"), QStringLiteral("Studnia")}}) ==
          synopsis->points);

    const std::optional<PublishingTemplate> each = PublishingTemplate::fromPackageJson(
        jsonOf(R"({ "id": "each_chapter", "part": "synopsis", "eachText": true,
                    "name": { "en": "Chapter by chapter" },
                    "hint": { "en": "What happens." },
                    "target": { "characters": 600, "words": 100 } })"),
        QStringLiteral("test.type"), QStringLiteral("publishingTemplates[1]"), problems);
    REQUIRE(each.has_value());
    CHECK(each->eachText);
    const QList<PublishingTemplatePoint> points =
        each->pointsFor({{QStringLiteral("c1"), QStringLiteral("Studnia")},
                         {QStringLiteral("c2"), QStringLiteral("Burza")}});
    REQUIRE(points.size() == 2);
    CHECK(points.at(1).id == QStringLiteral("c2"));
    CHECK(points.at(1).name.text(QStringLiteral("pl")) == QStringLiteral("Burza"));
    CHECK(points.at(1).hint.text(QStringLiteral("en")) == QStringLiteral("What happens."));

    const std::optional<PublishingTemplate> pitch = PublishingTemplate::fromPackageJson(
        jsonOf(R"({ "id": "when_must", "part": "pitch", "name": { "en": "When… must…" },
                    "pattern": { "pl": "Gdy {event}, {hero} musi.",
                                 "en": "When {event}, {hero} must." },
                    "fields": [ { "id": "event", "name": { "en": "event" },
                                  "hint": { "en": "what happens" } },
                                { "id": "hero", "name": { "en": "hero" },
                                  "hint": { "en": "who" } } ],
                    "target": { "characters": 200, "words": 35 } })"),
        QStringLiteral("test.type"), QStringLiteral("publishingTemplates[2]"), problems);
    REQUIRE(pitch.has_value());
    CHECK(pitch->pattern.text(QStringLiteral("pl")) == QStringLiteral("Gdy {event}, {hero} musi."));
    CHECK(pitch->fields.size() == 2);
    CHECK(problems.isEmpty());
}

TEST_CASE("Texts for publishers: problems of the templates of a package",
          "[core][publishing]") {
    const QString field = QStringLiteral("publishingTemplates[0]");
    CHECK(templateProblems(R"({ "id": "Big", "part": "synopsis", "name": { "en": "N" },
                                "target": { "characters": 1, "words": 1 },
                                "points": [ { "id": "a", "name": { "en": "A" } } ] })") ==
          field + QStringLiteral(".id: must be lowercase letters, digits and _, e.g. "
                                 "\"one_page\""));
    CHECK(templateProblems(R"({ "id": "t", "part": "letter", "name": { "en": "N" },
                                "target": { "characters": 1, "words": 1 } })") ==
          field + QStringLiteral(".part: must be \"pitch\" or \"synopsis\""));
    CHECK(templateProblems(R"({ "id": "t", "part": "synopsis", "name": { "en": "N" },
                                "target": { "characters": 0, "words": 1 }, "colour": 1,
                                "points": [ { "id": "a", "name": { "en": "A" } } ] })") ==
          field + QStringLiteral(": unknown key 'colour'; ") + field +
              QStringLiteral(".target: must be the target length in characters and in words, "
                             "both above 0, e.g. { \"characters\": 1800, \"words\": 300 }"));
    CHECK(templateProblems(R"({ "id": "t", "part": "synopsis",
                                "target": { "characters": 1, "words": 1 },
                                "points": [] })") ==
          field + QStringLiteral(".name: is missing; ") + field +
              QStringLiteral(".points: must be a list that is not empty"));
    CHECK(templateProblems(R"({ "id": "t", "part": "synopsis", "name": { "en": "N" },
                                "target": { "characters": 1, "words": 1 },
                                "points": [ { "id": "a", "name": { "en": "A" } },
                                            { "id": "a", "name": { "en": "B" } }, 4 ] })") ==
          field + QStringLiteral(".points[1].id: 'a' is already the id of another one; ") +
              field + QStringLiteral(".points[2]: must be an object"));
    CHECK(templateProblems(R"({ "id": "t", "part": "synopsis", "name": { "en": "N" },
                                "eachText": true, "points": [],
                                "target": { "characters": 1, "words": 1 } })") ==
          field + QStringLiteral(".points: a template of a synopsis of each text does not "
                                 "have it; ") +
              field + QStringLiteral(".hint: is missing"));
    CHECK(templateProblems(R"({ "id": "t", "part": "synopsis", "name": { "en": "N" },
                                "eachText": "yes", "hint": { "en": "H" }, "pattern": { "en": "P" },
                                "target": { "characters": 1, "words": 1 },
                                "points": [ { "id": "a", "name": { "en": "A" } } ] })") ==
          field + QStringLiteral(".pattern: a template of a synopsis does not have it; ") + field +
              QStringLiteral(".eachText: must be true or false; ") + field +
              QStringLiteral(".hint: a template of a synopsis with its own points does not have "
                             "it"));
    CHECK(templateProblems(R"({ "id": "t", "part": "pitch", "name": { "en": "N" },
                                "points": [], "target": { "characters": 1, "words": 1 },
                                "pattern": { "pl": "{a} {c}", "en": "{a}" },
                                "fields": [ { "id": "a", "name": { "en": "A" } },
                                            { "id": "b", "name": { "en": "B" },
                                              "hint": { "en": "b" } } ] })") ==
          field + QStringLiteral(".points: a template of a pitch does not have it; ") + field +
              QStringLiteral(".fields[0].hint: is missing; ") + field +
              QStringLiteral(".pattern.pl: names {c}, which is not one of its fields; ") + field +
              QStringLiteral(".fields: the pattern does not name the field 'b'"));

    // The list of a package: each id once
    QStringList problems;
    const QList<PublishingTemplate> templates = PublishingTemplate::listFromPackageJson(
        QJsonValue(QJsonArray{
            jsonOf(R"({ "id": "t", "part": "synopsis", "name": { "en": "N" },
                        "target": { "characters": 1, "words": 1 },
                        "points": [ { "id": "a", "name": { "en": "A" } } ] })"),
            jsonOf(R"({ "id": "t", "part": "synopsis", "name": { "en": "M" },
                        "target": { "characters": 1, "words": 1 },
                        "points": [ { "id": "a", "name": { "en": "A" } } ] })"),
            QStringLiteral("t2")}),
        QStringLiteral("test.type"), problems);
    CHECK(templates.size() == 1);
    CHECK(problems ==
          QStringList{QStringLiteral("publishingTemplates[1].id: 't' is already the id of "
                                     "another template"),
                      QStringLiteral("publishingTemplates[2]: must be an object")});
    problems.clear();
    CHECK(PublishingTemplate::listFromPackageJson(QJsonValue(QJsonValue::Undefined),
                                                  QStringLiteral("test.type"), problems)
              .isEmpty());
    CHECK(problems.isEmpty());
    PublishingTemplate::listFromPackageJson(QJsonValue(QStringLiteral("t")),
                                            QStringLiteral("test.type"), problems);
    CHECK(problems ==
          QStringList{QStringLiteral("publishingTemplates: must be a list of templates")});
}

TEST_CASE("Texts for publishers: the writer's own templates", "[core][publishing]") {
    PublishingTemplate own;
    own.id = QStringLiteral("t1");
    own.part = QString::fromLatin1(PublishingTexts::SYNOPSIS);
    own.name = english("Konkurs „Złote Pióro”");
    own.types = {QStringLiteral("kalahari.novel")};
    own.targetCharacters = 1000;
    own.targetWords = 170;
    own.points = {{QStringLiteral("p1"), english("Bohater i świat"), english("Dwa zdania.")},
                  {QStringLiteral("p2"), english("Zakończenie"), LocalizedText{}}};
    own.extra.insert(QStringLiteral("colour"), QStringLiteral("blue"));
    CHECK(own.isOwn());
    CHECK(own.reference() == QStringLiteral("user:t1"));
    CHECK(own.isFor(QStringLiteral("kalahari.novel")));
    CHECK_FALSE(own.isFor(QStringLiteral("kalahari.poetry")));

    const QJsonObject json = own.toOwnJson();
    CHECK(json.value(QStringLiteral("name")).toString() == QStringLiteral("Konkurs „Złote Pióro”"));
    CHECK_FALSE(json.contains(QStringLiteral("eachText")));
    CHECK_FALSE(json.contains(QStringLiteral("fields")));
    CHECK(PublishingTemplate::fromOwnJson(json) == own);

    PublishingTemplate pitch;
    pitch.id = QStringLiteral("t2");
    pitch.part = QString::fromLatin1(PublishingTexts::PITCH);
    pitch.name = english("Krótko");
    pitch.pattern = english("{hero} w {place}.");
    pitch.fields = {{QStringLiteral("hero"), english("bohater"), english("kto")},
                    {QStringLiteral("place"), english("miejsce"), LocalizedText{}}};
    CHECK(pitch.isFor(QStringLiteral("kalahari.poetry")));
    CHECK(PublishingTemplate::fromOwnJson(pitch.toOwnJson()) == pitch);

    PublishingTemplate each;
    each.id = QStringLiteral("t3");
    each.part = QString::fromLatin1(PublishingTexts::SYNOPSIS);
    each.eachText = true;
    each.hint = english("Dwa zdania.");
    CHECK(PublishingTemplate::fromOwnJson(each.toOwnJson()) == each);

    // A template without an id or a part is none
    CHECK_FALSE(PublishingTemplate::fromOwnJson(jsonOf(R"({ "part": "pitch" })")));
    CHECK_FALSE(PublishingTemplate::fromOwnJson(jsonOf(R"({ "id": "t", "part": "letter" })")));

    // In the Settings: each id once
    const ChangedSetting<nlohmann::json> none(PublishingTexts::TEMPLATES_SETTING,
                                              nlohmann::json::array());
    CHECK(PublishingTexts::ownTemplates().isEmpty());
    PublishingTemplate twice = pitch;
    twice.name = english("Drugi");
    PublishingTexts::setOwnTemplates({own, pitch, twice});
    const QList<PublishingTemplate> kept = PublishingTexts::ownTemplates();
    REQUIRE(kept.size() == 2);
    CHECK(kept.at(0) == own);
    CHECK(kept.at(1) == pitch);
    CHECK(PublishingTexts::newOwnTemplateId(kept) == QStringLiteral("t3"));
    CHECK(PublishingTexts::newOwnTemplateId({}) == QStringLiteral("t1"));
}

TEST_CASE("Texts for publishers: the parts of the set and their versions",
          "[core][publishing]") {
    const QByteArray elements = R"(
        { "id": "p1", "kind": "kalahari.base:work_note", "title": "Zdanie 1",
          "publishing": { "part": "pitch", "state": "rejected" } },
        { "id": "g", "kind": "kalahari.base:work_note", "title": "Group", "elements": [
          { "id": "p2", "kind": "kalahari.base:work_note", "title": "Zdanie 2",
            "publishing": { "part": "pitch", "state": "chosen" } } ] },
        { "id": "s1", "kind": "kalahari.base:work_note", "title": "Streszczenie",
          "publishing": { "part": "synopsis" } },
        { "id": "b1", "kind": "kalahari.base:cover", "title": "Opis – na skrzydełko",
          "cover": { "part": "blurb", "place": "frontFlap", "state": "chosen" } },
        { "id": "b2", "kind": "kalahari.base:cover", "title": "Opis – na tył",
          "cover": { "part": "blurb", "place": "back", "state": "chosen" } },
        { "id": "b3", "kind": "kalahari.base:cover", "title": "Opis – odrzucony",
          "cover": { "part": "blurb", "place": "back", "state": "rejected" } },
        { "id": "t1", "kind": "kalahari.base:cover", "title": "Hasło",
          "cover": { "part": "tagline", "place": "spine", "state": "chosen" } })";
    const QByteArray back = R"(
        { "id": "a1", "kind": "kalahari.base:afterword", "title": "Posłowie" },
        { "id": "a2", "kind": "kalahari.base:about_author", "title": "O autorce" })";
    const BookProject project = projectOf(projectWith(elements, back));
    const ProjectBook& book = project.books.first();

    const auto idsOf = [](const QList<const ProjectElement*>& found) {
        QStringList ids;
        for (const ProjectElement* element : found) {
            ids << element->id;
        }
        return ids;
    };
    CHECK(idsOf(PublishingTexts::textsOf(project, QStringLiteral("pitch"))) ==
          QStringList{QStringLiteral("p1"), QStringLiteral("p2")});
    CHECK(idsOf(PublishingTexts::versionsOf(project, QStringLiteral("blurb"))) ==
          QStringList{QStringLiteral("b1"), QStringLiteral("b2"), QStringLiteral("b3")});
    CHECK(idsOf(PublishingTexts::versionsOf(project, QStringLiteral("synopsis"))) ==
          QStringList{QStringLiteral("s1")});
    CHECK(PublishingTexts::versionsOf(project, QStringLiteral("aboutAuthor")).isEmpty());

    // The versions chosen: the description for the back cover, a tagline of another place,
    // no synopsis chosen, the note about the author of the back part
    PublishingSet set = PublishingSet::defaults();
    set.off = {QStringLiteral("tagline")};
    QList<PublishingSetPart> parts = PublishingTexts::resolveSet(project, book, set);
    REQUIRE(parts.size() == 5);
    CHECK(parts.at(0) == PublishingSetPart{QStringLiteral("pitch"), QStringLiteral("p2"), true,
                                           false});
    CHECK(parts.at(1) == PublishingSetPart{QStringLiteral("tagline"), QStringLiteral("t1"),
                                           false, false});
    CHECK(parts.at(2) == PublishingSetPart{QStringLiteral("blurb"), QStringLiteral("b2"), true,
                                           false});
    CHECK(parts.at(3) == PublishingSetPart{QStringLiteral("synopsis"), QString(), true, false});
    CHECK(parts.at(4) == PublishingSetPart{QStringLiteral("aboutAuthor"), QStringLiteral("a2"),
                                           true, false});

    // A pick of a version of its part takes the place of the chosen one; another is ignored
    set.picks = {{QStringLiteral("blurb"), QStringLiteral("b3")},
                 {QStringLiteral("pitch"), QStringLiteral("s1")}};
    parts = PublishingTexts::resolveSet(project, book, set);
    CHECK(parts.at(2) == PublishingSetPart{QStringLiteral("blurb"), QStringLiteral("b3"), true,
                                           true});
    CHECK(parts.at(0).elementId == QStringLiteral("p2"));
    CHECK_FALSE(parts.at(0).picked);

    // A description chosen for a flap only is the one of the set
    BookProject flapOnly = project;
    flapOnly.workshop.elements[4].cover.state.clear();
    set.picks.clear();
    CHECK(PublishingTexts::resolveSet(flapOnly, flapOnly.books.first(), set).at(2).elementId ==
          QStringLiteral("b1"));
}

TEST_CASE("Texts for publishers: one text of a part is chosen", "[core][publishing]") {
    BookProject project = projectOf(projectWith(R"(
        { "id": "p1", "kind": "kalahari.base:work_note",
          "publishing": { "part": "pitch", "state": "chosen" } },
        { "id": "p2", "kind": "kalahari.base:work_note", "publishing": { "part": "pitch" } },
        { "id": "s1", "kind": "kalahari.base:work_note",
          "publishing": { "part": "synopsis", "state": "chosen" } })"));
    const auto stateOf = [&project](qsizetype index) {
        return project.workshop.elements.at(index).publishing.state;
    };
    CHECK(PublishingTexts::setState(project, QStringLiteral("p2"), PublishingTexts::CHOSEN) ==
          QStringList{QStringLiteral("p1")});
    CHECK(stateOf(0).isEmpty());
    CHECK(stateOf(1) == PublishingTexts::CHOSEN);
    CHECK(stateOf(2) == PublishingTexts::CHOSEN);
    CHECK(PublishingTexts::setState(project, QStringLiteral("p1"), PublishingTexts::REJECTED)
              .isEmpty());
    CHECK(stateOf(0) == PublishingTexts::REJECTED);
    CHECK(stateOf(1) == PublishingTexts::CHOSEN);
    CHECK(PublishingTexts::setState(project, QStringLiteral("none"), PublishingTexts::CHOSEN)
              .isEmpty());
}

TEST_CASE("Texts for publishers: the set as plain text and as Markdown", "[core][publishing]") {
    const QList<PublishingSetText> parts{
        {QStringLiteral("Zdanie o książce"), QStringLiteral("Gdy studnia wysycha.")},
        {QStringLiteral("Streszczenie"),
         QStringLiteral("Sahara, 1912.\n\n# Nie nagłówek\n- nie lista\n12. nie lista")}};
    CHECK(PublishingTexts::setToPlainText(QStringLiteral("Karawana"),
                                          QStringLiteral("Ewa Lis · powieść"), parts) ==
          QStringLiteral("Karawana\nEwa Lis · powieść\n\nZDANIE O KSIĄŻCE\nGdy studnia "
                         "wysycha.\n\nSTRESZCZENIE\nSahara, 1912.\n\n# Nie nagłówek\n- nie "
                         "lista\n12. nie lista\n"));
    CHECK(PublishingTexts::setToPlainText(QString(), QStringLiteral(" "), parts.mid(0, 1)) ==
          QStringLiteral("ZDANIE O KSIĄŻCE\nGdy studnia wysycha.\n"));
    CHECK(PublishingTexts::setToMarkdown(QStringLiteral("Karawana"),
                                         QStringLiteral("Ewa Lis · powieść"), parts) ==
          QStringLiteral("# Karawana\n\nEwa Lis · powieść\n\n## Zdanie o książce\n\nGdy studnia "
                         "wysycha.\n\n## Streszczenie\n\nSahara, 1912.\n\n\\# Nie "
                         "nagłówek\n\n\\- nie lista\n\n\\12. nie lista\n"));
}

TEST_CASE("Texts for publishers: the templates of the book types", "[core][publishing]") {
    BookTypeRegistry registry;
    registry.load({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(registry.problems().isEmpty());
    const auto referencesOf = [&registry](const char* type) {
        QStringList references;
        for (const PublishingTemplate& templ :
             registry.publishingTemplates(QString::fromLatin1(type))) {
            references << templ.reference();
        }
        return references;
    };
    CHECK(referencesOf("kalahari.novel") ==
          QStringList{QStringLiteral("kalahari.novel:one_page"),
                      QStringLiteral("kalahari.novel:full"),
                      QStringLiteral("kalahari.novel:each_chapter"),
                      QStringLiteral("kalahari.novel:when_must"),
                      QStringLiteral("kalahari.novel:hero_goal"),
                      QStringLiteral("kalahari.novel:comparison")});
    CHECK(referencesOf("kalahari.short_stories").size() == 4);
    CHECK(referencesOf("kalahari.nonfiction").size() == 4);
    CHECK(referencesOf("kalahari.poetry").size() == 2);
    // The screenplay keeps its own kinds: a logline, a synopsis and a treatment
    CHECK(referencesOf("kalahari.screenplay").isEmpty());
    CHECK(referencesOf("no.such").isEmpty());

    const std::optional<PublishingTemplate> each =
        registry.findPublishingTemplate(QStringLiteral("kalahari.novel:each_chapter"));
    REQUIRE(each.has_value());
    CHECK(each->eachText);
    CHECK(each->targetCharacters == 600);
    CHECK(each->name.text(QStringLiteral("pl")) == QStringLiteral("Rozdział po rozdziale"));
    CHECK_FALSE(registry.findPublishingTemplate(QStringLiteral("kalahari.novel:none")));
    CHECK_FALSE(registry.findPublishingTemplate(QStringLiteral("none:one_page")));
    CHECK_FALSE(registry.findPublishingTemplate(QStringLiteral("user:t1")));
    CHECK_FALSE(registry.findPublishingTemplate(QStringLiteral("one_page")));
}

TEST_CASE("ProjectManager keeps the texts for publishers of a book", "[core][publishing]") {
    const QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Karawana", "Ewa Lis", "pl", true, "kalahari.novel"));
    const QString manifest = pm.getManifestPath();
    const KindRef cover = pm.bookTypes().findKind(QStringLiteral("kalahari.base"),
                                                  QStringLiteral("cover"));
    REQUIRE(cover);
    const auto addText = [&pm](const KindRef& kind, const char* part, const char* text) {
        NewMaterial material;
        material.title = QString::fromUtf8(text);
        material.description = QString::fromUtf8(text);
        material.publishing.part = QString::fromLatin1(part);
        return pm.addMaterial(material, kind);
    };
    const KindRef quote = pm.bookTypes().findKind(QStringLiteral("kalahari.base"),
                                                  QStringLiteral("quote"));
    const QString first = addText(quote, PublishingTexts::PITCH, "Gdy studnia wysycha.");
    const QString second = addText(quote, PublishingTexts::PITCH, "Lejla szuka wody.");
    const QString synopsis = addText(quote, PublishingTexts::SYNOPSIS, "Sahara, 1912.");
    NewMaterial blurb;
    blurb.title = QStringLiteral("Opis");
    blurb.description = QStringLiteral("Trzy dni drogi.");
    blurb.cover.part = QString::fromLatin1(CoverMaterial::BLURB);
    blurb.cover.place = QString::fromLatin1(CoverMaterial::BACK);
    const QString blurbId = pm.addMaterial(blurb, cover);
    for (const QString& id : {first, second, synopsis, blurbId}) {
        REQUIRE_FALSE(id.isEmpty());
    }
    CHECK(pm.findElement(first)->publishing.part == PublishingTexts::PITCH);

    // One text of a part is chosen for the set
    QStringList drafts;
    REQUIRE(pm.setPublishingState(first, PublishingTexts::CHOSEN, &drafts));
    CHECK(drafts.isEmpty());
    REQUIRE(pm.setPublishingState(second, PublishingTexts::CHOSEN, &drafts));
    CHECK(drafts == QStringList{first});
    REQUIRE(pm.setPublishingState(second, PublishingTexts::CHOSEN, &drafts));
    CHECK(drafts.isEmpty());
    CHECK_FALSE(pm.setPublishingState(blurbId, PublishingTexts::CHOSEN, &drafts));
    CHECK_FALSE(pm.setPublishingState(QStringLiteral("none"), PublishingTexts::CHOSEN));
    REQUIRE(pm.setCoverState(blurbId, CoverMaterial::CHOSEN));

    // Its template and its target length
    PublishingData data = pm.findElement(synopsis)->publishing;
    data.templateId = QStringLiteral("kalahari.novel:each_chapter");
    REQUIRE(pm.setPublishing(synopsis, data));
    const int chapters = static_cast<int>(pm.mainTexts().size());
    REQUIRE(chapters > 0);
    CHECK(pm.publishingTargetOf(synopsis, false) == 600 * chapters);
    CHECK(pm.publishingTargetOf(first, true) == 35);
    CHECK(pm.publishingTargetOf(QStringLiteral("none"), true) == 0);
    REQUIRE(pm.publishingTemplate(data.templateId).has_value());
    CHECK_FALSE(pm.publishingTemplate(QStringLiteral("user:t1")).has_value());

    // The templates of the book: of its type, then the writer's own for it
    {
        PublishingTemplate own;
        own.id = QStringLiteral("t1");
        own.part = QString::fromLatin1(PublishingTexts::SYNOPSIS);
        own.name = english("Konkurs");
        PublishingTemplate poems = own;
        poems.id = QStringLiteral("t2");
        poems.types = {QStringLiteral("kalahari.poetry")};
        const ChangedSetting<nlohmann::json> none(PublishingTexts::TEMPLATES_SETTING,
                                                  nlohmann::json::array());
        PublishingTexts::setOwnTemplates({own, poems});
        const QList<PublishingTemplate> templates = pm.publishingTemplates();
        REQUIRE(templates.size() == 7);
        CHECK(templates.first().reference() == QStringLiteral("kalahari.novel:one_page"));
        CHECK(templates.last().reference() == QStringLiteral("user:t1"));
        CHECK(pm.publishingTemplate(QStringLiteral("user:t2")).has_value());
        CHECK_FALSE(pm.publishingTemplate(QStringLiteral("user:t3")).has_value());
    }

    // The set: the chosen versions; the texts of its parts
    CHECK(pm.publishingSet() == PublishingSet::defaults());
    const QList<PublishingSetPart> parts = pm.publishingSetParts();
    REQUIRE(parts.size() == 5);
    CHECK(parts.at(0).elementId == second);
    CHECK(parts.at(2).elementId == blurbId);
    CHECK(parts.at(3).elementId.isEmpty());
    CHECK(pm.publishingTextOf(second) == QStringLiteral("Lejla szuka wody."));
    CHECK(pm.publishingTextOf(QStringLiteral("none")).isEmpty());
    const QString chapter = pm.mainTexts().first().first;
    CHECK(pm.publishingTextOf(chapter) == pm.paragraphsOf(chapter).join(QLatin1Char('\n')));

    PublishingSet set = pm.publishingSet();
    set.order = {QStringLiteral("synopsis"), QStringLiteral("pitch")};
    set.off = {QStringLiteral("aboutAuthor")};
    set.picks = {{QStringLiteral("pitch"), first}};
    int changes = 0;
    const auto connection = QObject::connect(&pm, &ProjectManager::publishingSetChanged,
                                             [&changes]() { ++changes; });
    REQUIRE(pm.setPublishingSet(set));
    REQUIRE(pm.setPublishingSet(set));
    CHECK(changes == 1);
    QObject::disconnect(connection);
    CHECK(pm.publishingSetParts().at(1) ==
          PublishingSetPart{QStringLiteral("pitch"), first, true, true});

    // A .klh file that cannot be written leaves all as it was
    const BookProject unchanged = *pm.project();
    REQUIRE(QFile::remove(manifest));
    REQUIRE(QDir().mkdir(manifest));
    CHECK_FALSE(pm.setPublishingState(first, PublishingTexts::CHOSEN, &drafts));
    CHECK(drafts.isEmpty());
    CHECK_FALSE(pm.setPublishingSet(PublishingSet{}));
    CHECK(*pm.project() == unchanged);
    REQUIRE(QDir().rmdir(manifest));
    REQUIRE(pm.saveManifest());

    // The .klh file has them
    REQUIRE(pm.closeProject(false));
    CHECK_FALSE(pm.setPublishingSet(set));
    CHECK(pm.publishingSetParts().isEmpty());
    CHECK(pm.publishingTemplates().isEmpty());
    CHECK(pm.mainTexts().isEmpty());
    QStringList problems;
    const std::optional<BookProject> saved = BookProject::load(manifest, problems);
    REQUIRE(saved.has_value());
    CHECK(saved->findElement(second)->publishing.state == PublishingTexts::CHOSEN);
    CHECK(saved->findElement(first)->publishing.state.isEmpty());
    CHECK(saved->findElement(synopsis)->publishing.templateId ==
          QStringLiteral("kalahari.novel:each_chapter"));
    CHECK(saved->books.first().publishingSet == set);
}
