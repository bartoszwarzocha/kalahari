// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_statistics_collector.cpp
/// @brief Unit tests for StatisticsCollector (OpenSpec #00042 Task 7.17)
/// Phase 11: Rewritten for QTextDocument-based architecture

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <kalahari/core/text_statistics.h>
#include <kalahari/editor/statistics_collector.h>
#include <kalahari/editor/book_editor.h>
#include "kalahari_tests/editor/editor_test_utils.h"

#include <kalahari/core/project_database.h>
#include <kalahari/core/writing_journal.h>
#include <kalahari/core/writing_statistics.h>

#include <QApplication>
#include <QKeyEvent>
#include <QMimeData>
#include <QTemporaryDir>
#include <memory>

using namespace kalahari::editor;

// =============================================================================
// Helper: Create KML for testing
// =============================================================================
namespace {

/// Create KML with single paragraph containing text
QString createKml(const QString& text) {
    return QString("<p>%1</p>").arg(text);
}

/// Create KML with multiple paragraphs
QString createKmlParagraphs(const QStringList& paragraphs) {
    QString kml;
    for (const QString& text : paragraphs) {
        kml += QString("<p>%1</p>\n").arg(text);
    }
    return kml;
}

/// Run the event loop past the count the collector makes after an edit (250 ms)
void waitForCount() {
    kalahari::test::runEventLoop(500);
}

}  // namespace

// =============================================================================
// Basic Construction Tests
// =============================================================================

TEST_CASE("StatisticsCollector: Basic construction", "[editor][statistics]") {
    StatisticsCollector collector;

    SECTION("Default state has zero counts") {
        REQUIRE(collector.wordCount() == 0);
        REQUIRE(collector.characterCount() == 0);
        REQUIRE(collector.characterCountNoSpaces() == 0);
        REQUIRE(collector.paragraphCount() == 0);
    }

    SECTION("No session active by default") {
        REQUIRE_FALSE(collector.isSessionActive());
    }

    SECTION("Reading time is zero without content") {
        REQUIRE(collector.estimatedReadingTime() == 0);
    }
}

// =============================================================================
// Editor Connection Tests
// =============================================================================

TEST_CASE("StatisticsCollector: Editor connection", "[editor][statistics]") {
    BookEditor editor;
    editor.fromKml(createKml("Hello world"));
    StatisticsCollector collector;

    SECTION("Connecting editor updates stats") {
        collector.setBookEditor(&editor);

        REQUIRE(collector.wordCount() == 2);
        REQUIRE(collector.characterCount() == 11);
        REQUIRE(collector.paragraphCount() == 1);
    }

    SECTION("Disconnecting editor resets stats") {
        collector.setBookEditor(&editor);
        REQUIRE(collector.wordCount() == 2);

        collector.setBookEditor(nullptr);
        REQUIRE(collector.wordCount() == 0);
        REQUIRE(collector.characterCount() == 0);
    }

    SECTION("Setting same editor does nothing") {
        collector.setBookEditor(&editor);
        int count1 = collector.wordCount();

        collector.setBookEditor(&editor);
        int count2 = collector.wordCount();

        REQUIRE(count1 == count2);
    }

    // Disconnect before destruction
    collector.setBookEditor(nullptr);
}

// =============================================================================
// Word Counting Tests
// =============================================================================

