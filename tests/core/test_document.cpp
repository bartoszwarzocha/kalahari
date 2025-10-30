/// @file test_document.cpp
/// @brief Unit tests for Document (Task #00012)
///
/// Tests cover:
/// - Construction with parameters
/// - UUID generation and uniqueness
/// - Getters/Setters operations
/// - Timestamp handling (created/modified)
/// - Book structure access (mutable/const)
/// - JSON serialization round-trip
/// - Save/load stub operations (Phase 0)
/// - Edge cases (empty strings, special characters)

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/document.h>
#include <thread>
#include <chrono>

using namespace kalahari::core;

// =============================================================================
// Construction Tests
// =============================================================================

TEST_CASE("Document construction", "[core][document]") {
    SECTION("Constructor with all parameters") {
        Document doc("The Great Adventure", "Jane Doe", "en");

        REQUIRE(doc.getTitle() == "The Great Adventure");
        REQUIRE(doc.getAuthor() == "Jane Doe");
        REQUIRE(doc.getLanguage() == "en");
        REQUIRE(doc.getGenre().empty());  // Default
        REQUIRE_FALSE(doc.getId().empty());  // UUID generated
    }

    SECTION("Constructor with default language") {
        Document doc("My Novel", "John Smith");

        REQUIRE(doc.getLanguage() == "en");  // Default language
    }

    SECTION("Constructor with custom language") {
        Document doc("Moja Powieść", "Jan Kowalski", "pl");

        REQUIRE(doc.getLanguage() == "pl");
    }

    SECTION("Default constructor creates empty document") {
        Document doc;

        REQUIRE(doc.getTitle().empty());
        REQUIRE(doc.getAuthor().empty());
        REQUIRE(doc.getLanguage().empty());
        REQUIRE(doc.getGenre().empty());
        REQUIRE(doc.getId().empty());  // No UUID until explicit set
    }
}

// =============================================================================
// UUID Generation Tests
// =============================================================================

TEST_CASE("Document UUID generation", "[core][document][uuid]") {
    SECTION("generateId() creates non-empty UUID") {
        std::string uuid = Document::generateId();

        REQUIRE_FALSE(uuid.empty());
    }

    SECTION("generateId() creates unique IDs") {
        std::string uuid1 = Document::generateId();
        std::string uuid2 = Document::generateId();

        REQUIRE(uuid1 != uuid2);
    }

    SECTION("UUID has expected format (timestamp-random)") {
        std::string uuid = Document::generateId();

        // Should contain hyphen separator
        REQUIRE(uuid.find('-') != std::string::npos);

        // Split by hyphen
        size_t hyphenPos = uuid.find('-');
        std::string timestamp = uuid.substr(0, hyphenPos);
        std::string random = uuid.substr(hyphenPos + 1);

        // Timestamp should be numeric
        REQUIRE_FALSE(timestamp.empty());
        REQUIRE(timestamp.find_first_not_of("0123456789") == std::string::npos);

        // Random part should be hex
        REQUIRE_FALSE(random.empty());
        REQUIRE(random.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos);
    }

    SECTION("Constructor assigns unique UUID") {
        Document doc1("Title 1", "Author 1");
        Document doc2("Title 2", "Author 2");

        REQUIRE(doc1.getId() != doc2.getId());
        REQUIRE_FALSE(doc1.getId().empty());
        REQUIRE_FALSE(doc2.getId().empty());
    }
}

// =============================================================================
// Getters/Setters Tests
// =============================================================================

TEST_CASE("Document getters and setters", "[core][document]") {
    Document doc("Original Title", "Original Author", "en");

    SECTION("Set and get title") {
        doc.setTitle("New Title");
        REQUIRE(doc.getTitle() == "New Title");
    }

    SECTION("Set and get author") {
        doc.setAuthor("New Author");
        REQUIRE(doc.getAuthor() == "New Author");
    }

    SECTION("Set and get language") {
        doc.setLanguage("pl");
        REQUIRE(doc.getLanguage() == "pl");
    }

    SECTION("Set and get genre") {
        doc.setGenre("fiction");
        REQUIRE(doc.getGenre() == "fiction");
    }

    SECTION("Setters update modified timestamp") {
        auto before = doc.getModified();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        doc.setTitle("Updated Title");
        auto after = doc.getModified();

        REQUIRE(after > before);
    }
}

// =============================================================================
// Book Access Tests
// =============================================================================

TEST_CASE("Document book structure access", "[core][document][book]") {
    Document doc("Test Document", "Test Author", "en");

    SECTION("Get mutable book reference") {
        Book& book = doc.getBook();

        auto part = std::make_shared<Part>("part-001", "Part I");
        book.addPart(part);

        REQUIRE(doc.getBook().getPartCount() == 1);
    }

    SECTION("Get const book reference") {
        const Document& constDoc = doc;
        const Book& book = constDoc.getBook();

        REQUIRE(book.isEmpty());
    }

    SECTION("Book modifications persist") {
        Book& book = doc.getBook();

        auto part = std::make_shared<Part>("part-001", "Part I");
        auto chapter = std::make_shared<BookElement>("chapter", "ch-001", "Chapter 1");
        chapter->setWordCount(2500);
        part->addChapter(chapter);
        book.addPart(part);

        REQUIRE(doc.getBook().getWordCount() == 2500);
    }
}

