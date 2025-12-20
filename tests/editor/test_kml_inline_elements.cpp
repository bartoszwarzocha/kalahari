/// @file test_kml_inline_elements.cpp
/// @brief Unit tests for KML Inline formatting elements (OpenSpec #00042 Phase 1.5)

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/kml_inline_elements.h>
#include <kalahari/editor/kml_text_run.h>
#include <memory>

using namespace kalahari::editor;

// =============================================================================
// Helper function to add text to container
// =============================================================================

template<typename T>
void addText(T& container, const QString& text) {
    container.appendChild(std::make_unique<KmlTextRun>(text));
}

// =============================================================================
// KmlInlineContainer Tests (using KmlBold as concrete implementation)
// =============================================================================

TEST_CASE("KmlInlineContainer default state", "[editor][kml_inline_elements]") {
    KmlBold bold;

    SECTION("Empty container") {
        REQUIRE(bold.isEmpty());
        REQUIRE(bold.length() == 0);
        REQUIRE(bold.childCount() == 0);
        REQUIRE(bold.plainText().isEmpty());
    }

    SECTION("Children access returns nullptr for empty") {
        REQUIRE(bold.childAt(0) == nullptr);
        REQUIRE(bold.childAt(-1) == nullptr);
        REQUIRE(bold.childAt(100) == nullptr);
    }
}

TEST_CASE("KmlInlineContainer appendChild", "[editor][kml_inline_elements]") {
    KmlBold bold;

    SECTION("Add single child") {
        bold.appendChild(std::make_unique<KmlTextRun>("Hello"));

        REQUIRE(bold.childCount() == 1);
        REQUIRE(bold.length() == 5);
        REQUIRE(bold.plainText() == "Hello");
        REQUIRE(bold.isEmpty() == false);
    }

    SECTION("Add multiple children") {
        bold.appendChild(std::make_unique<KmlTextRun>("Hello "));
        bold.appendChild(std::make_unique<KmlTextRun>("World"));

        REQUIRE(bold.childCount() == 2);
        REQUIRE(bold.length() == 11);
        REQUIRE(bold.plainText() == "Hello World");
    }

    SECTION("Ignore nullptr") {
        bold.appendChild(nullptr);
        REQUIRE(bold.childCount() == 0);
    }
}

TEST_CASE("KmlInlineContainer insertChild", "[editor][kml_inline_elements]") {
    KmlBold bold;
    addText(bold, "First");
    addText(bold, "Third");

    SECTION("Insert in middle") {
        bold.insertChild(1, std::make_unique<KmlTextRun>("Second"));

        REQUIRE(bold.childCount() == 3);
        REQUIRE(bold.childAt(0)->plainText() == "First");
        REQUIRE(bold.childAt(1)->plainText() == "Second");
        REQUIRE(bold.childAt(2)->plainText() == "Third");
    }

    SECTION("Insert at beginning") {
        bold.insertChild(0, std::make_unique<KmlTextRun>("Zero"));

        REQUIRE(bold.childCount() == 3);
        REQUIRE(bold.childAt(0)->plainText() == "Zero");
    }

    SECTION("Insert at end (beyond size)") {
        bold.insertChild(100, std::make_unique<KmlTextRun>("End"));

        REQUIRE(bold.childCount() == 3);
        REQUIRE(bold.childAt(2)->plainText() == "End");
    }

    SECTION("Insert with negative index") {
        bold.insertChild(-5, std::make_unique<KmlTextRun>("Negative"));

        REQUIRE(bold.childCount() == 3);
        REQUIRE(bold.childAt(0)->plainText() == "Negative");
    }
}

TEST_CASE("KmlInlineContainer removeChild", "[editor][kml_inline_elements]") {
    KmlBold bold;
    addText(bold, "First");
    addText(bold, "Second");
    addText(bold, "Third");

    SECTION("Remove middle child") {
        auto removed = bold.removeChild(1);

        REQUIRE(removed != nullptr);
        REQUIRE(removed->plainText() == "Second");
        REQUIRE(bold.childCount() == 2);
        REQUIRE(bold.childAt(0)->plainText() == "First");
        REQUIRE(bold.childAt(1)->plainText() == "Third");
    }

    SECTION("Remove first child") {
        auto removed = bold.removeChild(0);

        REQUIRE(removed != nullptr);
        REQUIRE(removed->plainText() == "First");
        REQUIRE(bold.childCount() == 2);
    }

    SECTION("Remove last child") {
        auto removed = bold.removeChild(2);

        REQUIRE(removed != nullptr);
        REQUIRE(removed->plainText() == "Third");
        REQUIRE(bold.childCount() == 2);
    }

    SECTION("Remove invalid index returns nullptr") {
        auto removed = bold.removeChild(100);
        REQUIRE(removed == nullptr);
        REQUIRE(bold.childCount() == 3);

        removed = bold.removeChild(-1);
        REQUIRE(removed == nullptr);
    }
}

