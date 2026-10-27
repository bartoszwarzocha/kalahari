/// @file word_repetitions.h
/// @brief The repetitions of words: those that come back too close and the most frequent ones
///
/// The panel "Repetitions" (Tools > Word Repetitions) shows the words of a chapter or of the
/// whole book that come back within a few words, and the words used most often. The forms of
/// one word count as one word ("powiedział", "powiedziała") when the spelling dictionary of the
/// book's language knows their base form (chooseBaseForm()). Short and very common words
/// ("się", "nie", "the") do not count.

#pragma once

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>
#include <utility>

namespace kalahari::editor {

/// @brief The theme's color of the repeated words in the text (the "editor" part of a theme;
///        Settings > Editor > Repetitions)
constexpr const char* REPETITION_COLOR_KEY = "repetition";

/// @brief What counts as a repetition (Settings > Editor > Repetitions)
struct RepetitionOptions {
    int distance = 30;        ///< A word that comes back within so many words repeats
    int minLength = 3;        ///< Shorter words do not count
    bool mergeForms = true;   ///< The forms of a word count as one word (by its base form)
    bool skipCommon = true;   ///< The very common words of the language do not count
    QSet<QString> skipped;    ///< Words the writer skips, in lower case (forms or base forms)
    QString language;         ///< The language of the text: "pl", "en_US"... (commonWords())

    bool operator==(const RepetitionOptions&) const = default;
};

/// @brief A word of the texts analysed, one of those that count
struct TextWord {
    int text = 0;        ///< Which of the texts it is in
    int paragraph = 0;   ///< Its paragraph in the text
    int start = 0;       ///< Where it starts in the paragraph
    int length = 0;      ///< How long it is
    int index = 0;       ///< Its place among all the words of its text, counted ones or not
    QString form;        ///< As written, in lower case: "powiedziała"
    QString key;         ///< Its base form in lower case ("powiedzieć"), or its form

    bool operator==(const TextWord&) const = default;
};

/// @brief A word that comes back within the distance: its occurrences one after another, each
///        within the distance of the one before
struct RepetitionCluster {
    QString key;          ///< The base form shared by its words
    QStringList forms;    ///< The forms of its words, each once, in the order of the text
    QList<int> words;     ///< Its words (RepetitionAnalysis::words), in the order of the text
    int span = 0;         ///< How many words it takes in the text, from its first to its last

    bool operator==(const RepetitionCluster&) const = default;
};

/// @brief How often a word is used
struct WordCount {
    QString key;                             ///< The base form in lower case
    int count = 0;                           ///< Its words in the texts
    QList<std::pair<QString, int>> forms;    ///< Its forms with their counts, the most frequent first
    QList<int> words;                        ///< Its words (RepetitionAnalysis::words)

    bool operator==(const WordCount&) const = default;
};

/// @brief The repetitions of the words of some texts
struct RepetitionAnalysis {
    QList<TextWord> words;                 ///< The words that count, in the order of the texts
    int totalWords = 0;                    ///< All the words, also those that do not count
    QList<RepetitionCluster> clusters;     ///< In the order of the texts
    QList<WordCount> counts;               ///< The most frequent first; a tie in the order of the key
};

/// @brief The base form in lower case of a word as written, or empty when it has none
using BaseFormOf = std::function<QString(const QString& word)>;

/// @brief The words of @p texts (each a list of paragraphs) that repeat
/// @param baseForm The base form of a word, when options.mergeForms is on; empty: none
///
/// A word is a run of letters, also joined by an apostrophe or a hyphen between letters
/// (e-mail, don't). Words of different texts (chapters) are never too close.
RepetitionAnalysis analyzeRepetitions(const QList<QStringList>& texts,
                                      const RepetitionOptions& options,
                                      const BaseFormOf& baseForm = {});

/// @brief The base form of @p word in lower case, from the stems the spelling dictionary gives
///        for it (SpellCheckService::stems()); the word in lower case when none fits
///
/// A Polish verb takes its infinitive ("była" -> "być"); otherwise the shortest stem that starts
/// with at least the first three letters of the word ("walking" -> "walk", "Anny" -> "anna"),
/// so that a stem of another word ("idzie" -> "id") does not join words that are not one.
QString chooseBaseForm(const QString& word, const QStringList& stems, const QString& language);

/// @brief The text a card of a repetition shows: its words with some text around them
struct RepetitionContext {
    QString text;                       ///< The text, with "…" where it is shortened
    QList<std::pair<int, int>> marks;   ///< Where the words are in it: start, length

    bool operator==(const RepetitionContext&) const = default;
};

/// @brief The text around some words of one text, for the card of their repetition: a few
///        words before the first and after the last; the text between two words far apart, or
///        in different paragraphs, is shortened to "…"
/// @param paragraphs The paragraphs of the text
/// @param words The words, in the order of the text
RepetitionContext repetitionContext(const QStringList& paragraphs, const QList<TextWord>& words);

/// @brief The very common words of a language ("pl", "en", "pl_PL"...), in lower case; empty
///        for a language without a list
const QSet<QString>& commonWords(const QString& language);

}  // namespace kalahari::editor
