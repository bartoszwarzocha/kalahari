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

#include <string>

class QCheckBox;
class QComboBox;
class QGridLayout;
class QLabel;
class QRadioButton;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace kalahari {
namespace gui {
namespace dialogs {

class RowList;

/// @brief The window of the export or of the print of the book: tabs as high as the highest,
///        so the window keeps its size and nothing in it scrolls, two of them the same in both
///        windows
///
/// The tab of the texts: the whole book, the texts chosen in its list (which stays in its
/// place and works with "Chosen texts"), or the text in front, also a file outside the book;
/// without a book only the text in front. The tab of the layout: the titles of the Navigator
/// as headings, and of the pages each text from a new page, the page numbers, the font, the
/// running head and the page of the settings (Settings > Editor > Pages and Margins). Its
/// choices are kept in the settings (export.*), for the export and the print alike; which
/// texts each window keeps on its own.
class BookOutputDialog : public ArchiveDialog {
    Q_OBJECT

public:
    /// @brief What goes to the file or to the printer
    enum class Content {
        Book,    ///< The whole book
        Chosen,  ///< The texts chosen in the list
        Front    ///< The text in front
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
    };

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
    };

    ~BookOutputDialog() override;

    /// @brief The choices of the tabs of the texts and of the layout now
    Layout layoutChoice() const;

    /// @brief Choose what goes to the file or to the printer, as its radio button does
    void setContent(Content content);

    /// @brief The radio buttons of the content
    QRadioButton* bookButton() const { return m_bookButton; }
    QRadioButton* chosenButton() const { return m_chosenButton; }
    QRadioButton* frontButton() const { return m_frontButton; }

    /// @brief The list of the texts of the book, with their check boxes
    QTreeWidget* textList() const;

    /// @brief The sentence under the list: how many texts are chosen
    QString chosenNote() const;

    /// @brief Check every text of the list, none, or only the final ones, as the buttons beside
    /// the list do
    void checkAll();
    void checkNone();
    void checkFinal();

    /// @brief The tabs of the window
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
    BookOutputDialog(Book book, QWidget* parent);

    /// @brief What the window says about the book
    const Book& book() const { return m_book; }

    /// @brief A new tab at the end, a grid whose second column stretches
    QGridLayout* newTab(const QString& title);

    /// @brief The rows of a tab start at its top
    static void endTab(QGridLayout* grid);

    /// @brief The tab of the texts
    /// @param title The title of the tab, with its access key
    /// @param note The sentence at its bottom
    void addTextsTab(const QString& title, const QString& note);

    /// @brief The tab of the layout: the titles as headings, then the options of the pages
    /// @param title The title of the tab, with its access key
    /// @param narrow The tab is narrow (beside the preview of the print): the font and the
    ///        running head each in a row of its own, a note of one line under the page
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

    Book m_book;
    int m_settingsListener = -1;
    QTabWidget* m_tabs = nullptr;

    QRadioButton* m_bookButton = nullptr;
    QRadioButton* m_chosenButton = nullptr;
    QRadioButton* m_frontButton = nullptr;
    QWidget* m_chosenBox = nullptr;
    RowList* m_textList = nullptr;
    QLabel* m_chosenNote = nullptr;

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
