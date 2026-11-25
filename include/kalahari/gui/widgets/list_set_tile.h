// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file list_set_tile.h
/// @brief A tile of a set of list formats: the first items of its first three levels and its
///        name (the tab Sets of Bullets and Numbering, the arrows of the buttons Numbering and
///        Bullets)

#ifndef KALAHARI_GUI_WIDGETS_LIST_SET_TILE_H
#define KALAHARI_GUI_WIDGETS_LIST_SET_TILE_H

#include "kalahari/gui/list_sets.h"

#include <QAbstractButton>
#include <QFont>
#include <QList>

#include <functional>

namespace kalahari::gui {

/// @brief The font of the pictures of lists: the serif one of a book
[[nodiscard]] QFont listPictureFont(const QFont& base);

/// @brief The numbers of an item of a level whose levels above have their first numbers, and
///        it the number @p number
[[nodiscard]] editor::ListPath firstListNumbers(const editor::ListDefinition& definition,
                                                int level, int number);

/// @brief A tile of a set: the first item of each of its first three levels, one under the
///        other, and its name
///
/// The arrows go to the tile next to it (setMove()); the context menu has what can be done
/// with the set (setMenu()). With clickOnEnter, Enter and Return click it, as in a menu.
class ListSetTile : public QAbstractButton {
public:
    /// @brief Go from a tile to the next one in the direction of an arrow key
    using Move = std::function<void(const ListSetTile*, int key)>;
    /// @brief The context menu of the set at a place of the screen
    using Menu = std::function<void(const QString& id, const QPoint& globalPos)>;

    /// @param set The set it shows
    /// @param isDefault It is the default set of its kind (F12, Shift+F12): its name says so
    /// @param parent Owner
    ListSetTile(const ListSet& set, bool isDefault, QWidget* parent);

    [[nodiscard]] const ListSet& set() const { return m_set; }

    void setMove(Move move) { m_move = std::move(move); }
    void setMenu(Menu menu) { m_menu = std::move(menu); }
    void setClickOnEnter(bool click) { m_clickOnEnter = click; }

    [[nodiscard]] QSize sizeHint() const override;

    /// @brief The tile of @p tiles nearest to @p from in the direction of an arrow key, as
    ///        their centres in @p area have it; none past the last one
    [[nodiscard]] static ListSetTile* nearest(const QList<ListSetTile*>& tiles,
                                              const ListSetTile* from, int key,
                                              const QWidget* area);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    ListSet m_set;
    bool m_default = false;
    bool m_clickOnEnter = false;
    Move m_move;
    Menu m_menu;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_WIDGETS_LIST_SET_TILE_H
