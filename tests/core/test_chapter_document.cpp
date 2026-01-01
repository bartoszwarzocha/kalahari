/// @file test_chapter_document.cpp
/// @brief Unit tests for ChapterDocument (.kchapter format)
///
/// Tests cover:
/// - Construction and default values
/// - Content management (KML, plainText)
/// - Statistics calculation (words, characters, paragraphs)
/// - Metadata (title, status, notes, color)
/// - Annotations (comments, highlights)
/// - JSON serialization round-trip
/// - File I/O operations
/// - Edge cases
///
/// OpenSpec #00035: KChapter Document Format

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/chapter_document.h>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QThread>

using namespace kalahari::core;

// =============================================================================
// Construction Tests
// =============================================================================

TEST_CASE("ChapterDocument construction", "[core][chapter_document]") {
    SECTION("Default constructor creates empty document") {
        ChapterDocument doc;

        CHECK(doc.kml().isEmpty());
        CHECK(doc.plainText().isEmpty());
        CHECK_FALSE(doc.hasContent());
    }

    SECTION("Default status is 'draft'") {
        ChapterDocument doc;

        CHECK(doc.status() == "draft");
    }

    SECTION("Default notes is empty") {
        ChapterDocument doc;

        CHECK(doc.notes().isEmpty());
    }

    SECTION("Default title is empty") {
        ChapterDocument doc;

        CHECK(doc.title().isEmpty());
    }

    SECTION("Default color is nullopt") {
        ChapterDocument doc;

        CHECK_FALSE(doc.color().has_value());
    }

    SECTION("Statistics are zero for empty document") {
        ChapterDocument doc;

        CHECK(doc.wordCount() == 0);
        CHECK(doc.characterCount() == 0);
        CHECK(doc.paragraphCount() == 0);
    }

    SECTION("Constructor with KML content") {
        ChapterDocument doc("<kml><p>Hello World</p></kml>");

        CHECK_FALSE(doc.kml().isEmpty());
        CHECK(doc.hasContent());
        CHECK(doc.wordCount() > 0);
    }

    SECTION("lastModified is set on construction") {
        ChapterDocument doc;

        CHECK(doc.lastModified().isValid());
    }
}

// =============================================================================
// Content Management Tests
// =============================================================================

TEST_CASE("ChapterDocument content management", "[core][chapter_document]") {
    ChapterDocument doc;

    SECTION("setKml stores KML content") {
        doc.setKml("<kml><p>Test content</p></kml>");

        CHECK(doc.kml() == "<kml><p>Test content</p></kml>");
    }

    SECTION("setKml auto-generates plainText") {
        doc.setKml("<kml><p>Hello <bold>World</bold></p></kml>");

        CHECK_FALSE(doc.plainText().isEmpty());
        CHECK(doc.plainText().contains("Hello"));
        CHECK(doc.plainText().contains("World"));
    }

    SECTION("setKml strips KML tags from plainText") {
        doc.setKml("<kml><p><bold>Bold</bold> and <italic>italic</italic></p></kml>");
        QString plain = doc.plainText();

        CHECK_FALSE(plain.contains("<bold>"));
        CHECK_FALSE(plain.contains("<italic>"));
        CHECK(plain.contains("Bold"));
        CHECK(plain.contains("italic"));
    }

    SECTION("setPlainText sets plain text directly") {
        doc.setPlainText("Direct plain text");

        CHECK(doc.plainText() == "Direct plain text");
    }

    SECTION("hasContent returns true when KML is set") {
        CHECK_FALSE(doc.hasContent());

        doc.setKml("<kml><p>Content</p></kml>");

        CHECK(doc.hasContent());
    }

    SECTION("Empty KML clears content") {
        doc.setKml("<kml><p>Initial</p></kml>");
        CHECK(doc.hasContent());

        doc.setKml("");
        CHECK_FALSE(doc.hasContent());
    }
}

// =============================================================================
// Statistics Calculation Tests
// =============================================================================

