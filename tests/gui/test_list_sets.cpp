// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_list_sets.cpp
/// @brief The sets of list formats: the built-in ones, the writer's in the book and in the
///        program, the defaults of F12 and Shift+F12, export and import, Format > List Sets

#include <catch2/catch_test_macros.hpp>

#include "kalahari_tests/gui/document_window.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/text_lists.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/list_sets.h"
#include "kalahari/gui/list_sets_menu.h"
#include "kalahari/gui/widgets/list_set_tile.h"
#include "kalahari_tests/test_support/window_parts.h"

#include <QAction>
#include <QFile>
#include <QSet>
#include <QTemporaryDir>

#include <nlohmann/json.hpp>

#include <cstdio>
#include <string>

using namespace kalahari;
using editor::ListDefinition;
using editor::ListKind;
using gui::ListSet;
using gui::ListSetGroup;
using gui::ListSetPlace;
using gui::ListSetsMenu;
using test::OpenNovel;
namespace list_sets = gui::list_sets;

namespace {

/// The settings of the sets as they were, given back after a test
struct KeptSettings {
    nlohmann::json sets;
    std::string numbered;
    std::string bulleted;

    KeptSettings() {
        auto& settings = core::SettingsManager::getInstance();
        sets = settings.get<nlohmann::json>(list_sets::PROGRAM_SETS_KEY, nlohmann::json::array());
        numbered = settings.get<std::string>(list_sets::NUMBERED_DEFAULT_KEY,
                                             list_sets::NUMBERED_ID);
        bulleted = settings.get<std::string>(list_sets::BULLETED_DEFAULT_KEY,
                                             list_sets::BULLETED_ID);
        list_sets::setProgram({});
        list_sets::setDefault(ListKind::Numbered, QString::fromLatin1(list_sets::NUMBERED_ID));
        list_sets::setDefault(ListKind::Bulleted, QString::fromLatin1(list_sets::BULLETED_ID));
    }
    ~KeptSettings() {
        try {
            auto& settings = core::SettingsManager::getInstance();
            settings.set<nlohmann::json>(list_sets::PROGRAM_SETS_KEY, sets);
            settings.set<std::string>(list_sets::NUMBERED_DEFAULT_KEY, numbered);
            settings.set<std::string>(list_sets::BULLETED_DEFAULT_KEY, bulleted);
        } catch (...) {
            // A destructor may not throw: the later tests then find the sets of this one
            static_cast<void>(std::fputs("KeptSettings: the list sets of the test stay\n", stderr));
        }
    }
    KeptSettings(const KeptSettings&) = delete;
    KeptSettings& operator=(const KeptSettings&) = delete;
};

/// A set of the writer
ListSet ownSet(const QString& name, ListDefinition definition, ListSetPlace place) {
    ListSet set;
    set.id = list_sets::newId();
    set.name = name;
    set.definition = std::move(definition);
    set.place = place;
    return set;
}

/// A numbered list with "Step " before the numbers
ListDefinition steps() {
    ListDefinition definition = ListDefinition::numbered();
    definition.levels[0].prefix = QStringLiteral("Step ");
    definition.levels[0].suffix = QStringLiteral(":");
    return definition;
}

/// The ids of sets, parted by " "
std::string idsOf(const QList<ListSet>& sets) {
    QStringList ids;
    for (const ListSet& set : sets) {
        ids << set.id;
    }
    return ids.join(QLatin1Char(' ')).toStdString();
}

}  // namespace

TEST_CASE("List sets: the built-in ones, in their groups", "[gui][lists][sets]") {
    const QList<ListSet> sets = list_sets::builtIn();
    CHECK(idsOf(sets) ==
          "numbered numbered-parentheses numbered-brackets multilevel outline paragraphs "
          "bulleted dashes checks");
    QSet<QString> names;
    for (const ListSet& set : sets) {
        INFO(set.id.toStdString());
        CHECK(set.place == ListSetPlace::BuiltIn);
        CHECK(set.name.isEmpty());
        CHECK(set.kind() == (set.group == ListSetGroup::Bulleted ? ListKind::Bulleted
                                                                  : ListKind::Numbered));
        names.insert(set.displayName());
    }
    CHECK(names.size() == sets.size());  // each one tells itself apart
    // The defaults are those of a new list
    CHECK(sets.first().definition == ListDefinition::numbered());
    CHECK(sets.first().displayName() == QStringLiteral("1. – a) – i."));
    CHECK(list_sets::find(QStringLiteral("bulleted"), nullptr)->definition ==
          ListDefinition::bulleted());
    CHECK(list_sets::find(QStringLiteral("multilevel"), nullptr)->displayName() ==
          QStringLiteral("1. – 1.1. – 1.1.1."));
    CHECK_FALSE(list_sets::find(QStringLiteral("none"), nullptr));
    CHECK(list_sets::groupTitle(sets.at(3)) == QStringLiteral("Multilevel"));

    // A list changed by hand has no set
    CHECK(list_sets::matching(ListDefinition::numbered(), sets)->id == QStringLiteral("numbered"));
    CHECK_FALSE(list_sets::matching(steps(), sets));
}