TEST_CASE("KmlInlineContainer clearChildren", "[editor][kml_inline_elements]") {
    KmlBold bold;
    addText(bold, "First");
    addText(bold, "Second");
    addText(bold, "Third");

    REQUIRE(bold.childCount() == 3);

    bold.clearChildren();

    REQUIRE(bold.childCount() == 0);
    REQUIRE(bold.isEmpty());
}

TEST_CASE("KmlInlineContainer children() access", "[editor][kml_inline_elements]") {
    KmlBold bold;
    addText(bold, "A");
    addText(bold, "B");

    const auto& children = bold.children();

    REQUIRE(children.size() == 2);
    REQUIRE(children[0]->plainText() == "A");
    REQUIRE(children[1]->plainText() == "B");
}

// =============================================================================
// KmlBold Tests
// =============================================================================

TEST_CASE("KmlBold type", "[editor][kml_inline_elements]") {
    KmlBold bold;
    REQUIRE(bold.type() == ElementType::Bold);
}

TEST_CASE("KmlBold toKml", "[editor][kml_inline_elements]") {
    SECTION("Empty bold") {
        KmlBold bold;
        REQUIRE(bold.toKml() == "<b></b>");
    }

    SECTION("Bold with text") {
        KmlBold bold;
        addText(bold, "Bold text");

        QString kml = bold.toKml();
        REQUIRE(kml.contains("<b>"));
        REQUIRE(kml.contains("</b>"));
        REQUIRE(kml.contains("Bold text"));
    }

    SECTION("Bold with styled text") {
        KmlBold bold;
        bold.appendChild(std::make_unique<KmlTextRun>("Styled", "emphasis"));

        QString kml = bold.toKml();
        REQUIRE(kml.contains("<b>"));
        REQUIRE(kml.contains("style=\"emphasis\""));
    }
}

TEST_CASE("KmlBold clone", "[editor][kml_inline_elements]") {
    KmlBold original;
    addText(original, "Clone me");

    auto cloned = original.clone();

    SECTION("Clone is not null") {
        REQUIRE(cloned != nullptr);
    }

    SECTION("Clone has correct type") {
        REQUIRE(cloned->type() == ElementType::Bold);
    }

    SECTION("Clone has same content") {
        REQUIRE(cloned->plainText() == "Clone me");
    }

    SECTION("Clone is independent") {
        auto* boldClone = dynamic_cast<KmlBold*>(cloned.get());
        REQUIRE(boldClone != nullptr);

        original.clearChildren();
        REQUIRE(cloned->plainText() == "Clone me");
    }
}

TEST_CASE("KmlBold copy constructor", "[editor][kml_inline_elements]") {
    KmlBold original;
    addText(original, "Copy me");

    KmlBold copy(original);

    REQUIRE(copy.plainText() == "Copy me");

    // Verify independence
    original.clearChildren();
    REQUIRE(copy.plainText() == "Copy me");
}

TEST_CASE("KmlBold move constructor", "[editor][kml_inline_elements]") {
    KmlBold original;
    addText(original, "Move me");

    KmlBold moved(std::move(original));

    REQUIRE(moved.plainText() == "Move me");
}

// =============================================================================
// KmlItalic Tests
// =============================================================================

TEST_CASE("KmlItalic type", "[editor][kml_inline_elements]") {
    KmlItalic italic;
    REQUIRE(italic.type() == ElementType::Italic);
}

TEST_CASE("KmlItalic toKml", "[editor][kml_inline_elements]") {
    SECTION("Empty italic") {
        KmlItalic italic;
        REQUIRE(italic.toKml() == "<i></i>");
    }

    SECTION("Italic with text") {
        KmlItalic italic;
        addText(italic, "Italic text");

        QString kml = italic.toKml();
        REQUIRE(kml.contains("<i>"));
        REQUIRE(kml.contains("</i>"));
        REQUIRE(kml.contains("Italic text"));
    }
}

TEST_CASE("KmlItalic clone", "[editor][kml_inline_elements]") {
    KmlItalic original;
    addText(original, "Clone me");

    auto cloned = original.clone();

    REQUIRE(cloned != nullptr);
    REQUIRE(cloned->type() == ElementType::Italic);
    REQUIRE(cloned->plainText() == "Clone me");
}