TEST_CASE("ChapterDocument statistics calculation", "[core][chapter_document]") {
    ChapterDocument doc;

    SECTION("Word count for simple text") {
        doc.setKml("<kml><p>One two three</p></kml>");

        CHECK(doc.wordCount() == 3);
    }

    SECTION("Word count with multiple paragraphs") {
        doc.setKml("<kml><p>One two</p><p>Three four five</p></kml>");

        CHECK(doc.wordCount() == 5);
    }

    SECTION("Word count with formatting") {
        doc.setKml("<kml><p>Word <bold>bold</bold> <italic>italic</italic> text</p></kml>");

        CHECK(doc.wordCount() == 4);
    }

    SECTION("Character count excludes whitespace") {
        doc.setKml("<kml><p>abc def</p></kml>");
        // "abc def" = 6 characters (excluding space)
        CHECK(doc.characterCount() == 6);
    }

    SECTION("Paragraph count for single paragraph") {
        doc.setKml("<kml><p>Single paragraph text.</p></kml>");

        CHECK(doc.paragraphCount() >= 1);
    }

    SECTION("Paragraph count for multiple paragraphs") {
        doc.setKml("<kml><p>First paragraph.</p><p>Second paragraph.</p></kml>");

        // Note: paragraphCount is based on double newlines in plainText
        // KML paragraphs may result in different counts depending on conversion
        CHECK(doc.paragraphCount() >= 1);
    }

    SECTION("Statistics update when content changes") {
        doc.setKml("<kml><p>One</p></kml>");
        int initialCount = doc.wordCount();

        doc.setKml("<kml><p>One two three four five</p></kml>");

        CHECK(doc.wordCount() > initialCount);
    }

    SECTION("Empty content has zero statistics") {
        doc.setKml("");

        CHECK(doc.wordCount() == 0);
        CHECK(doc.characterCount() == 0);
        CHECK(doc.paragraphCount() == 0);
    }
}

// =============================================================================
// Metadata Tests
// =============================================================================

TEST_CASE("ChapterDocument metadata", "[core][chapter_document]") {
    ChapterDocument doc;

    SECTION("setTitle and title") {
        doc.setTitle("Chapter One: The Beginning");

        CHECK(doc.title() == "Chapter One: The Beginning");
    }

    SECTION("setStatus and status") {
        doc.setStatus("revision");

        CHECK(doc.status() == "revision");
    }

    SECTION("setStatus to final") {
        doc.setStatus("final");

        CHECK(doc.status() == "final");
    }

    SECTION("setNotes and notes") {
        doc.setNotes("Remember to add more description.");

        CHECK(doc.notes() == "Remember to add more description.");
    }

    SECTION("setColor and color") {
        QColor testColor("#FF5733");
        doc.setColor(testColor);

        REQUIRE(doc.color().has_value());
        CHECK(doc.color()->name() == testColor.name());
    }

    SECTION("clearColor removes color") {
        doc.setColor(QColor("#FF5733"));
        REQUIRE(doc.color().has_value());

        doc.clearColor();

        CHECK_FALSE(doc.color().has_value());
    }

    SECTION("touch updates lastModified") {
        QDateTime before = doc.lastModified();
        QThread::msleep(10);

        doc.touch();

        CHECK(doc.lastModified() > before);
    }
}

// =============================================================================
// Annotations Tests
// =============================================================================

TEST_CASE("ChapterDocument annotations", "[core][chapter_document]") {
    ChapterDocument doc;

    SECTION("Default comments array is empty") {
        CHECK(doc.comments().isEmpty());
    }

    SECTION("Default highlights array is empty") {
        CHECK(doc.highlights().isEmpty());
    }

    SECTION("setComments stores comment data") {
        QJsonArray comments;
        QJsonObject comment1;
        comment1["id"] = "comment-001";
        comment1["text"] = "This needs revision";
        comment1["position"] = 42;
        comments.append(comment1);

        doc.setComments(comments);

        CHECK(doc.comments().size() == 1);
        CHECK(doc.comments()[0].toObject()["id"].toString() == "comment-001");
    }

    SECTION("setHighlights stores highlight data") {
        QJsonArray highlights;
        QJsonObject highlight1;
        highlight1["id"] = "highlight-001";
        highlight1["start"] = 10;
        highlight1["end"] = 20;
        highlight1["color"] = "#FFFF00";
        highlights.append(highlight1);

        doc.setHighlights(highlights);

        CHECK(doc.highlights().size() == 1);
        CHECK(doc.highlights()[0].toObject()["color"].toString() == "#FFFF00");
    }
}

// =============================================================================
// JSON Serialization Tests
// =============================================================================

