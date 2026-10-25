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

#include <QImage>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QEvent;
class QGroupBox;
class QLabel;
class QLayout;
class QLineEdit;
class QListWidget;
class QPushButton;
class QRadioButton;
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
/// editor it was opened on, with groups (a part) among the kinds and no empty text file. Its
/// top chooses what the tiles are (AddChooser): a section of the book or the Workshop, one at
/// a time, or the whole book or the Workshop; the place the window was opened on chooses only
/// where it starts, and Alt with the underlined letter, Ctrl+Tab and Ctrl+Shift+Tab choose
/// another. Without the choice the window offers the kinds of that place only. A text
/// element of the book gets its status, Draft at first. A new group can take inside it the
/// elements that close the main section at the end of the groups before it (the epilogue),
/// and the list only shows the book as it will be. The kinds of the Workshop are never in one
/// list with those of the book; its elements are in alphabetical order, so a note says where
/// a new one goes in place of the list.
///
/// A material comes from a source the window chooses above its title (Source): a file from
/// the disk, whose copy goes to the book, a web address, or neither (a paper book, a talk). The
/// path of a file is typed, pasted or dropped on its field, or chosen in the system's window of
/// files, which only Choose File opens; the address is typed or pasted, and a web address on
/// the clipboard when the window opens is there already, selected. The window tells a page from
/// a picture by the end of the address, without connecting to it. Whether the program downloads
/// the title of the page or a copy of the picture after Add is a box, empty each time, or the
/// setting MaterialDownloader::SETTING. The title of a material starts as the name of its file
/// or the end of its address. The window remembers the source chosen last.
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
        /// What the top of Add Element chooses from; AddChooser::None: no choice, the kinds
        /// of the place the window was opened on only
        AddChooser chooser = AddChooser::None;
        /// With a choice, the part of the book (or the Workshop) the window starts at
        core::BookPlace start = core::BookPlace::Main;
    };

    /// @brief The kinds and the parts of the book open in core::ProjectManager; at the start
    /// the main text kind of the book in its main part is chosen, else the first kind of which
    /// a new element can be made
    /// @param untitled Title of an empty text file; empty: no empty text file
    static Book openBook(const QString& untitled = QString());

    /// @brief The kinds of Add Element opened on @p target, a place of the book or of the
    /// Workshop, in core::ProjectManager; at the start @p target's kind is chosen when an
    /// element of it can be made there, else the usual kind of the place: the kind of the
    /// element the window was opened on, the main text kind of the book
    ///
    /// With a choice at the top (@p chooser) the window has all the kinds of the book and of
    /// the Workshop, and starts at the part of @p target; those of @p target's place go there
    /// (right after its element, inside its group). Without it, only the kinds of that place.
    static Book forAdding(const AddTarget& target, AddChooser chooser);

    /// @brief A window in place of the system's window of files that chooses the file of a
    /// material, for tests: it gets the folder the system's window would open in and returns
    /// the file, or an empty path for Cancel; an empty function gives the system's window back
    static void setFileChooserForTests(std::function<QString(const QString& folder)> chooser);

    /// @brief Create the dialog
    /// @param book The kinds and the parts of the book
    /// @param registry Kinds of the elements; it must outlive the dialog
    /// @param parent Parent widget
    NewFileDialog(const Book& book, const core::BookTypeRegistry& registry,
                  QWidget* parent = nullptr);

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

    /// @brief Where a new material comes from, chosen in the Source of the window
    enum class MaterialOrigin {
        File,     ///< A file from the disk, or a picture without a file (setMaterialPicture())
        Address,  ///< A web address
        None,     ///< Neither a file nor an address
    };

    /// @brief The setting of the source chosen last for a kind of materials without a source of
    /// its own (of a package other than the base one): "file", "address" or "none"; a web page
    /// starts with its address, a quote with neither, the other kinds of the base package with
    /// a file
    static constexpr const char* SOURCE_SETTING = "navigator.addMaterialSource";

    /// @brief The source of a new material; File for other elements
    MaterialOrigin materialOrigin() const;

    /// @brief The file of a new material (absolute); empty for other elements, other sources,
    /// a picture without a file and while the path names no file
    QString materialFile() const;

    /// @brief The picture without a file of a new material (setMaterialPicture()); a null
    /// image for other elements and sources
    QImage materialPicture() const;

    /// @brief The web address of a new material; empty for other elements and sources and
    /// while the text is no web address
    QUrl materialAddress() const;

    /// @brief Whether the program downloads the title of the page or the copy of the picture
    /// of a new material with an address after Add: the box of the window, or the setting
    bool downloadChosen() const;

    /// @brief The elements a new material is linked to (setMaterialLinks()); empty for other
    /// elements
    QStringList materialLinks() const;

    /// @brief A material from the file @p path: the source File with its path
    void setMaterialFile(const QString& path);

    /// @brief A material from the web address @p url: the source Address with it
    void setMaterialAddress(const QUrl& url);

    /// @brief A material from @p picture, without a file (a screenshot on the clipboard): the
    /// source File, with the picture in place of the path; its copy is a PNG file
    void setMaterialPicture(const QImage& picture);

    /// @brief The elements a new material is linked to (a chapter it was dropped on)
    void setMaterialLinks(const QStringList& links);

    /// @brief The status of a new text element of the book; hidden for other tiles
    QComboBox* statusBox() const { return m_statusBox; }

    /// @brief Whether a new group takes inside it the elements that close the main section;
    /// hidden when there are none
    QCheckBox* takeBox() const { return m_takeBox; }

    /// @brief The source of a new material, with its buttons, fields and notes; hidden for
    /// other tiles
    QGroupBox* sourceGroup() const { return m_sourceGroup; }

    /// @brief The buttons of the source of a new material, with the ids of MaterialOrigin
    QButtonGroup* sourceButtons() const { return m_sourceButtons; }

    /// @brief The path of the file of a new material
    QLineEdit* fileEdit() const { return m_fileEdit; }

    /// @brief The button that chooses the file of a new material in the system's window
    QPushButton* chooseButton() const { return m_chooseButton; }

    /// @brief What the window tells of the file of a new material
    QLabel* fileNote() const { return m_fileNote; }

    /// @brief The web address of a new material
    QLineEdit* addressEdit() const { return m_addressEdit; }

    /// @brief What the window tells of the address: a page or a picture
    QLabel* addressNote() const { return m_addressNote; }

    /// @brief The box that downloads the title of the page or the copy of the picture; hidden
    /// unless the setting asks
    QCheckBox* downloadBox() const { return m_downloadBox; }

    /// @brief What the program takes from the network for the address
    QLabel* networkNote() const { return m_networkNote; }

    /// @brief The tiles of the kinds, with the names of the parts of the book between them
    QListWidget* kindList() const { return m_kindList; }

    /// @brief The buttons at the top of Add Element that choose what the tiles are, in their
    /// order; nullptr without the choice
    QButtonGroup* chooserButtons() const { return m_chooser; }

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

    /// @brief A click in the proposed title selects it, so typing replaces it; a file dropped
    /// on the path of a material is its file
    bool eventFilter(QObject* watched, QEvent* event) override;

    /// @brief Add remembers the source of a material
    void done(int result) override;

