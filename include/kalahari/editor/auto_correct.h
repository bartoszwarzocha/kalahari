/// @file auto_correct.h
/// @brief Autocorrect: the typographic characters that take the place of what is typed
///
/// As in Word, a character just typed can change what the writer typed: straight quotation
/// marks become the quotation marks of the text's language, an apostrophe the typographic
/// one, a hyphen and a space at the start of a paragraph a dialogue dash, three periods an
/// ellipsis. A space, a punctuation mark or Enter after a word can change the word: a pair
/// of the replacement table ((c) to ©), hyphens to a dash, the first letter of a sentence to
/// a capital, a word typed with Caps Lock on by mistake. The editor (BookEditor::typeText())
/// makes each change an undo step of its own, so undoing right after it brings back what
/// was typed.

#pragma once

#include <kalahari/editor/paragraph_styles.h>

#include <QChar>
#include <QString>
#include <QStringList>

#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace kalahari::editor {

/// @brief The quotation marks of a language
struct QuoteMarks {
    QChar doubleOpening;  ///< „ in Polish, “ in English
    QChar doubleClosing;  ///< ” in Polish and in English
    QChar singleOpening;  ///< ‚ in Polish, ‘ in English
    QChar singleClosing;  ///< ’ in Polish and in English

    /// @brief The marks of a language, from its ISO 639 code with or without a country
    ///        ("pl", "pl_PL", "en-GB")
    ///
    /// Polish and English have the marks of the plan of the editor; another language has
    /// those of Qt's locale data (German „…“ and ‚…‘, French «…»). A code Qt does not know,
    /// or none, gives the English marks.
    static QuoteMarks forLanguage(const QString& language);

    bool operator==(const QuoteMarks&) const = default;
};

/// @brief The dash a hyphen becomes: at the start of a paragraph, or between words
enum class DialogueDash {
    Em,  ///< — (pauza), as Polish dialogue has it
    En   ///< – (półpauza)
};

/// @brief A pair of the replacement table: a word as it is typed and the text that takes
///        its place
struct Replacement {
    QString from;  ///< "(c)", "->"
    QString to;    ///< "©", "→"

    bool operator==(const Replacement&) const = default;
};

/// @brief What autocorrect changes as the text is typed: everything by default, as in Word
///
/// The lists (the replacement table and the exceptions) are empty by default; the editors
/// get them from the settings, which have the program's own ones.
struct AutoCorrectOptions {
    bool quotes = true;      ///< Straight quotation marks to those of the text's language
    bool apostrophe = true;  ///< ' after a letter or another character of a word to ’
    bool dialogueDash = true;               ///< A hyphen and a space starting a paragraph
                                            ///< to a dash and the space
    DialogueDash dash = DialogueDash::Em;   ///< The dash of dialogueDash
    bool ellipsis = true;    ///< Three periods to an ellipsis (…)

    bool dashBetweenWords = true;  ///< A hyphen between two spaces after a word (" - ") to
                                   ///< a dash
    DialogueDash wordDash = DialogueDash::En;  ///< The dash of dashBetweenWords
    bool dialogueDashInDialogue = true;  ///< In a paragraph that starts with a dash (a line
                                         ///< of dialogue) dashBetweenWords gives that dash
    bool doubleHyphen = true;  ///< Two hyphens to an en dash: alone ("a -- b") or between
                               ///< two words ("1939--1945")

    std::vector<Replacement> replacements;  ///< The replacement table: the whole word typed
                                            ///< before a space, a punctuation mark or Enter

    bool sentenceCapitals = true;  ///< The first letter of a sentence to a capital
    QStringList abbreviations;     ///< After these a period does not end a sentence ("np.",
                                   ///< "Mr."), with or without their period
    std::map<QString, QStringList> languageAbbreviations;  ///< The abbreviations of each
                                   ///< language, by its code ("pl", "en"): the editor gives
                                   ///< those of its text's language (abbreviationsLanguage())
                                   ///< in the place of abbreviations
    bool twoInitialCapitals = true;     ///< "TWo" to "Two"
    QStringList twoCapitalsExceptions;  ///< Words with two capitals that stay ("PCs")
    bool capsLock = true;  ///< A word typed with Caps Lock on by mistake ("wIELKI") to the
                           ///< word ("Wielki")
    bool links = true;     ///< An Internet address typed in the text to a link, when a space
                           ///< or Enter ends it (autoLink())

    bool sceneBreak = true;      ///< "***" and Enter in a paragraph of its own to a scene
                                 ///< break
    bool horizontalRule = true;  ///< "---" and Enter in a paragraph of its own to a line
    bool bulletedLists = true;   ///< "* " starting a paragraph to an item of a bulleted list
    bool numberedLists = true;   ///< "1. " or "1) " starting a paragraph to an item of a
                                 ///< numbered list from that number

    bool operator==(const AutoCorrectOptions&) const = default;
};

/// @brief The list of exceptions a change adds to, when the writer undoes it right after it
///        (as Word does)
enum class LearnedException {
    None,          ///< The change is not one the lists can keep from
    Abbreviation,  ///< A capital after a word with a period: the word is an abbreviation
                   ///< (AutoCorrectOptions::abbreviations)
    TwoCapitals    ///< "TWo" to "Two": the word keeps its two capitals
                   ///< (AutoCorrectOptions::twoCapitalsExceptions)
};