// =============================================================================
// KmlUnderline Tests
// =============================================================================

TEST_CASE("KmlUnderline type", "[editor][kml_inline_elements]") {
    KmlUnderline underline;
    REQUIRE(underline.type() == ElementType::Underline);
}

TEST_CASE("KmlUnderline toKml", "[editor][kml_inline_elements]") {
    SECTION("Empty underline") {
        KmlUnderline underline;
        REQUIRE(underline.toKml() == "<u></u>");
    }

    SECTION("Underline with text") {
        KmlUnderline underline;
        addText(underline, "Underlined text");

        QString kml = underline.toKml();
        REQUIRE(kml.contains("<u>"));
        REQUIRE(kml.contains("</u>"));
        REQUIRE(kml.contains("Underlined text"));
    }
}

TEST_CASE("KmlUnderline clone", "[editor][kml_inline_elements]") {
    KmlUnderline original;
    addText(original, "Clone me");

    auto cloned = original.clone();

    REQUIRE(cloned != nullptr);
    REQUIRE(cloned->type() == ElementType::Underline);
    REQUIRE(cloned->plainText() == "Clone me");
}

// =============================================================================
// KmlStrikethrough Tests
// =============================================================================

TEST_CASE("KmlStrikethrough type", "[editor][kml_inline_elements]") {
    KmlStrikethrough strike;
    REQUIRE(strike.type() == ElementType::Strikethrough);
}

TEST_CASE("KmlStrikethrough toKml", "[editor][kml_inline_elements]") {
    SECTION("Empty strikethrough") {
        KmlStrikethrough strike;
        REQUIRE(strike.toKml() == "<s></s>");
    }

    SECTION("Strikethrough with text") {
        KmlStrikethrough strike;
        addText(strike, "Deleted text");

        QString kml = strike.toKml();
        REQUIRE(kml.contains("<s>"));
        REQUIRE(kml.contains("</s>"));
        REQUIRE(kml.contains("Deleted text"));
    }
}

TEST_CASE("KmlStrikethrough clone", "[editor][kml_inline_elements]") {
    KmlStrikethrough original;
    addText(original, "Clone me");

    auto cloned = original.clone();

    REQUIRE(cloned != nullptr);
    REQUIRE(cloned->type() == ElementType::Strikethrough);
    REQUIRE(cloned->plainText() == "Clone me");
}

// =============================================================================
// KmlSubscript Tests
// =============================================================================

TEST_CASE("KmlSubscript type", "[editor][kml_inline_elements]") {
    KmlSubscript sub;
    REQUIRE(sub.type() == ElementType::Subscript);
}

TEST_CASE("KmlSubscript toKml", "[editor][kml_inline_elements]") {
    SECTION("Empty subscript") {
        KmlSubscript sub;
        REQUIRE(sub.toKml() == "<sub></sub>");
    }

    SECTION("Subscript with text") {
        KmlSubscript sub;
        addText(sub, "2");

        QString kml = sub.toKml();
        REQUIRE(kml.contains("<sub>"));
        REQUIRE(kml.contains("</sub>"));
        REQUIRE(kml.contains("2"));
    }
}

TEST_CASE("KmlSubscript clone", "[editor][kml_inline_elements]") {
    KmlSubscript original;
    addText(original, "2");

    auto cloned = original.clone();

    REQUIRE(cloned != nullptr);
    REQUIRE(cloned->type() == ElementType::Subscript);
    REQUIRE(cloned->plainText() == "2");
}

// =============================================================================
// KmlSuperscript Tests
// =============================================================================

TEST_CASE("KmlSuperscript type", "[editor][kml_inline_elements]") {
    KmlSuperscript sup;
    REQUIRE(sup.type() == ElementType::Superscript);
}

TEST_CASE("KmlSuperscript toKml", "[editor][kml_inline_elements]") {
    SECTION("Empty superscript") {
        KmlSuperscript sup;
        REQUIRE(sup.toKml() == "<sup></sup>");
    }

    SECTION("Superscript with text") {
        KmlSuperscript sup;
        addText(sup, "2");

        QString kml = sup.toKml();
        REQUIRE(kml.contains("<sup>"));
        REQUIRE(kml.contains("</sup>"));
        REQUIRE(kml.contains("2"));
    }
}

TEST_CASE("KmlSuperscript clone", "[editor][kml_inline_elements]") {
    KmlSuperscript original;
    addText(original, "2");

    auto cloned = original.clone();

    REQUIRE(cloned != nullptr);
    REQUIRE(cloned->type() == ElementType::Superscript);
    REQUIRE(cloned->plainText() == "2");
}

// =============================================================================
// Nested Element Tests
// =============================================================================

