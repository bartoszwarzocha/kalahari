/// @file test_style_draft.cpp
/// @brief A paragraph style as the window of a style changes it: what it sets itself, what it
///        takes from its base style, the sentence of what it is, and the entry the book keeps

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/editor/paragraph_styles.h"
#include "kalahari/gui/style_draft.h"
#include "kalahari/gui/widgets/length_spin_box.h"

#include <QList>
#include <QString>
#include <QVariantMap>

using namespace kalahari;
using gui::StyleDraft;

namespace {

/// A style of the book's type, named in English
core::PackageParagraphStyle typeStyle(const QString& id, const QString& name,
                                      const QString& base, const QVariantMap& properties,
                                      const QString& next = QString()) {
    core::PackageParagraphStyle style;
    style.id = id;
    style.name.values.insert(QStringLiteral("en"), name);
    style.baseStyle = base;
    style.nextStyle = next;
    style.properties = properties;
    return style;
}

/// The styles of the type: Normal, Quote in italics with indents, a heading followed by Normal
QList<core::PackageParagraphStyle> typeStyles() {
    return {typeStyle(QStringLiteral("normal"), QStringLiteral("Normal"), QString(),
                      {{QStringLiteral("fontSize"), 12}, {QStringLiteral("lineHeight"), 1.5}}),
            typeStyle(QStringLiteral("quote"), QStringLiteral("Quote"), QStringLiteral("normal"),
                      {{QStringLiteral("italic"), true},
                       {QStringLiteral("leftMargin"), 36},
                       {QStringLiteral("rightMargin"), 36}}),
            typeStyle(QStringLiteral("heading_1"), QStringLiteral("Heading 1"),
                      QStringLiteral("normal"),
                      {{QStringLiteral("fontSize"), 20}, {QStringLiteral("bold"), true}},
                      QStringLiteral("normal"))};
}

/// The styles of a book of the type, with its entries
editor::ParagraphStyleSet bookStyles(const QList<core::BookParagraphStyle>& entries = {}) {
    return editor::ParagraphStyleSet::forBook(typeStyles(), entries, QStringLiteral("en"));
}

}  // namespace

TEST_CASE("Style draft: a new style takes the formatting of the paragraph", "[gui][styles]") {
    StyleDraft draft = StyleDraft::newStyle(bookStyles(), QStringLiteral("quote"),
                                            {{QStringLiteral("italic"), true},
                                             {QStringLiteral("fontSize"), 11}});
    CHECK(draft.kind() == StyleDraft::Kind::New);
    CHECK_FALSE(draft.isFixed());
    CHECK(draft.id().isEmpty());
    CHECK(draft.name() == QStringLiteral("Style 1"));
    CHECK(draft.baseStyle() == QStringLiteral("quote"));

    // Italics are Quote's: not its own; the size is
    CHECK_FALSE(draft.isOwn(QStringLiteral("italic")));
    CHECK(draft.isOwn(QStringLiteral("fontSize")));
    CHECK(draft.ownProperties() == QVariantMap{{QStringLiteral("fontSize"), 11}});
    CHECK(draft.value(QStringLiteral("leftMargin")).toDouble() == 36.0);
    CHECK(draft.value(QStringLiteral("lineHeight")).toDouble() == 1.5);
    CHECK_FALSE(draft.value(QStringLiteral("bold")).toBool());
    CHECK(draft.value(QStringLiteral("spellCheck")).toBool());
    CHECK(draft.look().size() == StyleDraft::propertyNames().size());
    CHECK(draft.summary() == QStringLiteral("Like the style \"Quote\", and also: size 11 pt."));
    CHECK(draft.canReset());

    // The book keeps it under an id of its name
    draft.setName(QStringLiteral("Memory"));
    const std::optional<core::BookParagraphStyle> entry = draft.entry(QStringLiteral("memory"));
    REQUIRE(entry.has_value());
    CHECK(entry->id == QStringLiteral("memory"));
    CHECK(entry->name == QStringLiteral("Memory"));
    CHECK(entry->baseStyle == QStringLiteral("quote"));
    CHECK(entry->properties == QVariantMap{{QStringLiteral("fontSize"), 11}});

    // A value of the base style is the base style's again
    draft.setValue(QStringLiteral("fontSize"), 12.0);
    CHECK(draft.ownProperties().isEmpty());
    CHECK(draft.summary() == QStringLiteral("Everything as in the style \"Quote\" for now."));
    CHECK_FALSE(draft.canReset());

    // Another base: what is its own stays its own
    draft.setValue(QStringLiteral("bold"), true);
    draft.setBaseStyle(QStringLiteral("heading_1"));
    CHECK(draft.baseStyle() == QStringLiteral("heading_1"));
    CHECK(draft.isOwn(QStringLiteral("bold")));
    CHECK(draft.value(QStringLiteral("fontSize")).toDouble() == 20.0);
    draft.setBaseStyle(QStringLiteral("nothing"));  // not a style of the book
    CHECK(draft.baseStyle() == QStringLiteral("heading_1"));
    draft.reset();
    CHECK(draft.ownProperties().isEmpty());

    // Enter goes on in the same style: its own id is the same as none
    draft.setNextStyle(QStringLiteral("normal"));
    CHECK(draft.nextStyle() == QStringLiteral("normal"));
}

