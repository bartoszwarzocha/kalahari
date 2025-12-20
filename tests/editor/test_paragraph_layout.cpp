/// @file test_paragraph_layout.cpp
/// @brief Unit tests for ParagraphLayout (OpenSpec #00042 Phase 2.1/2.2/2.5)

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/paragraph_layout.h>
#include <kalahari/editor/format_converter.h>
#include <kalahari/editor/kml_paragraph.h>
#include <kalahari/editor/kml_text_run.h>
#include <kalahari/editor/kml_inline_elements.h>
#include <QFont>
#include <QFontDatabase>
#include <QTextCharFormat>
#include <QTextLine>
#include <QImage>
#include <QPainter>
#include <QColor>
#include <cmath>

using namespace kalahari::editor;

// =============================================================================
// Constructor Tests
// =============================================================================

TEST_CASE("ParagraphLayout default constructor", "[editor][paragraph_layout]") {
    ParagraphLayout layout;

    SECTION("Initial state") {
        REQUIRE(layout.text().isEmpty());
        REQUIRE(layout.isDirty());
        REQUIRE(layout.height() == 0.0);
        REQUIRE(layout.lineCount() == 0);
        REQUIRE(layout.layoutWidth() == 0.0);
    }

    SECTION("Bounding rect is empty before layout") {
        REQUIRE(layout.boundingRect().isEmpty());
    }
}

TEST_CASE("ParagraphLayout constructor with text", "[editor][paragraph_layout]") {
    ParagraphLayout layout("Hello, world!");

    SECTION("Has text") {
        REQUIRE(layout.text() == "Hello, world!");
    }

    SECTION("Is dirty before layout") {
        REQUIRE(layout.isDirty());
    }

    SECTION("No height before layout") {
        REQUIRE(layout.height() == 0.0);
    }
}

TEST_CASE("ParagraphLayout constructor with text and font", "[editor][paragraph_layout]") {
    QFont font("Serif", 14);
    ParagraphLayout layout("Test text", font);

    SECTION("Has text and font") {
        REQUIRE(layout.text() == "Test text");
        REQUIRE(layout.font().pointSize() == 14);
    }

    SECTION("Is dirty before layout") {
        REQUIRE(layout.isDirty());
    }
}

// =============================================================================
// Text and Font Tests
// =============================================================================

TEST_CASE("ParagraphLayout setText", "[editor][paragraph_layout]") {
    ParagraphLayout layout;

    SECTION("Set text marks dirty") {
        layout.setText("Hello");
        REQUIRE(layout.text() == "Hello");
        REQUIRE(layout.isDirty());
    }

    SECTION("Set same text does not mark dirty") {
        layout.setText("Hello");
        layout.doLayout(500.0);
        REQUIRE(layout.isDirty() == false);

        layout.setText("Hello");  // Same text
        REQUIRE(layout.isDirty() == false);
    }

    SECTION("Set different text marks dirty") {
        layout.setText("Hello");
        layout.doLayout(500.0);
        REQUIRE(layout.isDirty() == false);

        layout.setText("World");
        REQUIRE(layout.isDirty());
    }
}

TEST_CASE("ParagraphLayout setFont", "[editor][paragraph_layout]") {
    ParagraphLayout layout("Test");

    SECTION("Set font marks dirty") {
        QFont font("Serif", 16);
        layout.setFont(font);
        REQUIRE(layout.font().pointSize() == 16);
        REQUIRE(layout.isDirty());
    }

    SECTION("Set same font does not mark dirty") {
        QFont font("Serif", 16);
        layout.setFont(font);
        layout.doLayout(500.0);
        REQUIRE(layout.isDirty() == false);

        layout.setFont(font);  // Same font
        REQUIRE(layout.isDirty() == false);
    }

    SECTION("Set different font marks dirty") {
        QFont font1("Serif", 12);
        QFont font2("Serif", 16);

        layout.setFont(font1);
        layout.doLayout(500.0);
        REQUIRE(layout.isDirty() == false);

        layout.setFont(font2);
        REQUIRE(layout.isDirty());
    }
}

// =============================================================================
// Layout Operation Tests
// =============================================================================

TEST_CASE("ParagraphLayout doLayout basic", "[editor][paragraph_layout]") {
    ParagraphLayout layout("Hello, world!");

    SECTION("Layout returns positive height") {
        qreal height = layout.doLayout(500.0);
        REQUIRE(height > 0.0);
    }

    SECTION("Layout clears dirty flag") {
        REQUIRE(layout.isDirty());
        layout.doLayout(500.0);
        REQUIRE(layout.isDirty() == false);
    }

    SECTION("Layout stores width") {
        layout.doLayout(500.0);
        REQUIRE(layout.layoutWidth() == 500.0);
    }

    SECTION("Layout updates height") {
        layout.doLayout(500.0);
        REQUIRE(layout.height() > 0.0);
    }
}

TEST_CASE("ParagraphLayout doLayout caching", "[editor][paragraph_layout]") {
    ParagraphLayout layout("Hello, world!");

    SECTION("Same width uses cached result") {
        qreal height1 = layout.doLayout(500.0);
        REQUIRE(layout.isDirty() == false);

        qreal height2 = layout.doLayout(500.0);
        REQUIRE(height1 == height2);
        REQUIRE(layout.isDirty() == false);
    }

    SECTION("Different width triggers re-layout") {
        layout.doLayout(500.0);

        layout.doLayout(100.0);  // Much narrower

        // Layout width should be updated
        REQUIRE(layout.layoutWidth() == 100.0);
    }
}

TEST_CASE("ParagraphLayout doLayout with empty text", "[editor][paragraph_layout]") {
    ParagraphLayout layout;

    SECTION("Empty text has valid height") {
        qreal height = layout.doLayout(500.0);
        // QTextLayout may return line height even for empty text
        REQUIRE(height >= 0.0);
    }

    SECTION("Empty text line count") {
        layout.doLayout(500.0);
        // QTextLayout may create a line for empty text
        REQUIRE(layout.lineCount() >= 0);
    }
}

TEST_CASE("ParagraphLayout doLayout with long text", "[editor][paragraph_layout]") {
    QString longText = "This is a very long paragraph of text that should wrap "
                       "across multiple lines when laid out in a narrow width. "
                       "We want to test that the layout engine correctly handles "
                       "word wrapping and line height calculations.";
    ParagraphLayout layout(longText);

    SECTION("Narrow width creates multiple lines") {
        layout.doLayout(100.0);
        REQUIRE(layout.lineCount() > 1);
    }

    SECTION("Wide width creates fewer lines") {
        layout.doLayout(100.0);
        int narrowLineCount = layout.lineCount();

        layout.invalidate();
        layout.doLayout(1000.0);
        int wideLineCount = layout.lineCount();

        REQUIRE(wideLineCount < narrowLineCount);
    }
}

// =============================================================================
// Invalidate and Clear Tests
// =============================================================================

TEST_CASE("ParagraphLayout invalidate", "[editor][paragraph_layout]") {
    ParagraphLayout layout("Test");
    layout.doLayout(500.0);

    SECTION("Invalidate marks dirty") {
        REQUIRE(layout.isDirty() == false);
        layout.invalidate();
        REQUIRE(layout.isDirty());
    }

    SECTION("Invalidate preserves cached geometry until re-layout") {
        qreal heightBefore = layout.height();
        layout.invalidate();
        // Height is still accessible even when dirty
        REQUIRE(layout.height() == heightBefore);
    }
}

