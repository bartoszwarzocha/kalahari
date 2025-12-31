/// @file test_statistics_collector.cpp
/// @brief Unit tests for StatisticsCollector (OpenSpec #00042 Task 7.17)
/// Phase 11: Rewritten for QTextDocument-based architecture

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <kalahari/editor/statistics_collector.h>
#include <kalahari/editor/book_editor.h>

#include <QApplication>
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
