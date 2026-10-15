/// @file test_auto_correct.cpp
/// @brief Autocorrect: the typographic characters that take the place of what is typed
///
/// As in Word: straight quotation marks become those of the text's language, an apostrophe
/// the typographic one, a hyphen and a space starting a paragraph a dialogue dash, three
/// periods an ellipsis. Each change is an undo step of its own, so undoing right after it
/// brings back what was typed.

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/annotation.h>
#include <kalahari/editor/auto_correct.h>
#include <kalahari/editor/book_editor.h>
#include "editor_test_utils.h"

#include <QCoreApplication>
#include <QFont>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextFragment>

#include <memory>
#include <optional>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

const QuoteMarks POLISH{u'„', u'”', u'‚', u'’'};
const QuoteMarks ENGLISH{u'“', u'”', u'‘', u'’'};
const QuoteMarks GERMAN{u'„', u'“', u'‚', u'‘'};

/// The options with one change turned off
AutoCorrectOptions without(bool AutoCorrectOptions::*change) {
    AutoCorrectOptions options;
    options.*change = false;
    return options;
}

/// The text after autocorrect has seen the last character of @p before typed
QString corrected(const QString& before, const QuoteMarks& marks = POLISH,
                  const AutoCorrectOptions& options = {}) {
    const std::optional<AutoCorrection> change =
        autoCorrection(before, options, [&marks]() { return marks; });
    if (!change) {
        return before;
    }
    QString text = before;
    return text.replace(change->start, change->end - change->start, change->text);
}

/// An editor with KML loaded
std::unique_ptr<BookEditor> editorWith(const QString& kml) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(600, 400));
    editor->fromKml(kml);
    return editor;
}

/// An editor with an empty paragraph, in a language
std::unique_ptr<BookEditor> emptyEditor(const QString& language = QString()) {
    auto editor = editorWith(kmlOf({QString()}));
    if (!language.isEmpty()) {
        editor->setTextLanguage([language]() { return language; });
    }
    return editor;
}

/// Type the text as the keyboard does, a character at a time
void type(BookEditor& editor, const QString& text) {
    for (const QChar ch : text) {
        QKeyEvent event(QEvent::KeyPress, 0, Qt::NoModifier, QString(ch));
        QCoreApplication::sendEvent(&editor, &event);
    }
}

/// The text of a paragraph that has a format
QString textWith(const BookEditor& editor, bool (*has)(const QTextCharFormat&)) {
    QString text;
    const QTextBlock block = editor.textDocument()->findBlockByNumber(0);
    for (auto it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment fragment = it.fragment();
        if (fragment.isValid() && has(fragment.charFormat())) {
            text += fragment.text();
        }
    }
    return text;
}

bool isBold(const QTextCharFormat& format) {
    return format.fontWeight() >= QFont::Bold;
}

bool isItalic(const QTextCharFormat& format) {
    return format.fontItalic();
}

/// Where the annotations of an editor are, in text order: "id start-end" for a fragment,
/// "id @place" for a place
QString places(const BookEditor& editor) {
    QStringList list;
    for (const AnnotationPlace& place : editor.annotations()) {
        list << (place.annotation.point
                     ? QStringLiteral("%1 @%2").arg(place.annotation.id).arg(place.start)
                     : QStringLiteral("%1 %2-%3")
                           .arg(place.annotation.id)
                           .arg(place.start)
                           .arg(place.end));
    }
    return list.join(QStringLiteral(", "));
}

}  // anonymous namespace

// =============================================================================
// The changes
// =============================================================================

TEST_CASE("Autocorrect: the quotation marks of a language", "[editor][autocorrect]") {
    // Polish and English have the marks of the plan of the editor
    for (const char* code : {"pl", "pl_PL", "pl-PL", " pl "}) {
        INFO(code);
        CHECK(QuoteMarks::forLanguage(QString::fromLatin1(code)) == POLISH);
    }
    for (const char* code : {"en", "en_GB", "en-US"}) {
        INFO(code);
        CHECK(QuoteMarks::forLanguage(QString::fromLatin1(code)) == ENGLISH);
    }

    // Another language has those of Qt's locale data
    CHECK(QuoteMarks::forLanguage(QStringLiteral("de_DE")) == GERMAN);
    const QuoteMarks french = QuoteMarks::forLanguage(QStringLiteral("fr"));
    CHECK(french.doubleOpening == u'«');
    CHECK(french.doubleClosing == u'»');

    // None, or one Qt does not know: English
    for (const char* code : {"", "xx", "C"}) {
        INFO(code);
        CHECK(QuoteMarks::forLanguage(QString::fromLatin1(code)) == ENGLISH);
    }
}

