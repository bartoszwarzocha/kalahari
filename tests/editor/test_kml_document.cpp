/// @file test_kml_document.cpp
/// @brief Unit tests for KML Document (OpenSpec #00042 Phase 1.8)

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/kml_document.h>
#include <kalahari/editor/kml_paragraph.h>
#include <kalahari/editor/kml_text_run.h>
#include <kalahari/editor/kml_inline_elements.h>
#include <memory>

using namespace kalahari::editor;

// =============================================================================
// Helper Functions
// =============================================================================

std::unique_ptr<KmlParagraph> makePara(const QString& text) {
    return std::make_unique<KmlParagraph>(text);
}

std::unique_ptr<KmlParagraph> makePara(const QString& text, const QString& style) {
    return std::make_unique<KmlParagraph>(text, style);
}

// =============================================================================
// Test Observer (implements IDocumentObserver)
// =============================================================================

/// @brief Helper class to track observer notifications
class TestObserver : public IDocumentObserver {
public:
    int contentChangedCount = 0;
    int paragraphInsertedCount = 0;
    int paragraphRemovedCount = 0;
    int paragraphModifiedCount = 0;
    int lastInsertedIndex = -1;
    int lastRemovedIndex = -1;
    int lastModifiedIndex = -1;

    void reset() {
        contentChangedCount = 0;
        paragraphInsertedCount = 0;
        paragraphRemovedCount = 0;
        paragraphModifiedCount = 0;
        lastInsertedIndex = -1;
        lastRemovedIndex = -1;
        lastModifiedIndex = -1;
    }

    void onContentChanged() override { ++contentChangedCount; }
    void onParagraphInserted(int index) override { ++paragraphInsertedCount; lastInsertedIndex = index; }
    void onParagraphRemoved(int index) override { ++paragraphRemovedCount; lastRemovedIndex = index; }
    void onParagraphModified(int index) override { ++paragraphModifiedCount; lastModifiedIndex = index; }
};

// =============================================================================
// Constructor Tests
// =============================================================================

TEST_CASE("KmlDocument default constructor", "[editor][kml_document]") {
    KmlDocument doc;

    SECTION("Empty state") {
        REQUIRE(doc.isEmpty());
        REQUIRE(doc.paragraphCount() == 0);
        REQUIRE(doc.length() == 0);
        REQUIRE(doc.plainText().isEmpty());
    }

    SECTION("Not modified initially") {
        REQUIRE(doc.isModified() == false);
    }
}

// =============================================================================
// Paragraph Container Tests
// =============================================================================

TEST_CASE("KmlDocument addParagraph", "[editor][kml_document]") {
    KmlDocument doc;

    SECTION("Add single paragraph") {
        doc.addParagraph(makePara("First paragraph"));

        REQUIRE(doc.paragraphCount() == 1);
        REQUIRE(doc.isEmpty() == false);
        REQUIRE(doc.paragraph(0)->plainText() == "First paragraph");
    }

    SECTION("Add multiple paragraphs") {
        doc.addParagraph(makePara("First"));
        doc.addParagraph(makePara("Second"));
        doc.addParagraph(makePara("Third"));

        REQUIRE(doc.paragraphCount() == 3);
        REQUIRE(doc.paragraph(0)->plainText() == "First");
        REQUIRE(doc.paragraph(1)->plainText() == "Second");
        REQUIRE(doc.paragraph(2)->plainText() == "Third");
    }

    SECTION("Ignore nullptr") {
        doc.addParagraph(nullptr);
        REQUIRE(doc.paragraphCount() == 0);
    }

    SECTION("Sets modified flag") {
        REQUIRE(doc.isModified() == false);
        doc.addParagraph(makePara("Test"));
        REQUIRE(doc.isModified() == true);
    }
}

TEST_CASE("KmlDocument insertParagraph", "[editor][kml_document]") {
    KmlDocument doc;
    doc.addParagraph(makePara("First"));
    doc.addParagraph(makePara("Third"));
    doc.resetModified();

    SECTION("Insert in middle") {
        doc.insertParagraph(1, makePara("Second"));

        REQUIRE(doc.paragraphCount() == 3);
        REQUIRE(doc.paragraph(0)->plainText() == "First");
        REQUIRE(doc.paragraph(1)->plainText() == "Second");
        REQUIRE(doc.paragraph(2)->plainText() == "Third");
    }

    SECTION("Insert at beginning") {
        doc.insertParagraph(0, makePara("Zero"));

        REQUIRE(doc.paragraphCount() == 3);
        REQUIRE(doc.paragraph(0)->plainText() == "Zero");
        REQUIRE(doc.paragraph(1)->plainText() == "First");
    }

    SECTION("Insert at end (beyond size)") {
        doc.insertParagraph(100, makePara("End"));

        REQUIRE(doc.paragraphCount() == 3);
        REQUIRE(doc.paragraph(2)->plainText() == "End");
    }

    SECTION("Insert with negative index") {
        doc.insertParagraph(-5, makePara("Negative"));

        REQUIRE(doc.paragraphCount() == 3);
        REQUIRE(doc.paragraph(0)->plainText() == "Negative");
    }

    SECTION("Ignore nullptr") {
        doc.insertParagraph(1, nullptr);
        REQUIRE(doc.paragraphCount() == 2);
    }

    SECTION("Sets modified flag") {
        doc.insertParagraph(1, makePara("Middle"));
        REQUIRE(doc.isModified() == true);
    }
}

