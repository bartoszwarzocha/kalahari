/// @file quick_character_palette.h
/// @brief Insert > Quick Character: the favorite characters and those used last at the cursor
///        of the text, and a search of all of them

#pragma once

#include "kalahari/gui/special_characters.h"
#include "kalahari/gui/widgets/character_grid.h"

#include <QFrame>
#include <QList>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>

#include <functional>

class QEvent;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QShowEvent;

namespace kalahari {
namespace gui {

/// @brief A small window under the cursor of the text, for a character without leaving the
///        keyboard
///
/// With nothing typed it shows a row of the favorite characters and a row of those inserted
/// last; typing in its search finds the characters by a name or a code, as the window
/// Insert > Special Character... does. Up and Down choose the previous and the next character
/// (Left and Right too while nothing is typed), Tab goes to the rows, where the arrows move in
/// them. Enter or a click inserts the character and closes the palette, Esc closes it without
/// inserting, Ctrl+Enter (All Characters...) opens the window of all the characters. Under the
/// palette the character chosen with its name, code, keys and auto-correct.
class QuickCharacterPalette : public QFrame {
    Q_OBJECT

public:
    /// @brief The keys that insert a character, as the system writes them; empty: none
    using KeysOf = std::function<QString(char32_t code)>;

    /// @brief What auto-correct replaces with a character ("(c)", "--"); empty: nothing
    using AutoCorrectOf = std::function<QStringList(char32_t code)>;

    /// @brief How many characters a row of the palette has
    static constexpr int COLUMNS = 12;

    /// @brief How many rows of the characters found it shows
    static constexpr int FOUND_ROWS = 3;

    /// @param fontFamily The font of the text at the cursor: the characters are shown in it
    /// @param keysOf The keys of a character
    /// @param autoCorrectOf What auto-correct replaces with a character
    /// @param parent Parent widget (the main window); the palette is a window of its own
    QuickCharacterPalette(const QString& fontFamily, KeysOf keysOf, AutoCorrectOf autoCorrectOf,
                          QWidget* parent = nullptr);

    /// @brief Show the palette under the cursor of the text, or over it where the screen ends
    ///        below, within the screen
    /// @param cursor The cursor, in the coordinates of the screen
    void showAt(const QRect& cursor);

    /// @brief Where a palette of @p size goes for a cursor: under it, a little to its left;
    ///        over it where @p screen ends below; within @p screen
    [[nodiscard]] static QRect placeFor(const QRect& cursor, const QSize& size,
                                        const QRect& screen);

    /// @brief The character chosen (0: none)
    [[nodiscard]] char32_t currentCharacter() const;

    /// @brief The characters shown, in their order: the favorites and those used last, or
    ///        the characters found
    [[nodiscard]] QList<char32_t> shownCharacters() const { return m_shown; }

    /// @brief Search for a text, as typed in the field
    void setSearchText(const QString& text);

    /// @brief Choose the character @p step characters after the one chosen (before it, for a
    ///        step less than 0)
    void moveSelection(int step);

    /// @brief Insert the character chosen and close
    void insertCurrent();

    /// @brief Open the window of all the characters and close
    void showAllCharacters();

signals:
    /// @brief A character to insert in the text
    void insertRequested(char32_t code);

    /// @brief All Characters... (Ctrl+Enter): the window Insert > Special Character...
    void allCharactersRequested();

protected:
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief A label of a row
    QLabel* newRowLabel(const QString& text);

    /// @brief A row of the characters
    CharacterGrid* newGrid(int rows, const QString& accessibleName);

    /// @brief Fill the rows with the favorites and those used last, or with what was found
    void fill();

    /// @brief Show the character chosen: selected in its row, its name under the rows
    void showCurrent();

    /// @brief What a cell says of its character
    [[nodiscard]] QString toolTipOf(char32_t code) const;

    /// @brief Whether a key is one of the keys of the palette: handled
    bool handleKey(const QKeyEvent* key);

    SpecialCharacters m_characters;
    CharacterFonts m_fonts;
    KeysOf m_keysOf;
    AutoCorrectOf m_autoCorrectOf;

    QList<char32_t> m_shown;   ///< The characters of the rows shown, in their order
    int m_favoriteCount = 0;   ///< Of them, the favorites first (with nothing typed)
    int m_current = -1;        ///< The index of the one chosen in m_shown; -1: none

    QLineEdit* m_search = nullptr;
    QLabel* m_favoritesLabel = nullptr;
    CharacterGrid* m_favorites = nullptr;
    QLabel* m_noFavorites = nullptr;
    QLabel* m_recentLabel = nullptr;
    CharacterGrid* m_recent = nullptr;
    QLabel* m_noRecent = nullptr;
    QLabel* m_foundLabel = nullptr;
    CharacterGrid* m_found = nullptr;
    QLabel* m_nothingFound = nullptr;
    QLabel* m_details = nullptr;
    QPushButton* m_allButton = nullptr;
};

} // namespace gui
} // namespace kalahari
