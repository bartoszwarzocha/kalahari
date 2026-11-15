// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file name_declension_internal.h
/// @brief What the source files of NameDeclension share (not part of its API): the cells a
///        Polish model gives a word, and the models

#ifndef KALAHARI_CORE_NAME_DECLENSION_INTERNAL_H
#define KALAHARI_CORE_NAME_DECLENSION_INTERNAL_H

#include <kalahari/core/name_declension.h>

#include <QChar>
#include <QString>
#include <QStringList>

#include <array>
#include <cstddef>

namespace kalahari::core::declension {

/// @brief The forms a model gives one case: those it is sure of, and others that may be right
///        in their place, which count only when the dictionary knows them
struct Cell {
    QStringList forms;    ///< Right as far as the model can tell, the main first
    QStringList guesses;  ///< Likely, but only the dictionary can tell (Ksiąg for Księg)
};

/// @brief The cells of one number
using Cells = std::array<Cell, CASE_COUNT>;

/// @brief The cells a model gives a word
struct Candidates {
    Cells singular;  ///< By case
    Cells plural;    ///< By case
};

/// @brief Where a case is in the arrays
constexpr std::size_t at(GrammaticalCase grammaticalCase) {
    return static_cast<std::size_t>(grammaticalCase);
}

/// @brief The models of Polish
enum class PolishModel {
    Jan,       ///< A man's name ending in a consonant: Jan, Piotr, Eldrion
    Marek,     ///< ... whose e drops: Marek -> Marka, Paweł -> Pawła
    Kuba,      ///< A man's name ending in -a or -o: Kuba, Fredro
    Kowalski,  ///< A masculine adjective: Kowalski, Jerzy, Czarny (Las)
    Anna,      ///< A feminine noun ending in -a: Anna, Kasia, Zofia, Olga, Wieża
    Kowalska,  ///< A feminine adjective: Kowalska, Nowakowa, Biała (Wieża)
    Gdansk,    ///< A masculine thing ending in a consonant, genitive -a: Gdańsk, Kraków
    Londyn,    ///< ... genitive -u: Londyn
    Gniezno,   ///< A neuter noun ending in -o: Gniezno, Kłodzko
    Opole,     ///< A neuter noun ending in -e: Opole, Zabrze
    Zakopane,  ///< A neuter adjective: Zakopane, Złote (Wzgórze)
    None       ///< The word does not change
};

/// @brief How many Polish models change the word (all but None)
inline constexpr std::size_t POLISH_MODEL_COUNT = static_cast<std::size_t>(PolishModel::None);

/// @brief Whether @p character is a Polish vowel
[[nodiscard]] bool isVowel(QChar character);

/// @brief Whether @p text ends with one of @p endings
[[nodiscard]] bool endsWithAny(const QString& text, const QStringList& endings);

/// @brief Every cell of both numbers with @p form
[[nodiscard]] Candidates unchangedCandidates(const QString& form);

/// @brief The ids of the Polish models, in the order of PolishModel
[[nodiscard]] const QStringList& polishModelIds();

/// @brief The model of an id, or None
[[nodiscard]] PolishModel polishModelOf(const QString& model);

/// @brief The cells a Polish model gives a word
[[nodiscard]] Candidates polishCandidates(const QString& word, PolishModel model, NameKind kind);

/// @brief Whether a model can be given to @p word: the endings it takes off are there
[[nodiscard]] bool fitsWord(const QString& word, PolishModel model);

}  // namespace kalahari::core::declension

#endif  // KALAHARI_CORE_NAME_DECLENSION_INTERNAL_H
