/// @file test_part.cpp
/// @brief Unit tests for Part (Task #00012)
///
/// Tests cover:
/// - Construction with parameters
/// - Add/remove chapter operations
/// - Get chapter by ID
/// - Move/reorder chapters
/// - Word count aggregation (sum of all chapters)
/// - Chapter count queries
/// - JSON serialization round-trip
/// - Edge cases (empty part, null chapters, etc.)

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/part.h>
#include <kalahari/core/book_element.h>

using namespace kalahari::core;

// =============================================================================
// Construction Tests
// =============================================================================

TEST_CASE("Part construction", "[core][book][part]") {
    SECTION("Constructor with all parameters") {
        Part part("part-001", "Part I: The Beginning");

        REQUIRE(part.getId() == "part-001");
        REQUIRE(part.getTitle() == "Part I: The Beginning");
        REQUIRE(part.getChapters().empty());
        REQUIRE(part.getChapterCount() == 0);
        REQUIRE(part.getWordCount() == 0);
    }

    SECTION("Constructor with empty title") {
        Part part("part-001", "");

        REQUIRE(part.getTitle().empty());
    }
}

// =============================================================================
// Add Chapter Tests
// =============================================================================

TEST_CASE("Part add chapter operations", "[core][book][part][add]") {
    Part part("part-001", "Part I");

    SECTION("Add single chapter") {
        auto chapter = std::make_shared<BookElement>("chapter", "ch-001", "Chapter 1", "ch1.rtf");
        chapter->setWordCount(2500);

        part.addChapter(chapter);

        REQUIRE(part.getChapterCount() == 1);
        REQUIRE(part.getWordCount() == 2500);
    }

    SECTION("Add multiple chapters") {
        auto ch1 = std::make_shared<BookElement>("chapter", "ch-001", "Chapter 1");
        ch1->setWordCount(2500);

        auto ch2 = std::make_shared<BookElement>("chapter", "ch-002", "Chapter 2");
        ch2->setWordCount(3000);

        auto ch3 = std::make_shared<BookElement>("chapter", "ch-003", "Chapter 3");
        ch3->setWordCount(2200);

        part.addChapter(ch1);
        part.addChapter(ch2);
        part.addChapter(ch3);

        REQUIRE(part.getChapterCount() == 3);
        REQUIRE(part.getWordCount() == 7700);  // 2500 + 3000 + 2200
    }

    SECTION("Add null chapter is ignored") {
        part.addChapter(nullptr);

        REQUIRE(part.getChapterCount() == 0);
    }

    SECTION("Add chapters with zero word count") {
        auto ch1 = std::make_shared<BookElement>("chapter", "ch-001", "Chapter 1");
        auto ch2 = std::make_shared<BookElement>("chapter", "ch-002", "Chapter 2");

        part.addChapter(ch1);
        part.addChapter(ch2);

        REQUIRE(part.getChapterCount() == 2);
        REQUIRE(part.getWordCount() == 0);
    }
}

// =============================================================================
// Remove Chapter Tests
// =============================================================================

TEST_CASE("Part remove chapter operations", "[core][book][part][remove]") {
    Part part("part-001", "Part I");

    auto ch1 = std::make_shared<BookElement>("chapter", "ch-001", "Chapter 1");
    ch1->setWordCount(2500);

    auto ch2 = std::make_shared<BookElement>("chapter", "ch-002", "Chapter 2");
    ch2->setWordCount(3000);

    auto ch3 = std::make_shared<BookElement>("chapter", "ch-003", "Chapter 3");
    ch3->setWordCount(2200);

    part.addChapter(ch1);
    part.addChapter(ch2);
    part.addChapter(ch3);

    SECTION("Remove existing chapter by ID") {
        bool removed = part.removeChapter("ch-002");

        REQUIRE(removed == true);
        REQUIRE(part.getChapterCount() == 2);
        REQUIRE(part.getWordCount() == 4700);  // 2500 + 2200

        // Verify ch-002 is gone
        auto found = part.getChapter("ch-002");
        REQUIRE(found == nullptr);
    }

    SECTION("Remove first chapter") {
        part.removeChapter("ch-001");

        REQUIRE(part.getChapterCount() == 2);
        REQUIRE(part.getWordCount() == 5200);  // 3000 + 2200
    }

    SECTION("Remove last chapter") {
        part.removeChapter("ch-003");

        REQUIRE(part.getChapterCount() == 2);
        REQUIRE(part.getWordCount() == 5500);  // 2500 + 3000
    }

    SECTION("Remove non-existent chapter returns false") {
        bool removed = part.removeChapter("non-existent");

        REQUIRE(removed == false);
        REQUIRE(part.getChapterCount() == 3);  // Unchanged
    }

    SECTION("Remove all chapters") {
        part.removeChapter("ch-001");
        part.removeChapter("ch-002");
        part.removeChapter("ch-003");

        REQUIRE(part.getChapterCount() == 0);
        REQUIRE(part.getWordCount() == 0);
        REQUIRE(part.getChapters().empty());
    }
}

