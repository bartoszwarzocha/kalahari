/// @file test_auto_correct.cpp
/// @brief Autocorrect: the typographic characters that take the place of what is typed
///
/// As in Word: straight quotation marks become those of the text's language, an apostrophe
/// the typographic one, a hyphen and a space starting a paragraph a dialogue dash, three
/// periods an ellipsis. A space, a punctuation mark or Enter after a word changes the word:
/// the replacement table, hyphens to dashes, capitals at the start of a sentence, a word
/// typed with Caps Lock on. "***" and "---" with Enter make a scene break and a line. Each
/// change is an undo step of its own, so undoing right after it brings back what was typed.

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_type_package.h>
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

/// The options with every change turned off
AutoCorrectOptions nothingChanged() {
    AutoCorrectOptions options;
    for (bool AutoCorrectOptions::*change :
         {&AutoCorrectOptions::quotes, &AutoCorrectOptions::apostrophe,
          &AutoCorrectOptions::dialogueDash, &AutoCorrectOptions::ellipsis,
          &AutoCorrectOptions::dashBetweenWords, &AutoCorrectOptions::doubleHyphen,
          &AutoCorrectOptions::sentenceCapitals, &AutoCorrectOptions::twoInitialCapitals,
          &AutoCorrectOptions::capsLock, &AutoCorrectOptions::sceneBreak,
          &AutoCorrectOptions::horizontalRule}) {
        options.*change = false;
    }
    return options;
}

/// The text with a change made
QString changed(QString text, const std::optional<AutoCorrection>& change) {
    if (!change) {
        return text;
    }
    return text.replace(change->start, change->end - change->start, change->text);
}

/// The text after autocorrect has seen the last character of @p before typed
QString corrected(const QString& before, const QuoteMarks& marks = POLISH,
                  const AutoCorrectOptions& options = {}, bool capsLockOn = false) {
    return changed(before,
                   autoCorrection(before, options, [&marks]() { return marks; }, capsLockOn));
}

/// The text after autocorrect has seen the last character of @p before typed, in Polish
QString corrected(const QString& before, const AutoCorrectOptions& options,
                  bool capsLockOn = false) {
    return corrected(before, POLISH, options, capsLockOn);
}

/// The paragraph after autocorrect has seen Enter at its end
QString correctedAtEnd(const QString& paragraph, const AutoCorrectOptions& options = {}) {
    return changed(paragraph, autoCorrectionAtParagraphEnd(paragraph, options));
}

/// The options with the replacement table
AutoCorrectOptions withTable() {
    AutoCorrectOptions options;
    options.replacements = {{QStringLiteral("(c)"), QStringLiteral("©")},
                            {QStringLiteral("(tm)"), QStringLiteral("™")},
                            {QStringLiteral("1/2"), QStringLiteral("½")},
                            {QStringLiteral("->"), QStringLiteral("→")},
                            {QStringLiteral("teh"), QStringLiteral("the")},
                            {QStringLiteral("e.g."), QStringLiteral("for example")}};
    return options;
}

