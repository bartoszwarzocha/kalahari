/// @file test_book.cpp
/// @brief Unit tests for Book (Task #00012)
///
/// Tests cover:
/// - 3-section structure (frontMatter, body, backMatter)
/// - Add/remove operations for each section
/// - Word count calculation (body only - industry standard)
/// - Chapter and part count queries
/// - JSON serialization with all 3 sections
/// - Empty book handling
/// - Complete book clear operation

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book.h>
#include <kalahari/core/part.h>
#include <kalahari/core/book_element.h>

using namespace kalahari::core;

// =============================================================================
// Construction and Empty Book Tests
// =============================================================================

TEST_CASE("Book construction and empty state", "[core][book]") {
    SECTION("Default constructor creates empty book") {
        Book book;

        REQUIRE(book.getFrontMatter().empty());
        REQUIRE(book.getBody().empty());
        REQUIRE(book.getBackMatter().empty());
        REQUIRE(book.isEmpty());
        REQUIRE(book.getWordCount() == 0);
        REQUIRE(book.getChapterCount() == 0);
        REQUIRE(book.getPartCount() == 0);
    }
}

// =============================================================================
// Front Matter Tests
// =============================================================================

TEST_CASE("Book front matter operations", "[core][book][frontmatter]") {
    Book book;

    SECTION("Add front matter elements") {
        auto titlePage = std::make_shared<BookElement>("title_page", "fm-001", "Title Page");
        auto copyright = std::make_shared<BookElement>("copyright", "fm-002", "Copyright");
        auto dedication = std::make_shared<BookElement>("dedication", "fm-003", "Dedication");

        book.addFrontMatter(titlePage);
        book.addFrontMatter(copyright);
        book.addFrontMatter(dedication);

        REQUIRE(book.getFrontMatter().size() == 3);
        REQUIRE_FALSE(book.isEmpty());
    }

    SECTION("Remove front matter by ID") {
        auto element = std::make_shared<BookElement>("preface", "fm-001", "Preface");
        book.addFrontMatter(element);

        bool removed = book.removeFrontMatter("fm-001");

        REQUIRE(removed == true);
        REQUIRE(book.getFrontMatter().empty());
    }

    SECTION("Remove non-existent front matter returns false") {
        bool removed = book.removeFrontMatter("non-existent");

        REQUIRE(removed == false);
    }

    SECTION("Add null front matter is ignored") {
        book.addFrontMatter(nullptr);

        REQUIRE(book.getFrontMatter().empty());
    }
}

// =============================================================================
// Body (Parts) Tests
// =============================================================================

TEST_CASE("Book body operations", "[core][book][body]") {
    Book book;

    SECTION("Add parts to body") {
        auto part1 = std::make_shared<Part>("part-001", "Part I");
        auto part2 = std::make_shared<Part>("part-002", "Part II");

        book.addPart(part1);
        book.addPart(part2);

        REQUIRE(book.getBody().size() == 2);
        REQUIRE(book.getPartCount() == 2);
        REQUIRE_FALSE(book.isEmpty());
    }

    SECTION("Remove part by ID") {
        auto part = std::make_shared<Part>("part-001", "Part I");
        book.addPart(part);

        bool removed = book.removePart("part-001");

        REQUIRE(removed == true);
        REQUIRE(book.getBody().empty());
        REQUIRE(book.getPartCount() == 0);
    }

    SECTION("Remove non-existent part returns false") {
        bool removed = book.removePart("non-existent");

        REQUIRE(removed == false);
    }

    SECTION("Add null part is ignored") {
        book.addPart(nullptr);

        REQUIRE(book.getBody().empty());
    }
}

// =============================================================================
// Back Matter Tests
// =============================================================================

TEST_CASE("Book back matter operations", "[core][book][backmatter]") {
    Book book;

    SECTION("Add back matter elements") {
        auto epilogue = std::make_shared<BookElement>("epilogue", "bm-001", "Epilogue");
        auto glossary = std::make_shared<BookElement>("glossary", "bm-002", "Glossary");
        auto bibliography = std::make_shared<BookElement>("bibliography", "bm-003", "Bibliography");

        book.addBackMatter(epilogue);
        book.addBackMatter(glossary);
        book.addBackMatter(bibliography);

        REQUIRE(book.getBackMatter().size() == 3);
        REQUIRE_FALSE(book.isEmpty());
    }

    SECTION("Remove back matter by ID") {
        auto element = std::make_shared<BookElement>("about_author", "bm-001", "About Author");
        book.addBackMatter(element);

        bool removed = book.removeBackMatter("bm-001");

        REQUIRE(removed == true);
        REQUIRE(book.getBackMatter().empty());
    }

    SECTION("Remove non-existent back matter returns false") {
        bool removed = book.removeBackMatter("non-existent");

        REQUIRE(removed == false);
    }

    SECTION("Add null back matter is ignored") {
        book.addBackMatter(nullptr);

        REQUIRE(book.getBackMatter().empty());
    }
}

// =============================================================================
// Word Count Tests (Body Only - Industry Standard)
// =============================================================================

