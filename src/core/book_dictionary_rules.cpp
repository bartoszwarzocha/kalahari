// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_dictionary_rules.cpp
/// @brief The book dictionary's words of a text, the places that go against its rules, and
///        the making of the rules

#include <kalahari/core/book_dictionary.h>

#include <kalahari/core/detail/book_dictionary_internal.h>

#include <kalahari/core/affix_expander.h>

#include <QUuid>

#include <algorithm>
#include <functional>
#include <utility>

namespace kalahari::core {

namespace {

using dictionary::capitalFirst;
using dictionary::isWordCharacter;
using dictionary::smallFirst;
using dictionary::TextWord;
using dictionary::wordsOf;

/// An apostrophe or a hyphen: joins the letters around it into one word (e-mail, Anna's)
bool isJoiner(QChar character) {
    return character == QLatin1Char('-') || character == QLatin1Char('\'') ||
           character == QChar(u'‐') || character == QChar(u'‑') || character == QChar(u'’');
}

/// Opening quotes, brackets and dashes: what can come before the first word of a sentence
bool opensSentence(QChar character) {
    static const QString opening = QStringLiteral("\"'([{„“«‹–—-");
    return character.isSpace() || opening.contains(character);
}

/// Closing quotes and brackets: what can come after the end of a sentence
bool closesSentence(QChar character) {
    static const QString closing = QStringLiteral("\"')]}”’»›");
    return closing.contains(character);
}

/// Whether the word at @p start starts a sentence: the paragraph starts there (after quotes
/// or a dash of a dialogue), or the text before it ends a sentence
bool startsSentence(const QString& text, qsizetype start) {
    qsizetype index = start - 1;
    while (index >= 0 && opensSentence(text.at(index))) {
        --index;
    }
    if (index < 0) {
        return true;
    }
    while (index >= 0 && closesSentence(text.at(index))) {
        --index;
    }
    static const QString ends = QStringLiteral(".!?…");
    return index >= 0 && ends.contains(text.at(index));
}

// =============================================================================
// Deviations from the rules
// =============================================================================

/// Finds the words of a paragraph that go against the rules
class DeviationFinder {
public:
    DeviationFinder(const QString& text, const QHash<QString, QList<WrongSpelling>>& wrong)
        : m_text(text), m_wrong(wrong), m_words(wordsOf(text)) {}

    /// The deviations, in their order
    [[nodiscard]] QList<RuleDeviation> find() const {
        QList<RuleDeviation> found;
        qsizetype index = 0;
        while (index < m_words.size()) {
            index += std::max<qsizetype>(matchAt(index, found), 1);
        }
        return found;
    }

private:
    /// The word @p index of the text
    [[nodiscard]] QString wordAt(qsizetype index) const {
        const TextWord& word = m_words.at(index);
        return m_text.mid(word.start, word.length);
    }

    /// Whether only spaces are between word @p index and the one before it
    [[nodiscard]] bool spacesBefore(qsizetype index) const {
        const TextWord& previous = m_words.at(index - 1);
        const qsizetype gapStart = previous.start + previous.length;
        const QString gap = m_text.mid(gapStart, m_words.at(index).start - gapStart);
        return !gap.isEmpty() && gap.trimmed().isEmpty();
    }

    /// Whether the words of @p spelling are in the text from word @p first on, one space
    /// apart; with @p capitalized, its first word with a capital letter
    [[nodiscard]] bool matches(qsizetype first, const WrongSpelling& spelling,
                               bool capitalized) const {
        if (first + spelling.words.size() > m_words.size()) {
            return false;
        }
        for (qsizetype index = 0; index < spelling.words.size(); ++index) {
            const QString wanted = index == 0 && capitalized ? capitalFirst(spelling.words.front())
                                                             : spelling.words.at(index);
            if (wordAt(first + index) != wanted || (index > 0 && !spacesBefore(first + index))) {
                return false;
            }
        }
        return true;
    }

