/// @file test_repetitions_panel.cpp
/// @brief The Repetitions panel: its cards and words, what it says without them, its keys; with
///        the texts: the words counted, marked and framed in the text, gone to from the panel,
///        skipped; Next Repetition; the whole book; Settings > Editor > Repetitions

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/chapter_document.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/text_highlight.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/repetitions_panel.h"
#include "kalahari/gui/repetitions_coordinator.h"
#include "kalahari/gui/settings/settings_pages.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTreeWidget>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace kalahari;
using namespace kalahari::gui;
using kalahari::test::kmlOf;
using kalahari::test::waitUntil;

namespace {

using json = nlohmann::json;
using Coordinator = RepetitionsCoordinator;

core::SettingsManager& settings() {
    return core::SettingsManager::getInstance();
}

/// Gives back the settings of the repetitions and the spelling as they were before the test
class KeptSettings {
public:
    KeptSettings() {
        for (const char* key : KEYS) {
            m_values[key] = settings().get<json>(key, json());
        }
    }
    ~KeptSettings() {
        try {
            for (const auto& [key, value] : m_values) {
                if (value.is_null()) {
                    settings().removeKey(key);
                } else {
                    settings().set<json>(key, value);
                }
            }
        } catch (...) {
            // Left as the test made them; the next tests set what they need
            std::fputs("The settings of the repetitions could not be given back\n", stderr);
        }
    }
    KeptSettings(const KeptSettings&) = delete;
    KeptSettings& operator=(const KeptSettings&) = delete;

private:
    static constexpr const char* KEYS[] = {
        Coordinator::DISTANCE_KEY,    Coordinator::MIN_LENGTH_KEY, Coordinator::MERGE_FORMS_KEY,
        Coordinator::SKIP_COMMON_KEY, Coordinator::SKIPPED_KEY,    Coordinator::HIGHLIGHT_KEY,
        Coordinator::STYLE_KEY,       "editor.spellCheck.language"};
    std::map<std::string, json> m_values;
};

/// The settings the tests start from: English, the forms of a word not merged (there is no
/// dictionary), the rest as by default
void setDefaults() {
    settings().set<std::string>("editor.spellCheck.language", "en_US");
    settings().set<int>(Coordinator::DISTANCE_KEY, 30);
    settings().set<int>(Coordinator::MIN_LENGTH_KEY, 3);
    settings().set<bool>(Coordinator::MERGE_FORMS_KEY, false);
    settings().set<bool>(Coordinator::SKIP_COMMON_KEY, true);
    settings().set<json>(Coordinator::SKIPPED_KEY, json::array());
    settings().set<bool>(Coordinator::HIGHLIGHT_KEY, true);
    settings().set<std::string>(Coordinator::STYLE_KEY, "background");
}

/// The program's commands while the test lives
class Commands {
public:
    Commands() { registerAllCommands(CommandCallbacks{}); }
    ~Commands() { CommandRegistry::getInstance().clear(); }
    Commands(const Commands&) = delete;
    Commands& operator=(const Commands&) = delete;
};

/// Whether a widget takes a key ahead of the window's shortcuts
bool takesKey(QWidget* widget, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent shortcut(QEvent::ShortcutOverride, key, modifiers);
    shortcut.ignore();
    QApplication::sendEvent(widget, &shortcut);
    return shortcut.isAccepted();
}

/// A key pressed and released in a widget, as the keyboard does it
void press(QWidget* widget, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier,
           const QString& text = QString()) {
    REQUIRE(widget != nullptr);
    QKeyEvent pressed(QEvent::KeyPress, key, modifiers, text);
    QApplication::sendEvent(widget, &pressed);
    QKeyEvent released(QEvent::KeyRelease, key, modifiers, text);
    QApplication::sendEvent(widget, &released);
}

/// The texts a panel shows in its labels
QStringList shownTexts(const QWidget& panel) {
    QStringList texts;
    for (const QLabel* label : panel.findChildren<QLabel*>()) {
        if (label->isVisibleTo(&panel) && !label->text().isEmpty()) {
            texts.append(label->text());
        }
    }
    return texts;
}

/// The words marked in a text, "night 0:45" ("*" after the framed ones), sorted
QStringList marked(const editor::BookEditor& editor) {
    QStringList words;
    for (const editor::RepetitionMark& mark : editor.repetitionMarks()) {
        words.append(QStringLiteral("%1 %2:%3%4")
                         .arg(editor.paragraphPlainText(static_cast<std::size_t>(mark.paragraph))
                                  .mid(mark.start, mark.length))
                         .arg(mark.paragraph)
                         .arg(mark.start)
                         .arg(mark.current ? QStringLiteral("*") : QString()));
    }
    words.sort();
    return words;
}

/// The words of the cards of a panel
QStringList cardWords(const RepetitionsPanel& panel) {
    QStringList words;
    for (const RepetitionCard& card : panel.cards()) {
        words.append(card.word);
    }
    return words;
}

/// A text with two words that come back: "rain" three times, "night" twice; "the" is too
/// common to count
const QStringList RAIN = {QStringLiteral("Rain fell on the town. The rain came back at night."),
                          QStringLiteral("In the morning the rain stopped at night.")};

/// The window of the coordinator's tests, with its parts as MainWindow has them: the documents
/// in the middle; on the right the Annotations panel and the Repetitions panel, closed, in a
/// tab next to it
struct Desk {
    // Not the window's own: see test_dock_layout.cpp (QTBUG-137524)
    QStatusBar statusBar;
    QMainWindow window;
    QTabWidget* tabs = nullptr;
    QDockWidget* annotations = nullptr;
    QDockWidget* dock = nullptr;
    RepetitionsPanel* panel = nullptr;
    QStringList opened;  ///< The chapters the coordinator opened
    std::unique_ptr<RepetitionsCoordinator> coordinator;

