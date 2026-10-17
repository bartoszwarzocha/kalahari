/// @file test_spelling_panel.cpp
/// @brief The Spelling and Grammar panel: the cards and the words they show, the colors of the
///        kinds, the counts and filters, what the panel says without cards, its keys; and with
///        the text in front: the issues listed and selected both ways, put right from the
///        panel, the grammar of the whole text, and Next Spelling or Grammar Issue showing the
///        panel only while the issues are looked through

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "../editor/fake_language_tool.h"
#include "kalahari/core/kind_words.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/grammar_check_service.h"
#include "kalahari/editor/spell_check_service.h"
#include "kalahari/editor/text_issue.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/grammar_coordinator.h"
#include "kalahari/gui/issue_colors.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/annotation_colors.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/spelling_issue_list.h"
#include "kalahari/gui/panels/spelling_panel.h"
#include "kalahari/gui/spelling_coordinator.h"
#include "kalahari/gui/spelling_panel_coordinator.h"
#include "kalahari/gui/utils/panel_toggle.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextDocument>
#include <QToolButton>

#include <nlohmann/json.hpp>

#include <cstdio>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace kalahari;
using namespace kalahari::gui;
using editor::TextIssueKind;
using kalahari::test::DICTIONARY_WAIT_MS;
using kalahari::test::FakeIssue;
using kalahari::test::FakeLanguageTool;
using kalahari::test::kmlOf;
using kalahari::test::waitUntil;