TEST_CASE("KmlDocument removeParagraph", "[editor][kml_document]") {
    KmlDocument doc;
    doc.addParagraph(makePara("First"));
    doc.addParagraph(makePara("Second"));
    doc.addParagraph(makePara("Third"));
    doc.resetModified();

    SECTION("Remove middle paragraph") {
        auto removed = doc.removeParagraph(1);

        REQUIRE(removed != nullptr);
        REQUIRE(removed->plainText() == "Second");
        REQUIRE(doc.paragraphCount() == 2);
        REQUIRE(doc.paragraph(0)->plainText() == "First");
        REQUIRE(doc.paragraph(1)->plainText() == "Third");
    }

    SECTION("Remove first paragraph") {
        auto removed = doc.removeParagraph(0);

        REQUIRE(removed != nullptr);
        REQUIRE(removed->plainText() == "First");
        REQUIRE(doc.paragraphCount() == 2);
    }

    SECTION("Remove last paragraph") {
        auto removed = doc.removeParagraph(2);

        REQUIRE(removed != nullptr);
        REQUIRE(removed->plainText() == "Third");
        REQUIRE(doc.paragraphCount() == 2);
    }

    SECTION("Remove invalid index returns nullptr") {
        auto removed = doc.removeParagraph(100);
        REQUIRE(removed == nullptr);
        REQUIRE(doc.paragraphCount() == 3);

        removed = doc.removeParagraph(-1);
        REQUIRE(removed == nullptr);
    }

    SECTION("Sets modified flag") {
        doc.removeParagraph(1);
        REQUIRE(doc.isModified() == true);
    }
}

TEST_CASE("KmlDocument clear", "[editor][kml_document]") {
    KmlDocument doc;
    doc.addParagraph(makePara("First"));
    doc.addParagraph(makePara("Second"));
    doc.addParagraph(makePara("Third"));
    doc.resetModified();

    SECTION("Clears all paragraphs") {
        doc.clear();

        REQUIRE(doc.isEmpty());
        REQUIRE(doc.paragraphCount() == 0);
    }

    SECTION("Sets modified flag") {
        doc.clear();
        REQUIRE(doc.isModified() == true);
    }

    SECTION("Clear empty document does not set modified") {
        KmlDocument emptyDoc;
        emptyDoc.clear();
        REQUIRE(emptyDoc.isModified() == false);
    }
}

TEST_CASE("KmlDocument paragraph access", "[editor][kml_document]") {
    KmlDocument doc;
    doc.addParagraph(makePara("Test"));

    SECTION("Valid index") {
        auto* para = doc.paragraph(0);
        REQUIRE(para != nullptr);
        REQUIRE(para->plainText() == "Test");
    }

    SECTION("Invalid indices return nullptr") {
        REQUIRE(doc.paragraph(-1) == nullptr);
        REQUIRE(doc.paragraph(1) == nullptr);
        REQUIRE(doc.paragraph(100) == nullptr);
    }

    SECTION("Const access") {
        const KmlDocument& constDoc = doc;
        const auto* para = constDoc.paragraph(0);
        REQUIRE(para != nullptr);
        REQUIRE(para->plainText() == "Test");
    }

    SECTION("Mutable access allows modification") {
        auto* para = doc.paragraph(0);
        para->setStyleId("heading1");
        REQUIRE(doc.paragraph(0)->styleId() == "heading1");
    }
}

TEST_CASE("KmlDocument paragraphs() access", "[editor][kml_document]") {
    KmlDocument doc;
    doc.addParagraph(makePara("A"));
    doc.addParagraph(makePara("B"));

    const auto& paragraphs = doc.paragraphs();

    REQUIRE(paragraphs.size() == 2);
    REQUIRE(paragraphs[0]->plainText() == "A");
    REQUIRE(paragraphs[1]->plainText() == "B");
}

// =============================================================================
// Content Tests
// =============================================================================

TEST_CASE("KmlDocument plainText", "[editor][kml_document]") {
    KmlDocument doc;

    SECTION("Empty document") {
        REQUIRE(doc.plainText().isEmpty());
    }

    SECTION("Single paragraph") {
        doc.addParagraph(makePara("Hello, world!"));
        REQUIRE(doc.plainText() == "Hello, world!");
    }

    SECTION("Multiple paragraphs separated by newlines") {
        doc.addParagraph(makePara("First paragraph"));
        doc.addParagraph(makePara("Second paragraph"));
        doc.addParagraph(makePara("Third paragraph"));

        QString expected = "First paragraph\nSecond paragraph\nThird paragraph";
        REQUIRE(doc.plainText() == expected);
    }

    SECTION("Empty paragraphs") {
        doc.addParagraph(makePara(""));
        doc.addParagraph(makePara("Middle"));
        doc.addParagraph(makePara(""));

        QString expected = "\nMiddle\n";
        REQUIRE(doc.plainText() == expected);
    }
}

TEST_CASE("KmlDocument length", "[editor][kml_document]") {
    KmlDocument doc;

    SECTION("Empty document") {
        REQUIRE(doc.length() == 0);
    }

    SECTION("Single paragraph") {
        doc.addParagraph(makePara("Hello"));
        REQUIRE(doc.length() == 5);
    }

    SECTION("Multiple paragraphs") {
        doc.addParagraph(makePara("Hello"));
        doc.addParagraph(makePara("World"));
        // Length is sum of paragraph lengths (not including newlines)
        REQUIRE(doc.length() == 10);
    }
}

TEST_CASE("KmlDocument isEmpty", "[editor][kml_document]") {
    KmlDocument doc;

    SECTION("Empty document is empty") {
        REQUIRE(doc.isEmpty());
    }

    SECTION("Document with paragraph is not empty") {
        doc.addParagraph(makePara("Content"));
        REQUIRE(doc.isEmpty() == false);
    }

    SECTION("Document with empty paragraph is not empty") {
        // Empty paragraph still counts as a paragraph
        doc.addParagraph(makePara(""));
        REQUIRE(doc.isEmpty() == false);
    }
}