TEST_CASE("Autocorrect: a quotation mark opens a quotation after a space, closes it after a "
          "word",
          "[editor][autocorrect]") {
    // Opening: at the start of the paragraph, after a space, a bracket, a dash
    CHECK(corrected(QStringLiteral("\"")) == QStringLiteral("„"));
    CHECK(corrected(QStringLiteral("Ala \"")) == QStringLiteral("Ala „"));
    CHECK(corrected(QStringLiteral("(\"")) == QStringLiteral("(„"));
    CHECK(corrected(QStringLiteral("[\"")) == QStringLiteral("[„"));
    CHECK(corrected(QStringLiteral("—\"")) == QStringLiteral("—„"));
    CHECK(corrected(QStringLiteral("Ala") + QChar(QChar::LineSeparator) + QStringLiteral("\"")) ==
          QStringLiteral("Ala") + QChar(QChar::LineSeparator) + QStringLiteral("„"));

    // Closing: after a word, a punctuation mark or a closing quotation mark
    CHECK(corrected(QStringLiteral("„Ala\"")) == QStringLiteral("„Ala”"));
    CHECK(corrected(QStringLiteral("„Tak,\"")) == QStringLiteral("„Tak,”"));
    CHECK(corrected(QStringLiteral("„Tak?\"")) == QStringLiteral("„Tak?”"));

    // A quotation in a quotation opens after the opening mark of the other kind; after one
    // of its own kind a mark closes the pair
    CHECK(corrected(QStringLiteral("‚\"")) == QStringLiteral("‚„"));
    CHECK(corrected(QStringLiteral("„\"")) == QStringLiteral("„”"));

    // The marks of the language
    CHECK(corrected(QStringLiteral("say \""), ENGLISH) == QStringLiteral("say “"));
    CHECK(corrected(QStringLiteral("“Hi\""), ENGLISH) == QStringLiteral("“Hi”"));
    CHECK(corrected(QStringLiteral("„Wort\""), GERMAN) == QStringLiteral("„Wort“"));
}

TEST_CASE("Autocorrect: an apostrophe, and the single quotation marks", "[editor][autocorrect]") {
    // An apostrophe in a word
    CHECK(corrected(QStringLiteral("don'"), ENGLISH) == QStringLiteral("don’"));
    CHECK(corrected(QStringLiteral("Kennedy'")) == QStringLiteral("Kennedy’"));

    // At the start of a word it opens a quotation, also in a quotation of the other kind
    CHECK(corrected(QStringLiteral("'")) == QStringLiteral("‚"));
    CHECK(corrected(QStringLiteral("Ala '")) == QStringLiteral("Ala ‚"));
    CHECK(corrected(QStringLiteral("„'")) == QStringLiteral("„‚"));
    CHECK(corrected(QStringLiteral("‚'")) == QStringLiteral("‚’"));

    // After a word it closes the quotation in single marks that is open, or else it is an
    // apostrophe; the German closing mark is not the apostrophe
    CHECK(corrected(QStringLiteral("‚tak'")) == QStringLiteral("‚tak’"));
    CHECK(corrected(QStringLiteral("‚Wort'"), GERMAN) == QStringLiteral("‚Wort‘"));
    CHECK(corrected(QStringLiteral("geht'"), GERMAN) == QStringLiteral("geht’"));
    CHECK(corrected(QStringLiteral("‚Wort‘ und geht'"), GERMAN) ==
          QStringLiteral("‚Wort‘ und geht’"));
    CHECK(corrected(QStringLiteral("‚Peter’s Haus'"), GERMAN) == QStringLiteral("‚Peter’s Haus‘"));

    // French has the same single marks as double ones: after a word, an apostrophe
    const QuoteMarks french{u'«', u'»', u'«', u'»'};
    CHECK(corrected(QStringLiteral("«l'"), french) == QStringLiteral("«l’"));
}

TEST_CASE("Autocorrect: a hyphen and a space starting a paragraph make a dialogue dash",
          "[editor][autocorrect]") {
    CHECK(corrected(QStringLiteral("- ")) == QStringLiteral("— "));
    AutoCorrectOptions enDash;
    enDash.dash = DialogueDash::En;
    CHECK(corrected(QStringLiteral("- "), POLISH, enDash) == QStringLiteral("– "));

    // Only at the start of the paragraph, and only a hyphen alone
    for (const QString& text : {QStringLiteral("Ala - "), QStringLiteral(" - "),
                                QStringLiteral("-- "), QStringLiteral("—- "),
                                QStringLiteral("Ala") + QChar(QChar::LineSeparator) +
                                    QStringLiteral("- ")}) {
        INFO(text.toStdString());
        CHECK(corrected(text) == text);
    }
}

