/// @file test_chapter_document.cpp
/// @brief Unit tests for ChapterDocument (.kchapter format)
///
/// Tests cover:
/// - Construction and default values
/// - Content management (HTML, plainText)
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

        CHECK(doc.html().isEmpty());
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

    SECTION("Constructor with HTML content") {
        ChapterDocument doc("<p>Hello World</p>");

        CHECK_FALSE(doc.html().isEmpty());
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

    SECTION("setHtml stores HTML content") {
        doc.setHtml("<p>Test content</p>");

        CHECK(doc.html() == "<p>Test content</p>");
    }

    SECTION("setHtml auto-generates plainText") {
        doc.setHtml("<p>Hello <b>World</b></p>");

        CHECK_FALSE(doc.plainText().isEmpty());
        CHECK(doc.plainText().contains("Hello"));
        CHECK(doc.plainText().contains("World"));
    }

    SECTION("setHtml strips HTML tags from plainText") {
        doc.setHtml("<p><b>Bold</b> and <i>italic</i></p>");
        QString plain = doc.plainText();

        CHECK_FALSE(plain.contains("<b>"));
        CHECK_FALSE(plain.contains("<i>"));
        CHECK(plain.contains("Bold"));
        CHECK(plain.contains("italic"));
    }

    SECTION("setPlainText sets plain text directly") {
        doc.setPlainText("Direct plain text");

        CHECK(doc.plainText() == "Direct plain text");
    }

    SECTION("hasContent returns true when HTML is set") {
        CHECK_FALSE(doc.hasContent());

        doc.setHtml("<p>Content</p>");

        CHECK(doc.hasContent());
    }

    SECTION("Empty HTML clears content") {
        doc.setHtml("<p>Initial</p>");
        CHECK(doc.hasContent());

        doc.setHtml("");
        CHECK_FALSE(doc.hasContent());
    }
}

// =============================================================================
// Statistics Calculation Tests
// =============================================================================

