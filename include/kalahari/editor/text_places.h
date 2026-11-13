/// @file text_places.h
/// @brief The places of a text Edit > Go To... goes to: the pages, the headings, the notes, the
///        images, the tables and the annotations

#pragma once

#include <QString>

#include <array>

namespace kalahari::editor {

/// @brief The kinds of places of Edit > Go To..., in the order of its list
enum class PlaceKind {
    Page,        ///< The pages of the Page Layout view
    Heading,     ///< The paragraphs of the headings (Heading 1-3)
    Note,        ///< The footnotes and the endnotes, by the numbers they show
    Image,       ///< The images
    Table,       ///< The tables
    Annotation,  ///< The annotations: comments, to-dos and notes
};

/// @brief All the kinds, in the order of the list of Go To
inline constexpr std::array<PlaceKind, 6> PLACE_KINDS = {
    PlaceKind::Page,  PlaceKind::Heading, PlaceKind::Note,
    PlaceKind::Image, PlaceKind::Table,   PlaceKind::Annotation};

/// @brief What a step of Go To did
enum class GoToResult {
    Done,       ///< The cursor is at the place
    NoPlaces,   ///< The text has no place of the kind (no pages but in the Page Layout view)
    NoFurther,  ///< Next: no place after the cursor
    NoEarlier,  ///< Previous: no place before the cursor
    NoNumber,   ///< No place has the number, or it is further on or back than the places go
    Invalid,    ///< Not a number ("abc", "0")
};

/// @brief The outcome of a step of Go To
struct GoToOutcome {
    GoToResult result = GoToResult::Done;
    int ordinal = 0;  ///< Done: the place's one among the places of its kind, from 1 (a page:
                      ///< its number)
    int count = 0;    ///< The places of the kind in the text (the pages)
    QString number;   ///< Done, a note: the number it shows

    bool operator==(const GoToOutcome&) const = default;
};

/// @brief A number of Go To read: a place's own ("5") or places on or back ("+2", "-1")
struct PlaceNumber {
    bool valid = false;     ///< Whether it is a number of one of these kinds
    bool relative = false;  ///< "+2", "-1": places on (more than 0) or back (less than 0)
    int value = 0;          ///< The number (relative: with its sign)
};

/// @brief Read a number of Go To: digits ("5", from 1), or "+" or "-" and digits ("+2", "-1",
///        not 0), with spaces around them
PlaceNumber placeNumberOf(const QString& text);

}  // namespace kalahari::editor
