/// @file test_status_bar_statistics.cpp
/// @brief The words, characters and reading time of the document in front, in the status bar

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/core/text_statistics.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/widgets/status_bar_statistics.h"

#include <QLabel>
#include <QLocale>
#include <QStatusBar>

#include <memory>

using kalahari::core::READING_WORDS_PER_MINUTE;
using kalahari::editor::BookEditor;
using kalahari::gui::StatusBarStatistics;
namespace test = kalahari::test;

namespace {

/// Run the event loop past the interval of the counts, so that they show the last change
void waitForCounts() {
    test::runEventLoop(StatusBarStatistics::INTERVAL_MS * 3);
}

/// A number as the status bar writes it, in the system's way ("3,480" or "3 480")
QString number(int value) {
    return QLocale().toString(value);
}

/// The three labels are hidden (true) or shown (false)
bool allHidden(const StatusBarStatistics& statistics) {
    return statistics.wordsLabel()->isHidden() && statistics.charactersLabel()->isHidden() &&
           statistics.readingTimeLabel()->isHidden();
}

bool noneHidden(const StatusBarStatistics& statistics) {
    return !statistics.wordsLabel()->isHidden() && !statistics.charactersLabel()->isHidden() &&
           !statistics.readingTimeLabel()->isHidden();
}

/// One paragraph of @p count words
QString wordsText(int count) {
    QStringList text;
    for (int i = 0; i < count; ++i) {
        text << QStringLiteral("word");
    }
    return text.join(QLatin1Char(' '));
}

}  // anonymous namespace

TEST_CASE("Status bar counts: hidden without a document in front", "[gui][statistics]") {
    QStatusBar statusBar;
    StatusBarStatistics statistics(&statusBar);
    CHECK(allHidden(statistics));

    // On the right of the status bar
    CHECK(statistics.wordsLabel()->parent() == &statusBar);
    CHECK(statistics.charactersLabel()->parent() == &statusBar);
    CHECK(statistics.readingTimeLabel()->parent() == &statusBar);

    statistics.setEditor(nullptr);
    CHECK(allHidden(statistics));
}

TEST_CASE("Status bar counts: the words, characters and reading time of the text",
          "[gui][statistics]") {
    QStatusBar statusBar;
    StatusBarStatistics statistics(&statusBar);
    BookEditor editor;
    editor.fromKml(test::kmlOf({QStringLiteral("One two three"), QStringLiteral("Four")}));

    statistics.setEditor(&editor);
    CHECK(noneHidden(statistics));
    CHECK(statistics.wordsLabel()->text() == QStringLiteral("Words: 4"));
    // The paragraph ends are not characters
    CHECK(statistics.charactersLabel()->text() == QStringLiteral("Characters: 17"));
    CHECK(statistics.readingTimeLabel()->text() == QStringLiteral("Reading: 1 min"));

    SECTION("no editor in front hides them again") {
        statistics.setEditor(nullptr);
        CHECK(allHidden(statistics));
    }

    SECTION("an empty text reads in no time") {
        editor.fromKml(test::kmlOf({QString()}));
        waitForCounts();
        CHECK(statistics.wordsLabel()->text() == QStringLiteral("Words: 0"));
        CHECK(statistics.charactersLabel()->text() == QStringLiteral("Characters: 0"));
        CHECK(statistics.readingTimeLabel()->text() == QStringLiteral("Reading: 0 min"));
    }
}

TEST_CASE("Status bar counts: large numbers in the system's way, a minute begun is a minute",
          "[gui][statistics]") {
    QStatusBar statusBar;
    StatusBarStatistics statistics(&statusBar);
    BookEditor editor;
    // One word more than two minutes of reading
    const int words = READING_WORDS_PER_MINUTE * 2 + 1;
    editor.fromKml(test::kmlOf({wordsText(words)}));

    statistics.setEditor(&editor);
    CHECK(statistics.wordsLabel()->text() == QStringLiteral("Words: %1").arg(number(words)));
    CHECK(statistics.charactersLabel()->text() ==
          QStringLiteral("Characters: %1").arg(number(words * 5 - 1)));
    CHECK(statistics.readingTimeLabel()->text() == QStringLiteral("Reading: 3 min"));
}

TEST_CASE("Status bar counts: a reading time of an hour or more in hours and minutes",
          "[gui][statistics]") {
    QStatusBar statusBar;
    StatusBarStatistics statistics(&statusBar);
    BookEditor editor;
    // One word more than an hour of reading
    editor.fromKml(test::kmlOf({wordsText(READING_WORDS_PER_MINUTE * 60 + 1)}));

    statistics.setEditor(&editor);
    CHECK(statistics.readingTimeLabel()->text() == QStringLiteral("Reading: 1 h 1 min"));
}

