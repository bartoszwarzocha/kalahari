// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file export_book_dialog.h
/// @brief File > Export > DOCX, PDF, Markdown and EPUB: the file, the texts of the book and how
/// they look in it, and the progress and the result of the export in one window

#ifndef KALAHARI_GUI_DIALOGS_EXPORT_BOOK_DIALOG_H
#define KALAHARI_GUI_DIALOGS_EXPORT_BOOK_DIALOG_H

#include "kalahari/editor/book_export.h"
#include "kalahari/gui/dialogs/book_output_dialog.h"

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
class QButtonGroup;

namespace kalahari {
namespace core {
class IExporter;
} // namespace core

namespace gui::dialogs {

/// @brief Export of the book to a Word document (DOCX), a PDF document, Markdown or an EPUB
///        e-book
///
/// The options are on tabs as high as the highest, so the window keeps its size and nothing in
/// it scrolls: the file, the texts, the layout, and the annotations and the images; a PDF has
/// its own tab (its purpose, bookmarks, pages in pairs, shades of gray), an EPUB the tab of its
/// cover and data (the cover, the data of the book, its ISBN and publisher). The tabs of the
/// texts and of the layout are those of the print (BookOutputDialog). The progress and the
/// result show under the tabs. The command of the menu chooses the format; the list of the
/// formats can change it, and with it the extension of the name and the options shown. The
/// list has the program's formats and those of the exporters of the plugins; a plugin's format
/// with options of its own shows them on a tab of its own. The file goes to a folder that the
/// program remembers, as the archives do. The choices of the window are kept in the settings
/// (export.*) for the next export, those of a format that differ from the others' for that
/// format (the comments of Word and of a PDF, export.pdf.*, export.epub.*). The page comes from
/// the settings of the pages (Settings > Editor > Pages and Margins, the gear of the window).
class ExportBookDialog : public BookOutputDialog {
    Q_OBJECT

public:
    /// @brief The cover of an EPUB
    enum class Cover {
        Design,  ///< The design of the front chosen in the materials of the cover
        None,    ///< No cover
        File,    ///< Another picture, chosen in the window
    };

    /// @brief A format of the list of the formats
    struct Format {
        QString id;         ///< DOCX_ID, MARKDOWN_ID, or the id of a plugin's format
        QString name;       ///< Its name in the list, with the extension of its files
        QString extension;  ///< The extension of its files, with its dot
    };

    /// @brief The ids of the program's own formats
    static constexpr const char* DOCX_ID = "docx";
    static constexpr const char* PDF_ID = "pdf";
    static constexpr const char* MARKDOWN_ID = "markdown";
    static constexpr const char* EPUB_ID = "epub";

    /// @brief The choices of the window: those of the texts and the layout, and of the format
    struct Choice : Layout {
        QString formatId = QString::fromLatin1(DOCX_ID);  ///< The format of the list
        /// The program's own format; DOCX also for the format of a plugin (see formatId)
        editor::ExportFormat format = editor::ExportFormat::Docx;
        bool comments = false;      ///< The annotations as the comments of a Word document or
                                    ///< of a PDF (not one for a print shop)
        bool copyImages = true;     ///< The images copied next to the file (Markdown)
        QString imagesFolder;       ///< The name of their folder (Markdown): "<name> – images"
        editor::PdfOptions pdf;     ///< The options of a PDF
        bool bookStyles = true;     ///< EPUB: the look of the book's styles; false: a simple one
        bool embedFont = false;     ///< EPUB: the font of the text in the file
        Cover cover = Cover::Design;  ///< EPUB: its cover
        QString coverImage;         ///< EPUB: the picture of the cover; empty: none
        QString isbn;               ///< EPUB: its ISBN; empty: none
        QString publisher;          ///< EPUB: its publisher; empty: none
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
    [[nodiscard]] Choice choice() const;

    /// @brief The formats the window lists: the program's own (DOCX, PDF, Markdown, EPUB), then
    /// those of the exporters of the plugins (core::IExporter::getFormats())
    static QList<Format> formats();

    /// @brief The exporter of a plugin that writes the format @p id; null for the program's
    /// own formats and for an id no exporter has
    static std::shared_ptr<core::IExporter> exporterOf(const QString& id);

    /// @brief Choose one of the program's own formats, as the list of the formats does
    void setFormat(editor::ExportFormat format);

    /// @brief Choose the format @p id of the list; an id the list does not have changes nothing
    void setFormatId(const QString& id);

    /// @brief The folder of the file
    [[nodiscard]] QString folder() const { return m_folder; }

