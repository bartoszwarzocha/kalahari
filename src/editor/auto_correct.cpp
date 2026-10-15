/// @file auto_correct.cpp
/// @brief Autocorrect: the typographic characters that take the place of what is typed

#include <kalahari/editor/auto_correct.h>

#include <QLocale>

#include <utility>

namespace kalahari::editor {

namespace {

constexpr QChar QUOTATION_MARK = u'"';
constexpr QChar APOSTROPHE = u'\'';
constexpr QChar HYPHEN = u'-';
constexpr QChar SPACE = u' ';
constexpr QChar PERIOD = u'.';
constexpr QChar TYPOGRAPHIC_APOSTROPHE = u'’';  // ’
constexpr QChar EM_DASH = u'—';                 // —
constexpr QChar EN_DASH = u'–';                 // –
constexpr QChar ELLIPSIS = u'…';                // …

/// @brief The marks of the plan of the editor: Polish „…” and ‚…’
constexpr QuoteMarks POLISH_MARKS{u'„', u'”', u'‚', u'’'};

/// @brief English “…” and ‘…’
constexpr QuoteMarks ENGLISH_MARKS{u'“', u'”', u'‘', u'’'};

/// @brief The marks a locale puts around a text, if it puts one mark on each side
std::optional<std::pair<QChar, QChar>> marksOf(const QLocale& locale,
                                               QLocale::QuotationStyle style) {
    const QString quoted = locale.quoteString(QStringLiteral("x"), style);
    if (quoted.size() != 3 || quoted.at(1) != u'x' || quoted.at(0) == QUOTATION_MARK ||
        quoted.at(0) == APOSTROPHE) {
        return std::nullopt;
    }
    return std::make_pair(quoted.at(0), quoted.at(2));
}

/// @brief Whether the quotation mark at @p markIndex of @p before opens a quotation: at the
///        start of the paragraph, after a space, an opening bracket, a dash or the opening
///        mark of the other kind (a quotation in a quotation, as „‚…’”)
/// @param ownOpening The opening mark of the kind typed
/// @param otherOpening The opening mark of the other kind: double for a single mark typed,
///        single for a double one. After the opening mark of its own kind a mark closes
///        (an empty pair).
bool opensQuotation(const QString& before, qsizetype markIndex, QChar ownOpening,
                    QChar otherOpening) {
    if (markIndex == 0) {
        return true;  // the start of the paragraph
    }
    const QChar previous = before.at(markIndex - 1);
    return previous.isSpace() || previous == u'(' || previous == u'[' || previous == u'{' ||
           previous == u'<' || previous == EN_DASH || previous == EM_DASH ||
           (previous == otherOpening && otherOpening != ownOpening);
}

/// @brief Whether a character belongs to a word: a letter, a digit or a mark of a letter
bool isWordCharacter(QChar ch) {
    return ch.isLetterOrNumber() || ch.isMark();
}

/// @brief Whether a quotation in single marks is open at the end of @p text
///
/// The last opening single mark is open unless a closing one follows it; a closing mark
/// followed by a letter is an apostrophe, as in "don’t". A language whose single marks are
/// its double ones (French «…») has no quotations told apart by them.
bool singleQuotationOpen(const QString& text, const QuoteMarks& marks) {
    if (marks.singleOpening == marks.doubleOpening) {
        return false;
    }
    for (qsizetype i = text.size() - 1; i >= 0; --i) {
        const QChar ch = text.at(i);
        if (ch == marks.singleOpening) {
            return true;
        }
        if (ch == marks.singleClosing &&
            (i + 1 == text.size() || !isWordCharacter(text.at(i + 1)))) {
            return false;
        }
    }
    return false;
}

/// @brief One character in the place of the last one of @p before
AutoCorrection lastCharacterTo(const QString& before, QChar replacement) {
    const int end = static_cast<int>(before.size());
    return {end - 1, end, QString(replacement)};
}

}  // namespace

QuoteMarks QuoteMarks::forLanguage(const QString& language) {
    const QLocale locale(language.trimmed());
    switch (locale.language()) {
        case QLocale::Polish:
            return POLISH_MARKS;  // Qt's locale data has «…» for the single ones
        case QLocale::English:
        case QLocale::C:
        case QLocale::AnyLanguage:
            return ENGLISH_MARKS;
        default:
            break;
    }
    const auto doubleMarks = marksOf(locale, QLocale::StandardQuotation);
    const auto singleMarks = marksOf(locale, QLocale::AlternateQuotation);
    if (!doubleMarks || !singleMarks) {
        return ENGLISH_MARKS;
    }
    return {doubleMarks->first, doubleMarks->second, singleMarks->first, singleMarks->second};
}

bool startsAutoCorrection(QChar typed) {
    return typed == QUOTATION_MARK || typed == APOSTROPHE || typed == SPACE || typed == PERIOD;
}

std::optional<AutoCorrection> autoCorrection(const QString& before,
                                             const AutoCorrectOptions& options,
                                             const std::function<QuoteMarks()>& quoteMarks) {
    if (before.isEmpty()) {
        return std::nullopt;
    }
    const qsizetype last = before.size() - 1;
    const QChar typed = before.at(last);

    if (typed == QUOTATION_MARK) {
        if (!options.quotes) {
            return std::nullopt;
        }
        const QuoteMarks marks = quoteMarks();
        const bool opens =
            opensQuotation(before, last, marks.doubleOpening, marks.singleOpening);
        return lastCharacterTo(before, opens ? marks.doubleOpening : marks.doubleClosing);
    }

    if (typed == APOSTROPHE) {
        if (!options.quotes && !options.apostrophe) {
            return std::nullopt;
        }
        const QuoteMarks marks = quoteMarks();
        if (opensQuotation(before, last, marks.singleOpening, marks.doubleOpening)) {
            // At the start of a word a straight mark opens a quotation; an apostrophe there
            // ('90s) is left as typed when the quotation marks are not changed
            if (!options.quotes) {
                return std::nullopt;
            }
            return lastCharacterTo(before, marks.singleOpening);
        }
        if (options.quotes && singleQuotationOpen(before.left(last), marks)) {
            return lastCharacterTo(before, marks.singleClosing);
        }
        if (!options.apostrophe) {
            return std::nullopt;
        }
        return lastCharacterTo(before, TYPOGRAPHIC_APOSTROPHE);
    }

    if (typed == SPACE) {
        // Only the hyphen and the space typed after it start the paragraph
        if (!options.dialogueDash || before.size() != 2 || before.at(0) != HYPHEN) {
            return std::nullopt;
        }
        return AutoCorrection{0, 1, QString(options.dash == DialogueDash::En ? EN_DASH : EM_DASH)};
    }

    if (typed == PERIOD) {
        // Exactly three periods: a fourth one before them was meant (an undone ellipsis, or
        // the writer's own row of periods)
        if (!options.ellipsis || before.size() < 3 || before.at(last - 1) != PERIOD ||
            before.at(last - 2) != PERIOD || (before.size() > 3 && before.at(last - 3) == PERIOD)) {
            return std::nullopt;
        }
        const int end = static_cast<int>(before.size());
        return AutoCorrection{end - 3, end, QString(ELLIPSIS)};
    }

    return std::nullopt;
}

}  // namespace kalahari::editor