TEST_CASE("Autocorrect: three periods make an ellipsis", "[editor][autocorrect]") {
    CHECK(corrected(QStringLiteral("Wiem...")) == QStringLiteral("Wiem…"));
    CHECK(corrected(QStringLiteral("...")) == QStringLiteral("…"));
    CHECK(corrected(QStringLiteral("„Tak...")) == QStringLiteral("„Tak…"));

    // Not two, and not four: a fourth period before them was meant
    CHECK(corrected(QStringLiteral("Wiem..")) == QStringLiteral("Wiem.."));
    CHECK(corrected(QStringLiteral("Wiem....")) == QStringLiteral("Wiem...."));
    CHECK(corrected(QStringLiteral("....")) == QStringLiteral("...."));
}

TEST_CASE("Autocorrect: each change can be turned off", "[editor][autocorrect]") {
    SECTION("Quotation marks: the apostrophe in a word stays on") {
        const AutoCorrectOptions options = without(&AutoCorrectOptions::quotes);
        CHECK(corrected(QStringLiteral("Ala \""), POLISH, options) == QStringLiteral("Ala \""));
        CHECK(corrected(QStringLiteral("„Ala\""), POLISH, options) == QStringLiteral("„Ala\""));
        CHECK(corrected(QStringLiteral("'"), POLISH, options) == QStringLiteral("'"));
        CHECK(corrected(QStringLiteral("‚Wort'"), GERMAN, options) == QStringLiteral("‚Wort’"));
        CHECK(corrected(QStringLiteral("don'"), ENGLISH, options) == QStringLiteral("don’"));
    }

    SECTION("The apostrophe: the single quotation marks stay on") {
        const AutoCorrectOptions options = without(&AutoCorrectOptions::apostrophe);
        CHECK(corrected(QStringLiteral("don'"), ENGLISH, options) == QStringLiteral("don'"));
        CHECK(corrected(QStringLiteral("'"), POLISH, options) == QStringLiteral("‚"));
        CHECK(corrected(QStringLiteral("‚tak'"), POLISH, options) == QStringLiteral("‚tak’"));
    }

    SECTION("Both") {
        AutoCorrectOptions options = without(&AutoCorrectOptions::quotes);
        options.apostrophe = false;
        CHECK(corrected(QStringLiteral("don'"), ENGLISH, options) == QStringLiteral("don'"));
        CHECK(corrected(QStringLiteral("‚tak'"), POLISH, options) == QStringLiteral("‚tak'"));
    }

    SECTION("The dialogue dash") {
        CHECK(corrected(QStringLiteral("- "), POLISH,
                        without(&AutoCorrectOptions::dialogueDash)) == QStringLiteral("- "));
    }

    SECTION("The ellipsis") {
        CHECK(corrected(QStringLiteral("Wiem..."), POLISH,
                        without(&AutoCorrectOptions::ellipsis)) == QStringLiteral("Wiem..."));
    }
}

TEST_CASE("Autocorrect: what is looked at after a character is typed", "[editor][autocorrect]") {
    for (const QChar ch : QStringLiteral("\"' .")) {
        CHECK(startsAutoCorrection(ch));
    }
    for (const QChar ch : QStringLiteral("a-,…„")) {
        CHECK_FALSE(startsAutoCorrection(ch));
    }

    // The language's marks are asked for only for a quotation mark
    int asked = 0;
    const auto marks = [&asked]() {
        ++asked;
        return POLISH;
    };
    CHECK_FALSE(autoCorrection(QString(), {}, marks).has_value());
    CHECK_FALSE(autoCorrection(QStringLiteral("Ala"), {}, marks).has_value());
    CHECK(autoCorrection(QStringLiteral("- "), {}, marks).has_value());
    CHECK(autoCorrection(QStringLiteral("Ala..."), {}, marks).has_value());
    CHECK(asked == 0);
    CHECK(autoCorrection(QStringLiteral("Ala \""), {}, marks).has_value());
    CHECK(asked == 1);
}

// =============================================================================
// In the editor
// =============================================================================

