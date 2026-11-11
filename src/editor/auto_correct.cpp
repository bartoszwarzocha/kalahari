/// @file auto_correct.cpp
/// @brief Autocorrect: the typographic characters that take the place of what is typed

#include <kalahari/editor/auto_correct.h>

#include <QLocale>
#include <QRegularExpression>

#include <algorithm>
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

/// @brief The scene break and the horizontal line typed in a paragraph of their own
const QString SCENE_BREAK_SHORTCUT = QStringLiteral("***");
const QString HORIZONTAL_RULE_SHORTCUT = QStringLiteral("---");

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

// =============================================================================
// The words
// =============================================================================

/// @brief What ends the word before it
enum class WordEnd {
    Space,        ///< A space or a line break
    Punctuation,  ///< . , ; : ! ?
    Paragraph     ///< Enter
};

/// @brief Whether a punctuation mark ends a word: a period, a comma, a semicolon, a colon,
///        a question or an exclamation mark
bool endsWord(QChar ch) {
    return ch == PERIOD || ch == u',' || ch == u';' || ch == u':' || ch == u'!' || ch == u'?';
}

/// @brief Whether a character can open a word: a quotation mark or an opening bracket
bool isOpening(QChar ch) {
    return ch == QUOTATION_MARK || ch == APOSTROPHE || ch == u'„' || ch == u'“' ||
           ch == u'‚' || ch == u'‘' || ch == u'«' || ch == u'»' || ch == u'‹' || ch == u'›' ||
           ch == u'(' || ch == u'[' || ch == u'{' || ch == u'¿' || ch == u'¡';
}

/// @brief Whether a character can close a word: a quotation mark or a closing bracket
///        (“ and ‘ close German quotations, » and « French ones in both directions)
bool isClosing(QChar ch) {
    return ch == QUOTATION_MARK || ch == APOSTROPHE || ch == u'”' || ch == u'“' ||
           ch == u'’' || ch == u'‘' || ch == u'»' || ch == u'«' || ch == u'›' || ch == u'‹' ||
           ch == u')' || ch == u']' || ch == u'}';
}

/// @brief Whether a word with the mark typed after it goes on as an Internet address, which
///        keeps its letters as they are typed: "www" before a period, a scheme ("http",
///        "https", "ftp", "mailto") before a colon, an e-mail address ("jan@example") before
///        a period
bool goesOnAsAddress(const QString& before, QChar typed) {
    qsizetype start = before.size();
    while (start > 0 && !before.at(start - 1).isSpace()) {
        --start;
    }
    qsizetype lead = start;
    while (lead < before.size() && isOpening(before.at(lead))) {
        ++lead;
    }
    const QString word = before.mid(lead).toLower();
    if (typed == PERIOD) {
        return word == QStringLiteral("www") || word.contains(u'@');
    }
    return typed == u':' &&
           (word == QStringLiteral("http") || word == QStringLiteral("https") ||
            word == QStringLiteral("ftp") || word == QStringLiteral("mailto"));
}

/// @brief Whether a character is a dash of dialogue or of a sentence: a hyphen alone
///        (not changed by autocorrect), an en dash or an em dash
bool isDash(QChar ch) {
    return ch == HYPHEN || ch == EN_DASH || ch == EM_DASH;
}

/// @brief The text with its first character a capital
QString withCapital(const QString& text) {
    if (text.isEmpty()) {
        return text;
    }
    return text.at(0).toUpper() + text.mid(1);
}

/// @brief Whether a text has a letter in it, all its letters capitals
bool isUpperCase(const QString& text) {
    bool letter = false;
    for (const QChar ch : text) {
        if (ch.isLower()) {
            return false;
        }
        letter = letter || ch.isUpper();
    }
    return letter;
}

