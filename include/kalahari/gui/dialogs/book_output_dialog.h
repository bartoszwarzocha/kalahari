// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_output_dialog.h
/// @brief The base of the windows that put the texts of the book on pages, the export and the
///        print: the tabs of the texts and of the layout they share

#pragma once

#include "kalahari/editor/book_export.h"
#include "kalahari/gui/dialogs/archive_dialogs.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <string>
#include <utility>

class QCheckBox;
class QComboBox;
class QPushButton;
class QGridLayout;
class QLabel;
class QRadioButton;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace kalahari {
namespace gui {
namespace dialogs {

class KalahariDialog;
class RowList;

/// @brief The window of the export or of the print of the book: tabs as high as the highest,
///        so the window keeps its size and nothing in it scrolls (the export), or sections of
///        the window's own (the print), two of them the same in both windows
///
/// The texts: the whole book, the texts chosen in its list, or the text in front, also a file
/// outside the book; without a book only the text in front. On a tab the list stays in its
/// place and works with "Chosen texts"; in a section of the print it is in the small window
/// of Choose Texts.... The layout: the titles of the Navigator as headings, and of the pages
/// each text from a new page, the page numbers, the font, the running head and, on a tab,
/// the page of the settings (Settings > Editor > Pages and Margins). Its choices are kept in
/// the settings (export.*), for the export and the print alike; which texts each window keeps
/// on its own. A series has the list Volume with the texts: the window shows the texts and the
/// data of the volume chosen in it, the current one at first.
class BookOutputDialog : public ArchiveDialog {
    Q_OBJECT

public:
    /// @brief What goes to the file or to the printer
    enum class Content {
        Book,    ///< The whole book
        Chosen,  ///< The texts chosen in the list
        Front,   ///< The text in front
        Set      ///< The set for a publisher of the book: its parts under their names
    };

    /// @brief A row of the list of the texts, in reading order
    struct Row {
        QString id;          ///< The element; empty: a section of the book
        QString title;       ///< Its title
        QString status;      ///< Of a text: "draft", "revision", "final"; empty: none
        QString iconId;      ///< ArtProvider id of the icon of its kind
        int level = 0;       ///< Its depth: 0 a section, 1 an element of the section...
        bool leaf = true;    ///< A text (or a table of contents); false: a section or a group
    };

    /// @brief A volume of a series in the list Volume
    struct Volume {
        QString id;    ///< Its book (core::ProjectBook::id)
        QString name;  ///< As the list shows it: "Volume 2: Dunes"
    };

    /// @brief What the window says about the book and the text in front
    struct Book {
        QString title;       ///< The title of the book; empty: no book is open
        QList<Row> rows;     ///< Its texts with their sections and groups, in reading order
        QString frontTitle;  ///< The title of the text in front; empty: none
        QString frontId;     ///< Its element; empty: a file outside the book
        QString author;      ///< The author of the book (the data of an EPUB)
        QString language;    ///< The name of the language of its text, as the window shows it
        QString description;  ///< What it is about, from the materials of the cover; empty: none
        QString coverTitle;  ///< The title of the design of the front of the cover chosen in
                             ///< the materials of the cover; empty: none
        QString coverFile;   ///< Its picture; empty: none
        QString isbn;        ///< The ISBN of its e-book, as the book keeps it; empty: none
        QString publisher;   ///< Its publisher, as the book keeps it; empty: none
        QString setTitle;    ///< The set for a publisher as the name of its file: "Sand – set
                             ///< for the publisher"; empty: the set has no text
        int setParts = 0;    ///< The parts of the set with a text
        QList<Volume> volumes;  ///< The volumes of a series, in their order; empty: one book
        QString volume;         ///< The volume it is of a series; empty: one book
    };

    /// @brief What the window says about volume @p volumeId of the series, the text in front
    ///        aside
    using VolumeBook = std::function<Book(const QString& volumeId)>;

    /// @brief The choices of the tabs of the texts and of the layout
    struct Layout {
        Content content = Content::Book;
        QStringList chosen;         ///< Content::Chosen: the ids of the chosen texts
        bool titles = true;         ///< The titles of the Navigator as headings
        bool newPage = true;        ///< Each text from a new page (pages)
        bool pageNumbers = true;    ///< Page numbers (pages)
        QString fontFamily;         ///< The font of the text (pages); empty: the editor's
        /// At the top of the pages of the texts
        editor::RunningHead runningHead = editor::RunningHead::AuthorAndTitle;
        QString volume;  ///< The volume of a series whose texts go; empty: one book
    };