namespace {

/// How long an answer of the grammar server may take (ms)
constexpr int ANSWER_MS = 20000;

constexpr const char* NEEDS_OFFSCREEN =
    "needs an active window without a window on screen: run with QT_QPA_PLATFORM=offscreen";

/// The tests that give the keys to a part of a window need an active window; the offscreen
/// platform has one without a window on screen
bool canActivateWindows() {
    return QGuiApplication::platformName() == QStringLiteral("offscreen");
}

core::SettingsManager& settings() {
    return core::SettingsManager::getInstance();
}

/// Gives back the settings of the spelling, the grammar and the panel as they were before the
/// test
class KeptSettings {
public:
    KeptSettings() {
        for (const char* key : KEYS) {
            m_values[key] = settings().get<nlohmann::json>(key, nlohmann::json());
        }
    }
    ~KeptSettings() {
        try {
            for (const auto& [key, value] : m_values) {
                if (value.is_null()) {
                    settings().removeKey(key);
                } else {
                    settings().set<nlohmann::json>(key, value);
                }
            }
        } catch (...) {
            // Left as the test made them; the next tests set what they need
            std::fputs("The settings of the spelling panel could not be given back\n", stderr);
        }
    }
    KeptSettings(const KeptSettings&) = delete;
    KeptSettings& operator=(const KeptSettings&) = delete;

private:
    static constexpr const char* KEYS[] = {
        "editor.spellCheck.enabled",      "editor.spellCheck.language",
        "editor.grammarCheck.enabled",    "editor.grammarCheck.mode",
        "editor.grammarCheck.serverUrl",  "editor.grammarCheck.username",
        "editor.grammarCheck.apiKey",     "editor.grammarCheck.ignoredRules",
        "spellingPanel.pinned",           "spellingPanel.fixButtons",
        "spellingPanel.nextIssue",        "spellingPanel.grammarScope"};
    std::map<std::string, nlohmann::json> m_values;
};

/// The settings most tests start from: the spelling in American English, no grammar, the
/// panel as by default
void setDefaults() {
    settings().set<bool>("editor.spellCheck.enabled", true);
    settings().set<std::string>("editor.spellCheck.language", "en_US");
    settings().set<bool>(GrammarCoordinator::ENABLED_KEY, false);
    settings().set<bool>("spellingPanel.pinned", false);
    settings().set<std::string>("spellingPanel.fixButtons", "selected");
    settings().set<std::string>("spellingPanel.nextIssue", "panel");
    settings().set<std::string>("spellingPanel.grammarScope", "whole");
}

/// The program's commands while it lives (Next Spelling or Grammar Issue, the switches)
class Commands {
public:
    Commands() { registerAllCommands(CommandCallbacks{}); }
    ~Commands() { CommandRegistry::getInstance().clear(); }
    Commands(const Commands&) = delete;
    Commands& operator=(const Commands&) = delete;
};

/// A misspelled word of a paragraph, as the panel lists it
SpellingPanelIssue spellingIssue(int paragraph, const QString& text, const QString& word,
                                 int paragraphPosition) {
    SpellingPanelIssue listed;
    listed.issue.kind = TextIssueKind::Spelling;
    listed.issue.paragraph = paragraph;
    listed.issue.start = static_cast<int>(text.indexOf(word));
    listed.issue.length = static_cast<int>(word.size());
    listed.issue.text = word;
    listed.key = QStringLiteral("S::") + word + QStringLiteral("#0");
    listed.position = paragraphPosition + listed.issue.start;
    listed.number = paragraph + 1;
    listed.paragraphText = text;
    return listed;
}

/// A grammar issue of a paragraph, as the panel lists it
SpellingPanelIssue grammarIssue(int paragraph, const QString& text, const QString& phrase,
                                const QStringList& suggestions, int paragraphPosition) {
    SpellingPanelIssue listed = spellingIssue(paragraph, text, phrase, paragraphPosition);
    listed.issue.kind = TextIssueKind::Grammar;
    listed.issue.message = QStringLiteral("Use \"an\" instead of \"a\".");
    listed.issue.ruleId = QStringLiteral("EN_A_VS_AN");
    listed.issue.suggestions = suggestions;
    listed.key = QStringLiteral("G:EN_A_VS_AN:") + phrase + QStringLiteral("#0");
    return listed;
}

/// Two misspelled words and a grammar issue between them, in three paragraphs
std::vector<SpellingPanelIssue> sampleIssues() {
    return {spellingIssue(0, QStringLiteral("One two three four mistakke five."),
                          QStringLiteral("mistakke"), 0),
            grammarIssue(1, QStringLiteral("This is a apple."), QStringLiteral("a apple"),
                         {QStringLiteral("an apple")}, 34),
            spellingIssue(2, QStringLiteral("Two errorz here."), QStringLiteral("errorz"), 51)};
}

/// The keys of issues, in their order
QStringList keysOf(const std::vector<SpellingPanelIssue>& issues) {
    QStringList keys;
    for (const SpellingPanelIssue& issue : issues) {
        keys.append(issue.key);
    }
    return keys;
}

/// The issues listed by a panel: "spelling 2 errorz" (the kind, the paragraph, the text)
QStringList listed(const SpellingPanel& panel) {
    QStringList list;
    for (const SpellingPanelIssue& issue : panel.shownIssues()) {
        list.append(QStringLiteral("%1 %2 %3")
                        .arg(issue.issue.kind == TextIssueKind::Spelling
                                 ? QStringLiteral("spelling")
                                 : QStringLiteral("grammar"))
                        .arg(issue.number)
                        .arg(issue.issue.text));
    }
    return list;
}

/// Whether a panel shows a text (in a label it shows)
bool says(const QWidget& panel, const QString& text) {
    for (const QLabel* label : panel.findChildren<QLabel*>()) {
        if (label->text() == text && label->isVisibleTo(&panel)) {
            return true;
        }
    }
    return false;
}

/// The texts a panel shows in its labels, for the messages of failed checks
QStringList shownTexts(const QWidget& panel) {
    QStringList texts;
    for (const QLabel* label : panel.findChildren<QLabel*>()) {
        if (label->isVisibleTo(&panel) && !label->text().isEmpty()) {
            texts.append(label->text());
        }
    }
    return texts;
}

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

/// The button of a widget with a text
template <typename Button>
Button* buttonNamed(const QWidget& parent, const QString& text) {
    for (Button* button : parent.findChildren<Button*>()) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

/// The buttons of a card with their texts, in their order (only those it shows)
QStringList buttonTexts(const QList<QAbstractButton*>& buttons, const QWidget& card) {
    QStringList texts;
    for (const QAbstractButton* button : buttons) {
        if (button->isVisibleTo(&card)) {
            texts.append(button->text());
        }
    }
    return texts;
}

/// The words in their forms, as a package names a kind
core::KindWords chapterWords() {
    core::KindWords words;
    words.forms = {{QStringLiteral("singular"), QStringLiteral("chapter")},
                   {QStringLiteral("plural"), QStringLiteral("chapters")},
                   {QStringLiteral("indefinite"), QStringLiteral("a chapter")}};
    return words;
}

/// Title of the tab on top of the tab group holding @p dock (empty without a group)
QString raisedTab(const QMainWindow& window, const QDockWidget* dock) {
    for (const QTabBar* tabBar : window.findChildren<QTabBar*>()) {
        for (int i = 0; i < tabBar->count(); ++i) {
            if (tabBar->tabText(i) == dock->windowTitle()) {
                return tabBar->tabText(tabBar->currentIndex());
            }
        }
    }
    return {};
}

/// The window of the coordinator's tests, with its parts as MainWindow has them: the
/// documents in the middle; on the right two panels in tabs (stand-ins for Properties and
/// Annotations) and the Spelling and Grammar panel, closed, in a tab next to them
struct Desk {
    // Not the window's own: see test_dock_layout.cpp (QTBUG-137524)
    QStatusBar statusBar;
    QMainWindow window;
    QTabWidget* tabs = nullptr;
    QDockWidget* properties = nullptr;
    QDockWidget* annotations = nullptr;
    QDockWidget* dock = nullptr;
    SpellingPanel* panel = nullptr;
    QToolButton* pin = nullptr;
    std::unique_ptr<SpellingCoordinator> spelling;
    std::unique_ptr<GrammarCoordinator> grammar;
    std::unique_ptr<SpellingPanelCoordinator> coordinator;

    explicit Desk(bool withGrammar = false) {
        tabs = new QTabWidget();
        window.setCentralWidget(tabs);
        properties = addPanel(QStringLiteral("Properties"), new QLabel(QStringLiteral("-")));
        annotations = addPanel(QStringLiteral("Annotations"), new QLabel(QStringLiteral("-")));
        panel = new SpellingPanel();
        dock = addPanel(QStringLiteral("Spelling and Grammar"), panel);
        window.tabifyDockWidget(properties, annotations);
        window.tabifyDockWidget(annotations, dock);
        dock->hide();
        annotations->raise();
        pin = new QToolButton(&window);  // the pin of the panel's title bar

        spelling = std::make_unique<SpellingCoordinator>(tabs, &statusBar);
        spelling->connectCommands();
        if (withGrammar) {
            grammar = std::make_unique<GrammarCoordinator>(tabs, nullptr);
        }
        coordinator = std::make_unique<SpellingPanelCoordinator>(panel, dock, tabs, spelling.get(),
                                                                 grammar.get());
        coordinator->setPinButton(pin);
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
    editor::BookEditor* open(const QStringList& paragraphs) {
        auto* document = new EditorPanel();
        document->setContent(kmlOf(paragraphs));
        tabs->setCurrentIndex(tabs->addTab(document, QStringLiteral("Document")));
        return document->getBookEditor();
    }

    /// Open the panel as View > Panels does: its tab comes on top
    void openPanel() {
        REQUIRE_FALSE(utils::isPanelOpen(dock));
        utils::togglePanel(dock);
        QApplication::processEvents();
    }

    /// Run the event loop until the panel lists these issues
    bool lists(const QStringList& issues) const {
        const SpellingPanel* shown = panel;
        return waitUntil([shown, &issues]() { return listed(*shown) == issues; },
                         DICTIONARY_WAIT_MS);
    }

    /// Give the window the keys
    void activate() {
        window.activateWindow();
        REQUIRE(waitUntil([this]() { return QApplication::activeWindow() == &window; }));
    }

    /// Next Spelling or Grammar Issue (F7)
    static void nextIssue() {
        QAction* next = CommandRegistry::getInstance().getAction(std::string("tools.nextMisspelling"));
        REQUIRE(next != nullptr);
        next->trigger();
    }
};

/// The text of a paragraph of an editor
QString paragraphText(const editor::BookEditor& editor, int paragraph) {
    return editor.textDocument()->findBlockByNumber(paragraph).text();
}

}  // namespace

// =============================================================================
// The cards
// =============================================================================

TEST_CASE("Spelling panel: a folded card shows a few words around the issue, an unfolded one "
          "its sentence",
          "[gui][spelling-panel]") {
    const QString paragraph =
        QStringLiteral("He said so. One two three four mistakke five six seven eight nine. And more!");
    const int start = static_cast<int>(paragraph.indexOf(QStringLiteral("mistakke")));

    // Three words before it and four after it, with an ellipsis where the paragraph goes on
    const IssueContext few = shortContext(paragraph, start, 8);
    CHECK(few.before == QStringLiteral("…two three four "));
    CHECK(few.after == QStringLiteral(" five six seven eight…"));

    // From the start of its sentence to its end
    const IssueContext sentence = sentenceContext(paragraph, start, 8);
    CHECK(sentence.before == QStringLiteral("One two three four "));
    CHECK(sentence.after == QStringLiteral(" five six seven eight nine."));

    // At the start of a paragraph nothing comes before it, and a short paragraph is whole
    const QString shortOne = QStringLiteral("Mistakke here.");
    const IssueContext first = shortContext(shortOne, 0, 8);
    CHECK(first.before.isEmpty());
    CHECK(first.after == QStringLiteral(" here."));

    // A question in a quotation ends with its marks
    const QString quoted = QStringLiteral("She asked: \"Is it rihgt?\" Then she left.");
    const IssueContext asked =
        sentenceContext(quoted, static_cast<int>(quoted.indexOf(QStringLiteral("rihgt"))), 5);
    CHECK(asked.before == QStringLiteral("She asked: \"Is it "));
    CHECK(asked.after == QStringLiteral("?\""));
}

TEST_CASE("Spelling panel: the cards of a kind are framed in its color, every text readable",
          "[gui][spelling-panel]") {
    const QColor kind(QStringLiteral("#d32f2f"));
    for (const auto& [surface, text] :
         {std::pair{QColor(QStringLiteral("#ffffff")), QColor(QStringLiteral("#1f1f1f"))},
          std::pair{QColor(QStringLiteral("#1e1e1e")), QColor(QStringLiteral("#e8e8e8"))}}) {
        CAPTURE(surface.name());
        const IssueCardColors colors = issueCardColors(kind, surface, text);
        CHECK(colors.border == kind);
        CHECK(colors.card.kind == kind);
        CHECK(colors.card.background != surface);  // tinted with the kind
        CHECK(contrastRatio(colors.card.text, colors.card.background) >= MIN_TEXT_CONTRAST);
        CHECK(contrastRatio(colors.card.kindName, colors.card.background) >= MIN_TEXT_CONTRAST);
        CHECK(contrastRatio(colors.card.secondary, colors.card.background) >= MIN_TEXT_CONTRAST);
    }
}

TEST_CASE("Spelling panel: a card unfolded shows the suggestions and the actions of its kind",
          "[gui][spelling-panel]") {
    const std::vector<SpellingPanelIssue> issues = sampleIssues();
    SpellingIssueCard card;
    card.resize(320, 200);
    card.show();

    // A misspelled word: folded, the kind, the paragraph and the words around it
    card.setIssue(issues.at(0));
    card.setSuggestions({QStringLiteral("mistake"), QStringLiteral("mistakes")});
    CHECK_FALSE(card.isExpanded());
    CHECK(says(card, QStringLiteral("Spelling")));
    CHECK(says(card, QStringLiteral("paragraph 1")));
    CHECK(buttonTexts(card.buttons(), card).isEmpty());

    // Unfolded: its sentence, the suggestions, Ignore Word and Add to Dictionary
    card.setExpanded(true);
    CHECK(card.contextText() == QStringLiteral("One two three four mistakke five."));
    CHECK(buttonTexts(card.suggestionButtons(), card) ==
          QStringList{QStringLiteral("mistake"), QStringLiteral("mistakes")});
    CHECK(buttonTexts(card.actionButtons(), card) ==
          QStringList{QStringLiteral("Ignore Word"), QStringLiteral("Add to Dictionary")});

    // Its buttons are reached with Tab only while it is selected
    CHECK(card.suggestionButtons().first()->focusPolicy() == Qt::NoFocus);
    card.setSelected(true);
    CHECK(card.suggestionButtons().first()->focusPolicy() == Qt::StrongFocus);

    // A text only read: the suggestions are greyed out, ignoring still works
    card.setReadOnly(true);
    CHECK_FALSE(card.suggestionButtons().first()->isEnabled());
    CHECK_FALSE(card.suggestionButtons().first()->toolTip().isEmpty());
    CHECK(card.actionButtons().first()->isEnabled());
    card.setReadOnly(false);
    CHECK(card.suggestionButtons().first()->isEnabled());

    // A grammar issue: what is wrong, its replacements and Ignore Rule
    card.setIssue(issues.at(1));
    card.setSuggestions(issues.at(1).issue.suggestions);
    CHECK(says(card, QStringLiteral("Grammar")));
    CHECK(says(card, QStringLiteral("Use \"an\" instead of \"a\".")));
    CHECK(card.contextText() == QStringLiteral("This is a apple."));
    CHECK(buttonTexts(card.suggestionButtons(), card) == QStringList{QStringLiteral("an apple")});
    CHECK(buttonTexts(card.actionButtons(), card) == QStringList{QStringLiteral("Ignore Rule")});

    // No suggestions: the card says so
    card.setSuggestions({});
    CHECK(says(card, QStringLiteral("(No suggestions)")));

    // What it reports
    int chosen = -1;
    int ignoredRules = 0;
    QObject::connect(&card, &SpellingIssueCard::suggestionChosen,
                     [&chosen](int index) { chosen = index; });
    QObject::connect(&card, &SpellingIssueCard::ignoreRuleClicked,
                     [&ignoredRules]() { ++ignoredRules; });
    card.setSuggestions(issues.at(1).issue.suggestions);
    card.suggestionButtons().first()->click();
    CHECK(chosen == 0);
    card.actionButtons().first()->click();
    CHECK(ignoredRules == 1);
}

TEST_CASE("Spelling panel: only the cards in view are made, and the selected one unfolded",
          "[gui][spelling-panel]") {
    // Hundreds of issues, a paragraph each
    std::vector<SpellingPanelIssue> issues;
    for (int i = 0; i < 500; ++i) {
        SpellingPanelIssue issue = spellingIssue(i, QStringLiteral("A mistakke here."),
                                                 QStringLiteral("mistakke"), i * 17);
        issue.key = QStringLiteral("S::mistakke#%1").arg(i);
        issues.push_back(issue);
    }
    SpellingIssueList list;
    list.resize(300, 400);
    list.show();
    int asked = 0;
    list.setSuggestionProvider([&asked](const SpellingPanelIssue& /*issue*/) {
        ++asked;
        return QStringList{QStringLiteral("mistake"), QStringLiteral("mistakes")};
    });
    list.setIssues(issues);
    REQUIRE(list.count() == 500);

    // The first cards, folded; no suggestions asked for them
    const auto made = [&list]() { return list.findChildren<SpellingIssueCard*>().size(); };
    CHECK(made() < 40);
    REQUIRE(list.card(issues.front().key) != nullptr);
    CHECK_FALSE(list.card(issues.front().key)->isExpanded());
    CHECK(list.card(issues.back().key) == nullptr);
    CHECK(asked == 0);

    // The last one selected: made, in view, unfolded, with its suggestions numbered for keys
    list.setSelectedKey(issues.back().key);
    SpellingIssueCard* last = list.selectedCard();
    REQUIRE(last != nullptr);
    CHECK(last->issue().key == issues.back().key);
    CHECK(last->isSelected());
    CHECK(last->isExpanded());
    CHECK(list.viewport()->rect().contains(last->geometry()));
    CHECK(last->suggestions() == QStringList{QStringLiteral("mistake"), QStringLiteral("mistakes")});
    CHECK(asked == 1);
    CHECK(list.card(issues.front().key) == nullptr);
    CHECK(made() < 40);

    // The suggestions are asked once while the issue is listed, again when the dictionary
    // changes
    list.setSelectedKey(issues.front().key);
    list.setSelectedKey(issues.back().key);
    CHECK(asked == 2);
    list.forgetSuggestions();
    CHECK(asked == 3);

    // The buttons on every card: each card in view is unfolded
    list.setExpandedAll(true);
    const QList<SpellingIssueCard*> cards = list.findChildren<SpellingIssueCard*>();
    int inView = 0;
    for (const SpellingIssueCard* card : cards) {
        if (card->isVisible()) {
            ++inView;
            CHECK(card->isExpanded());
        }
    }
    CHECK(inView > 0);
    CHECK(made() < 40);

    // A page down from the first card goes past the cards a page holds
    list.setExpandedAll(false);
    const int pageDown = list.pageStepFrom(0, 1);
    CHECK(pageDown > 1);
    CHECK(pageDown < 40);
    CHECK(list.pageStepFrom(499, 1) == 499);
    CHECK(list.pageStepFrom(0, -1) == 0);
}

// =============================================================================
// The panel
// =============================================================================

TEST_CASE("Spelling panel: the buttons of the kinds count the issues and show or hide them",
          "[gui][spelling-panel]") {
    SpellingPanel panel;
    panel.resize(320, 480);
    panel.show();
    panel.setContent(SpellingPanel::Content::Issues);
    panel.setKindChecked(TextIssueKind::Spelling, true);
    panel.setKindChecked(TextIssueKind::Grammar, true);
    const std::vector<SpellingPanelIssue> issues = sampleIssues();
    panel.setIssues(issues);
    QToolButton* spellingButton = panel.kindButton(TextIssueKind::Spelling);
    QToolButton* grammarButton = panel.kindButton(TextIssueKind::Grammar);
    CHECK(spellingButton->text() == QStringLiteral("Spelling 2"));
    CHECK(grammarButton->text() == QStringLiteral("Grammar 1"));
    CHECK(keysOf(panel.shownIssues()) == keysOf(issues));

    // The grammar hidden: its button still counts it
    grammarButton->click();
    CHECK_FALSE(panel.isKindShown(TextIssueKind::Grammar));
    CHECK(keysOf(panel.shownIssues()) == QStringList{issues.at(0).key, issues.at(2).key});
    CHECK(grammarButton->text() == QStringLiteral("Grammar 1"));

    // Both hidden: no card matches
    spellingButton->click();
    CHECK(panel.shownIssues().empty());
    CHECK(says(panel, QStringLiteral("No issue matches the filters.")));

    // Shown again; then the grammar not checked: "–" on its greyed out button, no card of it
    spellingButton->click();
    grammarButton->click();
    CHECK(keysOf(panel.shownIssues()) == keysOf(issues));
    panel.setKindChecked(TextIssueKind::Grammar, false);
    CHECK(grammarButton->text() == QStringLiteral("Grammar –"));
    CHECK_FALSE(grammarButton->isEnabled());
    CHECK(keysOf(panel.shownIssues()) == QStringList{issues.at(0).key, issues.at(2).key});

    // The row under the buttons: how far the text is checked, or why it is not
    panel.setStatus(QStringLiteral("Checking the spelling: paragraph 2 of 3..."), false);
    CHECK(says(panel, QStringLiteral("Checking the spelling: paragraph 2 of 3...")));
    panel.setStatus(QString(), false);
    CHECK_FALSE(says(panel, QStringLiteral("Checking the spelling: paragraph 2 of 3...")));
}

TEST_CASE("Spelling panel: without cards it says what to open, how to turn the checking on, "
          "or that the text has no issues",
          "[gui][spelling-panel]") {
    SpellingPanel panel;
    panel.resize(320, 480);
    panel.show();
    auto* turnOn = buttonNamed<QPushButton>(panel, QStringLiteral("Turn On Checking"));
    REQUIRE(turnOn != nullptr);

    // Nothing in front: what to open
    panel.setContent(SpellingPanel::Content::NoText);
    panel.setKindNames(std::nullopt, std::nullopt, false);
    CHECK(says(panel, QStringLiteral("Open a book or a file to see the spelling and grammar issues.")));
    panel.setKindNames(std::nullopt, std::nullopt, true);
    CHECK(says(panel, QStringLiteral("Open an element of the book or a file to see its spelling "
                                     "and grammar issues.")));
    panel.setKindNames(std::nullopt, chapterWords(), true);
    CHECK(says(panel,
               QStringLiteral("Open a chapter or a file to see its spelling and grammar issues.")));
    CHECK_FALSE(turnOn->isVisibleTo(&panel));
    CHECK_FALSE(panel.kindButton(TextIssueKind::Spelling)->isVisibleTo(&panel));

    // Nothing checked: the button turns the checking on
    panel.setContent(SpellingPanel::Content::NotChecked);
    CHECK(says(panel, QStringLiteral("Spelling and grammar are not checked.")));
    REQUIRE(turnOn->isVisibleTo(&panel));
    int turnedOn = 0;
    QObject::connect(&panel, &SpellingPanel::turnOnCheckingRequested,
                     [&turnedOn]() { ++turnedOn; });
    turnOn->click();
    CHECK(turnedOn == 1);

    // No issues yet, or none in the whole text
    panel.setContent(SpellingPanel::Content::Issues);
    panel.setKindChecked(TextIssueKind::Spelling, true);
    panel.setKindChecked(TextIssueKind::Grammar, true);
    panel.setIssues({});
    panel.setKindNames(chapterWords(), chapterWords(), true);
    panel.setComplete(false);
    CAPTURE(shownTexts(panel));
    CHECK(says(panel, QStringLiteral("No issues found yet.")));
    CHECK_FALSE(turnOn->isVisibleTo(&panel));
    panel.setComplete(true);
    CHECK(says(panel, QStringLiteral("No spelling or grammar issues in this chapter.")));
    panel.setKindChecked(TextIssueKind::Grammar, false);
    CHECK(says(panel, QStringLiteral("No misspelled words in this chapter.")));
    panel.setKindChecked(TextIssueKind::Grammar, true);
    panel.setKindChecked(TextIssueKind::Spelling, false);
    CHECK(says(panel, QStringLiteral("No grammar issues in this chapter.")));
    panel.setKindChecked(TextIssueKind::Spelling, true);
    panel.setKindNames(fileWords(), std::nullopt, false);
    CHECK(says(panel, QStringLiteral("No spelling or grammar issues in this file.")));
}

TEST_CASE("Spelling panel: its keys select the issues and fix the selected one, ahead of the "
          "window's shortcuts",
          "[gui][spelling-panel]") {
    SpellingPanel panel;
    panel.resize(320, 480);
    panel.show();
    panel.setContent(SpellingPanel::Content::Issues);
    panel.setKindChecked(TextIssueKind::Spelling, true);
    panel.setKindChecked(TextIssueKind::Grammar, true);
    panel.setSuggestionProvider([](const SpellingPanelIssue& issue) {
        return issue.issue.text == QStringLiteral("mistakke")
                   ? QStringList{QStringLiteral("mistake"), QStringLiteral("mistakes")}
                   : QStringList{QStringLiteral("errors")};
    });
    const std::vector<SpellingPanelIssue> issues = sampleIssues();
    panel.setIssues(issues);

    QStringList selected;
    QStringList chosen;
    int menus = 0;
    int backToText = 0;
    QObject::connect(&panel, &SpellingPanel::issueSelected,
                     [&selected](const SpellingPanelIssue& issue) { selected.append(issue.key); });
    QObject::connect(&panel, &SpellingPanel::suggestionChosen,
                     [&chosen](const SpellingPanelIssue& issue, const QString& suggestion) {
                         chosen.append(issue.issue.text + QStringLiteral(" > ") + suggestion);
                     });
    QObject::connect(&panel, &SpellingPanel::menuRequested,
                     [&menus](const SpellingPanelIssue& /*issue*/, const QPoint& /*pos*/) {
                         ++menus;
                     });
    QObject::connect(&panel, &SpellingPanel::editorFocusRequested,
                     [&backToText]() { ++backToText; });

    // The keys go from the list, where they are, to the panel: not to the window's shortcuts
    QWidget* list = panel.list();
    for (const int key : {Qt::Key_Up, Qt::Key_Down, Qt::Key_Home, Qt::Key_End, Qt::Key_PageUp,
                          Qt::Key_PageDown, Qt::Key_Return, Qt::Key_Enter, Qt::Key_Escape,
                          Qt::Key_1, Qt::Key_5}) {
        CAPTURE(key);
        CHECK(takesKey(list, key));
    }
    CHECK(takesKey(list, Qt::Key_F10, Qt::ShiftModifier));
    CHECK_FALSE(takesKey(list, Qt::Key_F7));
    CHECK_FALSE(takesKey(list, Qt::Key_6));
    CHECK_FALSE(takesKey(list, Qt::Key_Z, Qt::ControlModifier));

    // Down, End, Home and Up select the issues, and the panel reports each one selected
    press(list, Qt::Key_Down);
    press(list, Qt::Key_Down);
    press(list, Qt::Key_End);
    press(list, Qt::Key_Home);
    press(list, Qt::Key_Up);  // already the first
    CHECK(selected == QStringList{issues.at(0).key, issues.at(1).key, issues.at(2).key,
                                  issues.at(0).key});
    CHECK(panel.selectedKey() == issues.at(0).key);

    // Enter: the first suggestion; a digit: the suggestion with that number, when it has one
    press(list, Qt::Key_Return);
    press(list, Qt::Key_2, Qt::NoModifier, QStringLiteral("2"));
    press(list, Qt::Key_5, Qt::NoModifier, QStringLiteral("5"));
    CHECK(chosen == QStringList{QStringLiteral("mistakke > mistake"),
                                QStringLiteral("mistakke > mistakes")});

    // A grammar issue's replacement
    press(list, Qt::Key_Down);
    press(list, Qt::Key_Enter);
    CHECK(chosen.last() == QStringLiteral("a apple > an apple"));

    // Shift+F10: the menu of the selected issue; Esc: back to the text
    press(list, Qt::Key_F10, Qt::ShiftModifier);
    CHECK(menus == 1);
    press(list, Qt::Key_Escape);
    CHECK(backToText == 1);

    // A text only read keeps its words
    panel.setReadOnly(true);
    press(list, Qt::Key_Return);
    CHECK(chosen.size() == 3);

    // Previous and Next do what Up and Down do
    auto* next = buttonNamed<QToolButton>(panel, QStringLiteral("Next"));
    REQUIRE(next != nullptr);
    next->click();
    CHECK(panel.selectedKey() == issues.at(2).key);
    CHECK_FALSE(next->isEnabled());  // the last one
}

TEST_CASE("Spelling panel: the frames of the cards follow the color of the waves in Settings",
          "[gui][spelling-panel][colors]") {
    auto& themes = core::ThemeManager::getInstance();
    const std::string setting = core::ThemeManager::editorColorSetting(
        themes.getCurrentTheme().name, editor::issueColorKey(TextIssueKind::Spelling));
    const std::string optimize = core::ThemeManager::OPTIMIZE_COLORS_SETTING;
    const auto clear = [&setting, &optimize]() {
        settings().removeKey(setting);
        settings().removeKey(optimize);
        QApplication::processEvents();  // the theme follows the settings
    };
    clear();

    SpellingPanel panel;
    panel.resize(320, 480);
    panel.show();
    panel.setContent(SpellingPanel::Content::Issues);
    panel.setKindChecked(TextIssueKind::Spelling, true);
    panel.setKindChecked(TextIssueKind::Grammar, true);
    const std::vector<SpellingPanelIssue> issues = sampleIssues();
    panel.setIssues(issues);
    QApplication::processEvents();

    // The color at the left edge of a folded card: its bar and its frame, in the kind's color
    const auto edgeColor = [&panel, &issues]() {
        SpellingIssueCard* card = panel.list()->card(issues.at(0).key);
        if (card == nullptr) {
            return QColor();
        }
        const QImage image = card->grab().toImage();
        const qreal ratio = image.devicePixelRatio();
        return QColor::fromRgb(image.pixel(qRound(2 * ratio), image.height() / 2));
    };
    // What the cards lie on
    const QColor base = themes.getCurrentTheme().palette.base.isValid()
                            ? themes.getCurrentTheme().palette.base
                            : panel.palette().color(QPalette::Base);

    // A color seen well on the panel: the cards of the kind are framed in it
    const QColor chosen(QStringLiteral("#d000d0"));
    REQUIRE(contrastRatio(chosen, base) >= MIN_MARK_CONTRAST);
    settings().set<std::string>(setting, chosen.name().toStdString());
    CHECK(waitUntil([&edgeColor, &chosen]() { return edgeColor() == chosen; }));

    // A color hardly seen on the panel: made visible, or as set with the colors not optimized
    const QColor faint = mixedColor(base, chosen, 0.15);
    const QColor visible = shownColor(faint, base, true);
    REQUIRE(visible != faint);
    settings().set<std::string>(setting, faint.name().toStdString());
    CHECK(waitUntil([&edgeColor, &visible]() { return edgeColor() == visible; }));
    settings().set<bool>(optimize, false);
    CHECK(waitUntil([&edgeColor, &faint]() { return edgeColor() == faint; }));

    // Back to the theme's color
    clear();
    const QColor themeColor = issueColorOn(TextIssueKind::Spelling, base);
    CHECK(waitUntil([&edgeColor, &themeColor]() { return edgeColor() == themeColor; }));
}

// =============================================================================
// The panel and the text in front
// =============================================================================

TEST_CASE("Spelling panel: the misspelled words of the text in front, selected in the panel "
          "and in the text",
          "[gui][spelling-panel][coordinator]") {
    KeptSettings kept;
    setDefaults();
    REQUIRE(core::ProjectManager::getInstance().closeProject(false));
    Commands commands;
    Desk desk;

    // No text in front
    desk.openPanel();
    CHECK(desk.panel->content() == SpellingPanel::Content::NoText);
    CHECK(says(*desk.panel,
               QStringLiteral("Open a book or a file to see the spelling and grammar issues.")));

    // A file in front: its misspelled words in the order of the text, numbered by the
    // paragraphs with text
    editor::BookEditor* editor = desk.open({QStringLiteral("One mistakke here."), QString(),
                                            QStringLiteral("Two errorz and a mistakke.")});
    REQUIRE(desk.lists({QStringLiteral("spelling 1 mistakke"), QStringLiteral("spelling 2 errorz"),
                        QStringLiteral("spelling 2 mistakke")}));
    const std::vector<SpellingPanelIssue> issues = desk.panel->shownIssues();
    CHECK(issues.at(0).key != issues.at(2).key);  // the same word twice
    CHECK(desk.panel->kindButton(TextIssueKind::Spelling)->text() == QStringLiteral("Spelling 3"));
    CHECK(desk.panel->kindButton(TextIssueKind::Grammar)->text() == QStringLiteral("Grammar –"));
    CHECK(waitUntil([&desk]() {
        return desk.panel->statusText() ==
               QStringLiteral("The whole file is checked. The grammar is not checked.");
    }));
    CHECK(says(*desk.panel, QStringLiteral("F7 – next issue (the keys go to the panel)")));

    // An issue selected in the panel is selected in the text
    press(desk.panel->list(), Qt::Key_Down);
    CHECK(editor->selectedText() == QStringLiteral("mistakke"));
    CHECK(editor->cursorPosition().paragraph == 0);
    press(desk.panel->list(), Qt::Key_End);
    CHECK(editor->selectedText() == QStringLiteral("mistakke"));
    CHECK(editor->cursorPosition().paragraph == 2);

    // A click on an issue in the text selects its card
    editor->setCursorPosition({2, 6});  // in "errorz"
    CHECK(desk.panel->selectedKey() == issues.at(1).key);

    // The settings of the panel
    settings().set<std::string>("spellingPanel.fixButtons", "every");
    CHECK(waitUntil([&desk]() { return desk.panel->list()->isExpandedAll(); }));
    settings().set<std::string>("spellingPanel.nextIssue", "menu");
    CHECK(waitUntil([&desk]() {
        return says(*desk.panel, QStringLiteral("F7 – next issue, with its menu at the word"));
    }));

    // A language without a dictionary: the row under the counts says why nothing is listed
    settings().set<std::string>("editor.spellCheck.language", "xx");
    CHECK(waitUntil(
        [&desk]() {
            return desk.panel->statusText() ==
                   QStringLiteral("No spelling dictionary for xx: the spelling is not checked.");
        },
        DICTIONARY_WAIT_MS));
    CHECK(desk.panel->kindButton(TextIssueKind::Spelling)->text() ==
          QStringLiteral("Spelling –"));
    CHECK(desk.panel->shownIssues().empty());
}

TEST_CASE("Spelling panel: a fix chosen in the panel goes to the text, and the next issue is "
          "selected",
          "[gui][spelling-panel][coordinator]") {
    KeptSettings kept;
    setDefaults();
    REQUIRE(core::ProjectManager::getInstance().closeProject(false));
    Commands commands;
    Desk desk;
    desk.openPanel();
    editor::BookEditor* editor =
        desk.open({QStringLiteral("One mistakke here."), QStringLiteral("Two errorz there.")});
    REQUIRE(desk.lists({QStringLiteral("spelling 1 mistakke"), QStringLiteral("spelling 2 errorz")}));
    QWidget* list = desk.panel->list();

    // The first suggestion takes the word's place; the issue after it is selected
    press(list, Qt::Key_Home);
    const std::optional<SpellingPanelIssue> first = desk.panel->selectedIssue();
    REQUIRE(first.has_value());
    const QStringList suggestions = desk.panel->list()->suggestionsFor(*first);
    REQUIRE_FALSE(suggestions.isEmpty());
    press(list, Qt::Key_Return);
    CHECK(paragraphText(*editor, 0) ==
          QStringLiteral("One %1 here.").arg(suggestions.first()));
    REQUIRE(desk.panel->selectedIssue().has_value());
    CHECK(desk.panel->selectedIssue()->issue.text == QStringLiteral("errorz"));
    CHECK(editor->selectedText() == QStringLiteral("errorz"));
    CHECK(desk.lists({QStringLiteral("spelling 2 errorz")}));

    // One step of Undo brings the word back, and its card
    editor->undo();
    CHECK(paragraphText(*editor, 0) == QStringLiteral("One mistakke here."));
    CHECK(desk.lists({QStringLiteral("spelling 1 mistakke"), QStringLiteral("spelling 2 errorz")}));

    // Ignore Word: the word is right in the text, and the issue after it is selected
    desk.panel->selectIssue(desk.panel->shownIssues().at(0).key);
    SpellingIssueCard* card = desk.panel->list()->selectedCard();
    REQUIRE(card != nullptr);
    REQUIRE(card->actionButtons().first()->text() == QStringLiteral("Ignore Word"));
    card->actionButtons().first()->click();
    CHECK(desk.spelling->service()->isIgnored(QStringLiteral("mistakke")));
    CHECK(desk.lists({QStringLiteral("spelling 2 errorz")}));
    REQUIRE(desk.panel->selectedIssue().has_value());
    CHECK(desk.panel->selectedIssue()->issue.text == QStringLiteral("errorz"));
    CHECK(paragraphText(*editor, 0) == QStringLiteral("One mistakke here."));

    // A text only read: the suggestions are greyed out, and Enter changes nothing
    editor->setReadOnly(true);
    desk.coordinator->refresh();
    press(list, Qt::Key_Return);
    CHECK(paragraphText(*editor, 1) == QStringLiteral("Two errorz there."));
}

TEST_CASE("Spelling panel: with the checking off it turns it on", "[gui][spelling-panel][coordinator]") {
    KeptSettings kept;
    setDefaults();
    settings().set<bool>("editor.spellCheck.enabled", false);
    REQUIRE(core::ProjectManager::getInstance().closeProject(false));
    Commands commands;
    Desk desk;
    desk.openPanel();
    desk.open({QStringLiteral("One mistakke here.")});
    CHECK(waitUntil([&desk]() {
        return desk.panel->content() == SpellingPanel::Content::NotChecked;
    }));
    auto* turnOn = buttonNamed<QPushButton>(*desk.panel, QStringLiteral("Turn On Checking"));
    REQUIRE(turnOn != nullptr);
    REQUIRE(turnOn->isVisibleTo(desk.panel));
    turnOn->click();
    CHECK(settings().get<bool>("editor.spellCheck.enabled", false));
    CHECK(desk.lists({QStringLiteral("spelling 1 mistakke")}));
    CHECK(desk.panel->content() == SpellingPanel::Content::Issues);
}

TEST_CASE("Spelling panel: the grammar issues with the misspelled words, the whole text checked "
          "while the panel shows",
          "[gui][spelling-panel][coordinator][grammar]") {
    KeptSettings kept;
    setDefaults();
    FakeLanguageTool languageTool;
    languageTool.addIssue(FakeIssue(QStringLiteral("a apple"), QStringLiteral("EN_A_VS_AN"),
                                    QStringLiteral("Use \"an\" instead of \"a\"."),
                                    {QStringLiteral("an apple")}));
    settings().set<bool>(GrammarCoordinator::ENABLED_KEY, true);
    settings().set<std::string>(GrammarCoordinator::MODE_KEY, GrammarCoordinator::REMOTE_MODE);
    settings().set<std::string>(GrammarCoordinator::SERVER_KEY, languageTool.url().toStdString());
    settings().set<std::string>(GrammarCoordinator::USERNAME_KEY, std::string());
    settings().set<std::string>(GrammarCoordinator::API_KEY_KEY, std::string());
    settings().set<nlohmann::json>(GrammarCoordinator::IGNORED_RULES_KEY, nlohmann::json::object());
    REQUIRE(core::ProjectManager::getInstance().closeProject(false));
    Commands commands;
    Desk desk(true);

    editor::BookEditor* editor = desk.open({QStringLiteral("One mistakke is a apple.")});
    CHECK_FALSE(editor->isWholeTextGrammar());  // the panel is closed

    // Shown: the grammar of the whole text, its issues in the order of the text
    desk.openPanel();
    CHECK(waitUntil([editor]() { return editor->isWholeTextGrammar(); }));
    REQUIRE(waitUntil(
        [&desk]() {
            return listed(*desk.panel) == QStringList{QStringLiteral("spelling 1 mistakke"),
                                                      QStringLiteral("grammar 1 a apple")};
        },
        ANSWER_MS + DICTIONARY_WAIT_MS));
    CHECK(desk.panel->kindButton(TextIssueKind::Grammar)->text() == QStringLiteral("Grammar 1"));
    CHECK(waitUntil([&desk]() {
        return desk.panel->statusText() == QStringLiteral("The whole file is checked.");
    }, ANSWER_MS));

    // Only around the part in view, as set
    settings().set<std::string>("spellingPanel.grammarScope", "nearby");
    CHECK(waitUntil([editor]() { return !editor->isWholeTextGrammar(); }));
    CHECK(waitUntil([&desk]() {
        return desk.panel->statusText() ==
               QStringLiteral("The spelling of the whole file is checked, the grammar only "
                              "around the part in view.");
    }));
    settings().set<std::string>("spellingPanel.grammarScope", "whole");
    CHECK(waitUntil([editor]() { return editor->isWholeTextGrammar(); }));

    // Hidden: only around the part in view again
    desk.dock->hide();
    CHECK(waitUntil([editor]() { return !editor->isWholeTextGrammar(); }));
    desk.openPanel();
    CHECK(waitUntil([editor]() { return editor->isWholeTextGrammar(); }));

    // A replacement of the grammar issue goes to the text
    const auto grammarKey = [&desk]() {
        for (const SpellingPanelIssue& issue : desk.panel->shownIssues()) {
            if (issue.issue.kind == TextIssueKind::Grammar) {
                return issue.key;
            }
        }
        return QString();
    };
    REQUIRE_FALSE(grammarKey().isEmpty());
    desk.panel->selectIssue(grammarKey());

    SECTION("its replacement") {
        press(desk.panel->list(), Qt::Key_Return);
        CHECK(paragraphText(*editor, 0) == QStringLiteral("One mistakke is an apple."));
    }

    SECTION("Ignore Rule: its issues leave the text") {
        SpellingIssueCard* card = desk.panel->list()->selectedCard();
        REQUIRE(card != nullptr);
        REQUIRE(card->actionButtons().first()->text() == QStringLiteral("Ignore Rule"));
        card->actionButtons().first()->click();
        CHECK(desk.lists({QStringLiteral("spelling 1 mistakke")}));
        CHECK(desk.panel->kindButton(TextIssueKind::Grammar)->text() ==
              QStringLiteral("Grammar 0"));
    }
}

TEST_CASE("Spelling panel: Next Spelling or Grammar Issue shows the panel only while the issues "
          "are looked through",
          "[gui][spelling-panel][coordinator]") {
    if (!canActivateWindows()) {
        SKIP(NEEDS_OFFSCREEN);
    }
    KeptSettings kept;
    setDefaults();
    REQUIRE(core::ProjectManager::getInstance().closeProject(false));
    Commands commands;
    Desk desk;
    editor::BookEditor* editor =
        desk.open({QStringLiteral("One mistakke here."), QStringLiteral("Two errorz there.")});
    desk.activate();
    REQUIRE(waitUntil([editor]() { return editor->textIssues().size() == 2; },
                      DICTIONARY_WAIT_MS));
    editor->setFocus();
    editor->setCursorPosition({0, 0});
    REQUIRE(waitUntil([editor]() { return editor->hasFocus(); }));

    SECTION("a closed panel shows for the issue, takes the keys, and Esc hides it again") {
        desk.properties->raise();  // on top of the tabs the panel comes into
        QApplication::processEvents();
        REQUIRE(raisedTab(desk.window, desk.annotations) == QStringLiteral("Properties"));

        Desk::nextIssue();
        CHECK(desk.dock->isVisible());
        CHECK(desk.coordinator->isTemporaryShow());
        CHECK(raisedTab(desk.window, desk.dock) == QStringLiteral("Spelling and Grammar"));
        REQUIRE(desk.panel->selectedIssue().has_value());
        CHECK(desk.panel->selectedIssue()->issue.text == QStringLiteral("mistakke"));
        CHECK(editor->selectedText() == QStringLiteral("mistakke"));
        CHECK(desk.panel->hasFocusInside());
        CHECK(says(*desk.panel, QStringLiteral("Enter, 1–5 – fix · Esc – back "
                                               "to the text")));

        // The keys look through the issues
        press(QApplication::focusWidget(), Qt::Key_Down);
        CHECK(editor->selectedText() == QStringLiteral("errorz"));

        // Esc: back to the text; the panel goes, and the tab it covered is on top again
        press(QApplication::focusWidget(), Qt::Key_Escape);
        CHECK(editor->hasFocus());
        CHECK(desk.dock->isHidden());
        CHECK_FALSE(desk.coordinator->isTemporaryShow());
        CHECK(raisedTab(desk.window, desk.annotations) == QStringLiteral("Properties"));
        CHECK(editor->selectedText() == QStringLiteral("errorz"));  // to be put right by hand
    }

    SECTION("a panel under another tab comes on top, and goes under it again") {
        desk.dock->show();
        desk.annotations->raise();
        QApplication::processEvents();
        REQUIRE(raisedTab(desk.window, desk.dock) == QStringLiteral("Annotations"));

        Desk::nextIssue();
        CHECK(raisedTab(desk.window, desk.dock) == QStringLiteral("Spelling and Grammar"));
        CHECK(desk.coordinator->isTemporaryShow());
        press(QApplication::focusWidget(), Qt::Key_Escape);
        CHECK_FALSE(desk.dock->isHidden());  // still open, in its tab
        CHECK(raisedTab(desk.window, desk.dock) == QStringLiteral("Annotations"));
    }

    SECTION("the first character typed in the text hides it") {
        Desk::nextIssue();
        REQUIRE(desk.coordinator->isTemporaryShow());
        editor->setFocus();
        REQUIRE(waitUntil([editor]() { return editor->hasFocus(); }));
        // A click at the start of the second paragraph: the issue found is not selected any more
        editor->clearSelection();
        editor->setCursorPosition({1, 0});
        press(editor, Qt::Key_X, Qt::NoModifier, QStringLiteral("x"));
        CHECK(paragraphText(*editor, 0) == QStringLiteral("One mistakke here."));
        CHECK(paragraphText(*editor, 1) == QStringLiteral("xTwo errorz there."));
        CHECK(desk.dock->isHidden());
    }

    SECTION("a fix chosen in the panel does not hide it") {
        Desk::nextIssue();
        REQUIRE(desk.coordinator->isTemporaryShow());
        press(QApplication::focusWidget(), Qt::Key_Return);
        CHECK(paragraphText(*editor, 0) != QStringLiteral("One mistakke here."));
        CHECK(desk.dock->isVisible());
        CHECK(desk.coordinator->isTemporaryShow());
    }

    SECTION("the issue found is shown also when its kind was hidden in the panel") {
        desk.openPanel();
        desk.panel->kindButton(TextIssueKind::Spelling)->click();
        REQUIRE_FALSE(desk.panel->isKindShown(TextIssueKind::Spelling));
        Desk::nextIssue();
        CHECK(desk.panel->isKindShown(TextIssueKind::Spelling));
        REQUIRE(desk.panel->selectedIssue().has_value());
        CHECK(desk.panel->selectedIssue()->issue.text == QStringLiteral("mistakke"));
    }

    SECTION("the pin keeps it open") {
        desk.pin->click();
        CHECK(settings().get<bool>("spellingPanel.pinned", false));
        CHECK(desk.pin->isChecked());
        CHECK(desk.pin->toolTip() ==
              QStringLiteral("The panel is kept open – click to let it hide after looking "
                             "through the issues"));
        Desk::nextIssue();
        CHECK(desk.dock->isVisible());
        CHECK_FALSE(desk.coordinator->isTemporaryShow());
        press(QApplication::focusWidget(), Qt::Key_Escape);
        CHECK(editor->hasFocus());
        CHECK(desk.dock->isVisible());

        // Unpinned: shown for the issues only, from the next time
        desk.pin->click();
        CHECK_FALSE(settings().get<bool>("spellingPanel.pinned", true));
        CHECK(desk.pin->toolTip() == QStringLiteral("Keep the Panel Open"));
    }

    SECTION("set to open the menu at the word, it leaves the panel closed") {
        settings().set<std::string>("spellingPanel.nextIssue", "menu");
        const QStringList texts = kalahari::test::runPopupMenu([]() { Desk::nextIssue(); });
        CHECK(editor->selectedText() == QStringLiteral("mistakke"));
        CHECK(texts.contains(QStringLiteral("mistake")));
        CHECK(desk.dock->isHidden());
    }
}
