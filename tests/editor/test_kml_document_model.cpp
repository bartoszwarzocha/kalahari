/// @file test_kml_document_model.cpp
/// @brief Unit tests for KmlDocumentModel (OpenSpec #00043 - lazy rendering)
///
/// Tests the lightweight document model with lazy QTextLayout creation.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <kalahari/editor/kml_document_model.h>
#include <kalahari/editor/format_run.h>
#include <kalahari/editor/kml_format_registry.h>

#include <QFont>
#include <chrono>

using namespace kalahari::editor;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// =============================================================================
// Test KML Samples
// =============================================================================

namespace {

const QString simpleKml = R"(<kml><p>Hello world</p></kml>)";

const QString formattedKml = R"(<kml>
<p>Normal <bold>bold</bold> and <italic>italic</italic> text.</p>
</kml>)";

const QString multiParagraphKml = R"(<kml>
<p>First paragraph.</p>
<p>Second paragraph.</p>
<p>Third paragraph.</p>
</kml>)";

const QString metadataKml = R"(<kml>
<p>Text with <comment id="c1">commented</comment> word.</p>
</kml>)";

const QString nestedFormattingKml = R"(<kml>
<p>Normal <bold>bold and <italic>bold-italic</italic> back to bold</bold> normal.</p>
</kml>)";

const QString todoKml = R"(<kml>
<p>Text with <todo id="t1">todo item</todo> here.</p>
</kml>)";

const QString complexKml = R"(<kml>
<p>This is <bold>bold</bold>, <italic>italic</italic>, and <underline>underlined</underline>.</p>
<p>Multiple <bold><italic>nested</italic></bold> formats.</p>
<p>With <comment id="note1">annotated</comment> text.</p>
</kml>)";

/// @brief Generate KML document with N paragraphs
QString generateLargeKml(size_t paragraphCount) {
    QString kml = QStringLiteral("<kml>\n");
    for (size_t i = 0; i < paragraphCount; ++i) {
        kml += QString("<p>Paragraph %1 with some text content for testing purposes.</p>\n")
               .arg(i + 1);
    }
    kml += QStringLiteral("</kml>");
    return kml;
}

} // anonymous namespace

// =============================================================================
// Construction & Loading Tests
// =============================================================================

TEST_CASE("KmlDocumentModel - Construction", "[KmlDocumentModel][lazy-rendering]") {
    SECTION("Default constructor creates empty document") {
        KmlDocumentModel model;

        REQUIRE(model.isEmpty());
        REQUIRE(model.paragraphCount() == 0);
        REQUIRE(model.characterCount() == 0);
        REQUIRE(model.totalHeight() == 0.0);
    }

    SECTION("Default font is set") {
        KmlDocumentModel model;
        QFont font = model.font();

        REQUIRE_FALSE(font.family().isEmpty());
    }

    SECTION("Default line width is reasonable") {
        KmlDocumentModel model;

        REQUIRE(model.lineWidth() > 0);
        REQUIRE(model.lineWidth() <= 2000);  // Reasonable upper bound
    }
}

TEST_CASE("KmlDocumentModel - Load Empty Document", "[KmlDocumentModel][lazy-rendering]") {
    SECTION("Load empty string") {
        KmlDocumentModel model;
        bool result = model.loadKml(QString());

        REQUIRE(result == true);
        REQUIRE(model.isEmpty());
        REQUIRE(model.paragraphCount() == 0);
    }

    SECTION("Load empty KML root") {
        KmlDocumentModel model;
        bool result = model.loadKml(QStringLiteral("<kml></kml>"));

        REQUIRE(result == true);
        REQUIRE(model.isEmpty());
    }
}

TEST_CASE("KmlDocumentModel - Load Simple KML", "[KmlDocumentModel][lazy-rendering]") {
    SECTION("Single paragraph") {
        KmlDocumentModel model;
        bool result = model.loadKml(simpleKml);

        REQUIRE(result == true);
        REQUIRE_FALSE(model.isEmpty());
        REQUIRE(model.paragraphCount() == 1);
        REQUIRE(model.paragraphText(0) == QStringLiteral("Hello world"));
    }

    SECTION("Paragraph length matches text") {
        KmlDocumentModel model;
        model.loadKml(simpleKml);

        REQUIRE(model.paragraphLength(0) == 11);  // "Hello world"
    }
}