    Desk() {
        tabs = new QTabWidget();
        window.setCentralWidget(tabs);
        annotations = addPanel(QStringLiteral("Annotations"), new QLabel(QStringLiteral("-")));
        panel = new RepetitionsPanel();
        dock = addPanel(QStringLiteral("Repetitions"), panel);
        window.tabifyDockWidget(annotations, dock);
        dock->hide();
        annotations->raise();
        coordinator = std::make_unique<RepetitionsCoordinator>(
            panel, dock, tabs, nullptr,
            [this](const QString& elementId) {
                opened.append(elementId);
                openChapter(elementId);
            },
            &statusBar);
        coordinator->connectCommands();
        window.resize(1000, 700);
        window.show();
        QApplication::processEvents();
    }

    Desk(const Desk&) = delete;
    Desk& operator=(const Desk&) = delete;

    QDockWidget* addPanel(const QString& title, QWidget* content) {
        auto* added = new QDockWidget(title, &window);
        added->setObjectName(title);
        added->setWidget(content);
        window.addDockWidget(Qt::RightDockWidgetArea, added);
        return added;
    }

    /// Open a document with the paragraphs in a tab of its own, in front
    editor::BookEditor* open(const QStringList& paragraphs,
                             const QString& title = QStringLiteral("Document")) {
        auto* document = new EditorPanel();
        document->setContent(kmlOf(paragraphs));
        tabs->setCurrentIndex(tabs->addTab(document, title));
        return document->getBookEditor();
    }

    /// Open a chapter of the book from its file, as the Navigator does
    editor::BookEditor* openChapter(const QString& elementId) {
        auto& pm = core::ProjectManager::getInstance();
        const core::ProjectElement* element = pm.findElement(elementId);
        REQUIRE(element != nullptr);
        const std::optional<core::ChapterDocument> chapter =
            core::ChapterDocument::load(pm.filePathOf(*element));
        REQUIRE(chapter.has_value());
        auto* document = new EditorPanel();
        document->setContent(chapter->kml());
        document->setProperty("elementId", elementId);
        tabs->setCurrentIndex(tabs->addTab(document, element->title));
        return document->getBookEditor();
    }

    /// Run the event loop until the panel has these cards
    bool lists(const QStringList& words) const {
        const RepetitionsPanel* shown = panel;
        const bool listed = waitUntil([shown, &words]() { return cardWords(*shown) == words; });
        if (!listed) {
            UNSCOPED_INFO("The cards: " << cardWords(*shown).join(QStringLiteral(", ")).toStdString());
        }
        return listed;
    }

    /// Next Repetition
    static void nextRepetition() {
        QAction* next =
            CommandRegistry::getInstance().getAction(std::string("tools.nextRepetition"));
        REQUIRE(next != nullptr);
        REQUIRE(next->isEnabled());
        next->trigger();
    }
};

/// A novel with two chapters of these texts, its own chapters taken out
struct Novel {
    QTemporaryDir dir;
    core::ProjectManager& pm = core::ProjectManager::getInstance();
    QStringList chapters;  ///< The ids of the chapters, in reading order