TEST_CASE("List sets: the writer's in the program and in the book", "[gui][lists][sets]") {
    const KeptSettings kept;
    const ListSet program = ownSet(QStringLiteral("Steps"), steps(), ListSetPlace::Program);
    list_sets::setProgram({program});
    REQUIRE(list_sets::program().size() == 1);
    CHECK(list_sets::program().first() == program);
    CHECK(list_sets::program().first().displayName() == QStringLiteral("Steps"));
    CHECK(list_sets::groupTitle(program) == QStringLiteral("In the Program"));
    CHECK(list_sets::newId() != list_sets::newId());

    // Without a book: none of the book's
    core::ProjectManager& pm = kalahari::test::book();
    CHECK_FALSE(list_sets::setOfBook(pm, {program}));
    {
        const OpenNovel book(QStringLiteral("Lists"));
        CHECK(list_sets::ofBook(pm).isEmpty());
        ListDefinition dashes = ListDefinition::bulleted();
        dashes.levels[0].bullet = QStringLiteral("–");
        const ListSet own = ownSet(QStringLiteral("Dashes"), dashes, ListSetPlace::Book);
        REQUIRE(list_sets::setOfBook(pm, {own}));
        REQUIRE(list_sets::ofBook(pm).size() == 1);
        CHECK(list_sets::ofBook(pm).first() == own);
        CHECK(list_sets::groupTitle(own) == QStringLiteral("In This Book"));
        // All: the built-in ones, the book's, the program's
        const QList<ListSet> sets = list_sets::all(&pm);
        REQUIRE(sets.size() == list_sets::builtIn().size() + 2);
        CHECK(sets.at(sets.size() - 2).id == own.id);
        CHECK(sets.last().id == program.id);
        CHECK(list_sets::find(own.id, &pm)->name == QStringLiteral("Dashes"));
        CHECK_FALSE(list_sets::find(own.id, nullptr));

        // Kept in the book's file
        REQUIRE(pm.saveManifest());
        const QString path = pm.getManifestPath();
        pm.closeProject(false);
        REQUIRE(pm.openProject(path));
        REQUIRE(list_sets::ofBook(pm).size() == 1);
        CHECK(list_sets::ofBook(pm).first().definition == dashes);
    }
    CHECK(list_sets::all(&pm).size() == list_sets::builtIn().size() + 1);
}

TEST_CASE("List sets: the defaults of F12 and Shift+F12", "[gui][lists][sets]") {
    const KeptSettings kept;
    CHECK(list_sets::defaultId(ListKind::Numbered) == QStringLiteral("numbered"));
    CHECK(list_sets::defaultId(ListKind::Bulleted) == QStringLiteral("bulleted"));
    CHECK(list_sets::defaultDefinition(ListKind::Numbered, nullptr) == ListDefinition::numbered());

    const ListSet program = ownSet(QStringLiteral("Steps"), steps(), ListSetPlace::Program);
    list_sets::setProgram({program});
    list_sets::setDefault(ListKind::Numbered, program.id);
    CHECK(list_sets::defaultDefinition(ListKind::Numbered, nullptr) == steps());
    list_sets::setDefault(ListKind::Bulleted, QStringLiteral("dashes"));
    CHECK(list_sets::defaultDefinition(ListKind::Bulleted, nullptr) ==
          list_sets::find(QStringLiteral("dashes"), nullptr)->definition);

    // A set of the other kind, or one gone: the built-in default
    list_sets::setDefault(ListKind::Bulleted, program.id);
    CHECK(list_sets::defaultDefinition(ListKind::Bulleted, nullptr) == ListDefinition::bulleted());
    list_sets::setProgram({});
    CHECK(list_sets::defaultDefinition(ListKind::Numbered, nullptr) == ListDefinition::numbered());
}

TEST_CASE("List sets: exported to a file and imported", "[gui][lists][sets]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QList<ListSet> sets{ownSet(QStringLiteral("Steps"), steps(), ListSetPlace::Program),
                              ownSet(QStringLiteral("Plain"), ListDefinition::numbered(),
                                     ListSetPlace::Book)};
    // As JSON: the place is given by the reader
    const QList<ListSet> read = list_sets::fromJson(list_sets::toJson(sets), ListSetPlace::Book);
    REQUIRE(read.size() == 2);
    CHECK(read.at(0).name == QStringLiteral("Steps"));
    CHECK(read.at(0).definition == steps());
    CHECK(read.at(0).place == ListSetPlace::Book);

    const QString path = dir.filePath(QStringLiteral("sets.json"));
    REQUIRE(list_sets::exportTo(path, sets));
    const std::optional<QList<ListSet>> imported = list_sets::importFrom(path);
    REQUIRE(imported);
    REQUIRE(imported->size() == 2);
    CHECK(imported->at(1).name == QStringLiteral("Plain"));
    CHECK(imported->at(1).place == ListSetPlace::Program);

    // A file that is not one, and one that is not there
    QFile other(dir.filePath(QStringLiteral("other.json")));
    REQUIRE(other.open(QIODevice::WriteOnly));
    other.write(R"({"styles": []})");
    other.close();
    CHECK_FALSE(list_sets::importFrom(other.fileName()));
    CHECK_FALSE(list_sets::importFrom(dir.filePath(QStringLiteral("none.json"))));
    CHECK_FALSE(list_sets::exportTo(dir.filePath(QStringLiteral("no/such/folder/sets.json")), sets));
}