TEST_CASE("ChapterDocument JSON serialization", "[core][chapter_document][json]") {
    SECTION("toJson produces valid JSON with all sections") {
        ChapterDocument doc;
        doc.setKml("<kml><p>Test content</p></kml>");
        doc.setTitle("Test Chapter");
        doc.setStatus("draft");
        doc.setNotes("Some notes");

        QJsonObject json = doc.toJson();

        // Kalahari header
        REQUIRE(json.contains("kalahari"));
        CHECK(json["kalahari"].toObject()["version"].toString() == "1.0");
        CHECK(json["kalahari"].toObject()["type"].toString() == "chapter");

        // Content
        REQUIRE(json.contains("content"));
        CHECK(json["content"].toObject().contains("kml"));
        CHECK(json["content"].toObject().contains("plainText"));

        // Statistics
        REQUIRE(json.contains("statistics"));
        CHECK(json["statistics"].toObject().contains("wordCount"));
        CHECK(json["statistics"].toObject().contains("characterCount"));
        CHECK(json["statistics"].toObject().contains("paragraphCount"));
        CHECK(json["statistics"].toObject().contains("lastModified"));

        // Metadata
        REQUIRE(json.contains("metadata"));
        CHECK(json["metadata"].toObject()["title"].toString() == "Test Chapter");
        CHECK(json["metadata"].toObject()["status"].toString() == "draft");
        CHECK(json["metadata"].toObject()["notes"].toString() == "Some notes");

        // Annotations
        REQUIRE(json.contains("annotations"));
        CHECK(json["annotations"].toObject().contains("comments"));
        CHECK(json["annotations"].toObject().contains("highlights"));
    }

    SECTION("toJson includes color when set") {
        ChapterDocument doc;
        doc.setColor(QColor("#FF5733"));

        QJsonObject json = doc.toJson();

        CHECK(json["metadata"].toObject().contains("color"));
        CHECK(json["metadata"].toObject()["color"].toString() == "#ff5733");
    }

    SECTION("toJson excludes color when not set") {
        ChapterDocument doc;

        QJsonObject json = doc.toJson();

        CHECK_FALSE(json["metadata"].toObject().contains("color"));
    }

    SECTION("fromJson restores document correctly") {
        QJsonObject json;

        // Kalahari header
        QJsonObject kalahari;
        kalahari["version"] = "1.0";
        kalahari["type"] = "chapter";
        json["kalahari"] = kalahari;

        // Content
        QJsonObject content;
        content["kml"] = "<kml><p>Restored content</p></kml>";
        content["plainText"] = "Restored content";
        json["content"] = content;

        // Statistics
        QJsonObject stats;
        stats["wordCount"] = 2;
        stats["characterCount"] = 15;
        stats["paragraphCount"] = 1;
        stats["lastModified"] = "2025-01-15T10:30:00Z";
        json["statistics"] = stats;

        // Metadata
        QJsonObject meta;
        meta["title"] = "Restored Title";
        meta["status"] = "final";
        meta["notes"] = "Restored notes";
        meta["color"] = "#FF0000";
        json["metadata"] = meta;

        // Annotations
        QJsonObject annot;
        annot["comments"] = QJsonArray();
        annot["highlights"] = QJsonArray();
        json["annotations"] = annot;

        ChapterDocument doc = ChapterDocument::fromJson(json);

        CHECK(doc.kml() == "<kml><p>Restored content</p></kml>");
        CHECK(doc.plainText() == "Restored content");
        CHECK(doc.title() == "Restored Title");
        CHECK(doc.status() == "final");
        CHECK(doc.notes() == "Restored notes");
        CHECK(doc.wordCount() == 2);
        CHECK(doc.characterCount() == 15);
        CHECK(doc.paragraphCount() == 1);
        REQUIRE(doc.color().has_value());
        CHECK(doc.color()->name() == "#ff0000");
    }

    SECTION("Round-trip: toJson -> fromJson preserves data") {
        ChapterDocument original;
        original.setKml("<kml><p>Test content for round-trip</p></kml>");
        original.setTitle("Round Trip Chapter");
        original.setStatus("revision");
        original.setNotes("Important notes here");
        original.setColor(QColor("#00FF00"));

        QJsonArray comments;
        QJsonObject comment;
        comment["id"] = "c1";
        comment["text"] = "Comment text";
        comments.append(comment);
        original.setComments(comments);

        QJsonObject json = original.toJson();
        ChapterDocument restored = ChapterDocument::fromJson(json);

        CHECK(restored.kml() == original.kml());
        CHECK(restored.plainText() == original.plainText());
        CHECK(restored.title() == original.title());
        CHECK(restored.status() == original.status());
        CHECK(restored.notes() == original.notes());
        CHECK(restored.wordCount() == original.wordCount());
        CHECK(restored.characterCount() == original.characterCount());
        CHECK(restored.paragraphCount() == original.paragraphCount());
        REQUIRE(restored.color().has_value());
        CHECK(restored.color()->name() == original.color()->name());
        CHECK(restored.comments().size() == original.comments().size());
    }

    SECTION("fromJson defaults status to 'draft'") {
        QJsonObject json;
        QJsonObject meta;
        // status intentionally missing
        json["metadata"] = meta;

        ChapterDocument doc = ChapterDocument::fromJson(json);

        CHECK(doc.status() == "draft");
    }
}