    ~BookOutputDialog() override;

    /// @brief The choices of the tabs of the texts and of the layout now
    Layout layoutChoice() const;

    /// @brief Choose what goes to the file or to the printer, as its radio button does
    void setContent(Content content);

    /// @brief Where the window takes the data of the volume chosen in the list Volume
    void setVolumeBook(VolumeBook volumeBook);

    /// @brief The list Volume of a series; nullptr for one book
    [[nodiscard]] QComboBox* volumeBox() const { return m_volumeBox; }

    /// @brief The radio buttons of the content
    QRadioButton* bookButton() const { return m_bookButton; }
    QRadioButton* chosenButton() const { return m_chosenButton; }
    QRadioButton* frontButton() const { return m_frontButton; }
    [[nodiscard]] QRadioButton* setButton() const { return m_setButton; }

    /// @brief The list of the texts of the book, with their check boxes
    QTreeWidget* textList() const;

    /// @brief The sentence under the list: how many texts are chosen
    QString chosenNote() const;

    /// @brief Choose Texts... beside the chosen texts in a section of the print; nullptr on a
    ///        tab, where the list shows
    [[nodiscard]] QPushButton* chooseTextsButton() const { return m_chooseTextsButton; }

    /// @brief The small window of the list of the texts, which Choose Texts... opens; nullptr
    ///        on a tab
    [[nodiscard]] KalahariDialog* chosenTextsWindow() const { return m_chosenWindow; }

    /// @brief Open the small window of the list, as Choose Texts... does: its OK chooses the
    ///        texts checked in it, its Cancel checks the texts as they were
    void chooseTexts();

    /// @brief Check every text of the list, none, or only the final ones, as the buttons beside
    /// the list do
    void checkAll();
    void checkNone();
    void checkFinal();

    /// @brief The tabs of the window; nullptr in a window of sections (the print)
    QTabWidget* tabs() const { return m_tabs; }

    /// @brief The check boxes of the layout
    QCheckBox* titlesBox() const { return m_titlesBox; }
    QCheckBox* newPageBox() const { return m_newPageBox; }
    QCheckBox* pageNumbersBox() const { return m_pageNumbersBox; }

    /// @brief The choice of the font of the text
    QComboBox* fontBox() const { return m_fontBox; }

    /// @brief The choice of the running head
    QComboBox* runningHeadBox() const { return m_runningHeadBox; }

    /// @brief The page of the settings, as the window says it: "A5, mirror margins: inner 3 cm,
    /// outer 2 cm"
    static QString pageText();

    /// @brief The size of the page of the settings, as the window says it: "A5", "6 x 9 in",
    /// "150 x 220 mm"
    static QString pageSizeName();

    /// @brief The page as the window shows it now
    QString shownPageText() const;

protected:
    /// @brief Create the window of @p book with its tabs, which the window puts in its place
    ///        and fills: the first one of its own, then addTextsTab() and addLayoutTab()
    /// @param withTabs false: no tabs; the window puts its pages in places of its own
    ///        (newTab())
    BookOutputDialog(Book book, QWidget* parent, bool withTabs = true);

    /// @brief What the window says about the book
    const Book& book() const { return m_book; }

    /// @brief The title of what goes to the file or to the printer: of the book, of the text
    ///        in front or of the set for a publisher
    [[nodiscard]] QString contentTitle() const;

    /// @brief Whether what goes is one text: the text in front or the set for a publisher
    [[nodiscard]] bool oneText() const;

    /// @brief Makes the place of a part of the fields with @p title in a window without tabs
    ///        (the print: a section), a grid whose second column stretches
    using PartMaker = std::function<QGridLayout*(const QString& title)>;

    /// @brief Where the parts of the fields go in a window without tabs, before they are made
    void setPartMaker(PartMaker maker) { m_partMaker = std::move(maker); }

    /// @brief A new tab at the end, a grid whose second column stretches; in a window without
    ///        tabs the place the part maker gives
    QGridLayout* newTab(const QString& title);

    /// @brief The rows of a tab start at its top
    static void endTab(QGridLayout* grid);

