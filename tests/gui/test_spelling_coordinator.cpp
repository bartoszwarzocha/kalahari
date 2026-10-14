/// @file test_spelling_coordinator.cpp
/// @brief Spelling as you type in the window: the dictionary follows the settings and the
///        book, the editors of the documents check with it (a document outside the book in
///        the default language), Tools > Check Spelling as You Type turns it on and off, and
///        Editor > Spelling in the settings chooses its language and edits the writer's own
///        words

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "../editor/fake_language_tool.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/icon_registry.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/grammar_check_service.h"
#include "kalahari/editor/spell_check_service.h"
#include "kalahari/editor/text_source_adapter.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/spelling_coordinator.h"

#include <QAction>
#include <QCheckBox>
#include <QCollator>
#include <QComboBox>
#include <QCoreApplication>
#include <QFileInfo>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>

using namespace kalahari;
using namespace kalahari::gui;
using kalahari::test::DICTIONARY_WAIT_MS;
using kalahari::test::kmlOf;
using kalahari::test::waitUntil;

namespace {

constexpr const char* ENABLED_KEY = "editor.spellCheck.enabled";
constexpr const char* LANGUAGE_KEY = "editor.spellCheck.language";

/// Gives the spelling settings and the language of the program back as they were before
/// the test
class KeptSpellingSettings {
public:
    KeptSpellingSettings()
        : m_enabled(settings().get<bool>(ENABLED_KEY, true))
        , m_language(settings().get<std::string>(LANGUAGE_KEY, std::string()))
        , m_program(settings().getLanguage()) {}
    ~KeptSpellingSettings() {
        try {
            settings().set<bool>(ENABLED_KEY, m_enabled);
            settings().set<std::string>(LANGUAGE_KEY, m_language);
            settings().setLanguage(m_program);
        } catch (...) {
            // Left as the test made them; the next tests set what they need
            std::fputs("The spelling settings could not be given back\n", stderr);
        }
    }
    KeptSpellingSettings(const KeptSpellingSettings&) = delete;
    KeptSpellingSettings& operator=(const KeptSpellingSettings&) = delete;

    static core::SettingsManager& settings() { return core::SettingsManager::getInstance(); }

private:
    bool m_enabled;
    std::string m_language;
    std::string m_program;
};

/// Run the event loop until the coordinator checks with @p dictionary
bool checksWith(const SpellingCoordinator& coordinator, const QString& dictionary) {
    const editor::SpellCheckService* service = coordinator.service();
    return waitUntil(
        [service, dictionary]() {
            return service->currentDictionary() == dictionary && !service->isLoading();
        },
        DICTIONARY_WAIT_MS);
}

/// The words with a spelling wave in a paragraph, in their order
QStringList wavyWords(const editor::BookEditor& bookEditor, int paragraph) {
    const QString text = bookEditor.textDocument()->findBlockByNumber(paragraph).text();
    QStringList words;
    for (const editor::TextHighlight& highlight :
         editor::QTextDocumentSource(bookEditor.textDocument()).paragraphHighlights(paragraph)) {
        if (highlight.kind == editor::HighlightKind::Spelling) {
            words.append(text.mid(highlight.start, highlight.length));
        }
    }
    return words;
}

/// Whether the first paragraph has a grammar wave
bool hasGrammarWave(const editor::BookEditor& bookEditor) {
    const auto highlights =
        editor::QTextDocumentSource(bookEditor.textDocument()).paragraphHighlights(0);
    return std::any_of(highlights.begin(), highlights.end(), [](const auto& highlight) {
        return highlight.kind == editor::HighlightKind::Grammar;
    });
}

/// Open the page with a title under a category of the settings
void openPage(SettingsDialog& dialog, const QString& category, const QString& title) {
    auto* tree = dialog.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        QTreeWidgetItem* parent = (*it)->parent();
        if ((*it)->text(0) == title && parent != nullptr && parent->text(0) == category) {
            tree->setCurrentItem(*it);
            return;
        }
    }
    FAIL("No page " << title.toStdString());
}

/// The language of the spelling on the page: the list of the dictionaries
QComboBox* languageBox(const SettingsDialog& dialog) {
    for (QComboBox* combo : dialog.findChildren<QComboBox*>()) {
        if (combo->findData(QStringLiteral("en_US")) >= 0) {
            return combo;
        }
    }
    return nullptr;
}

