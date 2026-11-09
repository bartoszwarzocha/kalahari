/// @file text_diff.h
/// @brief Differences between two texts, paragraph by paragraph and word by word
///
/// The comparison of a version of a text with the text as it is now (ChapterVersions): the
/// paragraphs of both texts in one list, the paragraphs that only one text has whole, and in a
/// paragraph that both have in another form the words deleted and the words added.

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace kalahari {
namespace core {

/// @brief A piece of a paragraph of the comparison
struct DiffPiece {
    /// @brief Which text has it
    enum class Kind {
        Same,     ///< Both texts
        Deleted,  ///< Only the older text
        Added     ///< Only the newer text
    };

    Kind kind = Kind::Same;  ///< Which text has it
    QString text;            ///< Its text

    bool operator==(const DiffPiece& other) const = default;
};

/// @brief A paragraph of the comparison
struct DiffParagraph {
    QList<DiffPiece> pieces;  ///< Its text in pieces, in order; an empty paragraph has none
    int older = -1;           ///< Its place in the older text; -1: only the newer has it
    int newer = -1;           ///< Its place in the newer text; -1: only the older has it

    /// @brief Whether both texts have it as it is
    bool isSame() const;

    bool operator==(const DiffParagraph& other) const = default;
};

/// @brief A change: deleted and added pieces next to each other, from the first one
struct DiffChange {
    int paragraph = 0;  ///< The paragraph of the comparison where it starts
    int piece = 0;      ///< Its first piece in that paragraph

    bool operator==(const DiffChange& other) const = default;
};

/// @brief The comparison of two texts
struct TextDiff {
    QList<DiffParagraph> paragraphs;  ///< The paragraphs of both texts, in reading order
    QList<DiffChange> changes;        ///< The changes, in reading order
    int deletedWords = 0;             ///< Words only the older text has (tallyWords())
    int addedWords = 0;               ///< Words only the newer text has (tallyWords())

    /// @brief Whether the texts are the same
    bool isSame() const { return changes.isEmpty(); }
};

/// @brief Compare text @p older with text @p newer, both as their paragraphs
///
/// The paragraphs that both texts have as they are, in the same order, stay; between them, a
/// paragraph of the older text and one of the newer text that share most of their words are
/// one paragraph of the comparison, with the words deleted and added, and the others are
/// deleted or added whole. In a paragraph the pieces are words, the spaces between them and
/// the characters between them (punctuation); deleted pieces come before the added pieces that
/// take their place. A text too long to be compared piece by piece in reasonable time is
/// compared by its paragraphs only.
TextDiff diffTexts(const QStringList& older, const QStringList& newer);

}  // namespace core
}  // namespace kalahari