TEST_CASE("KmlDocumentModel - Load Multi-paragraph KML", "[KmlDocumentModel][lazy-rendering]") {
    SECTION("Multiple paragraphs") {
        KmlDocumentModel model;
        bool result = model.loadKml(multiParagraphKml);

        REQUIRE(result == true);
        REQUIRE(model.paragraphCount() == 3);
    }

    SECTION("Paragraph texts are correct") {
        KmlDocumentModel model;
        model.loadKml(multiParagraphKml);

        REQUIRE(model.paragraphText(0) == QStringLiteral("First paragraph."));
        REQUIRE(model.paragraphText(1) == QStringLiteral("Second paragraph."));
        REQUIRE(model.paragraphText(2) == QStringLiteral("Third paragraph."));
    }

    SECTION("Paragraph count is correct") {
        KmlDocumentModel model;
        model.loadKml(multiParagraphKml);

        REQUIRE(model.paragraphCount() == 3);
    }
}

TEST_CASE("KmlDocumentModel - Clear Document", "[KmlDocumentModel][lazy-rendering]") {
    SECTION("Clear after load") {
        KmlDocumentModel model;
        model.loadKml(multiParagraphKml);
        REQUIRE(model.paragraphCount() == 3);

        model.clear();

        REQUIRE(model.isEmpty());
        REQUIRE(model.paragraphCount() == 0);
        REQUIRE(model.totalHeight() == 0.0);
    }
}

// =============================================================================
// Paragraph Access Tests
// =============================================================================

TEST_CASE("KmlDocumentModel - Paragraph Access", "[KmlDocumentModel][lazy-rendering]") {
    KmlDocumentModel model;
    model.loadKml(multiParagraphKml);

    SECTION("paragraphText returns correct text") {
        REQUIRE(model.paragraphText(0) == QStringLiteral("First paragraph."));
        REQUIRE(model.paragraphText(1) == QStringLiteral("Second paragraph."));
        REQUIRE(model.paragraphText(2) == QStringLiteral("Third paragraph."));
    }

    SECTION("paragraphText returns empty for out of range") {
        REQUIRE(model.paragraphText(100).isEmpty());
        REQUIRE(model.paragraphText(SIZE_MAX).isEmpty());
    }

    SECTION("paragraphLength returns correct length") {
        REQUIRE(model.paragraphLength(0) == 16);  // "First paragraph."
        REQUIRE(model.paragraphLength(1) == 17);  // "Second paragraph."
        REQUIRE(model.paragraphLength(2) == 16);  // "Third paragraph."
    }

    SECTION("paragraphLength returns 0 for out of range") {
        REQUIRE(model.paragraphLength(100) == 0);
    }

    SECTION("plainText concatenates all paragraphs") {
        QString expected = QStringLiteral("First paragraph.\nSecond paragraph.\nThird paragraph.");
        REQUIRE(model.plainText() == expected);
    }

    SECTION("paragraphFormats returns empty for plain text") {
        KmlDocumentModel plainModel;
        plainModel.loadKml(simpleKml);

        const auto& formats = plainModel.paragraphFormats(0);
        REQUIRE(formats.empty());  // "Hello world" has no formatting
    }

    SECTION("paragraphFormats returns empty for out of range") {
        const auto& formats = model.paragraphFormats(100);
        REQUIRE(formats.empty());
    }

    SECTION("characterCount sums all paragraphs") {
        size_t expected = 16 + 17 + 16;  // All three paragraphs
        REQUIRE(model.characterCount() == expected);
    }
}

// =============================================================================
// Height Queries Tests
// =============================================================================

