/// @file test_kml_paragraph.cpp
/// @brief Unit tests for KML Paragraph element (OpenSpec #00042 Phase 1.6)

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/kml_paragraph.h>
#include <kalahari/editor/kml_text_run.h>
#include <kalahari/editor/kml_inline_elements.h>
#include <memory>

using namespace kalahari::editor;

// =============================================================================
// Helper function to add text to paragraph
// =============================================================================

void addText(KmlParagraph& para, const QString& text) {
    para.addElement(std::make_unique<KmlTextRun>(text));
}

// =============================================================================
// Constructor Tests
// =============================================================================

TEST_CASE("KmlParagraph default constructor", "[editor][kml_paragraph]") {
    KmlParagraph para;

    SECTION("Empty state") {
        REQUIRE(para.isEmpty());
        REQUIRE(para.length() == 0);
        REQUIRE(para.elementCount() == 0);
        REQUIRE(para.plainText().isEmpty());
    }

    SECTION("No style by default") {
        REQUIRE(para.styleId().isEmpty());
        REQUIRE(para.hasStyle() == false);
    }
}

TEST_CASE("KmlParagraph constructor with text", "[editor][kml_paragraph]") {
    KmlParagraph para("Hello, world!");

    SECTION("Has content") {
        REQUIRE(para.isEmpty() == false);
        REQUIRE(para.length() == 13);
        REQUIRE(para.elementCount() == 1);
        REQUIRE(para.plainText() == "Hello, world!");
    }

    SECTION("No style") {
        REQUIRE(para.hasStyle() == false);
    }
}

TEST_CASE("KmlParagraph constructor with empty text", "[editor][kml_paragraph]") {
    KmlParagraph para("");

    SECTION("Empty paragraph") {
        REQUIRE(para.isEmpty());
        REQUIRE(para.elementCount() == 0);
    }
}

TEST_CASE("KmlParagraph constructor with text and style", "[editor][kml_paragraph]") {
    KmlParagraph para("Chapter One", "heading1");

    SECTION("Has content") {
        REQUIRE(para.plainText() == "Chapter One");
        REQUIRE(para.length() == 11);
    }

    SECTION("Has style") {
        REQUIRE(para.hasStyle());
        REQUIRE(para.styleId() == "heading1");
    }
}

// =============================================================================
// Element Container Tests
// =============================================================================

TEST_CASE("KmlParagraph addElement", "[editor][kml_paragraph]") {
    KmlParagraph para;

    SECTION("Add single element") {
        para.addElement(std::make_unique<KmlTextRun>("Hello"));

        REQUIRE(para.elementCount() == 1);
        REQUIRE(para.plainText() == "Hello");
    }

    SECTION("Add multiple elements") {
        addText(para, "Hello ");
        addText(para, "World");

        REQUIRE(para.elementCount() == 2);
        REQUIRE(para.plainText() == "Hello World");
    }

    SECTION("Ignore nullptr") {
        para.addElement(nullptr);
        REQUIRE(para.elementCount() == 0);
    }
}

TEST_CASE("KmlParagraph insertElement", "[editor][kml_paragraph]") {
    KmlParagraph para;
    addText(para, "First");
    addText(para, "Third");

    SECTION("Insert in middle") {
        para.insertElement(1, std::make_unique<KmlTextRun>("Second"));

        REQUIRE(para.elementCount() == 3);
        REQUIRE(para.elementAt(0)->plainText() == "First");
        REQUIRE(para.elementAt(1)->plainText() == "Second");
        REQUIRE(para.elementAt(2)->plainText() == "Third");
    }

    SECTION("Insert at beginning") {
        para.insertElement(0, std::make_unique<KmlTextRun>("Zero"));

        REQUIRE(para.elementCount() == 3);
        REQUIRE(para.elementAt(0)->plainText() == "Zero");
    }

    SECTION("Insert at end (beyond size)") {
        para.insertElement(100, std::make_unique<KmlTextRun>("End"));

        REQUIRE(para.elementCount() == 3);
        REQUIRE(para.elementAt(2)->plainText() == "End");
    }

    SECTION("Insert with negative index") {
        para.insertElement(-5, std::make_unique<KmlTextRun>("Negative"));

        REQUIRE(para.elementCount() == 3);
        REQUIRE(para.elementAt(0)->plainText() == "Negative");
    }

    SECTION("Ignore nullptr") {
        para.insertElement(1, nullptr);
        REQUIRE(para.elementCount() == 2);
    }
}