QListWidget* wordList(const SettingsDialog& dialog) {
    for (QListWidget* list : dialog.findChildren<QListWidget*>()) {
        if (list->accessibleName() == QStringLiteral("Your words")) {
            return list;
        }
    }
    return nullptr;
}

/// The words of the list, sorted
QStringList listed(const QListWidget& list) {
    QStringList words;
    for (int row = 0; row < list.count(); ++row) {
        words.append(list.item(row)->text());
    }
    words.sort();
    return words;
}

template <typename Button>
Button* buttonWithText(const QWidget& parent, const QString& text) {
    for (Button* button : parent.findChildren<Button*>()) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

}  // namespace

TEST_CASE("Spelling coordinator: the dictionary follows the settings", "[gui][spelling]") {
    KeptSpellingSettings kept;
    auto& settings = KeptSpellingSettings::settings();
    settings.set<bool>(ENABLED_KEY, true);
    settings.set<std::string>(LANGUAGE_KEY, "en");

    QStatusBar statusBar;
    SpellingCoordinator coordinator(nullptr, &statusBar);
    editor::SpellCheckService* service = coordinator.service();
    CHECK(service->userDictionaryFile() == SpellingCoordinator::userDictionaryFile());
    CHECK(SpellingCoordinator::userDictionaryFile().endsWith(
        QStringLiteral("/user_dictionary.txt")));
    REQUIRE(checksWith(coordinator, QStringLiteral("en_US")));
    CHECK(service->isActive());

    // A dictionary chosen in the settings
    settings.set<std::string>(LANGUAGE_KEY, "pl_PL");
    CHECK(checksWith(coordinator, QStringLiteral("pl_PL")));

    // A language without a dictionary (Hunspell has none for Japanese): nothing is checked,
    // and the status bar names the language in the language of the program
    settings.set<std::string>(LANGUAGE_KEY, "ja");
    CHECK(waitUntil([service]() { return !service->isDictionaryLoaded(); }));
    CHECK_FALSE(service->isActive());
    CHECK(statusBar.currentMessage() ==
          QStringLiteral("No spelling dictionary for Japanese: the spelling is not checked"));

    // A language the program has no name for goes by its code
    settings.set<std::string>(LANGUAGE_KEY, "xx");
    CHECK(waitUntil([&statusBar]() {
        return statusBar.currentMessage() ==
               QStringLiteral("No spelling dictionary for xx: the spelling is not checked");
    }));
    CHECK_FALSE(service->isActive());

    // Turned off and on again
    settings.set<std::string>(LANGUAGE_KEY, "en");
    REQUIRE(checksWith(coordinator, QStringLiteral("en_US")));
    settings.set<bool>(ENABLED_KEY, false);
    CHECK(waitUntil([service]() { return !service->isEnabled(); }));
    CHECK_FALSE(service->isActive());
    settings.set<bool>(ENABLED_KEY, true);
    CHECK(waitUntil([service]() { return service->isActive(); }));
}

TEST_CASE("Spelling coordinator: the book's language when the settings name none",
          "[gui][spelling]") {
    KeptSpellingSettings kept;
    auto& settings = KeptSpellingSettings::settings();
    settings.set<bool>(ENABLED_KEY, true);
    auto& projects = core::ProjectManager::getInstance();
    REQUIRE(projects.closeProject(false));

    settings.set<std::string>(LANGUAGE_KEY, " pl ");
    CHECK(SpellingCoordinator::wantedLanguage() == QStringLiteral("pl"));

    // No book: the language of the program
    settings.set<std::string>(LANGUAGE_KEY, "");
    CHECK(SpellingCoordinator::wantedLanguage() ==
          QString::fromStdString(settings.getLanguage()));

    SpellingCoordinator coordinator(nullptr, nullptr);
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    REQUIRE(projects.createProject(folder.path(), QStringLiteral("Spelling Test"),
                                   QStringLiteral("Author"), QStringLiteral("pl"), true));
    CHECK(SpellingCoordinator::wantedLanguage() == QStringLiteral("pl"));
    CHECK(checksWith(coordinator, QStringLiteral("pl_PL")));

    // The book's language changed in the Properties panel
    projects.book()->language = QStringLiteral("en");
    coordinator.updateDictionary();
    CHECK(checksWith(coordinator, QStringLiteral("en_US")));

    // A language set in the settings goes before the book's
    settings.set<std::string>(LANGUAGE_KEY, "pl_PL");
    CHECK(checksWith(coordinator, QStringLiteral("pl_PL")));
    settings.set<std::string>(LANGUAGE_KEY, "");
    CHECK(checksWith(coordinator, QStringLiteral("en_US")));

    REQUIRE(projects.closeProject(false));
}

