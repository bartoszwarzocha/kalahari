/// @file version_comparison.h
/// @brief A version of a text compared with the text as it is now, as a text to read: the
/// words that only the version has struck out, the words written later underlined
///
/// The comparison is a text of its own (KML), which a read-only editor shows: the paragraphs
/// of both texts in reading order (core::diffTexts()), each in the look of the text as it is
/// now, or of the version for a paragraph that only the version has. The annotations of the
/// texts are left out of it. Its places (positions in the document an editor makes of its KML)
/// lead to the changes, and from a fragment of it to the same fragment of the version.

#pragma once

#include <QColor>
#include <QList>
#include <QString>

namespace kalahari::editor {

/// @brief The colors of the comparison in the theme files' "editor" sections, kept for each
/// theme (core::ThemeManager::editorColor())
constexpr const char* DELETED_COLOR_KEY = "versionDeleted";  ///< The text only the version has
constexpr const char* ADDED_COLOR_KEY = "versionAdded";      ///< The text written later

/// @brief The colors of the marks of the comparison
struct ComparisonColors {
    QColor deleted;  ///< The text that only the version has, struck out
    QColor added;    ///< The text that only the text as it is now has, underlined
};

/// @brief A range of positions in a document: from start to end, without end
struct ComparisonRange {
    int start = 0;
    int end = 0;

    /// @brief Whether it has no character
    bool isEmpty() const { return end <= start; }

    bool operator==(const ComparisonRange& other) const = default;
};

/// @brief The comparison of a version of a text with the text as it is now
struct VersionComparison {
    /// @brief A piece of the comparison that the version has: struck out or the same in both
    struct OlderPiece {
        int start = 0;  ///< Where it starts in the comparison
        int end = 0;    ///< Where it ends in the comparison
        int older = 0;  ///< Where it starts in the version
    };

    QString kml;                     ///< The comparison as a text
    QList<ComparisonRange> changes;  ///< Its changes in reading order (core::TextDiff::changes)
    int deletedWords = 0;            ///< Words only the version has
    int addedWords = 0;              ///< Words only the text as it is now has
    QList<OlderPiece> olderPieces;   ///< In the order of the comparison
    int olderLength = 0;             ///< Characters of the version, its paragraph ends too

    /// @brief Whether the version has the words of the text as it is now
    bool isSame() const { return changes.isEmpty(); }

    /// @brief The range of the version that range [@p from, @p to) of the comparison shows:
    /// its struck out text and the text that both have, not the text written later
    /// @return Positions in the document an editor makes of the version's KML; empty when the
    ///         range shows nothing of the version
    ComparisonRange olderRange(int from, int to) const;
};

/// @brief Compare @p olderKml, a version of a text, with @p newerKml, the text as it is now
/// @param colors The colors of the marks of the changes
VersionComparison compareVersions(const QString& olderKml, const QString& newerKml,
                                  const ComparisonColors& colors);

/// @brief The text of range [@p from, @p to) of text @p kml, as KML: positions in the document
/// an editor makes of it, a fragment to insert with its formatting and annotations
QString kmlFragment(const QString& kml, int from, int to);

}  // namespace kalahari::editor
