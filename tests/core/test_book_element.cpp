/// @file test_book_element.cpp
/// @brief Unit tests for BookElement (Task #00012)
///
/// Tests cover:
/// - Construction with parameters
/// - Getters/Setters operations
/// - Metadata operations (set/get/remove)
/// - Known type validation (isKnownType)
/// - JSON serialization round-trip (toJson/fromJson)
/// - Timestamp handling (created/modified)
/// - Word count operations

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_element.h>
#include <thread>
#include <chrono>

using namespace kalahari::core;

// =============================================================================
// Construction Tests
// =============================================================================

TEST_CASE("BookElement construction", "[core][book][element]") {
    SECTION("Constructor with all parameters") {
        BookElement element("chapter", "test-001", "Chapter 1", "content/chapter_001.rtf");

        REQUIRE(element.getType() == "chapter");
        REQUIRE(element.getId() == "test-001");
        REQUIRE(element.getTitle() == "Chapter 1");
        REQUIRE(element.getFile().generic_string() == "content/chapter_001.rtf");
        REQUIRE(element.getWordCount() == 0);  // Default
    }

    SECTION("Constructor with default file path") {
        BookElement element("title_page", "front-001", "Title Page");

        REQUIRE(element.getType() == "title_page");
        REQUIRE(element.getFile().empty());
    }

    SECTION("Constructor with empty file path") {
        BookElement element("dedication", "front-002", "Dedication", "");

        REQUIRE(element.getFile().empty());
    }
}

// =============================================================================
// Getters/Setters Tests
// =============================================================================

TEST_CASE("BookElement getters and setters", "[core][book][element]") {
    BookElement element("chapter", "test-001", "Original Title", "original.rtf");

    SECTION("Set and get title") {
        element.setTitle("New Title");
        REQUIRE(element.getTitle() == "New Title");
    }

    SECTION("Set and get file path") {
        element.setFile("content/new_chapter.rtf");
        REQUIRE(element.getFile().generic_string() == "content/new_chapter.rtf");
    }

    SECTION("Set and get word count") {
        element.setWordCount(2500);
        REQUIRE(element.getWordCount() == 2500);
    }

    SECTION("Touch updates modified timestamp") {
        auto before = element.getModified();

        // Wait 10ms to ensure timestamp difference
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        element.touch();
        auto after = element.getModified();

        REQUIRE(after > before);
    }
}

// =============================================================================
// Metadata Operations Tests
// =============================================================================