    /// @brief Set the folder of the file, as Browse... does
    void setFolder(const QString& folder);

    /// @brief The field of the name of the file
    [[nodiscard]] QLineEdit* nameField() const { return m_nameField; }

    /// @brief The path of the file: the folder and the name with the extension of the format;
    /// empty while the file cannot be written there
    [[nodiscard]] QString filePath() const;

    /// @brief The line under the folder: why the program cannot use it, or that it creates it
    [[nodiscard]] QString folderNote() const { return noteText(m_folderNote); }

    /// @brief The line under the name: why it cannot be the name of the file, the name the file
    /// gets, or that the export replaces a file
    [[nodiscard]] QString nameNote() const { return noteText(m_nameNote); }

    /// @brief The list of the formats
    [[nodiscard]] QComboBox* formatBox() const { return m_formatBox; }

    /// @brief The tab of the options of the format of a plugin, @p id; null for a format
    /// without one
    [[nodiscard]] QWidget* formatPage(const QString& id) const { return m_formatPages.value(id); }

    // tabs(): the file, the texts, the layout, the annotations and the images, the PDF and the
    // cover and data of an EPUB (each shown while its format is chosen), and the tabs of the
    // plugins' formats

    /// @brief The tab of the options of a PDF, and the tab of the cover and data of an EPUB
    [[nodiscard]] QWidget* pdfTab() const { return m_pdfTab; }
    [[nodiscard]] QWidget* epubTab() const { return m_epubTab; }

    /// @brief The group of the annotations and the group of the images; each shows the option
    /// of its format (the comments of Word and of a PDF, the copy of the images of Markdown)
    /// and for the other formats says what they do
    [[nodiscard]] QGroupBox* notesGroup() const { return m_notesGroup; }
    [[nodiscard]] QGroupBox* imagesGroup() const { return m_imagesGroup; }

    /// @brief The check boxes of the annotations and of the images
    [[nodiscard]] QCheckBox* commentsBox() const { return m_commentsBox; }
    [[nodiscard]] QCheckBox* pdfCommentsBox() const { return m_pdfCommentsBox; }
    [[nodiscard]] QCheckBox* copyImagesBox() const { return m_copyImagesBox; }

    /// @brief The sentences under the annotations as comments of a PDF, and in the groups of
    /// the annotations and of the images for a format without their options
    [[nodiscard]] QString pdfCommentsNote() const;
    [[nodiscard]] QString notesNote() const;
    [[nodiscard]] QString imagesNote() const;

    /// @brief The options of a PDF: its purpose, the bookmarks, the pages in pairs, the shades
    /// of gray
    [[nodiscard]] QRadioButton* purposeButton(editor::PdfPurpose purpose) const;
    [[nodiscard]] QCheckBox* bookmarksBox() const { return m_bookmarksBox; }
    [[nodiscard]] QCheckBox* facingPagesBox() const { return m_facingPagesBox; }
    [[nodiscard]] QCheckBox* grayscaleBox() const { return m_grayscaleBox; }

    /// @brief The look of an EPUB (the book's styles or a simple one) and its font
    [[nodiscard]] QRadioButton* bookStylesButton() const { return m_bookStylesButton; }
    [[nodiscard]] QRadioButton* simpleLookButton() const { return m_simpleLookButton; }
    [[nodiscard]] QCheckBox* embedFontBox() const { return m_embedFontBox; }

    /// @brief The cover of an EPUB, the sentence under it, and the fields of its ISBN and its
    /// publisher
    [[nodiscard]] QComboBox* coverBox() const { return m_coverBox; }
    [[nodiscard]] QString coverNote() const { return noteText(m_coverNote); }
    [[nodiscard]] QLineEdit* isbnField() const { return m_isbnField; }
    [[nodiscard]] QLineEdit* publisherField() const { return m_publisherField; }

    /// @brief The sentence under the data of the book: where they change, or why the ISBN is
    /// not one
    [[nodiscard]] QString bookDataNote() const;

    /// @brief Take @p path as the picture of the cover, as Other Picture... does
    void setCoverFile(const QString& path);

    /// @brief The name of the folder of the images of a Markdown file: "<name> – images"
    [[nodiscard]] QString imagesFolderName() const;

protected:
    /// @brief Export the book
    void run() override;

    /// @brief The description, and the name follows the content until the writer types it
    void contentChanged() override;

    /// @brief The lines under the folder and the name, the sentence under the list, and the
    /// accept button
    void updateFields() override;