TEST_CASE("Style draft: the name of a new style", "[gui][styles]") {
    core::BookParagraphStyle first;
    first.id = QStringLiteral("style_1");
    first.name = QStringLiteral("Style 1");
    first.baseStyle = QStringLiteral("normal");
    const editor::ParagraphStyleSet styles = bookStyles({first});

    // The first number the book does not have; a base it does not have is Normal
    StyleDraft draft = StyleDraft::newStyle(styles, QStringLiteral("unknown"));
    CHECK(draft.name() == QStringLiteral("Style 2"));
    CHECK(draft.baseStyle() == QStringLiteral("normal"));
    CHECK(draft.problem().isEmpty());

    // No name, or the name of another style of the book, in any case
    draft.setName(QStringLiteral("   "));
    CHECK(draft.problem() == QStringLiteral("Give the style a name."));
    draft.setName(QStringLiteral("quote"));
    CHECK(draft.problem() == QStringLiteral("The book has a style \"Quote\" already."));
    draft.setName(QStringLiteral("style 1"));
    CHECK(draft.problem() == QStringLiteral("The book has a style \"Style 1\" already."));
    draft.setName(QStringLiteral("  Dream  "));
    CHECK(draft.problem().isEmpty());
    REQUIRE(draft.entry(QStringLiteral("dream")).has_value());
    CHECK(draft.entry(QStringLiteral("dream"))->name == QStringLiteral("Dream"));

    // A style of the writer's own keeps its own name, not taken by itself
    StyleDraft own = StyleDraft::existing(styles, typeStyles(), QStringLiteral("style_1"));
    CHECK(own.kind() == StyleDraft::Kind::Own);
    CHECK(own.problem().isEmpty());
    own.setName(QStringLiteral("Memory"));
    CHECK(own.name() == QStringLiteral("Memory"));
    REQUIRE(own.entry().has_value());
    CHECK(own.entry()->id == QStringLiteral("style_1"));
}

TEST_CASE("Style draft: a style of the type changed in the book and reset", "[gui][styles]") {
    StyleDraft draft = StyleDraft::existing(bookStyles(), typeStyles(), QStringLiteral("quote"));
    CHECK(draft.kind() == StyleDraft::Kind::Type);
    CHECK(draft.isFixed());
    CHECK(draft.name() == QStringLiteral("Quote"));
    CHECK(draft.baseStyle() == QStringLiteral("normal"));
    CHECK_FALSE(draft.canReset());
    CHECK_FALSE(draft.entry().has_value());  // as the type has it

    // Its name and base stay
    draft.setName(QStringLiteral("Citation"));
    draft.setBaseStyle(QStringLiteral("heading_1"));
    CHECK(draft.name() == QStringLiteral("Quote"));
    CHECK(draft.baseStyle() == QStringLiteral("normal"));

    // Changed in this book
    draft.setValue(QStringLiteral("italic"), false);
    draft.setNextStyle(QStringLiteral("heading_1"));
    CHECK(draft.canReset());
    CHECK(draft.summary().endsWith(
        QStringLiteral(" Changed in this book: not italic; the next paragraph in the style "
                       "\"Heading 1\".")));
    const std::optional<core::BookParagraphStyle> entry = draft.entry();
    REQUIRE(entry.has_value());
    CHECK(entry->id == QStringLiteral("quote"));
    CHECK(entry->name.isEmpty());
    CHECK(entry->baseStyle.isEmpty());
    CHECK(entry->nextStyle == QStringLiteral("heading_1"));
    // Not italic is Normal's: Quote no longer sets it; its indents it does
    CHECK_FALSE(entry->properties.contains(QStringLiteral("italic")));
    CHECK(entry->properties.value(QStringLiteral("leftMargin")).toInt() == 36);

    // The book's change is a style of the type changed: Reset brings back the type's
    const editor::ParagraphStyleSet changed = bookStyles({*entry});
    CHECK(changed.find(QStringLiteral("quote"))->origin == editor::StyleOrigin::Changed);
    StyleDraft again = StyleDraft::existing(changed, typeStyles(), QStringLiteral("quote"));
    CHECK(again.canReset());
    CHECK_FALSE(again.value(QStringLiteral("italic")).toBool());
    again.reset();
    CHECK_FALSE(again.canReset());
    CHECK(again.value(QStringLiteral("italic")).toBool());
    CHECK(again.nextStyle().isEmpty());
    CHECK_FALSE(again.entry().has_value());
}