    /// @brief The tab of the texts
    /// @param title The title of the tab, with its access key
    /// @param note The sentence at its bottom
    /// @param compact A narrow section (the print): the choices one under another, the text
    ///        in front first; the list in the small window of Choose Texts...
    void addTextsTab(const QString& title, const QString& note, bool compact = false);

    /// @brief The tab of the layout: the titles as headings, then the options of the pages
    /// @param title The title of the tab, with its access key
    /// @param narrow A narrow section (the print): the font and the running head each in a row
    ///        of its own, without the page, which the print sets in a section of its own
    /// @return Its grid, for more rows of the window's own; endTab() ends it
    QGridLayout* addLayoutTab(const QString& title, bool narrow = false);

    /// @brief Show or hide the options of the pages: each text from a new page, the page
    ///        numbers, the font, the running head and the page
    void setPagesVisible(bool visible);

    /// @brief Set the tabs as the settings keep the writer's last choices
    /// @param contentKey The setting of what goes to the file or to the printer
    void readLayout(const std::string& contentKey);

    /// @brief Keep the choices of the tabs for the next time
    void rememberLayout(const std::string& contentKey) const;

    /// @brief The sentence under the list says how many texts are chosen
    /// @return Whether there is something to export or print: not the chosen texts, or some
    ///         of them
    bool updateChosen();

    /// @brief What goes to the file or to the printer changed
    virtual void contentChanged() = 0;

    /// @brief Another volume of the series was chosen: book() is its; the window shows its
    ///        data (the base: nothing more than its texts)
    virtual void bookChanged();

    /// @brief A text of the list was checked or unchecked: the fields and the accept button
    ///        follow
    virtual void updateFields() = 0;

    /// @brief An option of the layout changed, or the page of the settings
    virtual void layoutChanged();

private slots:
    /// @brief The page of the settings shown again (queued from the settings' listener)
    void showPage();

private:
    /// @brief The rows of the book in the list of the texts, with their check boxes: the
    ///        sections and the groups with the texts under them, all checked
    void fillTextList();

    /// @brief The texts of the list (its leaves), in reading order
    QList<QTreeWidgetItem*> leaves() const;

    /// @brief The list Volume over the texts, the volume of book() chosen in it
    /// @return Its row: the label and the list
    QWidget* newVolumeRow(QWidget* parent);

    /// @brief How many texts the book has and the parts of its set, on their radio buttons
    void showBookCounts();

    /// @brief The texts that go were chosen on the tab of the texts: the list, the list Volume
    /// and the new page of each part of the set follow the choice
    void followContentChoice();

    /// @brief Show the texts and the data of the volume at @p index of the list Volume
    void showVolume(int index);

    Book m_book;
    VolumeBook m_volumeBook;
    int m_settingsListener = -1;
    QTabWidget* m_tabs = nullptr;
    PartMaker m_partMaker;  ///< The parts of a window without tabs; empty: its tabs

    QRadioButton* m_bookButton = nullptr;
    QRadioButton* m_chosenButton = nullptr;
    QRadioButton* m_frontButton = nullptr;
    QRadioButton* m_setButton = nullptr;
    QWidget* m_volumeRow = nullptr;  ///< The list Volume with its label; nullptr: one book
    QComboBox* m_volumeBox = nullptr;
    QWidget* m_chosenBox = nullptr;
    RowList* m_textList = nullptr;
    QLabel* m_chosenNote = nullptr;
    QPushButton* m_chooseTextsButton = nullptr;  ///< In a section: opens the list
    QLabel* m_chosenCount = nullptr;  ///< In a section: how many texts are chosen, beside it
    KalahariDialog* m_chosenWindow = nullptr;  ///< In a section: the list in a small window

    QCheckBox* m_titlesBox = nullptr;
    QWidget* m_pagesLayout = nullptr;  ///< Each text from a new page, the page numbers
    QCheckBox* m_newPageBox = nullptr;
    QCheckBox* m_pageNumbersBox = nullptr;
    QComboBox* m_fontBox = nullptr;
    QWidget* m_fontRow = nullptr;  ///< The font and the running head
    QWidget* m_runningHeadRow = nullptr;  ///< The running head in a narrow tab; null: beside
                                          ///< the font
    QComboBox* m_runningHeadBox = nullptr;
    QLabel* m_pageLabel = nullptr;
    FieldNote m_pageNote;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
