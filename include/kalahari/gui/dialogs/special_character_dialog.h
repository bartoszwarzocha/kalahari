/// @file special_character_dialog.h
/// @brief Insert > Special Character...: the characters by group or in a list with their names,
///        found by a name or a code, inserted in the text at the cursor

#pragma once

#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/special_characters.h"
#include "kalahari/gui/widgets/character_grid.h"

#include <QElapsedTimer>
#include <QList>
#include <QString>

#include <functional>

class QAbstractItemView;
class QAction;
class QButtonGroup;
class QCheckBox;
class QEvent;
class QLabel;
class QLayout;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QMenu;
class QPoint;
class QPushButton;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief Where the window inserts: the text at the cursor of the chapter in front
struct CharacterTarget {
    QString whyNot;      ///< Why the window cannot insert now ("No chapter is open"); empty:
                         ///< it can
    QString fontFamily;  ///< The font of the text at the cursor: the window shows the
                         ///< characters in it; empty: the dialog's font
};

/// @brief Insert > Special Character...: the window of the characters a keyboard does not have
///
/// Two views of the same characters, under one search that finds a character by its name (in
/// the program's language or in English, without the accents) or its code ("U+2014", "2014"),
/// beside the buttons Grid and List that switch them (the window opens in the view shown last,
/// the setting specialCharacters.view):
/// - the grid: the groups at the left (the favorites and the characters used last first),
///   the characters of the group, and the one selected beside them, large, with its name,
///   its code, its keys and its auto-correct; the characters used last under them;
/// - the list: the groups as buttons over a list of the characters with their names, codes,
///   keys and auto-correct, and a star for the favorites.
///
/// Enter or Insert inserts the character selected, as does a double click, and a click on a
/// character used last; the window stays open after it when the writer wants it (the setting
/// specialCharacters.keepOpen). The menu of a character (the right button, the Menu key or
/// Shift+F10) has Insert and the buttons of the character. The
/// characters are shown in the font of the text at the cursor: a character it has not is
/// greyed out, and its details say that another font shows it in the text. Where the window
/// cannot insert (no chapter, a book open read-only) Insert is greyed out, saying why.
///
/// The window does not insert itself: it asks for it (insertRequested()), as it asks for the
/// keys of a character and for a replacement of auto-correct; the favorites it keeps itself.
class SpecialCharacterDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief The two views of the window
    enum class View { Grid, List };

    /// @brief Where the window inserts, asked when the window is shown or comes to the front,
    ///        and by updateTarget()
    using TargetOf = std::function<CharacterTarget()>;

    /// @brief The keys that insert a character, as the system writes them; empty: none
    using KeysOf = std::function<QString(char32_t code)>;

    /// @brief What auto-correct replaces with a character ("(c)", "--"); empty: nothing
    using AutoCorrectOf = std::function<QStringList(char32_t code)>;

    /// @brief Create the window in the view shown last, with the first character of the
    ///        favorites
    /// @param targetOf Where it inserts
    /// @param keysOf The keys of a character
    /// @param autoCorrectOf What auto-correct replaces with a character
    /// @param parent Parent widget (the main window: the window stays in front of it)
    SpecialCharacterDialog(TargetOf targetOf, KeysOf keysOf, AutoCorrectOf autoCorrectOf,
                           QWidget* parent = nullptr);

    /// @brief The view shown
    [[nodiscard]] View view() const { return m_view; }

    /// @brief Show a view: the same character stays selected, with the same search. The window
    ///        opens in it next time.
    void setView(View view);

    /// @brief The character selected (0: none, nothing found)
    [[nodiscard]] char32_t currentCharacter() const { return m_current; }

    /// @brief Select a character; in the grid also its group, unless the search shows it
    void selectCharacter(char32_t code);

    /// @brief Search for a text, as typed in the field of the view
    void setSearchText(const QString& text);

    /// @brief The characters shown now, in their order
    [[nodiscard]] QList<char32_t> shownCharacters() const;

    /// @brief Ask where the window inserts again (Insert, its tooltip, the font)
    void updateTarget();

    /// @brief Show the favorites, the characters used last, the keys and the auto-correct
    ///        again, after a change (a character inserted, keys given)
    void refresh();

    /// @brief Choose a character for another window (the bullet of a list) instead of
    ///        inserting one: Choose names it (insertRequested()) and closes the window
    void setChoosing();

    /// @brief Insert the character selected, and close the window unless it stays open
    void accept() override;