TEST_CASE("ParagraphLayout clear", "[editor][paragraph_layout]") {
    ParagraphLayout layout("Test text");
    layout.setFont(QFont("Serif", 16));
    layout.doLayout(500.0);

    layout.clear();

    SECTION("Clear resets text") {
        REQUIRE(layout.text().isEmpty());
    }

    SECTION("Clear resets dimensions") {
        REQUIRE(layout.height() == 0.0);
        REQUIRE(layout.layoutWidth() == 0.0);
    }

    SECTION("Clear marks dirty") {
        REQUIRE(layout.isDirty());
    }
}

// =============================================================================
// Geometry Tests
// =============================================================================

TEST_CASE("ParagraphLayout geometry", "[editor][paragraph_layout]") {
    ParagraphLayout layout("Hello, world!");
    layout.doLayout(500.0);

    SECTION("Height is positive") {
        REQUIRE(layout.height() > 0.0);
    }

    SECTION("Line count is at least 1") {
        REQUIRE(layout.lineCount() >= 1);
    }

    SECTION("Bounding rect has dimensions") {
        QRectF rect = layout.boundingRect();
        REQUIRE(rect.width() > 0.0);
        REQUIRE(rect.height() > 0.0);
    }
}

TEST_CASE("ParagraphLayout lineRect", "[editor][paragraph_layout]") {
    QString longText = "Line one of text. Line two of text. Line three of text. "
                       "Line four of text. Line five of text.";
    ParagraphLayout layout(longText);
    layout.doLayout(100.0);

    SECTION("Valid line index returns non-empty rect") {
        REQUIRE(layout.lineCount() > 0);
        QRectF rect = layout.lineRect(0);
        REQUIRE(rect.isEmpty() == false);
    }

    SECTION("Negative index returns empty rect") {
        QRectF rect = layout.lineRect(-1);
        REQUIRE(rect.isEmpty());
    }

    SECTION("Out of bounds index returns empty rect") {
        QRectF rect = layout.lineRect(1000);
        REQUIRE(rect.isEmpty());
    }

    SECTION("Lines are stacked vertically") {
        if (layout.lineCount() >= 2) {
            QRectF line0 = layout.lineRect(0);
            QRectF line1 = layout.lineRect(1);
            REQUIRE(line1.top() >= line0.bottom());
        }
    }
}

TEST_CASE("ParagraphLayout geometry when dirty", "[editor][paragraph_layout]") {
    ParagraphLayout layout("Test");

    SECTION("lineCount returns 0 when dirty") {
        REQUIRE(layout.isDirty());
        REQUIRE(layout.lineCount() == 0);
    }

    SECTION("boundingRect returns empty when dirty") {
        REQUIRE(layout.isDirty());
        REQUIRE(layout.boundingRect().isEmpty());
    }

    SECTION("lineRect returns empty when dirty") {
        REQUIRE(layout.isDirty());
        REQUIRE(layout.lineRect(0).isEmpty());
    }
}

// =============================================================================
// Copy/Move Tests
// =============================================================================

TEST_CASE("ParagraphLayout copy constructor", "[editor][paragraph_layout]") {
    ParagraphLayout original("Copy me");
    original.setFont(QFont("Serif", 14));
    original.doLayout(500.0);

    ParagraphLayout copy(original);

    SECTION("Copy has same text") {
        REQUIRE(copy.text() == "Copy me");
    }

    SECTION("Copy has same font") {
        REQUIRE(copy.font().pointSize() == 14);
    }

    SECTION("Copy is dirty (requires re-layout)") {
        REQUIRE(copy.isDirty());
    }

    SECTION("Copy is independent") {
        copy.setText("Modified");
        REQUIRE(original.text() == "Copy me");
    }
}

TEST_CASE("ParagraphLayout move constructor", "[editor][paragraph_layout]") {
    ParagraphLayout original("Move me");
    original.doLayout(500.0);
    qreal originalHeight = original.height();

    ParagraphLayout moved(std::move(original));

    SECTION("Moved has original text") {
        REQUIRE(moved.text() == "Move me");
    }

    SECTION("Moved has original height") {
        REQUIRE(moved.height() == originalHeight);
    }

    SECTION("Original is reset") {
        REQUIRE(original.text().isEmpty());
        REQUIRE(original.isDirty());
    }
}

TEST_CASE("ParagraphLayout copy assignment", "[editor][paragraph_layout]") {
    ParagraphLayout original("Source");
    original.doLayout(500.0);

    ParagraphLayout target("Target");
    target = original;

    SECTION("Target has source text") {
        REQUIRE(target.text() == "Source");
    }

    SECTION("Target is dirty") {
        REQUIRE(target.isDirty());
    }

    SECTION("Self-assignment is safe") {
        target = target;
        REQUIRE(target.text() == "Source");
    }
}

TEST_CASE("ParagraphLayout move assignment", "[editor][paragraph_layout]") {
    ParagraphLayout original("Moving");
    original.doLayout(500.0);

    ParagraphLayout target("Target");
    target = std::move(original);

    SECTION("Target has moved text") {
        REQUIRE(target.text() == "Moving");
    }

    SECTION("Original is reset") {
        REQUIRE(original.text().isEmpty());
    }
}

// =============================================================================
// Advanced Access Tests
// =============================================================================

TEST_CASE("ParagraphLayout textLayout access", "[editor][paragraph_layout]") {
    ParagraphLayout layout("Test text");
    layout.doLayout(500.0);

    SECTION("Const access returns valid layout") {
        const QTextLayout& tl = layout.textLayout();
        REQUIRE(tl.text() == "Test text");
    }

    SECTION("Mutable access returns valid layout") {
        QTextLayout& tl = layout.textLayout();
        REQUIRE(tl.text() == "Test text");
    }
}

// =============================================================================
// Font Size Effects
// =============================================================================

TEST_CASE("ParagraphLayout font size affects height", "[editor][paragraph_layout]") {
    ParagraphLayout layoutSmall("Test text");
    ParagraphLayout layoutLarge("Test text");

    layoutSmall.setFont(QFont("Serif", 10));
    layoutLarge.setFont(QFont("Serif", 24));

    layoutSmall.doLayout(500.0);
    layoutLarge.doLayout(500.0);

    SECTION("Larger font produces taller layout") {
        REQUIRE(layoutLarge.height() > layoutSmall.height());
    }
}

// =============================================================================
// Unicode Text Tests
// =============================================================================