TEST_CASE("Spelling coordinator: the editors of the documents check with its dictionary",
          "[gui][spelling]") {
    KeptSpellingSettings kept;
    auto& settings = KeptSpellingSettings::settings();
    settings.set<bool>(ENABLED_KEY, true);
    settings.set<std::string>(LANGUAGE_KEY, "en_US");

    QTabWidget tabs;
    auto* first = new EditorPanel();
    first->setContent(kmlOf({QStringLiteral("One mistakke.")}));
    tabs.addTab(first, QStringLiteral("One"));
    SpellingCoordinator coordinator(&tabs, nullptr);
    CHECK(first->getBookEditor()->spellCheckService() == coordinator.service());

    // A document opened later, in a tab of its own: once the event loop runs, as a chapter
    // is tagged after its tab is shown
    auto* second = new EditorPanel();
    second->setContent(kmlOf({QStringLiteral("Two mistakkes.")}));
    tabs.setCurrentIndex(tabs.addTab(second, QStringLiteral("Two")));
    const editor::BookEditor* added = second->getBookEditor();
    CHECK(waitUntil([added, &coordinator]() {
        return added->spellCheckService() == coordinator.service();
    }));

    // The editor shown gets its waves
    tabs.resize(700, 500);
    tabs.show();
    REQUIRE(checksWith(coordinator, QStringLiteral("en_US")));
    const editor::BookEditor* shown = second->getBookEditor();
    CHECK(waitUntil(
        [shown]() { return wavyWords(*shown, 0) == QStringList{QStringLiteral("mistakkes")}; },
        DICTIONARY_WAIT_MS));
}

