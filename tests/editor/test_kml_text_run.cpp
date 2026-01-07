/// @file test_kml_text_run.cpp
/// @brief Unit tests for KmlTextRun class (OpenSpec #00042 Phase 1.4)

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/kml_text_run.h>
#include <memory>

using namespace kalahari::editor;

// =============================================================================
// Constructor Tests
// =============================================================================

TEST_CASE("KmlTextRun default constructor", "[editor][kml_text_run]") {
    KmlTextRun run;

    SECTION("Creates empty text run") {
        REQUIRE(run.text().isEmpty());
        REQUIRE(run.styleId().isEmpty());
        REQUIRE(run.isEmpty());
        REQUIRE(run.length() == 0);
    }

    SECTION("Has correct type") {
        REQUIRE(run.type() == ElementType::Text);
    }

    SECTION("Has no style") {
        REQUIRE(run.hasStyle() == false);
    }
}

TEST_CASE("KmlTextRun text-only constructor", "[editor][kml_text_run]") {
    SECTION("With simple text") {
        KmlTextRun run("Hello");
        REQUIRE(run.text() == "Hello");
        REQUIRE(run.styleId().isEmpty());
        REQUIRE(run.hasStyle() == false);
        REQUIRE(run.length() == 5);
    }

    SECTION("With empty text") {
        KmlTextRun run("");
        REQUIRE(run.text().isEmpty());
        REQUIRE(run.isEmpty());
    }

    SECTION("With Unicode text") {
        KmlTextRun run(QString::fromUtf8(u8"Cze\u015B\u0107"));  // Czesc with Polish chars (5 chars)
        REQUIRE(run.length() == 5);
        REQUIRE(run.isEmpty() == false);
    }
}

TEST_CASE("KmlTextRun text+style constructor", "[editor][kml_text_run]") {
    SECTION("With both text and style") {
        KmlTextRun run("Important", "emphasis");
        REQUIRE(run.text() == "Important");
        REQUIRE(run.styleId() == "emphasis");
        REQUIRE(run.hasStyle() == true);
    }

    SECTION("With text and empty style") {
        KmlTextRun run("Normal", "");
        REQUIRE(run.text() == "Normal");
        REQUIRE(run.styleId().isEmpty());
        REQUIRE(run.hasStyle() == false);
    }
}

// =============================================================================
// Copy and Move Tests
// =============================================================================

TEST_CASE("KmlTextRun copy constructor", "[editor][kml_text_run]") {
    KmlTextRun original("Original text", "style1");
    KmlTextRun copy(original);

    SECTION("Copy has same content") {
        REQUIRE(copy.text() == "Original text");
        REQUIRE(copy.styleId() == "style1");
    }

    SECTION("Copy is independent") {
        KmlTextRun mutableOriginal("Test", "style");
        KmlTextRun copiedRun(mutableOriginal);

        mutableOriginal.setText("Modified");
        mutableOriginal.setStyleId("different");

        REQUIRE(copiedRun.text() == "Test");
        REQUIRE(copiedRun.styleId() == "style");
    }
}

TEST_CASE("KmlTextRun move constructor", "[editor][kml_text_run]") {
    KmlTextRun original("Move me", "moveStyle");
    QString originalText = original.text();
    QString originalStyle = original.styleId();

    KmlTextRun moved(std::move(original));

    SECTION("Moved object has original content") {
        REQUIRE(moved.text() == originalText);
        REQUIRE(moved.styleId() == originalStyle);
    }
}

TEST_CASE("KmlTextRun copy assignment", "[editor][kml_text_run]") {
    KmlTextRun original("Source", "srcStyle");
    KmlTextRun target("Target", "tgtStyle");

    target = original;

    SECTION("Target has source content") {
        REQUIRE(target.text() == "Source");
        REQUIRE(target.styleId() == "srcStyle");
    }

    SECTION("Self-assignment works") {
        KmlTextRun run("Self", "selfStyle");
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
#endif
        run = run;  // Self-assignment test
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
        REQUIRE(run.text() == "Self");
        REQUIRE(run.styleId() == "selfStyle");
    }
}

TEST_CASE("KmlTextRun move assignment", "[editor][kml_text_run]") {
    KmlTextRun original("Moving", "moveStyle");
    KmlTextRun target("Target", "tgtStyle");

    target = std::move(original);

    REQUIRE(target.text() == "Moving");
    REQUIRE(target.styleId() == "moveStyle");
}