TEST_CASE("Book word count calculation", "[core][book][wordcount]") {
    Book book;

    SECTION("Empty book has zero word count") {
        REQUIRE(book.getWordCount() == 0);
    }

    SECTION("Word count includes only body chapters") {
        // Front matter (should NOT count)
        auto titlePage = std::make_shared<BookElement>("title_page", "fm-001", "Title");
        titlePage->setWordCount(50);
        book.addFrontMatter(titlePage);

        // Body (SHOULD count)
        auto part1 = std::make_shared<Part>("part-001", "Part I");

        auto ch1 = std::make_shared<BookElement>("chapter", "ch-001", "Chapter 1");
        ch1->setWordCount(2500);
        part1->addChapter(ch1);

        auto ch2 = std::make_shared<BookElement>("chapter", "ch-002", "Chapter 2");
        ch2->setWordCount(3000);
        part1->addChapter(ch2);

        book.addPart(part1);

        // Back matter (should NOT count)
        auto epilogue = std::make_shared<BookElement>("epilogue", "bm-001", "Epilogue");
        epilogue->setWordCount(800);
        book.addBackMatter(epilogue);

        // Total should be body only: 2500 + 3000 = 5500
        REQUIRE(book.getWordCount() == 5500);
    }

    SECTION("Word count aggregates multiple parts") {
        auto part1 = std::make_shared<Part>("part-001", "Part I");
        auto ch1 = std::make_shared<BookElement>("chapter", "ch-001", "Chapter 1");
        ch1->setWordCount(2000);
        part1->addChapter(ch1);

        auto part2 = std::make_shared<Part>("part-002", "Part II");
        auto ch2 = std::make_shared<BookElement>("chapter", "ch-002", "Chapter 2");
        ch2->setWordCount(3000);
        part2->addChapter(ch2);

        auto part3 = std::make_shared<Part>("part-003", "Part III");
        auto ch3 = std::make_shared<BookElement>("chapter", "ch-003", "Chapter 3");
        ch3->setWordCount(2500);
        part3->addChapter(ch3);

        book.addPart(part1);
        book.addPart(part2);
        book.addPart(part3);

        REQUIRE(book.getWordCount() == 7500);
    }

    SECTION("Word count handles empty parts") {
        auto part1 = std::make_shared<Part>("part-001", "Empty Part");
        book.addPart(part1);

        REQUIRE(book.getWordCount() == 0);
    }
}

// =============================================================================
// Chapter and Part Count Tests
// =============================================================================

TEST_CASE("Book chapter and part counts", "[core][book][counts]") {
    Book book;

    SECTION("Empty book has zero counts") {
        REQUIRE(book.getChapterCount() == 0);
        REQUIRE(book.getPartCount() == 0);
    }

    SECTION("Chapter count aggregates across parts") {
        auto part1 = std::make_shared<Part>("part-001", "Part I");
        part1->addChapter(std::make_shared<BookElement>("chapter", "ch-001", "Ch1"));
        part1->addChapter(std::make_shared<BookElement>("chapter", "ch-002", "Ch2"));

        auto part2 = std::make_shared<Part>("part-002", "Part II");
        part2->addChapter(std::make_shared<BookElement>("chapter", "ch-003", "Ch3"));
        part2->addChapter(std::make_shared<BookElement>("chapter", "ch-004", "Ch4"));
        part2->addChapter(std::make_shared<BookElement>("chapter", "ch-005", "Ch5"));

        book.addPart(part1);
        book.addPart(part2);

        REQUIRE(book.getChapterCount() == 5);
        REQUIRE(book.getPartCount() == 2);
    }

    SECTION("Part count matches body size") {
        book.addPart(std::make_shared<Part>("part-001", "Part I"));
        book.addPart(std::make_shared<Part>("part-002", "Part II"));
        book.addPart(std::make_shared<Part>("part-003", "Part III"));

        REQUIRE(book.getPartCount() == 3);
    }
}

// =============================================================================
// Clear All Test
// =============================================================================

TEST_CASE("Book clear all operation", "[core][book][clear]") {
    Book book;

    // Populate all sections
    book.addFrontMatter(std::make_shared<BookElement>("title_page", "fm-001", "Title"));
    book.addPart(std::make_shared<Part>("part-001", "Part I"));
    book.addBackMatter(std::make_shared<BookElement>("epilogue", "bm-001", "Epilogue"));

    REQUIRE_FALSE(book.isEmpty());

    SECTION("clearAll removes all content") {
        book.clearAll();

        REQUIRE(book.isEmpty());
        REQUIRE(book.getFrontMatter().empty());
        REQUIRE(book.getBody().empty());
        REQUIRE(book.getBackMatter().empty());
        REQUIRE(book.getWordCount() == 0);
        REQUIRE(book.getChapterCount() == 0);
        REQUIRE(book.getPartCount() == 0);
    }
}

// =============================================================================
// JSON Serialization Tests
// =============================================================================