TEST_CASE("Nested inline elements - bold inside italic", "[editor][kml_inline_elements]") {
    KmlItalic italic;

    auto bold = std::make_unique<KmlBold>();
    addText(*bold, "Bold and italic");
    italic.appendChild(std::move(bold));

    SECTION("Plain text extraction") {
        REQUIRE(italic.plainText() == "Bold and italic");
        REQUIRE(italic.length() == 15);
    }

    SECTION("KML serialization") {
        QString kml = italic.toKml();
        REQUIRE(kml.contains("<i>"));
        REQUIRE(kml.contains("<b>"));
        REQUIRE(kml.contains("Bold and italic"));
        REQUIRE(kml.contains("</b>"));
        REQUIRE(kml.contains("</i>"));
    }

    SECTION("Clone preserves nesting") {
        auto cloned = italic.clone();

        REQUIRE(cloned->plainText() == "Bold and italic");

        auto* italicClone = dynamic_cast<KmlItalic*>(cloned.get());
        REQUIRE(italicClone != nullptr);
        REQUIRE(italicClone->childCount() == 1);
        REQUIRE(italicClone->childAt(0)->type() == ElementType::Bold);
    }
}

TEST_CASE("Nested inline elements - mixed content", "[editor][kml_inline_elements]") {
    // Create: <b>Normal <i>italic</i> bold</b>
    KmlBold bold;

    bold.appendChild(std::make_unique<KmlTextRun>("Normal "));

    auto italic = std::make_unique<KmlItalic>();
    addText(*italic, "italic");
    bold.appendChild(std::move(italic));

    bold.appendChild(std::make_unique<KmlTextRun>(" bold"));

    SECTION("Plain text") {
        REQUIRE(bold.plainText() == "Normal italic bold");
    }

    SECTION("Length") {
        REQUIRE(bold.length() == 18);
    }

    SECTION("Child count") {
        REQUIRE(bold.childCount() == 3);
    }

    SECTION("KML structure") {
        QString kml = bold.toKml();

        // Check structure
        REQUIRE(kml.startsWith("<b>"));
        REQUIRE(kml.endsWith("</b>"));
        // Check for either direct italic tags or wrapped in text run
        bool hasItalic = kml.contains("<i>italic</i>") || kml.contains("<i><t>italic</t></i>");
        REQUIRE(hasItalic);
    }
}

TEST_CASE("Deep nesting - three levels", "[editor][kml_inline_elements]") {
    // Create: <b><i><u>Deep nesting</u></i></b>
    KmlBold bold;

    auto italic = std::make_unique<KmlItalic>();
    auto underline = std::make_unique<KmlUnderline>();
    addText(*underline, "Deep nesting");

    italic->appendChild(std::move(underline));
    bold.appendChild(std::move(italic));

    SECTION("Plain text extraction works through all levels") {
        REQUIRE(bold.plainText() == "Deep nesting");
    }

    SECTION("Length works through all levels") {
        REQUIRE(bold.length() == 12);
    }

    SECTION("Clone preserves deep nesting") {
        auto cloned = bold.clone();
        REQUIRE(cloned->plainText() == "Deep nesting");

        auto* boldClone = dynamic_cast<KmlBold*>(cloned.get());
        REQUIRE(boldClone->childCount() == 1);

        auto* italicChild = dynamic_cast<KmlItalic*>(boldClone->childAt(0));
        REQUIRE(italicChild != nullptr);
        REQUIRE(italicChild->childCount() == 1);

        auto* underlineChild = dynamic_cast<KmlUnderline*>(italicChild->childAt(0));
        REQUIRE(underlineChild != nullptr);
        REQUIRE(underlineChild->plainText() == "Deep nesting");
    }
}

// =============================================================================
// Chemical/Math notation examples
// =============================================================================

TEST_CASE("Chemical formula example - H2O", "[editor][kml_inline_elements]") {
    // Create: H<sub>2</sub>O
    KmlBold container;  // Using bold as generic container for test

    container.appendChild(std::make_unique<KmlTextRun>("H"));

    auto subscript = std::make_unique<KmlSubscript>();
    addText(*subscript, "2");
    container.appendChild(std::move(subscript));

    container.appendChild(std::make_unique<KmlTextRun>("O"));

    SECTION("Plain text") {
        REQUIRE(container.plainText() == "H2O");
    }

    SECTION("KML contains subscript") {
        QString kml = container.toKml();
        REQUIRE(kml.contains("<sub>"));
        REQUIRE(kml.contains("</sub>"));
    }
}