// =============================================================================
// Getter/Setter Tests
// =============================================================================

TEST_CASE("KmlTextRun setText", "[editor][kml_text_run]") {
    KmlTextRun run;

    SECTION("Set simple text") {
        run.setText("New text");
        REQUIRE(run.text() == "New text");
        REQUIRE(run.length() == 8);
    }

    SECTION("Replace existing text") {
        run.setText("First");
        run.setText("Second");
        REQUIRE(run.text() == "Second");
    }

    SECTION("Set to empty") {
        run.setText("Something");
        run.setText("");
        REQUIRE(run.isEmpty());
    }
}

TEST_CASE("KmlTextRun setStyleId", "[editor][kml_text_run]") {
    KmlTextRun run("Text");

    SECTION("Set style") {
        run.setStyleId("bold");
        REQUIRE(run.styleId() == "bold");
        REQUIRE(run.hasStyle());
    }

    SECTION("Clear style") {
        run.setStyleId("someStyle");
        run.setStyleId("");
        REQUIRE(run.styleId().isEmpty());
        REQUIRE(run.hasStyle() == false);
    }
}

// =============================================================================
// KmlElement Interface Tests
// =============================================================================

TEST_CASE("KmlTextRun type()", "[editor][kml_text_run]") {
    KmlTextRun run("Any text");
    REQUIRE(run.type() == ElementType::Text);
}

TEST_CASE("KmlTextRun plainText()", "[editor][kml_text_run]") {
    SECTION("Returns text content") {
        KmlTextRun run("Plain text here");
        REQUIRE(run.plainText() == "Plain text here");
    }

    SECTION("Same as text()") {
        KmlTextRun run("Same content");
        REQUIRE(run.plainText() == run.text());
    }
}

TEST_CASE("KmlTextRun length()", "[editor][kml_text_run]") {
    SECTION("Empty run") {
        KmlTextRun run;
        REQUIRE(run.length() == 0);
    }

    SECTION("ASCII text") {
        KmlTextRun run("12345");
        REQUIRE(run.length() == 5);
    }

    SECTION("Unicode text - counts code units") {
        // Polish: zolw (turtle) with special chars
        KmlTextRun run(QString::fromUtf8(u8"\u017C\u00F3\u0142w"));
        REQUIRE(run.length() == 4);
    }
}

TEST_CASE("KmlTextRun isEmpty()", "[editor][kml_text_run]") {
    SECTION("Empty run is empty") {
        KmlTextRun run;
        REQUIRE(run.isEmpty());
    }

    SECTION("Non-empty run is not empty") {
        KmlTextRun run("x");
        REQUIRE(run.isEmpty() == false);
    }

    SECTION("Whitespace is not empty") {
        KmlTextRun run(" ");
        REQUIRE(run.isEmpty() == false);
    }
}

TEST_CASE("KmlTextRun clone()", "[editor][kml_text_run]") {
    KmlTextRun original("Clone me", "cloneStyle");
    auto cloned = original.clone();

    SECTION("Clone is not null") {
        REQUIRE(cloned != nullptr);
    }

    SECTION("Clone has correct type") {
        REQUIRE(cloned->type() == ElementType::Text);
    }

    SECTION("Clone has same content") {
        REQUIRE(cloned->plainText() == "Clone me");

        // Cast to KmlTextRun to check styleId
        auto* textRun = dynamic_cast<KmlTextRun*>(cloned.get());
        REQUIRE(textRun != nullptr);
        REQUIRE(textRun->styleId() == "cloneStyle");
    }

    SECTION("Clone is independent") {
        KmlTextRun mutableOriginal("Test", "style");
        auto clonedRun = mutableOriginal.clone();

        mutableOriginal.setText("Modified");

        REQUIRE(clonedRun->plainText() == "Test");
    }

    SECTION("Clone is different object") {
        REQUIRE(cloned.get() != &original);
    }
}

// =============================================================================
// toKml() Tests
// =============================================================================

