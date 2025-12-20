/// @file test_kml_element.cpp
/// @brief Unit tests for KmlElement base class (OpenSpec #00042 Phase 1.3)

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/kml_element.h>
#include <memory>

using namespace kalahari::editor;

// =============================================================================
// Test implementation of KmlElement for testing abstract base class
// =============================================================================

/// @brief Concrete implementation of KmlElement for testing
class TestElement : public KmlElement {
public:
    explicit TestElement(const QString& text) : m_text(text) {}

    ElementType type() const override { return ElementType::Text; }

    QString toKml() const override {
        return m_text;  // Simple text - no markup needed
    }

    std::unique_ptr<KmlElement> clone() const override {
        return std::make_unique<TestElement>(m_text);
    }

    QString plainText() const override { return m_text; }

    int length() const override { return m_text.length(); }

    void setText(const QString& text) { m_text = text; }

private:
    QString m_text;
};

// =============================================================================
// ElementType Tests
// =============================================================================

TEST_CASE("ElementType enumeration values", "[editor][kml_element]") {
    SECTION("All element types have unique values") {
        // Verify each type is distinct
        REQUIRE(static_cast<int>(ElementType::Text) == 0);
        REQUIRE(static_cast<int>(ElementType::Bold) == 1);
        REQUIRE(static_cast<int>(ElementType::Italic) == 2);
        REQUIRE(static_cast<int>(ElementType::Underline) == 3);
        REQUIRE(static_cast<int>(ElementType::Strikethrough) == 4);
        REQUIRE(static_cast<int>(ElementType::Subscript) == 5);
        REQUIRE(static_cast<int>(ElementType::Superscript) == 6);
        REQUIRE(static_cast<int>(ElementType::Link) == 7);
        REQUIRE(static_cast<int>(ElementType::CharacterStyle) == 8);
    }
}

TEST_CASE("elementTypeToString conversion", "[editor][kml_element]") {
    SECTION("Text type") {
        REQUIRE(elementTypeToString(ElementType::Text) == "Text");
    }

    SECTION("Bold type") {
        REQUIRE(elementTypeToString(ElementType::Bold) == "Bold");
    }

    SECTION("Italic type") {
        REQUIRE(elementTypeToString(ElementType::Italic) == "Italic");
    }

    SECTION("Underline type") {
        REQUIRE(elementTypeToString(ElementType::Underline) == "Underline");
    }

    SECTION("Strikethrough type") {
        REQUIRE(elementTypeToString(ElementType::Strikethrough) == "Strikethrough");
    }

    SECTION("Subscript type") {
        REQUIRE(elementTypeToString(ElementType::Subscript) == "Subscript");
    }

    SECTION("Superscript type") {
        REQUIRE(elementTypeToString(ElementType::Superscript) == "Superscript");
    }

    SECTION("Link type") {
        REQUIRE(elementTypeToString(ElementType::Link) == "Link");
    }

    SECTION("CharacterStyle type") {
        REQUIRE(elementTypeToString(ElementType::CharacterStyle) == "CharacterStyle");
    }
}

// =============================================================================
// KmlElement Interface Tests (using TestElement concrete implementation)
// =============================================================================

TEST_CASE("KmlElement type() method", "[editor][kml_element]") {
    TestElement elem("Hello");
    REQUIRE(elem.type() == ElementType::Text);
}

TEST_CASE("KmlElement toKml() method", "[editor][kml_element]") {
    SECTION("Simple text") {
        TestElement elem("Hello World");
        REQUIRE(elem.toKml() == "Hello World");
    }

    SECTION("Empty text") {
        TestElement elem("");
        REQUIRE(elem.toKml() == "");
    }

    SECTION("Unicode text") {
        TestElement elem(QString::fromUtf8(u8"Witaj \u015Bwiecie"));  // Witaj swiecie with Polish chars
        REQUIRE(elem.toKml() == QString::fromUtf8(u8"Witaj \u015Bwiecie"));
    }
}

TEST_CASE("KmlElement clone() method", "[editor][kml_element]") {
    SECTION("Clone creates independent copy") {
        TestElement original("Original text");
        auto cloned = original.clone();

        REQUIRE(cloned != nullptr);
        REQUIRE(cloned->plainText() == "Original text");
        REQUIRE(cloned->type() == ElementType::Text);

        // Modify original - clone should not change
        original.setText("Modified text");
        REQUIRE(original.plainText() == "Modified text");
        REQUIRE(cloned->plainText() == "Original text");
    }

    SECTION("Clone is a different object") {
        TestElement original("Test");
        auto cloned = original.clone();

        // Pointer comparison - must be different objects
        REQUIRE(cloned.get() != &original);
    }
}

TEST_CASE("KmlElement plainText() method", "[editor][kml_element]") {
    SECTION("Simple text") {
        TestElement elem("Plain text content");
        REQUIRE(elem.plainText() == "Plain text content");
    }

    SECTION("Multiline text") {
        TestElement elem("Line 1\nLine 2\nLine 3");
        REQUIRE(elem.plainText() == "Line 1\nLine 2\nLine 3");
    }
}

TEST_CASE("KmlElement length() method", "[editor][kml_element]") {
    SECTION("Empty element") {
        TestElement elem("");
        REQUIRE(elem.length() == 0);
    }

    SECTION("Single character") {
        TestElement elem("X");
        REQUIRE(elem.length() == 1);
    }

    SECTION("Multiple characters") {
        TestElement elem("Hello");
        REQUIRE(elem.length() == 5);
    }

    SECTION("Unicode characters") {
        // Polish text: 5 characters (but possibly more bytes)
        TestElement elem(QString::fromUtf8(u8"\u017C\u00F3\u0142w"));  // zolw with Polish chars
        REQUIRE(elem.length() == 4);
    }
}

TEST_CASE("KmlElement isEmpty() method", "[editor][kml_element]") {
    SECTION("Empty element") {
        TestElement elem("");
        REQUIRE(elem.isEmpty() == true);
    }

    SECTION("Non-empty element") {
        TestElement elem("a");
        REQUIRE(elem.isEmpty() == false);
    }

    SECTION("Whitespace is not empty") {
        TestElement elem(" ");
        REQUIRE(elem.isEmpty() == false);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("KmlElement edge cases", "[editor][kml_element]") {
    SECTION("Special characters in text") {
        TestElement elem("<>&\"'");
        REQUIRE(elem.plainText() == "<>&\"'");
        REQUIRE(elem.length() == 5);
    }

    SECTION("Very long text") {
        QString longText(10000, 'x');
        TestElement elem(longText);
        REQUIRE(elem.length() == 10000);
        REQUIRE(elem.plainText() == longText);
    }

    SECTION("Text with null character") {
        QString textWithNull = "before";
        textWithNull.append(QChar(0));
        textWithNull.append("after");
        TestElement elem(textWithNull);
        // QString handles embedded nulls
        REQUIRE(elem.length() == 12);  // "before" + null + "after"
    }
}