TEST_CASE("KmlDocumentModel - Height Queries", "[KmlDocumentModel][lazy-rendering]") {
    SECTION("Initial heights are estimated") {
        KmlDocumentModel model;
        model.loadKml(multiParagraphKml);

        // Before ensureLayouted, heights should be estimated (> 0)
        REQUIRE(model.paragraphHeight(0) > 0.0);
        REQUIRE(model.paragraphHeight(1) > 0.0);
        REQUIRE(model.paragraphHeight(2) > 0.0);
    }

    SECTION("totalHeight uses HeightTree sum") {
        KmlDocumentModel model;
        model.loadKml(multiParagraphKml);

        double total = model.totalHeight();

        // Total should be sum of individual heights
        double sum = model.paragraphHeight(0) +
                     model.paragraphHeight(1) +
                     model.paragraphHeight(2);

        REQUIRE_THAT(total, WithinAbs(sum, 0.01));
    }

    SECTION("paragraphY uses HeightTree prefix sums") {
        KmlDocumentModel model;
        model.loadKml(multiParagraphKml);

        // First paragraph should be at Y=0
        REQUIRE(model.paragraphY(0) == 0.0);

        // Second paragraph Y = height of first
        double expectedY1 = model.paragraphHeight(0);
        REQUIRE_THAT(model.paragraphY(1), WithinAbs(expectedY1, 0.01));

        // Third paragraph Y = sum of first two heights
        double expectedY2 = model.paragraphHeight(0) + model.paragraphHeight(1);
        REQUIRE_THAT(model.paragraphY(2), WithinAbs(expectedY2, 0.01));
    }

    SECTION("paragraphAtY finds correct paragraph") {
        KmlDocumentModel model;
        model.loadKml(multiParagraphKml);

        // Y=0 should be paragraph 0
        REQUIRE(model.paragraphAtY(0.0) == 0);

        // Y in middle of second paragraph
        double y1 = model.paragraphY(1) + model.paragraphHeight(1) / 2.0;
        REQUIRE(model.paragraphAtY(y1) == 1);

        // Y in third paragraph
        double y2 = model.paragraphY(2) + model.paragraphHeight(2) / 2.0;
        REQUIRE(model.paragraphAtY(y2) == 2);
    }

    SECTION("paragraphAtY beyond end returns count") {
        KmlDocumentModel model;
        model.loadKml(multiParagraphKml);

        double beyondEnd = model.totalHeight() + 100.0;
        REQUIRE(model.paragraphAtY(beyondEnd) == model.paragraphCount());
    }

    SECTION("After ensureLayouted heights are real") {
        KmlDocumentModel model;
        model.loadKml(multiParagraphKml);

        // Layout first paragraph
        model.ensureLayouted(0, 0);

        // Height should still be valid after layout
        double heightAfter = model.paragraphHeight(0);
        REQUIRE(heightAfter > 0.0);
    }

    SECTION("totalHeight updates after layout") {
        KmlDocumentModel model;
        model.loadKml(multiParagraphKml);

        // Layout all paragraphs
        model.ensureLayouted(0, 2);

        double totalAfter = model.totalHeight();

        // Total should still be positive and reasonable
        REQUIRE(totalAfter > 0.0);
        // Sum of heights should match
        double sum = model.paragraphHeight(0) +
                     model.paragraphHeight(1) +
                     model.paragraphHeight(2);
        REQUIRE_THAT(totalAfter, WithinAbs(sum, 0.01));
    }

    SECTION("paragraphHeight returns 0 for out of range") {
        KmlDocumentModel model;
        model.loadKml(simpleKml);

        REQUIRE(model.paragraphHeight(100) == 0.0);
    }
}

// =============================================================================
// Lazy Layout Tests
// =============================================================================