TEST_CASE("Book JSON serialization", "[core][book][json]") {
    SECTION("toJson with all 3 sections") {
        Book book;

        // Front matter
        auto titlePage = std::make_shared<BookElement>("title_page", "fm-001", "Title Page");
        book.addFrontMatter(titlePage);

        // Body
        auto part1 = std::make_shared<Part>("part-001", "Part I");
        auto ch1 = std::make_shared<BookElement>("chapter", "ch-001", "Chapter 1");
        part1->addChapter(ch1);
        book.addPart(part1);

        // Back matter
        auto epilogue = std::make_shared<BookElement>("epilogue", "bm-001", "Epilogue");
        book.addBackMatter(epilogue);

        json j = book.toJson();

        REQUIRE(j["frontMatter"].is_array());
        REQUIRE(j["frontMatter"].size() == 1);
        REQUIRE(j["body"].is_array());
        REQUIRE(j["body"].size() == 1);
        REQUIRE(j["backMatter"].is_array());
        REQUIRE(j["backMatter"].size() == 1);
    }

    SECTION("toJson with empty sections") {
        Book book;

        json j = book.toJson();

        REQUIRE(j["frontMatter"].is_array());
        REQUIRE(j["frontMatter"].empty());
        REQUIRE(j["body"].is_array());
        REQUIRE(j["body"].empty());
        REQUIRE(j["backMatter"].is_array());
        REQUIRE(j["backMatter"].empty());
    }

    SECTION("fromJson with all sections") {
        json j = {
            {"frontMatter", json::array({
                {{"type", "title_page"}, {"id", "fm-001"}, {"title", "Title"}, {"file", ""}}
            })},
            {"body", json::array({
                {
                    {"id", "part-001"},
                    {"title", "Part I"},
                    {"chapters", json::array({
                        {{"type", "chapter"}, {"id", "ch-001"}, {"title", "Ch1"}, {"file", "ch1.rtf"}}
                    })}
                }
            })},
            {"backMatter", json::array({
                {{"type", "epilogue"}, {"id", "bm-001"}, {"title", "Epilogue"}, {"file", ""}}
            })}
        };

        Book book = Book::fromJson(j);

        REQUIRE(book.getFrontMatter().size() == 1);
        REQUIRE(book.getBody().size() == 1);
        REQUIRE(book.getBackMatter().size() == 1);
        REQUIRE(book.getPartCount() == 1);
        REQUIRE(book.getChapterCount() == 1);
    }

    SECTION("fromJson with missing sections") {
        json j = {
            {"frontMatter", json::array()},
            {"body", json::array()},
            {"backMatter", json::array()}
        };

        Book book = Book::fromJson(j);

        REQUIRE(book.isEmpty());
    }

    SECTION("Round-trip serialization preserves structure") {
        Book original;

        // Build complete structure
        original.addFrontMatter(std::make_shared<BookElement>("title_page", "fm-001", "Title"));

        auto part = std::make_shared<Part>("part-001", "Part I");
        auto chapter = std::make_shared<BookElement>("chapter", "ch-001", "Chapter 1");
        chapter->setWordCount(2500);
        part->addChapter(chapter);
        original.addPart(part);

        original.addBackMatter(std::make_shared<BookElement>("epilogue", "bm-001", "Epilogue"));

        // Serialize and deserialize
        json j = original.toJson();
        Book deserialized = Book::fromJson(j);

        REQUIRE(deserialized.getFrontMatter().size() == original.getFrontMatter().size());
        REQUIRE(deserialized.getBody().size() == original.getBody().size());
        REQUIRE(deserialized.getBackMatter().size() == original.getBackMatter().size());
        REQUIRE(deserialized.getWordCount() == original.getWordCount());
        REQUIRE(deserialized.getChapterCount() == original.getChapterCount());
    }
}

// =============================================================================
// Edge Cases Tests
// =============================================================================

TEST_CASE("Book edge cases", "[core][book][edge]") {
    SECTION("Book with only front matter") {
        Book book;
        book.addFrontMatter(std::make_shared<BookElement>("title_page", "fm-001", "Title"));

        REQUIRE_FALSE(book.isEmpty());
        REQUIRE(book.getWordCount() == 0);  // Front matter doesn't count
        REQUIRE(book.getChapterCount() == 0);
    }

    SECTION("Book with only back matter") {
        Book book;
        book.addBackMatter(std::make_shared<BookElement>("epilogue", "bm-001", "Epilogue"));

        REQUIRE_FALSE(book.isEmpty());
        REQUIRE(book.getWordCount() == 0);  // Back matter doesn't count
        REQUIRE(book.getChapterCount() == 0);
    }

    SECTION("Book with many parts") {
        Book book;

        for (int i = 0; i < 20; ++i) {
            auto part = std::make_shared<Part>("part-" + std::to_string(i), "Part " + std::to_string(i));
            book.addPart(part);
        }

        REQUIRE(book.getPartCount() == 20);
    }

    SECTION("Remove same element twice") {
        Book book;
        book.addFrontMatter(std::make_shared<BookElement>("title_page", "fm-001", "Title"));

        bool removed1 = book.removeFrontMatter("fm-001");
        bool removed2 = book.removeFrontMatter("fm-001");

        REQUIRE(removed1 == true);
        REQUIRE(removed2 == false);
    }
}