// =============================================================================
// Modification Tracking Tests
// =============================================================================

TEST_CASE("KmlDocument modification tracking", "[editor][kml_document]") {
    KmlDocument doc;

    SECTION("Initially not modified") {
        REQUIRE(doc.isModified() == false);
    }

    SECTION("Adding sets modified") {
        doc.addParagraph(makePara("Test"));
        REQUIRE(doc.isModified() == true);
    }

    SECTION("Reset modified") {
        doc.addParagraph(makePara("Test"));
        doc.resetModified();
        REQUIRE(doc.isModified() == false);
    }

    SECTION("setModified explicit") {
        doc.setModified(true);
        REQUIRE(doc.isModified() == true);

        doc.setModified(false);
        REQUIRE(doc.isModified() == false);
    }
}

// =============================================================================
// Serialization Tests
// =============================================================================

TEST_CASE("KmlDocument toKml", "[editor][kml_document]") {
    SECTION("Empty document") {
        KmlDocument doc;
        QString kml = doc.toKml();

        REQUIRE(kml.contains("<document>"));
        REQUIRE(kml.contains("</document>"));
    }

    SECTION("Document with paragraphs") {
        KmlDocument doc;
        doc.addParagraph(makePara("First paragraph"));
        doc.addParagraph(makePara("Second paragraph"));

        QString kml = doc.toKml();

        REQUIRE(kml.contains("<document>"));
        REQUIRE(kml.contains("<p>"));
        REQUIRE(kml.contains("First paragraph"));
        REQUIRE(kml.contains("Second paragraph"));
        REQUIRE(kml.contains("</p>"));
        REQUIRE(kml.contains("</document>"));
    }

    SECTION("Document with styled paragraphs") {
        KmlDocument doc;
        doc.addParagraph(makePara("Chapter One", "heading1"));
        doc.addParagraph(makePara("Content paragraph"));

        QString kml = doc.toKml();

        REQUIRE(kml.contains("<p style=\"heading1\">"));
        REQUIRE(kml.contains("Chapter One"));
    }

    SECTION("Document with formatted content") {
        KmlDocument doc;
        auto para = std::make_unique<KmlParagraph>();
        para->addElement(std::make_unique<KmlTextRun>("Normal "));
        auto bold = std::make_unique<KmlBold>();
        bold->appendChild(std::make_unique<KmlTextRun>("bold"));
        para->addElement(std::move(bold));
        doc.addParagraph(std::move(para));

        QString kml = doc.toKml();

        REQUIRE(kml.contains("<b>"));
        REQUIRE(kml.contains("bold"));
        REQUIRE(kml.contains("</b>"));
    }
}

// =============================================================================
// Clone Tests
// =============================================================================

TEST_CASE("KmlDocument clone", "[editor][kml_document]") {
    SECTION("Clone empty document") {
        KmlDocument original;
        auto cloned = original.clone();

        REQUIRE(cloned != nullptr);
        REQUIRE(cloned->isEmpty());
    }

    SECTION("Clone document with content") {
        KmlDocument original;
        original.addParagraph(makePara("First"));
        original.addParagraph(makePara("Second"));

        auto cloned = original.clone();

        REQUIRE(cloned != nullptr);
        REQUIRE(cloned->paragraphCount() == 2);
        REQUIRE(cloned->paragraph(0)->plainText() == "First");
        REQUIRE(cloned->paragraph(1)->plainText() == "Second");
    }

    SECTION("Clone is independent") {
        KmlDocument original;
        original.addParagraph(makePara("Original"));

        auto cloned = original.clone();

        original.clear();
        original.addParagraph(makePara("Modified"));

        REQUIRE(cloned->paragraph(0)->plainText() == "Original");
        REQUIRE(original.paragraph(0)->plainText() == "Modified");
    }

    SECTION("Clone preserves modified state") {
        KmlDocument original;
        original.addParagraph(makePara("Test"));
        REQUIRE(original.isModified() == true);

        auto cloned = original.clone();
        REQUIRE(cloned->isModified() == true);
    }

    SECTION("Clone preserves paragraph styles") {
        KmlDocument original;
        original.addParagraph(makePara("Heading", "heading1"));

        auto cloned = original.clone();
        REQUIRE(cloned->paragraph(0)->styleId() == "heading1");
    }
}

// =============================================================================
// Observer Tests
// =============================================================================