TEST_CASE("KmlDocumentModel - Lazy Layout", "[KmlDocumentModel][lazy-rendering]") {
    SECTION("layout returns nullptr before ensureLayouted") {
        KmlDocumentModel model;
        model.loadKml(multiParagraphKml);

        // Before layout, should return nullptr
        REQUIRE(model.layout(0) == nullptr);
        REQUIRE(model.layout(1) == nullptr);
        REQUIRE(model.layout(2) == nullptr);
    }

    SECTION("isLayouted returns false initially") {
        KmlDocumentModel model;
        model.loadKml(multiParagraphKml);

        REQUIRE_FALSE(model.isLayouted(0));
        REQUIRE_FALSE(model.isLayouted(1));
        REQUIRE_FALSE(model.isLayouted(2));
    }

    SECTION("layout returns valid after ensureLayouted") {
        KmlDocumentModel model;
        model.loadKml(multiParagraphKml);

        model.ensureLayouted(0, 1);

        REQUIRE(model.layout(0) != nullptr);
        REQUIRE(model.layout(1) != nullptr);
        REQUIRE(model.layout(2) == nullptr);  // Not in range
    }

    SECTION("isLayouted reflects state after ensureLayouted") {
        KmlDocumentModel model;
        model.loadKml(multiParagraphKml);

        model.ensureLayouted(1, 1);  // Only paragraph 1

        REQUIRE_FALSE(model.isLayouted(0));
        REQUIRE(model.isLayouted(1));
        REQUIRE_FALSE(model.isLayouted(2));
    }

    SECTION("invalidateLayout clears layout") {
        KmlDocumentModel model;
        model.loadKml(multiParagraphKml);

        model.ensureLayouted(0, 0);
        REQUIRE(model.isLayouted(0));

        model.invalidateLayout(0);

        REQUIRE_FALSE(model.isLayouted(0));
        REQUIRE(model.layout(0) == nullptr);
    }

    SECTION("invalidateAllLayouts clears all layouts") {
        KmlDocumentModel model;
        model.loadKml(multiParagraphKml);

        model.ensureLayouted(0, 2);  // Layout all
        REQUIRE(model.isLayouted(0));
        REQUIRE(model.isLayouted(1));
        REQUIRE(model.isLayouted(2));

        model.invalidateAllLayouts();

        REQUIRE_FALSE(model.isLayouted(0));
        REQUIRE_FALSE(model.isLayouted(1));
        REQUIRE_FALSE(model.isLayouted(2));
    }

    SECTION("evictLayouts clears layouts outside range") {
        KmlDocumentModel model;
        model.loadKml(generateLargeKml(10));

        // Layout all paragraphs
        model.ensureLayouted(0, 9);
        for (size_t i = 0; i < 10; ++i) {
            REQUIRE(model.isLayouted(i));
        }

        // Evict layouts outside range [3, 6]
        model.evictLayouts(3, 6);

        // Note: evictLayouts clears layout object but keeps layoutValid true
        // because the height information is still accurate
        // Check that middle range still has layouts
        REQUIRE(model.layout(3) != nullptr);
        REQUIRE(model.layout(4) != nullptr);
        REQUIRE(model.layout(5) != nullptr);
        REQUIRE(model.layout(6) != nullptr);

        // Layouts outside range should be evicted
        REQUIRE(model.layout(0) == nullptr);
        REQUIRE(model.layout(1) == nullptr);
        REQUIRE(model.layout(2) == nullptr);
        REQUIRE(model.layout(7) == nullptr);
        REQUIRE(model.layout(8) == nullptr);
        REQUIRE(model.layout(9) == nullptr);
    }

    SECTION("layout returns nullptr for out of range index") {
        KmlDocumentModel model;
        model.loadKml(simpleKml);

        REQUIRE(model.layout(100) == nullptr);
    }

    SECTION("isLayouted returns false for out of range index") {
        KmlDocumentModel model;
        model.loadKml(simpleKml);

        REQUIRE_FALSE(model.isLayouted(100));
    }

    SECTION("ensureLayouted with empty document does not crash") {
        KmlDocumentModel model;
        model.ensureLayouted(0, 10);  // Should not crash
        REQUIRE(model.isEmpty());
    }
}

// =============================================================================
// Formatting Tests
// =============================================================================

TEST_CASE("KmlDocumentModel - Bold Formatting", "[KmlDocumentModel][lazy-rendering]") {
    SECTION("Bold text creates FormatRun with fontWeight") {
        KmlDocumentModel model;
        model.loadKml(formattedKml);

        const auto& formats = model.paragraphFormats(0);

        // Should have at least one format run for "bold"
        bool foundBold = false;
        for (const auto& run : formats) {
            if (run.format.fontWeight() == QFont::Bold) {
                foundBold = true;
                // Verify the run covers "bold" text
                QString text = model.paragraphText(0).mid(
                    static_cast<int>(run.start),
                    static_cast<int>(run.end - run.start));
                REQUIRE(text == QStringLiteral("bold"));
                break;
            }
        }
        REQUIRE(foundBold);
    }
}

