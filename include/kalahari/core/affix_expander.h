// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file affix_expander.h
/// @brief The forms a Hunspell dictionary gives its words: each word with its suffixes
///
/// Hunspell tells whether a word is right, but it does not list the forms of a word. The
/// book dictionary needs them: a rule of spelling covers every form of its word ("smok":
/// "smoka", "smokowi"...), and the forms a model of declension gives a name are checked with
/// the ones the dictionary knows. The expander reads the two files of the dictionary itself:
/// a form is the word of an entry of the .dic file with one of the suffixes its flags give in
/// the .aff file, and with the suffixes those suffixes allow in turn (one level). Prefixes are
/// left out: the forms of a word are its endings, and a prefix makes another word ("nie-").

#ifndef KALAHARI_CORE_AFFIX_EXPANDER_H
#define KALAHARI_CORE_AFFIX_EXPANDER_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <utility>

namespace kalahari::core {

/// @brief The forms of the words of a Hunspell dictionary (an .aff and a .dic file)
///
/// The .dic file stays in memory while the expander lives (a few megabytes for a large
/// dictionary), and the entries of a word are looked up in it when they are asked for, so
/// an expander is made when forms are needed (a dialog of the book dictionary) and dropped
/// after. The files can be in UTF-8 or ISO 8859-1 (the SET line of the .aff file); flags of
/// one character, of two (FLAG long), numbers (FLAG num) and their aliases (AF) are read.
class AffixExpander {
public:
    /// @brief Read a dictionary in place of the one read before
    /// @param affixFile Its .aff file
    /// @param wordsFile Its .dic file
    /// @return Whether both files were read; when not, the expander has no dictionary
    bool load(const QString& affixFile, const QString& wordsFile);

    /// @brief Whether a dictionary is read
    [[nodiscard]] bool isLoaded() const;

    /// @brief Whether the dictionary has an entry of @p word, written as it is
    [[nodiscard]] bool hasEntry(const QString& word) const;

    /// @brief The forms of @p word, an entry of the dictionary written as it is: the word
    ///        itself first, then the word with each of its suffixes, each form once; empty when
    ///        the dictionary has no entry of it
    [[nodiscard]] QStringList formsOf(const QString& word) const;

    /// @brief The entries of the dictionary that @p form is a form of ("smoka" -> "smok";
    ///        "smok" -> "smok"), each once; empty when it is the form of none
    [[nodiscard]] QStringList entriesOf(const QString& form) const;

    /// @brief Every form of every entry that @p form is a form of ("smoka" -> "smok",
    ///        "smoka", "smokowi"...), each once; empty when it is the form of none
    [[nodiscard]] QStringList relatedForms(const QString& form) const;

private:
    /// @brief How the .aff file writes the flags
    enum class FlagType {
        Character,  ///< One character each (the default, and FLAG UTF-8)
        Long,       ///< Two characters each (FLAG long)
        Number      ///< Numbers separated by commas (FLAG num)
    };

    /// @brief A character of the condition of a suffix: any one, one of some, or none of some
    struct ConditionCharacter {
        QString characters;    ///< The characters of a set ([aeo]), or the one character
        bool negated = false;  ///< None of them ([^aeo])
        bool any = false;      ///< Any character (.)
    };

    /// @brief A suffix of a flag: a form drops @p strip from the end of the word and adds
    ///        @p add, when the end of the word matches the condition
    struct Suffix {
        QString strip;                        ///< Taken from the end of the word
        QString add;                          ///< Put at the end in its place
        QList<ConditionCharacter> condition;  ///< What the end of the word must be
        QStringList continuation;             ///< Flags of the suffixes the form can take
    };

    /// @brief An entry of the .dic file: a word and its flags
    struct Entry {
        QString word;       ///< The word
        QStringList flags;  ///< Its flags
    };

    /// @brief Read the .aff file; false when it cannot be read
    bool readAffixFile(const QString& path);

    /// @brief Read one line of the .aff file (its words, split at spaces)
    /// @param aliasesStarted Whether the first AF line (the count of the aliases) is read
    void readAffixLine(const QStringList& fields, bool& aliasesStarted);

    /// @brief Read an SFX line of the .aff file: a suffix (the line that starts the suffixes
    ///        of a flag gives only their count)
    void readSuffix(const QStringList& fields);

    /// @brief The flags of a field of flags (an alias number when the file has AF lines)
    [[nodiscard]] QStringList flagsOf(const QString& text) const;

    /// @brief The entries of @p word in the .dic file
    [[nodiscard]] QList<Entry> entries(const QString& word) const;

    /// @brief Add the forms of @p word with the suffixes of @p flags to @p forms
    /// @param continued Gets each form whose suffix allows further suffixes, with their
    ///        flags; nullptr: those are not wanted
    void addForms(const QString& word, const QStringList& flags, QStringList& forms,
                  QList<std::pair<QString, QStringList>>* continued) const;

    /// @brief Whether the end of @p word matches @p condition
    [[nodiscard]] static bool matches(const QString& word,
                                      const QList<ConditionCharacter>& condition);

    /// @brief The condition of a suffix as the .aff file writes it ("[^aeo]k", ".")
    [[nodiscard]] static QList<ConditionCharacter> parseCondition(const QString& text);

    /// @brief Text of the dictionary's files in their encoding
    [[nodiscard]] QString decode(const QByteArray& bytes) const;

    /// @brief Text in the encoding of the dictionary's files
    [[nodiscard]] QByteArray encode(const QString& text) const;

    QHash<QString, QList<Suffix>> m_suffixes;  ///< Flag -> its suffixes
    QList<QStringList> m_aliases;              ///< AF lines: the flags of alias 1, 2...
    FlagType m_flagType = FlagType::Character;
    QString m_needAffix;       ///< Flag of the entries that are a word only with a suffix
    QString m_forbidden;       ///< Flag of the entries that are wrong words
    QString m_onlyInCompound;  ///< Flag of the entries and suffixes only inside compounds
    bool m_latin1 = false;     ///< The files are in ISO 8859-1, not UTF-8
    QByteArray m_words;        ///< The .dic file, after a line end; empty: none read
};

}  // namespace kalahari::core

#endif  // KALAHARI_CORE_AFFIX_EXPANDER_H