TEST_CASE("Spelling coordinator: a document outside the book is checked in the default "
          "language",
          "[gui][spelling]") {
    KeptSpellingSettings kept;
    auto& settings = KeptSpellingSettings::settings();
    settings.set<bool>(ENABLED_KEY, true);
    settings.set<std::string>(LANGUAGE_KEY, "");
    settings.setLanguage("en");
    auto& projects = core::ProjectManager::getInstance();
    REQUIRE(projects.closeProject(false));

    registerAllCommands(CommandCallbacks{});
    auto& registry = CommandRegistry::getInstance();
    QStatusBar statusBar;
    QTabWidget tabs;
    tabs.resize(700, 500);
    tabs.show();
    {
        SpellingCoordinator coordinator(&tabs, &statusBar);
        coordinator.connectCommands();
        const editor::SpellCheckService* book = coordinator.service();
        CHECK(SpellingCoordinator::defaultLanguage() == QStringLiteral("en"));

        // A book in Polish, with a chapter: NavigatorCoordinator tags it after its tab is shown
        QTemporaryDir folder;
        REQUIRE(folder.isValid());
        REQUIRE(projects.createProject(folder.path(), QStringLiteral("Spelling Test"),
                                       QStringLiteral("Author"), QStringLiteral("pl"), true));
        REQUIRE(checksWith(coordinator, QStringLiteral("pl_PL")));
        CHECK(SpellingCoordinator::defaultLanguage() == QStringLiteral("en"));
        auto* chapter = new EditorPanel();
        chapter->setContent(kmlOf({QStringLiteral("Ta ksi\u0105\u017cka ma b\u0142\u0105d: "
                                                  "ksi\u0105rzka.")}));
        tabs.setCurrentIndex(tabs.addTab(chapter, QStringLiteral("Chapter")));
        chapter->setProperty("elementId", QStringLiteral("chapter-1"));
        const editor::BookEditor* chapterEditor = chapter->getBookEditor();
        CHECK(waitUntil(
            [chapterEditor]() {
                return wavyWords(*chapterEditor, 0) == QStringList{QStringLiteral("ksi\u0105rzka")};
            },
            DICTIONARY_WAIT_MS));
        CHECK(chapterEditor->spellCheckService() == book);

        // A file opened from the disk: in the language of the program, with a dictionary of
        // its own
        auto* note = new EditorPanel();
        note->setContent(kmlOf({QStringLiteral("This note has one mistakke.")}));
        tabs.setCurrentIndex(tabs.addTab(note, QStringLiteral("note.txt")));
        note->setProperty("isStandaloneFile", true);
        const editor::BookEditor* noteEditor = note->getBookEditor();
        CHECK(waitUntil(
            [noteEditor]() {
                return wavyWords(*noteEditor, 0) == QStringList{QStringLiteral("mistakke")};
            },
            DICTIONARY_WAIT_MS));
        editor::SpellCheckService* other = noteEditor->spellCheckService();
        REQUIRE(other != nullptr);
        CHECK(other != book);
        CHECK(other->currentDictionary() == QStringLiteral("en_US"));
        CHECK(book->currentDictionary() == QStringLiteral("pl_PL"));
        CHECK(chapterEditor->spellCheckService() == book);

        // The writer's own words are the same in both: a word ignored in the note is right in
        // the chapter
        other->ignoreWord(QStringLiteral("ksi\u0105rzka"));
        CHECK(book->isIgnored(QStringLiteral("ksi\u0105rzka")));
        tabs.setCurrentIndex(0);
        CHECK(waitUntil([chapterEditor]() { return wavyWords(*chapterEditor, 0).isEmpty(); }));
        tabs.setCurrentIndex(1);

        // The book's language without a dictionary: its chapters are not checked, the note
        // is, and Next Spelling or Grammar Issue goes to the note's word
        projects.book()->language = QStringLiteral("ja");
        coordinator.updateDictionary();
        CHECK(waitUntil([book]() { return !book->isDictionaryLoaded(); }));
        CHECK(statusBar.currentMessage() ==
              QStringLiteral("No spelling dictionary for Japanese: the spelling is not checked"));
        CHECK(noteEditor->spellCheckService() == other);
        CHECK(wavyWords(*noteEditor, 0) == QStringList{QStringLiteral("mistakke")});
        QAction* next = registry.getAction(std::string("tools.nextMisspelling"));
        REQUIRE(next != nullptr);
        note->getBookEditor()->setCursorPosition({0, 0});
        const QStringList texts = kalahari::test::runPopupMenu([next]() { next->trigger(); });
        CHECK(noteEditor->selectedText() == QStringLiteral("mistakke"));
        CHECK(texts.contains(QStringLiteral("mistake")));

        // The book in the language of the program: one dictionary for every document
        projects.book()->language = QStringLiteral("en");
        coordinator.updateDictionary();
        CHECK(checksWith(coordinator, QStringLiteral("en_US")));
        CHECK(noteEditor->spellCheckService() == book);
        CHECK(chapterEditor->spellCheckService() == book);

        // The language of the program without a dictionary: the note is not checked, and the
        // status bar says so
        settings.setLanguage("xx");
        CHECK(waitUntil([noteEditor, other]() {
            return noteEditor->spellCheckService() == other && !other->isDictionaryLoaded();
        }));
        CHECK(statusBar.currentMessage() ==
              QStringLiteral("No spelling dictionary for xx: the spelling is not checked"));
        CHECK(waitUntil([noteEditor]() { return wavyWords(*noteEditor, 0).isEmpty(); }));
        CHECK(book->currentDictionary() == QStringLiteral("en_US"));

        // A language chosen in the settings: every document in it
        settings.set<std::string>(LANGUAGE_KEY, "pl_PL");
        CHECK(checksWith(coordinator, QStringLiteral("pl_PL")));
        CHECK(noteEditor->spellCheckService() == book);
        CHECK(chapterEditor->spellCheckService() == book);

        REQUIRE(projects.closeProject(false));
    }
    registry.clear();
}

