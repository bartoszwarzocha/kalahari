// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_dictionary.h
/// @brief The dictionary of a book: its names in all their forms, the author's rules of
///        spelling and the words right only in this book
///
/// The checking of spelling takes every form of a name of the book for right, but only as it
/// is written ("Annie", not "annie"), marks the words that go against a rule of the book
/// ("smoka" where the book writes "Smoka") and takes the words of the book for right; Quick
/// Insert offers the names with their forms, and Find in Book finds every form of a name.
///
/// The dictionary belongs to the book and is kept in its folder, dictionary.json:
/// @code{.json}
/// {
///   "format": 1,
///   "names": [
///     { "id": "8c3f...", "kind": "character", "card": "e4", "plural": true,
///       "variants": [
///         { "text": "Anna Wolska", "models": ["anna", "kowalska"],
///           "forms": { "nominative": ["Anna Wolska"], "genitive": ["Anny Wolskiej"],
///                      "nominativePlural": ["Anny Wolskie"] },
///           "edited": ["vocative"] },
///         { "text": "Anna", "models": ["anna"], "automatic": true, "forms": { } },
///         { "text": "Ania", "models": ["anna"], "forms": { } }
///       ] }
///   ],
///   "rules": [
///     { "id": "1d2e...", "right": "Smok", "wrong": ["smok"], "note": "Smoki to ród.",
///       "forms": { "smok": "Smok", "smoka": "Smoka" } },
///     { "id": "7a9b...", "right": "okej", "wrong": ["OK", "okey"], "allForms": false,
///       "forms": { "OK": "okej", "okey": "okej" } }
///   ],
///   "words": ["Kirran", "eldryjski"]
/// }
/// @endcode
///
/// - A name has its kind ("character", "place", "item", "other"), the Workshop card it is
///   ("card", once there are cards), whether its plural is used ("plural") and whether Quick
///   Insert offers it ("quickInsert", true when left out). Its first variant is the name
///   itself; the others are its other names: the first name or the surname alone, which the
///   program adds for a character ("automatic"), diminutives and nicknames.
/// - A variant has the model of declension of each of its words ("models", see
///   NameDeclension), its forms by case ("forms", FormTable::cellKey()) and the cells the
///   author wrote ("edited"), which keep their forms when the model changes.
/// - A rule has the right spelling ("right"), the wrong ones ("wrong"), whether it covers
///   their forms too ("allForms", true when left out), the author's note ("note"), the word
///   of the spelling dictionary its forms come from when that is another word ("pattern":
///   "elf" for "półelf") and each wrong form with its right form ("forms").
/// - "words" are the words right only in this book: one in lower case is right in any case,
///   one with capitals only with them.

#ifndef KALAHARI_CORE_BOOK_DICTIONARY_H
#define KALAHARI_CORE_BOOK_DICTIONARY_H

#include <kalahari/core/name_declension.h>

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>

namespace kalahari::core {

class AffixExpander;

/// @brief One name of a name of the dictionary: the name itself, or another name of it
struct NameVariant {
    QString text;            ///< "Anna Wolska", "Ania"
    QStringList models;      ///< The model of declension of each word, in their order
    FormTable forms;         ///< Its forms by case
    QStringList edited;      ///< The cells the author wrote (FormTable::cellKey())
    bool automatic = false;  ///< The first name or the surname alone, added by the program

    /// @brief Whether two variants have the same content
    [[nodiscard]] bool operator==(const NameVariant& other) const = default;
};

/// @brief A name of the book: a character, a place, a thing or another proper name
struct DictionaryName {
    QString id;                           ///< Its id in the dictionary
    NameKind kind = NameKind::Character;  ///< What it names
    QString card;                         ///< The id of the Workshop card it is; empty: none
    bool plural = false;                  ///< Its plural is used ("u Wolskich")
    bool quickInsert = true;              ///< Quick Insert offers it
    QList<NameVariant> variants;          ///< The name itself first, then its other names

    /// @brief The name itself (the text of the first variant)
    [[nodiscard]] QString text() const;

    /// @brief Every form of every variant, each once (the plural ones when it is used)
    [[nodiscard]] QStringList allForms() const;