TEST_CASE("KmlParagraph removeElement", "[editor][kml_paragraph]") {
    KmlParagraph para;
    addText(para, "First");
    addText(para, "Second");
    addText(para, "Third");

    SECTION("Remove middle element") {
        auto removed = para.removeElement(1);

        REQUIRE(removed != nullptr);
        REQUIRE(removed->plainText() == "Second");
        REQUIRE(para.elementCount() == 2);
        REQUIRE(para.plainText() == "FirstThird");
    }

    SECTION("Remove first element") {
        auto removed = para.removeElement(0);

        REQUIRE(removed != nullptr);
        REQUIRE(removed->plainText() == "First");
        REQUIRE(para.elementCount() == 2);
    }

    SECTION("Remove last element") {
        auto removed = para.removeElement(2);

        REQUIRE(removed != nullptr);
        REQUIRE(removed->plainText() == "Third");
        REQUIRE(para.elementCount() == 2);
    }

    SECTION("Remove invalid index returns nullptr") {
        auto removed = para.removeElement(100);
        REQUIRE(removed == nullptr);
        REQUIRE(para.elementCount() == 3);

        removed = para.removeElement(-1);
        REQUIRE(removed == nullptr);
    }
}

TEST_CASE("KmlParagraph clearElements", "[editor][kml_paragraph]") {
    KmlParagraph para;
    addText(para, "First");
    addText(para, "Second");
    addText(para, "Third");

    REQUIRE(para.elementCount() == 3);

    para.clearElements();

    REQUIRE(para.elementCount() == 0);
    REQUIRE(para.isEmpty());
}

TEST_CASE("KmlParagraph elementAt", "[editor][kml_paragraph]") {
    KmlParagraph para;
    addText(para, "Hello");

    SECTION("Valid index") {
        auto* element = para.elementAt(0);
        REQUIRE(element != nullptr);
        REQUIRE(element->plainText() == "Hello");
    }

    SECTION("Invalid indices return nullptr") {
        REQUIRE(para.elementAt(-1) == nullptr);
        REQUIRE(para.elementAt(1) == nullptr);
        REQUIRE(para.elementAt(100) == nullptr);
    }

    SECTION("Const access") {
        const KmlParagraph& constPara = para;
        const auto* element = constPara.elementAt(0);
        REQUIRE(element != nullptr);
        REQUIRE(element->plainText() == "Hello");
    }
}

TEST_CASE("KmlParagraph elements() access", "[editor][kml_paragraph]") {
    KmlParagraph para;
    addText(para, "A");
    addText(para, "B");

    const auto& elements = para.elements();

    REQUIRE(elements.size() == 2);
    REQUIRE(elements[0]->plainText() == "A");
    REQUIRE(elements[1]->plainText() == "B");
}

// =============================================================================
// Style Tests
// =============================================================================

TEST_CASE("KmlParagraph style management", "[editor][kml_paragraph]") {
    KmlParagraph para;

    SECTION("No style initially") {
        REQUIRE(para.styleId().isEmpty());
        REQUIRE(para.hasStyle() == false);
    }

    SECTION("Set style") {
        para.setStyleId("quote");

        REQUIRE(para.styleId() == "quote");
        REQUIRE(para.hasStyle() == true);
    }

    SECTION("Clear style") {
        para.setStyleId("quote");
        para.setStyleId("");

        REQUIRE(para.styleId().isEmpty());
        REQUIRE(para.hasStyle() == false);
    }
}

// =============================================================================
// Content Tests
// =============================================================================

TEST_CASE("KmlParagraph plainText extraction", "[editor][kml_paragraph]") {
    KmlParagraph para;

    SECTION("Empty paragraph") {
        REQUIRE(para.plainText().isEmpty());
    }

    SECTION("Single text run") {
        addText(para, "Hello");
        REQUIRE(para.plainText() == "Hello");
    }

    SECTION("Multiple text runs") {
        addText(para, "Hello ");
        addText(para, "beautiful ");
        addText(para, "world!");
        REQUIRE(para.plainText() == "Hello beautiful world!");
    }

    SECTION("With inline formatting") {
        addText(para, "Normal ");

        auto bold = std::make_unique<KmlBold>();
        bold->appendChild(std::make_unique<KmlTextRun>("bold"));
        para.addElement(std::move(bold));

        addText(para, " text");

        REQUIRE(para.plainText() == "Normal bold text");
    }
}

TEST_CASE("KmlParagraph length calculation", "[editor][kml_paragraph]") {
    KmlParagraph para;

    SECTION("Empty paragraph") {
        REQUIRE(para.length() == 0);
    }

    SECTION("With content") {
        addText(para, "Hello");
        REQUIRE(para.length() == 5);

        addText(para, " World");
        REQUIRE(para.length() == 11);
    }

    SECTION("With nested elements") {
        auto bold = std::make_unique<KmlBold>();
        bold->appendChild(std::make_unique<KmlTextRun>("Bold"));
        para.addElement(std::move(bold));

        REQUIRE(para.length() == 4);
    }
}

TEST_CASE("KmlParagraph isEmpty", "[editor][kml_paragraph]") {
    SECTION("Empty paragraph is empty") {
        KmlParagraph para;
        REQUIRE(para.isEmpty());
    }

    SECTION("Paragraph with content is not empty") {
        KmlParagraph para("Content");
        REQUIRE(para.isEmpty() == false);
    }

    SECTION("Paragraph with empty text run is empty") {
        KmlParagraph para;
        para.addElement(std::make_unique<KmlTextRun>(""));
        REQUIRE(para.isEmpty());
    }
}