TEST_CASE("StatisticsCollector: Word counting", "[editor][statistics]") {
    StatisticsCollector collector;

    SECTION("Empty editor has zero words") {
        BookEditor editor;
        collector.setBookEditor(&editor);
        REQUIRE(collector.wordCount() == 0);
        collector.setBookEditor(nullptr);
    }

    SECTION("Single word") {
        BookEditor editor;
        editor.fromKml(createKml("Hello"));
        collector.setBookEditor(&editor);
        REQUIRE(collector.wordCount() == 1);
        collector.setBookEditor(nullptr);
    }

    SECTION("Multiple words with spaces") {
        BookEditor editor;
        editor.fromKml(createKml("The quick brown fox"));
        collector.setBookEditor(&editor);
        REQUIRE(collector.wordCount() == 4);
        collector.setBookEditor(nullptr);
    }

    SECTION("Words with punctuation") {
        BookEditor editor;
        editor.fromKml(createKml("Hello, world! How are you?"));
        collector.setBookEditor(&editor);
        REQUIRE(collector.wordCount() == 5);
        collector.setBookEditor(nullptr);
    }

    SECTION("Multiple paragraphs") {
        BookEditor editor;
        editor.fromKml(createKmlParagraphs({"First paragraph.", "Second paragraph."}));
        collector.setBookEditor(&editor);
        REQUIRE(collector.wordCount() == 4);
        REQUIRE(collector.paragraphCount() == 2);
        collector.setBookEditor(nullptr);
    }

    SECTION("Only the paragraphs with text are paragraphs") {
        BookEditor editor;
        editor.fromKml(createKmlParagraphs({"First paragraph.", "", " ", "Second paragraph."}));
        collector.setBookEditor(&editor);
        REQUIRE(editor.paragraphCount() == 4);
        REQUIRE(collector.paragraphCount() == 2);
        collector.setBookEditor(nullptr);
    }
}

// =============================================================================
// Character Counting Tests
// =============================================================================

TEST_CASE("StatisticsCollector: Character counting", "[editor][statistics]") {
    BookEditor editor;
    editor.fromKml(createKml("Hello World"));
    StatisticsCollector collector;
    collector.setBookEditor(&editor);

    SECTION("Total character count includes spaces") {
        REQUIRE(collector.characterCount() == 11);
    }

    SECTION("Character count without spaces") {
        REQUIRE(collector.characterCountNoSpaces() == 10);
    }

    // Disconnect before editor destruction
    collector.setBookEditor(nullptr);
}

// =============================================================================
// Reading Time Tests
// =============================================================================

TEST_CASE("StatisticsCollector: Reading time estimation", "[editor][statistics]") {
    StatisticsCollector collector;

    SECTION("Empty editor has zero reading time") {
        REQUIRE(collector.estimatedReadingTime() == 0);
    }

    SECTION("Short text under 1 minute rounds up") {
        BookEditor editor;
        editor.fromKml(createKml("Hello world test"));
        collector.setBookEditor(&editor);

        // 3 words at 200 wpm = ~0.015 minutes, rounds up to 1
        REQUIRE(collector.estimatedReadingTime() == 1);
        collector.setBookEditor(nullptr);
    }
}

// =============================================================================
// Session Tracking Tests
// =============================================================================

TEST_CASE("StatisticsCollector: Session tracking", "[editor][statistics]") {
    StatisticsCollector collector;

    SECTION("Start session activates tracking") {
        collector.startSession();
        REQUIRE(collector.isSessionActive());
    }

    SECTION("End session deactivates tracking") {
        collector.startSession();
        collector.endSession();
        REQUIRE_FALSE(collector.isSessionActive());
    }

    SECTION("Starting already active session does nothing") {
        collector.startSession();
        collector.startSession();  // Should not crash
        REQUIRE(collector.isSessionActive());
        collector.endSession();
    }

    SECTION("Ending inactive session does nothing") {
        collector.endSession();  // Should not crash
        REQUIRE_FALSE(collector.isSessionActive());
    }

    SECTION("Session counters start at zero") {
        collector.startSession();
        REQUIRE(collector.wordsWrittenThisSession() == 0);
        REQUIRE(collector.wordsDeletedThisSession() == 0);
        collector.endSession();
    }
}

// =============================================================================
// Signal Tests (manual verification without QSignalSpy)
// =============================================================================

TEST_CASE("StatisticsCollector: Signals", "[editor][statistics]") {
    BookEditor editor;
    editor.fromKml(createKml("Test"));
    StatisticsCollector collector;

    // Manual signal tracking
    int signalCount = 0;
    int lastWordCount = -1;
    int lastCharCount = -1;

    QObject::connect(&collector, &StatisticsCollector::statisticsChanged,
        [&](int words, int chars, int) {
            ++signalCount;
            lastWordCount = words;
            lastCharCount = chars;
        });

    SECTION("Setting editor emits statisticsChanged") {
        collector.setBookEditor(&editor);

        REQUIRE(signalCount >= 1);
        REQUIRE(lastWordCount == 1);
        REQUIRE(lastCharCount == 4);

        collector.setBookEditor(nullptr);
    }
}

