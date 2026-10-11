/// @file new_file_dialog.h
/// @brief New File: a new element of the open book, of the kind, with the title and at the
/// place the writer chooses

#pragma once

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/gui/widgets/element_place_picker.h"

#include <QDialog>
#include <QList>
#include <QString>
#include <QStringList>

class QEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QWidget;

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief A kind that the New File window offers in one part of the book: its tile, and what
/// the window says of it
struct NewFileKind {
    core::KindRef kind;                               ///< The kind
    core::BookPlace section = core::BookPlace::Main;  ///< The part of the book of its tile
    /// Why no element of the kind can be made now ("The book already has a title page."), the
    /// tooltip of its greyed tile; empty: one can
    QString unavailable;
    QString title;               ///< Title of a new element of the kind: "Chapter 4"
    core::ElementPlace place{};  ///< Place of a new element when the writer does not choose it
    QString where;  ///< Where its elements can be: "in the main section and in parts"
    QString count;  ///< How many the book can have: "any number", "one title page in the book"
    QString file;   ///< The file of a new element: "chapter_004.kchapter in the folder "book""
    QString type;   ///< The type of book the kind belongs to: "Novel"
};

/// @brief New File: a new element of the open book, which opens in the editor
///
/// The kinds of the book are tiles in groups of the parts of the book (front, main and back),
/// named as the Navigator names them; the groups of a book without sections are named after
/// their place in the book ("Content"). At the left the window describes the chosen kind:
/// where its elements can be, how many the book can have, the file a new one gets and the
/// type of book the kind belongs to. Under the tiles are the title of the new element and its
/// place in the list of the part of the book of its tile.
///
/// A kind of which the book has as many elements as it can (a title page), and a kind that
/// opens in a window the program does not have yet (a table of contents), have greyed tiles,
/// whose tooltips say why. A group (a part) is no file, so it has no tile: the Navigator adds
/// it.
///
/// The title starts as the title of the chosen kind ("Chapter 4"), selected, so typing
/// replaces it; it follows the kind until the writer changes it. The dialog cannot be
/// accepted without a kind, a title or a place; a double click on a tile accepts it.
class NewFileDialog : public QDialog {
    Q_OBJECT

public:
    /// @brief The kinds the window offers and the parts of the book
    struct Book {
        QList<NewFileKind> kinds;       ///< The tiles, by parts of the book in reading order
        QList<PickerSection> sections;  ///< The parts of the book, for the list of places
        /// Name of the group of the tiles of each part of @c sections: its name in the
        /// Navigator, or in a book without sections its place in the book ("Content")
        QStringList groupNames;
        int current = -1;  ///< Index in @c kinds of the kind chosen at the start; -1: none
    };

    /// @brief The kinds and the parts of the book open in core::ProjectManager; at the start
    /// the main text kind of the book in its main part is chosen, else the first kind of which
    /// a new element can be made
    static Book openBook();

    /// @brief Create the dialog
    /// @param book The kinds and the parts of the book
    /// @param registry Kinds of the elements; it must outlive the dialog
    /// @param parent Parent widget
    NewFileDialog(const Book& book, const core::BookTypeRegistry& registry,
                  QWidget* parent = nullptr);

    /// @brief The kind of the new element; none when no tile is chosen
    core::KindRef kind() const;

    /// @brief The title of the new element, without spaces around it
    QString title() const;

    /// @brief The place of the new element
    core::ElementPlace place() const;

    /// @brief The tiles of the kinds, with the names of the parts of the book between them
    QListWidget* kindList() const { return m_kindList; }

    /// @brief The title of the new element
    QLineEdit* titleEdit() const { return m_titleEdit; }

    /// @brief The place of the new element in the book
    ElementPlacePicker* picker() const { return m_picker; }

    /// @brief The button that creates the element
    QPushButton* createButton() const { return m_createButton; }

    /// @brief The name of the chosen kind, above its description
    QLabel* nameLabel() const { return m_nameLabel; }

    /// @brief Where the elements of the chosen kind can be
    QLabel* whereLabel() const { return m_whereLabel; }

    /// @brief How many elements of the chosen kind the book can have
    QLabel* countLabel() const { return m_countLabel; }

    /// @brief The file a new element of the chosen kind gets
    QLabel* fileLabel() const { return m_fileLabel; }

    /// @brief The type of book the chosen kind belongs to
    QLabel* typeLabel() const { return m_typeLabel; }

protected:
    /// @brief A new font gives the tiles a new size
    void changeEvent(QEvent* event) override;

private:
    /// @brief The description of the chosen kind, at the left
    QWidget* createDescriptionPanel(QWidget* parent);

    /// @brief The tiles of the kinds
    QWidget* createKindsGroup(QWidget* parent);

    /// @brief The title and the place of the new element
    QWidget* createDetailsGroup(QWidget* parent);

    /// @brief The groups of the parts of the book with their tiles
    void fillKinds();

    /// @brief Give the tiles the size of the longest name at the font of the list, and draw
    /// their icons in the colors of the current icons
    void sizeTiles();

    /// @brief The kind of the chosen tile; nullptr when none is chosen
    const NewFileKind* chosen() const;

    /// @brief A new kind: its description, the title of its element unless the writer has
    /// changed it, and the places in its part of the book
    void onKindChanged();

    /// @brief Draw the icon of the chosen kind in the colors of the current icons
    void updateKindIcon();

    /// @brief The dialog can be accepted only with a kind, a title and a place
    void updateCreateButton();

    Book m_book;
    const core::BookTypeRegistry* m_registry = nullptr;
    bool m_titleChanged = false;  ///< The writer typed the title: it stays as it is

    QLabel* m_iconLabel = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_whereLabel = nullptr;
    QLabel* m_countLabel = nullptr;
    QLabel* m_fileLabel = nullptr;
    QLabel* m_typeLabel = nullptr;
    QListWidget* m_kindList = nullptr;
    QLineEdit* m_titleEdit = nullptr;
    ElementPlacePicker* m_picker = nullptr;
    QPushButton* m_createButton = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
