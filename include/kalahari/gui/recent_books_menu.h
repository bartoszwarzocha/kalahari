/// @file recent_books_menu.h
/// @brief File > Recent Books submenu

#pragma once

#include <QMenu>
#include <QString>

namespace kalahari {
namespace gui {

/// @brief The File > Recent Books submenu
///
/// Lists the books from core::RecentBooksManager, numbered 1-9 and 0 for the keyboard,
/// and rebuilds itself whenever the list changes. Choosing a book emits bookChosen();
/// "Clear Recent Files" empties the list.
class RecentBooksMenu : public QMenu {
    Q_OBJECT

public:
    /// @brief Create the submenu with the current list
    /// @param parent Parent widget (usually the File menu)
    explicit RecentBooksMenu(QWidget* parent = nullptr);

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
};

} // namespace gui
} // namespace kalahari