// =============================================================================
// Serialization Tests
// =============================================================================

TEST_CASE("KmlParagraph toKml", "[editor][kml_paragraph]") {
    SECTION("Empty paragraph") {
        KmlParagraph para;
        REQUIRE(para.toKml() == "<p></p>");
    }

    SECTION("Empty paragraph with style") {
        KmlParagraph para;
        para.setStyleId("heading1");
        REQUIRE(para.toKml() == "<p style=\"heading1\"></p>");
    }

    SECTION("Paragraph with text") {
        KmlParagraph para("Hello, world!");
        QString kml = para.toKml();

        REQUIRE(kml.contains("<p>"));
        REQUIRE(kml.contains("</p>"));
        REQUIRE(kml.contains("Hello, world!"));
    }

    SECTION("Paragraph with style and text") {
        KmlParagraph para("Chapter One", "heading1");
        QString kml = para.toKml();

        REQUIRE(kml.contains("<p style=\"heading1\">"));
        REQUIRE(kml.contains("Chapter One"));
        REQUIRE(kml.contains("</p>"));
    }

    SECTION("Paragraph with inline formatting") {
        KmlParagraph para;
        addText(para, "Normal ");

        auto bold = std::make_unique<KmlBold>();
        bold->appendChild(std::make_unique<KmlTextRun>("bold"));
        para.addElement(std::move(bold));

        addText(para, " text");

        QString kml = para.toKml();
        REQUIRE(kml.startsWith("<p>"));
        REQUIRE(kml.endsWith("</p>"));
        REQUIRE(kml.contains("<b>"));
        REQUIRE(kml.contains("bold"));
        REQUIRE(kml.contains("</b>"));
    }

    SECTION("Paragraph with nested inline formatting") {
        KmlParagraph para;

        auto bold = std::make_unique<KmlBold>();
        auto italic = std::make_unique<KmlItalic>();
        italic->appendChild(std::make_unique<KmlTextRun>("bold italic"));
        bold->appendChild(std::move(italic));
        para.addElement(std::move(bold));

        QString kml = para.toKml();
        REQUIRE(kml.contains("<b>"));
        REQUIRE(kml.contains("<i>"));
        REQUIRE(kml.contains("</i>"));
        REQUIRE(kml.contains("</b>"));
    }
}

// =============================================================================
// Clone Tests
// =============================================================================

TEST_CASE("KmlParagraph clone", "[editor][kml_paragraph]") {
    SECTION("Clone empty paragraph") {
        KmlParagraph original;
        auto cloned = original.clone();

        REQUIRE(cloned != nullptr);
        REQUIRE(cloned->isEmpty());
    }

    SECTION("Clone paragraph with content") {
        KmlParagraph original("Hello, world!");
        auto cloned = original.clone();

        REQUIRE(cloned != nullptr);
        REQUIRE(cloned->plainText() == "Hello, world!");
    }

    SECTION("Clone paragraph with style") {
        KmlParagraph original("Chapter One", "heading1");
        auto cloned = original.clone();

        REQUIRE(cloned->styleId() == "heading1");
        REQUIRE(cloned->plainText() == "Chapter One");
    }

    SECTION("Clone is independent") {
        KmlParagraph original("Original");
        auto cloned = original.clone();

        original.clearElements();
        addText(original, "Modified");

        REQUIRE(cloned->plainText() == "Original");
        REQUIRE(original.plainText() == "Modified");
    }

    SECTION("Clone with inline formatting") {
        KmlParagraph original;
        auto bold = std::make_unique<KmlBold>();
        bold->appendChild(std::make_unique<KmlTextRun>("Bold text"));
        original.addElement(std::move(bold));

        auto cloned = original.clone();

        REQUIRE(cloned->plainText() == "Bold text");
        REQUIRE(cloned->elementCount() == 1);
        REQUIRE(cloned->elementAt(0)->type() == ElementType::Bold);
    }
}

// =============================================================================
// Copy/Move Constructor Tests
// =============================================================================

TEST_CASE("KmlParagraph copy constructor", "[editor][kml_paragraph]") {
    KmlParagraph original("Copy me", "quote");

    KmlParagraph copy(original);

    REQUIRE(copy.plainText() == "Copy me");
    REQUIRE(copy.styleId() == "quote");

    // Verify independence
    original.clearElements();
    REQUIRE(copy.plainText() == "Copy me");
}

TEST_CASE("KmlParagraph move constructor", "[editor][kml_paragraph]") {
    KmlParagraph original("Move me", "heading1");

    KmlParagraph moved(std::move(original));

    REQUIRE(moved.plainText() == "Move me");
    REQUIRE(moved.styleId() == "heading1");
}