    Novel(const QStringList& first, const QStringList& second) {
        REQUIRE(dir.isValid());
        pm.loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
        REQUIRE(pm.createProject(dir.path(), QStringLiteral("Rain"), QStringLiteral("Author"),
                                 QStringLiteral("en"), false, QStringLiteral("kalahari.novel")));
        const core::ProjectBook* book = pm.book();
        REQUIRE(book != nullptr);
        for (const QList<core::ProjectElement>* list : {&book->frontElements, &book->mainElements}) {
            while (!list->isEmpty()) {
                const std::optional<core::ProjectElement> removed =
                    pm.removeElement(list->first().id);
                REQUIRE(removed.has_value());
            }
        }
        add(QStringLiteral("Morning"), first);
        add(QStringLiteral("Evening"), second);
    }
    ~Novel() {
        try {
            pm.closeProject(false);
        } catch (...) {
            std::fputs("The novel of the repetitions could not be closed\n", stderr);
        }
    }
    Novel(const Novel&) = delete;
    Novel& operator=(const Novel&) = delete;

    void add(const QString& title, const QStringList& paragraphs) {
        const QString id = pm.addElement(pm.chapterKindFor(core::BookPlace::Main), title,
                                         core::BookPlace::Main);
        REQUIRE_FALSE(id.isEmpty());
        const core::ProjectElement* element = pm.findElement(id);
        REQUIRE(element != nullptr);
        core::ChapterDocument chapter;
        chapter.setTitle(title);
        chapter.setKml(kmlOf(paragraphs));
        REQUIRE(chapter.save(pm.filePathOf(*element)));
        chapters.append(id);
    }
};

}  // namespace

// =============================================================================
// The panel
// =============================================================================

TEST_CASE("Repetitions panel: the cards and the most frequent words, and what it says without "
          "them",
          "[gui][repetitions]") {
    RepetitionsPanel panel;
    panel.resize(320, 600);
    panel.show();
    QApplication::processEvents();

    // At first neither: the lists are empty
    CHECK(shownTexts(panel).contains(QStringLiteral("No word comes back too close.")));

    const QList<RepetitionCard> cards = {
        {QStringLiteral("rain"), QStringLiteral("3 times in 16 words"),
         QStringLiteral("Rain fell, the rain came, the rain stopped."),
         {{0, 4}, {14, 4}, {33, 4}}},
        {QStringLiteral("night"), QStringLiteral("2 times in 9 words"),
         QStringLiteral("… at night. … at night."), {{5, 5}, {17, 5}}}};
    const QList<FrequentWord> words = {
        {QStringLiteral("rain"), 3, 157.9, QString()},
        {QStringLiteral("say"), 2, 105.3, QStringLiteral("said 1, says 1")}};
    panel.setResults(cards, words, QStringLiteral("Document: 19 words · 2 close"),
                     QStringLiteral("Document: 19 words, 8 different"));
    CHECK(panel.cards() == cards);
    CHECK(panel.frequentWords() == words);
    CHECK(panel.cardList()->count() == 2);
    CHECK(panel.currentCard() == 0);
    CHECK(shownTexts(panel).contains(QStringLiteral("Document: 19 words · 2 close")));
    REQUIRE(panel.wordList()->topLevelItemCount() == 2);
    CHECK(panel.wordList()->topLevelItem(0)->text(0) == QStringLiteral("rain"));
    CHECK(panel.wordList()->topLevelItem(1)->text(0) == QStringLiteral("say\nsaid 1, says 1"));
    CHECK(panel.wordList()->topLevelItem(1)->text(1) == QStringLiteral("2"));
    CHECK(panel.wordList()->topLevelItem(1)->text(2) == QStringLiteral("105.3"));

    // The cards are as tall as their texts and buttons
    const int cardHeight = panel.cardList()->visualItemRect(panel.cardList()->item(0)).height();
    CHECK(cardHeight > 3 * panel.fontMetrics().height());

    // The card chosen stays chosen while there are as many
    panel.setCurrentCard(1);
    panel.setResults(cards, words, QString(), QString());
    CHECK(panel.currentCard() == 1);
    panel.setResults(cards.mid(0, 1), words, QString(), QString());
    CHECK(panel.currentCard() == 0);

    // A message in place of the lists
    panel.setMessage(QStringLiteral("No text is in front."));
    CHECK(panel.message() == QStringLiteral("No text is in front."));
    CHECK(shownTexts(panel).contains(QStringLiteral("No text is in front.")));
    CHECK_FALSE(panel.cardList()->isVisibleTo(&panel));
    panel.setMessage(QString());
    CHECK(panel.cardList()->isVisibleTo(&panel));

    // The pages
    int pages = 0;
    QObject::connect(&panel, &RepetitionsPanel::pageChanged, [&pages]() { ++pages; });
    panel.setPage(RepetitionsPanel::Page::Frequent);
    CHECK(panel.page() == RepetitionsPanel::Page::Frequent);
    CHECK(pages == 0);  // set by the program: no signal
    panel.setResults({}, {}, QString(), QString());
    CHECK(shownTexts(panel).contains(QStringLiteral("No words are counted.")));
}