TEST_CASE("Autocorrect: the editor changes the text as it is typed", "[editor][autocorrect]") {
    SECTION("A chapter in Polish") {
        auto editor = emptyEditor(QStringLiteral("pl"));
        type(*editor, QStringLiteral("- \"Tak,\" powiedział... 'Lalka' Prusa i Kennedy'ego"));
        CHECK(editor->plainText() ==
              QStringLiteral("— „Tak,” powiedział… ‚Lalka’ Prusa i Kennedy’ego"));
        CHECK(editor->cursorPosition() ==
              CursorPosition{0, static_cast<int>(editor->plainText().size())});
    }

    SECTION("Without a language: the English marks") {
        auto editor = emptyEditor();
        type(*editor, QStringLiteral("\"Don't,\" she said."));
        CHECK(editor->plainText() == QStringLiteral("“Don’t,” she said."));
    }

    SECTION("The language is asked for each quotation mark") {
        auto editor = emptyEditor();
        QString language = QStringLiteral("pl");
        editor->setTextLanguage([&language]() { return language; });
        type(*editor, QStringLiteral("\"A\" "));
        language = QStringLiteral("en");
        type(*editor, QStringLiteral("\"B\""));
        CHECK(editor->plainText() == QStringLiteral("„A” “B”"));
    }

    SECTION("A typed character in the place of a selection") {
        auto editor = editorWith(kmlOf({QStringLiteral("Ala ma kota")}));
        editor->setTextLanguage([]() { return QStringLiteral("pl"); });
        editor->setSelection({{0, 7}, {0, 11}});
        type(*editor, QStringLiteral("\""));
        CHECK(editor->plainText() == QStringLiteral("Ala ma „"));
        editor->undo();
        CHECK(editor->plainText() == QStringLiteral("Ala ma \""));
        editor->undo();
        CHECK(editor->plainText() == QStringLiteral("Ala ma kota"));
    }

    SECTION("Turned off") {
        auto editor = emptyEditor(QStringLiteral("pl"));
        AutoCorrectOptions off;
        off.quotes = false;
        off.apostrophe = false;
        off.dialogueDash = false;
        off.ellipsis = false;
        editor->setAutoCorrect(off);
        CHECK(editor->autoCorrect() == off);
        const QString typed = QStringLiteral("- \"Tak,\" powiedział... 'Lalka' Kennedy'ego");
        type(*editor, typed);
        CHECK(editor->plainText() == typed);
    }
}

TEST_CASE("Autocorrect: undo right after a change brings back what was typed",
          "[editor][autocorrect][undo]") {
    auto editor = emptyEditor(QStringLiteral("pl"));

    SECTION("A quotation mark: the straight one stays when the text goes on") {
        type(*editor, QStringLiteral("Ala \""));
        REQUIRE(editor->plainText() == QStringLiteral("Ala „"));
        editor->undo();
        CHECK(editor->plainText() == QStringLiteral("Ala \""));
        CHECK(editor->cursorPosition() == CursorPosition{0, 5});
        CHECK_FALSE(editor->hasSelection());

        editor->redo();
        CHECK(editor->plainText() == QStringLiteral("Ala „"));
        CHECK(editor->cursorPosition() == CursorPosition{0, 5});

        editor->undo();
        type(*editor, QStringLiteral("kot"));
        CHECK(editor->plainText() == QStringLiteral("Ala \"kot"));
    }

    SECTION("The dialogue dash") {
        type(*editor, QStringLiteral("- "));
        REQUIRE(editor->plainText() == QStringLiteral("— "));
        CHECK(editor->cursorPosition() == CursorPosition{0, 2});
        editor->undo();
        CHECK(editor->plainText() == QStringLiteral("- "));
        CHECK(editor->cursorPosition() == CursorPosition{0, 2});
    }

    SECTION("The ellipsis: a period typed after the periods brought back stays a period") {
        editor->fromKml(kmlOf({QStringLiteral("Wiem i tyle")}));
        editor->setCursorPosition({0, 4});
        type(*editor, QStringLiteral("..."));
        REQUIRE(editor->plainText() == QStringLiteral("Wiem… i tyle"));
        CHECK(editor->cursorPosition() == CursorPosition{0, 5});
        editor->undo();
        CHECK(editor->plainText() == QStringLiteral("Wiem... i tyle"));
        CHECK(editor->cursorPosition() == CursorPosition{0, 7});
        editor->redo();
        CHECK(editor->plainText() == QStringLiteral("Wiem… i tyle"));
        CHECK(editor->cursorPosition() == CursorPosition{0, 5});

        editor->undo();
        type(*editor, QStringLiteral("."));
        CHECK(editor->plainText() == QStringLiteral("Wiem.... i tyle"));
    }

    SECTION("The text typed after a change is a step of its own") {
        type(*editor, QStringLiteral("\"Ala"));
        REQUIRE(editor->plainText() == QStringLiteral("„Ala"));
        editor->undo();
        CHECK(editor->plainText() == QStringLiteral("„"));
        CHECK(editor->cursorPosition() == CursorPosition{0, 1});
        editor->undo();
        CHECK(editor->plainText() == QStringLiteral("\""));
        CHECK(editor->cursorPosition() == CursorPosition{0, 1});
        editor->undo();
        CHECK(editor->plainText().isEmpty());
        CHECK_FALSE(editor->canUndo());

        // And back
        editor->redo();
        editor->redo();
        editor->redo();
        CHECK(editor->plainText() == QStringLiteral("„Ala"));
        CHECK(editor->cursorPosition() == CursorPosition{0, 4});
        CHECK_FALSE(editor->canRedo());
    }
}