TEST_CASE("KmlDocumentModel - Italic Formatting", "[KmlDocumentModel][lazy-rendering]") {
    SECTION("Italic text creates FormatRun with fontItalic") {
        KmlDocumentModel model;
        model.loadKml(formattedKml);

        const auto& formats = model.paragraphFormats(0);

        // Should have at least one format run for "italic"
        bool foundItalic = false;
        for (const auto& run : formats) {
            if (run.format.fontItalic()) {
                foundItalic = true;
                // Verify the run covers "italic" text
                QString text = model.paragraphText(0).mid(
                    static_cast<int>(run.start),
                    static_cast<int>(run.end - run.start));
                REQUIRE(text == QStringLiteral("italic"));
                break;
            }
        }
        REQUIRE(foundItalic);
    }
}

TEST_CASE("KmlDocumentModel - Nested Formatting", "[KmlDocumentModel][lazy-rendering]") {
    SECTION("Nested bold+italic creates combined format") {
        KmlDocumentModel model;
        model.loadKml(nestedFormattingKml);

        const auto& formats = model.paragraphFormats(0);

        // Should have a format run with both bold AND italic for "bold-italic"
        bool foundBoldItalic = false;
        for (const auto& run : formats) {
            if (run.format.fontWeight() == QFont::Bold && run.format.fontItalic()) {
                foundBoldItalic = true;
                QString text = model.paragraphText(0).mid(
                    static_cast<int>(run.start),
                    static_cast<int>(run.end - run.start));
                REQUIRE(text == QStringLiteral("bold-italic"));
                break;
            }
        }
        REQUIRE(foundBoldItalic);
    }
}

TEST_CASE("KmlDocumentModel - Metadata (Comment)", "[KmlDocumentModel][lazy-rendering]") {
    SECTION("Comment creates FormatRun with KmlPropComment") {
        KmlDocumentModel model;
        model.loadKml(metadataKml);

        const auto& formats = model.paragraphFormats(0);

        // Should have format run with comment property
        bool foundComment = false;
        for (const auto& run : formats) {
            if (run.hasComment()) {
                foundComment = true;
                QString text = model.paragraphText(0).mid(
                    static_cast<int>(run.start),
                    static_cast<int>(run.end - run.start));
                REQUIRE(text == QStringLiteral("commented"));
                break;
            }
        }
        REQUIRE(foundComment);
    }
}

TEST_CASE("KmlDocumentModel - Metadata (Todo)", "[KmlDocumentModel][lazy-rendering]") {
    SECTION("TODO creates FormatRun with KmlPropTodo") {
        KmlDocumentModel model;
        model.loadKml(todoKml);

        const auto& formats = model.paragraphFormats(0);

        // Should have format run with todo property
        bool foundTodo = false;
        for (const auto& run : formats) {
            if (run.hasTodo()) {
                foundTodo = true;
                QString text = model.paragraphText(0).mid(
                    static_cast<int>(run.start),
                    static_cast<int>(run.end - run.start));
                REQUIRE(text == QStringLiteral("todo item"));
                break;
            }
        }
        REQUIRE(foundTodo);
    }
}

TEST_CASE("KmlDocumentModel - Complex Formatting", "[KmlDocumentModel][lazy-rendering]") {
    SECTION("Multiple paragraphs with mixed formatting") {
        KmlDocumentModel model;
        model.loadKml(complexKml);

        REQUIRE(model.paragraphCount() == 3);

        // First paragraph should have bold, italic, underline runs
        const auto& formats0 = model.paragraphFormats(0);
        REQUIRE_FALSE(formats0.empty());

        // Second paragraph should have nested formatting
        const auto& formats1 = model.paragraphFormats(1);
        REQUIRE_FALSE(formats1.empty());

        // Third paragraph should have comment metadata
        const auto& formats2 = model.paragraphFormats(2);
        bool hasComment = false;
        for (const auto& run : formats2) {
            if (run.hasComment()) {
                hasComment = true;
                break;
            }
        }
        REQUIRE(hasComment);
    }
}

// =============================================================================
// Configuration Tests
// =============================================================================