TEST_CASE("Repetitions panel: the scope, Recount for the whole book, the book only when open",
          "[gui][repetitions]") {
    RepetitionsPanel panel;
    panel.show();
    QApplication::processEvents();
    auto* chapter = panel.findChild<QToolButton*>(QStringLiteral("repetitionsChapterScope"));
    auto* book = panel.findChild<QToolButton*>(QStringLiteral("repetitionsBookScope"));
    REQUIRE(chapter != nullptr);
    REQUIRE(book != nullptr);
    CHECK(chapter->isChecked());
    CHECK(panel.recountButton()->isHidden());

    QList<RepetitionsPanel::Scope> scopes;
    QObject::connect(&panel, &RepetitionsPanel::scopeChanged,
                     [&scopes](RepetitionsPanel::Scope scope) { scopes.append(scope); });
    int recounts = 0;
    QObject::connect(&panel, &RepetitionsPanel::recountRequested, [&recounts]() { ++recounts; });
    book->click();
    CHECK(scopes == QList<RepetitionsPanel::Scope>{RepetitionsPanel::Scope::Book});
    CHECK(panel.scope() == RepetitionsPanel::Scope::Book);
    CHECK_FALSE(panel.recountButton()->isHidden());
    panel.recountButton()->click();
    CHECK(recounts == 1);

    // Chosen by the program: no signal
    panel.setScope(RepetitionsPanel::Scope::Chapter);
    CHECK(chapter->isChecked());
    CHECK(scopes.size() == 1);

    panel.setBookAvailable(false);
    CHECK_FALSE(book->isEnabled());
    CHECK(book->toolTip() == QStringLiteral("No book is open."));
    panel.setBookAvailable(true);
    CHECK(book->isEnabled());
}

TEST_CASE("Repetitions panel: its keys go to the words, skip a word and go back to the text, "
          "ahead of the window's shortcuts",
          "[gui][repetitions]") {
    RepetitionsPanel panel;
    panel.show();
    const QList<RepetitionCard> cards = {
        {QStringLiteral("rain"), QStringLiteral("3 times"), QStringLiteral("rain rain rain"), {}},
        {QStringLiteral("night"), QStringLiteral("2 times"), QStringLiteral("night night"), {}}};
    const QList<FrequentWord> words = {{QStringLiteral("rain"), 3, 150.0, QString()},
                                       {QStringLiteral("night"), 2, 100.0, QString()}};
    panel.setResults(cards, words, QString(), QString());
    QApplication::processEvents();

    // What the panel asked for: "selected 1", "activated 1"...
    QStringList asked;
    const auto follow = [&panel, &asked](auto signal, const QString& name) {
        QObject::connect(&panel, signal, [&asked, name](auto... index) {
            asked.append((name + QStringLiteral(" ") + QString::number(index...)).trimmed());
        });
    };
    follow(&RepetitionsPanel::cardSelected, QStringLiteral("selected"));
    follow(&RepetitionsPanel::cardActivated, QStringLiteral("activated"));
    follow(&RepetitionsPanel::skipWordRequested, QStringLiteral("skipped"));
    follow(&RepetitionsPanel::wordActivated, QStringLiteral("word"));
    QObject::connect(&panel, &RepetitionsPanel::editorFocusRequested,
                     [&asked]() { asked.append(QStringLiteral("back")); });
    QObject::connect(&panel, &RepetitionsPanel::pageChanged,
                     [&asked]() { asked.append(QStringLiteral("page")); });

    QListWidget* list = panel.cardList();
    // Enter, Delete and Esc are the panel's, before the window's shortcuts (Edit > Delete)
    CHECK(takesKey(list, Qt::Key_Return));
    CHECK(takesKey(list, Qt::Key_Enter, Qt::KeypadModifier));
    CHECK(takesKey(list, Qt::Key_Delete));
    CHECK(takesKey(list, Qt::Key_Escape));
    CHECK_FALSE(takesKey(list, Qt::Key_F7));
    CHECK_FALSE(takesKey(list, Qt::Key_Delete, Qt::ControlModifier));

    press(list, Qt::Key_Down);
    CHECK(panel.currentCard() == 1);
    press(list, Qt::Key_Return);
    press(list, Qt::Key_Delete);
    press(list, Qt::Key_Escape);
    CHECK(asked == QStringList{QStringLiteral("selected 1"), QStringLiteral("activated 1"),
                               QStringLiteral("skipped 1"), QStringLiteral("back")});
    asked.clear();

    // Ctrl+Tab: the other page; Enter there goes to the next use of the word
    press(list, Qt::Key_Tab, Qt::ControlModifier);
    CHECK(panel.page() == RepetitionsPanel::Page::Frequent);
    QTreeWidget* tree = panel.wordList();
    CHECK(takesKey(tree, Qt::Key_Return));
    CHECK_FALSE(takesKey(tree, Qt::Key_Delete));  // no word to skip there
    press(tree, Qt::Key_Return);
    press(tree, Qt::Key_Tab, Qt::ControlModifier | Qt::ShiftModifier);
    CHECK(panel.page() == RepetitionsPanel::Page::Close);
    CHECK(asked == QStringList{QStringLiteral("page"), QStringLiteral("word 0"),
                               QStringLiteral("page")});
}

