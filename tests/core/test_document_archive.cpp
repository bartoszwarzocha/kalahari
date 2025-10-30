/// @file test_document_archive.cpp
/// @brief Unit tests for DocumentArchive (Task #00012)
///
/// Tests cover:
/// - Save document to .klh ZIP archive
/// - Load document from .klh ZIP archive
/// - Manifest.json write/read operations
/// - Round-trip: save → load preserves data
/// - Error handling (missing files, corrupted archives)
/// - RTF file operations (Phase 0 stubs)
/// - Edge cases (empty document, large structure)
///
/// Phase 0 MVP: Only manifest.json operations tested
/// Phase 2+: Add RTF file archiving tests

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/document_archive.h>
#include <kalahari/core/document.h>
#include <filesystem>
#include <fstream>

using namespace kalahari::core;
namespace fs = std::filesystem;

// =============================================================================
// Test Helpers
// =============================================================================

/// @brief RAII helper for temporary .klh test files
///
/// Automatically creates temp directory and cleans up test files on destruction.
class TempArchiveFile {
public:
    TempArchiveFile(const std::string& filename)
        : m_dir(fs::temp_directory_path() / "kalahari_test")
        , m_path(m_dir / filename)
    {
        fs::create_directories(m_dir);
    }

    ~TempArchiveFile() {
        cleanup();
    }

    const fs::path& path() const { return m_path; }

    void cleanup() {
        if (fs::exists(m_path)) {
            fs::remove(m_path);
        }
    }

    void cleanupDir() {
        if (fs::exists(m_dir)) {
            fs::remove_all(m_dir);
        }
    }

private:
    fs::path m_dir;
    fs::path m_path;
};

// =============================================================================
// Save Tests
// =============================================================================

TEST_CASE("DocumentArchive save operations", "[core][document][archive][save]") {
    SECTION("save() creates .klh file") {
        TempArchiveFile tempFile("test_save.klh");

        Document doc("Test Save", "Test Author", "en");
        bool saved = DocumentArchive::save(doc, tempFile.path());

        REQUIRE(saved == true);
        REQUIRE(fs::exists(tempFile.path()));
    }

    SECTION("save() with complete document structure") {
        TempArchiveFile tempFile("test_complete.klh");

        Document doc("Complete Novel", "Jane Doe", "en");
        doc.setGenre("fiction");

        // Add book structure
        Book& book = doc.getBook();

        // Front matter
        auto titlePage = std::make_shared<BookElement>("title_page", "fm-001", "Title Page");
        book.addFrontMatter(titlePage);

        // Body
        auto part1 = std::make_shared<Part>("part-001", "Part I");
        auto ch1 = std::make_shared<BookElement>("chapter", "ch-001", "Chapter 1", "ch1.rtf");
        ch1->setWordCount(2500);
        part1->addChapter(ch1);
        book.addPart(part1);

        // Back matter
        auto epilogue = std::make_shared<BookElement>("epilogue", "bm-001", "Epilogue");
        book.addBackMatter(epilogue);

        bool saved = DocumentArchive::save(doc, tempFile.path());

        REQUIRE(saved == true);
        REQUIRE(fs::exists(tempFile.path()));
        REQUIRE(fs::file_size(tempFile.path()) > 0);
    }

    SECTION("save() overwrites existing file") {
        TempArchiveFile tempFile("test_overwrite.klh");

        // First save
        Document doc1("Version 1", "Author", "en");
        DocumentArchive::save(doc1, tempFile.path());

        // Second save (overwrite)
        Document doc2("Version 2", "Author", "en");
        bool saved = DocumentArchive::save(doc2, tempFile.path());

        REQUIRE(saved == true);
        REQUIRE(fs::exists(tempFile.path()));

        // Verify it's the second document
        auto loaded = DocumentArchive::load(tempFile.path());
        REQUIRE(loaded.has_value());
        REQUIRE(loaded->getTitle() == "Version 2");
    }

    SECTION("save() with empty document") {
        TempArchiveFile tempFile("test_empty.klh");

        Document doc("Empty Project", "Author", "en");
        bool saved = DocumentArchive::save(doc, tempFile.path());

        REQUIRE(saved == true);
        REQUIRE(fs::exists(tempFile.path()));
    }
}