/// @brief A change of the text: its characters [start, end) give their place to the text
struct AutoCorrection {
    int start = 0;
    int end = 0;
    QString text;

    LearnedException learns = LearnedException::None;  ///< The list undoing it adds to
    QString exception{};  ///< What that list gets: the abbreviation with its period ("godz."),
                          ///< or the word as typed ("PCs")
    bool capsLock = false;  ///< The word was typed with Caps Lock on by mistake

    bool operator==(const AutoCorrection&) const = default;
};

/// @brief The link autocorrect makes of an Internet address typed in the text
struct AutoLink {
    int start = 0;    ///< The address's characters [start, end) in the paragraph
    int end = 0;
    QString address;  ///< The address the link opens, with its scheme

    bool operator==(const AutoLink&) const = default;
};

/// @brief Whether autocorrect looks at the text after the character is typed: a quotation
///        mark, an apostrophe, a space (also a line break) or a punctuation mark that ends
///        a word (. , ; : ! ?)
bool startsAutoCorrection(QChar typed);

/// @brief What autocorrect changes after a character is typed
///
/// - " opens a quotation at the start of the paragraph, after a space, an opening bracket,
///   a dash or an opening single mark, and closes one after anything else
/// - ' there, or after an opening double mark, opens a quotation in single marks; after
///   anything else it closes one, when a quotation in single marks is open in the
///   paragraph, or else it is an apostrophe
/// - a space after a hyphen that starts the paragraph makes the hyphen a dialogue dash
/// - a third period makes the periods an ellipsis, unless a fourth one is before them
/// - a space or a punctuation mark ends a word: see autoCorrectionAtParagraphEnd() for what
///   changes in it; a period or a colon that goes on an Internet address ("www.",
///   "https:", "jan@example.") gives its word no capital
///
/// @param before The text of the paragraph from its start to the cursor, which is right after
///        the character just typed
/// @param options What is changed
/// @param quoteMarks The quotation marks of the text's language, asked for only when a
///        quotation mark changes
/// @param capsLockOn Whether the keys typed show Caps Lock on (a letter typed with Shift is
///        small, without it a capital)
/// @return The change, in offsets of @p before; none when nothing changes
std::optional<AutoCorrection> autoCorrection(const QString& before,
                                             const AutoCorrectOptions& options,
                                             const std::function<QuoteMarks()>& quoteMarks,
                                             bool capsLockOn = false);

/// @brief What autocorrect changes in the word before the end of a paragraph (Enter)
///
/// The word is all the characters after the last space; the changes of a word, at Enter
/// as after a space or a punctuation mark:
/// - a pair of the replacement table: the whole word, or the word without the quotation
///   marks and brackets around it; typed with a capital, or in capitals, the replacement
///   gets them too
/// - two hyphens alone become an en dash, as do two hyphens between two words or numbers
/// - a hyphen alone between a word and a space (" - ", only after a space) becomes a dash
/// - a word with a small letter first and capitals after it, typed with Caps Lock on,
///   becomes the word ("wIELKI" to "Wielki")
/// - "TWo" becomes "Two", unless it is an exception
/// - the first letter of a sentence becomes a capital: at the start of the paragraph, after
///   a dialogue dash that starts it, after . ? or ! (with closing quotation marks or
///   brackets after it), and after a dash that follows a period. Not after an
///   abbreviation, a number (5. pułk), a single letter (J. Kowalski) or periods of an
///   ellipsis, and not in a word with a capital in it (iPhone)
///
/// A word with a punctuation mark in it changes only as a whole word of the table: the
/// part before the mark had its changes when the mark was typed.
/// @param paragraph The text of the paragraph from its start to the cursor
/// @return The change, in offsets of @p paragraph; none when nothing changes
std::optional<AutoCorrection> autoCorrectionAtParagraphEnd(const QString& paragraph,
                                                           const AutoCorrectOptions& options,
                                                           bool capsLockOn = false);

/// @brief The link of the Internet address the word at the end of @p text is, when a space
///        or Enter ends it
///
/// The word is all the characters after the last space, without the quotation marks and
/// brackets around it and the punctuation marks after it ("www.example.com." gives
/// "www.example.com"; a closing bracket stays when the address opens one). It is an address
/// with a scheme (http://, https:// or ftp:// with a host, mailto: with an e-mail address),
/// one that starts with "www." and a name with a dot after it ("www.example.com") or an
/// e-mail address ("jan@example.com"). The link opens it with "https://" in front of
/// "www.", with "mailto:" in front of an e-mail address.
/// @param text The text of the paragraph from its start to the end of the word, without the
///        space or Enter that ends it
/// @return None when the word is not an address, or options.links is off
std::optional<AutoLink> autoLink(const QString& text, const AutoCorrectOptions& options);

/// @brief The list of abbreviations a text in a language uses: the key of @p lists of the
///        language ("pl" for "pl_PL"), or else English ("en"); empty without either
QString abbreviationsLanguage(const QString& language,
                              const std::map<QString, QStringList>& lists);

/// @brief The object a paragraph becomes when Enter is pressed at its end: "***" a scene
///        break, "---" a horizontal line (spaces around them do not count); None for any
///        other text
TextObject paragraphObjectShortcut(const QString& paragraph, const AutoCorrectOptions& options);

}  // namespace kalahari::editor
