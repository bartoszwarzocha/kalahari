/// @file auto_correct.h
/// @brief Autocorrect: the typographic characters that take the place of what is typed
///
/// As in Word, a character just typed can change what the writer typed: straight quotation
/// marks become the quotation marks of the text's language, an apostrophe the typographic
/// one, a hyphen and a space at the start of a paragraph a dialogue dash, three periods an
/// ellipsis. The editor (BookEditor::typeText()) makes each change an undo step of its own,
/// so undoing right after it brings back what was typed.

#pragma once

#include <QChar>
#include <QString>

#include <functional>
#include <optional>

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

/// @brief The dash a hyphen at the start of a paragraph becomes
enum class DialogueDash {
    Em,  ///< — (pauza), as Polish dialogue has it
    En   ///< – (półpauza)
};

/// @brief What autocorrect changes as the text is typed: everything by default, as in Word
struct AutoCorrectOptions {
    bool quotes = true;      ///< Straight quotation marks to those of the text's language
    bool apostrophe = true;  ///< ' after a letter or another character of a word to ’
    bool dialogueDash = true;               ///< A hyphen and a space starting a paragraph
                                            ///< to a dash and the space
    DialogueDash dash = DialogueDash::Em;   ///< The dash of dialogueDash
    bool ellipsis = true;    ///< Three periods to an ellipsis (…)

    bool operator==(const AutoCorrectOptions&) const = default;
};

/// @brief A change of the text: its characters [start, end) give their place to the text
struct AutoCorrection {
    int start = 0;
    int end = 0;
    QString text;

    bool operator==(const AutoCorrection&) const = default;
};

/// @brief Whether autocorrect looks at the text after the character is typed: a quotation
///        mark, an apostrophe, a space or a period
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
///
/// @param before The text of the paragraph from its start to the cursor, which is right after
///        the character just typed
/// @param options What is changed
/// @param quoteMarks The quotation marks of the text's language, asked for only when a
///        quotation mark changes
/// @return The change, in offsets of @p before; none when nothing changes
std::optional<AutoCorrection> autoCorrection(const QString& before,
                                             const AutoCorrectOptions& options,
                                             const std::function<QuoteMarks()>& quoteMarks);

}  // namespace kalahari::editor