    /// Add the deviation that starts at word @p index, if any
    /// @return How many words the wrong spelling found there takes (0: none is there)
    qsizetype matchAt(qsizetype index, QList<RuleDeviation>& found) const {
        const QString word = wordAt(index);
        const bool sentenceStart = startsSentence(m_text, m_words.at(index).start);
        bool capitalized = false;
        auto spellings = m_wrong.constFind(word);
        if (spellings == m_wrong.constEnd() && sentenceStart && word.front().isUpper()) {
            spellings = m_wrong.constFind(smallFirst(word));
            capitalized = true;
        }
        if (spellings == m_wrong.constEnd()) {
            return 0;
        }
        const auto spelling = std::ranges::find_if(
            *spellings, [this, index, capitalized](const WrongSpelling& candidate) {
                return matches(index, candidate, capitalized);
            });
        if (spelling == spellings->end()) {
            return 0;
        }
        const TextWord& last = m_words.at(index + spelling->words.size() - 1);
        const qsizetype start = m_words.at(index).start;
        const qsizetype length = last.start + last.length - start;
        const QString written = m_text.mid(start, length);
        const QString wanted = capitalized ? capitalFirst(spelling->right) : spelling->right;
        // A capital letter at the start of a sentence is right: "Okej" for "okej"
        const bool startOfRight = sentenceStart && written == capitalFirst(spelling->right);
        if (written != wanted && !startOfRight) {
            found.append(RuleDeviation{.start = start,
                                       .length = length,
                                       .text = written,
                                       .right = wanted,
                                       .ruleId = spelling->ruleId});
        }
        return spelling->words.size();
    }