TEST_CASE("KmlParagraph copy assignment", "[editor][kml_paragraph]") {
    KmlParagraph original("Source", "style1");
    KmlParagraph target("Target", "style2");

    target = original;

    REQUIRE(target.plainText() == "Source");
    REQUIRE(target.styleId() == "style1");

    // Self-assignment
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
#endif
    target = target;  // Self-assignment test
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    REQUIRE(target.plainText() == "Source");
}

TEST_CASE("KmlParagraph move assignment", "[editor][kml_paragraph]") {
    KmlParagraph original("Moving");
    KmlParagraph target("Target");

    target = std::move(original);

    REQUIRE(target.plainText() == "Moving");
}

// =============================================================================
// Mixed Content Tests
// =============================================================================

TEST_CASE("KmlParagraph with complex mixed content", "[editor][kml_paragraph]") {
    KmlParagraph para;

    // Build: "Normal <b>bold <i>bold italic</i></b> normal <u>underlined</u> end"
    addText(para, "Normal ");

    auto bold = std::make_unique<KmlBold>();
    bold->appendChild(std::make_unique<KmlTextRun>("bold "));
    auto italic = std::make_unique<KmlItalic>();
    italic->appendChild(std::make_unique<KmlTextRun>("bold italic"));
    bold->appendChild(std::move(italic));
    para.addElement(std::move(bold));

    addText(para, " normal ");

    auto underline = std::make_unique<KmlUnderline>();
    underline->appendChild(std::make_unique<KmlTextRun>("underlined"));
    para.addElement(std::move(underline));

    addText(para, " end");

    SECTION("Plain text") {
        REQUIRE(para.plainText() == "Normal bold bold italic normal underlined end");
    }

    SECTION("Length") {
        REQUIRE(para.length() == 45);
    }

    SECTION("Element count") {
        REQUIRE(para.elementCount() == 5);
    }

    SECTION("Clone preserves structure") {
        auto cloned = para.clone();
        REQUIRE(cloned->plainText() == para.plainText());
        REQUIRE(cloned->elementCount() == para.elementCount());
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("KmlParagraph with whitespace", "[editor][kml_paragraph]") {
    KmlParagraph para("  leading and trailing  ");

    REQUIRE(para.plainText() == "  leading and trailing  ");
    REQUIRE(para.length() == 24);  // "  leading and trailing  " = 24 chars
}

TEST_CASE("KmlParagraph with newlines", "[editor][kml_paragraph]") {
    // Note: In block-level paragraphs, newlines in content are typically preserved
    KmlParagraph para("Line1\nLine2\nLine3");

    REQUIRE(para.plainText() == "Line1\nLine2\nLine3");
}

TEST_CASE("KmlParagraph with Unicode", "[editor][kml_paragraph]") {
    KmlParagraph para(QString::fromUtf8(u8"Zaz\u00F3\u0142\u0107 g\u0119\u015Bl\u0105 ja\u017A\u0144"));

    REQUIRE(para.plainText() == QString::fromUtf8(u8"Zaz\u00F3\u0142\u0107 g\u0119\u015Bl\u0105 ja\u017A\u0144"));
    REQUIRE(para.isEmpty() == false);
}

TEST_CASE("KmlParagraph with special characters in style", "[editor][kml_paragraph]") {
    // Style IDs should be simple identifiers, but test edge case
    KmlParagraph para;
    para.setStyleId("my-style_name123");

    REQUIRE(para.styleId() == "my-style_name123");
    QString kml = para.toKml();
    REQUIRE(kml.contains("style=\"my-style_name123\""));
}

// =============================================================================
// Common Paragraph Style Tests
// =============================================================================

TEST_CASE("KmlParagraph common styles", "[editor][kml_paragraph]") {
    SECTION("Normal paragraph") {
        KmlParagraph para("Regular text");
        REQUIRE(para.hasStyle() == false);
    }

    SECTION("Heading styles") {
        KmlParagraph h1("Chapter Title", "heading1");
        KmlParagraph h2("Section Title", "heading2");
        KmlParagraph h3("Subsection", "heading3");

        REQUIRE(h1.styleId() == "heading1");
        REQUIRE(h2.styleId() == "heading2");
        REQUIRE(h3.styleId() == "heading3");
    }

    SECTION("Quote style") {
        KmlParagraph quote("\"To be or not to be\"", "quote");
        REQUIRE(quote.styleId() == "quote");
    }

    SECTION("Poem style") {
        KmlParagraph verse("Roses are red", "verse");
        REQUIRE(verse.styleId() == "verse");
    }
}

// =============================================================================
// Integration with Inline Elements
// =============================================================================

TEST_CASE("KmlParagraph all inline element types", "[editor][kml_paragraph]") {
    KmlParagraph para;

    // Add all inline element types
    auto bold = std::make_unique<KmlBold>();
    bold->appendChild(std::make_unique<KmlTextRun>("B"));
    para.addElement(std::move(bold));

    auto italic = std::make_unique<KmlItalic>();
    italic->appendChild(std::make_unique<KmlTextRun>("I"));
    para.addElement(std::move(italic));

    auto underline = std::make_unique<KmlUnderline>();
    underline->appendChild(std::make_unique<KmlTextRun>("U"));
    para.addElement(std::move(underline));

    auto strike = std::make_unique<KmlStrikethrough>();
    strike->appendChild(std::make_unique<KmlTextRun>("S"));
    para.addElement(std::move(strike));

    auto sub = std::make_unique<KmlSubscript>();
    sub->appendChild(std::make_unique<KmlTextRun>("2"));
    para.addElement(std::move(sub));

    auto sup = std::make_unique<KmlSuperscript>();
    sup->appendChild(std::make_unique<KmlTextRun>("n"));
    para.addElement(std::move(sup));

    SECTION("Plain text concatenation") {
        REQUIRE(para.plainText() == "BIUS2n");
    }

    SECTION("Element count") {
        REQUIRE(para.elementCount() == 6);
    }

    SECTION("KML contains all tags") {
        QString kml = para.toKml();
        REQUIRE(kml.contains("<b>"));
        REQUIRE(kml.contains("<i>"));
        REQUIRE(kml.contains("<u>"));
        REQUIRE(kml.contains("<s>"));
        REQUIRE(kml.contains("<sub>"));
        REQUIRE(kml.contains("<sup>"));
    }
}

// =============================================================================
// Phase 1.7: Advanced Text Manipulation Tests
// =============================================================================

// -----------------------------------------------------------------------------
// characterCount() Tests
// -----------------------------------------------------------------------------

TEST_CASE("KmlParagraph characterCount", "[editor][kml_paragraph][phase1.7]") {
    SECTION("Empty paragraph") {
        KmlParagraph para;
        REQUIRE(para.characterCount() == 0);
        REQUIRE(para.characterCount() == para.length());
    }

    SECTION("Paragraph with content") {
        KmlParagraph para("Hello, world!");
        REQUIRE(para.characterCount() == 13);
        REQUIRE(para.characterCount() == para.length());
    }

    SECTION("Paragraph with multiple elements") {
        KmlParagraph para;
        addText(para, "Hello ");
        addText(para, "World");
        REQUIRE(para.characterCount() == 11);
    }
}

// -----------------------------------------------------------------------------
// insertText() Tests
// -----------------------------------------------------------------------------

TEST_CASE("KmlParagraph insertText basic", "[editor][kml_paragraph][phase1.7]") {
    SECTION("Insert into empty paragraph") {
        KmlParagraph para;
        REQUIRE(para.insertText(0, "Hello"));
        REQUIRE(para.plainText() == "Hello");
        REQUIRE(para.elementCount() == 1);
    }

    SECTION("Insert at beginning") {
        KmlParagraph para("World");
        REQUIRE(para.insertText(0, "Hello "));
        REQUIRE(para.plainText() == "Hello World");
    }

    SECTION("Insert at end") {
        KmlParagraph para("Hello");
        REQUIRE(para.insertText(5, " World"));
        REQUIRE(para.plainText() == "Hello World");
    }

    SECTION("Insert in middle") {
        KmlParagraph para("Helo World");
        REQUIRE(para.insertText(3, "l"));
        REQUIRE(para.plainText() == "Hello World");
    }

    SECTION("Insert empty text is no-op") {
        KmlParagraph para("Hello");
        REQUIRE(para.insertText(0, ""));
        REQUIRE(para.plainText() == "Hello");
    }
}

TEST_CASE("KmlParagraph insertText validation", "[editor][kml_paragraph][phase1.7]") {
    KmlParagraph para("Hello");

    SECTION("Negative offset fails") {
        REQUIRE(para.insertText(-1, "X") == false);
        REQUIRE(para.plainText() == "Hello");
    }

    SECTION("Offset beyond length fails") {
        REQUIRE(para.insertText(10, "X") == false);
        REQUIRE(para.plainText() == "Hello");
    }

    SECTION("Offset at length succeeds (append)") {
        REQUIRE(para.insertText(5, "!"));
        REQUIRE(para.plainText() == "Hello!");
    }
}

TEST_CASE("KmlParagraph insertText multiple elements", "[editor][kml_paragraph][phase1.7]") {
    SECTION("Insert into first element") {
        KmlParagraph para;
        addText(para, "Hello");
        addText(para, "World");

        REQUIRE(para.insertText(2, "XX"));
        REQUIRE(para.plainText() == "HeXXlloWorld");
    }

    SECTION("Insert into second element") {
        KmlParagraph para;
        addText(para, "Hello");
        addText(para, "World");

        REQUIRE(para.insertText(7, "YY"));
        REQUIRE(para.plainText() == "HelloWoYYrld");
    }

    SECTION("Insert at element boundary") {
        KmlParagraph para;
        addText(para, "Hello");
        addText(para, "World");

        REQUIRE(para.insertText(5, " "));
        REQUIRE(para.plainText() == "Hello World");
    }
}

TEST_CASE("KmlParagraph insertText with formatting", "[editor][kml_paragraph][phase1.7]") {
    KmlParagraph para;
    addText(para, "Normal ");

    auto bold = std::make_unique<KmlBold>();
    bold->appendChild(std::make_unique<KmlTextRun>("bold"));
    para.addElement(std::move(bold));

    addText(para, " text");

    SECTION("Insert before formatted element") {
        REQUIRE(para.insertText(7, "X"));
        // Should insert before or after the formatted element
        QString result = para.plainText();
        REQUIRE(result.contains("X"));
        REQUIRE(result.contains("bold"));
    }

    SECTION("Insert after formatted element") {
        REQUIRE(para.insertText(11, "Y"));
        QString result = para.plainText();
        REQUIRE(result.contains("Y"));
        REQUIRE(result.contains("bold"));
    }
}

// -----------------------------------------------------------------------------
// deleteText() Tests
// -----------------------------------------------------------------------------

TEST_CASE("KmlParagraph deleteText basic", "[editor][kml_paragraph][phase1.7]") {
    SECTION("Delete from beginning") {
        KmlParagraph para("Hello World");
        REQUIRE(para.deleteText(0, 6));
        REQUIRE(para.plainText() == "World");
    }

    SECTION("Delete from end") {
        KmlParagraph para("Hello World");
        REQUIRE(para.deleteText(5, 11));
        REQUIRE(para.plainText() == "Hello");
    }

    SECTION("Delete from middle") {
        KmlParagraph para("Hello World");
        REQUIRE(para.deleteText(5, 6));
        REQUIRE(para.plainText() == "HelloWorld");
    }

    SECTION("Delete entire content") {
        KmlParagraph para("Hello");
        REQUIRE(para.deleteText(0, 5));
        REQUIRE(para.plainText().isEmpty());
        REQUIRE(para.isEmpty());
    }

    SECTION("Delete empty range is no-op") {
        KmlParagraph para("Hello");
        REQUIRE(para.deleteText(2, 2));
        REQUIRE(para.plainText() == "Hello");
    }
}

TEST_CASE("KmlParagraph deleteText validation", "[editor][kml_paragraph][phase1.7]") {
    KmlParagraph para("Hello");

    SECTION("Negative start fails") {
        REQUIRE(para.deleteText(-1, 3) == false);
        REQUIRE(para.plainText() == "Hello");
    }

    SECTION("End beyond length fails") {
        REQUIRE(para.deleteText(0, 10) == false);
        REQUIRE(para.plainText() == "Hello");
    }

    SECTION("Reversed range is normalized") {
        REQUIRE(para.deleteText(3, 0));
        REQUIRE(para.plainText() == "lo");
    }
}

TEST_CASE("KmlParagraph deleteText multiple elements", "[editor][kml_paragraph][phase1.7]") {
    SECTION("Delete within first element") {
        KmlParagraph para;
        addText(para, "Hello");
        addText(para, "World");

        REQUIRE(para.deleteText(1, 4));
        REQUIRE(para.plainText() == "HoWorld");
    }

    SECTION("Delete within second element") {
        KmlParagraph para;
        addText(para, "Hello");
        addText(para, "World");
        // "HelloWorld" - positions: H=0,e=1,l=2,l=3,o=4,W=5,o=6,r=7,l=8,d=9
        // Delete 6-9 (exclusive) removes 'o','r','l' leaving "HelloWd"
        REQUIRE(para.deleteText(6, 9));
        REQUIRE(para.plainText() == "HelloWd");
    }

    SECTION("Delete across elements") {
        KmlParagraph para;
        addText(para, "Hello");
        addText(para, "World");

        REQUIRE(para.deleteText(3, 7));
        REQUIRE(para.plainText() == "Helrld");
    }

    SECTION("Delete entire element") {
        KmlParagraph para;
        addText(para, "Hello");
        addText(para, " ");
        addText(para, "World");

        REQUIRE(para.deleteText(5, 6));
        REQUIRE(para.plainText() == "HelloWorld");
        // The single-space element should be removed
        REQUIRE(para.elementCount() == 2);
    }
}

TEST_CASE("KmlParagraph deleteText with formatting", "[editor][kml_paragraph][phase1.7]") {
    KmlParagraph para;
    addText(para, "Normal ");  // 7 chars

    auto bold = std::make_unique<KmlBold>();
    bold->appendChild(std::make_unique<KmlTextRun>("bold"));  // 4 chars
    para.addElement(std::move(bold));

    addText(para, " text");  // 5 chars

    REQUIRE(para.plainText() == "Normal bold text");
    REQUIRE(para.length() == 16);

    SECTION("Delete entire formatted element") {
        REQUIRE(para.deleteText(7, 11));
        REQUIRE(para.plainText() == "Normal  text");
    }

    SECTION("Delete across formatted element") {
        // "Normal bold text" positions: N=0,o=1,r=2,m=3,a=4,l=5,' '=6,b=7,o=8,l=9,d=10,' '=11,t=12,e=13,x=14,t=15
        // Delete 5-13 (exclusive) removes 'l',' ','b','o','l','d',' ','t' leaving "Norma" + "ext"
        REQUIRE(para.deleteText(5, 13));
        REQUIRE(para.plainText() == "Normaext");
    }
}

// -----------------------------------------------------------------------------
// splitAt() Tests
// -----------------------------------------------------------------------------

TEST_CASE("KmlParagraph splitAt basic", "[editor][kml_paragraph][phase1.7]") {
    SECTION("Split in middle of text") {
        KmlParagraph para("Hello World");
        auto newPara = para.splitAt(6);

        REQUIRE(newPara != nullptr);
        REQUIRE(para.plainText() == "Hello ");
        REQUIRE(newPara->plainText() == "World");
    }

    SECTION("Split at beginning returns nullptr") {
        KmlParagraph para("Hello");
        auto newPara = para.splitAt(0);

        REQUIRE(newPara == nullptr);
        REQUIRE(para.plainText() == "Hello");
    }

    SECTION("Split at end creates empty paragraph") {
        KmlParagraph para("Hello");
        auto newPara = para.splitAt(5);

        REQUIRE(newPara != nullptr);
        REQUIRE(para.plainText() == "Hello");
        REQUIRE(newPara->plainText().isEmpty());
    }

    SECTION("Split beyond length returns nullptr") {
        KmlParagraph para("Hello");
        auto newPara = para.splitAt(10);

        REQUIRE(newPara == nullptr);
        REQUIRE(para.plainText() == "Hello");
    }

    SECTION("Split negative offset returns nullptr") {
        KmlParagraph para("Hello");
        auto newPara = para.splitAt(-1);

        REQUIRE(newPara == nullptr);
    }
}

TEST_CASE("KmlParagraph splitAt preserves style", "[editor][kml_paragraph][phase1.7]") {
    KmlParagraph para("Chapter One", "heading1");
    auto newPara = para.splitAt(8);

    REQUIRE(newPara != nullptr);
    REQUIRE(para.styleId() == "heading1");
    REQUIRE(newPara->styleId() == "heading1");
}

TEST_CASE("KmlParagraph splitAt multiple elements", "[editor][kml_paragraph][phase1.7]") {
    SECTION("Split at element boundary") {
        KmlParagraph para;
        addText(para, "Hello");
        addText(para, "World");

        auto newPara = para.splitAt(5);

        REQUIRE(newPara != nullptr);
        REQUIRE(para.plainText() == "Hello");
        REQUIRE(newPara->plainText() == "World");
        REQUIRE(para.elementCount() == 1);
        REQUIRE(newPara->elementCount() == 1);
    }

    SECTION("Split within first element") {
        KmlParagraph para;
        addText(para, "Hello");
        addText(para, "World");

        auto newPara = para.splitAt(3);

        REQUIRE(newPara != nullptr);
        REQUIRE(para.plainText() == "Hel");
        REQUIRE(newPara->plainText() == "loWorld");
    }

    SECTION("Split within second element") {
        KmlParagraph para;
        addText(para, "Hello");
        addText(para, "World");

        auto newPara = para.splitAt(7);

        REQUIRE(newPara != nullptr);
        REQUIRE(para.plainText() == "HelloWo");
        REQUIRE(newPara->plainText() == "rld");
    }
}

TEST_CASE("KmlParagraph splitAt with formatting", "[editor][kml_paragraph][phase1.7]") {
    KmlParagraph para;
    addText(para, "Normal ");

    auto bold = std::make_unique<KmlBold>();
    bold->appendChild(std::make_unique<KmlTextRun>("bold"));
    para.addElement(std::move(bold));

    addText(para, " text");

    REQUIRE(para.plainText() == "Normal bold text");

    SECTION("Split before formatted element") {
        auto newPara = para.splitAt(7);

        REQUIRE(newPara != nullptr);
        REQUIRE(para.plainText() == "Normal ");
        REQUIRE(newPara->plainText() == "bold text");
    }

    SECTION("Split after formatted element") {
        auto newPara = para.splitAt(11);

        REQUIRE(newPara != nullptr);
        REQUIRE(para.plainText() == "Normal bold");
        REQUIRE(newPara->plainText() == " text");
    }
}

// -----------------------------------------------------------------------------
// mergeWith() Tests
// -----------------------------------------------------------------------------

TEST_CASE("KmlParagraph mergeWith basic", "[editor][kml_paragraph][phase1.7]") {
    SECTION("Merge two simple paragraphs") {
        KmlParagraph para1("Hello ");
        KmlParagraph para2("World");

        para1.mergeWith(para2);

        REQUIRE(para1.plainText() == "Hello World");
        REQUIRE(para2.plainText().isEmpty());
        REQUIRE(para2.isEmpty());
    }

    SECTION("Merge empty paragraph") {
        KmlParagraph para1("Hello");
        KmlParagraph para2;

        para1.mergeWith(para2);

        REQUIRE(para1.plainText() == "Hello");
    }

    SECTION("Merge into empty paragraph") {
        KmlParagraph para1;
        KmlParagraph para2("World");

        para1.mergeWith(para2);

        REQUIRE(para1.plainText() == "World");
        REQUIRE(para2.isEmpty());
    }
}

TEST_CASE("KmlParagraph mergeWith preserves elements", "[editor][kml_paragraph][phase1.7]") {
    KmlParagraph para1;
    addText(para1, "First");
    addText(para1, "Second");

    KmlParagraph para2;
    addText(para2, "Third");
    addText(para2, "Fourth");

    para1.mergeWith(para2);

    REQUIRE(para1.elementCount() == 4);
    REQUIRE(para1.plainText() == "FirstSecondThirdFourth");
    REQUIRE(para2.elementCount() == 0);
}

TEST_CASE("KmlParagraph mergeWith formatting", "[editor][kml_paragraph][phase1.7]") {
    KmlParagraph para1;
    addText(para1, "Normal ");

    KmlParagraph para2;
    auto bold = std::make_unique<KmlBold>();
    bold->appendChild(std::make_unique<KmlTextRun>("bold"));
    para2.addElement(std::move(bold));

    para1.mergeWith(para2);

    REQUIRE(para1.plainText() == "Normal bold");
    REQUIRE(para1.elementCount() == 2);
    REQUIRE(para1.elementAt(1)->type() == ElementType::Bold);
}

// -----------------------------------------------------------------------------
// Round-trip Tests (split + merge)
// -----------------------------------------------------------------------------

TEST_CASE("KmlParagraph split and merge round-trip", "[editor][kml_paragraph][phase1.7]") {
    SECTION("Split then merge restores content") {
        KmlParagraph original("Hello World");
        auto second = original.splitAt(6);

        REQUIRE(second != nullptr);
        original.mergeWith(*second);

        REQUIRE(original.plainText() == "Hello World");
    }

    SECTION("Multiple splits then merges") {
        KmlParagraph para("ABCDEFGHIJ");

        auto para2 = para.splitAt(5);
        REQUIRE(para2 != nullptr);
        REQUIRE(para.plainText() == "ABCDE");
        REQUIRE(para2->plainText() == "FGHIJ");

        auto para3 = para2->splitAt(3);
        REQUIRE(para3 != nullptr);
        REQUIRE(para2->plainText() == "FGH");
        REQUIRE(para3->plainText() == "IJ");

        // Merge back
        para2->mergeWith(*para3);
        REQUIRE(para2->plainText() == "FGHIJ");

        para.mergeWith(*para2);
        REQUIRE(para.plainText() == "ABCDEFGHIJ");
    }
}

// -----------------------------------------------------------------------------
// Edge Cases
// -----------------------------------------------------------------------------

TEST_CASE("KmlParagraph text manipulation edge cases", "[editor][kml_paragraph][phase1.7]") {
    SECTION("Insert Unicode text") {
        KmlParagraph para;
        REQUIRE(para.insertText(0, QString::fromUtf8(u8"Cze\u015B\u0107")));
        REQUIRE(para.insertText(0, QString::fromUtf8(u8"\u017B\u00F3\u0142w ")));
        REQUIRE(para.plainText().contains(QString::fromUtf8(u8"\u015B\u0107")));
    }

    SECTION("Delete Unicode text") {
        KmlParagraph para(QString::fromUtf8(u8"Zaz\u00F3\u0142\u0107"));
        int len = para.length();
        REQUIRE(para.deleteText(0, 3));
        REQUIRE(para.length() == len - 3);
    }

    SECTION("Split on Unicode boundary") {
        KmlParagraph para(QString::fromUtf8(u8"Zaz\u00F3\u0142\u0107 test"));
        auto second = para.splitAt(7);
        REQUIRE(second != nullptr);
        REQUIRE(second->plainText() == "test");
    }

    SECTION("Insert special characters") {
        KmlParagraph para("Hello");
        REQUIRE(para.insertText(5, "\n\t"));
        REQUIRE(para.plainText() == "Hello\n\t");
    }

    SECTION("Delete leaves empty elements removed") {
        KmlParagraph para;
        addText(para, "A");
        addText(para, "B");
        addText(para, "C");

        REQUIRE(para.elementCount() == 3);
        REQUIRE(para.deleteText(1, 2));  // Delete "B"
        REQUIRE(para.plainText() == "AC");
        REQUIRE(para.elementCount() == 2);  // Empty element removed
    }
}