TEST_CASE("BookElement metadata operations", "[core][book][element][metadata]") {
    BookElement element("chapter", "test-001", "Chapter 1");

    SECTION("Set and get metadata") {
        element.setMetadata("pov", "First Person");
        element.setMetadata("location", "London");

        auto pov = element.getMetadata("pov");
        REQUIRE(pov.has_value());
        REQUIRE(*pov == "First Person");

        auto location = element.getMetadata("location");
        REQUIRE(location.has_value());
        REQUIRE(*location == "London");
    }

    SECTION("Get non-existent metadata returns nullopt") {
        auto result = element.getMetadata("non_existent");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("Overwrite existing metadata") {
        element.setMetadata("pov", "First Person");
        element.setMetadata("pov", "Third Person");

        auto pov = element.getMetadata("pov");
        REQUIRE(*pov == "Third Person");
    }

    SECTION("Remove metadata") {
        element.setMetadata("temp", "value");
        REQUIRE(element.getMetadata("temp").has_value());

        element.removeMetadata("temp");
        REQUIRE_FALSE(element.getMetadata("temp").has_value());
    }

    SECTION("Remove non-existent metadata does not throw") {
        REQUIRE_NOTHROW(element.removeMetadata("non_existent"));
    }
}

// =============================================================================
// Known Types Tests
// =============================================================================

TEST_CASE("BookElement known type validation", "[core][book][element][types]") {
    SECTION("Known front matter types") {
        REQUIRE(BookElement("title_page", "id", "Title").isKnownType());
        REQUIRE(BookElement("copyright", "id", "Copyright").isKnownType());
        REQUIRE(BookElement("dedication", "id", "Dedication").isKnownType());
        REQUIRE(BookElement("preface", "id", "Preface").isKnownType());
    }

    SECTION("Known body type") {
        REQUIRE(BookElement("chapter", "id", "Chapter").isKnownType());
    }

    SECTION("Known back matter types") {
        REQUIRE(BookElement("epilogue", "id", "Epilogue").isKnownType());
        REQUIRE(BookElement("glossary", "id", "Glossary").isKnownType());
        REQUIRE(BookElement("bibliography", "id", "Bibliography").isKnownType());
        REQUIRE(BookElement("about_author", "id", "About").isKnownType());
    }

    SECTION("Unknown custom type") {
        REQUIRE_FALSE(BookElement("custom_section", "id", "Custom").isKnownType());
        REQUIRE_FALSE(BookElement("character_notes", "id", "Notes").isKnownType());
    }
}

// =============================================================================
// JSON Serialization Tests
// =============================================================================

TEST_CASE("BookElement JSON serialization", "[core][book][element][json]") {
    SECTION("toJson with all fields") {
        BookElement element("chapter", "test-001", "Chapter 1", "content/chapter_001.rtf");
        element.setWordCount(2500);
        element.setMetadata("pov", "First Person");
        element.setMetadata("location", "Paris");

        json j = element.toJson();

        REQUIRE(j["type"] == "chapter");
        REQUIRE(j["id"] == "test-001");
        REQUIRE(j["title"] == "Chapter 1");
        REQUIRE(j["file"] == "content/chapter_001.rtf");
        REQUIRE(j["wordCount"] == 2500);
        REQUIRE(j.contains("created"));
        REQUIRE(j.contains("modified"));
        REQUIRE(j["metadata"]["pov"] == "First Person");
        REQUIRE(j["metadata"]["location"] == "Paris");
    }

    SECTION("toJson with empty metadata") {
        BookElement element("title_page", "front-001", "Title Page");

        json j = element.toJson();

        REQUIRE(j["metadata"].is_object());
        REQUIRE(j["metadata"].empty());
    }

    SECTION("fromJson with all fields") {
        json j = {
            {"type", "chapter"},
            {"id", "ch-001"},
            {"title", "The Beginning"},
            {"file", "content/body/chapter_001.rtf"},
            {"wordCount", 3500},
            {"created", "2025-10-30T10:00:00Z"},
            {"modified", "2025-10-30T15:30:00Z"},
            {"metadata", {
                {"pov", "Third Person"},
                {"scene", "Night"}
            }}
        };

        BookElement element = BookElement::fromJson(j);

        REQUIRE(element.getType() == "chapter");
        REQUIRE(element.getId() == "ch-001");
        REQUIRE(element.getTitle() == "The Beginning");
        REQUIRE(element.getFile().generic_string() == "content/body/chapter_001.rtf");
        REQUIRE(element.getWordCount() == 3500);

        auto pov = element.getMetadata("pov");
        REQUIRE(pov.has_value());
        REQUIRE(*pov == "Third Person");

        auto scene = element.getMetadata("scene");
        REQUIRE(scene.has_value());
        REQUIRE(*scene == "Night");
    }

    SECTION("fromJson without optional fields") {
        json j = {
            {"type", "preface"},
            {"id", "pre-001"},
            {"title", "Preface"},
            {"file", "content/frontmatter/preface.rtf"}
        };

        BookElement element = BookElement::fromJson(j);

        REQUIRE(element.getType() == "preface");
        REQUIRE(element.getId() == "pre-001");
        REQUIRE(element.getWordCount() == 0);  // Default
    }

    SECTION("Round-trip serialization preserves data") {
        BookElement original("epilogue", "back-001", "Epilogue", "content/epilogue.rtf");
        original.setWordCount(1200);
        original.setMetadata("mood", "Reflective");

        json j = original.toJson();
        BookElement deserialized = BookElement::fromJson(j);

        REQUIRE(deserialized.getType() == original.getType());
        REQUIRE(deserialized.getId() == original.getId());
        REQUIRE(deserialized.getTitle() == original.getTitle());
        REQUIRE(deserialized.getFile() == original.getFile());
        REQUIRE(deserialized.getWordCount() == original.getWordCount());

        auto mood = deserialized.getMetadata("mood");
        REQUIRE(mood.has_value());
        REQUIRE(*mood == "Reflective");
    }
}

// =============================================================================
// Timestamp Tests
// =============================================================================

TEST_CASE("BookElement timestamp handling", "[core][book][element][timestamps]") {
    SECTION("Created and modified timestamps are set on construction") {
        BookElement element("chapter", "test-001", "Chapter 1");

        auto created = element.getCreated();
        auto modified = element.getModified();

        REQUIRE(created.time_since_epoch().count() > 0);
        REQUIRE(modified.time_since_epoch().count() > 0);
    }

    SECTION("setTitle updates modified timestamp") {
        BookElement element("chapter", "test-001", "Original");
        auto before = element.getModified();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        element.setTitle("Updated");
        auto after = element.getModified();

        REQUIRE(after > before);
    }

    SECTION("setFile updates modified timestamp") {
        BookElement element("chapter", "test-001", "Title", "original.rtf");
        auto before = element.getModified();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        element.setFile("updated.rtf");
        auto after = element.getModified();

        REQUIRE(after > before);
    }

    SECTION("setWordCount updates modified timestamp") {
        BookElement element("chapter", "test-001", "Title");
        auto before = element.getModified();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        element.setWordCount(1000);
        auto after = element.getModified();

        REQUIRE(after > before);
    }
}

// =============================================================================
// Edge Cases Tests
// =============================================================================

TEST_CASE("BookElement edge cases", "[core][book][element][edge]") {
    SECTION("Empty type string") {
        BookElement element("", "id-001", "Title");
        REQUIRE(element.getType().empty());
        REQUIRE_FALSE(element.isKnownType());
    }

    SECTION("Empty ID string") {
        BookElement element("chapter", "", "Title");
        REQUIRE(element.getId().empty());
    }

    SECTION("Empty title string") {
        BookElement element("chapter", "id-001", "");
        REQUIRE(element.getTitle().empty());
    }

    SECTION("Very long metadata value") {
        BookElement element("chapter", "id-001", "Title");
        std::string longValue(10000, 'x');  // 10k characters

        element.setMetadata("long_field", longValue);

        auto retrieved = element.getMetadata("long_field");
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->size() == 10000);
    }

    SECTION("Special characters in metadata") {
        BookElement element("chapter", "id-001", "Title");

        element.setMetadata("special", "Value with \"quotes\" and \nnewlines\t tabs");

        auto retrieved = element.getMetadata("special");
        REQUIRE(retrieved.has_value());
        REQUIRE(*retrieved == "Value with \"quotes\" and \nnewlines\t tabs");
    }

    SECTION("Negative word count") {
        BookElement element("chapter", "id-001", "Title");

        // Should accept negative (might be used for deltas or corrections)
        element.setWordCount(-100);
        REQUIRE(element.getWordCount() == -100);
    }
}
