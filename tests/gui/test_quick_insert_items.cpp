// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_quick_insert_items.cpp
/// @brief Quick insert in the program: the cards of the Workshop, the fragments and the
///        characters of the settings, the page Settings > Editor > Quick Insert and its window

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_dictionary.h>
#include <kalahari/core/book_project.h>
#include <kalahari/core/name_declension.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/editor/quick_insert.h>
#include <kalahari/gui/dialogs/quick_insert_item_dialog.h>
#include <kalahari/gui/quick_insert_items.h>
#include <kalahari/gui/settings/settings_pages.h>

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTreeWidget>

#include <nlohmann/json.hpp>

#include <array>
#include <cstdio>
#include <string>
#include <vector>
#include "kalahari_tests/test_support/window_parts.h"

using namespace kalahari;
using editor::QuickInsertItem;
using editor::QuickInsertSource;
namespace keys = gui::quick_insert_keys;

namespace {

using json = nlohmann::json;

/// The settings of quick insert as the program has them at first, given back after the test
class QuickInsertSettings {
public:
    QuickInsertSettings() {
        auto& settings = core::SettingsManager::getInstance();
        for (const char* key : KEYS) {
            m_kept.push_back(settings.get<json>(key));
        }
        settings.set<bool>(keys::ENABLED, true);
        settings.set<json>(keys::AT_SOURCES, json::array({"cards"}));
        settings.set<json>(keys::HASH_SOURCES, json::array({"fragments", "characters"}));
        settings.set<json>(keys::ITEMS, nullptr);
    }
    ~QuickInsertSettings() {
        try {
            auto& settings = core::SettingsManager::getInstance();
            for (std::size_t i = 0; i < m_kept.size(); ++i) {
                settings.set<json>(KEYS.at(i), m_kept.at(i));
            }
        } catch (...) {
            std::fputs("The settings of quick insert could not be given back\n", stderr);
        }
    }
    QuickInsertSettings(const QuickInsertSettings&) = delete;
    QuickInsertSettings& operator=(const QuickInsertSettings&) = delete;

private:
    static constexpr std::array<const char*, 4> KEYS = {keys::ENABLED, keys::AT_SOURCES,
                                                        keys::HASH_SOURCES, keys::ITEMS};
    std::vector<json> m_kept;
};

QStringList namesOf(const QList<QuickInsertItem>& items) {
    QStringList names;
    for (const QuickInsertItem& item : items) {
        names << item.name;
    }
    return names;
}

/// The forms of a list as "form: cases"
QStringList formsOf(const QList<editor::QuickInsertForm>& forms) {
    QStringList shown;
    for (const editor::QuickInsertForm& form : forms) {
        shown << form.text + QStringLiteral(": ") + form.cases;
    }
    return shown;
}

core::ProjectElement element(const QString& id, const char* kind, const QString& title) {
    core::ProjectElement made;
    made.id = id;
    made.kind = *core::KindReference::parse(QString::fromLatin1(kind));
    made.title = title;
    return made;
}

}  // namespace