    /// @brief The data and the cover of the volume chosen in the list Volume
    void bookChanged() override;

private:
    /// @brief Choose the folder in the system window
    void browse();

    /// @brief The fields of the format: the description, the extension, the groups
    void showFormat();

    /// @brief A PDF for a print shop has no comments
    void showPurpose();

    /// @brief The picture and the sentence of the cover of an EPUB
    void showCover();

    /// @brief The data of the book in the tab of an EPUB: its title and author, its language,
    ///        its description, its ISBN and its publisher
    void showBookData();

    /// @brief The design of the front of the cover of the book in the list of the cover, the
    ///        first one, when the book has one
    void showCoverDesign();

    /// @brief Choose the picture of the cover in the system window
    void browseCover();

    /// @brief The program's format chosen in the list
    [[nodiscard]] editor::ExportFormat currentFormat() const;

    /// @brief Keep the choices of the window for the next export
    void rememberChoices() const;

    /// @brief The file name: the name with the extension of the format; empty when the name
    /// is empty
    [[nodiscard]] QString fileName() const;

    /// @brief The extension of the format, with its dot
    [[nodiscard]] QString extension() const;

    /// @brief The name without the extension of either format
    [[nodiscard]] QString baseName() const;

    /// @brief The name the file gets for the content: the title of the book or of the text in
    /// front
    [[nodiscard]] QString suggestedName() const;

    /// @brief Why the name cannot be the name of a file; empty when it can
    [[nodiscard]] QString nameProblem() const;

    /// @brief Why the file could not be written, as the window says it; empty for a cause the
    /// window does not name
    [[nodiscard]] QString reasonOf(const editor::ExportProblem& problem) const;

    Exporter m_exporter;
    QString m_folder;
    bool m_nameTyped = false;  ///< The writer typed the name: it no longer follows the content

    QList<Format> m_formats;                ///< The formats of the list, in its order
    QComboBox* m_formatBox = nullptr;
    QHash<QString, QWidget*> m_formatPages;  ///< The tabs of the options of the plugins' formats
    QLineEdit* m_folderField = nullptr;
    QLineEdit* m_nameField = nullptr;
    FieldNote m_folderNote;
    FieldNote m_nameNote;

    QWidget* m_extrasTab = nullptr;  ///< The tab of the annotations and the images
    QWidget* m_epubLayout = nullptr;  ///< The look of an EPUB and its font
    QRadioButton* m_bookStylesButton = nullptr;
    QRadioButton* m_simpleLookButton = nullptr;
    QCheckBox* m_embedFontBox = nullptr;

    QGroupBox* m_notesGroup = nullptr;
    QCheckBox* m_commentsBox = nullptr;
    QWidget* m_commentsOption = nullptr;  ///< The comments with their remark (Word)
    QCheckBox* m_pdfCommentsBox = nullptr;
    QLabel* m_pdfCommentsRemark = nullptr;
    QWidget* m_pdfCommentsOption = nullptr;  ///< The comments with their remark (PDF)
    QLabel* m_noCommentsNote = nullptr;   ///< Markdown and EPUB have no comments

    QGroupBox* m_imagesGroup = nullptr;
    QCheckBox* m_copyImagesBox = nullptr;
    QWidget* m_copyImagesOption = nullptr;  ///< The copy of the images with its remark (Markdown)
    QLabel* m_imagesInFileNote = nullptr;   ///< A Word document, a PDF and an EPUB have the
                                            ///< images in themselves

    QWidget* m_pdfTab = nullptr;
    QButtonGroup* m_purposeButtons = nullptr;
    QCheckBox* m_bookmarksBox = nullptr;
    QCheckBox* m_facingPagesBox = nullptr;
    QCheckBox* m_grayscaleBox = nullptr;

    QWidget* m_epubTab = nullptr;
    QLabel* m_coverPicture = nullptr;  ///< The cover as the reader shows it
    QComboBox* m_coverBox = nullptr;
    FieldNote m_coverNote;
    QString m_coverFile;  ///< The picture chosen with Other Picture...; empty: none
    QLabel* m_titleAndAuthor = nullptr;
    QLabel* m_languageLabel = nullptr;
    QLabel* m_descriptionLabel = nullptr;
    QLineEdit* m_isbnField = nullptr;
    QLineEdit* m_publisherField = nullptr;
    FieldNote m_bookDataNote;
};

} // namespace gui::dialogs

} // namespace kalahari

#endif  // KALAHARI_GUI_DIALOGS_EXPORT_BOOK_DIALOG_H