TEST_CASE("KmlDocument observer notifications", "[editor][kml_document][observer]") {
    KmlDocument doc;
    TestObserver observer;
    doc.addObserver(&observer);

    SECTION("addParagraph notifies observer") {
        doc.addParagraph(makePara("Test"));

        REQUIRE(observer.contentChangedCount == 1);
        REQUIRE(observer.paragraphInsertedCount == 1);
        REQUIRE(observer.lastInsertedIndex == 0);
    }

    SECTION("insertParagraph notifies observer with correct index") {
        doc.addParagraph(makePara("First"));
        doc.addParagraph(makePara("Third"));
        observer.reset();

        doc.insertParagraph(1, makePara("Second"));

        REQUIRE(observer.paragraphInsertedCount == 1);
        REQUIRE(observer.lastInsertedIndex == 1);
    }

    SECTION("removeParagraph notifies observer") {
        doc.addParagraph(makePara("First"));
        doc.addParagraph(makePara("Second"));
        observer.reset();

        doc.removeParagraph(1);

        REQUIRE(observer.contentChangedCount == 1);
        REQUIRE(observer.paragraphRemovedCount == 1);
        REQUIRE(observer.lastRemovedIndex == 1);
    }

    SECTION("clear notifies observer") {
        doc.addParagraph(makePara("Test"));
        observer.reset();

        doc.clear();

        REQUIRE(observer.contentChangedCount == 1);
    }

    SECTION("notifyParagraphModified notifies observer") {
        doc.addParagraph(makePara("Test"));
        observer.reset();

        doc.notifyParagraphModified(0);

        REQUIRE(observer.contentChangedCount == 1);
        REQUIRE(observer.paragraphModifiedCount == 1);
        REQUIRE(observer.lastModifiedIndex == 0);
    }

    SECTION("notifyParagraphModified ignores invalid index") {
        doc.addParagraph(makePara("Test"));
        observer.reset();

        doc.notifyParagraphModified(-1);
        doc.notifyParagraphModified(100);

        REQUIRE(observer.paragraphModifiedCount == 0);
    }

    SECTION("No notifications when nullptr added") {
        doc.addParagraph(nullptr);

        REQUIRE(observer.contentChangedCount == 0);
        REQUIRE(observer.paragraphInsertedCount == 0);
    }
}

TEST_CASE("KmlDocument observer management", "[editor][kml_document][observer]") {
    KmlDocument doc;
    TestObserver observer1;
    TestObserver observer2;

    SECTION("Multiple observers receive notifications") {
        doc.addObserver(&observer1);
        doc.addObserver(&observer2);

        doc.addParagraph(makePara("Test"));

        REQUIRE(observer1.contentChangedCount == 1);
        REQUIRE(observer2.contentChangedCount == 1);
    }

    SECTION("Remove observer stops notifications") {
        doc.addObserver(&observer1);
        doc.removeObserver(&observer1);

        doc.addParagraph(makePara("Test"));

        REQUIRE(observer1.contentChangedCount == 0);
    }

    SECTION("Adding same observer twice is idempotent") {
        doc.addObserver(&observer1);
        doc.addObserver(&observer1);

        doc.addParagraph(makePara("Test"));

        REQUIRE(observer1.contentChangedCount == 1);
    }

    SECTION("Removing non-existent observer is safe") {
        doc.removeObserver(&observer1);  // Not added
        doc.addParagraph(makePara("Test"));  // Should not crash
        REQUIRE(doc.paragraphCount() == 1);
    }
}

// =============================================================================
// Copy/Move Tests
// =============================================================================

TEST_CASE("KmlDocument copy constructor", "[editor][kml_document]") {
    KmlDocument original;
    original.addParagraph(makePara("Test", "style1"));

    TestObserver observer;
    original.addObserver(&observer);

    KmlDocument copy(original);

    SECTION("Content is copied") {
        REQUIRE(copy.paragraphCount() == 1);
        REQUIRE(copy.paragraph(0)->plainText() == "Test");
        REQUIRE(copy.paragraph(0)->styleId() == "style1");
    }

    SECTION("Copy is independent") {
        copy.clear();
        REQUIRE(original.paragraphCount() == 1);
    }

    SECTION("Observers are NOT copied") {
        // Modify copy - original observer should NOT be notified
        observer.reset();
        copy.addParagraph(makePara("New"));
        REQUIRE(observer.contentChangedCount == 0);
    }
}

TEST_CASE("KmlDocument move constructor", "[editor][kml_document]") {
    KmlDocument original;
    original.addParagraph(makePara("Test"));

    KmlDocument moved(std::move(original));

    REQUIRE(moved.paragraphCount() == 1);
    REQUIRE(moved.paragraph(0)->plainText() == "Test");
}

TEST_CASE("KmlDocument copy assignment", "[editor][kml_document]") {
    KmlDocument source;
    source.addParagraph(makePara("Source"));

    KmlDocument target;
    target.addParagraph(makePara("Target"));

    target = source;

    REQUIRE(target.paragraphCount() == 1);
    REQUIRE(target.paragraph(0)->plainText() == "Source");

    // Self-assignment
    target = target;
    REQUIRE(target.paragraphCount() == 1);
}