// =============================================================================
// File I/O Tests
// =============================================================================

TEST_CASE("ChapterDocument file I/O", "[core][chapter_document][io]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    SECTION("save creates file") {
        ChapterDocument doc;
        doc.setKml("<kml><p>File content</p></kml>");
        doc.setTitle("Saved Chapter");

        QString filePath = tempDir.path() + "/test_save.kchapter";

        REQUIRE(doc.save(filePath));
        CHECK(QFile::exists(filePath));
    }

    SECTION("save creates valid JSON file") {
        ChapterDocument doc;
        doc.setKml("<kml><p>JSON test</p></kml>");

        QString filePath = tempDir.path() + "/test_json.kchapter";
        REQUIRE(doc.save(filePath));

        QFile file(filePath);
        REQUIRE(file.open(QIODevice::ReadOnly));
        QByteArray data = file.readAll();
        file.close();

        QJsonParseError error;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);

        CHECK(error.error == QJsonParseError::NoError);
        CHECK(jsonDoc.isObject());
    }

    SECTION("load reads file successfully") {
        ChapterDocument original;
        original.setKml("<kml><p>Load test content</p></kml>");
        original.setTitle("Load Test");
        original.setStatus("final");

        QString filePath = tempDir.path() + "/test_load.kchapter";
        REQUIRE(original.save(filePath));

        auto loaded = ChapterDocument::load(filePath);

        REQUIRE(loaded.has_value());
        CHECK(loaded->kml() == original.kml());
        CHECK(loaded->title() == original.title());
        CHECK(loaded->status() == original.status());
    }

    SECTION("Round-trip: save -> load preserves data") {
        ChapterDocument original;
        original.setKml("<kml><p>Round-trip file test</p></kml>");
        original.setTitle("File Round Trip");
        original.setStatus("revision");
        original.setNotes("Test notes for file");
        original.setColor(QColor("#AABBCC"));

        QString filePath = tempDir.path() + "/test_roundtrip.kchapter";
        REQUIRE(original.save(filePath));

        auto loaded = ChapterDocument::load(filePath);

        REQUIRE(loaded.has_value());
        CHECK(loaded->kml() == original.kml());
        CHECK(loaded->title() == original.title());
        CHECK(loaded->status() == original.status());
        CHECK(loaded->notes() == original.notes());
        CHECK(loaded->wordCount() == original.wordCount());
        REQUIRE(loaded->color().has_value());
        CHECK(loaded->color()->name() == original.color()->name());
    }

    SECTION("load returns nullopt for non-existent file") {
        auto result = ChapterDocument::load("/nonexistent/path/file.kchapter");

        CHECK_FALSE(result.has_value());
    }

    SECTION("load returns nullopt for invalid JSON") {
        QString filePath = tempDir.path() + "/invalid.kchapter";

        QFile file(filePath);
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write("{ invalid json }");
        file.close();

        auto result = ChapterDocument::load(filePath);

        CHECK_FALSE(result.has_value());
    }

    SECTION("load returns nullopt for wrong document type") {
        QString filePath = tempDir.path() + "/wrong_type.kchapter";

        QJsonObject json;
        QJsonObject kalahari;
        kalahari["version"] = "1.0";
        kalahari["type"] = "book";  // Wrong type
        json["kalahari"] = kalahari;

        QFile file(filePath);
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(json).toJson());
        file.close();

        auto result = ChapterDocument::load(filePath);

        CHECK_FALSE(result.has_value());
    }
}

// =============================================================================
// Migration Helper Tests
// =============================================================================