    /// @brief Whether two names have the same content
    [[nodiscard]] bool operator==(const DictionaryName& other) const = default;
};

/// @brief How the author wants a word written in the book
struct SpellingRule {
    QString id;                    ///< Its id in the dictionary
    QString right;                 ///< The right spelling: "Smok", "e-mail"
    QStringList wrong;             ///< The wrong ones: "smok"; "email", "e mail", "mail"
    bool allForms = true;          ///< It covers their forms too ("smoka" -> "Smoka")
    QString note;                  ///< The author's note on it
    QString pattern;               ///< The word whose forms it has, when another ("elf")
    QMap<QString, QString> forms;  ///< Each wrong form -> its right form (fillRuleForms())

    /// @brief Whether two rules have the same content
    [[nodiscard]] bool operator==(const SpellingRule& other) const = default;
};

/// @brief A word of a text that goes against a rule of the book
struct RuleDeviation {
    qsizetype start = 0;   ///< Where it starts in the text
    qsizetype length = 0;  ///< How long it is (it can be several words: "e mail")
    QString text;          ///< As it is written: "smoka"
    QString right;         ///< As the rule writes it there: "Smoka"
    QString ruleId;        ///< The rule

    /// @brief Whether two deviations have the same content
    [[nodiscard]] bool operator==(const RuleDeviation& other) const = default;
};

/// @brief A wrong form of a rule as the checking looks for it in a text
struct WrongSpelling {
    QStringList words;  ///< Its words: "e", "mail"
    QString text;       ///< As the rule has it: "e mail"
    QString right;      ///< Its right form: "e-mail"
    QString ruleId;     ///< The rule
};

/// @brief The dictionary of a book: names, rules of spelling and words of the book
class BookDictionary {
public:
    /// @brief The name of its file in the folder of the book ("dictionary.json")
    [[nodiscard]] static QString fileName();

    /// @brief Its format, written in the file
    static constexpr int FORMAT = 1;

    // =========================================================================
    // Content
    // =========================================================================

    /// @brief The names, in the order they were added
    [[nodiscard]] const QList<DictionaryName>& names() const;

    /// @brief The rules of spelling, in the order they were added
    [[nodiscard]] const QList<SpellingRule>& rules() const;

    /// @brief The words of the book, sorted
    [[nodiscard]] const QStringList& words() const;

    /// @brief Whether it has no names, rules or words
    [[nodiscard]] bool isEmpty() const;

    /// @brief The name with id @p nameId, or nullptr
    [[nodiscard]] const DictionaryName* findName(const QString& nameId) const;

    /// @brief The name of Workshop card @p card, or nullptr
    [[nodiscard]] const DictionaryName* findNameOfCard(const QString& card) const;

    /// @brief The name whose name itself is @p text, in any case, or nullptr; a new card of
    ///        that name takes it over with its forms
    [[nodiscard]] const DictionaryName* findNameWithText(const QString& text) const;

    /// @brief The rule with id @p ruleId, or nullptr
    [[nodiscard]] const SpellingRule* findRule(const QString& ruleId) const;

    /// @brief Put a name in the dictionary: in place of the one with its id, or as a new one
    ///        (with a new id when it has none)
    /// @return Its id
    QString putName(DictionaryName name);

    /// @brief Take the name with id @p nameId out; false when there is none
    bool removeName(const QString& nameId);

    /// @brief Put a rule in the dictionary: in place of the one with its id, or as a new one
    ///        (with a new id when it has none)
    /// @return Its id
    QString putRule(SpellingRule rule);

    /// @brief Take the rule with id @p ruleId out; false when there is none
    bool removeRule(const QString& ruleId);

    /// @brief Add a word of the book; false when it is empty or the dictionary has it
    bool addWord(const QString& word);

    /// @brief Take a word of the book out; false when the dictionary does not have it
    bool removeWord(const QString& word);

    /// @brief Whether @p word is a word of the book, as it is written
    [[nodiscard]] bool hasWord(const QString& word) const;

    /// @brief Take everything out
    void clear();

    // =========================================================================
    // File
    // =========================================================================

    /// @brief Read the dictionary from its file, in place of what it had
    ///
    /// A book without the file has an empty dictionary. A file of a later format, or one that
    /// cannot be read, leaves the dictionary empty: it must not be saved over the file.
    /// @param error Gets what is wrong (in English, for the log)
    /// @return Whether the dictionary is as the file has it
    bool load(const QString& path, QString* error = nullptr);

    /// @brief Write the dictionary to its file; an empty one makes no file where there is none
    /// @param error Gets what is wrong (in English, for the log)
    bool save(const QString& path, QString* error = nullptr) const;