TEST_CASE("KmlDocument move assignment", "[editor][kml_document]") {
    KmlDocument source;
    source.addParagraph(makePara("Source"));

    KmlDocument target;
    target.addParagraph(makePara("Target"));

    target = std::move(source);

    REQUIRE(target.paragraphCount() == 1);
    REQUIRE(target.paragraph(0)->plainText() == "Source");
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("KmlDocument with Unicode content", "[editor][kml_document]") {
    KmlDocument doc;
    doc.addParagraph(makePara(QString::fromUtf8(u8"Polski tekst: Zaz\u00F3\u0142\u0107 g\u0119\u015Bl\u0105 ja\u017A\u0144")));
    doc.addParagraph(makePara(QString::fromUtf8(u8"\u4E2D\u6587\u6587\u672C")));  // Chinese
    doc.addParagraph(makePara(QString::fromUtf8(u8"\u65E5\u672C\u8A9E")));  // Japanese

    REQUIRE(doc.paragraphCount() == 3);
    REQUIRE(doc.paragraph(0)->plainText().contains(QString::fromUtf8(u8"Zaz\u00F3\u0142\u0107")));
}

TEST_CASE("KmlDocument with empty paragraphs", "[editor][kml_document]") {
    KmlDocument doc;
    doc.addParagraph(makePara(""));
    doc.addParagraph(makePara(""));
    doc.addParagraph(makePara(""));

    SECTION("Has paragraphs but no content") {
        REQUIRE(doc.paragraphCount() == 3);
        REQUIRE(doc.length() == 0);
        REQUIRE(doc.isEmpty() == false);  // Has paragraphs!
    }

    SECTION("plainText shows separators") {
        // Empty paragraphs separated by newlines
        REQUIRE(doc.plainText() == "\n\n");
    }
}

TEST_CASE("KmlDocument large document", "[editor][kml_document]") {
    KmlDocument doc;

    // Add 100 paragraphs
    for (int i = 0; i < 100; ++i) {
        doc.addParagraph(makePara(QString("Paragraph %1").arg(i)));
    }

    REQUIRE(doc.paragraphCount() == 100);
    REQUIRE(doc.paragraph(50)->plainText() == "Paragraph 50");
    REQUIRE(doc.paragraph(99)->plainText() == "Paragraph 99");

    // Remove from middle
    doc.removeParagraph(50);
    REQUIRE(doc.paragraphCount() == 99);
    REQUIRE(doc.paragraph(50)->plainText() == "Paragraph 51");
}

TEST_CASE("KmlDocument typical novel structure", "[editor][kml_document]") {
    KmlDocument doc;

    // Chapter heading
    doc.addParagraph(makePara("Chapter 1: The Beginning", "heading1"));

    // Body paragraphs
    doc.addParagraph(makePara("It was a dark and stormy night."));
    doc.addParagraph(makePara("The old house creaked in the wind."));

    // Scene break (empty paragraph with style)
    auto sceneBreak = std::make_unique<KmlParagraph>("* * *");
    sceneBreak->setStyleId("scene-break");
    doc.addParagraph(std::move(sceneBreak));

    // More content
    doc.addParagraph(makePara("The next morning dawned bright and clear."));

    SECTION("Structure is preserved") {
        REQUIRE(doc.paragraphCount() == 5);
        REQUIRE(doc.paragraph(0)->styleId() == "heading1");
        REQUIRE(doc.paragraph(3)->styleId() == "scene-break");
    }

    SECTION("KML serialization") {
        QString kml = doc.toKml();
        REQUIRE(kml.contains("<p style=\"heading1\">"));
        REQUIRE(kml.contains("<p style=\"scene-break\">"));
    }
}

// =============================================================================
// Paragraph Order Tests
// =============================================================================

TEST_CASE("KmlDocument paragraph ordering", "[editor][kml_document]") {
    KmlDocument doc;

    // Add paragraphs in order: 1, 3, 5
    doc.addParagraph(makePara("1"));
    doc.addParagraph(makePara("3"));
    doc.addParagraph(makePara("5"));

    // Insert 2 and 4
    doc.insertParagraph(1, makePara("2"));
    doc.insertParagraph(3, makePara("4"));

    // Verify order
    REQUIRE(doc.paragraphCount() == 5);
    REQUIRE(doc.paragraph(0)->plainText() == "1");
    REQUIRE(doc.paragraph(1)->plainText() == "2");
    REQUIRE(doc.paragraph(2)->plainText() == "3");
    REQUIRE(doc.paragraph(3)->plainText() == "4");
    REQUIRE(doc.paragraph(4)->plainText() == "5");

    // plainText shows correct order
    REQUIRE(doc.plainText() == "1\n2\n3\n4\n5");
}

// =============================================================================
// Text Operations Tests (Phase 1.9)
// =============================================================================

TEST_CASE("KmlDocument insertText", "[editor][kml_document][text_ops]") {
    KmlDocument doc;

    SECTION("Insert into single paragraph") {
        doc.addParagraph(makePara("Hello World"));
        doc.resetModified();

        CursorPosition pos{0, 5};
        bool success = doc.insertText(pos, ",");

        REQUIRE(success);
        REQUIRE(doc.paragraph(0)->plainText() == "Hello, World");
        REQUIRE(doc.isModified());
    }

    SECTION("Insert at beginning") {
        doc.addParagraph(makePara("World"));
        doc.resetModified();

        CursorPosition pos{0, 0};
        bool success = doc.insertText(pos, "Hello ");

        REQUIRE(success);
        REQUIRE(doc.paragraph(0)->plainText() == "Hello World");
    }

    SECTION("Insert at end") {
        doc.addParagraph(makePara("Hello"));
        doc.resetModified();

        CursorPosition pos{0, 5};
        bool success = doc.insertText(pos, " World");

        REQUIRE(success);
        REQUIRE(doc.paragraph(0)->plainText() == "Hello World");
    }

    SECTION("Insert into second paragraph") {
        doc.addParagraph(makePara("First"));
        doc.addParagraph(makePara("Second"));
        doc.resetModified();

        CursorPosition pos{1, 6};
        bool success = doc.insertText(pos, " line");

        REQUIRE(success);
        REQUIRE(doc.paragraph(1)->plainText() == "Second line");
    }

    SECTION("Insert empty text succeeds") {
        doc.addParagraph(makePara("Test"));

        CursorPosition pos{0, 2};
        bool success = doc.insertText(pos, "");

        REQUIRE(success);
        REQUIRE(doc.paragraph(0)->plainText() == "Test");
    }

    SECTION("Insert with invalid paragraph index fails") {
        doc.addParagraph(makePara("Test"));

        CursorPosition invalidPos{5, 0};
        bool success = doc.insertText(invalidPos, "fail");

        REQUIRE(success == false);
    }

    SECTION("Insert with negative paragraph index fails") {
        doc.addParagraph(makePara("Test"));

        CursorPosition invalidPos{-1, 0};
        bool success = doc.insertText(invalidPos, "fail");

        REQUIRE(success == false);
    }
}

TEST_CASE("KmlDocument insertText observer notifications", "[editor][kml_document][text_ops][observer]") {
    KmlDocument doc;
    doc.addParagraph(makePara("Hello"));

    TestObserver observer;
    doc.addObserver(&observer);

    CursorPosition pos{0, 5};
    doc.insertText(pos, " World");

    REQUIRE(observer.contentChangedCount == 1);
    REQUIRE(observer.paragraphModifiedCount == 1);
    REQUIRE(observer.lastModifiedIndex == 0);
}

TEST_CASE("KmlDocument deleteText single paragraph", "[editor][kml_document][text_ops]") {
    KmlDocument doc;

    SECTION("Delete middle characters") {
        doc.addParagraph(makePara("Hello World"));
        doc.resetModified();

        CursorPosition start{0, 5};
        CursorPosition end{0, 11};
        bool success = doc.deleteText(start, end);

        REQUIRE(success);
        REQUIRE(doc.paragraph(0)->plainText() == "Hello");
        REQUIRE(doc.isModified());
    }

    SECTION("Delete from beginning") {
        doc.addParagraph(makePara("Hello World"));

        CursorPosition start{0, 0};
        CursorPosition end{0, 6};
        bool success = doc.deleteText(start, end);

        REQUIRE(success);
        REQUIRE(doc.paragraph(0)->plainText() == "World");
    }

    SECTION("Delete to end") {
        doc.addParagraph(makePara("Hello World"));

        CursorPosition start{0, 5};
        CursorPosition end{0, 11};
        bool success = doc.deleteText(start, end);

        REQUIRE(success);
        REQUIRE(doc.paragraph(0)->plainText() == "Hello");
    }

    SECTION("Delete empty range succeeds") {
        doc.addParagraph(makePara("Hello"));

        CursorPosition pos{0, 2};
        bool success = doc.deleteText(pos, pos);

        REQUIRE(success);
        REQUIRE(doc.paragraph(0)->plainText() == "Hello");
    }

    SECTION("Delete with reversed range normalizes") {
        doc.addParagraph(makePara("Hello World"));

        CursorPosition start{0, 11};
        CursorPosition end{0, 5};
        bool success = doc.deleteText(start, end);  // Reversed

        REQUIRE(success);
        REQUIRE(doc.paragraph(0)->plainText() == "Hello");
    }
}

TEST_CASE("KmlDocument deleteText multi-paragraph", "[editor][kml_document][text_ops]") {
    SECTION("Delete across two paragraphs") {
        KmlDocument doc;
        doc.addParagraph(makePara("First line"));
        doc.addParagraph(makePara("Second line"));
        doc.resetModified();

        CursorPosition start{0, 5};  // After "First"
        CursorPosition end{1, 7};    // After "Second "

        bool success = doc.deleteText(start, end);

        REQUIRE(success);
        REQUIRE(doc.paragraphCount() == 1);
        REQUIRE(doc.paragraph(0)->plainText() == "Firstline");
        REQUIRE(doc.isModified());
    }

    SECTION("Delete entire middle paragraph") {
        KmlDocument doc;
        doc.addParagraph(makePara("First"));
        doc.addParagraph(makePara("Middle"));
        doc.addParagraph(makePara("Last"));
        doc.resetModified();

        CursorPosition start{0, 5};  // End of first
        CursorPosition end{2, 0};    // Start of last

        bool success = doc.deleteText(start, end);

        REQUIRE(success);
        REQUIRE(doc.paragraphCount() == 1);
        REQUIRE(doc.paragraph(0)->plainText() == "FirstLast");
    }

    SECTION("Delete from middle of first to middle of last") {
        KmlDocument doc;
        doc.addParagraph(makePara("AAABBB"));
        doc.addParagraph(makePara("CCCDDD"));
        doc.addParagraph(makePara("EEEFFF"));

        CursorPosition start{0, 3};  // After "AAA"
        CursorPosition end{2, 3};    // After "EEE"

        bool success = doc.deleteText(start, end);

        REQUIRE(success);
        REQUIRE(doc.paragraphCount() == 1);
        REQUIRE(doc.paragraph(0)->plainText() == "AAAFFF");
    }
}

TEST_CASE("KmlDocument deleteText invalid ranges", "[editor][kml_document][text_ops]") {
    KmlDocument doc;
    doc.addParagraph(makePara("Test"));

    SECTION("Invalid start paragraph") {
        CursorPosition start{-1, 0};
        CursorPosition end{0, 4};
        REQUIRE(doc.deleteText(start, end) == false);
    }

    SECTION("Invalid end paragraph") {
        CursorPosition start{0, 0};
        CursorPosition end{5, 0};
        REQUIRE(doc.deleteText(start, end) == false);
    }
}

TEST_CASE("KmlDocument applyStyle", "[editor][kml_document][text_ops]") {
    SECTION("Apply style to single paragraph") {
        KmlDocument doc;
        doc.addParagraph(makePara("Heading text"));
        doc.resetModified();

        SelectionRange range{{0, 0}, {0, 12}};
        bool success = doc.applyStyle(range, "heading1");

        REQUIRE(success);
        REQUIRE(doc.paragraph(0)->styleId() == "heading1");
        REQUIRE(doc.isModified());
    }

    SECTION("Apply style to multiple paragraphs") {
        KmlDocument doc;
        doc.addParagraph(makePara("Line 1"));
        doc.addParagraph(makePara("Line 2"));
        doc.addParagraph(makePara("Line 3"));
        doc.resetModified();

        SelectionRange range{{0, 0}, {2, 6}};
        bool success = doc.applyStyle(range, "quote");

        REQUIRE(success);
        REQUIRE(doc.paragraph(0)->styleId() == "quote");
        REQUIRE(doc.paragraph(1)->styleId() == "quote");
        REQUIRE(doc.paragraph(2)->styleId() == "quote");
    }

    SECTION("Apply style to partial selection affects whole paragraphs") {
        KmlDocument doc;
        doc.addParagraph(makePara("First paragraph text"));
        doc.addParagraph(makePara("Second paragraph text"));
        doc.resetModified();

        // Select from middle of first to middle of second
        SelectionRange range{{0, 6}, {1, 6}};
        bool success = doc.applyStyle(range, "emphasis");

        REQUIRE(success);
        REQUIRE(doc.paragraph(0)->styleId() == "emphasis");
        REQUIRE(doc.paragraph(1)->styleId() == "emphasis");
    }

    SECTION("Apply empty style removes styling") {
        KmlDocument doc;
        doc.addParagraph(makePara("Styled", "heading1"));

        SelectionRange range{{0, 0}, {0, 6}};
        bool success = doc.applyStyle(range, "");

        REQUIRE(success);
        REQUIRE(doc.paragraph(0)->styleId() == "");
        REQUIRE(doc.paragraph(0)->hasStyle() == false);
    }

    SECTION("Invalid paragraph range fails") {
        KmlDocument doc;
        doc.addParagraph(makePara("Test"));

        SelectionRange range{{0, 0}, {5, 0}};
        bool success = doc.applyStyle(range, "style");

        REQUIRE(success == false);
    }
}

TEST_CASE("KmlDocument splitParagraph", "[editor][kml_document][text_ops]") {
    SECTION("Split in middle of paragraph") {
        KmlDocument doc;
        doc.addParagraph(makePara("HelloWorld"));
        doc.resetModified();

        CursorPosition pos{0, 5};
        bool success = doc.splitParagraph(pos);

        REQUIRE(success);
        REQUIRE(doc.paragraphCount() == 2);
        REQUIRE(doc.paragraph(0)->plainText() == "Hello");
        REQUIRE(doc.paragraph(1)->plainText() == "World");
        REQUIRE(doc.isModified());
    }

    SECTION("Split at beginning creates empty paragraph before") {
        KmlDocument doc;
        doc.addParagraph(makePara("Content"));
        doc.resetModified();

        CursorPosition pos{0, 0};
        bool success = doc.splitParagraph(pos);

        REQUIRE(success);
        REQUIRE(doc.paragraphCount() == 2);
        REQUIRE(doc.paragraph(0)->plainText() == "");
        REQUIRE(doc.paragraph(1)->plainText() == "Content");
    }

    SECTION("Split at end creates empty paragraph after") {
        KmlDocument doc;
        doc.addParagraph(makePara("Content"));
        doc.resetModified();

        CursorPosition pos{0, 7};
        bool success = doc.splitParagraph(pos);

        REQUIRE(success);
        REQUIRE(doc.paragraphCount() == 2);
        REQUIRE(doc.paragraph(0)->plainText() == "Content");
        REQUIRE(doc.paragraph(1)->plainText() == "");
    }

    SECTION("Split preserves style") {
        KmlDocument doc;
        doc.addParagraph(makePara("HeadingText", "heading1"));
        doc.resetModified();

        CursorPosition pos{0, 7};
        bool success = doc.splitParagraph(pos);

        REQUIRE(success);
        REQUIRE(doc.paragraph(0)->styleId() == "heading1");
        REQUIRE(doc.paragraph(1)->styleId() == "heading1");
    }

    SECTION("Split second paragraph") {
        KmlDocument doc;
        doc.addParagraph(makePara("First"));
        doc.addParagraph(makePara("SecondThird"));
        doc.resetModified();

        CursorPosition pos{1, 6};
        bool success = doc.splitParagraph(pos);

        REQUIRE(success);
        REQUIRE(doc.paragraphCount() == 3);
        REQUIRE(doc.paragraph(0)->plainText() == "First");
        REQUIRE(doc.paragraph(1)->plainText() == "Second");
        REQUIRE(doc.paragraph(2)->plainText() == "Third");
    }

    SECTION("Invalid paragraph index fails") {
        KmlDocument doc;
        doc.addParagraph(makePara("Test"));

        CursorPosition pos{5, 0};
        bool success = doc.splitParagraph(pos);

        REQUIRE(success == false);
    }
}

TEST_CASE("KmlDocument splitParagraph observer notifications", "[editor][kml_document][text_ops][observer]") {
    KmlDocument doc;
    doc.addParagraph(makePara("HelloWorld"));

    TestObserver observer;
    doc.addObserver(&observer);

    CursorPosition pos{0, 5};
    doc.splitParagraph(pos);

    REQUIRE(observer.contentChangedCount == 1);
    REQUIRE(observer.paragraphInsertedCount == 1);
    REQUIRE(observer.lastInsertedIndex == 1);
}

TEST_CASE("KmlDocument mergeParagraphWithPrevious", "[editor][kml_document][text_ops]") {
    SECTION("Merge second into first") {
        KmlDocument doc;
        doc.addParagraph(makePara("Hello "));
        doc.addParagraph(makePara("World"));
        doc.resetModified();

        int cursorOffset = doc.mergeParagraphWithPrevious(1);

        REQUIRE(cursorOffset == 6);  // "Hello " has 6 characters
        REQUIRE(doc.paragraphCount() == 1);
        REQUIRE(doc.paragraph(0)->plainText() == "Hello World");
        REQUIRE(doc.isModified());
    }

    SECTION("Merge third into second") {
        KmlDocument doc;
        doc.addParagraph(makePara("First"));
        doc.addParagraph(makePara("Second"));
        doc.addParagraph(makePara("Third"));
        doc.resetModified();

        int cursorOffset = doc.mergeParagraphWithPrevious(2);

        REQUIRE(cursorOffset == 6);  // "Second" has 6 characters
        REQUIRE(doc.paragraphCount() == 2);
        REQUIRE(doc.paragraph(0)->plainText() == "First");
        REQUIRE(doc.paragraph(1)->plainText() == "SecondThird");
    }

    SECTION("Merge empty paragraph") {
        KmlDocument doc;
        doc.addParagraph(makePara("Content"));
        doc.addParagraph(makePara(""));

        int cursorOffset = doc.mergeParagraphWithPrevious(1);

        REQUIRE(cursorOffset == 7);  // "Content" has 7 characters
        REQUIRE(doc.paragraphCount() == 1);
        REQUIRE(doc.paragraph(0)->plainText() == "Content");
    }

    SECTION("Merge into empty paragraph") {
        KmlDocument doc;
        doc.addParagraph(makePara(""));
        doc.addParagraph(makePara("Content"));

        int cursorOffset = doc.mergeParagraphWithPrevious(1);

        REQUIRE(cursorOffset == 0);  // Empty paragraph has 0 characters
        REQUIRE(doc.paragraphCount() == 1);
        REQUIRE(doc.paragraph(0)->plainText() == "Content");
    }

    SECTION("Cannot merge first paragraph") {
        KmlDocument doc;
        doc.addParagraph(makePara("First"));
        doc.addParagraph(makePara("Second"));

        int cursorOffset = doc.mergeParagraphWithPrevious(0);

        REQUIRE(cursorOffset == -1);
        REQUIRE(doc.paragraphCount() == 2);
    }

    SECTION("Invalid index returns -1") {
        KmlDocument doc;
        doc.addParagraph(makePara("Test"));

        REQUIRE(doc.mergeParagraphWithPrevious(-1) == -1);
        REQUIRE(doc.mergeParagraphWithPrevious(5) == -1);
    }
}

TEST_CASE("KmlDocument mergeParagraphWithPrevious observer notifications", "[editor][kml_document][text_ops][observer]") {
    KmlDocument doc;
    doc.addParagraph(makePara("Hello "));
    doc.addParagraph(makePara("World"));

    TestObserver observer;
    doc.addObserver(&observer);

    doc.mergeParagraphWithPrevious(1);

    REQUIRE(observer.contentChangedCount == 1);
    REQUIRE(observer.paragraphModifiedCount == 1);
    REQUIRE(observer.lastModifiedIndex == 0);
    REQUIRE(observer.paragraphRemovedCount == 1);
    REQUIRE(observer.lastRemovedIndex == 1);
}

TEST_CASE("KmlDocument Enter and Backspace workflow", "[editor][kml_document][text_ops]") {
    SECTION("Enter at end, then Backspace") {
        KmlDocument doc;
        doc.addParagraph(makePara("Line one"));

        // Press Enter at end of line
        CursorPosition enterPos{0, 8};
        doc.splitParagraph(enterPos);

        REQUIRE(doc.paragraphCount() == 2);
        REQUIRE(doc.paragraph(0)->plainText() == "Line one");
        REQUIRE(doc.paragraph(1)->plainText() == "");

        // Type some text in new line
        CursorPosition insertPos{1, 0};
        doc.insertText(insertPos, "Line two");

        REQUIRE(doc.paragraph(1)->plainText() == "Line two");

        // Press Backspace at start of second line (merge with first)
        int cursorOffset = doc.mergeParagraphWithPrevious(1);

        REQUIRE(cursorOffset == 8);
        REQUIRE(doc.paragraphCount() == 1);
        REQUIRE(doc.paragraph(0)->plainText() == "Line oneLine two");
    }

    SECTION("Enter in middle of text") {
        KmlDocument doc;
        doc.addParagraph(makePara("Hello World"));

        // Press Enter after "Hello"
        CursorPosition enterPos{0, 5};
        doc.splitParagraph(enterPos);

        REQUIRE(doc.paragraphCount() == 2);
        REQUIRE(doc.paragraph(0)->plainText() == "Hello");
        REQUIRE(doc.paragraph(1)->plainText() == " World");

        // Press Backspace at start of second line
        int cursorOffset = doc.mergeParagraphWithPrevious(1);

        REQUIRE(cursorOffset == 5);
        REQUIRE(doc.paragraphCount() == 1);
        REQUIRE(doc.paragraph(0)->plainText() == "Hello World");
    }
}

TEST_CASE("KmlDocument multi-paragraph selection delete", "[editor][kml_document][text_ops]") {
    SECTION("Select all and delete") {
        KmlDocument doc;
        doc.addParagraph(makePara("First paragraph"));
        doc.addParagraph(makePara("Second paragraph"));
        doc.addParagraph(makePara("Third paragraph"));

        CursorPosition start{0, 0};
        CursorPosition end{2, 15};

        bool success = doc.deleteText(start, end);

        REQUIRE(success);
        REQUIRE(doc.paragraphCount() == 1);
        REQUIRE(doc.paragraph(0)->plainText() == "");
    }

    SECTION("Delete leaves content on both ends") {
        KmlDocument doc;
        doc.addParagraph(makePara("AAA_BBB"));
        doc.addParagraph(makePara("CCC_DDD"));
        doc.addParagraph(makePara("EEE_FFF"));

        // Delete from after AAA to before FFF
        CursorPosition start{0, 4};  // After "AAA_"
        CursorPosition end{2, 4};    // After "EEE_"

        bool success = doc.deleteText(start, end);

        REQUIRE(success);
        REQUIRE(doc.paragraphCount() == 1);
        REQUIRE(doc.paragraph(0)->plainText() == "AAA_FFF");
    }
}
