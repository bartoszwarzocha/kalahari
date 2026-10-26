/// @file test_text_lists.cpp
/// @brief Bulleted and numbered lists: the numbers of the items, the formats of the levels and
///        the lists in KML
///
/// A list goes on over paragraphs outside it, a deeper level starts again after an item of a
/// lower one, an item may set its own number or have none. KML keeps all of it: <ol>, <ul>,
/// <li>, a list of a deeper level inside an item.

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/kml_document_model.h>
#include <kalahari/editor/paragraph_format.h>
#include <kalahari/editor/text_lists.h>
#include "editor_test_utils.h"

#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QXmlStreamReader>

#include <utility>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

/// A document with a paragraph for each entry: a list item of the list @c id at @c level, or
/// a paragraph outside a list for id 0
struct Entry {
    Entry(int listId, int itemLevel, std::optional<int> itemValue = std::nullopt,
          bool itemNumbered = true, ListDefinition itemDefinition = ListDefinition::numbered())
        : id(listId), level(itemLevel), value(itemValue), numbered(itemNumbered),
          definition(std::move(itemDefinition)) {}

    int id = 0;
    int level = 0;
    std::optional<int> value;
    bool numbered = true;
    ListDefinition definition;
};

void fill(QTextDocument& document, const std::vector<Entry>& entries) {
    QTextCursor cursor(&document);
    for (size_t i = 0; i < entries.size(); ++i) {
        QTextBlockFormat format;
        if (entries[i].id > 0) {
            ListItem item;
            item.id = entries[i].id;
            item.level = entries[i].level;
            item.value = entries[i].value;
            item.numbered = entries[i].numbered;
            item.definition = entries[i].definition;
            setListItem(format, item);
        }
        if (i == 0) {
            cursor.setBlockFormat(format);
        } else {
            cursor.insertBlock(format);
        }
        cursor.insertText(QStringLiteral("Text %1").arg(i));
    }
}

/// KML of an editor whose paragraphs are the entries
QString kmlOfEntries(const std::vector<Entry>& entries) {
    BookEditor editor;
    editor.fromKml(QStringLiteral("<kml><p></p></kml>"));
    fill(*editor.textDocument(), entries);
    return editor.toKml();
}

/// The marks of the paragraphs of an editor
std::vector<QString> markersOf(const BookEditor& editor) {
    return listMarkers(*editor.textDocument());
}

/// The list item of a paragraph of an editor
std::optional<ListItem> itemOf(const BookEditor& editor, int paragraph) {
    return listItemOf(editor.textDocument()->findBlockByNumber(paragraph).blockFormat());
}

/// KML read and written back by the editor
QString roundTrip(const QString& kml) {
    BookEditor editor;
    editor.fromKml(kml);
    return editor.toKml();
}

}  // namespace

// =============================================================================
// Numbers and formats
// =============================================================================

TEST_CASE("Lists: the numbers in each format", "[editor][lists]") {
    CHECK(listNumberText(7, ListNumberFormat::Decimal) == QStringLiteral("7"));
    CHECK(listNumberText(1, ListNumberFormat::LowerAlpha) == QStringLiteral("a"));
    CHECK(listNumberText(26, ListNumberFormat::LowerAlpha) == QStringLiteral("z"));
    CHECK(listNumberText(27, ListNumberFormat::LowerAlpha) == QStringLiteral("aa"));
    CHECK(listNumberText(28, ListNumberFormat::UpperAlpha) == QStringLiteral("BB"));
    CHECK(listNumberText(4, ListNumberFormat::LowerRoman) == QStringLiteral("iv"));
    CHECK(listNumberText(9, ListNumberFormat::LowerRoman) == QStringLiteral("ix"));
    CHECK(listNumberText(14, ListNumberFormat::UpperRoman) == QStringLiteral("XIV"));
    CHECK(listNumberText(1990, ListNumberFormat::UpperRoman) == QStringLiteral("MCMXC"));
    CHECK(listNumberText(0, ListNumberFormat::LowerRoman) == QStringLiteral("0"));
    CHECK(listNumberText(3, ListNumberFormat::Bullet).isEmpty());

    ListLevel level;
    level.format = ListNumberFormat::LowerAlpha;
    level.suffix = QStringLiteral(")");
    CHECK(listMarkerText(level, 3) == QStringLiteral("c)"));
    level.format = ListNumberFormat::Bullet;
    level.bullet = QStringLiteral("✓");
    CHECK(listMarkerText(level, 3) == QStringLiteral("✓"));
}