TEST_CASE("Quick insert items: the characters at first, the sources of @ and #",
          "[gui][quickinsert]") {
    QuickInsertSettings settings;
    auto& manager = core::SettingsManager::getInstance();

    const QList<QuickInsertItem> characters = gui::quickInsertFragments();
    REQUIRE(characters.size() == 12);
    CHECK(characters.first().name == QStringLiteral("en dash"));
    CHECK(characters.first().text == QStringLiteral("–"));
    CHECK(characters.first().source == QuickInsertSource::Character);
    CHECK(characters.first().group == QStringLiteral("Typographic Characters"));
    // The quotation marks of the language of the program
    CHECK(characters.at(3).text == QStringLiteral("“”"));

    // @ for the cards, # for the fragments and the characters
    CHECK(gui::quickInsertTriggers() == QStringLiteral("@#"));
    CHECK(gui::quickInsertItems(kalahari::test::book(), QLatin1Char('#')).size() == 12);
    REQUIRE(test::book().project() == nullptr);
    CHECK(
        gui::quickInsertItems(kalahari::test::book(), QLatin1Char('@')).isEmpty());  // no book open
    manager.set<json>(keys::HASH_SOURCES, json::array());
    CHECK(gui::quickInsertTriggers() == QStringLiteral("@"));
    CHECK(gui::quickInsertItems(kalahari::test::book(), QLatin1Char('#')).isEmpty());
    // The command lists all of them
    CHECK(gui::quickInsertItems(kalahari::test::book(), QChar()).size() == 12);
    manager.set<bool>(keys::ENABLED, false);
    CHECK(gui::quickInsertTriggers().isEmpty());

    // A fragment of the writer: the fragments before the characters
    manager.set<bool>(keys::ENABLED, true);
    manager.set<json>(keys::HASH_SOURCES, json::array({"fragments", "characters"}));
    QList<QuickInsertItem> items = characters;
    items.append(gui::fragmentItem(QuickInsertSource::Fragment, QStringLiteral("scene"),
                                   QStringLiteral("* * *")));
    gui::setQuickInsertFragments(items);
    CHECK(manager.get<json>(keys::ITEMS).is_array());
    const QList<QuickInsertItem> found =
        gui::quickInsertItems(kalahari::test::book(), QLatin1Char('#'));
    REQUIRE(found.size() == 13);
    CHECK(found.first().name == QStringLiteral("scene"));
    CHECK(found.first().group == QStringLiteral("Text Fragments"));
    CHECK(found.at(1).name == QStringLiteral("en dash"));

    // The characters as at first are kept as none: they follow the language of the program
    gui::setQuickInsertFragments(characters);
    CHECK(manager.get<json>(keys::ITEMS).is_null());

    // Entries without a name or a text are left out
    const json stored = {{{"name", "a"}, {"text", "b"}, {"kind", "fragment"}},
                         {{"name", ""}, {"text", "c"}},
                         {{"name", "d"}},
                         {{"name", "e"}, {"text", "f"}, {"kind", "character"}}};
    const QList<QuickInsertItem> read = gui::quickInsertFragmentsOf(stored);
    REQUIRE(read.size() == 2);
    CHECK(read.at(0).source == QuickInsertSource::Fragment);
    CHECK(read.at(1).source == QuickInsertSource::Character);
}

TEST_CASE("Quick insert items: the names of the cards of the Workshop, a group for each kind",
          "[gui][quickinsert]") {
    QuickInsertSettings settings;
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    auto& pm = test::book();
    pm.loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(pm.createProject(dir.path(), QStringLiteral("Cards"), QStringLiteral("Author"),
                             QStringLiteral("en"), false, QStringLiteral("kalahari.novel")));
    REQUIRE(pm.project() != nullptr);

    // The program makes no cards yet: the book has them, as a book of a later version can
    QList<core::ProjectElement>& workshop =
        pm.project()->elementsIn(core::BookPlace::Workshop);
    workshop.append(element(QStringLiteral("c1"), "kalahari.base:location",
                            QStringLiteral("Żoliborz")));
    workshop.append(element(QStringLiteral("c2"), "kalahari.base:character",
                            QStringLiteral("Anna Wolska")));
    workshop.append(element(QStringLiteral("c3"), "kalahari.base:work_note",
                            QStringLiteral("Ideas")));  // not a card
    workshop.append(element(QStringLiteral("c4"), "kalahari.base:character",
                            QStringLiteral(" Jan Anioł ")));
    workshop.append(element(QStringLiteral("c5"), "kalahari.base:character", QStringLiteral(" ")));

    const QList<QuickInsertItem> cards = gui::cardItems(*pm.project(), pm.bookTypes());
    // The kinds in the order of their first card, the cards of a kind in the Workshop's order
    CHECK(namesOf(cards) == QStringList{QStringLiteral("Żoliborz"), QStringLiteral("Anna Wolska"),
                                        QStringLiteral("Jan Anioł")});
    REQUIRE(cards.size() == 3);
    CHECK(cards.at(0).group == QStringLiteral("Locations"));
    CHECK(cards.at(1).group == QStringLiteral("Characters"));
    CHECK(cards.at(1).detail == QStringLiteral("character"));
    CHECK(cards.at(2).text == QStringLiteral("Jan Anioł"));
    CHECK(cards.at(0).source == QuickInsertSource::Card);

    CHECK(namesOf(gui::quickInsertItems(kalahari::test::book(), QLatin1Char('@'))) ==
          namesOf(cards));
    pm.closeProject(false);
}

