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
