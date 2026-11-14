// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file export_book_dialog.h
/// @brief File > Export > DOCX and Markdown: the file, the texts of the book and how they look
/// in it, and the progress and the result of the export in one window

#pragma once

#include "kalahari/editor/book_export.h"
#include "kalahari/gui/dialogs/archive_dialogs.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace kalahari {
namespace core {
class IExporter;
} // namespace core

namespace gui {
namespace dialogs {

class RowList;

/// @brief Export of the book to a Word document (DOCX) or to Markdown
///
/// The options are on four tabs as high as the highest, so the window keeps its size and
/// nothing in it scrolls: the file, the texts, the layout, and the annotations and the images.
/// The progress and the result show under the tabs. The command of the menu chooses the
/// format; the list of the formats can change it, and with it the extension of the name and
/// the options shown. The list has the program's formats and those of the exporters of the
/// plugins; a plugin's format with options of its own shows them on a tab of its own. The file goes to a folder that the program remembers, as the archives do. The window
/// exports the whole book, the texts chosen in its list (which stays in its place and works
/// with "Chosen texts"), or the text in front, also a file outside the book; without a book
/// only the text in front. The choices of the
/// window are kept in the settings (export.*) for the next export. The page comes from the
/// settings of the pages (Settings > Editor > Pages and Margins, the gear of the window).
class ExportBookDialog : public ArchiveDialog {
    Q_OBJECT

public:
    /// @brief What goes to the file
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
    };

    /// @brief A format of the list of the formats
    struct Format {
        QString id;         ///< DOCX_ID, MARKDOWN_ID, or the id of a plugin's format
        QString name;       ///< Its name in the list, with the extension of its files
        QString extension;  ///< The extension of its files, with its dot
    };

    /// @brief The ids of the program's own formats
    static constexpr const char* DOCX_ID = "docx";
    static constexpr const char* MARKDOWN_ID = "markdown";

    /// @brief The choices of the window
    struct Choice {
        QString formatId = QString::fromLatin1(DOCX_ID);  ///< The format of the list
        /// The program's own format; DOCX also for the format of a plugin (see formatId)
        editor::ExportFormat format = editor::ExportFormat::Docx;
        Content content = Content::Book;
        QStringList chosen;         ///< Content::Chosen: the ids of the chosen texts
        bool titles = true;         ///< The titles of the Navigator as headings
        bool newPage = true;        ///< Each text from a new page (DOCX)
        bool pageNumbers = true;    ///< Page numbers (DOCX)
        QString fontFamily;         ///< The font of the text (DOCX); empty: the editor's
        bool comments = false;      ///< The annotations as Word's comments (DOCX)
        bool copyImages = true;     ///< The images copied next to the file (Markdown)
        QString imagesFolder;       ///< The name of their folder (Markdown): "<name> – images"
    };

    /// @brief Writes the file @p path as @p choice says; false, with what went wrong in
    /// @p problem, when it cannot
    using Exporter =
        std::function<bool(const QString& path, const Choice& choice, const Progress& progress,
                           editor::ExportResult* result, editor::ExportProblem* problem)>;

    /// @brief Create the window
    /// @param book What the window says about the book
    /// @param format The format of the command of the menu
    /// @param folder The folder of the file at the start
    /// @param exporter Writes the file
    /// @param parent Parent widget
    ExportBookDialog(const Book& book, editor::ExportFormat format, const QString& folder,
                     Exporter exporter, QWidget* parent = nullptr);

    ~ExportBookDialog() override;

    /// @brief The choices of the window now
    Choice choice() const;

    /// @brief The formats the window lists: the program's own (DOCX, Markdown), then those of
    /// the exporters of the plugins (core::IExporter::getFormats())
    static QList<Format> formats();

    /// @brief The exporter of a plugin that writes the format @p id; null for the program's
    /// own formats and for an id no exporter has
    static std::shared_ptr<core::IExporter> exporterOf(const QString& id);

    /// @brief Choose one of the program's own formats, as the list of the formats does
    void setFormat(editor::ExportFormat format);

    /// @brief Choose the format @p id of the list; an id the list does not have changes nothing
    void setFormatId(const QString& id);

    /// @brief Choose what goes to the file, as its radio button does
    void setContent(Content content);

    /// @brief The folder of the file
    QString folder() const { return m_folder; }

    /// @brief Set the folder of the file, as Browse... does
    void setFolder(const QString& folder);

    /// @brief The field of the name of the file
    QLineEdit* nameField() const { return m_nameField; }

    /// @brief The path of the file: the folder and the name with the extension of the format;
    /// empty while the file cannot be written there
    QString filePath() const;

    /// @brief The line under the folder: why the program cannot use it, or that it creates it
    QString folderNote() const { return noteText(m_folderNote); }

    /// @brief The line under the name: why it cannot be the name of the file, the name the file
    /// gets, or that the export replaces a file
    QString nameNote() const { return noteText(m_nameNote); }

