/// @file special_characters.h
/// @brief The characters of Insert > Special Character...: their groups, their names in the
///        program's language and in English, and finding them by a name or a code

#pragma once

#include <QCoreApplication>
#include <QHash>
#include <QList>
#include <QString>

#include <optional>

namespace kalahari {
namespace gui {

/// @brief A character of the window
struct SpecialCharacter {
    char32_t code = 0;
    QString name;         ///< In the program's language: "Pauza" in Polish
    QString englishName;  ///< "Em dash"
    QString words;        ///< Other words to find it by, in the program's language and in
                          ///< English: "myślnik dialogowy dash dialogue"
    QString mark;         ///< What the window shows for a character that cannot be seen (a
                          ///< space): "°", "NNBSP"; empty for the others
    QString group;        ///< The id of its group; empty for a character of no group

    /// @brief The character as text
    QString text() const { return QString::fromUcs4(&code, 1); }

    bool operator==(const SpecialCharacter&) const = default;
};

/// @brief A group of the window's characters
struct SpecialCharacterGroup {
    QString id;                   ///< "punctuation", "spaces", "accents", "greek", "arrows",
                                  ///< "math", "currencies", "fractions", "ornaments"
    QString name;                 ///< In the program's language
    QList<char32_t> characters;   ///< In the window's order

    bool operator==(const SpecialCharacterGroup&) const = default;
};

/// @brief The characters of the window, named in the program's language when it is made
///
/// Letters with accents are named from their letter and their mark ("a with ogonek"); the
/// others have names of their own. Any other character can be found by its code.
class SpecialCharacters {
    Q_DECLARE_TR_FUNCTIONS(kalahari::gui::SpecialCharacters)

public:
    SpecialCharacters();

    /// @brief The groups, in the window's order
    const QList<SpecialCharacterGroup>& groups() const { return m_groups; }

    /// @brief A character of the window, or any other one, named by its code ("Character
    ///        U+2603"), without a group
    SpecialCharacter character(char32_t code) const;

    /// @brief Whether a character is one of the window's groups
    bool contains(char32_t code) const { return m_characters.contains(code); }

    /// @brief The characters a text finds, in the order of the groups
    ///
    /// Each word of the text is in the name, the English name or the other words of the
    /// character, in any case and without the accents ("polpauza" finds "Półpauza"). A code
    /// ("2014", "U+2014") finds its character first, also one that is in no group; so does a
    /// character of the groups typed itself ("—").
    QList<char32_t> find(const QString& text) const;

    /// @brief The code of a character as the window shows it: "U+2014"
    static QString codeText(char32_t code);

    /// @brief The character a code gives: "U+2014", "u+2014", "0x2014", or "2014" (without
    ///        "U+" four hexadecimal digits or more, one of them decimal); none for another
    ///        text, or a code no character can be inserted for (beyond U+10FFFF, a surrogate,
    ///        a control character, a code Unicode does not assign)
    static std::optional<char32_t> codeOf(const QString& text);

private:
    /// @brief Add a character to the last group
    void add(SpecialCharacter character);

    QList<SpecialCharacterGroup> m_groups;
    QHash<char32_t, SpecialCharacter> m_characters;
    QHash<char32_t, QString> m_searchTexts;  ///< The names and words of each, folded for
                                             ///< finding (no case, no accents)
};

} // namespace gui
} // namespace kalahari