TEST_CASE("Settings > Editor > Quick Insert: the sources of @ and #, the fragments",
          "[gui][quickinsert][settings]") {
    QuickInsertSettings settings;
    auto& manager = core::SettingsManager::getInstance();
    gui::EditorQuickInsertPage page(kalahari::test::commands());
    page.load();
    const auto box = [&page](const char* name) {
        auto* found = page.findChild<QCheckBox*>(QString::fromLatin1(name));
        REQUIRE(found != nullptr);
        return found;
    };
    CHECK(box("quickInsertEnabled")->isChecked());
    CHECK(box("quickInsert_at_cards")->isChecked());
    CHECK_FALSE(box("quickInsert_at_fragments")->isChecked());
    CHECK_FALSE(box("quickInsert_at_characters")->isChecked());
    CHECK_FALSE(box("quickInsert_hash_cards")->isChecked());
    CHECK(box("quickInsert_hash_fragments")->isChecked());
    CHECK(box("quickInsert_hash_characters")->isChecked());
    CHECK(box("quickInsert_hash_characters")->accessibleName() ==
          QStringLiteral("Typographic characters after #"));
    CHECK_FALSE(page.isChanged());

    // Off, the sources wait
    auto* sources = page.findChild<QWidget*>(QStringLiteral("quickInsertSources"));
    REQUIRE(sources != nullptr);
    box("quickInsertEnabled")->setChecked(false);
    CHECK_FALSE(sources->isEnabled());
    box("quickInsertEnabled")->setChecked(true);
    CHECK(sources->isEnabled());

    box("quickInsert_at_fragments")->setChecked(true);
    CHECK(page.isChanged());
    page.apply();
    CHECK(manager.get<json>(keys::AT_SOURCES) == json::array({"cards", "fragments"}));

    // The characters at first; one removed comes back at the end of the list
    auto* list = page.findChild<QTreeWidget*>(QStringLiteral("quickInsertItems"));
    auto* remove = page.findChild<QPushButton*>(QStringLiteral("quickInsertRemove"));
    auto* restore = page.findChild<QPushButton*>(QStringLiteral("quickInsertRestore"));
    REQUIRE(list != nullptr);
    REQUIRE(remove != nullptr);
    REQUIRE(restore != nullptr);
    REQUIRE(list->topLevelItemCount() == 12);
    CHECK(list->topLevelItem(0)->text(0) == QStringLiteral("en dash"));
    CHECK(list->topLevelItem(5)->text(1) == QStringLiteral("U+00A0"));  // no-break space
    CHECK_FALSE(restore->isEnabled());
    CHECK_FALSE(remove->isEnabled());

    list->setCurrentItem(list->topLevelItem(0));
    CHECK(remove->isEnabled());
    remove->click();
    CHECK(list->topLevelItemCount() == 11);
    CHECK(list->currentItem()->text(0) == QStringLiteral("em dash"));
    CHECK(restore->isEnabled());
    CHECK(page.isChanged());
    restore->click();
    CHECK(list->topLevelItemCount() == 12);
    CHECK(list->topLevelItem(11)->text(0) == QStringLiteral("en dash"));
    CHECK_FALSE(restore->isEnabled());

    page.apply();
    const json stored = manager.get<json>(keys::ITEMS);
    REQUIRE(stored.is_array());
    CHECK(stored.size() == 12);
    CHECK(stored.at(0).at("name") == "em dash");
    CHECK(stored.at(11).at("kind") == "character");
    CHECK_FALSE(page.isChanged());
}

TEST_CASE("Quick insert: the window of a fragment needs a name and a text",
          "[gui][quickinsert]") {
    QuickInsertItem added;
    gui::dialogs::QuickInsertItemDialog dialog(added);
    auto* name = dialog.findChild<QLineEdit*>(QStringLiteral("quickInsertItemName"));
    auto* text = dialog.findChild<QPlainTextEdit*>(QStringLiteral("quickInsertItemText"));
    auto* kind = dialog.findChild<QComboBox*>(QStringLiteral("quickInsertItemKind"));
    REQUIRE(name != nullptr);
    REQUIRE(text != nullptr);
    REQUIRE(kind != nullptr);
    CHECK(dialog.acceptButton()->text() == QStringLiteral("Add"));
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    CHECK(text->tabChangesFocus());

    name->setText(QStringLiteral("  letter "));
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    text->setPlainText(QStringLiteral("Dear Anna,\nYours"));
    CHECK(dialog.acceptButton()->isEnabled());
    QuickInsertItem item = dialog.item();
    CHECK(item.name == QStringLiteral("letter"));
    CHECK(item.text == QStringLiteral("Dear Anna,\nYours"));
    CHECK(item.source == QuickInsertSource::Fragment);
    CHECK(item.group == QStringLiteral("Text Fragments"));

    kind->setCurrentIndex(1);
    CHECK(dialog.item().source == QuickInsertSource::Character);

    // An item to change: its values, and Save
    gui::dialogs::QuickInsertItemDialog edited(
        gui::fragmentItem(QuickInsertSource::Character, QStringLiteral("arrow"),
                          QStringLiteral("→")));
    CHECK(edited.acceptButton()->text() == QStringLiteral("Save"));
    CHECK(edited.acceptButton()->isEnabled());
    CHECK(edited.item().source == QuickInsertSource::Character);
    CHECK(edited.item().text == QStringLiteral("→"));
}

