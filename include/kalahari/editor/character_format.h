/// @file character_format.h
/// @brief The character formatting of Format > Font and of the color submenus, and the
///        changes of Format > Change Case
///
/// The text keeps it in its QTextCharFormat: the font, size and colors as Qt's properties,
/// the letter spacing in points as KmlPropLetterSpacing (kml_format_registry.h).
#pragma once

#include <QColor>
#include <QFont>
#include <QString>
#include <QTextCharFormat>
#include <optional>

namespace kalahari::editor {

/// @brief The character formatting of text: each part without a value is not the same in
///        all of the text (shown), or stays as it is (given)
struct CharacterFormat {
    std::optional<QString> family;
    std::optional<qreal> size;  ///< Points
    std::optional<bool> bold;
    std::optional<bool> italic;
    std::optional<bool> underline;
    std::optional<bool> strikethrough;
    /// @brief AlignNormal, AlignSuperScript or AlignSubScript
    std::optional<QTextCharFormat::VerticalAlignment> position;
    /// @brief MixedCase, SmallCaps or AllUppercase: how the letters are shown
    std::optional<QFont::Capitalization> capitals;
    std::optional<qreal> letterSpacing;  ///< Points: more than 0 expanded, less condensed
    std::optional<QColor> color;         ///< An invalid color: automatic (the editor's)
    std::optional<QColor> highlight;     ///< An invalid color: none

    bool operator==(const CharacterFormat&) const = default;
};

/// @brief Give a character format the parts of @p changes that have a value
///
/// An automatic color, no highlight and a letter spacing of 0 take the property off.
void mergeCharacterFormat(QTextCharFormat& format, const CharacterFormat& changes);

/// @brief The color of text with a format: invalid for the automatic one
QColor textColorOf(const QTextCharFormat& format);

/// @brief The highlight of text with a format: invalid for none
QColor highlightColorOf(const QTextCharFormat& format);

// =============================================================================
// Colors on dark paper: lighter text and darker highlights, so the text stays readable; the
// saved colors stay as they are
// =============================================================================

/// @brief Text of a paragraph with colors of its own, or of a link (drawn in the link color
///        unless it has a color of its own, and underlined)
struct ColoredText {
    int start = 0;
    int length = 0;
    QColor color;      ///< Invalid: automatic
    QColor highlight;  ///< Invalid: none
    bool link = false; ///< Text of a link (text_link.h)

    bool operator==(const ColoredText&) const = default;
};

/// @brief The contrast text should have with what is under it (WCAG, level AA)
inline constexpr qreal MIN_TEXT_CONTRAST = 4.5;

/// @brief Contrast ratio of two colors (WCAG): from 1 (the same) to 21 (black and white)
qreal contrastRatio(const QColor& first, const QColor& second);

/// @brief Whether a paper is dark (its text is light)
bool isDarkPaper(const QColor& paper);

/// @brief A color of text on dark paper: lighter, in the same hue, until it has the contrast
///        of text with what is under it (the paper or a highlight)
QColor lightenedForDarkPaper(const QColor& color, const QColor& under);

/// @brief A highlight on dark paper: darker, in the same hue, until the text of the paper has
///        the contrast of text with it
QColor darkenedForDarkPaper(const QColor& highlight, const QColor& text);

/// @brief Format > Change Case
enum class CaseChange {
    Sentence,    ///< Sentence case: the first letter of each sentence capital, the others small
    Lower,       ///< lowercase
    Upper,       ///< UPPERCASE
    Capitalize,  ///< Capitalize Each Word: the first letter of each word capital
    Toggle       ///< tOGGLE cASE: each letter the other case
};

/// @brief A text in another case, letter for letter: the text keeps its length, so its
///        formats and annotations stay on the same letters
/// @param text The text of a paragraph
/// @param from Where the changed part starts
/// @param to Where it ends
/// @param change The case
/// @return @p text with the part from @p from to @p to changed
QString changedCase(const QString& text, qsizetype from, qsizetype to, CaseChange change);

}  // namespace kalahari::editor