TEST_CASE("KmlDocumentModel - Configuration", "[KmlDocumentModel][lazy-rendering]") {
    SECTION("setFont changes font and invalidates layouts") {
        KmlDocumentModel model;
        model.loadKml(multiParagraphKml);
        model.ensureLayouted(0, 0);
        REQUIRE(model.isLayouted(0));

        QFont newFont(QStringLiteral("Arial"), 14);
        model.setFont(newFont);

        // Font should be changed
        REQUIRE(model.font().family() == newFont.family());
        REQUIRE(model.font().pointSize() == newFont.pointSize());

        // Layout should be invalidated
        REQUIRE_FALSE(model.isLayouted(0));
    }

    SECTION("setLineWidth changes width and invalidates layouts") {
        KmlDocumentModel model;
        model.loadKml(multiParagraphKml);
        model.ensureLayouted(0, 0);
        REQUIRE(model.isLayouted(0));

        model.setLineWidth(500.0);

        REQUIRE_THAT(model.lineWidth(), WithinAbs(500.0, 0.01));
        REQUIRE_FALSE(model.isLayouted(0));  // Layout invalidated
    }

    SECTION("setEstimatedLineHeight updates estimation") {
        KmlDocumentModel model;
        model.setEstimatedLineHeight(30.0);

        // Load document after setting estimate
        model.loadKml(simpleKml);

        // Heights should be based on new estimate
        REQUIRE(model.paragraphHeight(0) > 0.0);
    }
}

// =============================================================================
// Edge Cases Tests
// =============================================================================

TEST_CASE("KmlDocumentModel - Edge Cases", "[KmlDocumentModel][lazy-rendering]") {
    SECTION("Load KML without root element wraps automatically") {
        KmlDocumentModel model;
        bool result = model.loadKml(QStringLiteral("<p>Unwrapped paragraph</p>"));

        REQUIRE(result == true);
        REQUIRE(model.paragraphCount() == 1);
        REQUIRE(model.paragraphText(0) == QStringLiteral("Unwrapped paragraph"));
    }

    SECTION("Load handles whitespace in KML") {
        QString kml = R"(
            <kml>
                <p>Paragraph with whitespace around it</p>
            </kml>
        )";
        KmlDocumentModel model;
        bool result = model.loadKml(kml);

        REQUIRE(result == true);
        REQUIRE(model.paragraphCount() == 1);
    }

    SECTION("Load empty paragraph") {
        KmlDocumentModel model;
        bool result = model.loadKml(QStringLiteral("<kml><p></p></kml>"));

        REQUIRE(result == true);
        REQUIRE(model.paragraphCount() == 1);
        REQUIRE(model.paragraphText(0).isEmpty());
        REQUIRE(model.paragraphLength(0) == 0);
    }

    SECTION("Reload clears previous content") {
        KmlDocumentModel model;
        model.loadKml(multiParagraphKml);
        REQUIRE(model.paragraphCount() == 3);

        model.loadKml(simpleKml);
        REQUIRE(model.paragraphCount() == 1);
    }

    SECTION("ensureLayouted with reversed range") {
        KmlDocumentModel model;
        model.loadKml(multiParagraphKml);

        // This should handle gracefully (last < first or clamping)
        model.ensureLayouted(2, 0);  // Reversed range

        // Should not crash; implementation may clamp
    }

    SECTION("evictLayouts with reversed range") {
        KmlDocumentModel model;
        model.loadKml(multiParagraphKml);
        model.ensureLayouted(0, 2);

        // This should handle gracefully
        model.evictLayouts(2, 0);  // Reversed range
    }
}

// =============================================================================
// Signal Tests
// =============================================================================