TEST_CASE("Quick insert items: the forms of a name, each once, with their cases",
          "[gui][quickinsert][dictionary]") {
    const core::NameDeclension polish(QStringLiteral("pl"));
    const core::DictionaryName anna =
        core::makeName(QStringLiteral("Anna Wolska"), core::NameKind::Character, polish);
    REQUIRE_FALSE(anna.variants.isEmpty());
    const QStringList forms = formsOf(gui::quickInsertForms(anna.variants.front(), false));
    CHECK(forms == QStringList{QStringLiteral("Anna Wolska: nominative"),
                               QStringLiteral("Anny Wolskiej: genitive"),
                               QStringLiteral("Annie Wolskiej: dative, locative"),
                               QStringLiteral("Annę Wolską: accusative"),
                               QStringLiteral("Anną Wolską: instrumental"),
                               QStringLiteral("Anno Wolska: vocative")});
    // With the plural: its forms after those of the singular
    const QStringList withPlural = formsOf(gui::quickInsertForms(anna.variants.front(), true));
    REQUIRE(withPlural.size() > forms.size());
    CHECK(withPlural.mid(0, forms.size()) == forms);
    CHECK(withPlural.at(forms.size()).contains(QStringLiteral(": nominative plural")));

    // A name that does not change has nothing to choose
    const core::DictionaryName ruth = core::makeName(QStringLiteral("Ruth"),
                                                     core::NameKind::Character, polish);
    REQUIRE_FALSE(ruth.variants.isEmpty());
    core::NameVariant unchanged = ruth.variants.front();
    for (const core::GrammaticalCase cell : core::FormTable::allCases()) {
        unchanged.forms.cell(cell, false) = QStringList{QStringLiteral("Ruth")};
    }
    CHECK(gui::quickInsertForms(unchanged, false).isEmpty());

    // In a language without cases, the forms are named by their number
    core::NameVariant english;
    english.text = QStringLiteral("Ruth");
    english.forms.cell(core::GrammaticalCase::Nominative, false) = {QStringLiteral("Ruth")};
    english.forms.cell(core::GrammaticalCase::Nominative, true) = {QStringLiteral("Ruths")};
    CHECK(formsOf(gui::quickInsertForms(english, true)) ==
          QStringList{QStringLiteral("Ruth: singular"), QStringLiteral("Ruths: plural")});
    CHECK(gui::quickInsertForms(english, false).isEmpty());
}

TEST_CASE("Quick insert items: the names of the book dictionary with their other names",
          "[gui][quickinsert][dictionary]") {
    const core::NameDeclension polish(QStringLiteral("pl"));
    core::BookDictionary dictionary;
    dictionary.putName(
        core::makeName(QStringLiteral("Księga Popiołów"), core::NameKind::Item, polish));
    dictionary.putName(
        core::makeName(QStringLiteral("Anna Wolska"), core::NameKind::Character, polish));
    core::DictionaryName hidden =
        core::makeName(QStringLiteral("Jan Anioł"), core::NameKind::Character, polish);
    hidden.quickInsert = false;
    dictionary.putName(hidden);
    dictionary.putName(core::makeName(QStringLiteral("Gdańsk"), core::NameKind::Place, polish));

    const QList<QuickInsertItem> items = gui::dictionaryItems(dictionary);
    // The kinds in the order of their first name; Jan Anioł is not offered
    CHECK(namesOf(items) == QStringList{QStringLiteral("Księga Popiołów"),
                                        QStringLiteral("Anna Wolska"), QStringLiteral("Anna"),
                                        QStringLiteral("Wolska"), QStringLiteral("Gdańsk")});
    REQUIRE(items.size() == 5);
    CHECK(items.at(0).group == QStringLiteral("Items"));
    CHECK(items.at(0).detail == QStringLiteral("item"));
    CHECK(items.at(1).source == QuickInsertSource::Name);
    CHECK(items.at(1).group == QStringLiteral("Characters"));
    CHECK(items.at(1).detail == QStringLiteral("character"));
    CHECK(items.at(1).owner.isEmpty());
    CHECK(items.at(1).forms.size() == 6);
    CHECK(items.at(2).detail == QStringLiteral("Anna Wolska"));
    CHECK(items.at(2).owner == QStringLiteral("Anna Wolska"));
    CHECK(items.at(2).forms.at(3).text == QStringLiteral("Annę"));
    CHECK(items.at(4).group == QStringLiteral("Locations"));
}

