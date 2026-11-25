// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file name_declension.h
/// @brief The forms of names in the grammatical cases: models of declension of a language
///
/// The book dictionary keeps every form of the names of a book ("Anna Wolska": "Anny
/// Wolskiej", "Annie Wolskiej"...), so that the checking of spelling takes them all for right
/// and the search finds them. The program suggests the forms and the author corrects them.
/// A model of declension gives the forms of a word that ends like its sample: "Wolska"
/// declines like "Kowalska", an invented "Eldrion" like "Jan". Each word of a name has its
/// model, picked by its ending, by the kind of the name (a woman's surname ending in a
/// consonant does not decline; a place has the accusative of a thing) and, when the spelling
/// dictionary knows the word, by the forms the dictionary gives it, which also confirm the
/// forms of the model.
///
/// Polish has the seven cases and its models; English has only the singular and the plural
/// (its possessive with 's is taken for right by the checking of spelling itself); in another
/// language the author writes the forms.

#ifndef KALAHARI_CORE_NAME_DECLENSION_H
#define KALAHARI_CORE_NAME_DECLENSION_H

#include <QList>
#include <QString>
#include <QStringList>

#include <array>
#include <cstddef>
#include <functional>

namespace kalahari::core {

/// @brief A grammatical case, in the order of Polish grammars
enum class GrammaticalCase {
    Nominative,    ///< mianownik: kto? co?
    Genitive,      ///< dopełniacz: kogo? czego?
    Dative,        ///< celownik: komu? czemu?
    Accusative,    ///< biernik: kogo? co?
    Instrumental,  ///< narzędnik: z kim? z czym?
    Locative,      ///< miejscownik: o kim? o czym?
    Vocative       ///< wołacz: o!
};

/// @brief How many grammatical cases there are
inline constexpr std::size_t CASE_COUNT = 7;

/// @brief What a name names
enum class NameKind {
    Character,  ///< A person or another being of the story
    Place,      ///< A place: a town, a land, a building
    Item,       ///< A thing: a book, a sword, a ship
    Other       ///< Another proper name: a people, an order, an event
};

/// @brief The forms of a name in each case, in the singular and the plural
///
/// A cell has the forms of one case and number, the main one first; it can have several
/// right ones ("Zakopanem", "Zakopanym") or none (an English name has only the nominative).
struct FormTable {
    std::array<QStringList, CASE_COUNT> singular;  ///< The forms in the singular, by case
    std::array<QStringList, CASE_COUNT> plural;    ///< The forms in the plural, by case

    /// @brief The forms of a case in the singular or the plural
    [[nodiscard]] const QStringList& cell(GrammaticalCase grammaticalCase, bool inPlural) const;

    /// @brief The forms of a case in the singular or the plural, to change
    [[nodiscard]] QStringList& cell(GrammaticalCase grammaticalCase, bool inPlural);

    /// @brief Every form, each once: those of the singular, then those of the plural when
    ///        @p withPlural
    [[nodiscard]] QStringList allForms(bool withPlural) const;

    /// @brief Whether no cell has a form
    [[nodiscard]] bool isEmpty() const;

    /// @brief The name of a cell in the book's files: "genitive", "genitivePlural"
    [[nodiscard]] static QString cellKey(GrammaticalCase grammaticalCase, bool inPlural);

    /// @brief The cell named @p key (cellKey()); false when it names none
    [[nodiscard]] static bool cellOfKey(const QString& key, GrammaticalCase& grammaticalCase,
                                        bool& inPlural);

    /// @brief The cases, in their order
    [[nodiscard]] static const std::array<GrammaticalCase, CASE_COUNT>& allCases();

    /// @brief Whether two tables have the same content
    [[nodiscard]] bool operator==(const FormTable& other) const = default;
};

/// @brief The models of declension of a language, and the forms they give names
class NameDeclension {
public:
    /// @brief The forms the spelling dictionary knows that are forms of the same word as
    ///        @p word ("Annie" -> "Anna", "Anny"...); empty when it does not know the word
    using DictionaryForms = std::function<QStringList(const QString& word)>;

    /// @brief The models of a language
    /// @param language Its code ("pl", "en") or a dictionary's name ("pl_PL")
    explicit NameDeclension(const QString& language);

    /// @brief The code of the language ("pl", "en")
    [[nodiscard]] QString language() const;

    /// @brief The cases the forms of a name have in the language: in Polish all seven; in
    ///        others only the nominative (the singular and the plural)
    [[nodiscard]] QList<GrammaticalCase> cases() const;

    /// @brief The ids of the models of the language, the likeliest first; unchanged() last
    ///
    /// Polish: "jan", "marek", "kuba", "kowalski", "anna", "kowalska", "gdansk", "londyn",
    /// "gniezno", "opole", "zakopane" (each declines like its sample word); English:
    /// "regular" (the plural with -s); any language: "none".
    [[nodiscard]] QStringList models() const;

    /// @brief The id of the model of a word that does not change ("none")
    [[nodiscard]] static QString unchanged();

    /// @brief Whether @p model can be given to @p word: the endings it takes off are there
    ///        ("Kowalska" for "Wolska", not for "Jan"); unchanged() can be given to any word,
    ///        and a model the language does not have to none
    [[nodiscard]] bool fits(const QString& word, const QString& model) const;

    /// @brief The words of a name, as the models are given for them (split at spaces)
    [[nodiscard]] static QStringList wordsOf(const QString& name);

    /// @brief The model of each word of a name, in their order
    ///
    /// By the endings of the words and the kind of the name; when the dictionary knows a
    /// word, the model whose forms it knows best. A word after the first that is already a
    /// form of another word ("Popiołów", "Ossów") or comes after a preposition ("nad
    /// Morzem") does not change, and neither does a woman's surname ending in a consonant.
    [[nodiscard]] QStringList modelsFor(const QString& name, NameKind kind,
                                        const DictionaryForms& dictionary = {}) const;

    /// @brief The forms of a word with a model
    ///
    /// When the dictionary knows the word, a form the model is not sure of is the one the
    /// dictionary knows ("Ksiąg", not "Księg"). A model the language does not have gives the
    /// word unchanged.
    [[nodiscard]] FormTable wordForms(const QString& word, const QString& model, NameKind kind,
                                      const DictionaryForms& dictionary = {}) const;

    /// @brief The forms of a name: in each cell the forms of its words in that case
    /// @param models The model of each word (modelsFor()); when they are not as many as the
    ///        words, modelsFor() gives them
    [[nodiscard]] FormTable nameForms(const QString& name, const QStringList& models, NameKind kind,
                                      const DictionaryForms& dictionary = {}) const;

private:
    /// @brief The languages that have models
    enum class Language { Polish, English, Other };

    /// @brief The model of a word of a Polish name
    [[nodiscard]] static QString polishModel(const QString& word, NameKind kind, bool feminine,
                                             bool first, const DictionaryForms& dictionary);

    /// @brief The model of each word of a Polish name (modelsFor())
    [[nodiscard]] static QStringList polishModels(const QStringList& words, NameKind kind,
                                                  const DictionaryForms& dictionary);

    Language m_language = Language::Other;  ///< The language of the models
    QString m_code;                         ///< Its code ("pl", "en")
};

}  // namespace kalahari::core

#endif  // KALAHARI_CORE_NAME_DECLENSION_H
