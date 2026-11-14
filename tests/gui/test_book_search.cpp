/// @file test_book_search.cpp
/// @brief Find in Book: the texts of the book searched as the editor would search them, open
///        or not, with the texts of their notes; the matches in the Search panel, going to them,
///        their following the changes, and Replace All in Book with its versions and Undo

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "document_window.h"
#include "kalahari/core/chapter_versions.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/find_replace_bar.h"
#include "kalahari/editor/search_engine.h"
#include "kalahari/editor/text_footnotes.h"
#include "kalahari/gui/book_search_coordinator.h"
#include "kalahari/gui/panels/search_panel.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QPushButton>
#include <QStringList>
#include <QTextDocument>
#include <QToolButton>
#include <QTreeWidget>

#include <algorithm>
#include <cstdio>
#include <functional>

using namespace kalahari;
using editor::SearchMatch;
using editor::SearchOptions;
using gui::BookSearchCoordinator;
using gui::SearchPanel;
using test::OpenNovel;
using test::Window;

namespace {

const QChar ELLIPSIS(0x2026);

/// Save a text of the book in its file
void save(OpenNovel& book, const QString& id, const QString& kml) {
    book.pm.setChapterContent(id, kml);
    REQUIRE(book.pm.saveChapterContent(id));
}

/// The text of a chapter as its file on the disk has it
QString fileOf(const OpenNovel& book, const QString& id) {
    const core::ProjectElement* element = book.pm.findElement(id);
    REQUIRE(element != nullptr);
    QFile file(book.pm.filePathOf(*element));
    REQUIRE(file.open(QIODevice::ReadOnly));
    return QJsonDocument::fromJson(file.readAll())
        .object()
        .value(QStringLiteral("content"))
        .toObject()
        .value(QStringLiteral("kml"))
        .toString();
}

/// The places of matches: "paragraph:offset", "paragraph:offset@note:offset" in a note
QStringList placesOf(const std::vector<SearchMatch>& matches) {
    QStringList places;
    for (const SearchMatch& match : matches) {
        QString place = QStringLiteral("%1:%2").arg(match.paragraph).arg(match.paragraphOffset);
        if (match.inNote()) {
            place += QStringLiteral("@%1:%2").arg(match.noteId).arg(match.noteOffset);
        }
        places << place;
    }
    return places;
}

/// A text with notes, an image, a table and a line: everything that makes the paragraphs of
/// the editor's document differ from those of the KML
const QString NOTED_TEXT = QStringLiteral(
    "<kml><p>Burza nad miastem<footnote id=\"f1\"><b>Burza</b> z roku 1931. Wielka burza."
    "</footnote> i burza.</p><p>Koniec<endnote id=\"e1\"><p>Pierwszy</p><p>Burza</p>"
    "</endnote> burzy <img src=\"images/a.png\"/> burza.</p><hr/>"
    "<table><tr><td><p>burza w tabeli</p></td><td><p>cisza</p></td></tr></table>"
    "<p>Po burzy.</p></kml>");

/// Keeps the settings of the versions as they were when a test ends
struct VersionSettings {
    core::SettingsManager& settings = core::SettingsManager::getInstance();
    bool onSave = settings.get<bool>(core::ChapterVersions::ON_SAVE_SETTING);
    bool enabled = settings.get<bool>(core::ChapterVersions::ENABLED_SETTING);
    bool beforeReplace = settings.get<bool>(core::ChapterVersions::BEFORE_REPLACE_ALL_SETTING);

    VersionSettings() {
        // Not on each save of a test's text, only before Replace All
        settings.set<bool>(core::ChapterVersions::ON_SAVE_SETTING, false);
        settings.set<bool>(core::ChapterVersions::ENABLED_SETTING, true);
        settings.set<bool>(core::ChapterVersions::BEFORE_REPLACE_ALL_SETTING, true);
    }
    ~VersionSettings() {
        try {
            settings.set<bool>(core::ChapterVersions::ON_SAVE_SETTING, onSave);
            settings.set<bool>(core::ChapterVersions::ENABLED_SETTING, enabled);
            settings.set<bool>(core::ChapterVersions::BEFORE_REPLACE_ALL_SETTING, beforeReplace);
        } catch (...) {
            std::fputs("VersionSettings: the settings of the versions stay\n", stderr);
        }
    }
    VersionSettings(const VersionSettings&) = delete;
    VersionSettings& operator=(const VersionSettings&) = delete;
};

/// A book of four texts: a foreword, two chapters and notes in the back part
struct SearchedBook {
    OpenNovel book{QStringLiteral("Search")};
    QString foreword;
    QString storm;
    QString calm;
    QString notes;