// =============================================================================
// Words Written and Deleted in the Session
// =============================================================================

TEST_CASE("StatisticsCollector: words typed and deleted count to the session",
          "[editor][statistics]") {
    BookEditor editor;
    editor.fromKml(createKml("Hello world"));
    StatisticsCollector collector;
    collector.setBookEditor(&editor);
    collector.startSession();

    editor.setCursorPosition({0, 11});
    editor.typeText(QStringLiteral(" and more"));
    waitForCount();
    CHECK(collector.wordCount() == 4);
    CHECK(collector.wordsWrittenThisSession() == 2);
    CHECK(collector.wordsDeletedThisSession() == 0);

    editor.setSelection({{0, 0}, {0, 6}});  // "Hello "
    editor.deleteSelectedText();
    waitForCount();
    CHECK(collector.wordCount() == 3);
    CHECK(collector.wordsWrittenThisSession() == 2);
    CHECK(collector.wordsDeletedThisSession() == 1);

    collector.endSession();
    collector.setBookEditor(nullptr);
}

TEST_CASE("StatisticsCollector: a text loaded into the editor is not written",
          "[editor][statistics]") {
    // As a chapter opened in the editor the collector follows already
    BookEditor editor;
    StatisticsCollector collector;
    collector.setBookEditor(&editor);
    collector.startSession();

    editor.fromKml(createKmlParagraphs({"First paragraph here.", "Second one."}));
    CHECK(collector.wordCount() == 5);
    waitForCount();
    CHECK(collector.wordsWrittenThisSession() == 0);
    CHECK(collector.wordsDeletedThisSession() == 0);

    // The words typed in it afterwards are
    editor.setCursorPosition({1, 11});
    editor.typeText(QStringLiteral(" Third"));
    waitForCount();
    CHECK(collector.wordsWrittenThisSession() == 1);

    collector.endSession();
    collector.setBookEditor(nullptr);
}

TEST_CASE("StatisticsCollector: text moved to another text is not written or deleted",
          "[editor][statistics]") {
    // Book > Chapter Break: the text from the cursor goes to a new text, and comes back with
    // its Undo
    BookEditor editor;
    editor.fromKml(createKmlParagraphs({"One two three.", "Four five six seven."}));
    StatisticsCollector collector;
    collector.setBookEditor(&editor);
    collector.startSession();
    REQUIRE(collector.wordCount() == 7);

    // The words typed right before it are counted, the words moved are not
    editor.setCursorPosition({0, 14});
    editor.typeText(QStringLiteral(" Eight"));
    REQUIRE(editor.removeTextFrom({1, 0}));
    CHECK(collector.wordCount() == 4);
    waitForCount();
    CHECK(collector.wordsWrittenThisSession() == 1);
    CHECK(collector.wordsDeletedThisSession() == 0);

    editor.undoMovedText();
    CHECK(collector.wordCount() == 8);
    waitForCount();
    CHECK(collector.wordsWrittenThisSession() == 1);
    CHECK(collector.wordsDeletedThisSession() == 0);

    // The words typed after it are writing again
    editor.setCursorPosition({1, 20});
    editor.typeText(QStringLiteral(" nine"));
    waitForCount();
    CHECK(collector.wordsWrittenThisSession() == 2);

    collector.endSession();
    collector.setBookEditor(nullptr);
}