// =============================================================================
// The panel and the texts
// =============================================================================

TEST_CASE("Repetitions: the words of the text in front, marked in it while the panel shows",
          "[gui][repetitions][coordinator]") {
    KeptSettings kept;
    setDefaults();
    REQUIRE(core::ProjectManager::getInstance().closeProject(false));
    Commands commands;
    Desk desk;

    // No text in front
    desk.coordinator->showPanel();
    CHECK(desk.dock->isVisible());
    CHECK(desk.panel->message() == QStringLiteral("No text is in front. Open a chapter to see "
                                                  "the words that come back too close."));

    // A text: its words that come back, in the order of the text
    editor::BookEditor* editor = desk.open(RAIN);
    REQUIRE(desk.lists({QStringLiteral("rain"), QStringLiteral("night")}));
    CHECK(desk.panel->message().isEmpty());
    const RepetitionCard rain = desk.panel->cards().at(0);
    CHECK(rain.detail == QStringLiteral("3 time(s) in 16 words"));
    CHECK(rain.marks.size() == 3);
    for (const auto& [start, length] : rain.marks) {
        CHECK(rain.context.mid(start, length).toLower() == QStringLiteral("rain"));
    }
    CHECK(desk.panel->cards().at(1).detail == QStringLiteral("2 time(s) in 9 words"));
    CHECK(shownTexts(*desk.panel)
              .contains(QStringLiteral("Document: 19 word(s) · 2 repetition(s) close together")));
    REQUIRE(desk.panel->frequentWords().size() == 8);
    CHECK(desk.panel->frequentWords().at(0).word == QStringLiteral("rain"));
    CHECK(desk.panel->frequentWords().at(0).count == 3);
    CHECK(std::abs(desk.panel->frequentWords().at(0).perThousand - 3000.0 / 19.0) < 0.01);

    // In the text: the words of both, those of the card chosen framed
    CHECK(marked(*editor) == QStringList{QStringLiteral("Rain 0:0*"), QStringLiteral("night 0:45"),
                                         QStringLiteral("night 1:35"), QStringLiteral("rain 0:27*"),
                                         QStringLiteral("rain 1:19*")});
    press(desk.panel->cardList(), Qt::Key_Down);
    CHECK(marked(*editor) == QStringList{QStringLiteral("Rain 0:0"), QStringLiteral("night 0:45*"),
                                         QStringLiteral("night 1:35*"), QStringLiteral("rain 0:27"),
                                         QStringLiteral("rain 1:19")});

    // Without the marks of all: only the chosen ones, framed
    settings().set<bool>(Coordinator::HIGHLIGHT_KEY, false);
    CHECK(waitUntil([editor]() { return editor->repetitionMarks().size() == 2; }));
    CHECK(marked(*editor) ==
          QStringList{QStringLiteral("night 0:45*"), QStringLiteral("night 1:35*")});
    settings().set<bool>(Coordinator::HIGHLIGHT_KEY, true);
    CHECK(waitUntil([editor]() { return editor->repetitionMarks().size() == 5; }));

    // View > Repetitions in Text turns the setting off and on, checked while it is on
    QAction* marks = CommandRegistry::getInstance().getAction(std::string("view.repetitionMarks"));
    REQUIRE(marks != nullptr);
    CHECK(marks->isChecked());
    marks->trigger();
    CHECK_FALSE(settings().get<bool>(Coordinator::HIGHLIGHT_KEY));
    CHECK_FALSE(marks->isChecked());
    marks->trigger();
    CHECK(settings().get<bool>(Coordinator::HIGHLIGHT_KEY));

    // The most frequent words: the uses of the word chosen framed
    desk.panel->setPage(RepetitionsPanel::Page::Frequent);
    desk.panel->setCurrentWord(1);  // night
    emit desk.panel->wordSelected(1);
    CHECK(marked(*editor) == QStringList{QStringLiteral("Rain 0:0"), QStringLiteral("night 0:45*"),
                                         QStringLiteral("night 1:35*"), QStringLiteral("rain 0:27"),
                                         QStringLiteral("rain 1:19")});
    desk.panel->setPage(RepetitionsPanel::Page::Close);

    // The panel closed: no marks
    desk.dock->hide();
    QApplication::processEvents();
    CHECK(editor->repetitionMarks().empty());
    desk.coordinator->showPanel();
    QApplication::processEvents();
    CHECK(editor->repetitionMarks().size() == 5);

    // Writing: counted again a moment after it stops
    editor->setCursorPosition({1, 0});
    editor->insertText(QStringLiteral("Morning "));
    CHECK(desk.lists({QStringLiteral("rain"), QStringLiteral("night"), QStringLiteral("morning")}));

    // Another setting: counted again ("rain" at the 1st, 7th and 17th word, "night" at the 11th
    // and 20th, "morning" at the 12th and 15th)
    settings().set<int>(Coordinator::DISTANCE_KEY, 8);
    CHECK(desk.lists({QStringLiteral("rain"), QStringLiteral("morning")}));
}

