/// @file test_statistics_collector.cpp
/// @brief Unit tests for StatisticsCollector (OpenSpec #00042 Task 7.17)

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <kalahari/editor/statistics_collector.h>
#include <kalahari/editor/kml_document.h>
#include <kalahari/editor/kml_paragraph.h>
#include <kalahari/editor/kml_text_run.h>

#include <memory>

using namespace kalahari::editor;

// =============================================================================
// Helper: Create document with text
// =============================================================================
namespace {

std::unique_ptr<KmlDocument> createDocumentWithText(const QString& text) {
    auto doc = std::make_unique<KmlDocument>();
    auto para = std::make_unique<KmlParagraph>();
    para->addElement(std::make_unique<KmlTextRun>(text));
    doc->addParagraph(std::move(para));
    return doc;
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
// Document Connection Tests
// =============================================================================

TEST_CASE("StatisticsCollector: Document connection", "[editor][statistics]") {
    // CRITICAL: Document must be created BEFORE collector for proper destruction order
    // When objects go out of scope, collector is destroyed first and calls removeObserver
    // on still-valid document
    auto doc = createDocumentWithText("Hello world");
    StatisticsCollector collector;

    SECTION("Connecting document updates stats") {
        collector.setDocument(doc.get());

        REQUIRE(collector.wordCount() == 2);
        REQUIRE(collector.characterCount() == 11);
        REQUIRE(collector.paragraphCount() == 1);
    }

    SECTION("Disconnecting document resets stats") {
        collector.setDocument(doc.get());
        REQUIRE(collector.wordCount() == 2);

        collector.setDocument(nullptr);
        REQUIRE(collector.wordCount() == 0);
        REQUIRE(collector.characterCount() == 0);
    }

    SECTION("Setting same document does nothing") {
        collector.setDocument(doc.get());
        int count1 = collector.wordCount();

        collector.setDocument(doc.get());
        int count2 = collector.wordCount();

        REQUIRE(count1 == count2);
    }
}

// =============================================================================
// Word Counting Tests
// =============================================================================

TEST_CASE("StatisticsCollector: Word counting", "[editor][statistics]") {
    auto doc = std::make_unique<KmlDocument>();
    StatisticsCollector collector;

    SECTION("Empty document has zero words") {
        collector.setDocument(doc.get());
        REQUIRE(collector.wordCount() == 0);
    }

    SECTION("Single word") {
        auto para = std::make_unique<KmlParagraph>();
        para->addElement(std::make_unique<KmlTextRun>("Hello"));
        doc->addParagraph(std::move(para));

        collector.setDocument(doc.get());
        REQUIRE(collector.wordCount() == 1);
    }

    SECTION("Multiple words with spaces") {
        auto para = std::make_unique<KmlParagraph>();
        para->addElement(std::make_unique<KmlTextRun>("The quick brown fox"));
        doc->addParagraph(std::move(para));

        collector.setDocument(doc.get());
        REQUIRE(collector.wordCount() == 4);
    }

    SECTION("Words with punctuation") {
        auto para = std::make_unique<KmlParagraph>();
        para->addElement(std::make_unique<KmlTextRun>("Hello, world! How are you?"));
        doc->addParagraph(std::move(para));

        collector.setDocument(doc.get());
        REQUIRE(collector.wordCount() == 5);
    }

    SECTION("Multiple paragraphs") {
        auto para1 = std::make_unique<KmlParagraph>();
        para1->addElement(std::make_unique<KmlTextRun>("First paragraph."));
        doc->addParagraph(std::move(para1));

        auto para2 = std::make_unique<KmlParagraph>();
        para2->addElement(std::make_unique<KmlTextRun>("Second paragraph."));
        doc->addParagraph(std::move(para2));

        collector.setDocument(doc.get());
        REQUIRE(collector.wordCount() == 4);
        REQUIRE(collector.paragraphCount() == 2);
    }

    // Disconnect before doc destruction
    collector.setDocument(nullptr);
}

// =============================================================================
// Character Counting Tests
// =============================================================================

TEST_CASE("StatisticsCollector: Character counting", "[editor][statistics]") {
    auto doc = createDocumentWithText("Hello World");
    StatisticsCollector collector;
    collector.setDocument(doc.get());

    SECTION("Total character count includes spaces") {
        REQUIRE(collector.characterCount() == 11);
    }

    SECTION("Character count without spaces") {
        REQUIRE(collector.characterCountNoSpaces() == 10);
    }

    // Disconnect before doc destruction
    collector.setDocument(nullptr);
}

// =============================================================================
// Reading Time Tests
// =============================================================================

TEST_CASE("StatisticsCollector: Reading time estimation", "[editor][statistics]") {
    StatisticsCollector collector;

    SECTION("Empty document has zero reading time") {
        REQUIRE(collector.estimatedReadingTime() == 0);
    }

    SECTION("Short text under 1 minute rounds up") {
        auto doc = createDocumentWithText("Hello world test");
        collector.setDocument(doc.get());

        // 3 words at 200 wpm = ~0.015 minutes, rounds up to 1
        REQUIRE(collector.estimatedReadingTime() == 1);

        collector.setDocument(nullptr);
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
    auto doc = std::make_unique<KmlDocument>();
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

    SECTION("Setting document emits statisticsChanged") {
        auto para = std::make_unique<KmlParagraph>();
        para->addElement(std::make_unique<KmlTextRun>("Test"));
        doc->addParagraph(std::move(para));

        collector.setDocument(doc.get());

        REQUIRE(signalCount >= 1);
        REQUIRE(lastWordCount == 1);
        REQUIRE(lastCharCount == 4);
    }

    // Disconnect before doc destruction
    collector.setDocument(nullptr);
}
