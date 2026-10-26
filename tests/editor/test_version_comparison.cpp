/// @file test_version_comparison.cpp
/// @brief A version of a text compared with the text as it is now: the comparison an editor
/// shows, its changes, and the fragments of the version it leads to

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/version_comparison.h>
#include "editor_test_utils.h"

#include <QTextBlock>
#include <QTextDocument>

#include <memory>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

const ComparisonColors kColors{QColor(QStringLiteral("#c62828")),
                               QColor(QStringLiteral("#2e7d32"))};

/// An editor with text @p kml, as the window of the versions shows it
std::unique_ptr<BookEditor> shown(const QString& kml) {
    auto editor = std::make_unique<BookEditor>();
    editor->fromKml(kml);
    return editor;
}

/// The text of range @p range of the document of @p editor
QString textOf(const BookEditor& editor, const ComparisonRange& range) {
    return editor.textDocument()->toRawText().mid(range.start, range.end - range.start);
}

/// The paragraphs of the document of @p editor
QStringList paragraphsOf(const BookEditor& editor) {
    QStringList paragraphs;
    for (size_t i = 0; i < editor.paragraphCount(); ++i) {
        paragraphs << editor.paragraphPlainText(i);
    }
    return paragraphs;
}

}  // namespace

TEST_CASE("Version comparison: the same text has no changes and reads as it is",
          "[editor][versions]") {
    const QString kml = kmlOf({QStringLiteral("The quick fox."), QStringLiteral("The end.")});
    const VersionComparison comparison = compareVersions(kml, kml, kColors);
    CHECK(comparison.isSame());
    CHECK(comparison.deletedWords == 0);
    CHECK(comparison.addedWords == 0);
    const auto editor = shown(comparison.kml);
    CHECK(paragraphsOf(*editor) == QStringList{"The quick fox.", "The end."});
    CHECK_FALSE(comparison.kml.contains(QStringLiteral("<s>")));
    CHECK_FALSE(comparison.kml.contains(QStringLiteral("<u>")));
}

TEST_CASE("Version comparison: a word changed is struck out and the new one underlined",
          "[editor][versions]") {
    const QString older = kmlOf({QStringLiteral("The quick fox.")});
    const QString newer = kmlOf({QStringLiteral("The slow fox.")});
    const VersionComparison comparison = compareVersions(older, newer, kColors);
    REQUIRE(comparison.changes.size() == 1);
    CHECK(comparison.deletedWords == 1);
    CHECK(comparison.addedWords == 1);

    const auto editor = shown(comparison.kml);
    CHECK(paragraphsOf(*editor) == QStringList{"The quickslow fox."});
    CHECK(textOf(*editor, comparison.changes.first()) == QStringLiteral("quickslow"));

    const QTextBlock block = editor->textDocument()->firstBlock();
    const QTextCharFormat deleted = formatOfFragmentContaining(block, QStringLiteral("quick"));
    CHECK(deleted.fontStrikeOut());
    CHECK_FALSE(deleted.fontUnderline());
    CHECK(deleted.foreground().color() == kColors.deleted);
    const QTextCharFormat added = formatOfFragmentContaining(block, QStringLiteral("slow"));
    CHECK(added.fontUnderline());
    CHECK_FALSE(added.fontStrikeOut());
    CHECK(added.foreground().color() == kColors.added);
    const QTextCharFormat same = formatOfFragmentContaining(block, QStringLiteral("The"));
    CHECK_FALSE(same.fontStrikeOut());
    CHECK_FALSE(same.fontUnderline());

    SECTION("a fragment of the comparison is the same fragment of the version") {
        const ComparisonRange all = comparison.olderRange(0, 18);
        CHECK(all == ComparisonRange{0, 14});
        CHECK(kmlFragment(older, all.start, all.end).contains(QStringLiteral("The quick fox.")));
        // "quickslow fox": the version's "quick fox"
        const ComparisonRange words = comparison.olderRange(4, 17);
        CHECK(shown(kmlFragment(older, words.start, words.end))->paragraphPlainText(0) ==
              QStringLiteral("quick fox"));
        // Only what was written later: nothing of the version
        CHECK(comparison.olderRange(9, 13).isEmpty());
    }
}