private:
    /// @brief forAdding() without a choice: the kinds of the place of @p target only
    static Book forPlace(const AddTarget& target);

    /// @brief The buttons at the top of Add Element that choose what the tiles are, each
    /// named with an underlined letter that the window's other words do not underline
    QWidget* createChooser(QWidget* parent);

    /// @brief The parts of the book (or the Workshop) of choice @p choice of the buttons at
    /// the top; all of them without the choice
    QList<core::BookPlace> placesOf(int choice) const;

    /// @brief The tiles of choice @p choice of the buttons at the top, with the kind chosen
    /// there last, or at first the usual one (the main text kind of the book); the title gets
    /// the keys, so that typing replaces it
    void showChoice(int choice);

    /// @brief The description of the chosen kind, at the left
    QWidget* createDescriptionPanel(QWidget* parent);

    /// @brief The tiles of the kinds
    QWidget* createKindsGroup(QWidget* parent);

    /// @brief The title of the new element, with its label, and its status
    QLayout* createTitleRow(QWidget* parent);

    /// @brief The source of a new material: its buttons, and the path, the address or a note
    QGroupBox* createSourceGroup(QWidget* parent);

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

    /// @brief The tiles shown are the kinds of the Workshop
    bool workshopShown() const;

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

    /// @brief The page of the chosen source, what the window tells of it, and the title it
    /// gives unless the writer has changed it
    void showSource();

    /// @brief The title the source of a new material gives: the name of its file, the end of
    /// its address; empty without either
    QString sourceTitle() const;

    /// @brief The file the path of a new material names, absolute; empty when it names none
    QString typedFile() const;

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

    Book m_book;
    const core::BookTypeRegistry* m_registry = nullptr;
    bool m_titleChanged = false;  ///< The writer typed the title: it stays as it is
    bool m_choosingFile = false;  ///< The system's window of files is open
    QImage m_materialPicture;     ///< A picture without a file; null: none
    QString m_pictureTitle;       ///< The title of the picture without a file
    QStringList m_materialLinks;  ///< The elements a new material is linked to
    bool m_sourceGiven = false;   ///< The program gave the source: it is not remembered
    bool m_sourceChosen = false;  ///< The writer chose the source: it stays for another kind

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
    QButtonGroup* m_chooser = nullptr;  ///< What the tiles are; nullptr without the choice
    QList<int> m_choices;               ///< Of each choice the index in Book::kinds chosen there
    int m_shownChoice = -1;             ///< The choice shown; -1: no choice
    QLabel* m_titleLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QComboBox* m_statusBox = nullptr;

    // The source of a new material
    QGroupBox* m_sourceGroup = nullptr;
    QButtonGroup* m_sourceButtons = nullptr;
    QStackedWidget* m_sourcePages = nullptr;  ///< The path, the address or the note of none
    QLabel* m_fileCaption = nullptr;
    QLineEdit* m_fileEdit = nullptr;
    QPushButton* m_chooseButton = nullptr;
    QLabel* m_fileNote = nullptr;
    QLabel* m_addressCaption = nullptr;
    QLineEdit* m_addressEdit = nullptr;
    QLabel* m_addressNote = nullptr;
    QCheckBox* m_downloadBox = nullptr;
    QWidget* m_networkBox = nullptr;  ///< What the program takes from the network, with an icon
    QLabel* m_networkIcon = nullptr;
    QLabel* m_networkNote = nullptr;

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