TEST_CASE("Repetitions: Go To and Skip This Word on a card", "[gui][repetitions][coordinator]") {
    KeptSettings kept;
    setDefaults();
    REQUIRE(core::ProjectManager::getInstance().closeProject(false));
    Commands commands;
    Desk desk;
    editor::BookEditor* editor = desk.open(RAIN);
    desk.coordinator->showPanel();
    REQUIRE(desk.lists({QStringLiteral("rain"), QStringLiteral("night")}));

    // Enter on the card of "night": its first word selected in the text
    press(desk.panel->cardList(), Qt::Key_Down);
    press(desk.panel->cardList(), Qt::Key_Return);
    CHECK(editor->selectedText() == QStringLiteral("night"));
    CHECK(editor->cursorPosition().paragraph == 0);

    // A card whose words are not where they were counted: counted again, nothing selected
    editor->clearSelection();
    editor->setCursorPosition({0, 0});
    editor->insertText(QStringLiteral("X"));
    desk.coordinator->refresh();
    REQUIRE(desk.lists({QStringLiteral("rain"), QStringLiteral("night")}));
    editor->setCursorPosition({0, 0});
    editor->insertText(QStringLiteral("Y"));  // the words move after the count
    QApplication::processEvents();
    press(desk.panel->cardList(), Qt::Key_Return);
    CHECK(desk.statusBar.currentMessage() ==
          QStringLiteral("The text changed: the repetitions are counted again"));
    CHECK(editor->selectedText().isEmpty());

    // Delete: the word of the card is skipped from now on, and counted again
    desk.panel->setCurrentCard(0);
    REQUIRE(desk.panel->cards().at(0).word == QStringLiteral("rain"));
    press(desk.panel->cardList(), Qt::Key_Delete);
    CHECK(settings().get<std::vector<std::string>>(Coordinator::SKIPPED_KEY) ==
          std::vector<std::string>{"rain"});
    CHECK(desk.statusBar.currentMessage() ==
          QStringLiteral("“rain” is not counted any more. The skipped words are in Settings > "
                         "Editor > Repetitions"));
    CHECK(desk.lists({QStringLiteral("night")}));

    // Once is enough
    Coordinator::skipWord(QStringLiteral("Rain"));
    CHECK(settings().get<std::vector<std::string>>(Coordinator::SKIPPED_KEY) ==
          std::vector<std::string>{"rain"});
}