TEST_CASE("Status bar counts: with a selection, its counts out of the whole text",
          "[gui][statistics]") {
    QStatusBar statusBar;
    StatusBarStatistics statistics(&statusBar);
    BookEditor editor;
    editor.fromKml(test::kmlOf({QStringLiteral("One two three"), QStringLiteral("Four")}));
    statistics.setEditor(&editor);

    editor.setSelection({{0, 4}, {1, 2}});  // "two three", the paragraph end, "Fo"
    waitForCounts();
    CHECK(statistics.wordsLabel()->text() == QStringLiteral("Words: 3 of 4"));
    CHECK(statistics.charactersLabel()->text() == QStringLiteral("Characters: 11 of 17"));
    // The reading time is the whole text's
    CHECK(statistics.readingTimeLabel()->text() == QStringLiteral("Reading: 1 min"));

    editor.clearSelection();
    waitForCounts();
    CHECK(statistics.wordsLabel()->text() == QStringLiteral("Words: 4"));
    CHECK(statistics.charactersLabel()->text() == QStringLiteral("Characters: 17"));
}

TEST_CASE("Status bar counts: follow the edits, at most every interval", "[gui][statistics]") {
    QStatusBar statusBar;
    StatusBarStatistics statistics(&statusBar);
    BookEditor editor;
    editor.fromKml(test::kmlOf({QStringLiteral("One two")}));
    statistics.setEditor(&editor);
    REQUIRE(statistics.wordsLabel()->text() == QStringLiteral("Words: 2"));

    editor.setCursorPosition({0, 7});
    editor.insertText(QStringLiteral(" three"));
    // Not at every key: once the interval has passed
    CHECK(statistics.wordsLabel()->text() == QStringLiteral("Words: 2"));
    waitForCounts();
    CHECK(statistics.wordsLabel()->text() == QStringLiteral("Words: 3"));
    CHECK(statistics.charactersLabel()->text() == QStringLiteral("Characters: 13"));

    // A text loaded into the editor
    editor.fromKml(test::kmlOf({QStringLiteral("A"), QStringLiteral("B C")}));
    waitForCounts();
    CHECK(statistics.wordsLabel()->text() == QStringLiteral("Words: 3"));
    CHECK(statistics.charactersLabel()->text() == QStringLiteral("Characters: 4"));
}

TEST_CASE("Status bar counts: follow new rules of counting", "[gui][statistics]") {
    QStatusBar statusBar;
    StatusBarStatistics statistics(&statusBar);
    BookEditor editor;
    editor.fromKml(test::kmlOf({QStringLiteral("– Tak – powiedział.")}));
    statistics.setEditor(&editor);
    REQUIRE(statistics.wordsLabel()->text() == QStringLiteral("Words: 2"));

    kalahari::core::WordCountRules dashesAsWords;
    dashesAsWords.dashesAreWords = true;
    editor.setWordCountRules(dashesAsWords);
    waitForCounts();
    CHECK(statistics.wordsLabel()->text() == QStringLiteral("Words: 4"));
    CHECK(statistics.charactersLabel()->text() == QStringLiteral("Characters: 19"));
}

TEST_CASE("Status bar counts: the editor in front only", "[gui][statistics]") {
    QStatusBar statusBar;
    StatusBarStatistics statistics(&statusBar);
    BookEditor first;
    first.fromKml(test::kmlOf({QStringLiteral("One two")}));
    BookEditor second;
    second.fromKml(test::kmlOf({QStringLiteral("Three four five")}));

    statistics.setEditor(&first);
    CHECK(statistics.wordsLabel()->text() == QStringLiteral("Words: 2"));
    statistics.setEditor(&second);
    CHECK(statistics.wordsLabel()->text() == QStringLiteral("Words: 3"));

    // The edits of the editor behind change nothing
    first.setCursorPosition({0, 7});
    first.insertText(QStringLiteral(" more words"));
    waitForCounts();
    CHECK(statistics.wordsLabel()->text() == QStringLiteral("Words: 3"));

    statistics.setEditor(&first);
    CHECK(statistics.wordsLabel()->text() == QStringLiteral("Words: 4"));
}

TEST_CASE("Status bar counts: a closed editor hides them", "[gui][statistics]") {
    QStatusBar statusBar;
    StatusBarStatistics statistics(&statusBar);
    auto editor = std::make_unique<BookEditor>();
    editor->fromKml(test::kmlOf({QStringLiteral("One two")}));
    statistics.setEditor(editor.get());
    REQUIRE(noneHidden(statistics));

    // An edit waits for the interval when the editor closes
    editor->setCursorPosition({0, 7});
    editor->insertText(QStringLiteral(" three"));
    editor.reset();
    CHECK(allHidden(statistics));
    waitForCounts();
    CHECK(allHidden(statistics));
}