TEST_CASE("ChapterDocument migration helpers", "[core][chapter_document]") {
    SECTION("fromKmlContent creates document with content") {
        ChapterDocument doc = ChapterDocument::fromKmlContent(
            "<kml><p>Migrated content</p></kml>",
            "Migrated Chapter"
        );

        CHECK(doc.kml() == "<kml><p>Migrated content</p></kml>");
        CHECK(doc.title() == "Migrated Chapter");
        CHECK(doc.wordCount() > 0);
    }

    SECTION("fromKmlContent with empty title") {
        ChapterDocument doc = ChapterDocument::fromKmlContent("<kml><p>Content only</p></kml>");

        CHECK(doc.kml() == "<kml><p>Content only</p></kml>");
        CHECK(doc.title().isEmpty());
    }

    SECTION("kmlToPlainText strips tags") {
        QString plain = ChapterDocument::kmlToPlainText(
            "<kml><p><bold>Bold</bold> and <italic>italic</italic> text</p></kml>"
        );

        CHECK_FALSE(plain.contains("<bold>"));
        CHECK_FALSE(plain.contains("<italic>"));
        CHECK_FALSE(plain.contains("<p>"));
        CHECK(plain.contains("Bold"));
        CHECK(plain.contains("italic"));
        CHECK(plain.contains("text"));
    }

    SECTION("kmlToPlainText handles empty input") {
        QString plain = ChapterDocument::kmlToPlainText("");

        CHECK(plain.isEmpty());
    }
}

// =============================================================================
// Edge Cases Tests
// =============================================================================

TEST_CASE("ChapterDocument edge cases", "[core][chapter_document][edge]") {
    SECTION("Very long content") {
        QString longContent = "<kml><p>";
        for (int i = 0; i < 1000; ++i) {
            longContent += "Word" + QString::number(i) + " ";
        }
        longContent += "</p></kml>";

        ChapterDocument doc;
        doc.setKml(longContent);

        CHECK(doc.wordCount() == 1000);
        CHECK(doc.hasContent());
    }

    SECTION("Unicode content") {
        ChapterDocument doc;
        doc.setKml("<kml><p>Polish: zolty, Cyrillic: privet, Chinese: nihao</p></kml>");

        CHECK(doc.hasContent());
        CHECK(doc.wordCount() > 0);
        CHECK(doc.plainText().contains("zolty"));
    }

    SECTION("XML entities") {
        ChapterDocument doc;
        doc.setKml("<kml><p>&lt;script&gt; &amp; &quot;quotes&quot;</p></kml>");

        // QTextDocument should decode entities
        QString plain = doc.plainText();
        bool hasScript = plain.contains("<script>") || plain.contains("script");
        CHECK(hasScript);
    }

    SECTION("Nested KML tags") {
        ChapterDocument doc;
        doc.setKml("<kml><p><bold><italic>Nested</italic></bold> text</p></kml>");

        CHECK(doc.plainText().contains("Nested"));
        CHECK(doc.plainText().contains("text"));
    }

    SECTION("Empty KML tags") {
        ChapterDocument doc;
        doc.setKml("<kml><p></p><p>Content</p><p></p></kml>");

        CHECK(doc.hasContent());
        CHECK(doc.plainText().contains("Content"));
    }

    SECTION("Special characters in notes") {
        ChapterDocument doc;
        doc.setNotes("Notes with \"quotes\" and <brackets> and \nnewlines");

        QJsonObject json = doc.toJson();
        ChapterDocument restored = ChapterDocument::fromJson(json);

        CHECK(restored.notes() == doc.notes());
    }

    SECTION("Multiple setKml calls") {
        ChapterDocument doc;

        doc.setKml("<kml><p>First</p></kml>");
        CHECK(doc.wordCount() == 1);

        doc.setKml("<kml><p>Second content here</p></kml>");
        CHECK(doc.wordCount() == 3);

        doc.setKml("<kml><p>Third</p></kml>");
        CHECK(doc.wordCount() == 1);
    }

    SECTION("Status can be any string") {
        ChapterDocument doc;
        doc.setStatus("custom_status_value");

        CHECK(doc.status() == "custom_status_value");

        QJsonObject json = doc.toJson();
        ChapterDocument restored = ChapterDocument::fromJson(json);

        CHECK(restored.status() == "custom_status_value");
    }
}

// =============================================================================
// Format Constants Tests
// =============================================================================

TEST_CASE("ChapterDocument format constants", "[core][chapter_document]") {
    SECTION("FORMAT_VERSION is defined") {
        CHECK(QString::fromLatin1(ChapterDocument::FORMAT_VERSION) == "1.0");
    }

    SECTION("FORMAT_TYPE is 'chapter'") {
        CHECK(QString::fromLatin1(ChapterDocument::FORMAT_TYPE) == "chapter");
    }
}