TEST_CASE("ParagraphLayout with Unicode", "[editor][paragraph_layout]") {
    SECTION("Polish characters") {
        ParagraphLayout layout(QString::fromUtf8(u8"Zaz\u00F3\u0142\u0107 g\u0119\u015Bl\u0105 ja\u017A\u0144"));
        qreal height = layout.doLayout(500.0);
        REQUIRE(height > 0.0);
        REQUIRE(layout.lineCount() >= 1);
    }

    SECTION("Chinese characters") {
        ParagraphLayout layout(QString::fromUtf8(u8"\u4F60\u597D\u4E16\u754C"));
        qreal height = layout.doLayout(500.0);
        REQUIRE(height > 0.0);
    }

    SECTION("Mixed scripts") {
        ParagraphLayout layout(QString::fromUtf8(u8"Hello \u4F60\u597D \u041F\u0440\u0438\u0432\u0435\u0442"));
        qreal height = layout.doLayout(500.0);
        REQUIRE(height > 0.0);
    }

    SECTION("Emoji") {
        ParagraphLayout layout(QString::fromUtf8(u8"Hello \U0001F44B World \U0001F30D"));
        qreal height = layout.doLayout(500.0);
        REQUIRE(height > 0.0);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("ParagraphLayout edge cases", "[editor][paragraph_layout]") {
    SECTION("Very narrow width") {
        ParagraphLayout layout("Hello world");
        qreal height = layout.doLayout(1.0);
        // Should still produce valid output
        REQUIRE(height > 0.0);
    }

    SECTION("Very wide width") {
        ParagraphLayout layout("Hello world");
        qreal height = layout.doLayout(10000.0);
        REQUIRE(height > 0.0);
        REQUIRE(layout.lineCount() == 1);  // Single line with wide width
    }

    SECTION("Zero width") {
        ParagraphLayout layout("Hello world");
        qreal height = layout.doLayout(0.0);
        // Behavior may vary, but should not crash
        REQUIRE(height >= 0.0);
    }

    SECTION("Single character") {
        ParagraphLayout layout("X");
        qreal height = layout.doLayout(500.0);
        REQUIRE(height > 0.0);
        REQUIRE(layout.lineCount() == 1);
    }

    SECTION("Only whitespace") {
        ParagraphLayout layout("   ");
        qreal height = layout.doLayout(500.0);
        // Whitespace-only should still layout
        REQUIRE(height >= 0.0);
    }

    SECTION("Newlines in text") {
        ParagraphLayout layout("Line1\nLine2\nLine3");
        layout.doLayout(500.0);
        // QTextLayout treats \n as line breaks
        REQUIRE(layout.lineCount() >= 1);
    }

    SECTION("Tabs in text") {
        ParagraphLayout layout("Col1\tCol2\tCol3");
        qreal height = layout.doLayout(500.0);
        REQUIRE(height > 0.0);
    }
}

// =============================================================================
// Multiple Layout Calls
// =============================================================================

TEST_CASE("ParagraphLayout multiple operations", "[editor][paragraph_layout]") {
    ParagraphLayout layout;

    SECTION("Sequential text changes") {
        layout.setText("First");
        layout.doLayout(500.0);
        qreal h1 = layout.height();

        layout.setText("Second text that is longer");
        layout.doLayout(500.0);
        qreal h2 = layout.height();

        layout.setText("A");
        layout.doLayout(500.0);
        qreal h3 = layout.height();

        // Heights should be valid after each layout
        REQUIRE(h1 > 0.0);
        REQUIRE(h2 > 0.0);
        REQUIRE(h3 > 0.0);
    }

    SECTION("Alternating layout widths") {
        layout.setText("Some text for testing");

        layout.doLayout(100.0);
        int c1 = layout.lineCount();

        layout.doLayout(500.0);
        int c2 = layout.lineCount();

        layout.doLayout(100.0);
        int c3 = layout.lineCount();

        REQUIRE(c1 == c3);  // Same width should give same result
        REQUIRE(c2 <= c1);  // Wider should give fewer or equal lines
    }
}

// =============================================================================
// Formatting Tests (Phase 2.2)
// =============================================================================

TEST_CASE("ParagraphLayout setFormats basic", "[editor][paragraph_layout][formatting]") {
    ParagraphLayout layout("Hello World");

    SECTION("No formats initially") {
        REQUIRE(layout.hasFormats() == false);
        REQUIRE(layout.formats().isEmpty());
    }

    SECTION("Set single format range") {
        QTextLayout::FormatRange range;
        range.start = 0;
        range.length = 5;
        range.format.setFontWeight(QFont::Bold);

        layout.setFormats({range});

        REQUIRE(layout.hasFormats());
        REQUIRE(layout.formats().size() == 1);
        REQUIRE(layout.isDirty());
    }

    SECTION("Set multiple format ranges") {
        QTextLayout::FormatRange boldRange;
        boldRange.start = 0;
        boldRange.length = 5;
        boldRange.format.setFontWeight(QFont::Bold);

        QTextLayout::FormatRange italicRange;
        italicRange.start = 6;
        italicRange.length = 5;
        italicRange.format.setFontItalic(true);

        layout.setFormats({boldRange, italicRange});

        REQUIRE(layout.hasFormats());
        REQUIRE(layout.formats().size() == 2);
    }

    SECTION("Clear formats") {
        QTextLayout::FormatRange range;
        range.start = 0;
        range.length = 5;
        range.format.setFontWeight(QFont::Bold);

        layout.setFormats({range});
        layout.doLayout(500.0);
        REQUIRE(layout.isDirty() == false);

        layout.clearFormats();

        REQUIRE(layout.hasFormats() == false);
        REQUIRE(layout.formats().isEmpty());
        REQUIRE(layout.isDirty());
    }
}

TEST_CASE("ParagraphLayout formats preserved after operations", "[editor][paragraph_layout][formatting]") {
    ParagraphLayout layout("Hello World");

    QTextLayout::FormatRange range;
    range.start = 0;
    range.length = 5;
    range.format.setFontWeight(QFont::Bold);
    layout.setFormats({range});

    SECTION("Formats preserved after layout") {
        layout.doLayout(500.0);
        REQUIRE(layout.hasFormats());
        REQUIRE(layout.formats().size() == 1);
    }

    SECTION("Formats copied with copy constructor") {
        ParagraphLayout copy(layout);
        REQUIRE(copy.hasFormats());
        REQUIRE(copy.formats().size() == 1);
    }

    SECTION("Formats copied with copy assignment") {
        ParagraphLayout target;
        target = layout;
        REQUIRE(target.hasFormats());
        REQUIRE(target.formats().size() == 1);
    }

    SECTION("Formats moved with move constructor") {
        ParagraphLayout moved(std::move(layout));
        REQUIRE(moved.hasFormats());
        REQUIRE(moved.formats().size() == 1);
        REQUIRE(layout.hasFormats() == false);  // Original cleared
    }

    SECTION("Formats cleared with clear()") {
        layout.clear();
        REQUIRE(layout.hasFormats() == false);
    }
}

TEST_CASE("ParagraphLayout with formatted text layout", "[editor][paragraph_layout][formatting]") {
    ParagraphLayout layout("Bold and Italic text");
    layout.setFont(QFont("Serif", 12));

    QTextLayout::FormatRange boldRange;
    boldRange.start = 0;
    boldRange.length = 4;  // "Bold"
    boldRange.format.setFontWeight(QFont::Bold);

    QTextLayout::FormatRange italicRange;
    italicRange.start = 9;
    italicRange.length = 6;  // "Italic"
    italicRange.format.setFontItalic(true);

    layout.setFormats({boldRange, italicRange});

    SECTION("Layout succeeds with formats") {
        qreal height = layout.doLayout(500.0);
        REQUIRE(height > 0.0);
        REQUIRE(layout.lineCount() >= 1);
    }

    SECTION("Layout clears dirty flag with formats") {
        REQUIRE(layout.isDirty());
        layout.doLayout(500.0);
        REQUIRE(layout.isDirty() == false);
    }
}

// =============================================================================
// FormatConverter Tests (Phase 2.2)
// =============================================================================

TEST_CASE("FormatConverter elementTypeToFormat", "[editor][format_converter]") {
    QFont baseFont("Serif", 12);

    SECTION("Bold format") {
        auto format = FormatConverter::elementTypeToFormat(ElementType::Bold, baseFont);
        REQUIRE(format.fontWeight() == QFont::Bold);
    }

    SECTION("Italic format") {
        auto format = FormatConverter::elementTypeToFormat(ElementType::Italic, baseFont);
        REQUIRE(format.fontItalic());
    }

    SECTION("Underline format") {
        auto format = FormatConverter::elementTypeToFormat(ElementType::Underline, baseFont);
        REQUIRE(format.fontUnderline());
    }

    SECTION("Strikethrough format") {
        auto format = FormatConverter::elementTypeToFormat(ElementType::Strikethrough, baseFont);
        REQUIRE(format.fontStrikeOut());
    }

    SECTION("Subscript format") {
        auto format = FormatConverter::elementTypeToFormat(ElementType::Subscript, baseFont);
        REQUIRE(format.verticalAlignment() == QTextCharFormat::AlignSubScript);
    }

    SECTION("Superscript format") {
        auto format = FormatConverter::elementTypeToFormat(ElementType::Superscript, baseFont);
        REQUIRE(format.verticalAlignment() == QTextCharFormat::AlignSuperScript);
    }

    SECTION("Text has no special formatting") {
        auto format = FormatConverter::elementTypeToFormat(ElementType::Text, baseFont);
        REQUIRE(format.fontWeight() != QFont::Bold);
        REQUIRE(format.fontItalic() == false);
    }
}

TEST_CASE("FormatConverter combineFormats", "[editor][format_converter]") {
    QFont baseFont("Serif", 12);

    SECTION("Empty list produces default format") {
        auto format = FormatConverter::combineFormats({}, baseFont);
        REQUIRE(format.fontWeight() != QFont::Bold);
        REQUIRE(format.fontItalic() == false);
    }

    SECTION("Bold + Italic") {
        QList<ElementType> types = {ElementType::Bold, ElementType::Italic};
        auto format = FormatConverter::combineFormats(types, baseFont);
        REQUIRE(format.fontWeight() == QFont::Bold);
        REQUIRE(format.fontItalic());
    }

    SECTION("Bold + Italic + Underline") {
        QList<ElementType> types = {ElementType::Bold, ElementType::Italic, ElementType::Underline};
        auto format = FormatConverter::combineFormats(types, baseFont);
        REQUIRE(format.fontWeight() == QFont::Bold);
        REQUIRE(format.fontItalic());
        REQUIRE(format.fontUnderline());
    }
}

TEST_CASE("FormatConverter buildFormatRanges plain text", "[editor][format_converter]") {
    QFont baseFont("Serif", 12);

    SECTION("Plain text produces no format ranges") {
        KmlParagraph para("Hello World");
        auto ranges = FormatConverter::buildFormatRanges(para, baseFont);
        REQUIRE(ranges.isEmpty());
    }

    SECTION("Empty paragraph produces no format ranges") {
        KmlParagraph para;
        auto ranges = FormatConverter::buildFormatRanges(para, baseFont);
        REQUIRE(ranges.isEmpty());
    }
}

TEST_CASE("FormatConverter buildFormatRanges with bold", "[editor][format_converter]") {
    QFont baseFont("Serif", 12);

    SECTION("Single bold word") {
        // Build: "Hello <b>World</b>"
        KmlParagraph para;
        para.addElement(std::make_unique<KmlTextRun>("Hello "));

        auto bold = std::make_unique<KmlBold>();
        bold->appendChild(std::make_unique<KmlTextRun>("World"));
        para.addElement(std::move(bold));

        auto ranges = FormatConverter::buildFormatRanges(para, baseFont);

        REQUIRE(ranges.size() == 1);
        REQUIRE(ranges[0].start == 6);  // After "Hello "
        REQUIRE(ranges[0].length == 5);  // "World"
        REQUIRE(ranges[0].format.fontWeight() == QFont::Bold);
    }

    SECTION("Bold at start") {
        // Build: "<b>Bold</b> text"
        KmlParagraph para;

        auto bold = std::make_unique<KmlBold>();
        bold->appendChild(std::make_unique<KmlTextRun>("Bold"));
        para.addElement(std::move(bold));
        para.addElement(std::make_unique<KmlTextRun>(" text"));

        auto ranges = FormatConverter::buildFormatRanges(para, baseFont);

        REQUIRE(ranges.size() == 1);
        REQUIRE(ranges[0].start == 0);
        REQUIRE(ranges[0].length == 4);  // "Bold"
        REQUIRE(ranges[0].format.fontWeight() == QFont::Bold);
    }
}

TEST_CASE("FormatConverter buildFormatRanges with italic", "[editor][format_converter]") {
    QFont baseFont("Serif", 12);

    SECTION("Single italic word") {
        // Build: "Hello <i>World</i>"
        KmlParagraph para;
        para.addElement(std::make_unique<KmlTextRun>("Hello "));

        auto italic = std::make_unique<KmlItalic>();
        italic->appendChild(std::make_unique<KmlTextRun>("World"));
        para.addElement(std::move(italic));

        auto ranges = FormatConverter::buildFormatRanges(para, baseFont);

        REQUIRE(ranges.size() == 1);
        REQUIRE(ranges[0].start == 6);
        REQUIRE(ranges[0].length == 5);
        REQUIRE(ranges[0].format.fontItalic());
    }
}

TEST_CASE("FormatConverter buildFormatRanges nested formatting", "[editor][format_converter]") {
    QFont baseFont("Serif", 12);

    SECTION("Bold inside italic") {
        // Build: "Hello <i><b>World</b></i>"
        KmlParagraph para;
        para.addElement(std::make_unique<KmlTextRun>("Hello "));

        auto italic = std::make_unique<KmlItalic>();
        auto bold = std::make_unique<KmlBold>();
        bold->appendChild(std::make_unique<KmlTextRun>("World"));
        italic->appendChild(std::move(bold));
        para.addElement(std::move(italic));

        auto ranges = FormatConverter::buildFormatRanges(para, baseFont);

        REQUIRE(ranges.size() == 1);
        REQUIRE(ranges[0].start == 6);
        REQUIRE(ranges[0].length == 5);
        // Should have both bold AND italic
        REQUIRE(ranges[0].format.fontWeight() == QFont::Bold);
        REQUIRE(ranges[0].format.fontItalic());
    }

    SECTION("Mixed content in italic container") {
        // Build: "<i>Normal <b>bold</b> normal</i>"
        KmlParagraph para;

        auto italic = std::make_unique<KmlItalic>();
        italic->appendChild(std::make_unique<KmlTextRun>("Normal "));

        auto bold = std::make_unique<KmlBold>();
        bold->appendChild(std::make_unique<KmlTextRun>("bold"));
        italic->appendChild(std::move(bold));

        italic->appendChild(std::make_unique<KmlTextRun>(" normal"));
        para.addElement(std::move(italic));

        auto ranges = FormatConverter::buildFormatRanges(para, baseFont);

        // Should have 3 ranges: italic only, bold+italic, italic only
        REQUIRE(ranges.size() == 3);

        // First: "Normal " - italic only
        REQUIRE(ranges[0].start == 0);
        REQUIRE(ranges[0].length == 7);
        REQUIRE(ranges[0].format.fontItalic());
        REQUIRE(ranges[0].format.fontWeight() != QFont::Bold);

        // Second: "bold" - bold + italic
        REQUIRE(ranges[1].start == 7);
        REQUIRE(ranges[1].length == 4);
        REQUIRE(ranges[1].format.fontItalic());
        REQUIRE(ranges[1].format.fontWeight() == QFont::Bold);

        // Third: " normal" - italic only
        REQUIRE(ranges[2].start == 11);
        REQUIRE(ranges[2].length == 7);
        REQUIRE(ranges[2].format.fontItalic());
        REQUIRE(ranges[2].format.fontWeight() != QFont::Bold);
    }
}

TEST_CASE("FormatConverter buildFormatRanges multiple siblings", "[editor][format_converter]") {
    QFont baseFont("Serif", 12);

    SECTION("Bold then italic") {
        // Build: "<b>Bold</b> <i>Italic</i>"
        KmlParagraph para;

        auto bold = std::make_unique<KmlBold>();
        bold->appendChild(std::make_unique<KmlTextRun>("Bold"));
        para.addElement(std::move(bold));

        para.addElement(std::make_unique<KmlTextRun>(" "));

        auto italic = std::make_unique<KmlItalic>();
        italic->appendChild(std::make_unique<KmlTextRun>("Italic"));
        para.addElement(std::move(italic));

        auto ranges = FormatConverter::buildFormatRanges(para, baseFont);

        REQUIRE(ranges.size() == 2);

        // First: "Bold"
        REQUIRE(ranges[0].start == 0);
        REQUIRE(ranges[0].length == 4);
        REQUIRE(ranges[0].format.fontWeight() == QFont::Bold);
        REQUIRE(ranges[0].format.fontItalic() == false);

        // Second: "Italic"
        REQUIRE(ranges[1].start == 5);
        REQUIRE(ranges[1].length == 6);
        REQUIRE(ranges[1].format.fontWeight() != QFont::Bold);
        REQUIRE(ranges[1].format.fontItalic());
    }
}

TEST_CASE("FormatConverter with underline and strikethrough", "[editor][format_converter]") {
    QFont baseFont("Serif", 12);

    SECTION("Underline") {
        KmlParagraph para;
        auto underline = std::make_unique<KmlUnderline>();
        underline->appendChild(std::make_unique<KmlTextRun>("Underlined"));
        para.addElement(std::move(underline));

        auto ranges = FormatConverter::buildFormatRanges(para, baseFont);

        REQUIRE(ranges.size() == 1);
        REQUIRE(ranges[0].format.fontUnderline());
    }

    SECTION("Strikethrough") {
        KmlParagraph para;
        auto strike = std::make_unique<KmlStrikethrough>();
        strike->appendChild(std::make_unique<KmlTextRun>("Deleted"));
        para.addElement(std::move(strike));

        auto ranges = FormatConverter::buildFormatRanges(para, baseFont);

        REQUIRE(ranges.size() == 1);
        REQUIRE(ranges[0].format.fontStrikeOut());
    }
}

TEST_CASE("FormatConverter with subscript and superscript", "[editor][format_converter]") {
    QFont baseFont("Serif", 12);

    SECTION("Subscript") {
        // Build: "H<sub>2</sub>O"
        KmlParagraph para;
        para.addElement(std::make_unique<KmlTextRun>("H"));

        auto sub = std::make_unique<KmlSubscript>();
        sub->appendChild(std::make_unique<KmlTextRun>("2"));
        para.addElement(std::move(sub));

        para.addElement(std::make_unique<KmlTextRun>("O"));

        auto ranges = FormatConverter::buildFormatRanges(para, baseFont);

        REQUIRE(ranges.size() == 1);
        REQUIRE(ranges[0].start == 1);  // After "H"
        REQUIRE(ranges[0].length == 1);  // "2"
        REQUIRE(ranges[0].format.verticalAlignment() == QTextCharFormat::AlignSubScript);
    }

    SECTION("Superscript") {
        // Build: "x<sup>2</sup>"
        KmlParagraph para;
        para.addElement(std::make_unique<KmlTextRun>("x"));

        auto sup = std::make_unique<KmlSuperscript>();
        sup->appendChild(std::make_unique<KmlTextRun>("2"));
        para.addElement(std::move(sup));

        auto ranges = FormatConverter::buildFormatRanges(para, baseFont);

        REQUIRE(ranges.size() == 1);
        REQUIRE(ranges[0].start == 1);  // After "x"
        REQUIRE(ranges[0].length == 1);  // "2"
        REQUIRE(ranges[0].format.verticalAlignment() == QTextCharFormat::AlignSuperScript);
    }
}

TEST_CASE("FormatConverter integration with ParagraphLayout", "[editor][format_converter][integration]") {
    QFont baseFont("Serif", 12);

    SECTION("Apply converted formats to layout") {
        // Build: "Hello <b>bold</b> world"
        KmlParagraph para;
        para.addElement(std::make_unique<KmlTextRun>("Hello "));
        auto bold = std::make_unique<KmlBold>();
        bold->appendChild(std::make_unique<KmlTextRun>("bold"));
        para.addElement(std::move(bold));
        para.addElement(std::make_unique<KmlTextRun>(" world"));

        // Create layout with paragraph text
        ParagraphLayout layout(para.plainText());
        layout.setFont(baseFont);

        // Convert formats and apply
        auto ranges = FormatConverter::buildFormatRanges(para, baseFont);
        layout.setFormats(ranges);

        // Layout should work with formats
        qreal height = layout.doLayout(500.0);
        REQUIRE(height > 0.0);
        REQUIRE(layout.lineCount() >= 1);
        REQUIRE(layout.hasFormats());
    }
}

// =============================================================================
// Hit Testing Tests (Phase 2.4)
// =============================================================================

TEST_CASE("ParagraphLayout positionAt basic", "[editor][paragraph_layout][hit_testing]") {
    ParagraphLayout layout("Hello, world!");
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(500.0);

    SECTION("Position at start of text") {
        // Click near x=0 should return position near 0
        int pos = layout.positionAt(QPointF(0.0, 5.0));
        REQUIRE(pos >= 0);
        REQUIRE(pos <= 1);  // Should be at or near the start
    }

    SECTION("Position in middle of text") {
        // Get cursor rect for middle position to find its x
        QRectF midRect = layout.cursorRect(6);  // After "Hello,"
        int pos = layout.positionAt(QPointF(midRect.x() + 1.0, 5.0));
        REQUIRE(pos >= 5);
        REQUIRE(pos <= 7);  // Should be near position 6
    }

    SECTION("Position at end of text") {
        // Click far right should return position at or near end
        int pos = layout.positionAt(QPointF(1000.0, 5.0));
        REQUIRE(pos >= 12);  // "Hello, world!" has 13 chars
        REQUIRE(pos <= 13);
    }

    SECTION("Returns -1 when dirty") {
        ParagraphLayout dirtyLayout("Test");
        REQUIRE(dirtyLayout.isDirty());
        int pos = dirtyLayout.positionAt(QPointF(10.0, 5.0));
        REQUIRE(pos == -1);
    }
}

TEST_CASE("ParagraphLayout positionAt multiline", "[editor][paragraph_layout][hit_testing]") {
    QString longText = "This is the first line of text that wraps. "
                       "And here is more text on additional lines.";
    ParagraphLayout layout(longText);
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(150.0);  // Narrow width to force wrapping

    REQUIRE(layout.lineCount() > 1);

    SECTION("Click on first line") {
        QRectF line0 = layout.lineRect(0);
        int pos = layout.positionAt(QPointF(10.0, line0.center().y()));
        REQUIRE(pos >= 0);
        // Position should be within first line's range
        QTextLine qtLine = layout.textLayout().lineAt(0);
        REQUIRE(pos >= qtLine.textStart());
        REQUIRE(pos <= qtLine.textStart() + qtLine.textLength());
    }

    SECTION("Click on second line") {
        QRectF line1 = layout.lineRect(1);
        int pos = layout.positionAt(QPointF(10.0, line1.center().y()));
        // Position should be within second line's range
        QTextLine qtLine = layout.textLayout().lineAt(1);
        REQUIRE(pos >= qtLine.textStart());
        REQUIRE(pos <= qtLine.textStart() + qtLine.textLength());
    }

    SECTION("Click above all lines uses first line") {
        int pos = layout.positionAt(QPointF(50.0, -100.0));
        REQUIRE(pos >= 0);
        // Should be on first line
        QTextLine qtLine = layout.textLayout().lineAt(0);
        REQUIRE(pos <= qtLine.textStart() + qtLine.textLength());
    }

    SECTION("Click below all lines uses last line") {
        int pos = layout.positionAt(QPointF(50.0, 1000.0));
        // Should be on last line
        QTextLine qtLine = layout.textLayout().lineAt(layout.lineCount() - 1);
        REQUIRE(pos >= qtLine.textStart());
    }
}

TEST_CASE("ParagraphLayout cursorRect basic", "[editor][paragraph_layout][hit_testing]") {
    ParagraphLayout layout("Hello");
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(500.0);

    SECTION("Cursor at position 0") {
        QRectF rect = layout.cursorRect(0);
        REQUIRE(rect.isEmpty() == false);
        REQUIRE(rect.x() >= 0.0);  // At or near left edge
        REQUIRE(rect.height() > 0.0);
        REQUIRE(rect.width() == 1.0);  // Cursor width
    }

    SECTION("Cursor at end of text") {
        QRectF rect = layout.cursorRect(5);  // After "Hello"
        REQUIRE(rect.isEmpty() == false);
        REQUIRE(rect.x() > 0.0);  // Should be to the right
        REQUIRE(rect.height() > 0.0);
    }

    SECTION("Cursor in middle") {
        QRectF rect0 = layout.cursorRect(0);
        QRectF rect2 = layout.cursorRect(2);
        QRectF rect5 = layout.cursorRect(5);

        // Positions should be ordered left to right
        REQUIRE(rect2.x() > rect0.x());
        REQUIRE(rect5.x() > rect2.x());
    }

    SECTION("Negative position clamped to 0") {
        QRectF rectNeg = layout.cursorRect(-5);
        QRectF rect0 = layout.cursorRect(0);
        REQUIRE(rectNeg.x() == rect0.x());
    }

    SECTION("Position beyond text clamped to end") {
        QRectF rectBeyond = layout.cursorRect(100);
        QRectF rectEnd = layout.cursorRect(5);
        REQUIRE(rectBeyond.x() == rectEnd.x());
    }

    SECTION("Returns empty rect when dirty") {
        ParagraphLayout dirtyLayout("Test");
        REQUIRE(dirtyLayout.isDirty());
        QRectF rect = dirtyLayout.cursorRect(2);
        REQUIRE(rect.isEmpty());
    }
}

TEST_CASE("ParagraphLayout cursorRect multiline", "[editor][paragraph_layout][hit_testing]") {
    QString longText = "First line. Second line. Third line.";
    ParagraphLayout layout(longText);
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(80.0);  // Narrow to force multiple lines

    REQUIRE(layout.lineCount() > 1);

    SECTION("Cursor on different lines has different y") {
        QTextLine line0 = layout.textLayout().lineAt(0);
        QTextLine line1 = layout.textLayout().lineAt(1);

        QRectF cursorLine0 = layout.cursorRect(line0.textStart() + 1);
        QRectF cursorLine1 = layout.cursorRect(line1.textStart() + 1);

        REQUIRE(cursorLine1.y() > cursorLine0.y());
    }

    SECTION("Cursor at line end vs next line start") {
        QTextLine line0 = layout.textLayout().lineAt(0);
        int endOfLine0 = line0.textStart() + line0.textLength();

        QRectF cursorAtEnd = layout.cursorRect(endOfLine0);
        REQUIRE(cursorAtEnd.isEmpty() == false);
    }
}

TEST_CASE("ParagraphLayout lineForPosition", "[editor][paragraph_layout][hit_testing]") {
    QString longText = "Line one text here. Line two continues. Line three ends.";
    ParagraphLayout layout(longText);
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(100.0);  // Force multiple lines

    REQUIRE(layout.lineCount() > 1);

    SECTION("Position 0 is on line 0") {
        int line = layout.lineForPosition(0);
        REQUIRE(line == 0);
    }

    SECTION("Position at text end is on last line") {
        int line = layout.lineForPosition(longText.length());
        REQUIRE(line == layout.lineCount() - 1);
    }

    SECTION("Position in middle line") {
        QTextLine qtLine1 = layout.textLayout().lineAt(1);
        int midPos = qtLine1.textStart() + qtLine1.textLength() / 2;
        int line = layout.lineForPosition(midPos);
        REQUIRE(line == 1);
    }

    SECTION("Negative position clamped") {
        int line = layout.lineForPosition(-10);
        REQUIRE(line == 0);
    }

    SECTION("Position beyond text clamped") {
        int line = layout.lineForPosition(1000);
        REQUIRE(line == layout.lineCount() - 1);
    }

    SECTION("Returns -1 when dirty") {
        ParagraphLayout dirtyLayout("Test");
        REQUIRE(dirtyLayout.isDirty());
        int line = dirtyLayout.lineForPosition(2);
        REQUIRE(line == -1);
    }
}

TEST_CASE("ParagraphLayout hit testing round trip", "[editor][paragraph_layout][hit_testing]") {
    ParagraphLayout layout("Hello, world!");
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(500.0);

    SECTION("cursorRect -> positionAt round trip") {
        for (int pos = 0; pos <= 13; ++pos) {
            QRectF rect = layout.cursorRect(pos);
            REQUIRE(rect.isEmpty() == false);

            // Click in the center of the cursor rect
            QPointF clickPoint(rect.x() + 0.5, rect.center().y());
            int foundPos = layout.positionAt(clickPoint);

            // Should find the same or adjacent position
            REQUIRE(std::abs(foundPos - pos) <= 1);
        }
    }
}

TEST_CASE("ParagraphLayout positionAt with empty text", "[editor][paragraph_layout][hit_testing]") {
    ParagraphLayout layout("");
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(500.0);

    SECTION("Click on empty layout") {
        // Empty layout may or may not have a line
        int pos = layout.positionAt(QPointF(10.0, 5.0));
        // Should return 0 or -1 depending on implementation
        REQUIRE(pos >= -1);
        REQUIRE(pos <= 0);
    }
}

TEST_CASE("ParagraphLayout cursorRect with empty text", "[editor][paragraph_layout][hit_testing]") {
    ParagraphLayout layout("");
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(500.0);

    SECTION("Cursor at position 0 in empty layout") {
        QRectF rect = layout.cursorRect(0);
        // Should return valid cursor rect at position 0
        if (layout.lineCount() > 0) {
            REQUIRE(rect.height() > 0.0);
        }
    }
}

TEST_CASE("ParagraphLayout hit testing with Unicode", "[editor][paragraph_layout][hit_testing]") {
    // Polish text with special characters
    ParagraphLayout layout(QString::fromUtf8(u8"Zaz\u00F3\u0142\u0107 g\u0119\u015Bl\u0105"));
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(500.0);

    SECTION("positionAt works with Unicode") {
        int pos = layout.positionAt(QPointF(50.0, 5.0));
        REQUIRE(pos >= 0);
        REQUIRE(pos <= layout.text().length());
    }

    SECTION("cursorRect works with Unicode") {
        // Test cursor at various positions including Unicode characters
        for (int i = 0; i <= layout.text().length(); ++i) {
            QRectF rect = layout.cursorRect(i);
            REQUIRE(rect.isEmpty() == false);
        }
    }
}

TEST_CASE("ParagraphLayout hit testing click between characters", "[editor][paragraph_layout][hit_testing]") {
    ParagraphLayout layout("ABCDEF");
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(500.0);

    SECTION("Click exactly between characters") {
        // Get cursor rects for adjacent positions
        QRectF rectB = layout.cursorRect(1);  // Before 'B'
        QRectF rectC = layout.cursorRect(2);  // Before 'C'

        // Click halfway between them
        qreal midX = (rectB.x() + rectC.x()) / 2.0;
        int pos = layout.positionAt(QPointF(midX, 5.0));

        // Should return either 1 or 2 (nearest valid position)
        REQUIRE(pos >= 1);
        REQUIRE(pos <= 2);
    }

    SECTION("Click left of center goes to earlier position") {
        QRectF rectB = layout.cursorRect(1);
        QRectF rectC = layout.cursorRect(2);

        // Click closer to B
        qreal leftX = rectB.x() + (rectC.x() - rectB.x()) * 0.25;
        int pos = layout.positionAt(QPointF(leftX, 5.0));
        REQUIRE(pos == 1);
    }

    SECTION("Click right of center goes to later position") {
        QRectF rectB = layout.cursorRect(1);
        QRectF rectC = layout.cursorRect(2);

        // Click closer to C
        qreal rightX = rectB.x() + (rectC.x() - rectB.x()) * 0.75;
        int pos = layout.positionAt(QPointF(rightX, 5.0));
        REQUIRE(pos == 2);
    }
}

// =============================================================================
// Selection Tests (Phase 2.5)
// =============================================================================

TEST_CASE("ParagraphLayout selection basic", "[editor][paragraph_layout][drawing]") {
    ParagraphLayout layout("Hello, world!");
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(500.0);

    SECTION("No selection initially") {
        REQUIRE(layout.hasSelection() == false);
        REQUIRE(layout.selectionStart() == -1);
        REQUIRE(layout.selectionEnd() == -1);
    }

    SECTION("Set selection") {
        layout.setSelection(0, 5);
        REQUIRE(layout.hasSelection());
        REQUIRE(layout.selectionStart() == 0);
        REQUIRE(layout.selectionEnd() == 5);
    }

    SECTION("Clear selection") {
        layout.setSelection(0, 5);
        REQUIRE(layout.hasSelection());

        layout.clearSelection();
        REQUIRE(layout.hasSelection() == false);
        REQUIRE(layout.selectionStart() == -1);
        REQUIRE(layout.selectionEnd() == -1);
    }

    SECTION("Empty selection (start == end) is not a selection") {
        layout.setSelection(3, 3);
        REQUIRE(layout.hasSelection() == false);
    }

    SECTION("Reversed selection still valid") {
        layout.setSelection(10, 5);  // end < start
        REQUIRE(layout.hasSelection());
        REQUIRE(layout.selectionStart() == 10);
        REQUIRE(layout.selectionEnd() == 5);
    }
}

TEST_CASE("ParagraphLayout selection colors", "[editor][paragraph_layout][drawing]") {
    ParagraphLayout layout("Test");

    SECTION("Default colors are set") {
        // Default colors come from QPalette, should be valid
        REQUIRE(layout.selectionBackgroundColor().isValid());
        REQUIRE(layout.selectionForegroundColor().isValid());
    }

    SECTION("Custom colors") {
        QColor bg(Qt::blue);
        QColor fg(Qt::white);

        layout.setSelectionColors(bg, fg);

        REQUIRE(layout.selectionBackgroundColor() == bg);
        REQUIRE(layout.selectionForegroundColor() == fg);
    }
}

TEST_CASE("ParagraphLayout selection preserved in copy/move", "[editor][paragraph_layout][drawing]") {
    ParagraphLayout original("Test text");
    original.setSelection(2, 6);
    original.setSelectionColors(Qt::blue, Qt::white);

    SECTION("Copy preserves selection") {
        ParagraphLayout copy(original);
        REQUIRE(copy.hasSelection());
        REQUIRE(copy.selectionStart() == 2);
        REQUIRE(copy.selectionEnd() == 6);
        REQUIRE(copy.selectionBackgroundColor() == Qt::blue);
    }

    SECTION("Move preserves selection") {
        ParagraphLayout moved(std::move(original));
        REQUIRE(moved.hasSelection());
        REQUIRE(moved.selectionStart() == 2);
        REQUIRE(moved.selectionEnd() == 6);

        // Original should be cleared
        REQUIRE(original.hasSelection() == false);
    }

    SECTION("Clear resets selection") {
        original.clear();
        REQUIRE(original.hasSelection() == false);
    }
}

// =============================================================================
// Spell Error Tests (Phase 2.5)
// =============================================================================

TEST_CASE("ParagraphLayout spell errors basic", "[editor][paragraph_layout][drawing]") {
    ParagraphLayout layout("Hello wrold!");
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(500.0);

    SECTION("No spell errors initially") {
        REQUIRE(layout.hasSpellErrors() == false);
        REQUIRE(layout.spellErrors().empty());
    }

    SECTION("Add spell error") {
        layout.addSpellError(6, 5);  // "wrold"
        REQUIRE(layout.hasSpellErrors());

        auto errors = layout.spellErrors();
        REQUIRE(errors.size() == 1);
        REQUIRE(errors[0].start == 6);
        REQUIRE(errors[0].length == 5);
    }

    SECTION("Add multiple spell errors") {
        layout.addSpellError(0, 5);   // "Hello" (not really an error, just testing)
        layout.addSpellError(6, 5);   // "wrold"

        auto errors = layout.spellErrors();
        REQUIRE(errors.size() == 2);
    }

    SECTION("Clear spell errors") {
        layout.addSpellError(6, 5);
        REQUIRE(layout.hasSpellErrors());

        layout.clearSpellErrors();
        REQUIRE(layout.hasSpellErrors() == false);
        REQUIRE(layout.spellErrors().empty());
    }

    SECTION("Invalid error ranges ignored") {
        layout.addSpellError(-1, 5);   // Negative start
        layout.addSpellError(0, 0);    // Zero length
        layout.addSpellError(0, -5);   // Negative length

        REQUIRE(layout.hasSpellErrors() == false);
    }
}

TEST_CASE("ParagraphLayout spell errors preserved in copy/move", "[editor][paragraph_layout][drawing]") {
    ParagraphLayout original("Teh quick fox");
    original.addSpellError(0, 3);  // "Teh"

    SECTION("Copy preserves spell errors") {
        ParagraphLayout copy(original);
        REQUIRE(copy.hasSpellErrors());
        auto errors = copy.spellErrors();
        REQUIRE(errors.size() == 1);
        REQUIRE(errors[0].start == 0);
        REQUIRE(errors[0].length == 3);
    }

    SECTION("Move preserves spell errors") {
        ParagraphLayout moved(std::move(original));
        REQUIRE(moved.hasSpellErrors());

        // Original should be cleared
        REQUIRE(original.hasSpellErrors() == false);
    }

    SECTION("Clear resets spell errors") {
        original.clear();
        REQUIRE(original.hasSpellErrors() == false);
    }
}

TEST_CASE("SpellError struct", "[editor][paragraph_layout][drawing]") {
    SECTION("Default constructor") {
        SpellError error;
        REQUIRE(error.start == 0);
        REQUIRE(error.length == 0);
    }

    SECTION("Parameterized constructor") {
        SpellError error(5, 10);
        REQUIRE(error.start == 5);
        REQUIRE(error.length == 10);
    }

    SECTION("Equality operator") {
        SpellError e1(5, 10);
        SpellError e2(5, 10);
        SpellError e3(6, 10);
        SpellError e4(5, 11);

        REQUIRE(e1 == e2);
        REQUIRE_FALSE(e1 == e3);
        REQUIRE_FALSE(e1 == e4);
    }
}

// =============================================================================
// Drawing Tests (Phase 2.5)
// =============================================================================

TEST_CASE("ParagraphLayout draw basic", "[editor][paragraph_layout][drawing]") {
    ParagraphLayout layout("Hello, world!");
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(500.0);

    SECTION("Draw to image does not crash") {
        QImage image(600, 100, QImage::Format_ARGB32);
        image.fill(Qt::white);

        QPainter painter(&image);
        layout.draw(&painter, QPointF(10, 10));
        painter.end();

        // Drawing should produce non-white pixels
        // (We can't easily verify exact output, but we check it doesn't crash)
        REQUIRE(true);
    }

    SECTION("Draw with null painter is safe") {
        layout.draw(nullptr, QPointF(0, 0));
        REQUIRE(true);  // Should not crash
    }

    SECTION("Draw when dirty does nothing") {
        ParagraphLayout dirtyLayout("Test");
        REQUIRE(dirtyLayout.isDirty());

        QImage image(100, 50, QImage::Format_ARGB32);
        image.fill(Qt::white);
        QPainter painter(&image);

        dirtyLayout.draw(&painter, QPointF(0, 0));
        painter.end();

        // Image should still be all white (nothing drawn)
        bool allWhite = true;
        for (int y = 0; y < image.height() && allWhite; ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (image.pixel(x, y) != qRgb(255, 255, 255)) {
                    allWhite = false;
                    break;
                }
            }
        }
        REQUIRE(allWhite);
    }
}

TEST_CASE("ParagraphLayout draw with selection", "[editor][paragraph_layout][drawing]") {
    ParagraphLayout layout("Hello, world!");
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(500.0);

    SECTION("Draw with selection does not crash") {
        layout.setSelection(0, 5);
        layout.setSelectionColors(Qt::blue, Qt::white);

        QImage image(600, 100, QImage::Format_ARGB32);
        image.fill(Qt::white);

        QPainter painter(&image);
        layout.draw(&painter, QPointF(10, 10));
        painter.end();

        // Check that some blue pixels exist (selection background)
        bool hasBlue = false;
        for (int y = 0; y < image.height() && !hasBlue; ++y) {
            for (int x = 0; x < image.width(); ++x) {
                QColor pixel = image.pixelColor(x, y);
                if (pixel.blue() > 200 && pixel.red() < 50 && pixel.green() < 50) {
                    hasBlue = true;
                    break;
                }
            }
        }
        REQUIRE(hasBlue);
    }

    SECTION("Draw with reversed selection") {
        layout.setSelection(10, 5);  // Reversed

        QImage image(600, 100, QImage::Format_ARGB32);
        image.fill(Qt::white);

        QPainter painter(&image);
        layout.draw(&painter, QPointF(10, 10));
        painter.end();

        REQUIRE(true);  // Should not crash
    }
}

TEST_CASE("ParagraphLayout draw with spell errors", "[editor][paragraph_layout][drawing]") {
    ParagraphLayout layout("Hello wrold!");
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(500.0);

    SECTION("Draw with spell error does not crash") {
        layout.addSpellError(6, 5);  // "wrold"

        QImage image(600, 100, QImage::Format_ARGB32);
        image.fill(Qt::white);

        QPainter painter(&image);
        layout.draw(&painter, QPointF(10, 10));
        painter.end();

        // Check that some red pixels exist (wavy underline)
        bool hasRed = false;
        for (int y = 0; y < image.height() && !hasRed; ++y) {
            for (int x = 0; x < image.width(); ++x) {
                QColor pixel = image.pixelColor(x, y);
                if (pixel.red() > 200 && pixel.green() < 50 && pixel.blue() < 50) {
                    hasRed = true;
                    break;
                }
            }
        }
        REQUIRE(hasRed);
    }

    SECTION("Draw with multiple spell errors") {
        layout.addSpellError(0, 5);
        layout.addSpellError(6, 5);

        QImage image(600, 100, QImage::Format_ARGB32);
        image.fill(Qt::white);

        QPainter painter(&image);
        layout.draw(&painter, QPointF(10, 10));
        painter.end();

        REQUIRE(true);  // Should not crash
    }
}

TEST_CASE("ParagraphLayout draw multiline with selection", "[editor][paragraph_layout][drawing]") {
    QString longText = "This is a long text that wraps across multiple lines. "
                       "We want to test selection spanning lines.";
    ParagraphLayout layout(longText);
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(150.0);  // Narrow to force wrapping

    REQUIRE(layout.lineCount() > 1);

    SECTION("Selection spanning lines") {
        // Select text that spans multiple lines
        layout.setSelection(20, 60);
        layout.setSelectionColors(Qt::blue, Qt::white);

        QImage image(200, 200, QImage::Format_ARGB32);
        image.fill(Qt::white);

        QPainter painter(&image);
        layout.draw(&painter, QPointF(10, 10));
        painter.end();

        REQUIRE(true);  // Should not crash
    }
}

TEST_CASE("ParagraphLayout draw multiline with spell errors", "[editor][paragraph_layout][drawing]") {
    QString longText = "This is a long text with wrold and teh errors "
                       "that wrap across multiple lines.";
    ParagraphLayout layout(longText);
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(150.0);

    REQUIRE(layout.lineCount() > 1);

    SECTION("Spell error spanning line break") {
        // This might not actually span, but tests boundary handling
        layout.addSpellError(25, 5);  // "wrold"
        layout.addSpellError(35, 3);  // "teh"

        QImage image(200, 200, QImage::Format_ARGB32);
        image.fill(Qt::white);

        QPainter painter(&image);
        layout.draw(&painter, QPointF(10, 10));
        painter.end();

        REQUIRE(true);
    }
}

TEST_CASE("ParagraphLayout draw combined selection and spell errors", "[editor][paragraph_layout][drawing]") {
    ParagraphLayout layout("Hello wrold!");
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(500.0);

    SECTION("Selection and spell error together") {
        layout.setSelection(0, 11);
        layout.addSpellError(6, 5);

        QImage image(600, 100, QImage::Format_ARGB32);
        image.fill(Qt::white);

        QPainter painter(&image);
        layout.draw(&painter, QPointF(10, 10));
        painter.end();

        REQUIRE(true);  // Should not crash
    }

    SECTION("Overlapping selection and spell error") {
        layout.setSelection(4, 9);  // Overlaps with "wrold"
        layout.addSpellError(6, 5);

        QImage image(600, 100, QImage::Format_ARGB32);
        image.fill(Qt::white);

        QPainter painter(&image);
        layout.draw(&painter, QPointF(10, 10));
        painter.end();

        REQUIRE(true);
    }
}

TEST_CASE("ParagraphLayout draw at different positions", "[editor][paragraph_layout][drawing]") {
    ParagraphLayout layout("Test");
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(500.0);

    SECTION("Draw at origin") {
        QImage image(100, 50, QImage::Format_ARGB32);
        image.fill(Qt::white);

        QPainter painter(&image);
        layout.draw(&painter, QPointF(0, 0));
        painter.end();

        REQUIRE(true);
    }

    SECTION("Draw at offset") {
        QImage image(200, 100, QImage::Format_ARGB32);
        image.fill(Qt::white);

        QPainter painter(&image);
        layout.draw(&painter, QPointF(50, 25));
        painter.end();

        REQUIRE(true);
    }

    SECTION("Draw with negative offset") {
        QImage image(100, 50, QImage::Format_ARGB32);
        image.fill(Qt::white);

        QPainter painter(&image);
        layout.draw(&painter, QPointF(-10, -5));
        painter.end();

        REQUIRE(true);  // Should not crash even with negative offset
    }
}

TEST_CASE("ParagraphLayout draw empty text", "[editor][paragraph_layout][drawing]") {
    ParagraphLayout layout("");
    layout.setFont(QFont("Serif", 12));
    layout.doLayout(500.0);

    SECTION("Draw empty layout") {
        QImage image(100, 50, QImage::Format_ARGB32);
        image.fill(Qt::white);

        QPainter painter(&image);
        layout.draw(&painter, QPointF(10, 10));
        painter.end();

        REQUIRE(true);  // Should not crash
    }

    SECTION("Draw empty with selection set") {
        layout.setSelection(0, 0);

        QImage image(100, 50, QImage::Format_ARGB32);
        image.fill(Qt::white);

        QPainter painter(&image);
        layout.draw(&painter, QPointF(10, 10));
        painter.end();

        REQUIRE(true);
    }
}