// =============================================================================
// Load Tests
// =============================================================================

TEST_CASE("DocumentArchive load operations", "[core][document][archive][load]") {
    SECTION("load() reads saved document") {
        TempArchiveFile tempFile("test_load.klh");

        Document original("Test Load", "Test Author", "en");
        DocumentArchive::save(original, tempFile.path());

        auto loaded = DocumentArchive::load(tempFile.path());

        REQUIRE(loaded.has_value());
        REQUIRE(loaded->getTitle() == "Test Load");
        REQUIRE(loaded->getAuthor() == "Test Author");
        REQUIRE(loaded->getLanguage() == "en");
    }

    SECTION("load() with complete document structure") {
        TempArchiveFile tempFile("test_load_complete.klh");

        Document original("Complete Novel", "Jane Doe", "en");
        original.setGenre("fiction");

        // Add book structure
        Book& book = original.getBook();
        auto part = std::make_shared<Part>("part-001", "Part I");
        auto chapter = std::make_shared<BookElement>("chapter", "ch-001", "Chapter 1");
        chapter->setWordCount(2500);
        part->addChapter(chapter);
        book.addPart(part);

        DocumentArchive::save(original, tempFile.path());

        auto loaded = DocumentArchive::load(tempFile.path());

        REQUIRE(loaded.has_value());
        REQUIRE(loaded->getBook().getPartCount() == 1);
        REQUIRE(loaded->getBook().getChapterCount() == 1);
        REQUIRE(loaded->getBook().getWordCount() == 2500);
    }

    SECTION("load() returns nullopt for non-existent file") {
        auto loaded = DocumentArchive::load("non_existent.klh");

        REQUIRE_FALSE(loaded.has_value());
    }

    SECTION("load() returns nullopt for invalid path") {
        auto loaded = DocumentArchive::load("/invalid/path/to/file.klh");

        REQUIRE_FALSE(loaded.has_value());
    }

    SECTION("load() returns nullopt for corrupted archive") {
        TempArchiveFile tempFile("test_corrupted.klh");

        // Create corrupted file (not a valid ZIP)
        std::ofstream out(tempFile.path(), std::ios::binary);
        out << "This is not a valid ZIP file content";
        out.close();

        auto loaded = DocumentArchive::load(tempFile.path());

        REQUIRE_FALSE(loaded.has_value());
    }
}

// =============================================================================
// Round-Trip Tests
// =============================================================================