TEST_CASE("Lists: the default formats of the levels", "[editor][lists]") {
    const ListDefinition numbered = ListDefinition::numbered();
    CHECK(listMarkerText(numbered.levels[0], 1) == QStringLiteral("1."));
    CHECK(listMarkerText(numbered.levels[1], 1) == QStringLiteral("a)"));
    CHECK(listMarkerText(numbered.levels[2], 1) == QStringLiteral("i."));
    CHECK(listMarkerText(numbered.levels[3], 1) == QStringLiteral("1."));
    CHECK(numbered.kind() == ListKind::Numbered);

    const ListDefinition bulleted = ListDefinition::bulleted();
    CHECK(listMarkerText(bulleted.levels[0], 1) == QStringLiteral("•"));
    CHECK(listMarkerText(bulleted.levels[1], 1) == QStringLiteral("◦"));
    CHECK(listMarkerText(bulleted.levels[2], 1) == QStringLiteral("▪"));
    CHECK(bulleted.kind() == ListKind::Bulleted);

    // 0.635 cm a level: the number of the first level at 0.635 cm, its text at 1.27 cm
    CHECK(numbered.levels[0].markerIndent == 18.0);
    CHECK(numbered.levels[0].textIndent == 36.0);
    CHECK(numbered.levels[1].markerIndent == 36.0);
    CHECK(numbered.levels[1].textIndent == 54.0);
}

TEST_CASE("Lists: a definition kept as text comes back the same", "[editor][lists]") {
    ListDefinition definition = ListDefinition::numbered();
    definition.levels[0].format = ListNumberFormat::UpperRoman;
    definition.levels[0].suffix = QStringLiteral(")");
    definition.levels[0].start = 4;
    definition.levels[1].format = ListNumberFormat::Bullet;
    definition.levels[1].suffix.clear();
    definition.levels[1].bullet = QStringLiteral(";");  // the separators of the text
    definition.levels[2].format = ListNumberFormat::Bullet;
    definition.levels[2].suffix.clear();
    definition.levels[2].bullet = QStringLiteral("😀");
    definition.levels[3].markerIndent = 12.5;
    definition.levels[3].textIndent = 40.25;
    definition.levels[3].alignment = ListMarkerAlignment::Right;
    CHECK(ListDefinition::fromString(definition.toString()) == definition);

    // Text that cannot be read: a numbered list
    CHECK(ListDefinition::fromString(QStringLiteral("nonsense")) == ListDefinition::numbered());
}

TEST_CASE("Lists: the items of a document and their numbers", "[editor][lists]") {
    SECTION("A deeper level starts again after an item of a lower one") {
        QTextDocument document;
        fill(document, {{1, 0}, {1, 1}, {1, 1}, {1, 0}, {1, 1}, {1, 2}, {1, 0}});
        CHECK(listMarkers(document) == std::vector<QString>{QStringLiteral("1."), QStringLiteral("a)"),
                                                            QStringLiteral("b)"), QStringLiteral("2."),
                                                            QStringLiteral("a)"), QStringLiteral("i."),
                                                            QStringLiteral("3.")});
    }

    SECTION("A list goes on over paragraphs outside it; another list has its own numbers") {
        QTextDocument document;
        fill(document, {{1, 0}, {0, 0}, {1, 0}, {2, 0}, {2, 0}, {1, 0}});
        CHECK(listNumbers(document) == std::vector<int>{1, -1, 2, 1, 2, 3});
    }

    SECTION("An item with its own number, an item without one, a level's start") {
        QTextDocument document;
        ListDefinition fromFive = ListDefinition::numbered();
        fromFive.levels[0].start = 5;
        fill(document, {{1, 0, std::nullopt, true, fromFive},
                        {1, 0, std::nullopt, false, fromFive},
                        {1, 0, std::nullopt, true, fromFive},
                        {1, 0, 10, true, fromFive},
                        {1, 0, std::nullopt, true, fromFive}});
        CHECK(listNumbers(document) == std::vector<int>{5, -1, 6, 10, 11});
        CHECK(listMarkers(document).at(1).isEmpty());
    }

    SECTION("A bulleted item shows its bullet") {
        QTextDocument document;
        fill(document, {{1, 0, std::nullopt, true, ListDefinition::bulleted()},
                        {1, 1, std::nullopt, true, ListDefinition::bulleted()}});
        CHECK(listMarkers(document) == std::vector<QString>{QStringLiteral("•"), QStringLiteral("◦")});
    }

    SECTION("A new id and the ids of pasted lists") {
        QTextDocument document;
        fill(document, {{1, 0}, {3, 0}});
        CHECK(newListId(document) == 4);

        QTextDocument content;
        fill(content, {{1, 0}, {0, 0}, {2, 0}});
        giveListsNewIds(content, document);
        CHECK(listItemOf(content.findBlockByNumber(0).blockFormat())->id == 4);
        CHECK_FALSE(listItemOf(content.findBlockByNumber(1).blockFormat()).has_value());
        CHECK(listItemOf(content.findBlockByNumber(2).blockFormat())->id == 5);
    }

    SECTION("A block format out of its list") {
        QTextBlockFormat format;
        ListItem item;
        item.id = 2;
        item.value = 3;
        item.numbered = false;
        setListItem(format, item);
        REQUIRE(isListItem(format));
        CHECK(*listItemOf(format) == item);
        clearListItem(format);
        CHECK_FALSE(isListItem(format));
        CHECK(format.properties().isEmpty());
    }
}