TEST_CASE("Format > List Sets: the sets in their groups, one chosen for the list",
          "[gui][lists][sets][menu]") {
    const KeptSettings kept;
    gui::registerAllCommands(kalahari::test::commands(), gui::CommandCallbacks{});
    const ListSet program = ownSet(QStringLiteral("Steps"), steps(), ListSetPlace::Program);
    list_sets::setProgram({program});
    editor::BookEditor text;
    text.fromKml(QStringLiteral("<kml><ol><li>One</li><li>Two</li></ol><p>After</p></kml>"));
    text.setCursorPosition(editor::CursorPosition{.paragraph = 0, .offset = 0});
    editor::BookEditor* inFront = &text;
    ListSetsMenu menu(ListSetsMenu::Kind::All, kalahari::test::commands(),
                      [&inFront]() { return inFront; }, []() { return nullptr; }, nullptr);
    menu.rebuild();
    REQUIRE(menu.setActions().size() == list_sets::builtIn().size() + 1);
    CHECK(menu.checkedSet() == QStringLiteral("numbered"));
    // The default of its kind says so
    CHECK(menu.setActions().first()->text().contains(QStringLiteral("default")));

    // A set chosen: the list takes it
    QStringList chosen;
    QObject::connect(&menu, &ListSetsMenu::setChosen,
                     [&chosen](const QString& id) { chosen << id; });
    REQUIRE(menu.chooseSet(program.id));
    CHECK(chosen == QStringList{program.id});
    CHECK(editor::listMarkers(*text.textDocument()) ==
          std::vector<QString>{QStringLiteral("Step 1:"), QStringLiteral("Step 2:"), QString()});
    menu.rebuild();
    CHECK(menu.checkedSet() == program.id);
    CHECK_FALSE(menu.chooseSet(QStringLiteral("none")));

    // A text that cannot be changed, and none: greyed out
    text.setReadOnly(true);
    menu.rebuild();
    CHECK_FALSE(menu.setActions().first()->isEnabled());
    CHECK_FALSE(menu.chooseSet(QStringLiteral("numbered")));
    inFront = nullptr;
    menu.rebuild();
    CHECK(menu.checkedSet().isEmpty());
    CHECK_FALSE(menu.chooseSet(QStringLiteral("numbered")));
    kalahari::test::commands().clear();
}

TEST_CASE("Numbering and Bullets: the arrow of each gives the tiles of its sets",
          "[gui][lists][sets][menu]") {
    const KeptSettings kept;
    gui::registerAllCommands(kalahari::test::commands(), gui::CommandCallbacks{});
    editor::BookEditor text;
    text.fromKml(QStringLiteral("<kml><ul><li>One</li></ul></kml>"));
    text.setCursorPosition(editor::CursorPosition{.paragraph = 0, .offset = 0});
    ListSetsMenu numbered(ListSetsMenu::Kind::Numbered, kalahari::test::commands(),
                          [&text]() { return &text; }, []() { return nullptr; }, nullptr);
    ListSetsMenu bulleted(ListSetsMenu::Kind::Bulleted, kalahari::test::commands(),
                          [&text]() { return &text; }, []() { return nullptr; }, nullptr);
    numbered.rebuild();
    bulleted.rebuild();
    CHECK(numbered.tiles().size() == 6);
    CHECK(bulleted.tiles().size() == 3);
    CHECK(numbered.checkedSet().isEmpty());
    CHECK(bulleted.checkedSet() == QStringLiteral("bulleted"));
    // Bullets and Numbering... under the tiles
    bool dialog = false;
    for (const QAction* action : numbered.actions()) {
        dialog = dialog || action == kalahari::test::commands().getAction(
                                         QStringLiteral("format.bulletsNumbering"));
    }
    CHECK(dialog);

    // A tile clicked gives its set to the list
    numbered.tiles().at(3)->click();
    CHECK(editor::listMarkers(*text.textDocument()) == std::vector<QString>{QStringLiteral("1.")});
    CHECK(text.currentListItem()->definition ==
          list_sets::find(QStringLiteral("multilevel"), nullptr)->definition);
    kalahari::test::commands().clear();
}