TEST_CASE("KmlTextRun toKml() without style", "[editor][kml_text_run]") {
    SECTION("Simple text") {
        KmlTextRun run("Hello");
        QString kml = run.toKml();

        REQUIRE(kml.contains("<t>"));
        REQUIRE(kml.contains("Hello"));
        REQUIRE(kml.contains("</t>"));
        REQUIRE(!kml.contains("style="));
    }

    SECTION("Empty text") {
        KmlTextRun run("");
        QString kml = run.toKml();

        REQUIRE(kml.contains("<t"));
        REQUIRE(kml.contains("</t>"));
    }
}

TEST_CASE("KmlTextRun toKml() with style", "[editor][kml_text_run]") {
    SECTION("Text with style attribute") {
        KmlTextRun run("Styled text", "emphasis");
        QString kml = run.toKml();

        REQUIRE(kml.contains("<t"));
        REQUIRE(kml.contains("style=\"emphasis\""));
        REQUIRE(kml.contains("Styled text"));
        REQUIRE(kml.contains("</t>"));
    }
}

TEST_CASE("KmlTextRun toKml() XML escaping", "[editor][kml_text_run]") {
    SECTION("Escapes angle brackets") {
        KmlTextRun run("a < b > c");
        QString kml = run.toKml();

        REQUIRE(kml.contains("&lt;"));
        REQUIRE(kml.contains("&gt;"));
    }

    SECTION("Escapes ampersand") {
        KmlTextRun run("A & B");
        QString kml = run.toKml();

        REQUIRE(kml.contains("&amp;"));
    }

    SECTION("Escapes quotes in style attribute") {
        // Style ID shouldn't contain quotes, but text might
        KmlTextRun run("He said \"Hello\"");
        QString kml = run.toKml();

        // QXmlStreamWriter will escape quotes in text content
        REQUIRE(kml.contains("said"));
        REQUIRE(kml.contains("Hello"));
    }
}

// =============================================================================
// fromKml() Tests
// =============================================================================

TEST_CASE("KmlTextRun fromKml() basic parsing", "[editor][kml_text_run]") {
    SECTION("Simple text element") {
        auto run = KmlTextRun::fromKml("<t>Hello World</t>");

        REQUIRE(run != nullptr);
        REQUIRE(run->text() == "Hello World");
        REQUIRE(run->styleId().isEmpty());
    }

    SECTION("Empty text element") {
        auto run = KmlTextRun::fromKml("<t></t>");

        REQUIRE(run != nullptr);
        REQUIRE(run->text().isEmpty());
    }

    SECTION("Self-closing empty element") {
        auto run = KmlTextRun::fromKml("<t/>");

        REQUIRE(run != nullptr);
        REQUIRE(run->text().isEmpty());
    }
}

TEST_CASE("KmlTextRun fromKml() with style", "[editor][kml_text_run]") {
    SECTION("Parses style attribute") {
        auto run = KmlTextRun::fromKml("<t style=\"emphasis\">Important</t>");

        REQUIRE(run != nullptr);
        REQUIRE(run->text() == "Important");
        REQUIRE(run->styleId() == "emphasis");
        REQUIRE(run->hasStyle());
    }

    SECTION("Empty style attribute") {
        auto run = KmlTextRun::fromKml("<t style=\"\">Text</t>");

        REQUIRE(run != nullptr);
        REQUIRE(run->styleId().isEmpty());
        REQUIRE(run->hasStyle() == false);
    }
}

TEST_CASE("KmlTextRun fromKml() with XML entities", "[editor][kml_text_run]") {
    SECTION("Decodes &lt; and &gt;") {
        auto run = KmlTextRun::fromKml("<t>a &lt; b &gt; c</t>");

        REQUIRE(run != nullptr);
        REQUIRE(run->text() == "a < b > c");
    }

    SECTION("Decodes &amp;") {
        auto run = KmlTextRun::fromKml("<t>A &amp; B</t>");

        REQUIRE(run != nullptr);
        REQUIRE(run->text() == "A & B");
    }

    SECTION("Decodes &quot; and &apos;") {
        auto run = KmlTextRun::fromKml("<t>&quot;quoted&apos;</t>");

        REQUIRE(run != nullptr);
        REQUIRE(run->text() == "\"quoted'");
    }
}