TEST_CASE("Spelling coordinator: Check Spelling as You Type turns the checking on and off",
          "[gui][spelling][command]") {
    KeptSpellingSettings kept;
    auto& settings = KeptSpellingSettings::settings();
    settings.set<bool>(ENABLED_KEY, true);
    settings.set<std::string>(LANGUAGE_KEY, "en");

    registerAllCommands(CommandCallbacks{});
    auto& registry = CommandRegistry::getInstance();
    QStatusBar statusBar;
    {
        SpellingCoordinator coordinator(nullptr, &statusBar);
        coordinator.connectCommands();
        const editor::SpellCheckService* service = coordinator.service();
        QAction* action = registry.getAction(std::string("tools.spellcheck"));
        REQUIRE(action != nullptr);
        CHECK(action->isEnabled());
        CHECK(action->isCheckable());
        CHECK(action->isChecked());
        CHECK(action->shortcut() == QKeySequence(Qt::SHIFT | Qt::Key_F7));

        action->trigger();
        CHECK_FALSE(settings.get<bool>(ENABLED_KEY, true));
        CHECK_FALSE(action->isChecked());
        CHECK(statusBar.currentMessage() == QStringLiteral("Spelling as you type: off"));
        CHECK(waitUntil([service]() { return !service->isEnabled(); }));

        action->trigger();
        CHECK(settings.get<bool>(ENABLED_KEY, false));
        CHECK(action->isChecked());
        CHECK(statusBar.currentMessage() == QStringLiteral("Spelling as you type: on"));
        CHECK(waitUntil([service]() { return service->isEnabled(); }));

        // Turned off in the settings: the command shows it
        settings.set<bool>(ENABLED_KEY, false);
        CHECK_FALSE(action->isChecked());
    }

    // The command must not reach the coordinator that is gone
    registry.clear();
}

TEST_CASE("Spelling coordinator: Next Spelling or Grammar Issue selects the next issue and "
          "opens its menu",
          "[gui][spelling][command]") {
    KeptSpellingSettings kept;
    auto& settings = KeptSpellingSettings::settings();
    settings.set<bool>(ENABLED_KEY, true);
    settings.set<std::string>(LANGUAGE_KEY, "en_US");

    registerAllCommands(CommandCallbacks{});
    auto& registry = CommandRegistry::getInstance();
    QStatusBar statusBar;
    QTabWidget tabs;
    {
        SpellingCoordinator coordinator(&tabs, &statusBar);
        coordinator.connectCommands();
        QAction* action = registry.getAction(std::string("tools.nextMisspelling"));
        REQUIRE(action != nullptr);
        CHECK(action->shortcut() == QKeySequence(Qt::Key_F7));
        CHECK_FALSE(action->isEnabled());  // no document

        auto* panel = new EditorPanel();
        panel->setContent(kmlOf({QStringLiteral("One mistakke and anothr.")}));
        tabs.setCurrentIndex(tabs.addTab(panel, QStringLiteral("One")));
        tabs.resize(700, 500);
        tabs.show();
        REQUIRE(checksWith(coordinator, QStringLiteral("en_US")));
        CHECK(action->isEnabled());
        editor::BookEditor* bookEditor = panel->getBookEditor();
        bookEditor->setCursorPosition({0, 0});

        // The word is selected and its menu offers what to put in its place
        QStringList texts = kalahari::test::runPopupMenu([action]() { action->trigger(); });
        CHECK(bookEditor->selectedText() == QStringLiteral("mistakke"));
        CHECK(texts.contains(QStringLiteral("mistake")));
        CHECK(texts.contains(QStringLiteral("Add t&o Dictionary")));

        // The next one, and a word chosen from the menu
        texts = kalahari::test::runPopupMenu([action]() { action->trigger(); },
                                             QStringLiteral("another"));
        CHECK(texts.contains(QStringLiteral("another")));
        CHECK(bookEditor->textDocument()->findBlockByNumber(0).text() ==
              QStringLiteral("One mistakke and another."));

        SECTION("the status bar says when there is none") {
            bookEditor->fromKml(kmlOf({QStringLiteral("All good here.")}));
            texts = kalahari::test::runPopupMenu([action]() { action->trigger(); });
            CHECK(texts.isEmpty());
            CHECK(statusBar.currentMessage() == QStringLiteral("No misspelled words"));
        }

        SECTION("with the grammar: its issues too, and the status bar says when there is none") {
            kalahari::test::FakeLanguageTool server;
            server.addIssue({QStringLiteral("kat"), QStringLiteral("RULE_A"),
                             QStringLiteral("Did you mean a cat?")});
            editor::GrammarCheckService grammar;
            grammar.setServer(server.url());
            bookEditor->setGrammarCheckService(&grammar);
            bookEditor->fromKml(kmlOf({QStringLiteral("The kat is here.")}));
            REQUIRE(waitUntil([bookEditor]() { return hasGrammarWave(*bookEditor); }, 20000));
            bookEditor->setCursorPosition({0, 0});
            texts = kalahari::test::runPopupMenu([action]() { action->trigger(); });
            CHECK(bookEditor->selectedText() == QStringLiteral("kat"));
            CHECK(texts.contains(QStringLiteral("&Ignore This Rule")));

            bookEditor->fromKml(kmlOf({QStringLiteral("All good here.")}));
            REQUIRE(waitUntil([bookEditor]() { return !bookEditor->isGrammarCheckPending(); },
                              20000));
            texts = kalahari::test::runPopupMenu([action]() { action->trigger(); });
            CHECK(texts.isEmpty());
            CHECK(statusBar.currentMessage() ==
                  QStringLiteral("No misspelled words or grammar issues"));

            // The spelling off: the grammar alone
            settings.set<bool>(ENABLED_KEY, false);
            const editor::SpellCheckService* service = coordinator.service();
            REQUIRE(waitUntil([service]() { return !service->isEnabled(); }));
            texts = kalahari::test::runPopupMenu([action]() { action->trigger(); });
            CHECK(texts.isEmpty());
            CHECK(statusBar.currentMessage() == QStringLiteral("No grammar issues"));
            bookEditor->setGrammarCheckService(nullptr);
        }

        SECTION("the status bar says when the spelling is not checked") {
            settings.set<bool>(ENABLED_KEY, false);
            const editor::SpellCheckService* service = coordinator.service();
            REQUIRE(waitUntil([service]() { return !service->isEnabled(); }));
            texts = kalahari::test::runPopupMenu([action]() { action->trigger(); });
            CHECK(texts.isEmpty());
            CHECK(statusBar.currentMessage() == QStringLiteral("The spelling is not checked"));
        }

        SECTION("greyed out again without a document") {
            tabs.removeTab(0);
            delete panel;
            CHECK_FALSE(action->isEnabled());
        }
    }

    // The coordinator that is gone gave its callbacks back
    Command* command = registry.getCommand(std::string("tools.nextMisspelling"));
    REQUIRE(command != nullptr);
    CHECK_FALSE(static_cast<bool>(command->execute));
    registry.clear();
}