    /// @brief The dictionary as its file has it
    [[nodiscard]] QJsonObject toJson() const;

    /// @brief Read the dictionary from what its file has, in place of what it had
    /// @return False, and an empty dictionary, for a later format
    bool fromJson(const QJsonObject& json, QString* error = nullptr);

    // =========================================================================
    // Checking
    // =========================================================================

    /// @brief Whether the dictionary takes @p word for right: a word of a form of a name as
    ///        it is written, a word of the book, or a word of the right spelling of a rule
    ///        (also with a capital letter, at the start of a sentence); also with an ending
    ///        after an apostrophe (Anna's)
    [[nodiscard]] bool isRight(const QString& word) const;

    /// @brief The word of a form of a name that @p word is written in another case ("anna"
    ///        -> "Anna"); empty when it is none
    [[nodiscard]] QString nameSpelling(const QString& word) const;

    /// @brief The words of @p text (a paragraph) that go against the rules, in their order
    ///
    /// The case of the letters counts. A word at the start of a sentence may have a capital
    /// letter: "Okej" there is not against the rule "okej", and "Email" there is against the
    /// rule "e-mail", with "E-mail" for it.
    [[nodiscard]] QList<RuleDeviation> deviations(const QString& text) const;

    // =========================================================================
    // Search
    // =========================================================================

    /// @brief Every form of the name or other name that @p text is a form of, in any case
    ///        ("anna" -> "Anna", "Anny", "Annie"...); empty when it is none
    [[nodiscard]] QStringList formsOf(const QString& text) const;

private:
    /// @brief Make the lookups of the checking anew from the content
    void rebuild();

    /// @brief Whether the dictionary takes @p word for right, without an ending after an
    ///        apostrophe
    [[nodiscard]] bool isRightWord(const QString& word) const;

    /// @brief Add the words of @p form to the words of the names
    void addNameWords(const QString& form);

    /// @brief Add the words of the right spellings of @p rule and its wrong spellings
    void addRuleSpellings(const SpellingRule& rule);

    QList<DictionaryName> m_names;  ///< The names
    QList<SpellingRule> m_rules;    ///< The rules
    QStringList m_words;            ///< The words of the book, sorted

    QSet<QString> m_nameWords;                     ///< Words of the forms of the names
    QHash<QString, QString> m_nameWordCase;        ///< A word of the names in lower case -> it
    QSet<QString> m_rightWords;                    ///< Words of the right forms of the rules
    QSet<QString> m_bookWords;                     ///< The words of the book
    QHash<QString, QList<WrongSpelling>> m_wrong;  ///< First word -> the wrong spellings
};

// =============================================================================
// Making names and rules
// =============================================================================

/// @brief A new name: the name itself with the models and forms the program gives it, and
///        for a character of several words the first name and the surname alone
[[nodiscard]] DictionaryName makeName(const QString& text, NameKind kind,
                                      const NameDeclension& declension,
                                      const NameDeclension::DictionaryForms& dictionary = {});

/// @brief Give a variant the forms of its models anew, but in the cells the author wrote;
///        without a model for each word, the ones the program picks
void refreshForms(NameVariant& variant, NameKind kind, const NameDeclension& declension,
                  const NameDeclension::DictionaryForms& dictionary = {});

/// @brief The wrong spellings a rule usually has besides those the author gives: the other
///        case of a capital letter ("Smok": "smok"), without the hyphen and apart ("e-mail":
///        "email", "e mail"), together and with a hyphen ("na pewno": "napewno", "na-pewno"),
///        and for a word made of two words, with a hyphen and apart ("półelf": "pół-elf",
///        "pół elf")
/// @param isWord Whether a word is right (the spelling dictionary); without it, a word is
///        not split into two
[[nodiscard]] QStringList suggestedWrongSpellings(
    const QString& right, const std::function<bool(const QString&)>& isWord = {});

/// @brief Give a rule its forms: each wrong spelling with its right one, and, when the rule
///        covers forms, each of their forms with the same form of the right spelling
///
/// The forms come from the spelling dictionary: those of the right spelling, else of a wrong
/// one, else of the longest word the right spelling ends with ("półelf" from "elf", which
/// becomes the rule's pattern). A word the dictionary does not know has no other forms.
void fillRuleForms(SpellingRule& rule, const AffixExpander* dictionary);

}  // namespace kalahari::core

#endif  // KALAHARI_CORE_BOOK_DICTIONARY_H