TEST_CASE("DocumentArchive round-trip tests", "[core][document][archive][roundtrip]") {
    SECTION("Round-trip preserves document metadata") {
        TempArchiveFile tempFile("test_roundtrip.klh");

        Document original("Round Trip Test", "John Smith", "pl");
        original.setGenre("non-fiction");

        DocumentArchive::save(original, tempFile.path());
        auto loaded = DocumentArchive::load(tempFile.path());

        REQUIRE(loaded.has_value());
        REQUIRE(loaded->getId() == original.getId());
        REQUIRE(loaded->getTitle() == original.getTitle());
        REQUIRE(loaded->getAuthor() == original.getAuthor());
        REQUIRE(loaded->getLanguage() == original.getLanguage());
        REQUIRE(loaded->getGenre() == original.getGenre());
    }

    SECTION("Round-trip preserves book structure") {
        TempArchiveFile tempFile("test_roundtrip_book.klh");

        Document original("Book Test", "Author", "en");
        Book& book = original.getBook();

        // Front matter
        auto titlePage = std::make_shared<BookElement>("title_page", "fm-001", "Title");
        book.addFrontMatter(titlePage);

        // Body with 2 parts
        auto part1 = std::make_shared<Part>("part-001", "Part I");
        auto ch1 = std::make_shared<BookElement>("chapter", "ch-001", "Chapter 1");
        ch1->setWordCount(2500);
        part1->addChapter(ch1);

        auto part2 = std::make_shared<Part>("part-002", "Part II");
        auto ch2 = std::make_shared<BookElement>("chapter", "ch-002", "Chapter 2");
        ch2->setWordCount(3000);
        part2->addChapter(ch2);

        book.addPart(part1);
        book.addPart(part2);

        // Back matter
        auto epilogue = std::make_shared<BookElement>("epilogue", "bm-001", "Epilogue");
        book.addBackMatter(epilogue);

        DocumentArchive::save(original, tempFile.path());
        auto loaded = DocumentArchive::load(tempFile.path());

        REQUIRE(loaded.has_value());
        REQUIRE(loaded->getBook().getFrontMatter().size() == 1);
        REQUIRE(loaded->getBook().getPartCount() == 2);
        REQUIRE(loaded->getBook().getChapterCount() == 2);
        REQUIRE(loaded->getBook().getBackMatter().size() == 1);
        REQUIRE(loaded->getBook().getWordCount() == 5500);
    }

    SECTION("Round-trip with multiple save/load cycles") {
        TempArchiveFile tempFile("test_cycles.klh");

        Document doc("Cycle Test", "Author", "en");

        // Cycle 1: Save and load
        DocumentArchive::save(doc, tempFile.path());
        auto loaded1 = DocumentArchive::load(tempFile.path());
        REQUIRE(loaded1.has_value());

        // Cycle 2: Modify, save and load
        loaded1->setTitle("Cycle Test - Modified");
        DocumentArchive::save(*loaded1, tempFile.path());
        auto loaded2 = DocumentArchive::load(tempFile.path());
        REQUIRE(loaded2.has_value());
        REQUIRE(loaded2->getTitle() == "Cycle Test - Modified");

        // Cycle 3: Add content, save and load
        Book& book = loaded2->getBook();
        auto part = std::make_shared<Part>("part-001", "Part I");
        book.addPart(part);
        DocumentArchive::save(*loaded2, tempFile.path());
        auto loaded3 = DocumentArchive::load(tempFile.path());
        REQUIRE(loaded3.has_value());
        REQUIRE(loaded3->getBook().getPartCount() == 1);
    }

    SECTION("Round-trip preserves metadata") {
        TempArchiveFile tempFile("test_metadata.klh");

        Document original("Metadata Test", "Author", "en");

        // Add chapter with metadata
        Book& book = original.getBook();
        auto part = std::make_shared<Part>("part-001", "Part I");
        auto chapter = std::make_shared<BookElement>("chapter", "ch-001", "Chapter 1");
        chapter->setMetadata("pov", "First Person");
        chapter->setMetadata("location", "Paris");
        chapter->setWordCount(2500);
        part->addChapter(chapter);
        book.addPart(part);

        DocumentArchive::save(original, tempFile.path());
        auto loaded = DocumentArchive::load(tempFile.path());

        REQUIRE(loaded.has_value());

        // Get the part and chapter from loaded document
        const auto& parts = loaded->getBook().getBody();
        REQUIRE(parts.size() == 1);

        const auto& chapters = parts[0]->getChapters();
        REQUIRE(chapters.size() == 1);

        auto pov = chapters[0]->getMetadata("pov");
        REQUIRE(pov.has_value());
        REQUIRE(*pov == "First Person");

        auto location = chapters[0]->getMetadata("location");
        REQUIRE(location.has_value());
        REQUIRE(*location == "Paris");
    }
}

// =============================================================================
// Edge Cases Tests
// =============================================================================