TEST_CASE("StatisticsCollector: a cut to the clippings lowers the words, or moves them",
          "[editor][statistics][clippings]") {
    // Edit > Cut to Clippings: the selection goes to the Workshop
    BookEditor editor;
    editor.fromKml(createKmlParagraphs({"One two three.", "Four five six seven."}));
    StatisticsCollector collector;
    collector.setBookEditor(&editor);
    collector.startSession();
    REQUIRE(collector.wordCount() == 7);

    SECTION("deleted, as any deletion is") {
        editor.setSelection({.start = {.paragraph = 1, .offset = 0},
                             .end = {.paragraph = 1, .offset = 10}});
        REQUIRE(editor.takeSelection(false));
        waitForCount();
        CHECK(collector.wordCount() == 5);
        CHECK(collector.wordsDeletedThisSession() == 2);
        CHECK_FALSE(editor.hasSelection());
        // Undo writes them back, as an undone deletion does
        editor.undo();
        waitForCount();
        CHECK(collector.wordCount() == 7);
    }

    SECTION("moved: neither deleted nor written, with Undo and Redo") {
        editor.setSelection({.start = {.paragraph = 1, .offset = 0},
                             .end = {.paragraph = 1, .offset = 10}});
        REQUIRE(editor.takeSelection(true));
        waitForCount();
        CHECK(collector.wordCount() == 5);
        CHECK(collector.wordsDeletedThisSession() == 0);
        editor.undo();
        waitForCount();
        CHECK(collector.wordCount() == 7);
        CHECK(collector.wordsWrittenThisSession() == 0);
        editor.redo();
        waitForCount();
        CHECK(collector.wordCount() == 5);
        CHECK(collector.wordsDeletedThisSession() == 0);
    }

    SECTION("nothing selected: nothing taken") {
        editor.setCursorPosition({.paragraph = 1, .offset = 4});
        CHECK_FALSE(editor.takeSelection(true));
        CHECK(collector.wordCount() == 7);
    }

    collector.endSession();
    collector.setBookEditor(nullptr);
}

TEST_CASE("StatisticsCollector: new rules of counting are not words written or deleted",
          "[editor][statistics]") {
    BookEditor editor;
    editor.fromKml(createKml("– Tak – powiedział."));
    StatisticsCollector collector;
    collector.setBookEditor(&editor);
    collector.startSession();
    REQUIRE(collector.wordCount() == 2);

    kalahari::core::WordCountRules dashesAsWords;
    dashesAsWords.dashesAreWords = true;
    editor.setWordCountRules(dashesAsWords);
    CHECK(collector.wordCount() == 4);
    waitForCount();
    CHECK(collector.wordsWrittenThisSession() == 0);

    editor.setWordCountRules(kalahari::core::WordCountRules{});
    CHECK(collector.wordCount() == 2);
    waitForCount();
    CHECK(collector.wordsDeletedThisSession() == 0);

    collector.endSession();
    collector.setBookEditor(nullptr);
}

TEST_CASE("StatisticsCollector: follows one editor at a time", "[editor][statistics]") {
    BookEditor first;
    first.fromKml(createKml("One two"));
    BookEditor second;
    second.fromKml(createKml("Three four five"));
    StatisticsCollector collector;
    collector.setBookEditor(&first);
    collector.startSession();

    SECTION("the words just typed in the editor left count to the session") {
        first.setCursorPosition({0, 7});
        first.typeText(QStringLiteral(" more words"));
        collector.setBookEditor(&second);  // before the count after the edit
        CHECK(collector.wordsWrittenThisSession() == 2);

        // The text of the editor followed now is not written
        CHECK(collector.wordCount() == 3);
        waitForCount();
        CHECK(collector.wordsWrittenThisSession() == 2);
        CHECK(collector.wordsDeletedThisSession() == 0);
    }

    SECTION("the edits in the editor left are not followed") {
        collector.setBookEditor(&second);
        first.setCursorPosition({0, 7});
        first.typeText(QStringLiteral(" more words"));
        waitForCount();
        CHECK(collector.wordCount() == 3);
        CHECK(collector.wordsWrittenThisSession() == 0);
    }

    collector.endSession();
    collector.setBookEditor(nullptr);
}

TEST_CASE("StatisticsCollector: an editor closed while followed is forgotten",
          "[editor][statistics]") {
    StatisticsCollector collector;
    auto editor = std::make_unique<BookEditor>();
    editor->fromKml(createKml("Hello world"));
    collector.setBookEditor(editor.get());
    REQUIRE(collector.wordCount() == 2);

    editor.reset();
    CHECK(collector.wordCount() == 0);
    CHECK(collector.characterCount() == 0);
    CHECK(collector.paragraphCount() == 0);

    // Another editor is followed as before
    BookEditor other;
    other.fromKml(createKml("One"));
    collector.setBookEditor(&other);
    CHECK(collector.wordCount() == 1);
    collector.setBookEditor(nullptr);
}

