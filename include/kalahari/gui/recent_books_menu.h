// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file recent_books_menu.h
/// @brief File > Recent Books submenu

#ifndef KALAHARI_GUI_RECENT_BOOKS_MENU_H
#define KALAHARI_GUI_RECENT_BOOKS_MENU_H

#include <QMenu>
#include <QString>

namespace kalahari::gui {
class CommandRegistry;
}  // namespace kalahari::gui

namespace kalahari::gui {

/// @brief The File > Recent Books submenu
///
/// Lists the books from core::RecentBooksManager, numbered 1-9 and 0 for the keyboard,
/// and rebuilds itself whenever the list changes. Choosing a book emits bookChosen();
/// "Clear Recent Files" empties the list.
class RecentBooksMenu : public QMenu {
    Q_OBJECT

public:
    /// @brief Create the submenu with the current list
    /// @param commands The commands of its window
    /// @param parent Parent widget (usually the File menu)
    explicit RecentBooksMenu(CommandRegistry& commands, QWidget* parent = nullptr);

    /// @brief Put the submenu into a menu, before the Close Book command
    /// @param menu The File menu; the submenu goes last when Close Book is not in it
    void insertInto(QMenu* menu);

signals:
    /// @brief The user chose a book from the list
    /// @param filePath Path of the book's .klh file
    void bookChosen(const QString& filePath);

private:
    /// @brief Rebuild the items from the current list
    void rebuild();
    CommandRegistry& m_commands;  ///< The commands of its window
};

} // namespace kalahari::gui

#endif  // KALAHARI_GUI_RECENT_BOOKS_MENU_H