    /// @brief The list of the formats
    QComboBox* formatBox() const { return m_formatBox; }

    /// @brief The tab of the options of the format of a plugin, @p id; null for a format
    /// without one
    QWidget* formatPage(const QString& id) const { return m_formatPages.value(id); }

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

    /// @brief The tabs of the window: the file, the texts, the layout, and the annotations and
    /// the images
    QTabWidget* tabs() const { return m_tabs; }

    /// @brief The group of the annotations and the group of the images; each shows the option
    /// of its format (the comments of Word, the copy of the images of Markdown) and for the
    /// other format says what it does
    QGroupBox* notesGroup() const { return m_notesGroup; }
    QGroupBox* imagesGroup() const { return m_imagesGroup; }

    /// @brief The check boxes of the options
    QCheckBox* titlesBox() const { return m_titlesBox; }
    QCheckBox* newPageBox() const { return m_newPageBox; }
    QCheckBox* pageNumbersBox() const { return m_pageNumbersBox; }
    QCheckBox* commentsBox() const { return m_commentsBox; }
    QCheckBox* copyImagesBox() const { return m_copyImagesBox; }

    /// @brief The choice of the font of the text
    QComboBox* fontBox() const { return m_fontBox; }

    /// @brief The page of the settings, as the window says it: "A5, mirror margins: inner 3 cm,
    /// outer 2 cm"
    static QString pageText();

    /// @brief The page as the window shows it now
    QString shownPageText() const;

    /// @brief The name of the folder of the images of a Markdown file: "<name> – images"
    QString imagesFolderName() const;

protected:
    /// @brief Export the book
    void run() override;

private slots:
    /// @brief The page of the settings shown again (queued from the settings' listener)
    void showPage();

private:
    /// @brief Choose the folder in the system window
    void browse();

    /// @brief The fields of the format: the description, the extension, the groups
    void showFormat();

    /// @brief The list of the texts shows with Chosen; the name follows the content until the
    /// writer types it
    void showContent();

    /// @brief The lines under the folder and the name, the sentence under the list, and the
    /// accept button
    void updateFields();

    /// @brief A check box of the list changed: its group's box and the sentence follow
    void textChanged(QTreeWidgetItem* item);

    /// @brief The texts of the list (its leaves), in reading order
    QList<QTreeWidgetItem*> leaves() const;

    /// @brief Keep the choices of the window for the next export
    void rememberChoices() const;

    /// @brief The file name: the name with the extension of the format; empty when the name
    /// is empty
    QString fileName() const;

    /// @brief The extension of the format, with its dot
    QString extension() const;

    /// @brief The name without the extension of either format
    QString baseName() const;

    /// @brief The name the file gets for the content: the title of the book or of the text in
    /// front
    QString suggestedName() const;

    /// @brief Why the name cannot be the name of a file; empty when it can
    QString nameProblem() const;

    /// @brief Why the file could not be written, as the window says it; empty for a cause the
    /// window does not name
    QString reasonOf(const editor::ExportProblem& problem) const;

    Book m_book;
    Exporter m_exporter;
    QString m_folder;
    bool m_nameTyped = false;  ///< The writer typed the name: it no longer follows the content
    int m_settingsListener = -1;

    QList<Format> m_formats;                ///< The formats of the list, in its order
    QComboBox* m_formatBox = nullptr;
    QHash<QString, QWidget*> m_formatPages;  ///< The tabs of the options of the plugins' formats
    QLineEdit* m_folderField = nullptr;
    QLineEdit* m_nameField = nullptr;
    FieldNote m_folderNote;
    FieldNote m_nameNote;

    QRadioButton* m_bookButton = nullptr;
    QRadioButton* m_chosenButton = nullptr;
    QRadioButton* m_frontButton = nullptr;
    QWidget* m_chosenBox = nullptr;
    RowList* m_textList = nullptr;
    QLabel* m_chosenNote = nullptr;

    QTabWidget* m_tabs = nullptr;
    QWidget* m_extrasTab = nullptr;  ///< The tab of the annotations and the images
    QCheckBox* m_titlesBox = nullptr;
    QWidget* m_wordLayout = nullptr;
    QCheckBox* m_newPageBox = nullptr;
    QCheckBox* m_pageNumbersBox = nullptr;
    QComboBox* m_fontBox = nullptr;
    QLabel* m_pageLabel = nullptr;
    FieldNote m_pageNote;

    QGroupBox* m_notesGroup = nullptr;
    QCheckBox* m_commentsBox = nullptr;
    QWidget* m_commentsOption = nullptr;  ///< The comments with their remark (Word)
    QLabel* m_noCommentsNote = nullptr;   ///< Markdown has no comments

    QGroupBox* m_imagesGroup = nullptr;
    QCheckBox* m_copyImagesBox = nullptr;
    QWidget* m_copyImagesOption = nullptr;  ///< The copy of the images with its remark (Markdown)
    QLabel* m_imagesInFileNote = nullptr;   ///< A Word document has the images in itself
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