// =============================================================================
// The Record of Writing
// =============================================================================

TEST_CASE("StatisticsCollector: the record of writing goes to the book and the journal",
          "[editor][statistics]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    kalahari::core::ProjectDatabase database;
    REQUIRE(database.open(dir.path()));
    kalahari::core::WritingJournal journal;
    REQUIRE(journal.open(dir.filePath("writing_journal.db")));

    // The panel of a chapter knows its element, and the editor is inside it
    QWidget panel;
    panel.setProperty("elementId", QStringLiteral("chapter-7"));
    auto* editor = new BookEditor(&panel);
    editor->fromKml(createKml("Hello world"));

    StatisticsCollector collector;
    collector.setDatabase(&database);
    collector.setJournal(&journal);
    collector.setBookId(QStringLiteral("book-1"));
    collector.setBookEditor(editor);
    collector.startSession();

    editor->setCursorPosition({0, 11});
    editor->typeText(QStringLiteral(" and more"));
    waitForCount();
    REQUIRE(collector.pendingActivity().size() == 1);
    const kalahari::core::WritingActivity entry = collector.pendingActivity().first();
    CHECK(entry.bookId == "book-1");
    CHECK(entry.elementId == "chapter-7");
    CHECK(entry.written == 2);
    CHECK(entry.date == QDate::currentDate());

    int saved = 0;
    QObject::connect(&collector, &StatisticsCollector::activitySaved, &collector,
                     [&saved](const QList<kalahari::core::WritingActivity>& activity) {
                         saved += int(activity.size());
                     });
    collector.flush();
    CHECK(saved == 1);
    CHECK(collector.pendingActivity().isEmpty());

    const QDate today = QDate::currentDate();
    const auto inBook = database.writingActivity(today.addDays(-1), today);
    const auto inJournal = journal.activity(today.addDays(-1), today, QStringLiteral("book-1"));
    REQUIRE(inBook.size() == 1);
    REQUIRE(inJournal.size() == 1);
    CHECK(inBook.first().written == 2);
    CHECK(inJournal.first().elementId == "chapter-7");

    collector.endSession();
    collector.setBookEditor(nullptr);
    collector.setDatabase(nullptr);
    collector.setJournal(nullptr);
}

TEST_CASE("StatisticsCollector: a large change at once is not writing", "[editor][statistics]") {
    BookEditor editor;
    editor.fromKml(createKml("Start"));
    StatisticsCollector collector;
    kalahari::core::WritingRules rules;
    rules.largeChangeWords = 5;
    collector.setRules(rules);
    collector.setBookEditor(&editor);
    collector.startSession();
    QList<int> skipped;
    QObject::connect(&collector, &StatisticsCollector::largeChangeSkipped, &collector,
                     [&skipped](int words) { skipped.append(words); });

    // Typed over a selection of more words than the limit: they are not deleted
    editor.setSelection({{0, 0}, {0, 5}});
    editor.typeText(QStringLiteral("One two three four five six seven"));
    waitForCount();
    REQUIRE(collector.wordCount() == 7);
    editor.setSelection({{0, 0}, {0, int(editor.plainText().length())}});
    editor.typeText(QStringLiteral("x"));
    waitForCount();
    CHECK(collector.wordCount() == 1);
    CHECK(collector.wordsDeletedThisSession() == 0);
    REQUIRE(skipped.size() == 1);
    CHECK(skipped.first() == 6);

    // What is written after it counts from the text with those words: the six words more
    // than "Start", and "eight"
    editor.typeText(QStringLiteral(" eight"));
    waitForCount();
    CHECK(collector.wordsWrittenThisSession() == 7);

    collector.endSession();
    collector.setBookEditor(nullptr);
}