// =============================================================================
// KML
// =============================================================================

TEST_CASE("Lists in KML: read and written back", "[editor][lists][kml]") {
    SECTION("A numbered list with a list of a deeper level in an item") {
        const QString kml = QStringLiteral(
            "<kml><p>Before</p><ol><li>One</li><li>Two<ol type=\"a\" suffix=\")\"><li>Two a</li>"
            "<li>Two b</li></ol></li><li>Three</li></ol><p>After</p></kml>");
        BookEditor editor;
        editor.fromKml(kml);
        CHECK(editor.plainText() == QStringLiteral("Before\nOne\nTwo\nTwo a\nTwo b\nThree\nAfter"));
        CHECK(markersOf(editor) == std::vector<QString>{QString(), QStringLiteral("1."), QStringLiteral("2."),
                                                        QStringLiteral("a)"), QStringLiteral("b)"),
                                                        QStringLiteral("3."), QString()});
        CHECK(itemOf(editor, 3)->level == 1);
        CHECK(itemOf(editor, 3)->id == itemOf(editor, 1)->id);
        CHECK(editor.toKml() == kml);
    }

    SECTION("The formats of the levels") {
        for (const char* text :
             {"<kml><ul><li>Disc</li></ul></kml>",
              "<kml><ul type=\"check\"><li>Check</li></ul></kml>",
              "<kml><ul type=\"★\"><li>Star</li></ul></kml>",
              "<kml><ol type=\"I\" start=\"4\"><li>Four</li></ol></kml>",
              "<kml><ol type=\"A\" suffix=\")\" marker-indent=\"10\" text-indent=\"30\" "
              "marker-align=\"right\"><li>A</li></ol></kml>",
              "<kml><ol><li>One<ul type=\"dash\"><li>Dash</li></ul></li></ol></kml>"}) {
            const QString kml = QString::fromUtf8(text);
            DYNAMIC_SECTION(text) {
                CHECK(roundTrip(kml) == kml);
            }
        }
        BookEditor editor;
        editor.fromKml(QStringLiteral("<kml><ol type=\"I\" start=\"4\"><li>Four</li><li>Five</li></ol>"
                                      "<ul type=\"square\"><li>Square</li></ul></kml>"));
        CHECK(markersOf(editor) ==
              std::vector<QString>{QStringLiteral("IV."), QStringLiteral("V."), QStringLiteral("▪")});
        const ListLevel level = itemOf(editor, 0)->levelFormat();
        CHECK(level.format == ListNumberFormat::UpperRoman);
        CHECK(level.start == 4);
    }

    SECTION("A list that goes on, an item with its own number, an item without one") {
        const QString kml = QStringLiteral(
            "<kml><ol><li>One</li><li numbered=\"false\">Under one</li></ol><p>Between</p>"
            "<ol continue=\"true\"><li>Two</li><li value=\"7\">Seven</li><li>Eight</li></ol>"
            "<ol><li>New one</li></ol></kml>");
        BookEditor editor;
        editor.fromKml(kml);
        CHECK(markersOf(editor) == std::vector<QString>{QStringLiteral("1."), QString(), QString(),
                                                        QStringLiteral("2."), QStringLiteral("7."),
                                                        QStringLiteral("8."), QStringLiteral("1.")});
        CHECK(itemOf(editor, 3)->id == itemOf(editor, 0)->id);
        CHECK(itemOf(editor, 6)->id != itemOf(editor, 0)->id);
        CHECK(editor.toKml() == kml);
    }

    SECTION("Two lists in turn keep their numbers") {
        // The first list comes back after another one: its item keeps its number
        CHECK(kmlOfEntries({{1, 0}, {1, 0}, {2, 0}, {1, 0}}) == QStringLiteral("<kml><ol><li>Text 0</li><li>Text 1</li></ol><ol><li>Text 2</li>"
                                    "</ol><ol><li value=\"3\">Text 3</li></ol></kml>"));
    }

    SECTION("Items with their style, alignment and own formatting") {
        const QString kml = QStringLiteral(
            "<kml><ul><li style=\"quote\" align=\"center\" space-after=\"6\">Quoted</li></ul></kml>");
        BookEditor editor;
        editor.fromKml(kml);
        const QTextBlockFormat format = editor.textDocument()->firstBlock().blockFormat();
        CHECK(paragraphStyleOf(format) == QStringLiteral("quote"));
        CHECK(paragraphFormatOf(format).spaceAfter == 6.0);
        CHECK(editor.toKml() == kml);
    }

    SECTION("Lists as other programs and people write them") {
        BookEditor editor;
        // Indented KML, a paragraph in an item, text after a list inside an item, a list of
        // a deeper level without an item of its own, elements a list does not know
        editor.fromKml(QStringLiteral(
            "<kml>\n  <ol>\n    <li><p>In a paragraph</p></li>\n    <li>Text\n      <ol>\n"
            "        <li>Inner</li>\n      </ol>\n    and after</li>\n    <ol><li>Orphan</li></ol>\n"
            "    <img src=\"x.png\"/>\n  </ol>\n</kml>"));
        CHECK(editor.plainText() == QStringLiteral("In a paragraph\nText\nInner\nand after\nOrphan"));
        // <ol> without a type is numbered 1., 2.; an item without a number leaves the
        // numbers of the deeper level going on
        CHECK(markersOf(editor) == std::vector<QString>{QStringLiteral("1."), QStringLiteral("2."),
                                                        QStringLiteral("1."), QString(),
                                                        QStringLiteral("2.")});
        CHECK(itemOf(editor, 3)->level == 0);
        CHECK_FALSE(itemOf(editor, 3)->numbered);
        CHECK(itemOf(editor, 4)->level == 1);
        CHECK(editor.toKml() ==
              QStringLiteral("<kml><ol><li>In a paragraph</li><li>Text<ol><li>Inner</li></ol></li>"
                             "<li numbered=\"false\">and after<ol><li>Orphan</li></ol></li></ol></kml>"));
    }

    SECTION("An item of a level of its own, without the items above it") {
        const QString kml = kmlOfEntries({{1, 2}});
        CHECK(kml == QStringLiteral("<kml><ol><ol type=\"a\" suffix=\")\"><ol type=\"i\"><li>Text 0</li>"
                                    "</ol></ol></ol></kml>"));
        CHECK(roundTrip(kml) == kml);
    }
}