TEST_CASE("Version comparison: paragraphs deleted and added whole, and the fragments across "
          "them",
          "[editor][versions]") {
    const QString older = kmlOf({QStringLiteral("Storm from the west."),
                                 QStringLiteral("Nadia asked about the water."),
                                 QStringLiteral("Tau was silent.")});
    const QString newer = kmlOf({QStringLiteral("Storm from the west."),
                                 QStringLiteral("Somebody laughed at night, far away."),
                                 QStringLiteral("Tau was silent.")});
    const VersionComparison comparison = compareVersions(older, newer, kColors);
    const auto editor = shown(comparison.kml);
    CHECK(paragraphsOf(*editor) ==
          QStringList{"Storm from the west.", "Nadia asked about the water.",
                      "Somebody laughed at night, far away.", "Tau was silent."});
    // The deleted paragraph and the added one next to it are one change
    REQUIRE(comparison.changes.size() == 1);
    CHECK(textOf(*editor, comparison.changes.first()) ==
          QStringLiteral("Nadia asked about the water. Somebody laughed at night, far away."));
    CHECK(comparison.deletedWords == 5);
    CHECK(comparison.addedWords == 6);

    const QTextBlock deleted = editor->textDocument()->findBlockByNumber(1);
    CHECK(formatOfFragmentContaining(deleted, QStringLiteral("Nadia")).fontStrikeOut());
    const QTextBlock added = editor->textDocument()->findBlockByNumber(2);
    CHECK(formatOfFragmentContaining(added, QStringLiteral("laughed")).fontUnderline());

    // The whole comparison is the whole version, without the paragraph written later
    const int end = editor->textDocument()->characterCount() - 1;
    const ComparisonRange all = comparison.olderRange(0, end);
    CHECK(all == ComparisonRange{0, comparison.olderLength});
    CHECK(paragraphsOf(*shown(kmlFragment(older, all.start, all.end))) ==
          QStringList{"Storm from the west.", "Nadia asked about the water.", "Tau was silent."});
    // From the paragraph written later to the end: the last paragraph of the version
    const int laterStart = deleted.next().position();
    const ComparisonRange last = comparison.olderRange(laterStart, end);
    CHECK(paragraphsOf(*shown(kmlFragment(older, last.start, last.end))) ==
          QStringList{"Tau was silent."});
}

TEST_CASE("Version comparison: the formatting of the texts stays, their annotations do not",
          "[editor][versions]") {
    const QString older = QStringLiteral(
        "<kml><annotations><annotation id=\"a\" kind=\"comment\">Why?</annotation></annotations>"
        "<p>The <b><anchor ref=\"a\">quick</anchor></b> fox.</p></kml>");
    const QString newer = QStringLiteral(
        "<kml><annotations><annotation id=\"a\" kind=\"comment\">Why?</annotation></annotations>"
        "<p>The <b><anchor ref=\"a\">quick</anchor></b> brown fox.</p></kml>");
    const VersionComparison comparison = compareVersions(older, newer, kColors);
    REQUIRE(comparison.changes.size() == 1);
    CHECK_FALSE(comparison.kml.contains(QStringLiteral("annotation")));
    CHECK_FALSE(comparison.kml.contains(QStringLiteral("anchor")));

    const auto editor = shown(comparison.kml);
    const QTextBlock block = editor->textDocument()->firstBlock();
    CHECK(block.text() == QStringLiteral("The quick brown fox."));
    CHECK(formatOfFragmentContaining(block, QStringLiteral("quick")).fontWeight() == QFont::Bold);
    CHECK(formatOfFragmentContaining(block, QStringLiteral("brown")).fontUnderline());

    // The fragment of the version keeps its annotation
    CHECK(kmlFragment(older, 0, 14).contains(QStringLiteral("anchor")));
}

TEST_CASE("Version comparison: an empty version against a text, and a text against none",
          "[editor][versions]") {
    const QString text = kmlOf({QStringLiteral("One."), QStringLiteral("Two.")});
    const QString empty = QStringLiteral("<kml></kml>");

    const VersionComparison added = compareVersions(empty, text, kColors);
    CHECK_FALSE(added.isSame());
    CHECK(added.addedWords == 2);
    CHECK(added.deletedWords == 0);
    CHECK(added.olderRange(0, 9).isEmpty());

    const VersionComparison deleted = compareVersions(text, empty, kColors);
    CHECK_FALSE(deleted.isSame());
    CHECK(deleted.deletedWords == 2);
    const auto editor = shown(deleted.kml);
    const QStringList paragraphs = paragraphsOf(*editor);
    CHECK(paragraphs.contains(QStringLiteral("One.")));
    CHECK(paragraphs.contains(QStringLiteral("Two.")));
}