/// The options with abbreviations
AutoCorrectOptions withAbbreviations() {
    AutoCorrectOptions options;
    options.abbreviations = {QStringLiteral("np."), QStringLiteral("dr"),
                             QStringLiteral("m.in."), QStringLiteral("godz.")};
    return options;
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

/// Type the text with Caps Lock on: Shift held for its small letters
void typeWithCapsLock(BookEditor& editor, const QString& text) {
    for (const QChar ch : text) {
        QKeyEvent event(QEvent::KeyPress, 0,
                        ch.isLower() ? Qt::ShiftModifier : Qt::NoModifier, QString(ch));
        QCoreApplication::sendEvent(&editor, &event);
    }
}

/// Press a key without text: Enter, Shift+Enter
void press(BookEditor& editor, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QCoreApplication::sendEvent(&editor, &event);
}

/// The paragraphs of an editor, an object as its KML element ("<scene-break>")
QStringList paragraphsOf(const BookEditor& editor) {
    QStringList paragraphs;
    for (QTextBlock block = editor.textDocument()->begin(); block.isValid();
         block = block.next()) {
        const TextObject object = blockObject(block);
        paragraphs << (object == TextObject::None
                           ? block.text()
                           : QStringLiteral("<%1>").arg(kmlElementOf(object)));
    }
    return paragraphs;
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

    // Only at the start of the paragraph, and only a hyphen alone (between words it is a
    // dash of its own, two hyphens an en dash)
    for (const QString& text : {QStringLiteral(" - "), QStringLiteral("—- "),
                                QStringLiteral("Ala") + QChar(QChar::LineSeparator) +
                                    QStringLiteral("- ")}) {
        INFO(text.toStdString());
        CHECK(corrected(text) == text);
    }
}

TEST_CASE("Autocorrect: a hyphen between words makes a dash", "[editor][autocorrect]") {
    CHECK(corrected(QStringLiteral("Ala - ")) == QStringLiteral("Ala – "));
    CHECK(corrected(QStringLiteral("5 - ")) == QStringLiteral("5 – "));
    CHECK(corrected(QStringLiteral("Ala  - ")) == QStringLiteral("Ala  – "));
    CHECK(corrected(QStringLiteral("Ala, - ")) == QStringLiteral("Ala, – "));

    // The dash of the setting
    AutoCorrectOptions emDash;
    emDash.wordDash = DialogueDash::Em;
    CHECK(corrected(QStringLiteral("Ala - "), emDash) == QStringLiteral("Ala — "));

    // In a line of dialogue the dash it starts with, unless the setting says otherwise
    CHECK(corrected(QStringLiteral("— Tak - ")) == QStringLiteral("— Tak — "));
    CHECK(corrected(QStringLiteral("– Tak - "), emDash) == QStringLiteral("– Tak – "));
    CHECK(corrected(QStringLiteral("— Tak - "),
                    without(&AutoCorrectOptions::dialogueDashInDialogue)) ==
          QStringLiteral("— Tak – "));

    // Not without a space and a word before it, nor before anything but a space
    for (const QString& text :
         {QStringLiteral("Ala-"), QStringLiteral("Ala- "), QStringLiteral("Ala -,"),
          QStringLiteral("Ala") + QChar(QChar::LineSeparator) + QStringLiteral("- "),
          QStringLiteral("Ala -x ")}) {
        INFO(text.toStdString());
        CHECK(corrected(text) == text);
    }
    CHECK(correctedAtEnd(QStringLiteral("Ala -")) == QStringLiteral("Ala -"));
    CHECK(corrected(QStringLiteral("Ala - "), without(&AutoCorrectOptions::dashBetweenWords)) ==
          QStringLiteral("Ala - "));
}

TEST_CASE("Autocorrect: two hyphens make an en dash", "[editor][autocorrect]") {
    // Alone, when a space or Enter follows them
    CHECK(corrected(QStringLiteral("Ala -- ")) == QStringLiteral("Ala – "));
    CHECK(corrected(QStringLiteral("-- ")) == QStringLiteral("– "));
    CHECK(correctedAtEnd(QStringLiteral("Ala --")) == QStringLiteral("Ala –"));

    // Between two words or numbers, when the word ends
    CHECK(corrected(QStringLiteral("1939--1945 ")) == QStringLiteral("1939–1945 "));
    CHECK(corrected(QStringLiteral("w latach 1939--1945,")) ==
          QStringLiteral("w latach 1939–1945,"));
    CHECK(corrected(QStringLiteral("(1939--1945) ")) == QStringLiteral("(1939–1945) "));
    CHECK(correctedAtEnd(QStringLiteral("Ala ma--kota")) == QStringLiteral("Ala ma–kota"));

    // Three hyphens stay (a line, with Enter), as do two at the end of a word, two before a
    // punctuation mark, and those of a word with a punctuation mark in it
    for (const QString& text :
         {QStringLiteral("--- "), QStringLiteral("Ala a---b "), QStringLiteral("Ala-- "),
          QStringLiteral("Ala --,"), QStringLiteral("Ala a--b, ")}) {
        INFO(text.toStdString());
        CHECK(corrected(text) == text);
    }
    CHECK(correctedAtEnd(QStringLiteral("---")) == QStringLiteral("---"));
    CHECK(corrected(QStringLiteral("Ala -- "), without(&AutoCorrectOptions::doubleHyphen)) ==
          QStringLiteral("Ala -- "));
    CHECK(corrected(QStringLiteral("1939--1945 "), without(&AutoCorrectOptions::doubleHyphen)) ==
          QStringLiteral("1939--1945 "));
}

TEST_CASE("Autocorrect: the replacement table", "[editor][autocorrect]") {
    const AutoCorrectOptions options = withTable();

    // The whole word, before a space, a punctuation mark or Enter
    CHECK(corrected(QStringLiteral("Ala (c) "), options) == QStringLiteral("Ala © "));
    CHECK(corrected(QStringLiteral("Ala (c)."), options) == QStringLiteral("Ala ©."));
    CHECK(corrected(QStringLiteral("Ala (tm),"), options) == QStringLiteral("Ala ™,"));
    CHECK(corrected(QStringLiteral("Ala -> "), options) == QStringLiteral("Ala → "));
    CHECK(corrected(QStringLiteral("Ala 1/2 "), options) == QStringLiteral("Ala ½ "));
    CHECK(correctedAtEnd(QStringLiteral("Ala (c)"), options) == QStringLiteral("Ala ©"));

    // In quotation marks and brackets
    CHECK(corrected(QStringLiteral("Ala „(c)” "), options) == QStringLiteral("Ala „©” "));
    CHECK(corrected(QStringLiteral("Ala (teh) "), options) == QStringLiteral("Ala (the) "));
    CHECK(corrected(QStringLiteral("Ala „teh”,"), options) == QStringLiteral("Ala „the”,"));

    // A pair with a punctuation mark in it
    CHECK(corrected(QStringLiteral("Ala e.g. "), options) == QStringLiteral("Ala for example "));

    // Typed with a capital, or in capitals
    CHECK(corrected(QStringLiteral("Ala Teh "), options) == QStringLiteral("Ala The "));
    CHECK(corrected(QStringLiteral("Ala TEH "), options) == QStringLiteral("Ala THE "));
    CHECK(corrected(QStringLiteral("Ala (C) "), options) == QStringLiteral("Ala © "));

    // At the start of a sentence the replacement gets a capital
    CHECK(corrected(QStringLiteral("teh "), options) == QStringLiteral("The "));

    // Only the whole word: a date, a word with more letters, a word with a punctuation mark
    // in it (its part before the mark had its change when the mark was typed)
    for (const QString& text :
         {QStringLiteral("Ala 1/2/2026 "), QStringLiteral("Ala tehx "), QStringLiteral("Ala x(c) "),
          QStringLiteral("Ala teh, "), QStringLiteral("Ala (c). ")}) {
        INFO(text.toStdString());
        CHECK(corrected(text, options) == text);
    }

    // An empty table changes nothing
    CHECK(corrected(QStringLiteral("Ala (c) ")) == QStringLiteral("Ala (c) "));
}

TEST_CASE("Autocorrect: the first letter of a sentence becomes a capital",
          "[editor][autocorrect]") {
    const AutoCorrectOptions options = withAbbreviations();

    // At the start of the paragraph, of a line of dialogue, after . ? !
    CHECK(corrected(QStringLiteral("tak "), options) == QStringLiteral("Tak "));
    CHECK(corrected(QStringLiteral("tak,"), options) == QStringLiteral("Tak,"));
    CHECK(corrected(QStringLiteral("  tak "), options) == QStringLiteral("  Tak "));
    CHECK(correctedAtEnd(QStringLiteral("tak"), options) == QStringLiteral("Tak"));
    CHECK(corrected(QStringLiteral("— tak "), options) == QStringLiteral("— Tak "));
    CHECK(corrected(QStringLiteral("Ala ma kota. on "), options) ==
          QStringLiteral("Ala ma kota. On "));
    CHECK(corrected(QStringLiteral("Tak? on "), options) == QStringLiteral("Tak? On "));
    CHECK(corrected(QStringLiteral("Tak! on "), options) == QStringLiteral("Tak! On "));
    CHECK(corrected(QStringLiteral("Tak.” on "), options) == QStringLiteral("Tak.” On "));
    CHECK(corrected(QStringLiteral("(Tak.) on "), options) == QStringLiteral("(Tak.) On "));
    CHECK(corrected(QStringLiteral("Tak. „on "), options) == QStringLiteral("Tak. „On "));
    CHECK(corrected(QStringLiteral("Tak.") + QChar(QChar::LineSeparator) + QStringLiteral("on "),
                    options) ==
          QStringLiteral("Tak.") + QChar(QChar::LineSeparator) + QStringLiteral("On "));
    CHECK(corrected(QStringLiteral("Ala ma kota. więc "), options) ==
          QStringLiteral("Ala ma kota. Więc "));

    // A line of dialogue after a sentence of narration that ends with a period
    CHECK(corrected(QStringLiteral("— Tak — powiedział. — idę "), options) ==
          QStringLiteral("— Tak — powiedział. — Idę "));

    // Not after an abbreviation of the list (with or without its period, in any case), a
    // number, an initial, an ellipsis; not in the narration after a question in dialogue
    // nor after a comma; not in a word with a capital in it
    for (const QString& text :
         {QStringLiteral("— Naprawdę? — zapytała "), QStringLiteral("Ala, ma "),
          QStringLiteral("Ala ma "), QStringLiteral("Przyszedł np. pies "),
          QStringLiteral("Przyszedł Np. pies "), QStringLiteral("Przyszedł dr. pies "),
          QStringLiteral("Byli m.in. oni "), QStringLiteral("To 5. pułk "),
          QStringLiteral("J. kowalski "), QStringLiteral("Wiem… tak "),
          QStringLiteral("Wiem... tak "), QStringLiteral("iPhone "),
          QStringLiteral("Ala.tak "), QStringLiteral("Tak — powiedział "),
          QStringLiteral("Tak: on ")}) {
        INFO(text.toStdString());
        CHECK(corrected(text, options) == text);
    }
    CHECK(corrected(QStringLiteral("tak "), without(&AutoCorrectOptions::sentenceCapitals)) ==
          QStringLiteral("tak "));

    // After a word with a period that is not an abbreviation of the list: undone, the
    // capital gives the list the word
    const std::optional<AutoCorrection> change =
        autoCorrection(QStringLiteral("Przyszedł wg. pies "), {}, [] { return POLISH; });
    REQUIRE(change.has_value());
    CHECK(change->text == QStringLiteral("P"));
    CHECK(change->learns == LearnedException::Abbreviation);
    CHECK(change->exception == QStringLiteral("wg."));

    // At the start of a paragraph or after ? there is nothing to learn
    const std::optional<AutoCorrection> start =
        autoCorrection(QStringLiteral("tak "), {}, [] { return POLISH; });
    REQUIRE(start.has_value());
    CHECK(start->learns == LearnedException::None);
}

TEST_CASE("Autocorrect: two capitals at the start of a word", "[editor][autocorrect]") {
    CHECK(corrected(QStringLiteral("Ala TWo ")) == QStringLiteral("Ala Two "));
    CHECK(corrected(QStringLiteral("WIęc,")) == QStringLiteral("Więc,"));
    CHECK(correctedAtEnd(QStringLiteral("Ala POlska")) == QStringLiteral("Ala Polska"));

    const std::optional<AutoCorrection> change =
        autoCorrection(QStringLiteral("Ala TWo "), {}, [] { return POLISH; });
    REQUIRE(change.has_value());
    CHECK(change->learns == LearnedException::TwoCapitals);
    CHECK(change->exception == QStringLiteral("TWo"));

    // Not a word in capitals, of two letters, of the exceptions
    AutoCorrectOptions options;
    options.twoCapitalsExceptions = {QStringLiteral("PCs")};
    for (const QString& text : {QStringLiteral("Ala ONZ "), QStringLiteral("Ala PC "),
                                QStringLiteral("Ala PCs "), QStringLiteral("Ala TWoJ ")}) {
        INFO(text.toStdString());
        CHECK(corrected(text, options) == text);
    }
    CHECK(corrected(QStringLiteral("Ala TWo "), without(&AutoCorrectOptions::twoInitialCapitals)) ==
          QStringLiteral("Ala TWo "));
}

TEST_CASE("Autocorrect: a word typed with Caps Lock on by mistake", "[editor][autocorrect]") {
    CHECK(corrected(QStringLiteral("Ala wIELKI "), {}, true) == QStringLiteral("Ala Wielki "));
    CHECK(corrected(QStringLiteral("wIĘC "), {}, true) == QStringLiteral("Więc "));
    const std::optional<AutoCorrection> change =
        autoCorrection(QStringLiteral("Ala wIELKI "), {}, [] { return POLISH; }, true);
    REQUIRE(change.has_value());
    CHECK(change->capsLock);
    CHECK(change->learns == LearnedException::None);

    // Only when the keys typed show Caps Lock on: "mRNA" and "iOS" typed with it off stay
    CHECK(corrected(QStringLiteral("Ala mRNA "), {}, false) == QStringLiteral("Ala mRNA "));
    CHECK(corrected(QStringLiteral("Ala iOS "), {}, false) == QStringLiteral("Ala iOS "));
    CHECK(corrected(QStringLiteral("Ala wI "), {}, true) == QStringLiteral("Ala wI "));
    CHECK(corrected(QStringLiteral("Ala wIELKI "), without(&AutoCorrectOptions::capsLock), true) ==
          QStringLiteral("Ala wIELKI "));
}

TEST_CASE("Autocorrect: a scene break and a line typed in a paragraph of their own",
          "[editor][autocorrect]") {
    const AutoCorrectOptions options;
    CHECK(paragraphObjectShortcut(QStringLiteral("***"), options) == TextObject::SceneBreak);
    CHECK(paragraphObjectShortcut(QStringLiteral(" *** "), options) == TextObject::SceneBreak);
    CHECK(paragraphObjectShortcut(QStringLiteral("---"), options) == TextObject::HorizontalRule);
    for (const char* text : {"", "**", "****", "* * *", "Ala ***", "----", "—-"}) {
        INFO(text);
        CHECK(paragraphObjectShortcut(QString::fromUtf8(text), options) == TextObject::None);
    }
    CHECK(paragraphObjectShortcut(QStringLiteral("***"),
                                  without(&AutoCorrectOptions::sceneBreak)) == TextObject::None);
    CHECK(paragraphObjectShortcut(QStringLiteral("---"),
                                  without(&AutoCorrectOptions::horizontalRule)) ==
          TextObject::None);
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
    for (const QChar ch : QStringLiteral("\"' .,;:!?\u00a0\u2028")) {
        CHECK(startsAutoCorrection(ch));
    }
    for (const QChar ch : QStringLiteral("a-…„)*")) {
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
        const AutoCorrectOptions off = nothingChanged();
        editor->setAutoCorrect(off);
        CHECK(editor->autoCorrect() == off);
        const QString typed =
            QStringLiteral("- \"tak,\" powiedział... 'Lalka' Kennedy'ego - TAk -- 1--2. wIELKI");
        type(*editor, typed);
        CHECK(editor->plainText() == typed);
    }

    SECTION("The words, as a space, a punctuation mark and Enter end them") {
        auto editor = emptyEditor(QStringLiteral("pl"));
        AutoCorrectOptions options = withTable();
        options.abbreviations = withAbbreviations().abbreviations;
        editor->setAutoCorrect(options);
        type(*editor, QStringLiteral("- tak - powiedział np. (c) -- teh. TWo"));
        press(*editor, Qt::Key_Return);
        type(*editor, QStringLiteral("rok 1939--1945"));
        press(*editor, Qt::Key_Return, Qt::ShiftModifier);
        type(*editor, QStringLiteral("x"));
        CHECK(paragraphsOf(*editor) ==
              QStringList{QStringLiteral("— Tak — powiedział np. © – the. Two"),
                          QStringLiteral("Rok 1939–1945") + QChar(QChar::LineSeparator) +
                              QStringLiteral("x")});
    }

    SECTION("Caps Lock: the keys typed show it on") {
        auto editor = emptyEditor(QStringLiteral("pl"));
        QStringList corrected;
        QObject::connect(editor.get(), &BookEditor::capsLockCorrected,
                         [&corrected, &editor] { corrected << editor->plainText(); });
        typeWithCapsLock(*editor, QStringLiteral("Ala wIELKI "));
        CHECK(editor->plainText() == QStringLiteral("Ala Wielki "));
        CHECK(corrected == QStringList{QStringLiteral("Ala Wielki ")});

        // Without it on the word stays: "mRNA" is typed with Shift for the capitals
        editor->fromKml(kmlOf({QString()}));
        QKeyEvent m(QEvent::KeyPress, 0, Qt::NoModifier, QStringLiteral("m"));
        QCoreApplication::sendEvent(editor.get(), &m);
        for (const QChar ch : QStringLiteral("RNA")) {
            QKeyEvent capital(QEvent::KeyPress, 0, Qt::ShiftModifier, QString(ch));
            QCoreApplication::sendEvent(editor.get(), &capital);
        }
        type(*editor, QStringLiteral(" "));
        CHECK(editor->plainText() == QStringLiteral("mRNA "));
        CHECK(corrected.size() == 1);
    }

    SECTION("In code nothing changes") {
        auto editor = std::make_unique<BookEditor>();
        resizeWidget(*editor, QSize(600, 400));
        kalahari::core::PackageParagraphStyle normal;
        normal.id = QStringLiteral("normal");
        kalahari::core::PackageParagraphStyle code;
        code.id = QStringLiteral("code");
        code.properties.insert(QStringLiteral("spellCheck"), false);
        editor->setParagraphStyles(
            ParagraphStyleSet::fromPackage({normal, code}, QStringLiteral("en")));
        editor->fromKml(QStringLiteral("<kml><p style=\"code\"></p></kml>"));
        editor->setAutoCorrect(withTable());
        const QString typed = QStringLiteral("x = \"a\" -- (c) - y... teh");
        type(*editor, typed);
        press(*editor, Qt::Key_Return);
        type(*editor, QStringLiteral("***"));
        press(*editor, Qt::Key_Return);
        CHECK(paragraphsOf(*editor) ==
              QStringList{typed, QStringLiteral("***"), QString()});
    }
}

TEST_CASE("Autocorrect: \"***\" and \"---\" with Enter make a scene break and a line",
          "[editor][autocorrect][objects]") {
    auto editor = emptyEditor(QStringLiteral("pl"));
    type(*editor, QStringLiteral("Koniec."));
    press(*editor, Qt::Key_Return);
    type(*editor, QStringLiteral("***"));
    press(*editor, Qt::Key_Return);
    CHECK(paragraphsOf(*editor) ==
          QStringList{QStringLiteral("Koniec."), QStringLiteral("<scene-break>"), QString()});
    CHECK(editor->cursorPosition() == CursorPosition{2, 0});

    type(*editor, QStringLiteral("Dalej"));
    press(*editor, Qt::Key_Return);
    type(*editor, QStringLiteral("---"));
    press(*editor, Qt::Key_Return);
    CHECK(paragraphsOf(*editor) ==
          QStringList{QStringLiteral("Koniec."), QStringLiteral("<scene-break>"),
                      QStringLiteral("Dalej"), QStringLiteral("<hr>"), QString()});

    SECTION("Undone right after, the text typed comes back, Enter stays") {
        editor->undo();
        CHECK(paragraphsOf(*editor) ==
              QStringList{QStringLiteral("Koniec."), QStringLiteral("<scene-break>"),
                          QStringLiteral("Dalej"), QStringLiteral("---"), QString()});
        CHECK(editor->cursorPosition() == CursorPosition{4, 0});
        editor->redo();
        CHECK(paragraphsOf(*editor)[3] == QStringLiteral("<hr>"));
    }

    SECTION("Not with Enter in the middle of the text, nor with more text") {
        editor->fromKml(kmlOf({QStringLiteral("***x")}));
        editor->setCursorPosition({0, 3});
        press(*editor, Qt::Key_Return);
        CHECK(paragraphsOf(*editor) == QStringList{QStringLiteral("***"), QStringLiteral("x")});
    }

    SECTION("Turned off") {
        editor->fromKml(kmlOf({QString()}));
        editor->setAutoCorrect(without(&AutoCorrectOptions::sceneBreak));
        type(*editor, QStringLiteral("***"));
        press(*editor, Qt::Key_Return);
        CHECK(paragraphsOf(*editor) == QStringList{QStringLiteral("***"), QString()});
    }
}

TEST_CASE("Autocorrect: a change undone right after it gives an exception list the word",
          "[editor][autocorrect][undo]") {
    auto editor = emptyEditor(QStringLiteral("pl"));
    QStringList learned;
    QObject::connect(editor.get(), &BookEditor::autoCorrectionUndone,
                     [&learned](LearnedException list, const QString& exception) {
                         learned << QStringLiteral("%1 %2")
                                        .arg(list == LearnedException::Abbreviation
                                                 ? QStringLiteral("abbreviation")
                                                 : QStringLiteral("capitals"),
                                             exception);
                     });

    SECTION("An abbreviation") {
        type(*editor, QStringLiteral("O godz. siódmej "));
        REQUIRE(editor->plainText() == QStringLiteral("O godz. Siódmej "));
        editor->undo();
        CHECK(editor->plainText() == QStringLiteral("O godz. siódmej "));
        CHECK(learned == QStringList{QStringLiteral("abbreviation godz.")});
    }

    SECTION("Two capitals, ended by Enter") {
        type(*editor, QStringLiteral("Ala PCs"));
        press(*editor, Qt::Key_Return);
        REQUIRE(paragraphsOf(*editor) == QStringList{QStringLiteral("Ala Pcs"), QString()});
        editor->undo();
        CHECK(paragraphsOf(*editor) == QStringList{QStringLiteral("Ala PCs"), QString()});
        CHECK(learned == QStringList{QStringLiteral("capitals PCs")});
    }

    SECTION("Not when something was done after the change") {
        type(*editor, QStringLiteral("O godz. siódmej x"));
        editor->undo();  // "x"
        editor->undo();  // the capital
        CHECK(editor->plainText() == QStringLiteral("O godz. siódmej "));
        CHECK(learned.isEmpty());
    }

    SECTION("Not for a capital at the start of a paragraph") {
        type(*editor, QStringLiteral("tak "));
        REQUIRE(editor->plainText() == QStringLiteral("Tak "));
        editor->undo();
        CHECK(editor->plainText() == QStringLiteral("tak "));
        CHECK(learned.isEmpty());
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