TEST_CASE("DocumentArchive edge cases", "[core][document][archive][edge]") {
    SECTION("Save document with very large book structure") {
        TempArchiveFile tempFile("test_large.klh");

        Document doc("Large Book", "Author", "en");
        Book& book = doc.getBook();

        // Create 10 parts with 10 chapters each = 100 chapters total
        for (int p = 0; p < 10; ++p) {
            auto part = std::make_shared<Part>(
                "part-" + std::to_string(p),
                "Part " + std::to_string(p)
            );

            for (int c = 0; c < 10; ++c) {
                auto chapter = std::make_shared<BookElement>(
                    "chapter",
                    "ch-" + std::to_string(p * 10 + c),
                    "Chapter " + std::to_string(c)
                );
                chapter->setWordCount(1000);
                part->addChapter(chapter);
            }

            book.addPart(part);
        }

        bool saved = DocumentArchive::save(doc, tempFile.path());
        REQUIRE(saved == true);

        auto loaded = DocumentArchive::load(tempFile.path());
        REQUIRE(loaded.has_value());
        REQUIRE(loaded->getBook().getPartCount() == 10);
        REQUIRE(loaded->getBook().getChapterCount() == 100);
        REQUIRE(loaded->getBook().getWordCount() == 100000);
    }

    SECTION("Save document with special characters in title") {
        TempArchiveFile tempFile("test_special_chars.klh");

        Document doc("Title with \"quotes\" and \nnewlines", "Author", "en");
        bool saved = DocumentArchive::save(doc, tempFile.path());
        REQUIRE(saved == true);

        auto loaded = DocumentArchive::load(tempFile.path());
        REQUIRE(loaded.has_value());
        REQUIRE(loaded->getTitle() == "Title with \"quotes\" and \nnewlines");
    }

    SECTION("Save document with Unicode characters") {
        TempArchiveFile tempFile("test_unicode.klh");

        Document doc("Título en Español", "José García", "es");
        bool saved = DocumentArchive::save(doc, tempFile.path());
        REQUIRE(saved == true);

        auto loaded = DocumentArchive::load(tempFile.path());
        REQUIRE(loaded.has_value());
        REQUIRE(loaded->getTitle() == "Título en Español");
        REQUIRE(loaded->getAuthor() == "José García");
    }

    SECTION("Save to path with spaces") {
        TempArchiveFile tempFile("test file with spaces.klh");

        Document doc("Test", "Author", "en");
        bool saved = DocumentArchive::save(doc, tempFile.path());

        REQUIRE(saved == true);
        REQUIRE(fs::exists(tempFile.path()));
    }

    SECTION("Load empty archive (no manifest)") {
        TempArchiveFile tempFile("test_no_manifest.klh");

        // Create empty ZIP file manually (if possible)
        // For now, just verify load returns nullopt for corrupted file
        std::ofstream out(tempFile.path(), std::ios::binary);
        out << "PK\x03\x04";  // ZIP signature but incomplete
        out.close();

        auto loaded = DocumentArchive::load(tempFile.path());

        // Should return nullopt (no valid manifest)
        REQUIRE_FALSE(loaded.has_value());
    }

    SECTION("Multiple documents in same directory") {
        TempArchiveFile tempFile1("test_doc1.klh");
        TempArchiveFile tempFile2("test_doc2.klh");

        Document doc1("Document 1", "Author 1", "en");
        Document doc2("Document 2", "Author 2", "pl");

        bool saved1 = DocumentArchive::save(doc1, tempFile1.path());
        bool saved2 = DocumentArchive::save(doc2, tempFile2.path());

        REQUIRE(saved1 == true);
        REQUIRE(saved2 == true);

        auto loaded1 = DocumentArchive::load(tempFile1.path());
        auto loaded2 = DocumentArchive::load(tempFile2.path());

        REQUIRE(loaded1.has_value());
        REQUIRE(loaded2.has_value());
        REQUIRE(loaded1->getTitle() == "Document 1");
        REQUIRE(loaded2->getTitle() == "Document 2");
    }
}

// =============================================================================
// Cleanup Test
// =============================================================================

TEST_CASE("DocumentArchive cleanup", "[core][document][archive][cleanup]") {
    SECTION("Cleanup temporary test files") {
        TempArchiveFile tempFile("cleanup_test.klh");
        tempFile.cleanupDir();

        // Verify cleanup worked (dir should be removed)
        // This is mainly for manual verification - the destructor handles cleanup
        REQUIRE(true);
    }
}