TEST_CASE("Autocorrect: a change keeps the format of what was typed", "[editor][autocorrect]") {
    SECTION("Bold given without a selection, for the text typed next") {
        auto editor = emptyEditor(QStringLiteral("pl"));
        editor->toggleBold();
        type(*editor, QStringLiteral("\"Ala\"..."));
        CHECK(editor->plainText() == QStringLiteral("„Ala”…"));
        CHECK(textWith(*editor, isBold) == QStringLiteral("„Ala”…"));
    }

    SECTION("Italic text") {
        auto editor = editorWith(QStringLiteral("<kml><p><i>Ala ma kota</i> i psa</p></kml>"));
        editor->setCursorPosition({0, 11});
        type(*editor, QStringLiteral("..."));
        CHECK(editor->plainText() == QStringLiteral("Ala ma kota… i psa"));
        CHECK(textWith(*editor, isItalic) == QStringLiteral("Ala ma kota…"));
    }
}

TEST_CASE("Autocorrect: annotations stay where the writer put them",
          "[editor][autocorrect][annotations]") {
    // "The quick fox": the comment on "quick" 4-9, a note after "fox"
    const QString kml = QStringLiteral(
        "<kml><annotations><annotation id=\"a\" kind=\"comment\">Text</annotation>"
        "<annotation id=\"n\" kind=\"note\">Text</annotation></annotations>"
        "<p>The <anchor ref=\"a\">quick</anchor> fox..<anchor ref=\"n\"/></p></kml>");
    auto editor = editorWith(kml);
    editor->setTextLanguage([]() { return QStringLiteral("pl"); });
    REQUIRE(places(*editor) == QStringLiteral("a 4-9, n @15"));

    SECTION("A mark typed inside a fragment stays in it") {
        editor->setCursorPosition({0, 6});
        type(*editor, QStringLiteral("\""));
        CHECK(editor->plainText() == QStringLiteral("The qu”ick fox.."));
        CHECK(places(*editor) == QStringLiteral("a 4-10, n @16"));
    }

    SECTION("A mark typed after a fragment does not join it") {
        editor->setCursorPosition({0, 9});
        type(*editor, QStringLiteral("\""));
        CHECK(editor->plainText() == QStringLiteral("The quick” fox.."));
        CHECK(places(*editor) == QStringLiteral("a 4-9, n @16"));
    }

    SECTION("A note on a period that becomes a part of the ellipsis stays after it") {
        editor->setCursorPosition({0, 15});
        type(*editor, QStringLiteral("."));
        CHECK(editor->plainText() == QStringLiteral("The quick fox…"));
        CHECK(places(*editor) == QStringLiteral("a 4-9, n @14"));

        editor->undo();
        CHECK(editor->plainText() == QStringLiteral("The quick fox..."));
        CHECK(places(*editor) == QStringLiteral("a 4-9, n @15"));
    }
}

TEST_CASE("Autocorrect: text composed with an input method", "[editor][autocorrect]") {
    auto editor = emptyEditor(QStringLiteral("pl"));

    QInputMethodEvent composing(QStringLiteral("\"Tak"), {});
    QCoreApplication::sendEvent(editor.get(), &composing);
    QInputMethodEvent committed;
    committed.setCommitString(QStringLiteral("\"Tak...\""));
    QCoreApplication::sendEvent(editor.get(), &committed);
    CHECK(editor->plainText() == QStringLiteral("„Tak…”"));
    CHECK(editor->cursorPosition() == CursorPosition{0, 6});

    // Undo brings back the last mark typed
    editor->undo();
    CHECK(editor->plainText() == QStringLiteral("„Tak…\""));
}