TEST_CASE("StatisticsCollector: only the writer's own writing is words written or deleted",
          "[editor][statistics]") {
    // Test of 22.11: the graph of writing jumped with the captions, the clippings and their
    // Undo; it shows the writing only
    BookEditor editor;
    editor.fromKml(createKml("One two"));
    StatisticsCollector collector;
    collector.setBookEditor(&editor);
    collector.startSession();
    editor.setCursorPosition({0, 7});

    // Typed
    editor.typeText(QStringLiteral(" three"));
    CHECK(collector.wordsWrittenThisSession() == 1);

    // Put in by a command (a field, a caption, a replacement), with its Undo and Redo: not
    // written, not deleted
    editor.insertText(QStringLiteral(" four five"));
    CHECK_FALSE(editor.isWriting());
    CHECK(collector.wordCount() == 5);
    editor.undo();
    CHECK(collector.wordCount() == 3);
    editor.redo();
    CHECK(collector.wordCount() == 5);
    waitForCount();
    CHECK(collector.wordsWrittenThisSession() == 1);
    CHECK(collector.wordsDeletedThisSession() == 0);

    // Typed right before a command, at once: the words typed are written
    editor.typeText(QStringLiteral(" six"));
    editor.insertText(QStringLiteral(" seven eight"));
    CHECK(collector.wordsWrittenThisSession() == 2);
    CHECK(collector.wordCount() == 8);

    // The keys of the text: a character typed, Backspace
    QKeyEvent typed(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" "));
    QApplication::sendEvent(&editor, &typed);
    QKeyEvent letter(QEvent::KeyPress, Qt::Key_X, Qt::NoModifier, QStringLiteral("x"));
    QApplication::sendEvent(&editor, &letter);
    CHECK(collector.wordsWrittenThisSession() == 3);
    QKeyEvent backspace(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
    QApplication::sendEvent(&editor, &backspace);
    CHECK(collector.wordsDeletedThisSession() == 1);
    CHECK_FALSE(editor.isWriting());

    // Cut, and its Undo: deleted, and written back
    const QString text = editor.plainText();
    const int six = int(text.indexOf(QStringLiteral(" six")));
    editor.setSelection({{0, six}, {0, six + 4}});
    editor.cut();
    CHECK(collector.wordCount() == 7);
    CHECK(collector.wordsDeletedThisSession() == 2);
    editor.undo();
    CHECK(collector.wordCount() == 8);
    CHECK(collector.wordsWrittenThisSession() == 4);

    // The text of a note written in its frame (HandWriting)
    {
        const BookEditor::HandWriting hand(editor);
        CHECK(editor.isWriting());
        editor.setCursorPosition({0, int(editor.plainText().length())});
        editor.insertText(QStringLiteral(" nine"));
    }
    CHECK_FALSE(editor.isWriting());
    CHECK(collector.wordsWrittenThisSession() == 5);

    // The whole text replaced (Replace All in Book): not writing
    editor.replaceWithKml(createKml("A b c d e f g h i j k l"));
    CHECK(collector.wordCount() == 12);
    waitForCount();
    CHECK(collector.wordsWrittenThisSession() == 5);
    CHECK(collector.wordsDeletedThisSession() == 2);

    collector.endSession();
    collector.setBookEditor(nullptr);
}

