/// @file test_kml_document_model.cpp
/// @brief Unit tests for KmlDocumentModel, the KML reader
///
/// Tests reading KML into paragraphs: text, format runs and metadata.

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <kalahari/editor/kml_document_model.h>
#include <kalahari/editor/format_run.h>
#include <kalahari/editor/kml_format_registry.h>

#include <QFont>
#include <chrono>

using namespace kalahari::editor;

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

TEST_CASE("KmlDocumentModel - Construction", "[editor][KmlDocumentModel]") {
    SECTION("Default constructor creates empty document") {
        KmlDocumentModel model;

        REQUIRE(model.isEmpty());
        REQUIRE(model.paragraphCount() == 0);
    }
}

TEST_CASE("KmlDocumentModel - Load Empty Document", "[editor][KmlDocumentModel]") {
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

TEST_CASE("KmlDocumentModel - Load Simple KML", "[editor][KmlDocumentModel]") {
    SECTION("Single paragraph") {
        KmlDocumentModel model;
        bool result = model.loadKml(simpleKml);

        REQUIRE(result == true);
        REQUIRE_FALSE(model.isEmpty());
        REQUIRE(model.paragraphCount() == 1);
        REQUIRE(model.paragraphText(0) == QStringLiteral("Hello world"));
    }
}

TEST_CASE("KmlDocumentModel - Load Multi-paragraph KML", "[editor][KmlDocumentModel]") {
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
}

TEST_CASE("KmlDocumentModel - Clear Document", "[editor][KmlDocumentModel]") {
    SECTION("Clear after load") {
        KmlDocumentModel model;
        model.loadKml(multiParagraphKml);
        REQUIRE(model.paragraphCount() == 3);

        model.clear();

        REQUIRE(model.isEmpty());
        REQUIRE(model.paragraphCount() == 0);
    }
}

TEST_CASE("KmlDocumentModel - Unreadable KML", "[editor][KmlDocumentModel]") {
    SECTION("Paragraphs read before the error are kept") {
        KmlDocumentModel model;
        bool result = model.loadKml(QStringLiteral("<p>Readable</p><p><b>Broken</i></p><p>Lost</p>"));

        REQUIRE(result == false);
        REQUIRE(model.paragraphCount() >= 1);
        REQUIRE(model.paragraphText(0) == QStringLiteral("Readable"));
    }

    SECTION("Loading again replaces them") {
        KmlDocumentModel model;
        model.loadKml(QStringLiteral("<invalid<<<"));
        bool result = model.loadKml(simpleKml);

        REQUIRE(result == true);
        REQUIRE(model.paragraphCount() == 1);
        REQUIRE(model.paragraphText(0) == QStringLiteral("Hello world"));
    }
}

// =============================================================================
// Paragraph Access Tests
// =============================================================================

TEST_CASE("KmlDocumentModel - Paragraph Access", "[editor][KmlDocumentModel]") {
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

    SECTION("paragraphAlignment returns none without an align attribute") {
        REQUIRE_FALSE(model.paragraphAlignment(0));
        REQUIRE_FALSE(model.paragraphAlignment(100));
    }

    SECTION("paragraphAlignment returns the align attribute") {
        KmlDocumentModel aligned;
        aligned.loadKml(QStringLiteral("<p align=\"center\">Title</p><p align=\"right\">Signed</p>"));

        REQUIRE(aligned.paragraphAlignment(0) == Qt::AlignHCenter);
        REQUIRE(aligned.paragraphAlignment(1) == Qt::AlignRight);
    }
}

// =============================================================================
// Formatting Tests
// =============================================================================

TEST_CASE("KmlDocumentModel - Bold Formatting", "[editor][KmlDocumentModel]") {
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

TEST_CASE("KmlDocumentModel - Italic Formatting", "[editor][KmlDocumentModel]") {
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

TEST_CASE("KmlDocumentModel - Nested Formatting", "[editor][KmlDocumentModel]") {
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

TEST_CASE("KmlDocumentModel - Metadata (Comment)", "[editor][KmlDocumentModel]") {
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

TEST_CASE("KmlDocumentModel - Metadata (Todo)", "[editor][KmlDocumentModel]") {
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

TEST_CASE("KmlDocumentModel - Complex Formatting", "[editor][KmlDocumentModel]") {
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
// Edge Cases Tests
// =============================================================================

TEST_CASE("KmlDocumentModel - Edge Cases", "[editor][KmlDocumentModel]") {
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
    }

    SECTION("Reload clears previous content") {
        KmlDocumentModel model;
        model.loadKml(multiParagraphKml);
        REQUIRE(model.paragraphCount() == 3);

        model.loadKml(simpleKml);
        REQUIRE(model.paragraphCount() == 1);
    }
}

// =============================================================================
// Performance Tests (Basic)
// =============================================================================

TEST_CASE("KmlDocumentModel - Performance: Load 1000 paragraphs", "[editor][KmlDocumentModel][!benchmark]") {
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

#ifdef CATCH_CONFIG_ENABLE_BENCHMARKING
TEST_CASE("KmlDocumentModel - Benchmarks", "[editor][KmlDocumentModel][.benchmark]") {
    BENCHMARK("Load 1000 paragraphs") {
        QString largeKml = generateLargeKml(1000);
        KmlDocumentModel model;
        return model.loadKml(largeKml);
    };
}
#endif