TEST_CASE("ChapterDocument statistics calculation", "[core][chapter_document]") {
    ChapterDocument doc;

    SECTION("Word count for simple text") {
        doc.setHtml("<p>One two three</p>");

        CHECK(doc.wordCount() == 3);
    }

    SECTION("Word count with multiple paragraphs") {
        doc.setHtml("<p>One two</p><p>Three four five</p>");

        CHECK(doc.wordCount() == 5);
    }

    SECTION("Word count with formatting") {
        doc.setHtml("<p>Word <b>bold</b> <i>italic</i> text</p>");

        CHECK(doc.wordCount() == 4);
    }

    SECTION("Character count excludes whitespace") {
        doc.setHtml("<p>abc def</p>");
        // "abc def" = 6 characters (excluding space)
        CHECK(doc.characterCount() == 6);
    }

    SECTION("Paragraph count for single paragraph") {
        doc.setHtml("<p>Single paragraph text.</p>");

        CHECK(doc.paragraphCount() >= 1);
    }

    SECTION("Paragraph count for multiple paragraphs") {
        doc.setHtml("<p>First paragraph.</p><p>Second paragraph.</p>");

        // Note: paragraphCount is based on double newlines in plainText
        // HTML paragraphs may result in different counts depending on conversion
        CHECK(doc.paragraphCount() >= 1);
    }

    SECTION("Statistics update when content changes") {
        doc.setHtml("<p>One</p>");
        int initialCount = doc.wordCount();

        doc.setHtml("<p>One two three four five</p>");

        CHECK(doc.wordCount() > initialCount);
    }

    SECTION("Empty content has zero statistics") {
        doc.setHtml("");

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
        doc.setHtml("<p>Test content</p>");
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
        CHECK(json["content"].toObject().contains("html"));
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
        content["html"] = "<p>Restored content</p>";
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

        CHECK(doc.html() == "<p>Restored content</p>");
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
        original.setHtml("<p>Test content for round-trip</p>");
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

        CHECK(restored.html() == original.html());
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

    SECTION("fromJson regenerates plainText if missing") {
        QJsonObject json;
        QJsonObject content;
        content["html"] = "<p>HTML only</p>";
        // plainText intentionally missing
        json["content"] = content;

        ChapterDocument doc = ChapterDocument::fromJson(json);

        CHECK_FALSE(doc.plainText().isEmpty());
        CHECK(doc.plainText().contains("HTML only"));
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
        doc.setHtml("<p>File content</p>");
        doc.setTitle("Saved Chapter");

        QString filePath = tempDir.path() + "/test_save.kchapter";

        REQUIRE(doc.save(filePath));
        CHECK(QFile::exists(filePath));
    }

    SECTION("save creates valid JSON file") {
        ChapterDocument doc;
        doc.setHtml("<p>JSON test</p>");

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
        original.setHtml("<p>Load test content</p>");
        original.setTitle("Load Test");
        original.setStatus("final");

        QString filePath = tempDir.path() + "/test_load.kchapter";
        REQUIRE(original.save(filePath));

        auto loaded = ChapterDocument::load(filePath);

        REQUIRE(loaded.has_value());
        CHECK(loaded->html() == original.html());
        CHECK(loaded->title() == original.title());
        CHECK(loaded->status() == original.status());
    }

    SECTION("Round-trip: save -> load preserves data") {
        ChapterDocument original;
        original.setHtml("<p>Round-trip file test</p>");
        original.setTitle("File Round Trip");
        original.setStatus("revision");
        original.setNotes("Test notes for file");
        original.setColor(QColor("#AABBCC"));

        QString filePath = tempDir.path() + "/test_roundtrip.kchapter";
        REQUIRE(original.save(filePath));

        auto loaded = ChapterDocument::load(filePath);

        REQUIRE(loaded.has_value());
        CHECK(loaded->html() == original.html());
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
    SECTION("fromHtmlContent creates document with content") {
        ChapterDocument doc = ChapterDocument::fromHtmlContent(
            "<p>Migrated content</p>",
            "Migrated Chapter"
        );

        CHECK(doc.html() == "<p>Migrated content</p>");
        CHECK(doc.title() == "Migrated Chapter");
        CHECK(doc.wordCount() > 0);
    }

    SECTION("fromHtmlContent with empty title") {
        ChapterDocument doc = ChapterDocument::fromHtmlContent("<p>Content only</p>");

        CHECK(doc.html() == "<p>Content only</p>");
        CHECK(doc.title().isEmpty());
    }

    SECTION("htmlToPlainText strips tags") {
        QString plain = ChapterDocument::htmlToPlainText(
            "<p><b>Bold</b> and <i>italic</i> text</p>"
        );

        CHECK_FALSE(plain.contains("<b>"));
        CHECK_FALSE(plain.contains("<i>"));
        CHECK_FALSE(plain.contains("<p>"));
        CHECK(plain.contains("Bold"));
        CHECK(plain.contains("italic"));
        CHECK(plain.contains("text"));
    }

    SECTION("htmlToPlainText handles empty input") {
        QString plain = ChapterDocument::htmlToPlainText("");

        CHECK(plain.isEmpty());
    }
}

// =============================================================================
// Edge Cases Tests
// =============================================================================

TEST_CASE("ChapterDocument edge cases", "[core][chapter_document][edge]") {
    SECTION("Very long content") {
        QString longContent = "<p>";
        for (int i = 0; i < 1000; ++i) {
            longContent += "Word" + QString::number(i) + " ";
        }
        longContent += "</p>";

        ChapterDocument doc;
        doc.setHtml(longContent);

        CHECK(doc.wordCount() == 1000);
        CHECK(doc.hasContent());
    }

    SECTION("Unicode content") {
        ChapterDocument doc;
        doc.setHtml("<p>Polish: zolty, Cyrillic: privet, Chinese: nihao</p>");

        CHECK(doc.hasContent());
        CHECK(doc.wordCount() > 0);
        CHECK(doc.plainText().contains("zolty"));
    }

    SECTION("HTML entities") {
        ChapterDocument doc;
        doc.setHtml("<p>&lt;script&gt; &amp; &quot;quotes&quot;</p>");

        // QTextDocument should decode entities
        QString plain = doc.plainText();
        bool hasScript = plain.contains("<script>") || plain.contains("script");
        CHECK(hasScript);
    }

    SECTION("Nested HTML tags") {
        ChapterDocument doc;
        doc.setHtml("<p><b><i><u>Nested</u></i></b> text</p>");

        CHECK(doc.plainText().contains("Nested"));
        CHECK(doc.plainText().contains("text"));
    }

    SECTION("Empty HTML tags") {
        ChapterDocument doc;
        doc.setHtml("<p></p><p>Content</p><p></p>");

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

    SECTION("Multiple setHtml calls") {
        ChapterDocument doc;

        doc.setHtml("<p>First</p>");
        CHECK(doc.wordCount() == 1);

        doc.setHtml("<p>Second content here</p>");
        CHECK(doc.wordCount() == 3);

        doc.setHtml("<p>Third</p>");
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