// =============================================================================
// Get Chapter Tests
// =============================================================================

TEST_CASE("Part get chapter operations", "[core][book][part][get]") {
    Part part("part-001", "Part I");

    auto ch1 = std::make_shared<BookElement>("chapter", "ch-001", "Chapter 1");
    auto ch2 = std::make_shared<BookElement>("chapter", "ch-002", "Chapter 2");

    part.addChapter(ch1);
    part.addChapter(ch2);

    SECTION("Get existing chapter by ID") {
        auto found = part.getChapter("ch-001");

        REQUIRE(found != nullptr);
        REQUIRE(found->getId() == "ch-001");
        REQUIRE(found->getTitle() == "Chapter 1");
    }

    SECTION("Get another existing chapter") {
        auto found = part.getChapter("ch-002");

        REQUIRE(found != nullptr);
        REQUIRE(found->getId() == "ch-002");
    }

    SECTION("Get non-existent chapter returns nullptr") {
        auto found = part.getChapter("non-existent");

        REQUIRE(found == nullptr);
    }

    SECTION("Get chapter from empty part returns nullptr") {
        Part emptyPart("empty", "Empty Part");

        auto found = emptyPart.getChapter("any-id");

        REQUIRE(found == nullptr);
    }
}

// =============================================================================
// Move Chapter Tests
// =============================================================================

TEST_CASE("Part move chapter operations", "[core][book][part][move]") {
    Part part("part-001", "Part I");

    auto ch1 = std::make_shared<BookElement>("chapter", "ch-001", "Chapter 1");
    auto ch2 = std::make_shared<BookElement>("chapter", "ch-002", "Chapter 2");
    auto ch3 = std::make_shared<BookElement>("chapter", "ch-003", "Chapter 3");

    part.addChapter(ch1);
    part.addChapter(ch2);
    part.addChapter(ch3);

    SECTION("Move chapter forward (0 → 2)") {
        bool moved = part.moveChapter(0, 2);

        REQUIRE(moved == true);

        const auto& chapters = part.getChapters();
        REQUIRE(chapters[0]->getId() == "ch-002");
        REQUIRE(chapters[1]->getId() == "ch-003");
        REQUIRE(chapters[2]->getId() == "ch-001");
    }

    SECTION("Move chapter backward (2 → 0)") {
        bool moved = part.moveChapter(2, 0);

        REQUIRE(moved == true);

        const auto& chapters = part.getChapters();
        REQUIRE(chapters[0]->getId() == "ch-003");
        REQUIRE(chapters[1]->getId() == "ch-001");
        REQUIRE(chapters[2]->getId() == "ch-002");
    }

    SECTION("Move chapter to same position") {
        bool moved = part.moveChapter(1, 1);

        REQUIRE(moved == true);

        const auto& chapters = part.getChapters();
        REQUIRE(chapters[0]->getId() == "ch-001");
        REQUIRE(chapters[1]->getId() == "ch-002");
        REQUIRE(chapters[2]->getId() == "ch-003");
    }

    SECTION("Move with out-of-bounds from index returns false") {
        bool moved = part.moveChapter(10, 0);

        REQUIRE(moved == false);
    }

    SECTION("Move with out-of-bounds to index returns false") {
        bool moved = part.moveChapter(0, 10);

        REQUIRE(moved == false);
    }
}

// =============================================================================
// Getters/Setters Tests
// =============================================================================

TEST_CASE("Part getters and setters", "[core][book][part]") {
    Part part("part-001", "Original Title");

    SECTION("Set and get title") {
        part.setTitle("New Title");

        REQUIRE(part.getTitle() == "New Title");
    }

    SECTION("Get chapters (const and non-const)") {
        auto ch1 = std::make_shared<BookElement>("chapter", "ch-001", "Chapter 1");
        part.addChapter(ch1);

        const auto& constChapters = part.getChapters();
        REQUIRE(constChapters.size() == 1);

        auto& nonConstChapters = part.getChapters();
        REQUIRE(nonConstChapters.size() == 1);
    }
}

// =============================================================================
// JSON Serialization Tests
// =============================================================================

