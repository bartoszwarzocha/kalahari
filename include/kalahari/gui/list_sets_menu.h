// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file list_sets_menu.h
/// @brief Format > List Sets and the arrows of the buttons Numbering and Bullets: the sets of
///        list formats (list_sets.h) given to the selected paragraphs

#ifndef KALAHARI_GUI_LIST_SETS_MENU_H
#define KALAHARI_GUI_LIST_SETS_MENU_H

#include <QList>
#include <QMenu>
#include <QString>

#include <functional>

class QWidgetAction;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::editor {
class BookEditor;
}  // namespace kalahari::editor

namespace kalahari::gui {

class CommandRegistry;
class ListSetTile;
struct ListSet;

/// @brief The sets of list formats, to give one to the selected paragraphs
///
/// The menu is made anew each time it opens: the sets of the program, of the book of the text
/// in front and of the writer's program; the set of the list at the cursor is checked, the
/// default set of each kind (F12, Shift+F12) is marked. A set chosen goes on the lists of the
/// selected paragraphs, and the paragraphs outside a list become one (as the window Bullets
/// and Numbering does). In a text that cannot be changed the sets are greyed out.
class ListSetsMenu : public QMenu {
    Q_OBJECT

public:
    /// @brief What the menu shows
    enum class Kind {
        All,       ///< Format > List Sets: every set, in its group, as an item of the menu
        Numbered,  ///< The arrow of Numbering: the tiles of the numbered and multilevel sets,
                   ///< Restart at, Continue Numbering and Bullets and Numbering...
        Bulleted,  ///< The arrow of Bullets: the tiles of the bulleted sets and Bullets and
                   ///< Numbering...
    };

    /// @brief The text in front (nullptr: none)
    using EditorInFront = std::function<editor::BookEditor*()>;
    /// @brief The book whose sets the text in front has (nullptr: a text outside a book)
    using BookInFront = std::function<const core::ProjectManager*()>;

    /// @param kind What it shows
    /// @param commands The commands of its window (Bullets and Numbering...)
    /// @param editorInFront The text the sets go on
    /// @param bookInFront The book of the text
    /// @param parent Owner
    ListSetsMenu(Kind kind, CommandRegistry& commands, EditorInFront editorInFront,
                 BookInFront bookInFront, QWidget* parent);

    [[nodiscard]] Kind kind() const { return m_kind; }

    /// @brief Make the items anew (as it opens)
    void rebuild();

    /// @brief The items of the sets (Kind::All), as the menu shows them now
    [[nodiscard]] const QList<QAction*>& setActions() const { return m_setActions; }

    /// @brief The tiles of the sets (Kind::Numbered and Bulleted), as the menu shows them now
    [[nodiscard]] const QList<ListSetTile*>& tiles() const { return m_tiles; }

    /// @brief The id of the set checked: of the list at the cursor; empty: none
    [[nodiscard]] QString checkedSet() const;

    /// @brief Give the set to the selected paragraphs of the text in front
    /// @return false without such a set, without a text or in one that cannot be changed
    bool chooseSet(const QString& id);

signals:
    /// @brief A set went on the text
    void setChosen(const QString& id);

protected:
    void showEvent(QShowEvent* event) override;

private:
    /// @brief The tiles of the sets of the kind of the menu, in a grid
    void addTiles(const QList<ListSet>& sets, const QString& checked, bool enabled);

    /// @brief Restart at and Continue Numbering, for the item at the cursor
    void addNumberingItems(const editor::BookEditor* text, bool enabled);

    /// @brief The keys go to the checked tile, or the first one
    void focusTile();

    /// @brief From a tile in the direction of an arrow: the next tile, or the item under them
    void moveFrom(const ListSetTile* tile, int key);

    Kind m_kind;
    CommandRegistry& m_commands;
    EditorInFront m_editorInFront;
    BookInFront m_bookInFront;
    QList<QAction*> m_setActions;
    QList<ListSetTile*> m_tiles;
    QWidgetAction* m_tilesAction = nullptr;  ///< The grid of the tiles; none in Kind::All
    QWidget* m_tilesGrid = nullptr;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_LIST_SETS_MENU_H
