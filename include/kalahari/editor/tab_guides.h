// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file tab_guides.h
/// @brief The tab guides of a text: vertical dashed lines across its pages that a Tab aligns
///        the text to

#ifndef KALAHARI_EDITOR_TAB_GUIDES_H
#define KALAHARI_EDITOR_TAB_GUIDES_H

#include <vector>

class QTextDocument;

namespace kalahari::editor {

/// @brief The property of a QTextDocument with its tab guides: a QVariantList of doubles, the
///        millimetres from the left edge of the text, in order
inline constexpr const char* TAB_GUIDES_PROPERTY = "kalahariTabGuides";

/// @brief A guide is set and moved in steps of this length (millimetres)
inline constexpr double TAB_GUIDE_STEP_MM = 1.0;

/// @brief The closest two guides may be, and the farthest from the left edge of the text a
///        guide may be (millimetres)
inline constexpr double TAB_GUIDE_MIN_GAP_MM = 2.0;
inline constexpr double TAB_GUIDE_MAX_MM = 500.0;

/// @brief The most guides a text has
inline constexpr int MAX_TAB_GUIDES = 20;

/// @brief What a command of the tab guides did (Format > Align Text)
enum class TabGuideResult {
    Added,     ///< A guide set at the cursor
    Removed,   ///< The guide at the cursor taken away
    Moved,     ///< The guide nearest the cursor moved a step
    Cleared,   ///< All the guides taken away
    NoGuides,  ///< Nothing to move or take away: the text has no guides
    AtEdge,    ///< The cursor at an edge of the text: no guide goes there
    Full,      ///< The text has MAX_TAB_GUIDES already
    Blocked,   ///< The guide cannot go further: another guide or an edge of the text is there
    Refused    ///< A text only read: nothing changes (BookEditor::changeRefused())
};

/// @brief What a command of the tab guides did, and the guide it did it with
struct TabGuideOutcome {
    TabGuideResult result = TabGuideResult::NoGuides;
    double at = 0.0;  ///< Millimetres from the left edge of the text (Added, Removed, Moved,
                      ///< Blocked)
};

/// @brief Guides as a text keeps them: in order, on the steps (TAB_GUIDE_STEP_MM), between 0
///        and TAB_GUIDE_MAX_MM, none closer to the one before it than TAB_GUIDE_MIN_GAP_MM
///        (the later one goes), at most MAX_TAB_GUIDES; a number that is not finite goes
[[nodiscard]] std::vector<double> normalizedTabGuides(std::vector<double> guides);

/// @brief The tab guides of a document (TAB_GUIDES_PROPERTY); none when it has none
[[nodiscard]] std::vector<double> tabGuidesOf(const QTextDocument& document);

/// @brief Give a document its tab guides (normalizedTabGuides()); a KalahariTextDocumentLayout
///        of the document lays its text out with them
void setTabGuidesOf(QTextDocument& document, const std::vector<double>& guides);

}  // namespace kalahari::editor

#endif  // KALAHARI_EDITOR_TAB_GUIDES_H