TEST_CASE("Part JSON serialization", "[core][book][part][json]") {
    SECTION("toJson with chapters") {
        Part part("part-001", "Part I: The Beginning");

        auto ch1 = std::make_shared<BookElement>("chapter", "ch-001", "Chapter 1", "ch1.rtf");
        ch1->setWordCount(2500);

        auto ch2 = std::make_shared<BookElement>("chapter", "ch-002", "Chapter 2", "ch2.rtf");
        ch2->setWordCount(3000);

        part.addChapter(ch1);
        part.addChapter(ch2);

        json j = part.toJson();

        REQUIRE(j["id"] == "part-001");
        REQUIRE(j["title"] == "Part I: The Beginning");
        REQUIRE(j["chapters"].is_array());
        REQUIRE(j["chapters"].size() == 2);
        REQUIRE(j["chapters"][0]["id"] == "ch-001");
        REQUIRE(j["chapters"][1]["id"] == "ch-002");
    }

    SECTION("toJson with empty chapters") {
        Part part("part-001", "Empty Part");

        json j = part.toJson();

        REQUIRE(j["id"] == "part-001");
        REQUIRE(j["chapters"].is_array());
        REQUIRE(j["chapters"].empty());
    }

    SECTION("fromJson with chapters") {
        json j = {
            {"id", "part-001"},
            {"title", "Part I"},
            {"chapters", json::array({
                {
                    {"type", "chapter"},
                    {"id", "ch-001"},
                    {"title", "Chapter 1"},
                    {"file", "ch1.rtf"},
                    {"wordCount", 2500}
                },
                {
                    {"type", "chapter"},
                    {"id", "ch-002"},
                    {"title", "Chapter 2"},
                    {"file", "ch2.rtf"},
                    {"wordCount", 3000}
                }
            })}
        };

        Part part = Part::fromJson(j);

        REQUIRE(part.getId() == "part-001");
        REQUIRE(part.getTitle() == "Part I");
        REQUIRE(part.getChapterCount() == 2);
        REQUIRE(part.getWordCount() == 5500);

        auto ch1 = part.getChapter("ch-001");
        REQUIRE(ch1 != nullptr);
        REQUIRE(ch1->getTitle() == "Chapter 1");

        auto ch2 = part.getChapter("ch-002");
        REQUIRE(ch2 != nullptr);
        REQUIRE(ch2->getTitle() == "Chapter 2");
    }

    SECTION("fromJson without chapters") {
        json j = {
            {"id", "part-002"},
            {"title", "Part II"}
        };

        Part part = Part::fromJson(j);

        REQUIRE(part.getId() == "part-002");
        REQUIRE(part.getChapterCount() == 0);
    }

    SECTION("Round-trip serialization preserves data") {
        Part original("part-001", "Part I: The Beginning");

        auto ch1 = std::make_shared<BookElement>("chapter", "ch-001", "Chapter 1");
        ch1->setWordCount(2500);
        original.addChapter(ch1);

        json j = original.toJson();
        Part deserialized = Part::fromJson(j);

        REQUIRE(deserialized.getId() == original.getId());
        REQUIRE(deserialized.getTitle() == original.getTitle());
        REQUIRE(deserialized.getChapterCount() == original.getChapterCount());
        REQUIRE(deserialized.getWordCount() == original.getWordCount());
    }
}

// =============================================================================
// Edge Cases Tests
// =============================================================================

TEST_CASE("Part edge cases", "[core][book][part][edge]") {
    SECTION("Empty part has zero word count") {
        Part part("part-001", "Empty Part");

        REQUIRE(part.getWordCount() == 0);
        REQUIRE(part.getChapterCount() == 0);
    }

    SECTION("Part with negative word count chapters") {
        Part part("part-001", "Part I");

        auto ch1 = std::make_shared<BookElement>("chapter", "ch-001", "Chapter 1");
        ch1->setWordCount(-100);  // Negative (e.g., deletion delta)

        auto ch2 = std::make_shared<BookElement>("chapter", "ch-002", "Chapter 2");
        ch2->setWordCount(500);

        part.addChapter(ch1);
        part.addChapter(ch2);

        REQUIRE(part.getWordCount() == 400);  // -100 + 500
    }

    SECTION("Part with many chapters") {
        Part part("part-001", "Large Part");

        for (int i = 0; i < 100; ++i) {
            auto chapter = std::make_shared<BookElement>(
                "chapter",
                "ch-" + std::to_string(i),
                "Chapter " + std::to_string(i)
            );
            chapter->setWordCount(1000);
            part.addChapter(chapter);
        }

        REQUIRE(part.getChapterCount() == 100);
        REQUIRE(part.getWordCount() == 100000);
    }

    SECTION("Remove same chapter twice") {
        Part part("part-001", "Part I");

        auto ch1 = std::make_shared<BookElement>("chapter", "ch-001", "Chapter 1");
        part.addChapter(ch1);

        bool removed1 = part.removeChapter("ch-001");
        bool removed2 = part.removeChapter("ch-001");

        REQUIRE(removed1 == true);
        REQUIRE(removed2 == false);
        REQUIRE(part.getChapterCount() == 0);
    }
}