/// @brief The text that takes the place of a word in the table
///
/// The pair of the word as typed goes first; a word typed with a capital first, or in
/// capitals, takes the pair of its small letters and gives the replacement the same.
std::optional<QString> replacementOf(const QString& word,
                                     const std::vector<Replacement>& replacements) {
    for (const Replacement& pair : replacements) {
        if (!pair.from.isEmpty() && pair.from == word) {
            return pair.to;
        }
    }
    for (const Replacement& pair : replacements) {
        if (pair.from.isEmpty() || pair.from.compare(word, Qt::CaseInsensitive) != 0) {
            continue;
        }
        if (word == withCapital(pair.from)) {
            return withCapital(pair.to);
        }
        if (word.size() > 1 && isUpperCase(word) && word == pair.from.toUpper()) {
            return pair.to.toUpper();
        }
    }
    return std::nullopt;
}

/// @brief Whether a word is an abbreviation of the list; the period at the end of either
///        does not count, nor does the case
bool isAbbreviation(QString word, const QStringList& abbreviations) {
    if (word.endsWith(PERIOD)) {
        word.chop(1);
    }
    if (word.isEmpty()) {
        return false;
    }
    for (QString abbreviation : abbreviations) {
        abbreviation = abbreviation.trimmed();
        if (abbreviation.endsWith(PERIOD)) {
            abbreviation.chop(1);
        }
        if (abbreviation.compare(word, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

/// @brief Where a word is: whether it starts a sentence, and the word with a period that
///        ends the sentence before it
struct SentencePlace {
    bool starts = false;
    QString afterPeriod;  ///< The word before the period ("godz."), when a period ends the
                          ///< sentence before
};

/// @brief Where the word that starts at @p wordStart of @p text is
SentencePlace sentencePlace(const QString& text, qsizetype wordStart,
                            const QStringList& abbreviations) {
    const auto skipSpaces = [&text](qsizetype i) {
        while (i > 0 && text.at(i - 1).isSpace()) {
            --i;
        }
        return i;
    };
    qsizetype end = skipSpaces(wordStart);
    if (end == 0) {
        return {true, {}};  // the start of the paragraph
    }

    // A dash after a space: a line of dialogue starts after it at the start of the
    // paragraph, or after a sentence that ends with a period ("— Tak — powiedział. — Idę");
    // after ? or ! the narration goes on ("— Naprawdę? — zapytała")
    bool afterDash = false;
    if (end < wordStart && isDash(text.at(end - 1)) &&
        (end == 1 || text.at(end - 2).isSpace())) {
        end = skipSpaces(end - 1);
        if (end == 0) {
            return {true, {}};
        }
        afterDash = true;
    }

    // The closing marks after the end of a sentence: tak.” or (tak.)
    while (end > 0 && isClosing(text.at(end - 1))) {
        --end;
    }
    if (end == 0) {
        return {};
    }
    const QChar mark = text.at(end - 1);
    if (mark == u'?' || mark == u'!') {
        return {!afterDash, {}};
    }
    if (mark != PERIOD) {
        return {};  // a comma, a colon, a word: the sentence goes on
    }

    // The word with the period, without the marks that open it
    qsizetype start = end;
    while (start > 0 && !text.at(start - 1).isSpace()) {
        --start;
    }
    while (start < end && isOpening(text.at(start))) {
        ++start;
    }
    const QString word = text.mid(start, end - start);
    const QString bare = word.chopped(1);
    if (bare.isEmpty() || bare.endsWith(PERIOD)) {
        return {};  // a period alone, or the periods of an ellipsis
    }
    if (bare.size() == 1 && bare.at(0).isLetter()) {
        return {};  // an initial: J. Kowalski
    }
    if (std::all_of(bare.begin(), bare.end(), [](QChar ch) { return ch.isDigit(); })) {
        return {};  // an ordinal number: 5. pułk
    }
    if (isAbbreviation(word, abbreviations)) {
        return {};
    }
    return {true, word};
}

/// @brief The dash a hyphen between words becomes in a paragraph
QChar wordDashOf(const QString& paragraph, const AutoCorrectOptions& options) {
    // A line of dialogue: the dash it starts with ("— Tak — powiedział")
    if (options.dialogueDashInDialogue && paragraph.size() > 1 &&
        (paragraph.at(0) == EM_DASH || paragraph.at(0) == EN_DASH) &&
        paragraph.at(1).isSpace()) {
        return paragraph.at(0);
    }
    return options.wordDash == DialogueDash::En ? EN_DASH : EM_DASH;
}

/// @brief Two hyphens between two characters of words to an en dash ("1939--1945")
QString withDoubleHyphensChanged(const QString& word) {
    QString changed;
    changed.reserve(word.size());
    for (qsizetype i = 0; i < word.size(); ++i) {
        if (i > 0 && i + 2 < word.size() && word.at(i) == HYPHEN && word.at(i + 1) == HYPHEN &&
            isWordCharacter(word.at(i - 1)) && isWordCharacter(word.at(i + 2))) {
            changed += EN_DASH;
            ++i;
        } else {
            changed += word.at(i);
        }
    }
    return changed;
}

/// @brief A change of a word: only its characters that change, so that those that stay
///        keep their formats and annotations; none when nothing changes
std::optional<AutoCorrection> changeOf(const QString& word, const QString& changed, int start) {
    if (changed == word) {
        return std::nullopt;
    }
    qsizetype prefix = 0;
    while (prefix < word.size() && prefix < changed.size() &&
           word.at(prefix) == changed.at(prefix)) {
        ++prefix;
    }
    qsizetype suffix = 0;
    while (suffix < word.size() - prefix && suffix < changed.size() - prefix &&
           word.at(word.size() - 1 - suffix) == changed.at(changed.size() - 1 - suffix)) {
        ++suffix;
    }
    return AutoCorrection{start + static_cast<int>(prefix),
                          start + static_cast<int>(word.size() - suffix),
                          changed.mid(prefix, changed.size() - prefix - suffix)};
}

/// @brief A replacement of the table for the word that starts at @p wordStart of @p text,
///        with a capital at the start of a sentence
QString replacementAt(const QString& text, qsizetype wordStart, const QString& replacement,
                      const AutoCorrectOptions& options) {
    if (options.sentenceCapitals && !replacement.isEmpty() && replacement.at(0).isLower() &&
        sentencePlace(text, wordStart, options.abbreviations).starts) {
        return withCapital(replacement);
    }
    return replacement;
}

/// @brief The change of the word at the end of @p text, before what ends it
std::optional<AutoCorrection> wordCorrection(const QString& text, WordEnd wordEnd,
                                             const AutoCorrectOptions& options,
                                             bool capsLockOn) {
    qsizetype tokenStart = text.size();
    while (tokenStart > 0 && !text.at(tokenStart - 1).isSpace()) {
        --tokenStart;
    }
    const QString token = text.mid(tokenStart);
    if (token.isEmpty() || token.contains(QChar::ObjectReplacementCharacter)) {
        return std::nullopt;
    }
    const int start = static_cast<int>(tokenStart);
    const int end = static_cast<int>(text.size());

    // The whole word in the table, punctuation marks in it too ("(c)", "e.g.")
    if (const std::optional<QString> replacement = replacementOf(token, options.replacements)) {
        return changeOf(token, replacementAt(text, tokenStart, *replacement, options), start);
    }

    // A word with a punctuation mark in it had its changes when the mark was typed
    if (std::any_of(token.begin(), token.end(), endsWord)) {
        return std::nullopt;
    }

    // Two hyphens alone, and a hyphen alone after a word and a space, when a space follows
    if (token == QStringLiteral("--")) {
        if (!options.doubleHyphen || wordEnd == WordEnd::Punctuation) {
            return std::nullopt;
        }
        return AutoCorrection{start, end, QString(EN_DASH)};
    }
    if (token == QString(HYPHEN)) {
        // A space or more before it, and a word before them; a hyphen starting the paragraph
        // or a line is not between words
        qsizetype before = tokenStart;
        while (before > 0 && text.at(before - 1).isSpace() &&
               text.at(before - 1) != QChar::LineSeparator) {
            --before;
        }
        if (!options.dashBetweenWords || wordEnd != WordEnd::Space || before == tokenStart ||
            before == 0 || text.at(before - 1).isSpace()) {
            return std::nullopt;
        }
        return AutoCorrection{start, end, QString(wordDashOf(text, options))};
    }

    // The word without the quotation marks and brackets around it
    qsizetype lead = 0;
    while (lead < token.size() && isOpening(token.at(lead))) {
        ++lead;
    }
    qsizetype trail = 0;
    while (trail < token.size() - lead && isClosing(token.at(token.size() - 1 - trail))) {
        ++trail;
    }

    // The table: the word with fewer of the marks around it first ("„(c)”": "(c)")
    for (qsizetype stripped = 1; stripped <= lead + trail; ++stripped) {
        for (qsizetype left = std::min(stripped, lead); left >= 0; --left) {
            const qsizetype right = stripped - left;
            if (right > trail) {
                break;
            }
            const QString part = token.mid(left, token.size() - left - right);
            if (part.isEmpty()) {
                continue;
            }
            if (const std::optional<QString> replacement =
                    replacementOf(part, options.replacements)) {
                return changeOf(part, replacementAt(text, tokenStart, *replacement, options),
                                start + static_cast<int>(left));
            }
        }
    }

    const QString word = token.mid(lead, token.size() - lead - trail);
    if (word.isEmpty()) {
        return std::nullopt;
    }
    const int wordStart = start + static_cast<int>(lead);
    QString changed = options.doubleHyphen ? withDoubleHyphensChanged(word) : word;
    const bool onlyCase = changed == word;  // undone, a change of the case alone is learned

    // Caps Lock on by mistake: "wIELKI" typed for "Wielki"
    const QString rest = changed.mid(1);
    const auto restLetters = std::count_if(rest.begin(), rest.end(),
                                           [](QChar ch) { return ch.isLetter(); });
    if (options.capsLock && capsLockOn && changed.at(0).isLower() && restLetters > 1 &&
        isUpperCase(rest)) {
        std::optional<AutoCorrection> change =
            changeOf(word, withCapital(changed.toLower()), wordStart);
        if (change) {
            change->capsLock = true;
        }
        return change;
    }

    // Two capitals at the start of a word, small letters after them: "TWo"
    const QString afterTwo = changed.mid(2);
    if (options.twoInitialCapitals && changed.size() > 2 && changed.at(0).isUpper() &&
        changed.at(1).isUpper() &&
        std::any_of(afterTwo.begin(), afterTwo.end(), [](QChar ch) { return ch.isLower(); }) &&
        std::none_of(afterTwo.begin(), afterTwo.end(), [](QChar ch) { return ch.isUpper(); }) &&
        !options.twoCapitalsExceptions.contains(word)) {
        changed[1] = changed.at(1).toLower();
        std::optional<AutoCorrection> change = changeOf(word, changed, wordStart);
        if (change && onlyCase) {
            change->learns = LearnedException::TwoCapitals;
            change->exception = word;
        }
        return change;
    }

    // The first letter of a sentence, in a word without capitals ("iPhone" stays)
    if (options.sentenceCapitals && changed.at(0).isLower() &&
        std::none_of(changed.begin(), changed.end(), [](QChar ch) { return ch.isUpper(); })) {
        const SentencePlace place = sentencePlace(text, tokenStart, options.abbreviations);
        if (place.starts) {
            std::optional<AutoCorrection> change =
                changeOf(word, withCapital(changed), wordStart);
            if (change && onlyCase && !place.afterPeriod.isEmpty()) {
                change->learns = LearnedException::Abbreviation;
                change->exception = place.afterPeriod;
            }
            return change;
        }
    }

    return changeOf(word, changed, wordStart);
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
    return typed == QUOTATION_MARK || typed == APOSTROPHE || typed.isSpace() || endsWord(typed);
}

std::optional<AutoCorrection> autoCorrection(const QString& before,
                                             const AutoCorrectOptions& options,
                                             const std::function<QuoteMarks()>& quoteMarks,
                                             bool capsLockOn) {
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

    if (typed == SPACE && before.size() == 2 && before.at(0) == HYPHEN) {
        // Only the hyphen and the space typed after it start the paragraph
        if (!options.dialogueDash) {
            return std::nullopt;
        }
        return AutoCorrection{0, 1, QString(options.dash == DialogueDash::En ? EN_DASH : EM_DASH)};
    }

    if (typed == PERIOD && before.size() >= 3 && before.at(last - 1) == PERIOD &&
        before.at(last - 2) == PERIOD) {
        // Exactly three periods: a fourth one before them was meant (an undone ellipsis, or
        // the writer's own row of periods)
        if (!options.ellipsis || (before.size() > 3 && before.at(last - 3) == PERIOD)) {
            return std::nullopt;
        }
        const int end = static_cast<int>(before.size());
        return AutoCorrection{end - 3, end, QString(ELLIPSIS)};
    }

    if (endsWord(typed) && goesOnAsAddress(before.left(last), typed)) {
        return std::nullopt;
    }

    if (typed.isSpace() || endsWord(typed)) {
        return wordCorrection(before.left(last),
                              typed.isSpace() ? WordEnd::Space : WordEnd::Punctuation, options,
                              capsLockOn);
    }

    return std::nullopt;
}

std::optional<AutoCorrection> autoCorrectionAtParagraphEnd(const QString& paragraph,
                                                           const AutoCorrectOptions& options,
                                                           bool capsLockOn) {
    return wordCorrection(paragraph, WordEnd::Paragraph, options, capsLockOn);
}

std::optional<AutoLink> autoLink(const QString& text, const AutoCorrectOptions& options) {
    if (!options.links) {
        return std::nullopt;
    }
    qsizetype start = text.size();
    while (start > 0 && !text.at(start - 1).isSpace()) {
        --start;
    }
    while (start < text.size() && isOpening(text.at(start))) {
        ++start;
    }
    // The punctuation marks and the closing marks after it; a closing bracket the address
    // opened stays ("…/wiki/Kalahari_(desert)")
    qsizetype end = text.size();
    while (end > start) {
        const QChar last = text.at(end - 1);
        const QStringView address = QStringView(text).mid(start, end - start);
        const bool ownBracket = last == u')' && address.count(u'(') >= address.count(u')');
        if ((!endsWord(last) && !isClosing(last)) || ownBracket) {
            break;
        }
        --end;
    }
    const QString address = text.mid(start, end - start);
    if (address.isEmpty() || address.contains(QChar::ObjectReplacementCharacter)) {
        return std::nullopt;
    }

    const auto matches = [&address](const QString& pattern) {
        const QRegularExpression expression(
            pattern, QRegularExpression::CaseInsensitiveOption |
                         QRegularExpression::UseUnicodePropertiesOption);
        return expression.match(address).hasMatch();
    };
    const QString email = QStringLiteral("[\\w.%+-]+@[\\w-]+(\\.[\\w-]+)+");
    const int from = static_cast<int>(start);
    const int to = static_cast<int>(end);
    if (matches(QStringLiteral("^(https?|ftp)://[\\w-]")) ||
        matches(QStringLiteral("^mailto:") + email + QLatin1Char('$'))) {
        return AutoLink{from, to, address};
    }
    if (matches(QStringLiteral("^www\\.[\\w-]+(\\.[\\w-]+)+([/?#:]\\S*)?$"))) {
        return AutoLink{from, to, QStringLiteral("https://") + address};
    }
    if (matches(QLatin1Char('^') + email + QLatin1Char('$'))) {
        return AutoLink{from, to, QStringLiteral("mailto:") + address};
    }
    return std::nullopt;
}

QString abbreviationsLanguage(const QString& language,
                              const std::map<QString, QStringList>& lists) {
    // The language without its country: "pl" of "pl_PL" and "pl-PL"
    QString code = language.trimmed().toLower();
    code = code.left(code.indexOf(QRegularExpression(QStringLiteral("[_-]"))));
    if (lists.contains(code)) {
        return code;
    }
    const QString english = QStringLiteral("en");
    return lists.contains(english) ? english : QString();
}

TextObject paragraphObjectShortcut(const QString& paragraph, const AutoCorrectOptions& options) {
    const QString text = paragraph.trimmed();
    if (options.sceneBreak && text == SCENE_BREAK_SHORTCUT) {
        return TextObject::SceneBreak;
    }
    if (options.horizontalRule && text == HORIZONTAL_RULE_SHORTCUT) {
        return TextObject::HorizontalRule;
    }
    return TextObject::None;
}

}  // namespace kalahari::editor