// =============================================================================
// Timestamp Tests
// =============================================================================

TEST_CASE("Document timestamp handling", "[core][document][timestamps]") {
    SECTION("Created and modified timestamps are set on construction") {
        Document doc("Test", "Author", "en");

        auto created = doc.getCreated();
        auto modified = doc.getModified();

        REQUIRE(created.time_since_epoch().count() > 0);
        REQUIRE(modified.time_since_epoch().count() > 0);
    }

    SECTION("touch() updates modified timestamp") {
        Document doc("Test", "Author", "en");
        auto before = doc.getModified();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        doc.touch();
        auto after = doc.getModified();

        REQUIRE(after > before);
    }

    SECTION("setTitle() updates modified timestamp") {
        Document doc("Test", "Author", "en");
        auto before = doc.getModified();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        doc.setTitle("Updated");
        auto after = doc.getModified();

        REQUIRE(after > before);
    }

    SECTION("setAuthor() updates modified timestamp") {
        Document doc("Test", "Author", "en");
        auto before = doc.getModified();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        doc.setAuthor("New Author");
        auto after = doc.getModified();

        REQUIRE(after > before);
    }

    SECTION("setLanguage() updates modified timestamp") {
        Document doc("Test", "Author", "en");
        auto before = doc.getModified();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        doc.setLanguage("pl");
        auto after = doc.getModified();

        REQUIRE(after > before);
    }

    SECTION("setGenre() updates modified timestamp") {
        Document doc("Test", "Author", "en");
        auto before = doc.getModified();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        doc.setGenre("fiction");
        auto after = doc.getModified();

        REQUIRE(after > before);
    }

    SECTION("Created timestamp is immutable") {
        Document doc("Test", "Author", "en");
        auto created = doc.getCreated();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        doc.setTitle("Updated");

        REQUIRE(doc.getCreated() == created);  // Created unchanged
    }
}

// =============================================================================
// JSON Serialization Tests
// =============================================================================

TEST_CASE("Document JSON serialization", "[core][document][json]") {
    SECTION("toJson with complete structure") {
        Document doc("The Great Adventure", "Jane Doe", "en");
        doc.setGenre("fiction");

        // Add book content
        Book& book = doc.getBook();
        auto part = std::make_shared<Part>("part-001", "Part I");
        auto chapter = std::make_shared<BookElement>("chapter", "ch-001", "Chapter 1");
        chapter->setWordCount(2500);
        part->addChapter(chapter);
        book.addPart(part);

        json j = doc.toJson();

        REQUIRE(j.contains("version"));
        REQUIRE(j["version"] == "1.0.0");

        REQUIRE(j.contains("document"));
        REQUIRE(j["document"]["id"] == doc.getId());
        REQUIRE(j["document"]["title"] == "The Great Adventure");
        REQUIRE(j["document"]["author"] == "Jane Doe");
        REQUIRE(j["document"]["language"] == "en");
        REQUIRE(j["document"]["genre"] == "fiction");
        REQUIRE(j["document"].contains("created"));
        REQUIRE(j["document"].contains("modified"));

        REQUIRE(j.contains("book"));
        REQUIRE(j["book"]["body"].is_array());
        REQUIRE(j["book"]["body"].size() == 1);
    }

    SECTION("toJson with empty book") {
        Document doc("Empty Project", "John Doe", "en");

        json j = doc.toJson();

        REQUIRE(j["document"]["title"] == "Empty Project");
        REQUIRE(j["book"]["frontMatter"].is_array());
        REQUIRE(j["book"]["frontMatter"].empty());
        REQUIRE(j["book"]["body"].empty());
        REQUIRE(j["book"]["backMatter"].empty());
    }

    SECTION("fromJson with complete structure") {
        json j = {
            {"version", "1.0.0"},
            {"document", {
                {"id", "test-uuid-12345"},
                {"title", "The Beginning"},
                {"author", "John Smith"},
                {"language", "pl"},
                {"genre", "non-fiction"},
                {"created", "2025-10-30T10:00:00Z"},
                {"modified", "2025-10-30T15:30:00Z"}
            }},
            {"book", {
                {"frontMatter", json::array()},
                {"body", json::array({
                    {
                        {"id", "part-001"},
                        {"title", "Part I"},
                        {"chapters", json::array({
                            {
                                {"type", "chapter"},
                                {"id", "ch-001"},
                                {"title", "Chapter 1"},
                                {"file", "ch1.rtf"},
                                {"wordCount", 2500}
                            }
                        })}
                    }
                })},
                {"backMatter", json::array()}
            }}
        };

        Document doc = Document::fromJson(j);

        REQUIRE(doc.getId() == "test-uuid-12345");
        REQUIRE(doc.getTitle() == "The Beginning");
        REQUIRE(doc.getAuthor() == "John Smith");
        REQUIRE(doc.getLanguage() == "pl");
        REQUIRE(doc.getGenre() == "non-fiction");
        REQUIRE(doc.getBook().getPartCount() == 1);
        REQUIRE(doc.getBook().getChapterCount() == 1);
    }

    SECTION("fromJson with minimal structure") {
        json j = {
            {"version", "1.0.0"},
            {"document", {
                {"id", "minimal-uuid"},
                {"title", "Minimal Doc"},
                {"author", "Author"},
                {"language", "en"},
                {"created", "2025-10-30T10:00:00Z"},
                {"modified", "2025-10-30T10:00:00Z"}
            }},
            {"book", {
                {"frontMatter", json::array()},
                {"body", json::array()},
                {"backMatter", json::array()}
            }}
        };

        Document doc = Document::fromJson(j);

        REQUIRE(doc.getTitle() == "Minimal Doc");
        REQUIRE(doc.getGenre().empty());  // Optional field
        REQUIRE(doc.getBook().isEmpty());
    }

    SECTION("Round-trip serialization preserves data") {
        Document original("Round Trip Test", "Test Author", "en");
        original.setGenre("fiction");

        // Add book structure
        Book& book = original.getBook();
        auto part = std::make_shared<Part>("part-001", "Part I");
        auto chapter = std::make_shared<BookElement>("chapter", "ch-001", "Chapter 1");
        chapter->setWordCount(2500);
        part->addChapter(chapter);
        book.addPart(part);

        // Serialize and deserialize
        json j = original.toJson();
        Document deserialized = Document::fromJson(j);

        REQUIRE(deserialized.getId() == original.getId());
        REQUIRE(deserialized.getTitle() == original.getTitle());
        REQUIRE(deserialized.getAuthor() == original.getAuthor());
        REQUIRE(deserialized.getLanguage() == original.getLanguage());
        REQUIRE(deserialized.getGenre() == original.getGenre());
        REQUIRE(deserialized.getBook().getWordCount() == original.getBook().getWordCount());
        REQUIRE(deserialized.getBook().getChapterCount() == original.getBook().getChapterCount());
    }
}