TEST_CASE("Paragraph formatting in KML: read and written back", "[editor][kml][paragraph]") {
    const QString kml = QStringLiteral(
        "<kml><p indent-left=\"36\" indent-right=\"12.5\" indent-first=\"-18\" space-before=\"6\" "
        "space-after=\"0\" line-height=\"1.5\" keep-with-next=\"true\" keep-together=\"true\" "
        "page-break-before=\"true\" widow-control=\"false\">Text</p>"
        "<h1 line-height-exact=\"20\">Heading</h1><p line-height-min=\"14\">At least</p></kml>");
    BookEditor editor;
    editor.fromKml(kml);
    const ParagraphFormat format = paragraphFormatOf(editor.textDocument()->firstBlock().blockFormat());
    CHECK(format.leftIndent == 36.0);
    CHECK(format.rightIndent == 12.5);
    CHECK(format.firstLineIndent == -18.0);
    CHECK(format.spaceBefore == 6.0);
    CHECK(format.spaceAfter == 0.0);
    CHECK(format.lineSpacing == LineSpacing{LineSpacingRule::Multiple, 1.5});
    CHECK(format.keepWithNext == true);
    CHECK(format.keepTogether == true);
    CHECK(format.pageBreakBefore == true);
    CHECK(format.widowControl == false);
    CHECK(paragraphFormatOf(editor.textDocument()->findBlockByNumber(1).blockFormat()).lineSpacing ==
          LineSpacing{LineSpacingRule::Exactly, 20.0});
    CHECK(editor.toKml() == kml);

    SECTION("Values out of range come into it; others are ignored") {
        QXmlStreamReader reader(QStringLiteral(
            "<p indent-left=\"5000\" space-before=\"-3\" line-height=\"9\" space-after=\"x\"/>"));
        reader.readNextStartElement();
        const ParagraphFormat read = paragraphFormatFromKml(reader.attributes());
        CHECK(read.leftIndent == MAX_PARAGRAPH_INDENT);
        CHECK(read.spaceBefore == 0.0);
        CHECK(read.lineSpacing == LineSpacing{LineSpacingRule::Multiple, MAX_LINE_SPACING_MULTIPLE});
        CHECK_FALSE(read.spaceAfter.has_value());
    }

    SECTION("Numbers as KML writes them") {
        CHECK(kmlNumber(12.0) == QStringLiteral("12"));
        CHECK(kmlNumber(12.5) == QStringLiteral("12.5"));
        CHECK(kmlNumber(35.433) == QStringLiteral("35.43"));
        CHECK(kmlNumber(-0.001) == QStringLiteral("0"));
        CHECK(kmlNumber(10.0) == QStringLiteral("10"));
    }
}
