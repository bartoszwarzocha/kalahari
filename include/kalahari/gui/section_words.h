/// @file section_words.h
/// @brief How the program's sentences name the front, main and back part of a book

#pragma once

#include "kalahari/core/book_type_package.h"

#include <QCoreApplication>
#include <QString>

namespace kalahari {
namespace core {
struct ProjectBook;
} // namespace core

namespace gui {

/// @brief A part of the book (front, main or back) as the program's sentences name it
///
/// The words are in the language of the program and follow the book's set of names, while the
/// Navigator shows the names themselves in the language of the book: "at the end of the main
/// section" in English, "na końcu sekcji głównej" or "na końcu tekstu głównego" in Polish.
/// The writer's own names are quoted after the word for a section: "in the section "Story"".
/// The parts of a book without sections are named after their place in the book: "at the
/// beginning of the book", "in the content of the book", "at the end of the book".
///
/// Each phrase is whole, with its preposition, so that a language can say it its own way;
/// capitalized() starts a sentence or an option with it.
struct SectionWords {
    Q_DECLARE_TR_FUNCTIONS(SectionWords)

public:
    QString inPart;   ///< As in "the last one %1": "in the main section"
    QString atStart;  ///< As in "The prologue goes %1": "at the start of the main section"
    QString atEnd;    ///< As in "The part is added %1": "at the end of the main section"

    /// @brief The words for @p place (Front, Main or Back) of @p book
    /// @param book The book; nullptr: a book with the first set of names
    static SectionWords forPart(const core::ProjectBook* book, core::BookPlace place);

    /// @brief @p phrase with a capital letter, at the start of a sentence or of an option:
    /// "At the end of the main section"
    static QString capitalized(const QString& phrase);
};

} // namespace gui
} // namespace kalahari