TEST_CASE("Mathematical exponent example - x^2", "[editor][kml_inline_elements]") {
    // Create: x<sup>2</sup>
    KmlItalic container;  // Using italic as generic container

    container.appendChild(std::make_unique<KmlTextRun>("x"));

    auto superscript = std::make_unique<KmlSuperscript>();
    addText(*superscript, "2");
    container.appendChild(std::move(superscript));

    SECTION("Plain text") {
        REQUIRE(container.plainText() == "x2");
    }

    SECTION("KML contains superscript") {
        QString kml = container.toKml();
        REQUIRE(kml.contains("<sup>"));
        REQUIRE(kml.contains("</sup>"));
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("Inline element with empty text run", "[editor][kml_inline_elements]") {
    KmlBold bold;
    bold.appendChild(std::make_unique<KmlTextRun>(""));

    REQUIRE(bold.childCount() == 1);
    REQUIRE(bold.isEmpty());
    REQUIRE(bold.length() == 0);
    REQUIRE(bold.plainText().isEmpty());
}

TEST_CASE("Inline element with whitespace", "[editor][kml_inline_elements]") {
    KmlBold bold;
    addText(bold, "  spaces  ");

    REQUIRE(bold.plainText() == "  spaces  ");
    REQUIRE(bold.length() == 10);
}

TEST_CASE("Inline element with newlines", "[editor][kml_inline_elements]") {
    KmlBold bold;
    addText(bold, "line1\nline2\nline3");

    REQUIRE(bold.plainText() == "line1\nline2\nline3");
    REQUIRE(bold.length() == 17);
}

TEST_CASE("Inline element with Unicode", "[editor][kml_inline_elements]") {
    KmlBold bold;
    addText(bold, QString::fromUtf8(u8"Zaz\u00F3\u0142\u0107 g\u0119\u015Bl\u0105 ja\u017A\u0144"));

    REQUIRE(bold.plainText() == QString::fromUtf8(u8"Zaz\u00F3\u0142\u0107 g\u0119\u015Bl\u0105 ja\u017A\u0144"));
    REQUIRE(bold.isEmpty() == false);
}

TEST_CASE("Multiple text runs concatenation", "[editor][kml_inline_elements]") {
    KmlBold bold;
    addText(bold, "A");
    addText(bold, "B");
    addText(bold, "C");
    addText(bold, "D");
    addText(bold, "E");

    REQUIRE(bold.plainText() == "ABCDE");
    REQUIRE(bold.length() == 5);
    REQUIRE(bold.childCount() == 5);
}

// =============================================================================
// Assignment Operator Tests
// =============================================================================

TEST_CASE("KmlBold copy assignment", "[editor][kml_inline_elements]") {
    KmlBold original;
    addText(original, "Source");

    KmlBold target;
    addText(target, "Target");

    target = original;

    REQUIRE(target.plainText() == "Source");

    // Self-assignment
    target = target;
    REQUIRE(target.plainText() == "Source");
}

TEST_CASE("KmlBold move assignment", "[editor][kml_inline_elements]") {
    KmlBold original;
    addText(original, "Moving");

    KmlBold target;
    addText(target, "Target");

    target = std::move(original);

    REQUIRE(target.plainText() == "Moving");
}

// =============================================================================
// Polymorphism Tests
// =============================================================================

TEST_CASE("Inline elements through base pointer", "[editor][kml_inline_elements]") {
    std::vector<std::unique_ptr<KmlElement>> elements;

    auto bold = std::make_unique<KmlBold>();
    addText(*bold, "Bold");
    elements.push_back(std::move(bold));

    auto italic = std::make_unique<KmlItalic>();
    addText(*italic, "Italic");
    elements.push_back(std::move(italic));

    auto underline = std::make_unique<KmlUnderline>();
    addText(*underline, "Underline");
    elements.push_back(std::move(underline));

    SECTION("Types are correct") {
        REQUIRE(elements[0]->type() == ElementType::Bold);
        REQUIRE(elements[1]->type() == ElementType::Italic);
        REQUIRE(elements[2]->type() == ElementType::Underline);
    }

    SECTION("Plain text works through base") {
        REQUIRE(elements[0]->plainText() == "Bold");
        REQUIRE(elements[1]->plainText() == "Italic");
        REQUIRE(elements[2]->plainText() == "Underline");
    }

    SECTION("Clone works through base") {
        auto cloned = elements[0]->clone();
        REQUIRE(cloned->type() == ElementType::Bold);
        REQUIRE(cloned->plainText() == "Bold");
    }

    SECTION("toKml works through base") {
        QString kml = elements[0]->toKml();
        REQUIRE(kml.contains("<b>"));
    }
}