TEST_CASE("Repetitions: Next Repetition selects the next word that comes back too close, with "
          "the panel closed too",
          "[gui][repetitions][coordinator]") {
    KeptSettings kept;
    setDefaults();
    REQUIRE(core::ProjectManager::getInstance().closeProject(false));
    Commands commands;
    Desk desk;

    // No text in front: greyed out
    QAction* next = CommandRegistry::getInstance().getAction(std::string("tools.nextRepetition"));
    REQUIRE(next != nullptr);
    CHECK_FALSE(next->isEnabled());

    editor::BookEditor* editor = desk.open(RAIN);
    QApplication::processEvents();
    CHECK(next->isEnabled());
    editor->setCursorPosition({0, 0});

    Desk::nextRepetition();
    CHECK(editor->selectedText() == QStringLiteral("Rain"));
    CHECK(desk.statusBar.currentMessage() ==
          QStringLiteral("Repetition: “rain”, 3 time(s) in 16 words"));
    CHECK(desk.dock->isHidden());  // the panel stays closed

    Desk::nextRepetition();
    CHECK(editor->selectedText() == QStringLiteral("night"));
    CHECK(editor->cursorPosition().paragraph == 0);

    // After the last one, the first again
    Desk::nextRepetition();
    CHECK(editor->selectedText() == QStringLiteral("Rain"));

    // The text as it is now, also when the panel counted it before
    editor->clearSelection();
    editor->setCursorPosition({0, 0});
    editor->insertText(QStringLiteral("Night "));
    editor->setCursorPosition({0, 1});
    Desk::nextRepetition();
    CHECK(editor->selectedText() == QStringLiteral("Rain"));
    CHECK(editor->selection().normalized().start.offset == 6);

    // A text without words that come back
    desk.open({QStringLiteral("One short line.")}, QStringLiteral("Other"));
    QApplication::processEvents();
    Desk::nextRepetition();
    CHECK(desk.statusBar.currentMessage() == QStringLiteral("No word comes back too close here"));
}

TEST_CASE("Repetitions: the whole book, its chapters open or not",
          "[gui][repetitions][coordinator]") {
    KeptSettings kept;
    setDefaults();
    Commands commands;
    Novel novel({QStringLiteral("The storm came. The storm went.")},
                {QStringLiteral("A quiet evening, a quiet street.")});
    Desk desk;
    editor::BookEditor* morning = desk.openChapter(novel.chapters.at(0));
    desk.coordinator->showPanel();
    REQUIRE(desk.lists({QStringLiteral("storm")}));

    // The whole book: the chapters in reading order, the one open as it is in its editor
    morning->setCursorPosition({0, 0});
    morning->insertText(QStringLiteral("Thunder. Thunder. "));
    auto* book = desk.panel->findChild<QToolButton*>(QStringLiteral("repetitionsBookScope"));
    REQUIRE(book != nullptr);
    REQUIRE(book->isEnabled());
    book->click();
    REQUIRE(desk.lists({QStringLiteral("thunder"), QStringLiteral("storm"),
                        QStringLiteral("quiet")}));
    CHECK(desk.panel->cards().at(0).detail.startsWith(QStringLiteral("Morning · ")));
    CHECK(desk.panel->cards().at(2).detail.startsWith(QStringLiteral("Evening · ")));
    CHECK(shownTexts(*desk.panel).contains(
        QStringLiteral("Whole book: 14 word(s) · 3 repetition(s) close together")));

    // Go To on a card of a chapter not open opens it, its word selected
    desk.panel->setCurrentCard(2);
    press(desk.panel->cardList(), Qt::Key_Return);
    CHECK(desk.opened == QStringList{novel.chapters.at(1)});
    auto* evening = qobject_cast<EditorPanel*>(desk.tabs->currentWidget());
    REQUIRE(evening != nullptr);
    CHECK(evening->getBookEditor()->selectedText() == QStringLiteral("quiet"));

    // A chapter written after the count: the panel says so; Recount counts it again
    morning->setCursorPosition({0, 0});
    morning->insertText(QStringLiteral("Rain. "));
    QApplication::processEvents();
    CHECK(shownTexts(*desk.panel)
              .contains(QStringLiteral("Whole book: 14 word(s) · 3 repetition(s) close together\n"
                                       "The text changed after it was counted: click Recount.")));
    desk.panel->recountButton()->click();
    CHECK(waitUntil([&desk]() {
        return shownTexts(*desk.panel)
            .contains(QStringLiteral("Whole book: 15 word(s) · 3 repetition(s) close together"));
    }));

    // The book closed: the chapter again
    desk.tabs->clear();
    REQUIRE(core::ProjectManager::getInstance().closeProject(false));
    QApplication::processEvents();
    CHECK(desk.panel->scope() == RepetitionsPanel::Scope::Chapter);
    CHECK_FALSE(book->isEnabled());
}