TEST_CASE("Quick insert items: @ offers the names of the book dictionary before the cards",
          "[gui][quickinsert][dictionary]") {
    const QuickInsertSettings settings;
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    auto& pm = test::book();
    core::ProjectManager::loadBookTypes(
        {QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(pm.createProject(dir.path(), QStringLiteral("Names"), QStringLiteral("Author"),
                             QStringLiteral("pl"), false, QStringLiteral("kalahari.novel")));
    REQUIRE(pm.project() != nullptr);
    QList<core::ProjectElement>& workshop =
        pm.project()->elementsIn(core::BookPlace::Workshop);
    workshop.append(element(QStringLiteral("c1"), "kalahari.base:character",
                            QStringLiteral("Jan Anioł")));
    workshop.append(element(QStringLiteral("c2"), "kalahari.base:character",
                            QStringLiteral("Anna Wolska")));
    REQUIRE(pm.changeBookDictionary([](core::BookDictionary& dictionary) {
        dictionary.putName(core::makeName(QStringLiteral("Anna Wolska"),
                                          core::NameKind::Character,
                                          core::NameDeclension(QStringLiteral("pl"))));
    }));

    // The card of Anna Wolska is her name in the dictionary, with its forms; the others stay
    const QList<QuickInsertItem> items = gui::quickInsertItems(pm, QLatin1Char('@'));
    CHECK(namesOf(items) == QStringList{QStringLiteral("Anna Wolska"), QStringLiteral("Anna"),
                                        QStringLiteral("Wolska"), QStringLiteral("Jan Anioł")});
    REQUIRE(items.size() == 4);
    CHECK(items.at(0).source == QuickInsertSource::Name);
    CHECK(items.at(3).source == QuickInsertSource::Card);
    CHECK(items.at(3).group == items.at(0).group);
    CHECK(items.at(0).color.isEmpty());  // the cards have no colors yet

    // The names of a card with its color: by the card the dictionary says, else by the title
    pm.project()->findElement(QStringLiteral("c1"))->card.color = QStringLiteral("#2e9e6a");
    pm.project()->findElement(QStringLiteral("c2"))->card.color = QStringLiteral("#7e57c2");
    QList<QuickInsertItem> colored = gui::quickInsertItems(pm, QLatin1Char('@'));
    REQUIRE(colored.size() == 4);
    for (int i = 0; i < 3; ++i) {
        CHECK(colored.at(i).color == QStringLiteral("#7e57c2"));
    }
    CHECK(colored.at(3).color == QStringLiteral("#2e9e6a"));
    REQUIRE(pm.changeBookDictionary([](core::BookDictionary& dictionary) {
        core::DictionaryName own = *dictionary.findNameWithText(QStringLiteral("Anna Wolska"));
        own.card = QStringLiteral("c1");  // another card, whatever its title
        dictionary.putName(own);
    }));
    colored = gui::quickInsertItems(pm, QLatin1Char('@'));
    REQUIRE(colored.size() == 4);
    CHECK(colored.at(1).color == QStringLiteral("#2e9e6a"));

    // Without the names in the settings of @, none of them
    core::SettingsManager::getInstance().set<json>(keys::AT_SOURCES, json::array({"fragments"}));
    CHECK(namesOf(gui::quickInsertItems(pm, QLatin1Char('@'))).indexOf(QStringLiteral("Anna")) ==
          -1);
    pm.closeProject(false);
}