TEST_CASE("StatisticsCollector: the words pasted are kept apart from the words written",
          "[editor][statistics]") {
    BookEditor editor;
    editor.fromKml(createKml("Start"));
    StatisticsCollector collector;
    kalahari::core::WritingRules rules;
    rules.largeChangeWords = 2;  // a paste is not a large change, whatever its size
    collector.setRules(rules);
    collector.setBookEditor(&editor);
    collector.startSession();
    QList<int> skipped;
    QObject::connect(&collector, &StatisticsCollector::largeChangeSkipped, &collector,
                     [&skipped](int words) { skipped.append(words); });

    // Typed, and pasted at once after it: the words typed are written, those pasted pasted
    editor.setCursorPosition({0, 5});
    editor.typeText(QStringLiteral(" typed "));
    QMimeData data;
    data.setText(QStringLiteral("one two three"));
    editor.insertFromMimeData(&data);
    CHECK(collector.wordCount() == 5);
    CHECK(collector.wordsWrittenThisSession() == 1);
    CHECK(collector.wordsPastedThisSession() == 3);
    waitForCount();
    CHECK(collector.wordsWrittenThisSession() == 1);
    CHECK(collector.wordsPastedThisSession() == 3);
    CHECK(skipped.isEmpty());
    int written = 0;
    int pasted = 0;
    for (const kalahari::core::WritingActivity& entry : collector.pendingActivity()) {
        written += entry.written;
        pasted += entry.pasted;
    }
    CHECK(written == 1);
    CHECK(pasted == 3);

    // Undone and redone, the paste takes its words away and brings them back: neither
    // deleted nor written
    editor.undo();
    waitForCount();
    CHECK(collector.wordCount() == 2);
    CHECK(collector.wordsPastedThisSession() == 0);
    CHECK(collector.wordsDeletedThisSession() == 0);
    editor.redo();
    waitForCount();
    CHECK(collector.wordCount() == 5);
    CHECK(collector.wordsPastedThisSession() == 3);
    CHECK(collector.wordsWrittenThisSession() == 1);

    // Typing after it is writing again
    editor.setCursorPosition({0, int(editor.plainText().length())});
    editor.typeText(QStringLiteral(" more"));
    waitForCount();
    CHECK(collector.wordsWrittenThisSession() == 2);
    CHECK(collector.wordsPastedThisSession() == 3);

    // Typed right after the paste, it is undone with it: the words typed are deleted, the
    // words pasted taken away
    editor.undo();
    waitForCount();
    CHECK(editor.plainText() == QStringLiteral("Start typed "));
    CHECK(collector.wordsDeletedThisSession() == 1);
    CHECK(collector.wordsPastedThisSession() == 0);
    editor.redo();
    waitForCount();
    CHECK(collector.wordsWrittenThisSession() == 3);
    CHECK(collector.wordsPastedThisSession() == 3);

    // Undoing what was typed in a level of its own is not undoing a paste
    editor.setCursorPosition({0, 0});
    editor.typeText(QStringLiteral("New "));
    waitForCount();
    editor.undo();
    waitForCount();
    CHECK(collector.wordsDeletedThisSession() == 2);
    CHECK(collector.wordsPastedThisSession() == 3);

    collector.endSession();
    collector.setBookEditor(nullptr);
}

TEST_CASE("StatisticsCollector: a paste keeps its words while another text is followed",
          "[editor][statistics]") {
    BookEditor chapterA;
    chapterA.fromKml(createKml("Start"));
    BookEditor chapterB;
    chapterB.fromKml(createKml("Other"));
    StatisticsCollector collector;
    collector.setBookEditor(&chapterA);
    collector.startSession();

    // Pasted, and typed right after it, in its level of undo
    chapterA.setCursorPosition({0, 5});
    QMimeData data;
    data.setText(QStringLiteral(" one two three"));
    chapterA.insertFromMimeData(&data);
    chapterA.typeText(QStringLiteral(" more"));
    waitForCount();
    CHECK(collector.wordsPastedThisSession() == 3);
    CHECK(collector.wordsWrittenThisSession() == 1);

    // Regression: another text followed in between forgot the words of the paste, and its
    // undo took away four pasted words, the word typed not deleted
    collector.setBookEditor(&chapterB);
    collector.setBookEditor(&chapterA);
    chapterA.undo();
    waitForCount();
    CHECK(chapterA.plainText() == QStringLiteral("Start"));
    CHECK(collector.wordsPastedThisSession() == 0);
    CHECK(collector.wordsDeletedThisSession() == 1);

    // An editor left and closed takes its pastes with it
    {
        BookEditor closed;
        closed.fromKml(createKml("Closed"));
        collector.setBookEditor(&closed);
        closed.setCursorPosition({0, 6});
        closed.insertFromMimeData(&data);
        collector.setBookEditor(&chapterB);
    }
    CHECK(collector.wordsPastedThisSession() == 3);

    collector.endSession();
    collector.setBookEditor(nullptr);
}