signals:
    /// @brief Insert or Enter: insert a character in the text
    void insertRequested(char32_t code);

    /// @brief Assign Shortcut...: give a character keys
    void shortcutRequested(char32_t code);

    /// @brief Add Replacement...: a text auto-correct replaces with a character
    void replacementRequested(char32_t code);

protected:
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The grid of a view, or the row of the characters used last
    CharacterGrid* newCharacterGrid(const QString& accessibleName, bool oneRow);

    /// @brief The search field, and the buttons of the views beside it
    QLayout* createSearchRow();

    QWidget* createGridView();
    QWidget* createListView();

    /// @brief The menu of a character, with Insert and the buttons of the character
    void createCharacterMenu();

    /// @brief Show the menu of the character at @p position of a view (the character selected,
    ///        for the keys)
    void showCharacterMenu(QAbstractItemView* view, const QPoint& position);

    /// @brief The icons of the buttons of the views, in the colors of the theme
    void updateViewIcons();

    /// @brief The characters of a group of the grid, or of a button of the list ("all",
    ///        "favorites", "recent" or the id of a group)
    [[nodiscard]] QList<char32_t> charactersOf(const QString& group) const;

    /// @brief Fill the grid or the list with the characters of the search or the group
    void fillCharacters();
    void fillList(const QList<char32_t>& characters);

    /// @brief Show the character selected: in the grid or the list, its details, the buttons
    void showCurrent();

    /// @brief A character chosen in a grid or the list
    void onCharacterChosen(char32_t code);

    /// @brief The fonts of the characters and their sizes, from the font of the text
    void updateFonts();

    /// @brief What a cell and a row of the list say of a character: its name and code, and
    ///        that the font has it not
    [[nodiscard]] QString toolTipOf(char32_t code) const;

    /// @brief Move the selection of the view shown by @p step characters (Up and Down in the
    ///        search field)
    void moveSelection(int step);

    /// @brief Make a character a favorite, or not
    void toggleFavorite();

    SpecialCharacters m_characters;
    TargetOf m_targetOf;
    KeysOf m_keysOf;
    AutoCorrectOf m_autoCorrectOf;
    CharacterTarget m_target;

    View m_view = View::Grid;
    char32_t m_current = 0;
    QString m_gridGroup;              ///< The group of the grid
    QString m_listGroup;              ///< The group of the list ("all" at first)
    bool m_filling = false;           ///< Filling a view: its selection is not the writer's
    bool m_choosing = false;          ///< Choosing a character for another window

    CharacterFonts m_fonts;           ///< Of the characters, from the font of the text

    QLineEdit* m_search = nullptr;
    QToolButton* m_gridButton = nullptr;
    QToolButton* m_listButton = nullptr;

    // The menu of a character
    QMenu* m_menu = nullptr;
    QAction* m_insertAction = nullptr;
    QAction* m_favoriteAction = nullptr;
    QAction* m_shortcutAction = nullptr;
    QAction* m_replacementAction = nullptr;

    // The grid view
    QWidget* m_gridView = nullptr;
    QListWidget* m_groups = nullptr;
    CharacterGrid* m_grid = nullptr;
    QLabel* m_glyph = nullptr;
    QLabel* m_name = nullptr;
    QLabel* m_code = nullptr;
    QLabel* m_keys = nullptr;
    QLabel* m_autoCorrect = nullptr;
    QLabel* m_fontNote = nullptr;
    QPushButton* m_favoriteButton = nullptr;
    QPushButton* m_shortcutButton = nullptr;
    QPushButton* m_replacementButton = nullptr;
    CharacterGrid* m_recent = nullptr;
    QElapsedTimer m_recentInserted;  ///< Since a click inserted a character used last
    QLabel* m_noRecent = nullptr;
    QLabel* m_nothingFound = nullptr;

    // The list view
    QWidget* m_listView = nullptr;
    QButtonGroup* m_groupButtons = nullptr;
    QTreeWidget* m_list = nullptr;
    QLabel* m_listNothingFound = nullptr;
    QPushButton* m_listFavoriteButton = nullptr;
    QPushButton* m_listShortcutButton = nullptr;
    QPushButton* m_listReplacementButton = nullptr;

    QCheckBox* m_keepOpen = nullptr;
    QLabel* m_whyNot = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