TEST_CASE("Spelling settings: the language and the writer's own words",
          "[gui][spelling][settings]") {
    KeptSpellingSettings kept;
    auto& settings = KeptSpellingSettings::settings();
    settings.set<bool>(ENABLED_KEY, true);
    settings.set<std::string>(LANGUAGE_KEY, "");

    // The words of a dictionary kept in no file
    editor::SpellCheckService spelling;
    spelling.addToUserDictionary(QStringLiteral("kalahari"));

    SettingsDialog dialog(nullptr, false, &spelling);
    dialog.show();
    openPage(dialog, QStringLiteral("Editor"), QStringLiteral("Spelling"));
    CHECK_FALSE(dialog.hasChanges());

    QComboBox* language = languageBox(dialog);
    REQUIRE(language != nullptr);
    CHECK(language->currentIndex() == 0);
    CHECK(language->currentData().toString().isEmpty());
    CHECK(language->itemText(0) == QStringLiteral("Language of the book"));

    // The dictionaries by their languages in the language of the program, in the order of
    // the names
    CHECK(language->itemText(language->findData(QStringLiteral("pl_PL"))) ==
          QStringLiteral("Polish (pl_PL)"));
    CHECK(language->itemText(language->findData(QStringLiteral("en_US"))) ==
          QStringLiteral("English (en_US)"));
    const QCollator collator(QLocale(QString::fromStdString(settings.getLanguage())));
    for (int index = 1; index + 1 < language->count(); ++index) {
        INFO(language->itemText(index).toStdString());
        CHECK(collator.compare(language->itemText(index), language->itemText(index + 1)) <= 0);
    }

    QListWidget* words = wordList(dialog);
    REQUIRE(words != nullptr);
    CHECK(listed(*words) == QStringList{QStringLiteral("kalahari")});
    QWidget* group = words->parentWidget();
    REQUIRE(group != nullptr);
    auto* newWord = group->findChild<QLineEdit*>();
    auto* add = buttonWithText<QPushButton>(*group, QStringLiteral("Add"));
    auto* remove = buttonWithText<QPushButton>(*group, QStringLiteral("Remove"));
    REQUIRE(newWord != nullptr);
    REQUIRE(add != nullptr);
    REQUIRE(remove != nullptr);
    CHECK_FALSE(add->isEnabled());
    CHECK_FALSE(remove->isEnabled());

    // Enter in the field adds the word; the dialog stays open
    newWord->setText(QStringLiteral("Quuxly"));
    CHECK(add->isEnabled());
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QCoreApplication::sendEvent(newWord, &enter);
    CHECK(dialog.isVisible());
    CHECK(newWord->text().isEmpty());
    CHECK(listed(*words) == (QStringList{QStringLiteral("Quuxly"), QStringLiteral("kalahari")}));

    // A word listed already, two words, or a comment of the file cannot be added
    newWord->setText(QStringLiteral("kalahari"));
    CHECK_FALSE(add->isEnabled());
    newWord->setText(QStringLiteral("two words"));
    CHECK_FALSE(add->isEnabled());
    newWord->setText(QStringLiteral("#tag"));
    CHECK_FALSE(add->isEnabled());
    newWord->clear();

    // The words selected are removed
    words->clearSelection();
    const QList<QListWidgetItem*> found =
        words->findItems(QStringLiteral("kalahari"), Qt::MatchExactly);
    REQUIRE(found.size() == 1);
    found.first()->setSelected(true);
    CHECK(remove->isEnabled());
    remove->click();
    CHECK(listed(*words) == QStringList{QStringLiteral("Quuxly")});

    // The dictionary gets them on Apply; they are not a setting
    CHECK(spelling.userDictionaryWords() == QStringList{QStringLiteral("kalahari")});
    CHECK(dialog.hasChanges());
    CHECK(dialog.applyChanges().isEmpty());
    CHECK(spelling.userDictionaryWords() == QStringList{QStringLiteral("Quuxly")});
    CHECK_FALSE(dialog.hasChanges());

    // The language, greyed out while the checking is off
    auto* enabled = buttonWithText<QCheckBox>(dialog, QStringLiteral("Check spelling as you type"));
    REQUIRE(enabled != nullptr);
    CHECK(enabled->isChecked());
    CHECK(language->isEnabled());
    language->setCurrentIndex(language->findData(QStringLiteral("pl_PL")));
    enabled->setChecked(false);
    CHECK_FALSE(language->isEnabled());
    const QStringList applied = dialog.applyChanges();
    CHECK(applied.contains(QString::fromLatin1(ENABLED_KEY)));
    CHECK(applied.contains(QString::fromLatin1(LANGUAGE_KEY)));
    CHECK_FALSE(settings.get<bool>(ENABLED_KEY, true));
    CHECK(settings.get<std::string>(LANGUAGE_KEY, std::string()) == "pl_PL");
}