TEST_CASE("Style draft: Normal sets what differs from the program", "[gui][styles]") {
    StyleDraft draft = StyleDraft::existing(bookStyles(), typeStyles(), QStringLiteral("normal"));
    CHECK(draft.kind() == StyleDraft::Kind::Normal);
    CHECK(draft.isFixed());
    CHECK(draft.baseStyle().isEmpty());
    CHECK_FALSE(draft.isOwn(QStringLiteral("fontSize")));  // no base to differ from
    CHECK(draft.value(QStringLiteral("fontSize")).toDouble() == 12.0);
    CHECK(draft.value(QStringLiteral("alignment")).toString() == QStringLiteral("justify"));
    CHECK(draft.summary().startsWith(QStringLiteral("The base style: ")));
    CHECK(draft.summary().contains(QStringLiteral(" 12 pt, justified, ")));
    CHECK_FALSE(draft.canReset());

    draft.setValue(QStringLiteral("fontSize"), 13);
    CHECK(draft.canReset());
    CHECK(draft.summary().endsWith(QStringLiteral(" Changed in this book: size 13 pt.")));
    REQUIRE(draft.entry().has_value());
    CHECK(draft.entry()->properties.value(QStringLiteral("fontSize")).toDouble() == 13.0);
}

TEST_CASE("Style draft: the settings in words", "[gui][styles]") {
    using gui::lengthText;
    using gui::LengthUnit;
    CHECK(StyleDraft::phrase(QStringLiteral("fontFamily"), QStringLiteral("Georgia")) ==
          QStringLiteral("font Georgia"));
    CHECK(StyleDraft::phrase(QStringLiteral("fontSize"), 10.5) == QStringLiteral("size 10.5 pt"));
    CHECK(StyleDraft::phrase(QStringLiteral("bold"), false) == QStringLiteral("not bold"));
    CHECK(StyleDraft::phrase(QStringLiteral("alignment"), QStringLiteral("center")) ==
          QStringLiteral("centered"));
    CHECK(StyleDraft::phrase(QStringLiteral("firstLineIndent"), 18) ==
          QStringLiteral("first line ") + lengthText(18, LengthUnit::Points));
    CHECK(StyleDraft::phrase(QStringLiteral("firstLineIndent"), -18) ==
          QStringLiteral("hanging ") + lengthText(18, LengthUnit::Points));
    CHECK(StyleDraft::phrase(QStringLiteral("firstLineIndent"), 0) ==
          QStringLiteral("no first line indent"));
    CHECK(StyleDraft::phrase(QStringLiteral("lineHeight"), 1.0) ==
          QStringLiteral("single line spacing"));
    CHECK(StyleDraft::phrase(QStringLiteral("lineHeight"), 1.5) ==
          QStringLiteral("line spacing 1.5 lines"));
    CHECK(StyleDraft::phrase(QStringLiteral("lineHeight"), 1.15) ==
          QStringLiteral("line spacing multiple 1.15"));
    CHECK(StyleDraft::phrase(QStringLiteral("spellCheck"), false) ==
          QStringLiteral("spelling not checked"));

    // Numbers to a hundredth; a flag is not a number
    CHECK(StyleDraft::same(12, 12.001));
    CHECK_FALSE(StyleDraft::same(12, 12.1));
    CHECK(StyleDraft::same(true, true));
    CHECK_FALSE(StyleDraft::same(true, false));
    CHECK_FALSE(StyleDraft::same(QVariant(), false));
    CHECK(StyleDraft::defaultValue(QStringLiteral("widowControl")).toBool());
    CHECK_FALSE(StyleDraft::defaultValue(QStringLiteral("nothing")).isValid());
}