    QString m_text;                                ///< The paragraph
    QHash<QString, QList<WrongSpelling>> m_wrong;  ///< First word -> wrong spellings
    QList<TextWord> m_words;                       ///< The words of the paragraph
};

// =============================================================================
// Rule forms
// =============================================================================

/// The shortest part of a word that splitting it into two words or finding its pattern takes
constexpr qsizetype MIN_PART = 3;

/// How a form is made of its word: so many characters taken from its end, and an ending
struct FormChange {
    qsizetype dropped = 0;  ///< Characters taken from the end of the word
    QString ending;         ///< Put in their place
    QString droppedText;    ///< The characters taken, in lower case
};

/// How @p form is made of @p word (smok -> smoka: nothing dropped, "a" added)
FormChange changeOf(const QString& word, const QString& form) {
    const QString lowerWord = word.toLower();
    const QString lowerForm = form.toLower();
    qsizetype common = 0;
    while (common < lowerWord.size() && common < lowerForm.size() &&
           lowerWord.at(common) == lowerForm.at(common)) {
        ++common;
    }
    return FormChange{.dropped = word.size() - common,
                      .ending = form.mid(common),
                      .droppedText = lowerWord.mid(common)};
}

/// @p word changed as a form is made of its word; empty when its end is not the same
QString applyChange(const QString& word, const FormChange& change) {
    if (word.size() < change.dropped || !word.toLower().endsWith(change.droppedText)) {
        return {};
    }
    return word.chopped(change.dropped) + change.ending;
}

/// The words the forms of a rule can come from, in the order they are tried: its right and
/// wrong spellings (also with a small first letter), then the words its right spelling ends
/// with, the longest first, as the pattern
std::pair<QStringList, QStringList> baseCandidates(const SpellingRule& rule) {
    QStringList own{rule.right, smallFirst(rule.right)};
    for (const QString& wrong : rule.wrong) {
        own.append(wrong);
        own.append(smallFirst(wrong));
    }
    own.removeDuplicates();
    QStringList patterns;
    const QString& right = rule.right;
    for (qsizetype start = 1; start + MIN_PART <= right.size(); ++start) {
        const QString tail = right.mid(start);
        if (tail.front().isLetter() && std::ranges::all_of(tail, isWordCharacter)) {
            patterns.append(tail);
        }
    }
    return {own, patterns};
}

/// The word of the spelling dictionary the forms of a rule come from: one of its spellings,
/// else a word its right spelling ends with (then the rule's pattern); empty: none
QString baseWordOf(SpellingRule& rule, const AffixExpander& dictionary) {
    const auto [own, patterns] = baseCandidates(rule);
    const auto known = [&dictionary](const QString& word) { return dictionary.hasEntry(word); };
    if (const auto found = std::ranges::find_if(own, known); found != own.end()) {
        return *found;
    }
    if (const auto found = std::ranges::find_if(patterns, known); found != patterns.end()) {
        rule.pattern = *found;
        return *found;
    }
    return {};
}

/// Give a rule each form of @p base made of its wrong spellings, with the same form of its
/// right spelling
void addFormsOf(SpellingRule& rule, const QString& base, const AffixExpander& dictionary) {
    for (const QString& form : dictionary.formsOf(base)) {
        const FormChange change = changeOf(base, form);
        const QString rightForm = applyChange(rule.right, change);
        if (rightForm.isEmpty()) {
            continue;
        }
        for (const QString& wrong : std::as_const(rule.wrong)) {
            const QString wrongForm = applyChange(wrong, change);
            if (!wrongForm.isEmpty() && wrongForm != rightForm) {
                rule.forms.insert(wrongForm, rightForm);
            }
        }
    }
}

}  // namespace

namespace dictionary {

bool isWordCharacter(QChar character) {
    return character.isLetter() || character.isMark();
}

QList<TextWord> wordsOf(const QString& text) {
    QList<TextWord> words;
    qsizetype index = 0;
    while (index < text.size()) {
        if (!text.at(index).isLetter()) {
            ++index;
            continue;
        }
        qsizetype end = index;
        while (end < text.size() &&
               (isWordCharacter(text.at(end)) ||
                (isJoiner(text.at(end)) && end + 1 < text.size() && text.at(end + 1).isLetter()))) {
            ++end;
        }
        words.append(TextWord{.start = index, .length = end - index});
        index = end;
    }
    return words;
}

QStringList wordTexts(const QString& text) {
    QStringList texts;
    for (const TextWord& word : wordsOf(text)) {
        texts.append(text.mid(word.start, word.length));
    }
    return texts;
}

QString capitalFirst(const QString& text) {
    return text.isEmpty() ? text : text.front().toUpper() + text.mid(1);
}

QString smallFirst(const QString& text) {
    return text.isEmpty() ? text : text.front().toLower() + text.mid(1);
}

bool isLowerCase(const QString& text) {
    return text == text.toLower();
}

QString beforeApostrophe(const QString& word) {
    for (qsizetype index = 1; index < word.size(); ++index) {
        if (word.at(index) == QLatin1Char('\'') || word.at(index) == QChar(u'’')) {
            return word.left(index);
        }
    }
    return {};
}

QList<RuleDeviation> findDeviations(const QString& text,
                                    const QHash<QString, QList<WrongSpelling>>& wrong) {
    return DeviationFinder(text, wrong).find();
}

QString newId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

}  // namespace dictionary

// =============================================================================
// Making rules
// =============================================================================

QStringList suggestedWrongSpellings(const QString& right,
                                    const std::function<bool(const QString&)>& isWord) {
    QStringList spellings;
    const QString text = right.simplified();
    if (text.isEmpty()) {
        return spellings;
    }
    const QString small = smallFirst(text);
    if (small != text && text != text.toUpper()) {
        spellings.append(small);  // Smok: smok
    }
    if (text.contains(QLatin1Char('-'))) {
        spellings.append(QString(text).remove(QLatin1Char('-')));  // e-mail: email
        spellings.append(QString(text).replace(QLatin1Char('-'), QLatin1Char(' ')));
    } else if (text.contains(QLatin1Char(' '))) {
        spellings.append(QString(text).remove(QLatin1Char(' ')));  // na pewno: napewno
        spellings.append(QString(text).replace(QLatin1Char(' '), QLatin1Char('-')));
    } else if (isWord) {
        // A word made of two words: półelf -> pół-elf, pół elf
        for (qsizetype split = MIN_PART; split + MIN_PART <= text.size(); ++split) {
            const QString first = text.left(split);
            const QString second = text.mid(split);
            if (isWord(first) && isWord(second)) {
                spellings.append(first + QLatin1Char('-') + second);
                spellings.append(first + QLatin1Char(' ') + second);
                break;
            }
        }
    }
    spellings.removeDuplicates();
    spellings.removeAll(text);
    return spellings;
}

void fillRuleForms(SpellingRule& rule, const AffixExpander* dictionary) {
    rule.forms.clear();
    rule.pattern.clear();
    for (const QString& wrong : std::as_const(rule.wrong)) {
        if (!wrong.isEmpty() && wrong != rule.right) {
            rule.forms.insert(wrong, rule.right);
        }
    }
    if (!rule.allForms || dictionary == nullptr || !dictionary->isLoaded() ||
        rule.right.isEmpty()) {
        return;
    }
    if (const QString base = baseWordOf(rule, *dictionary); !base.isEmpty()) {
        addFormsOf(rule, base, *dictionary);
    }
}

}  // namespace kalahari::core