TEST_CASE("Spelling settings: a language whose dictionary is gone is kept",
          "[gui][spelling][settings]") {
    KeptSpellingSettings kept;
    auto& settings = KeptSpellingSettings::settings();
    settings.set<bool>(ENABLED_KEY, true);

    // A language the program knows (Hunspell has no dictionary for Japanese), and one it
    // does not know
    const std::pair<std::string, QString> gone[] = {
        {"ja_JP", QStringLiteral("Japanese (ja_JP): no dictionary")},
        {"xx_YY", QStringLiteral("xx_YY: no dictionary")}};
    for (const auto& [stored, shown] : gone) {
        INFO(stored);
        settings.set<std::string>(LANGUAGE_KEY, stored);

        // Without the dictionary of the editors the page does not list its words
        SettingsDialog dialog(nullptr);
        openPage(dialog, QStringLiteral("Editor"), QStringLiteral("Spelling"));
        CHECK(wordList(dialog) == nullptr);

        QComboBox* language = languageBox(dialog);
        REQUIRE(language != nullptr);
        CHECK(language->currentData().toString() == QString::fromStdString(stored));
        CHECK(language->currentText() == shown);
        CHECK_FALSE(dialog.hasChanges());
    }
}

TEST_CASE("Spelling commands: each has an icon of its own in every icon theme",
          "[gui][spelling]") {
    registerAllIcons();
    const auto& icons = core::IconRegistry::getInstance();
    QStringList files;
    for (const char* id : {"tools.spellcheck", "tools.nextMisspelling"}) {
        INFO(id);
        const core::IconDescriptor* icon = icons.getIconDescriptor(QString::fromLatin1(id));
        REQUIRE(icon != nullptr);
        const QString file = QFileInfo(icon->defaultSVGPath).fileName();
        CHECK_FALSE(files.contains(file));
        files.append(file);
        for (const char* theme : {"twotone", "filled", "outlined", "rounded"}) {
            INFO(theme);
            CHECK(QFileInfo::exists(QStringLiteral(KALAHARI_SOURCE_DIR "/resources/icons/%1/%2")
                                        .arg(QLatin1String(theme), file)));
        }
    }
}