    static inline const QString FOREWORD_TEXT = QStringLiteral("<kml><p>A storm is coming.</p></kml>");
    static inline const QString STORM_TEXT = QStringLiteral(
        "<kml><p>The storm came at night.</p><p>No Storm<footnote id=\"f1\">The <b>storm</b> "
        "of 1931.</footnote> then.</p></kml>");
    static inline const QString CALM_TEXT = QStringLiteral("<kml><p>Calm after the storm.</p></kml>");
    static inline const QString NOTES_SAVED = QStringLiteral("<kml><p>Nothing.</p></kml>");
    static inline const QString NOTES_TEXT = QStringLiteral("<kml><p>Stormy notes.</p></kml>");

    SearchedBook() {
        storm = book.chapterId;
        foreword = book.pm.addElement(book.pm.chapterKindFor(core::BookPlace::Front),
                                      QStringLiteral("Foreword"), core::BookPlace::Front);
        calm = book.pm.addElement(book.pm.chapterKindFor(core::BookPlace::Main),
                                  QStringLiteral("The Calm"), core::BookPlace::Main);
        notes = book.pm.addElement(book.pm.chapterKindFor(core::BookPlace::Back),
                                   QStringLiteral("Notes"), core::BookPlace::Back);
        REQUIRE_FALSE(foreword.isEmpty());
        REQUIRE_FALSE(calm.isEmpty());
        REQUIRE_FALSE(notes.isEmpty());
        save(book, foreword, FOREWORD_TEXT);
        save(book, storm, STORM_TEXT);
        save(book, calm, CALM_TEXT);
        // The notes have changes not saved yet: the program keeps them
        save(book, notes, NOTES_SAVED);
        book.pm.setChapterContent(notes, NOTES_TEXT);
    }
};

/// Find in Book over the tabs of a window, as the main window has it; the notices under the
/// text and the questions it asks are kept
struct Search {
    SearchPanel panel;
    editor::BookEditor* keys = nullptr;  ///< The text with the keys
    QStringList notices;
    QList<std::function<void()>> undoButtons;
    QStringList asked;
    bool answer = true;
    editor::FootnoteContext noteContext;  ///< The numbering of the notes of every text
    BookSearchCoordinator coordinator;

    explicit Search(Window& window)
        : coordinator(
              test::book(), test::commands(), window.tabs, &panel, []() {},
              [this]() { return keys; },
              [&window](const QString& id) {
                  const core::ProjectElement* element = test::book().findElement(id);
                  window.navigator.onElementSelected(id, element != nullptr ? element->title
                                                                            : QString());
              },
              [this](const QString&) { return noteContext; },
              [this](gui::EditorPanel*, const QString& message, const QString&, const QString&,
                     const std::function<void()>& action) {
                  notices.append(message);
                  undoButtons.append(action);
              },
              &window.statusBar, &window.window) {
        coordinator.setConfirmer([this](int matches, int texts) {
            asked << QStringLiteral("%1 in %2").arg(matches).arg(texts);
            return answer;
        });
    }

    /// Search the whole book
    void find(const QString& text, const SearchOptions& options = {}) {
        panel.setOptions(options);
        panel.setSearchText(text);
        coordinator.search();
        coordinator.finishSearch();
    }

    /// The texts found: "Title 3"
    [[nodiscard]] QStringList foundTexts() const {
        QStringList texts;
        for (const BookSearchCoordinator::TextMatches& text : coordinator.found()) {
            texts << QStringLiteral("%1 %2").arg(text.title).arg(text.matches.size());
        }
        return texts;
    }