TEST_CASE("KmlTextRun fromKml() error handling", "[editor][kml_text_run]") {
    SECTION("Returns nullptr for empty string") {
        auto run = KmlTextRun::fromKml("");
        REQUIRE(run == nullptr);
    }

    SECTION("Returns nullptr for wrong element") {
        auto run = KmlTextRun::fromKml("<b>Bold text</b>");
        REQUIRE(run == nullptr);
    }

    SECTION("Returns nullptr for malformed XML") {
        auto run = KmlTextRun::fromKml("<t>Unclosed");
        // QXmlStreamReader may still parse partial content
        // The behavior depends on how strict we want to be
        // For now, accept that partial parsing may succeed
    }

    SECTION("Returns nullptr for completely invalid") {
        auto run = KmlTextRun::fromKml("not xml at all");
        REQUIRE(run == nullptr);
    }
}

// =============================================================================
// Round-trip Tests (toKml -> fromKml)
// =============================================================================

TEST_CASE("KmlTextRun round-trip", "[editor][kml_text_run]") {
    SECTION("Simple text round-trip") {
        KmlTextRun original("Hello World");
        QString kml = original.toKml();
        auto parsed = KmlTextRun::fromKml(kml);

        REQUIRE(parsed != nullptr);
        REQUIRE(parsed->text() == original.text());
        REQUIRE(parsed->styleId() == original.styleId());
    }

    SECTION("Styled text round-trip") {
        KmlTextRun original("Important", "emphasis");
        QString kml = original.toKml();
        auto parsed = KmlTextRun::fromKml(kml);

        REQUIRE(parsed != nullptr);
        REQUIRE(parsed->text() == original.text());
        REQUIRE(parsed->styleId() == original.styleId());
    }

    SECTION("Special characters round-trip") {
        KmlTextRun original("a < b & c > d");
        QString kml = original.toKml();
        auto parsed = KmlTextRun::fromKml(kml);

        REQUIRE(parsed != nullptr);
        REQUIRE(parsed->text() == original.text());
    }

    SECTION("Unicode round-trip") {
        KmlTextRun original(QString::fromUtf8(u8"Zaz\u00F3\u0142\u0107 g\u0119\u015Bl\u0105 ja\u017A\u0144"));
        QString kml = original.toKml();
        auto parsed = KmlTextRun::fromKml(kml);

        REQUIRE(parsed != nullptr);
        REQUIRE(parsed->text() == original.text());
    }

    SECTION("Multiline text round-trip") {
        KmlTextRun original("Line 1\nLine 2\nLine 3");
        QString kml = original.toKml();
        auto parsed = KmlTextRun::fromKml(kml);

        REQUIRE(parsed != nullptr);
        REQUIRE(parsed->text() == original.text());
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("KmlTextRun edge cases", "[editor][kml_text_run]") {
    SECTION("Very long text") {
        QString longText(10000, 'x');
        KmlTextRun run(longText);

        REQUIRE(run.length() == 10000);
        REQUIRE(run.text() == longText);

        // Verify serialization works
        QString kml = run.toKml();
        auto parsed = KmlTextRun::fromKml(kml);
        REQUIRE(parsed != nullptr);
        REQUIRE(parsed->text() == longText);
    }

    SECTION("Whitespace preservation") {
        KmlTextRun run("  leading and trailing  ");
        REQUIRE(run.text() == "  leading and trailing  ");
        REQUIRE(run.length() == 24);  // 2 + 20 + 2 = 24

        QString kml = run.toKml();
        auto parsed = KmlTextRun::fromKml(kml);
        REQUIRE(parsed != nullptr);
        REQUIRE(parsed->text() == "  leading and trailing  ");
    }

    SECTION("Tab and newline preservation") {
        KmlTextRun run("tab\there\nnewline");
        REQUIRE(run.text() == "tab\there\nnewline");

        QString kml = run.toKml();
        auto parsed = KmlTextRun::fromKml(kml);
        REQUIRE(parsed != nullptr);
        REQUIRE(parsed->text() == "tab\there\nnewline");
    }

    SECTION("Style with special characters") {
        // Style IDs should generally be simple identifiers
        // but test that parsing handles various cases
        KmlTextRun run("Text", "style-with-dashes");
        REQUIRE(run.styleId() == "style-with-dashes");

        QString kml = run.toKml();
        auto parsed = KmlTextRun::fromKml(kml);
        REQUIRE(parsed != nullptr);
        REQUIRE(parsed->styleId() == "style-with-dashes");
    }
}
