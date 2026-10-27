/// @file new_file_dialog.h
/// @brief New File: a new element of the open book, of the kind, with the title and at the
/// place the writer chooses, or an empty text file, outside the book or with it; and Add
/// Element, the same window for the place of the Navigator or of the editor it was opened on

#pragma once

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/gui/add_target.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/widgets/element_place_picker.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

class QCheckBox;
class QComboBox;
class QEvent;
class QLabel;
class QLayout;
class QLineEdit;
class QListWidget;
class QPushButton;
class QRadioButton;
class QShowEvent;
class QStackedWidget;
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
    /// Where a new element goes when the window shows no list of places (the Workshop, whose
    /// elements are in alphabetical order); empty: the list shows the place
    QString placeNote;
};

/// @brief New File: a new element of the open book, which opens in the editor, or an empty
/// text file
///
/// One of the program's own dialogs: its heading says what the window makes, and Create and
/// Cancel are at the bottom. The choices go from the top down: the kind, the title, the place.
/// The kinds of the book are tiles in groups of the parts of the book (front, main and back),
/// named as the Navigator names them; the groups of a book without sections are named after
/// their place in the book ("Content"). Under the tiles is the title of the new element, and under it its place in the
/// list of the part of the book of its tile. At the left, along the whole height, the window
/// describes the chosen kind: where its elements can be, how many the book can have, the file
/// a new one gets and the type of book the kind belongs to.
///
/// A kind of which the book has as many elements as it can (a title page), and a kind that
/// opens in a window the program does not have yet (a table of contents), have greyed tiles,
/// whose tooltips say why. A group (a part) is no file, so it has no tile: the Navigator adds
/// it.
///
/// The last group, Other Files, has the tile of an empty text file, which is no element of
/// the text of the book. Its place is outside the book at first: an empty file in a tab of its
/// own, as New File without a book makes it. With the book it is one of the files of the book
/// outside its text (core::ProjectBook::files), in "Other Files" in the Navigator, which the
/// finished book does not have. It has no place in the list of the book: a note says where
/// the file goes instead.
///
/// The title starts as the title of the chosen kind ("Chapter 4"), selected, so typing
/// replaces it, and a click in it selects it while it is the proposed one; it follows the kind
/// until the writer changes it. The dialog cannot be accepted without a kind, a title or a
/// place; a double click on a tile accepts it.
///
/// Add Element (forAdding()) is the same window for the place of the Navigator or of the
/// editor it was opened on: it offers the kinds of that place only, groups (a part) among
/// them, and has no empty text file. A text element of the book gets its status, Draft at
/// first. A new group can take inside it the elements that close the main section at the end
/// of the groups before it (the epilogue), and the list only shows the book as it will be.
/// The kinds of the Workshop are never offered with those of the book; its elements are in
/// alphabetical order, so a note says where a new one goes in place of the list. A material
/// is a copy of a file the writer chooses: choosing its tile opens the system's window of
/// files, and its title starts as the name of the file.
///
/// More Kinds... (setMoreKinds()), at the left of the buttons, opens Project Properties: a
/// kind checked there has its tile at once, in the group of its section, and the window keeps
/// the chosen kind, its title and its place. The gear of the heading of Add Element opens the
/// settings of the Navigator, whose menus open the window.
class NewFileDialog : public KalahariDialog {
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
        /// The file of an empty text file with the book: "file_001.kchapter in the folder
        /// "book""; empty: an empty text file cannot go with the book
        QString bookFile;
        /// Title of an empty text file ("Untitled 2"), as New File without a book names it;
        /// empty: the window has no tile of an empty text file
        QString untitled;
        /// The window adds an element to the place it was opened on (Add Element): it is
        /// named so, and has the status of a text element of the book
        bool adding = false;
        /// The main text kind of the book, for the words of a new group: "the chapters added
        /// to the new part"
        core::KindRef mainKind;
    };

    /// @brief The kinds and the parts of the book open in core::ProjectManager; at the start
    /// the main text kind of the book in its main part is chosen, else the first kind of which
    /// a new element can be made
    /// @param untitled Title of an empty text file; empty: no empty text file
    static Book openBook(const QString& untitled = QString());

    /// @brief The kinds of @p target, the place of the book or of the Workshop that Add
    /// Element was opened on, in core::ProjectManager; at the start @p target's kind is chosen
    /// when an element of it can be made there, else the usual kind of the place: the main
    /// text kind of the book, the kind of the element the window was opened on
    static Book forAdding(const AddTarget& target);

    /// @brief A window in place of the system's window of files that chooses the file of a
    /// material, for tests: it gets the folder the system's window would open in and returns
    /// the file, or an empty path for Cancel; an empty function gives the system's window back
    static void setFileChooserForTests(std::function<QString(const QString& folder)> chooser);

    /// @brief Gives the window the kinds it offers after More Kinds...: opens Project
    /// Properties over @p window and returns the window's kinds then, or nullopt when the book
    /// got no new kinds
    using MoreKinds = std::function<std::optional<Book>(QWidget* window)>;

    /// @brief Create the dialog
    /// @param book The kinds and the parts of the book
    /// @param registry Kinds of the elements; it must outlive the dialog
    /// @param parent Parent widget
    NewFileDialog(const Book& book, const core::BookTypeRegistry& registry,
                  QWidget* parent = nullptr);

    /// @brief Show More Kinds... at the left of the buttons, which calls @p more
    void setMoreKinds(MoreKinds more);

    /// @brief The button More Kinds...; nullptr until setMoreKinds()
    QPushButton* moreKindsButton() const { return m_moreKindsButton; }

    /// @brief The kind of the new element; none when no tile is chosen and for an empty text
    /// file
    core::KindRef kind() const;

    /// @brief An empty text file outside the book is chosen: the caller opens an empty file
    /// with the title in a tab of its own
    bool outsideBook() const;

    /// @brief An empty text file with the book is chosen: the caller adds it to the files of
    /// the book outside its text (core::ProjectManager::addBookFile())
    bool emptyFileInBook() const;

    /// @brief The title of the new element, without spaces around it
    QString title() const;

    /// @brief The place of the new element
    core::ElementPlace place() const;

    /// @brief The status of a new text element of the book ("draft", "revision" or "final");
    /// empty for the other elements and in New File
    QString status() const;

    /// @brief Ids of the elements a new group takes inside it: those that close the main
    /// section at the end of the groups before it, unless the writer leaves them there; empty
    /// for other elements
    QStringList takeInside() const;

    /// @brief The file of a new material (absolute); empty for other elements and before the
    /// writer chooses one
    QString materialFile() const;

    /// @brief The status of a new text element of the book; hidden for other tiles
    QComboBox* statusBox() const { return m_statusBox; }

    /// @brief Whether a new group takes inside it the elements that close the main section;
    /// hidden when there are none
    QCheckBox* takeBox() const { return m_takeBox; }

    /// @brief The file of a new material; hidden for other tiles
    QLabel* materialLabel() const { return m_materialLabel; }

    /// @brief The button that chooses the file of a new material again
    QPushButton* chooseButton() const { return m_chooseButton; }

    /// @brief The tiles of the kinds, with the names of the parts of the book between them
    QListWidget* kindList() const { return m_kindList; }

    /// @brief The title of the new element
    QLineEdit* titleEdit() const { return m_titleEdit; }

    /// @brief The place of the new element in the book; hidden for an empty text file
    ElementPlacePicker* picker() const { return m_picker; }

    /// @brief Where an empty text file goes, in place of the list of places
    QLabel* placeNote() const { return m_placeNote; }

    /// @brief The place of an empty text file: outside the book; hidden for the other tiles
    QRadioButton* outsideButton() const { return m_outsideButton; }

    /// @brief The place of an empty text file: with the book; hidden for the other tiles
    QRadioButton* insideButton() const { return m_insideButton; }

    /// @brief The button that creates the element
    QPushButton* createButton() const { return acceptButton(); }

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

    /// @brief A material chosen at the start asks for its file once the window is shown
    void showEvent(QShowEvent* event) override;

    /// @brief A click in the proposed title selects it, so typing replaces it
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The description of the chosen kind, at the left
    QWidget* createDescriptionPanel(QWidget* parent);

    /// @brief The tiles of the kinds
    QWidget* createKindsGroup(QWidget* parent);

    /// @brief The title of the new element, with its label, and its status
    QLayout* createTitleRow(QWidget* parent);

    /// @brief The file of a new material, with the button that chooses it
    QWidget* createMaterialRow(QWidget* parent);

    /// @brief The place of the new element
    QWidget* createPlaceGroup(QWidget* parent);

    /// @brief The groups of the parts of the book with their tiles
    void fillKinds();

    /// @brief Give the tiles the size of the longest name at the font of the list, and draw
    /// their icons in the colors of the current icons
    void sizeTiles();

    /// @brief The kind of the chosen tile; nullptr when none is chosen and for an empty text
    /// file
    const NewFileKind* chosen() const;

    /// @brief The tile of an empty text file is chosen
    bool emptyFileChosen() const;

    /// @brief The name of an empty text file: its tile, and the name above its description
    static QString emptyFileName();

    /// @brief The places of @p entry in the part of the book of its tile, with the new
    /// element at the place it gets when the writer does not choose it; for a group, the book
    /// as it will be, with the elements it can take inside it
    void showPlaces(const NewFileKind& entry);

    /// @brief The book as it will be with a new group of @p entry, which takes inside it the
    /// elements that close the main section unless the writer leaves them there
    void showGroup(const NewFileKind& entry);

    /// @brief What the window notes about the elements a new group takes inside it, with the
    /// icon @p iconId; an empty @p text hides the note
    void showNotice(const QString& iconId, const QString& text);

    /// @brief @p text with the words of the chosen kind ({kind}) and of the main text kind of
    /// the book ({main})
    QString named(const QString& text) const;

    /// @brief The heading of Add Element for the chosen tile: what the writer chooses
    void updateHeading();

    /// @brief Ask for the file of a new material in the system's window of files; its title
    /// follows the file unless the writer has changed it
    void chooseMaterialFile();

    /// @brief Show the file of a new material, and what its copy is named
    void showMaterialFile();

    /// @brief Whether a new element of @p entry is a material
    static bool isMaterial(const NewFileKind* entry);

    /// @brief Whether a new element of @p entry gets a status: a text element of the book in
    /// Add Element
    bool hasStatus(const NewFileKind* entry) const;

    /// @brief Describe @p kind, a kind of @p section, in an entry: where its elements can be,
    /// how many, the file of a new one, the type, and why none can be made now
    /// @param offered The kinds of which a new element can be made there
    /// @param groups The group kinds of the book, for the words of where the kind can be
    static NewFileKind entryOf(const core::KindRef& kind, core::BookPlace section,
                               const QList<core::KindRef>& offered,
                               const QList<core::KindRef>& groups);

    /// @brief The description, the title and the place of an empty text file, outside the book
    /// or with it
    void showEmptyFile();

    /// @brief A new kind: its description, the title of its element unless the writer has
    /// changed it, and the places in its part of the book
    void onKindChanged();

    /// @brief Draw the icon of the chosen kind in the colors of the current icons
    void updateKindIcon();

    /// @brief The dialog can be accepted only with a kind, a title and a place
    void updateCreateButton();

    /// @brief Offer the kinds of @p book from now on; the chosen kind, or an empty text file,
    /// stays chosen with its title and its place
    void replaceBook(const Book& book);

    Book m_book;
    const core::BookTypeRegistry* m_registry = nullptr;
    bool m_titleChanged = false;  ///< The writer typed the title: it stays as it is
    QString m_materialFile;       ///< The file of a new material; empty: none chosen yet
    bool m_choosingFile = false;  ///< The system's window of files is open
    MoreKinds m_moreKinds;        ///< Opens Project Properties for More Kinds...
    QPushButton* m_moreKindsButton = nullptr;

    QLabel* m_iconLabel = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_whereLabel = nullptr;
    QLabel* m_countLabel = nullptr;
    QLabel* m_fileLabel = nullptr;
    QLabel* m_typeLabel = nullptr;
    QLabel* m_typeCaption = nullptr;  ///< "Type:", hidden with the type of an empty text file
    QListWidget* m_kindList = nullptr;
    QLineEdit* m_titleEdit = nullptr;
    QRadioButton* m_outsideButton = nullptr;
    QRadioButton* m_insideButton = nullptr;
    /// The list of places, or the note of an empty text file in its place: as high as the
    /// higher one, so that a choice moves nothing in the window
    QStackedWidget* m_placePages = nullptr;
    ElementPlacePicker* m_picker = nullptr;
    QLabel* m_placeNote = nullptr;

    // Add Element
    QLabel* m_statusLabel = nullptr;
    QComboBox* m_statusBox = nullptr;
    QWidget* m_materialRow = nullptr;
    QLabel* m_materialLabel = nullptr;
    QPushButton* m_chooseButton = nullptr;

    // A new group: the elements that close the main section, which it takes inside it
    QList<core::ProjectElement> m_closing;  ///< At the end of the groups before the group
    bool m_canTake = false;                 ///< The group can have all of them inside it
    QCheckBox* m_takeBox = nullptr;
    QWidget* m_noticeBox = nullptr;         ///< What the window notes about them
    QLabel* m_noticeIcon = nullptr;
    QLabel* m_noticeText = nullptr;
    QLabel* m_previewNote = nullptr;        ///< The list only shows the book as it will be
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