    /// The index in found() of a text
    [[nodiscard]] int indexOf(const QString& id) const {
        const auto& found = coordinator.found();
        const auto it = std::find_if(found.cbegin(), found.cend(),
                                     [&id](const auto& text) { return text.elementId == id; });
        REQUIRE(it != found.cend());
        return static_cast<int>(it - found.cbegin());
    }
};

/// The editor of a text open in a window
editor::BookEditor* openText(Window& window, const QString& id) {
    for (int i = 0; i < window.tabs->count(); ++i) {
        if (window.tabs->widget(i)->property("elementId").toString() == id) {
            return window.editor(i)->getBookEditor();
        }
    }
    return nullptr;
}

/// Press a key on a widget, as the system sends it: a shortcut override first
void press(QWidget* widget, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent override(QEvent::ShortcutOverride, key, modifiers);
    QApplication::sendEvent(widget, &override);
    QKeyEvent down(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(widget, &down);
    QKeyEvent up(QEvent::KeyRelease, key, modifiers);
    QApplication::sendEvent(widget, &up);
}

/// A widget of the panel by its accessible name or text
template <typename Widget>
Widget* widgetNamed(const SearchPanel& panel, const QString& name) {
    for (Widget* widget : panel.findChildren<Widget*>()) {
        if (widget->accessibleName() == name || widget->property("text").toString() == name) {
            return widget;
        }
    }
    return nullptr;
}

}  // namespace

// =============================================================================
// The text searched without its editor
// =============================================================================

TEST_CASE("Find in Book: a text's KML is searched as the editor searches its document",
          "[editor][search][book]") {
    editor::BookEditor text;
    REQUIRE(text.fromKml(NOTED_TEXT));
    const QTextDocument& document = *text.textDocument();

    // The same paragraphs, numbers and positions as in the editor, the notes' texts with them
    const std::vector<editor::SearchableParagraph> paragraphs =
        editor::searchableParagraphsOfKml(NOTED_TEXT);
    CHECK(paragraphs == editor::searchableParagraphs(document));
    REQUIRE(paragraphs.size() == static_cast<size_t>(document.blockCount()));
    CHECK(paragraphs.front().notes.size() == 1);
    CHECK(paragraphs.front().notes.front().kind == editor::NoteKind::Footnote);
    CHECK(paragraphs.at(1).notes.front().kind == editor::NoteKind::Endnote);
    CHECK(paragraphs.at(1).notes.front().text == QStringLiteral("Pierwszy\nBurza"));

    // findIn() finds what findAll() finds in the document, with the options
    for (const SearchOptions& options :
         {SearchOptions{}, SearchOptions{.caseSensitive = true}, SearchOptions{.wholeWord = true},
          SearchOptions{.useRegex = true}}) {
        editor::SearchEngine inDocument;
        inDocument.setDocument(text.textDocument());
        inDocument.setSearchText(options.useRegex ? QStringLiteral("bur[zy]+") : QStringLiteral("burza"));
        inDocument.setOptions(options);
        editor::SearchEngine apart;
        apart.setSearchText(inDocument.searchText());
        apart.setOptions(options);
        const std::vector<SearchMatch> all = inDocument.findAll();
        CHECK(apart.findIn(paragraphs) == all);
        CHECK(placesOf(apart.findIn(paragraphs)) == placesOf(all));
    }
    editor::SearchEngine burza;
    burza.setSearchText(QStringLiteral("burza"));
    CHECK(placesOf(burza.findIn(paragraphs)) ==
          QStringList{QStringLiteral("0:0"), QStringLiteral("0:17@f1:0"),
                      QStringLiteral("0:17@f1:26"), QStringLiteral("0:21"),
                      QStringLiteral("1:6@e1:9"), QStringLiteral("1:16"),
                      QStringLiteral("3:0")});

    // What can be searched for
    editor::SearchEngine engine;
    CHECK_FALSE(engine.canSearch());
    engine.setSearchText(QStringLiteral("(burza"));
    CHECK(engine.canSearch());
    engine.setOptions({.useRegex = true});
    CHECK_FALSE(engine.canSearch());
    CHECK(engine.findIn(paragraphs).empty());
}

TEST_CASE("Find in Book: Replace All in a text's KML, as the editor replaces in its document",
          "[editor][search][book]") {
    const QString wichura = QStringLiteral("wichura");
    for (const SearchOptions& options :
         {SearchOptions{}, SearchOptions{.caseSensitive = true}, SearchOptions{.wholeWord = true}}) {
        editor::BookEditor text;
        REQUIRE(text.fromKml(NOTED_TEXT));
        int count = -1;
        const QString replaced =
            editor::replacedInKml(NOTED_TEXT, QStringLiteral("burza"), wichura, options, &count);
        int aboutToReplace = -1;
        QObject::connect(&text, &editor::BookEditor::aboutToReplaceAll,
                         [&aboutToReplace](const QString&, const QString&, int matches) {
                             aboutToReplace = matches;
                         });
        CHECK(text.replaceAllInText(QStringLiteral("burza"), wichura, options) == count);
        CHECK(aboutToReplace == count);
        CHECK(text.toKml() == replaced);

        // One step of the editor's undo history
        text.undo();
        CHECK(text.toKml() == NOTED_TEXT);
    }

    // The formatting of a match stays; nothing to replace: the KML as it is
    int count = -1;
    const QString replaced = editor::replacedInKml(NOTED_TEXT, QStringLiteral("Burza"), wichura,
                                                   {.caseSensitive = true}, &count);
    CHECK(count == 3);
    CHECK(replaced.contains(QStringLiteral("<b>wichura</b> z roku 1931")));
    CHECK(replaced.contains(QStringLiteral("<p>wichura</p></endnote>")));
    CHECK(editor::replacedInKml(NOTED_TEXT, QStringLiteral("grad"), wichura, {}, &count) ==
          NOTED_TEXT);
    CHECK(count == 0);

    // A read-only text replaces nothing
    editor::BookEditor readOnly;
    REQUIRE(readOnly.fromKml(NOTED_TEXT));
    readOnly.setReadOnly(true);
    CHECK(readOnly.replaceAllInText(QStringLiteral("burza"), wichura, {}) == 0);
    CHECK(readOnly.toKml() == NOTED_TEXT);
}

TEST_CASE("Find in Book: a match goes to its place in the text, in a note's text too",
          "[editor][search][book]") {
    editor::BookEditor text;
    REQUIRE(text.fromKml(NOTED_TEXT));
    editor::SearchEngine engine;
    engine.setSearchText(QStringLiteral("burza"));
    const std::vector<SearchMatch> matches =
        engine.findIn(editor::searchableParagraphs(*text.textDocument()));
    REQUIRE(matches.size() == 7);

    // In the text: the match selected
    CHECK(text.goToSearchMatch(matches.at(3)));
    CHECK(text.selectedText() == QStringLiteral("burza"));
    CHECK(text.selection().normalized().start == editor::CursorPosition{0, 21});

    // In a note: its number selected, the range of its text with it
    CHECK(text.goToSearchMatch(matches.at(4)));
    REQUIRE(text.noteSelection().has_value());
    CHECK(*text.noteSelection() == editor::NoteRange{QStringLiteral("e1"), 9, 5});
    CHECK(text.searchTextFromSelection() == QStringLiteral("Burza"));

    // The text changed since: not there
    text.clearSelection();
    text.setCursorPosition({0, 0});
    text.insertText(QStringLiteral("Wielka "));
    CHECK_FALSE(text.goToSearchMatch(matches.at(0)));
    CHECK_FALSE(text.goToSearchMatch(SearchMatch{}));
}

// =============================================================================
// The line around a match
// =============================================================================

TEST_CASE("Find in Book: the line around a match in the Search panel", "[gui][search][book]") {
    // A short line: all of it
    gui::SearchPanelHit hit = gui::shownSearchHit(QStringLiteral("The storm came."), 4, 5);
    CHECK(hit.context == QStringLiteral("The storm came."));
    CHECK(hit.start == 4);
    CHECK(hit.length == 5);
    CHECK(hit.note.isEmpty());

    // Long before the match: from a word, after "…"
    const QString before = QStringLiteral("word ").repeated(20);
    hit = gui::shownSearchHit(before + QStringLiteral("storm came."), static_cast<int>(before.size()),
                              5);
    CHECK(hit.context.startsWith(ELLIPSIS + QStringLiteral("word ")));
    CHECK(hit.context.mid(hit.start, hit.length) == QStringLiteral("storm"));
    CHECK(hit.start <= 31);

    // Long after it: cut with "…"
    const QString after = QStringLiteral(" and the rain").repeated(20);
    hit = gui::shownSearchHit(QStringLiteral("storm") + after, 0, 5,
                              QStringLiteral("footnote 2"));
    CHECK(hit.context.endsWith(ELLIPSIS));
    CHECK(hit.context.size() == 5 + 80 + 1);
    CHECK(hit.note == QStringLiteral("footnote 2"));

    // On one line: the line breaks and the objects as spaces, without the spaces before
    hit = gui::shownSearchHit(QStringLiteral("  a") + QChar::LineSeparator + QStringLiteral("b") +
                                  QChar::ObjectReplacementCharacter + QStringLiteral("storm"),
                              6, 5);
    CHECK(hit.context == QStringLiteral("a b storm"));
    CHECK(hit.start == 4);
}

// =============================================================================
// The book
// =============================================================================

TEST_CASE("Find in Book: all the texts of the book, open or not, in the order of the book",
          "[gui][search][book]") {
    SearchedBook book;
    Window window;
    Search search(window);

    // The chapter open with a change not saved yet: as the editor has it
    window.navigator.onElementSelected(book.storm, QStringLiteral("The Storm"));
    editor::BookEditor* storm = openText(window, book.storm);
    REQUIRE(storm != nullptr);
    storm->setCursorPosition({0, 0});
    storm->insertText(QStringLiteral("Storm! "));

    search.find(QStringLiteral("storm"));
    CHECK(search.foundTexts() == QStringList{QStringLiteral("Foreword 1"),
                                             QStringLiteral("The Storm 4"),
                                             QStringLiteral("The Calm 1"), QStringLiteral("Notes 1")});
    CHECK(search.panel.status() == QStringLiteral("7 matches in 4 texts."));

    // The panel lists them: each text with its count, its matches with the line around them
    REQUIRE(search.panel.texts().size() == 4);
    CHECK(search.panel.texts().at(1).title == QStringLiteral("The Storm"));
    CHECK(search.panel.texts().at(1).count == 4);
    QTreeWidget* tree = search.panel.tree();
    REQUIRE(tree->topLevelItemCount() == 4);
    CHECK(tree->topLevelItem(1)->text(0) == QStringLiteral("The Storm (4)"));
    CHECK(tree->topLevelItem(1)->childCount() == 4);
    const QList<gui::SearchPanelHit>& hits = search.panel.texts().at(1).hits;
    CHECK(hits.at(0).context == QStringLiteral("Storm! The storm came at night."));
    CHECK(hits.at(1).start == 11);
    // A match in a note: the line of its text, with the note's number
    CHECK(hits.at(3).note == QStringLiteral("footnote 1"));
    CHECK(hits.at(3).context == QStringLiteral("The storm of 1931."));
    CHECK(hits.at(3).start == 4);
    CHECK(tree->topLevelItem(1)->child(3)->text(0) ==
          QStringLiteral("footnote 1: The storm of 1931."));
    // The text with changes not saved yet: as the program keeps it
    CHECK(search.panel.texts().at(3).hits.at(0).context == QStringLiteral("Stormy notes."));

    // The options
    search.find(QStringLiteral("storm"), {.wholeWord = true});
    CHECK(search.foundTexts() == QStringList{QStringLiteral("Foreword 1"),
                                             QStringLiteral("The Storm 4"),
                                             QStringLiteral("The Calm 1")});
    search.find(QStringLiteral("storm"), {.caseSensitive = true});
    CHECK(search.foundTexts() == QStringList{QStringLiteral("Foreword 1"),
                                             QStringLiteral("The Storm 2"),
                                             QStringLiteral("The Calm 1")});
    search.find(QStringLiteral("storm\\w"), {.useRegex = true});
    CHECK(search.foundTexts() == QStringList{QStringLiteral("Notes 1")});
    CHECK(search.panel.status() == QStringLiteral("1 matches in 1 texts."));

    // Nothing, an expression that is not one, nothing to find
    search.find(QStringLiteral("hail"));
    CHECK(search.foundTexts().isEmpty());
    CHECK(search.panel.status() == QStringLiteral("No matches in the book."));
    CHECK(search.coordinator.whyNotReplace() == QStringLiteral("Nothing was found to replace"));
    search.find(QStringLiteral("(storm"), {.useRegex = true});
    CHECK(search.panel.status() == QStringLiteral("The regular expression is not valid."));
    search.find(QString());
    CHECK(search.panel.status() == QStringLiteral("Type what to find in all the texts of the book."));
    CHECK(tree->topLevelItemCount() == 0);
}

TEST_CASE("Find in Book: going to a match, and the matches following the changes",
          "[gui][search][book]") {
    SearchedBook book;
    Window window;
    Search search(window);
    search.find(QStringLiteral("storm"));
    REQUIRE(window.tabs->count() == 0);

    // A text not open opens, with the match selected
    const int calm = search.indexOf(book.calm);
    CHECK(search.coordinator.goToMatch(calm, 0, true));
    editor::BookEditor* calmText = openText(window, book.calm);
    REQUIRE(calmText != nullptr);
    CHECK(calmText->selectedText() == QStringLiteral("storm"));
    CHECK(calmText->selection().normalized().start == editor::CursorPosition{0, 15});

    // A match in a note: its number selected, the match in its text
    const int storm = search.indexOf(book.storm);
    CHECK(search.coordinator.goToMatch(storm, 2, true));
    editor::BookEditor* stormText = openText(window, book.storm);
    REQUIRE(stormText != nullptr);
    REQUIRE(stormText->noteSelection().has_value());
    CHECK(*stormText->noteSelection() == editor::NoteRange{QStringLiteral("f1"), 4, 5});
    CHECK(window.tabs->currentWidget()->property("elementId").toString() == book.storm);

    // Not a match
    CHECK_FALSE(search.coordinator.goToMatch(storm, 7, true));
    CHECK_FALSE(search.coordinator.goToMatch(9, 0, true));

    // An open text changed: its matches a moment later, the others as they were
    stormText->clearSelection();
    stormText->setCursorPosition({0, 0});
    stormText->insertText(QStringLiteral("Storm, storm! "));
    CHECK(test::waitUntil([&search]() {
        return search.foundTexts().contains(QStringLiteral("The Storm 5"));
    }));
    CHECK(search.foundTexts() == QStringList{QStringLiteral("Foreword 1"),
                                             QStringLiteral("The Storm 5"),
                                             QStringLiteral("The Calm 1"), QStringLiteral("Notes 1")});
    CHECK(search.panel.status() == QStringLiteral("8 matches in 4 texts."));

    // Changed again, and a match chosen before its matches follow: they follow at once, the
    // match with the same index chosen
    stormText->setCursorPosition({0, 0});
    stormText->insertText(QStringLiteral("Calm. "));
    CHECK(search.coordinator.goToMatch(search.indexOf(book.storm), 1, true));
    CHECK(stormText->selectedText() == QStringLiteral("storm"));
    CHECK(stormText->selection().normalized().start == editor::CursorPosition{0, 13});
    CHECK(search.panel.currentHit() == SearchPanel::HitIndex{search.indexOf(book.storm), 1});

    // A text without its matches any more leaves the list
    stormText->selectAll();
    stormText->insertText(QStringLiteral("Quiet."));
    CHECK(test::waitUntil([&search]() { return search.foundTexts().size() == 3; }));
    CHECK(search.foundTexts() == QStringList{QStringLiteral("Foreword 1"),
                                             QStringLiteral("The Calm 1"), QStringLiteral("Notes 1")});
}

TEST_CASE("Find in Book: the numbers of the notes the matches are in follow the book's",
          "[gui][search][book]") {
    SearchedBook book;
    Window window;
    Search search(window);
    const auto noteOfStorm = [&search, &book]() {
        const auto& texts = search.panel.texts();
        const auto it = std::find_if(texts.cbegin(), texts.cend(), [&book](const auto& text) {
            return text.key == book.storm;
        });
        REQUIRE(it != texts.cend());
        REQUIRE(it->hits.size() == 1);
        return it->hits.first().note;
    };

    // The chapter not open: its notes numbered as the book numbers them
    search.noteContext.footnoteFormat = editor::NoteNumberFormat::LowerAlpha;
    search.noteContext.footnotesBefore = 2;
    search.find(QStringLiteral("1931"));
    CHECK(noteOfStorm() == QStringLiteral("footnote c"));

    // Insert > Footnote Layout...: the numbers as they are now
    search.noteContext = {};
    core::BookFootnotes footnotes = book.book.pm.bookFootnotes().value_or(core::BookFootnotes{});
    footnotes.footnotes.perChapter = !footnotes.footnotes.perChapter;
    REQUIRE(book.book.pm.setBookFootnotes(footnotes));
    search.coordinator.finishSearch();
    CHECK(noteOfStorm() == QStringLiteral("footnote 1"));

    // Another text changed (a note before it, through the book): the numbers follow too
    window.navigator.onElementSelected(book.foreword, QStringLiteral("Foreword"));
    editor::BookEditor* foreword = openText(window, book.foreword);
    REQUIRE(foreword != nullptr);
    search.noteContext.footnotesBefore = 1;
    foreword->setCursorPosition({0, 0});
    foreword->insertText(QStringLiteral("Calm. "));
    CHECK(test::waitUntil([&]() { return noteOfStorm() == QStringLiteral("footnote 2"); }));

    // A text closed without its change: its matches as the book keeps it
    foreword->insertText(QStringLiteral("In 1931. "));
    CHECK(test::waitUntil([&search]() { return search.foundTexts().size() == 2; }));
    delete window.editor(window.tabs->currentIndex());
    REQUIRE(book.book.pm.chapterText(book.foreword) == SearchedBook::FOREWORD_TEXT);
    CHECK(test::waitUntil([&search]() {
        return search.foundTexts() == QStringList{QStringLiteral("The Storm 1")};
    }));
}

TEST_CASE("Find in Book: Edit > Find in Book takes the selection, else the find bar's search",
          "[gui][search][book]") {
    SearchedBook book;
    Window window;
    Search search(window);
    window.navigator.onElementSelected(book.storm, QStringLiteral("The Storm"));
    editor::BookEditor* storm = openText(window, book.storm);
    REQUIRE(storm != nullptr);
    search.keys = storm;

    // The text selected
    storm->setSelection({{0, 4}, {0, 9}});
    search.coordinator.findInBook();
    search.coordinator.finishSearch();
    CHECK(search.panel.searchText() == QStringLiteral("storm"));
    CHECK(search.foundTexts().size() == 4);

    // Without a selection: what the find bar searches for, with its options
    storm->clearSelection();
    storm->showFind();
    auto* bar = storm->findChild<editor::FindReplaceBar*>();
    REQUIRE(bar != nullptr);
    bar->setSearchText(QStringLiteral("night"));
    storm->searchEngine()->setOptions({.caseSensitive = true});
    search.coordinator.findInBook();
    search.coordinator.finishSearch();
    CHECK(search.panel.searchText() == QStringLiteral("night"));
    CHECK(search.panel.options().caseSensitive);
    CHECK(search.foundTexts() == QStringList{QStringLiteral("The Storm 1")});

    // The keys elsewhere: the panel's own search
    search.keys = nullptr;
    search.panel.setSearchText(QStringLiteral("calm"));
    search.panel.setOptions({});
    search.coordinator.findInBook();
    search.coordinator.finishSearch();
    CHECK(search.foundTexts() == QStringList{QStringLiteral("The Calm 1")});
}

TEST_CASE("Find in Book: Replace All in Book, with a version of each text and Undo",
          "[gui][search][book][versions]") {
    VersionSettings versionSettings;
    SearchedBook book;
    Window window;
    Search search(window);
    window.navigator.onElementSelected(book.storm, QStringLiteral("The Storm"));
    editor::BookEditor* storm = openText(window, book.storm);
    REQUIRE(storm != nullptr);
    const QString stormBefore = storm->toKml();
    search.find(QStringLiteral("storm"));
    REQUIRE(search.foundTexts().size() == 4);
    CHECK(search.coordinator.whyNotReplace().isEmpty());
    QLineEdit* replacement = widgetNamed<QLineEdit>(search.panel, QStringLiteral("Replace With"));
    REQUIRE(replacement != nullptr);
    replacement->setText(QStringLiteral("gale"));

    // The writer says no: nothing changes
    search.answer = false;
    CHECK(search.coordinator.replaceAllInBook() == 0);
    CHECK(search.asked == QStringList{QStringLiteral("6 in 4")});
    CHECK(storm->toKml() == stormBefore);
    CHECK(book.book.pm.chapterText(book.calm) == SearchedBook::CALM_TEXT);
    CHECK_FALSE(search.coordinator.canUndo());

    // Yes: the open text in its editor, the saved ones in their files, the one with changes
    // not saved yet only in the program; each kept as a version first; the notice, with Undo
    search.answer = true;
    CHECK(search.coordinator.replaceAllInBook() == 6);
    CHECK(storm->toKml() ==
          QStringLiteral("<kml><p>The gale came at night.</p><p>No gale<footnote id=\"f1\">The "
                         "<b>gale</b> of 1931.</footnote> then.</p></kml>"));
    CHECK(fileOf(book.book, book.calm) == QStringLiteral("<kml><p>Calm after the gale.</p></kml>"));
    CHECK(fileOf(book.book, book.foreword) == QStringLiteral("<kml><p>A gale is coming.</p></kml>"));
    CHECK(book.book.pm.chapterText(book.notes) == QStringLiteral("<kml><p>galey notes.</p></kml>"));
    CHECK(fileOf(book.book, book.notes) == SearchedBook::NOTES_SAVED);
    CHECK(search.notices == QStringList{QStringLiteral("6 matches replaced in the book.")});
    for (const QString& id : {book.storm, book.calm, book.notes}) {
        const std::optional<core::ChapterVersions> versions = book.book.pm.versionsOf(id);
        REQUIRE(versions.has_value());
        REQUIRE(versions->count() == 1);
        const core::ChapterVersion version = *versions->newest();
        CHECK(version.origin == core::VersionOrigin::ReplaceAll);
        CHECK(version.found == QStringLiteral("storm"));
        CHECK(version.replacement == QStringLiteral("gale"));
    }
    CHECK(book.book.pm.versionsOf(book.calm)->newest()->replaced == 1);
    CHECK(book.book.pm.versionsOf(book.storm)->newest()->replaced == 3);
    // The matches as they are now: none
    search.coordinator.finishSearch();
    CHECK(search.foundTexts().isEmpty());

    // Undo of the notice: every text as it was
    REQUIRE(search.coordinator.canUndo());
    REQUIRE(search.undoButtons.size() == 1);
    search.undoButtons.first()();
    CHECK(storm->toKml() == stormBefore);
    CHECK(fileOf(book.book, book.calm) == SearchedBook::CALM_TEXT);
    CHECK(fileOf(book.book, book.foreword) == SearchedBook::FOREWORD_TEXT);
    CHECK(book.book.pm.chapterText(book.notes) == SearchedBook::NOTES_TEXT);
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("The texts are as they were before the replacement."));
    CHECK_FALSE(search.coordinator.canUndo());
    search.coordinator.finishSearch();
    CHECK(search.foundTexts().size() == 4);