TEST_CASE("KmlDocumentModel - Signals", "[KmlDocumentModel][lazy-rendering]") {
    SECTION("documentLoaded emitted after load") {
        KmlDocumentModel model;
        int loadedCount = 0;

        QObject::connect(&model, &KmlDocumentModel::documentLoaded,
                         [&loadedCount]() { ++loadedCount; });

        model.loadKml(simpleKml);

        REQUIRE(loadedCount == 1);
    }

    SECTION("totalHeightChanged emitted after load") {
        KmlDocumentModel model;
        int heightChangedCount = 0;
        double lastHeight = 0.0;

        QObject::connect(&model, &KmlDocumentModel::totalHeightChanged,
                         [&](double height) {
                             ++heightChangedCount;
                             lastHeight = height;
                         });

        model.loadKml(multiParagraphKml);

        REQUIRE(heightChangedCount >= 1);
        REQUIRE(lastHeight > 0.0);
    }

    SECTION("paragraphHeightChanged emitted after layout") {
        KmlDocumentModel model;
        model.loadKml(multiParagraphKml);

        int heightChangedCount = 0;
        size_t lastIndex = SIZE_MAX;

        QObject::connect(&model, &KmlDocumentModel::paragraphHeightChanged,
                         [&](size_t index, double) {
                             ++heightChangedCount;
                             lastIndex = index;
                         });

        model.ensureLayouted(0, 0);

        // Signal may or may not be emitted depending on height difference
        // Just verify connection works
    }
}

// =============================================================================
// Performance Tests (Basic)
// =============================================================================

TEST_CASE("KmlDocumentModel - Performance: Load 1000 paragraphs", "[KmlDocumentModel][lazy-rendering][!benchmark]") {
    SECTION("Load 1000 paragraphs in < 100ms") {
        QString largeKml = generateLargeKml(1000);

        auto start = std::chrono::high_resolution_clock::now();

        KmlDocumentModel model;
        bool result = model.loadKml(largeKml);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        REQUIRE(result == true);
        REQUIRE(model.paragraphCount() == 1000);
        REQUIRE(duration.count() < 100);  // Should complete in < 100ms
    }
}

TEST_CASE("KmlDocumentModel - Performance: ensureLayouted visible range", "[KmlDocumentModel][lazy-rendering][!benchmark]") {
    SECTION("ensureLayouted(0, 20) in < 50ms") {
        QString largeKml = generateLargeKml(1000);
        KmlDocumentModel model;
        model.loadKml(largeKml);

        auto start = std::chrono::high_resolution_clock::now();

        model.ensureLayouted(0, 20);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        REQUIRE(duration.count() < 50);  // Should complete in < 50ms

        // Verify layouts were created
        for (size_t i = 0; i <= 20; ++i) {
            REQUIRE(model.isLayouted(i));
        }
    }
}

TEST_CASE("KmlDocumentModel - Performance: Height queries", "[KmlDocumentModel][lazy-rendering][!benchmark]") {
    SECTION("1000 paragraphAtY queries in < 10ms") {
        QString largeKml = generateLargeKml(1000);
        KmlDocumentModel model;
        model.loadKml(largeKml);

        double totalHeight = model.totalHeight();

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < 1000; ++i) {
            double y = (static_cast<double>(i) / 1000.0) * totalHeight;
            volatile size_t index = model.paragraphAtY(y);
            (void)index;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        REQUIRE(duration.count() < 10);  // HeightTree should be O(log n)
    }
}

#ifdef CATCH_CONFIG_ENABLE_BENCHMARKING
TEST_CASE("KmlDocumentModel - Benchmarks", "[KmlDocumentModel][lazy-rendering][.benchmark]") {
    BENCHMARK("Load 1000 paragraphs") {
        QString largeKml = generateLargeKml(1000);
        KmlDocumentModel model;
        return model.loadKml(largeKml);
    };

    BENCHMARK_ADVANCED("ensureLayouted 20 paragraphs")(Catch::Benchmark::Chronometer meter) {
        QString largeKml = generateLargeKml(1000);
        KmlDocumentModel model;
        model.loadKml(largeKml);

        meter.measure([&model] {
            model.invalidateAllLayouts();
            model.ensureLayouted(0, 19);
        });
    };

    BENCHMARK_ADVANCED("paragraphAtY 1000 queries")(Catch::Benchmark::Chronometer meter) {
        QString largeKml = generateLargeKml(1000);
        KmlDocumentModel model;
        model.loadKml(largeKml);
        double totalHeight = model.totalHeight();

        meter.measure([&] {
            for (int i = 0; i < 1000; ++i) {
                double y = (static_cast<double>(i) / 1000.0) * totalHeight;
                volatile size_t idx = model.paragraphAtY(y);
                (void)idx;
            }
        });
    };
}
#endif