// =============================================================================
// Settings > Editor > Repetitions
// =============================================================================

TEST_CASE("Repetitions: the page of the settings", "[gui][repetitions][settings]") {
    KeptSettings kept;
    setDefaults();
    settings().set<json>(Coordinator::SKIPPED_KEY, json::array({"said"}));
    Commands commands;

    EditorRepetitionsPage page;
    page.load();
    const auto child = [&page]<typename Widget>(Widget*, const char* name) {
        auto* found = page.findChild<Widget*>(QString::fromLatin1(name));
        REQUIRE(found != nullptr);
        return found;
    };
    auto* distance = child(static_cast<QSpinBox*>(nullptr), "repetitionsDistance");
    auto* length = child(static_cast<QSpinBox*>(nullptr), "repetitionsMinLength");
    auto* highlight = child(static_cast<QCheckBox*>(nullptr), "repetitionsHighlight");
    auto* style = child(static_cast<QComboBox*>(nullptr), "repetitionsStyle");
    auto* words = child(static_cast<QListWidget*>(nullptr), "repetitionsSkipped");
    auto* newWord = child(static_cast<QLineEdit*>(nullptr), "repetitionsNewWord");
    auto* add = child(static_cast<QPushButton*>(nullptr), "repetitionsAddWord");
    auto* remove = child(static_cast<QPushButton*>(nullptr), "repetitionsRemoveWords");
    CHECK(distance->value() == 30);
    CHECK(length->value() == 3);
    CHECK(highlight->isChecked());
    CHECK(style->currentData().toString() == QStringLiteral("background"));
    REQUIRE(words->count() == 1);
    CHECK(words->item(0)->text() == QStringLiteral("said"));
    CHECK_FALSE(page.isChanged());

    // The word after the number in the form the number needs
    const auto unitAfter = [](QSpinBox* spin) {
        const auto* label = spin->parentWidget()->findChild<QLabel*>();
        REQUIRE(label != nullptr);
        return label->text();
    };
    CHECK(unitAfter(distance) == QStringLiteral("word(s)"));

    // The way of the marks waits while they are off
    highlight->setChecked(false);
    CHECK_FALSE(style->isEnabled());
    highlight->setChecked(true);
    CHECK(style->isEnabled());

    // Words added with Enter, in lower case, once; removed with Delete
    CHECK_FALSE(add->isEnabled());
    newWord->setText(QStringLiteral("Looked"));
    CHECK(add->isEnabled());
    press(newWord, Qt::Key_Return);
    CHECK(newWord->text().isEmpty());
    REQUIRE(words->count() == 2);
    newWord->setText(QStringLiteral("looked"));
    CHECK_FALSE(add->isEnabled());
    newWord->setText(QStringLiteral("two words"));
    CHECK_FALSE(add->isEnabled());
    newWord->clear();
    words->setCurrentRow(words->row(words->findItems(QStringLiteral("said"),
                                                     Qt::MatchExactly).first()));
    CHECK(remove->isEnabled());
    press(words, Qt::Key_Delete);
    REQUIRE(words->count() == 1);
    CHECK(words->item(0)->text() == QStringLiteral("looked"));

    distance->setValue(12);
    style->setCurrentIndex(style->findData(QStringLiteral("wave")));
    CHECK(page.isChanged());
    page.apply();
    CHECK(settings().get<int>(Coordinator::DISTANCE_KEY) == 12);
    CHECK(settings().get<std::string>(Coordinator::STYLE_KEY) == "wave");
    CHECK(settings().get<json>(Coordinator::SKIPPED_KEY) == json::array({"looked"}));

    // What counts, as the settings say now
    const editor::RepetitionOptions options = Coordinator::options(QStringLiteral("en_US"));
    CHECK(options.distance == 12);
    CHECK(options.skipped == QSet<QString>{QStringLiteral("looked")});
    CHECK(options.language == QStringLiteral("en_US"));
}