    // Again, and the open text written in: the change is final
    CHECK(search.coordinator.replaceAllInBook() == 6);
    REQUIRE(search.coordinator.canUndo());
    storm->setCursorPosition({0, 0});
    storm->insertText(QStringLiteral("x"));
    CHECK_FALSE(search.coordinator.canUndo());
    CHECK_FALSE(search.coordinator.undo());
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("This can no longer be undone: the texts changed since."));
    CHECK(fileOf(book.book, book.calm) == QStringLiteral("<kml><p>Calm after the gale.</p></kml>"));

    // Nothing to replace
    search.find(QStringLiteral("hail"));
    CHECK(search.coordinator.replaceAllInBook() == 0);
}

// =============================================================================
// The panel
// =============================================================================

TEST_CASE("Search panel: the keys, the options, and the tree kept as it was",
          "[gui][search][book]") {
    SearchPanel panel;
    int searches = 0;
    QStringList activated;
    int chosen = 0;
    int back = 0;
    int replace = 0;
    QObject::connect(&panel, &SearchPanel::searchRequested, [&searches]() { ++searches; });
    QObject::connect(&panel, &SearchPanel::hitActivated, [&activated](int text, int hit, bool keys) {
        activated << QStringLiteral("%1 %2 %3")
                         .arg(text)
                         .arg(hit)
                         .arg(keys ? QStringLiteral("true") : QStringLiteral("false"));
    });
    QObject::connect(&panel, &SearchPanel::hitChosen, [&chosen]() { ++chosen; });
    QObject::connect(&panel, &SearchPanel::editorFocusRequested, [&back]() { ++back; });
    QObject::connect(&panel, &SearchPanel::replaceAllRequested, [&replace]() { ++replace; });
    QLineEdit* field = widgetNamed<QLineEdit>(panel, QStringLiteral("Find in Book"));
    REQUIRE(field != nullptr);

    // Typing asks for the search a moment later, Enter at once
    field->setText(QStringLiteral("storm"));
    CHECK(searches == 0);
    CHECK(test::waitUntil([&searches]() { return searches == 1; }));
    press(field, Qt::Key_Return);
    CHECK(searches == 2);

    // The options' keys, as on the find bar; each asks for the search
    const QList<QKeyCombination> keys = editor::FindReplaceBar::optionKeys();
    press(field, keys.at(0).key(), keys.at(0).keyboardModifiers());
    CHECK(panel.options().caseSensitive);
    press(field, keys.at(1).key(), keys.at(1).keyboardModifiers());
    CHECK(panel.options().wholeWord);
    press(field, keys.at(2).key(), keys.at(2).keyboardModifiers());
    CHECK(panel.options().useRegex);
    CHECK(searches == 5);
    // Set by the program: no search
    panel.setOptions({});
    panel.setSearchText(QStringLiteral("rain"));
    CHECK_FALSE(test::waitUntil([&searches]() { return searches > 5; }, 500));

    // The texts: chosen and folded as they were after a refresh
    const gui::SearchPanelHit hit{QStringLiteral("The storm came."), 4, 5, QString()};
    panel.setTexts({{QStringLiteral("a"), QStringLiteral("One"), 2, {hit, hit}},
                    {QStringLiteral("b"), QStringLiteral("Two"), 1, {hit}}});
    QTreeWidget* tree = panel.tree();
    REQUIRE(tree->topLevelItemCount() == 2);
    CHECK(tree->topLevelItem(0)->isExpanded());
    panel.setCurrentHit({0, 1});
    CHECK(chosen == 0);
    tree->topLevelItem(1)->setExpanded(false);
    panel.setTexts({{QStringLiteral("c"), QStringLiteral("New"), 1, {hit}},
                    {QStringLiteral("a"), QStringLiteral("One"), 2, {hit, hit}},
                    {QStringLiteral("b"), QStringLiteral("Two"), 1, {hit}}});
    CHECK(panel.currentHit() == SearchPanel::HitIndex{1, 1});
    CHECK(tree->topLevelItem(0)->isExpanded());
    CHECK_FALSE(tree->topLevelItem(2)->isExpanded());
    CHECK(chosen == 0);

    // Down from the field to the first match; Enter goes to it with the keys; Esc back
    press(field, Qt::Key_Down);
    CHECK(panel.currentHit() == SearchPanel::HitIndex{0, 0});
    CHECK(chosen == 1);
    press(tree, Qt::Key_Return);
    CHECK(activated == QStringList{QStringLiteral("0 0 true")});
    press(tree, Qt::Key_Escape);
    CHECK(back == 1);

    // Replace All in Book: as it is allowed, Enter in the field of the replacement presses it
    QLineEdit* replacement = widgetNamed<QLineEdit>(panel, QStringLiteral("Replace With"));
    REQUIRE(replacement != nullptr);
    panel.setReplaceEnabled(false, QStringLiteral("No book is open"));
    press(replacement, Qt::Key_Return);
    CHECK(replace == 0);
    panel.setReplaceEnabled(true);
    press(replacement, Qt::Key_Return);
    CHECK(replace == 1);
}