// =============================================================================
// Save/Load Tests (Phase 0 Stubs)
// =============================================================================

TEST_CASE("Document save/load operations", "[core][document][io]") {
    SECTION("save() stub implementation") {
        Document doc("Test Save", "Author", "en");

        // Phase 0: Stub implementation (may return false or true)
        // We just verify it doesn't crash
        REQUIRE_NOTHROW(doc.save("test.klh"));
    }

    SECTION("load() stub implementation") {
        // Phase 0: Stub implementation (may return nullopt)
        // We just verify it doesn't crash
        REQUIRE_NOTHROW(Document::load("nonexistent.klh"));
    }
}

// =============================================================================
// Edge Cases Tests
// =============================================================================

TEST_CASE("Document edge cases", "[core][document][edge]") {
    SECTION("Empty title string") {
        Document doc("", "Author", "en");

        REQUIRE(doc.getTitle().empty());
    }

    SECTION("Empty author string") {
        Document doc("Title", "", "en");

        REQUIRE(doc.getAuthor().empty());
    }

    SECTION("Empty language string") {
        Document doc("Title", "Author", "");

        REQUIRE(doc.getLanguage().empty());
    }

    SECTION("Special characters in title") {
        Document doc("Title with \"quotes\" and \nnewlines", "Author", "en");

        REQUIRE(doc.getTitle() == "Title with \"quotes\" and \nnewlines");
    }

    SECTION("Very long title") {
        std::string longTitle(10000, 'x');
        Document doc(longTitle, "Author", "en");

        REQUIRE(doc.getTitle().size() == 10000);
    }

    SECTION("Unicode in author name") {
        Document doc("Title", "Søren Kierkegaard", "en");

        REQUIRE(doc.getAuthor() == "Søren Kierkegaard");
    }

    SECTION("Non-standard language code") {
        Document doc("Title", "Author", "xyz");

        REQUIRE(doc.getLanguage() == "xyz");  // No validation in Phase 0
    }

    SECTION("Genre with special characters") {
        Document doc("Title", "Author", "en");
        doc.setGenre("Sci-Fi / Fantasy");

        REQUIRE(doc.getGenre() == "Sci-Fi / Fantasy");
    }

    SECTION("Multiple touch() calls") {
        Document doc("Test", "Author", "en");
        auto before = doc.getModified();

        for (int i = 0; i < 5; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            doc.touch();
        }

        auto after = doc.getModified();
        REQUIRE(after > before);
    }
}
