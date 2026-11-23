// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file import_book_dialog.h
/// @brief File > Import > DOCX, Markdown and Plain Text: the files, the place in the book, the
/// texts they become and how, and the progress and the result of the import in one window

#pragma once

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/editor/book_import.h"
#include "kalahari/gui/book_import_plan.h"
#include "kalahari/gui/dialogs/archive_dialogs.h"
#include "kalahari/gui/widgets/element_place_picker.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <vector>

class QCheckBox;
class QComboBox;
class QEvent;
class QGridLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QShowEvent;
class QSpinBox;
class QStackedWidget;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace kalahari {
namespace gui {
namespace dialogs {

class RowList;

/// @brief Import of Word documents (DOCX), Markdown and plain text into a book
///
/// The command of the menu chooses the format and the files in the system window first; the
/// window shows at once what is in them and the texts they become. The options are on tabs as
/// high as the highest, so the window keeps its size: the files, the place in the book, the
/// division into texts (the list of the texts, their titles and kinds), the styles, the
/// cleanups of the text, and the annotations and the images. The progress and the result show
/// under the tabs, with Show in Navigator, Open First Text and Undo Import. The window keeps
/// its choices in the settings (import.*), each format its own, for the next import.
class ImportBookDialog : public ArchiveDialog {
    Q_OBJECT

   public:
    /// @brief Where the texts go
    enum class Target {
        Book,        ///< The open book, at the place chosen in its list
        NewBook,     ///< A new book
        OtherFiles,  ///< The open book's group Other Files, outside its text
    };

    /// @brief A book type of a new book
    struct BookType {
        QString id;    ///< Its package
        QString name;  ///< Its name in the program's language
    };

    /// @brief What the window says about the open book
    struct Book {
        QString title;                     ///< Its title; empty: no book is open
        QString language;                  ///< Its language code
        QList<PickerSection> sections;     ///< Its parts, for the list of the place
        core::ElementPlace place;          ///< The place the texts go to at first
        ImportKinds kinds;                 ///< The kinds of its texts
        editor::ParagraphStyleSet styles;  ///< Its paragraph styles
        bool readOnly = false;             ///< It cannot be changed: only a new book
    };

    /// @brief A new book
    struct NewBook {
        QString title;
        QString author;
        QString language;  ///< Its language code
        QString typeId;    ///< Its book type
        QString folder;    ///< The folder its folder is made in
    };

    /// @brief The choices of the window
    struct Choice {
        ImportFormat format = ImportFormat::Docx;
        QStringList files;  ///< In the order of the texts
        ImportReading reading;
        Target target = Target::Book;
        core::ElementPlace place;  ///< Target::Book
        NewBook newBook;           ///< Target::NewBook
        bool wholeFiles = false;   ///< Each file one text
        editor::SplitOptions split;
        editor::ImportKmlOptions kml;    ///< How the texts are written, without the sources of
                                         ///< the images and the mapping of the styles
        QHash<QString, QString> styles;  ///< The book's style of each style of the files, by
                                         ///< its name; NEW_STYLE_PREFIX: a new style of the
                                         ///< book; empty: suggestedBookStyle()
    };

    /// @brief What the import added, as the window says it and Undo Import takes it out
    struct Outcome {
        int texts = 0;            ///< The texts it added
        int parts = 0;            ///< The parts it added
        int images = 0;           ///< The image files it wrote
        int styles = 0;           ///< The new styles of the book
        QString firstText;        ///< The element of its first text; empty: none
        QString bookFile;         ///< Of a new book: its .klh file
        bool bookOpened = false;  ///< The new book opened in the window
        ImportedElements added;   ///< What Undo Import takes out
    };

    /// @brief Adds the texts of the checked @p rows of @p files to the book as @p choice says;
    /// false, with what went wrong in @p problem (in the program's language) and how in
    /// @p details, when it cannot
    using Importer = std::function<bool(const Choice& choice, const std::vector<ImportFile>& files,
                                        const std::vector<ImportRow>& rows,
                                        const ImportKinds& kinds, const Progress& progress,
                                        Outcome* outcome, QString* problem, QStringList* details)>;

    /// @brief What the window does with the book after the import
    struct Actions {
        Importer import;
        std::function<bool(const Outcome&)> undo;             ///< Undo Import
        std::function<void(const Outcome&)> showInNavigator;  ///< Show in Navigator
        std::function<void(const Outcome&)> openFirstText;    ///< Open First Text
    };

    /// @brief Create the window
    /// @param book What the window says about the open book
    /// @param registry The installed book types, for the list of the place and a new book
    /// @param format The format of the command of the menu
    /// @param files The files chosen in the system window
    /// @param actions What the window does with the book
    /// @param parent Parent widget
    ImportBookDialog(const Book& book, const core::BookTypeRegistry& registry, ImportFormat format,
                     const QStringList& files, Actions actions, QWidget* parent = nullptr);

    /// @brief Create the window of the format @p plugin of a plugin (File > Import > Other
    ///        Formats)
    /// @param book What the window says about the open book
    /// @param registry The installed book types, for the list of the place and a new book
    /// @param plugin The id of the format (PluginImportFormat::id)
    /// @param files The files chosen in the system window
    /// @param actions What the window does with the book
    /// @param parent Parent widget
    ImportBookDialog(const Book& book, const core::BookTypeRegistry& registry,
                     const QString& plugin, const QStringList& files, Actions actions,
                     QWidget* parent = nullptr);

    ~ImportBookDialog() override;

    /// @brief The choices of the window now
    Choice choice() const;

    /// @brief The book types of a new book, in the program's language
    static QList<BookType> bookTypes(const core::BookTypeRegistry& registry);

    /// @brief The files of the import as the window read them, in the order of the list
    const std::vector<ImportFile>& files() const { return m_files; }

    /// @brief The texts and parts of the files, as the list of the texts shows them
    const std::vector<ImportRow>& rows() const { return m_rows; }

    /// @brief The kinds of the texts of the book the texts go to
    const ImportKinds& kinds() const { return m_kinds; }

    /// @brief Choose the format, as its list does: the files stay in the list, read again
    void setFormat(ImportFormat format);

    /// @brief Choose the format @p id of a plugin, as its list does
    void setPluginFormat(const QString& id);

    /// @brief The id of the format of a plugin chosen in the list; empty: one of the program's
    QString pluginFormat() const;

    /// @brief The files of the list, in place of those it has
    void setFiles(const QStringList& paths);

    /// @brief Add files to the list, as Add... does: each one once, in the order of the names
    void addFiles(const QStringList& paths);

    /// @brief Move the file chosen in the list up (-1) or down (1), as the buttons do
    void moveFile(int step);

    /// @brief Take the file chosen in the list out of it, as Remove from List does
    void removeFile();

    /// @brief Choose where the texts go, as the radio buttons of the place do
    void setTarget(Target target);

    /// @brief Choose the folder a new book is made in, as Browse... does
    void setNewBookFolder(const QString& folder);

    /// @brief The tabs: the files, the place, the division, the styles, the cleanups, and the
    /// annotations and the images
    QTabWidget* tabs() const { return m_tabs; }

    /// @brief The list of the formats
    QComboBox* formatBox() const { return m_formatBox; }

    /// @brief The list of the files
    QTreeWidget* fileList() const;

    /// @brief The list of the encodings of Markdown and plain text
    QComboBox* encodingBox() const { return m_encodingBox; }

    /// @brief The start of the first file as the encoding reads it
    QString previewText() const;

    /// @brief What is in the files, as the window says it under them
    QString contentsText() const;

    /// @brief The line under the files: what could not be read
    QString filesNote() const { return noteText(m_filesNote); }

    /// @brief The radio buttons of the place
    QRadioButton* bookButton() const { return m_bookButton; }
    QRadioButton* newBookButton() const { return m_newBookButton; }
    QRadioButton* otherFilesButton() const { return m_otherFilesButton; }

    /// @brief The place in the open book
    ElementPlacePicker* picker() const { return m_picker; }

    /// @brief The fields of a new book
    QLineEdit* titleField() const { return m_titleField; }
    QLineEdit* authorField() const { return m_authorField; }
    QComboBox* languageBox() const { return m_languageBox; }
    QComboBox* typeBox() const { return m_typeBox; }

    /// @brief The line under the folder of a new book: where it goes, or why it cannot
    QString newBookNote() const { return noteText(m_newBookNote); }

    /// @brief The radio buttons of the division: at the headings (at the titles of plain text),
    /// at the page breaks (after empty lines of plain text), the whole file as one text
    QRadioButton* headingsButton() const { return m_headingsButton; }
    QRadioButton* pagesButton() const { return m_pagesButton; }
    QRadioButton* wholeButton() const { return m_wholeButton; }

    /// @brief The levels of the headings that start a part and a text
    QComboBox* partLevelBox() const { return m_partLevelBox; }
    QComboBox* textLevelBox() const { return m_textLevelBox; }

    /// @brief The patterns of the titles of the texts and the parts of plain text
    QComboBox* titlePatternBox() const { return m_titlePatternBox; }
    QComboBox* partPatternBox() const { return m_partPatternBox; }
    QLineEdit* prefixField() const { return m_prefixField; }

    /// @brief The empty lines that start a text of plain text
    QSpinBox* emptyLinesBox() const { return m_emptyLinesBox; }

    /// @brief Each file as one text (more files)
    QCheckBox* wholeFilesBox() const { return m_wholeFilesBox; }

    /// @brief The heading is the title only
    QCheckBox* headingAsTitleBox() const { return m_headingAsTitleBox; }

    /// @brief The line of the division of a format of a plugin: the plugin gives the texts
    QLabel* pluginNote() const { return m_pluginNote; }

    /// @brief The tab of the options of the format @p id of a plugin; nullptr: none (yet)
    QWidget* pluginPage(const QString& id) const { return m_pluginPages.value(id); }

    /// @brief The list of the texts, with their check boxes, titles, kinds and words
    QTreeWidget* textList() const;

    /// @brief The kind of the text chosen in the list
    QComboBox* kindBox() const { return m_kindBox; }

    /// @brief The sentences under the list: how many texts go, and the kinds the book cannot
    /// have more of
    QString textsNote() const;

    /// @brief Check every text of the list, or none, as the buttons beside it do
    void checkAll();
    void checkNone();

    /// @brief How much of the file's formatting the texts keep
    QComboBox* formattingBox() const { return m_formattingBox; }

    /// @brief The list of the styles of the files, with the book's style of each
    QTreeWidget* styleList() const;

    /// @brief The choice of the book's style of the file's style @p name; nullptr: none
    QComboBox* styleChoice(const QString& name) const;

    /// @brief The paragraphs of plain text: each line, or those an empty line separates
    QRadioButton* linesButton() const { return m_linesButton; }
    QRadioButton* emptyLineButton() const { return m_emptyLineButton; }

    /// @brief The style of the paragraphs of plain text
    QComboBox* textStyleBox() const { return m_textStyleBox; }

    /// @brief The paragraphs that start with a dash in the style Dialogue
    QCheckBox* dialogueBox() const { return m_dialogueBox; }

    /// @brief The check boxes of the cleanups, and what the empty paragraphs become
    QCheckBox* typographyBox() const { return m_typographyBox; }
    QCheckBox* doubleSpacesBox() const { return m_doubleSpacesBox; }
    QCheckBox* leadingSpacesBox() const { return m_leadingSpacesBox; }
    QCheckBox* sceneMarkersBox() const { return m_sceneMarkersBox; }
    QComboBox* emptyBox() const { return m_emptyBox; }

    /// @brief The comments of Word as annotations
    QCheckBox* commentsBox() const { return m_commentsBox; }

    /// @brief The tracked changes rejected
    QRadioButton* rejectButton() const { return m_rejectButton; }

    /// @brief The sentence of the images and the notes of the files
    QString imagesText() const;

    /// @brief Download the images of Internet addresses when the import starts; shown while
    /// the files have them
    QCheckBox* webImagesBox() const { return m_webImagesBox; }

    /// @brief The links under the result
    QLabel* showLink() const { return m_showInNavigatorLink; }
    QLabel* openTextLink() const { return m_openTextLink; }
    QLabel* undoLink() const { return m_undoLink; }

    /// @brief Undo the import, as Undo Import does
    void undoImport();

    /// @brief What the import added; empty before it
    const Outcome& outcome() const { return m_outcome; }

   protected:
    /// @brief Import the texts
    void run() override;

    /// @brief F2 in the list of the texts changes the title of the text, F4 its kind
    bool eventFilter(QObject* watched, QEvent* event) override;

    /// @brief The window opens EXTRA_HEIGHT higher than its content asks for, within the
    /// screen: the lists take the room
    void showEvent(QShowEvent* event) override;

   private:
    /// @brief Create the window of @p format; of ImportFormat::Plugin, of the format @p plugin
    ImportBookDialog(const Book& book, const core::BookTypeRegistry& registry, ImportFormat format,
                     const QString& plugin, const QStringList& files, Actions actions,
                     QWidget* parent);

    /// @brief The tabs of the window: the files, the place, the division, the styles, the
    /// cleanups, and the annotations and the images
    void buildFileTab();
    void buildPlaceTab();
    void buildDivisionTab();
    void buildStylesTab();
    /// @brief The list of the styles takes the height of its tab; without it (plain text) the
    /// paragraphs stay at the top
    void stretchStyles();
    void buildCleanupTab();
    void buildExtrasTab();

    /// @brief The fields of the window follow the writer's changes
    void connectFields();

    /// @brief The format chosen in its list: its choices, its fields and its files read again
    void formatChanged();

    /// @brief The tab of the options of the format of a plugin chosen, made when it is first
    ///        chosen; the tabs of the other formats hidden
    void showPluginPage();

    /// @brief Read the files again, as the format and the reading of plain text say
    void readFiles();

    /// @brief The texts of the files again, as the division says
    void splitFiles();

    /// @brief Show the files in their list, and what is in them
    void showFiles();

    /// @brief The fields of the format: the encoding, the division, the styles, the tabs
    void showFormat();

    /// @brief The fields of the place, and the kinds of the book the texts go to
    void showTarget();

    /// @brief Show the texts in their list
    void showRows();

    /// @brief Show the styles of the files in their list
    void showStyles();

    /// @brief The sentences of the annotations and the images
    void showExtras();

    /// @brief The lines under the fields, the sentences under the list, and the accept button
    void updateFields();

    /// @brief The kind of the text chosen in the list shows in the list of the kinds
    void showKindOfCurrent();

    /// @brief The writer chose a kind for the text chosen in the list
    void kindChosen();

    /// @brief A check box or a title of the list changed
    void rowChanged(QTreeWidgetItem* item, int column);

    /// @brief Choose the files in the system window
    void browseFiles();

    /// @brief Choose the folder of a new book in the system window
    void browseFolder();

    /// @brief The split options of the window
    editor::SplitOptions splitOptions() const;

    /// @brief How the files are read
    ImportReading reading() const;

    /// @brief The settings' prefix of the format: "import.docx."
    std::string settingsPrefix() const;

    /// @brief The writer's last choices of the format
    void loadChoices();

    /// @brief Keep the choices of the window for the next import
    void rememberChoices() const;

    /// @brief The format of the list of the formats
    ImportFormat format() const;

    Book m_book;
    const core::BookTypeRegistry& m_registry;
    Actions m_actions;
    QList<BookType> m_types;
    std::vector<ImportFile> m_files;
    std::vector<ImportRow> m_rows;
    ImportKinds m_kinds;
    editor::ParagraphStyleSet m_styles;  ///< The styles of the book the texts go to
    Outcome m_outcome;
    bool m_filling = false;       ///< The window fills its fields: no reaction to them
    bool m_newBookTyped = false;  ///< The writer typed the title of a new book
    bool m_suggestSplit = true;   ///< The division the files suggest, at the next split
    int m_settingsListener = -1;

    QTabWidget* m_tabs = nullptr;
    QComboBox* m_formatBox = nullptr;
    RowList* m_fileList = nullptr;
    QPushButton* m_upButton = nullptr;
    QPushButton* m_downButton = nullptr;
    QPushButton* m_removeButton = nullptr;
    QComboBox* m_encodingBox = nullptr;
    QLabel* m_previewLabel = nullptr;
    QLabel* m_contentsLabel = nullptr;
    FieldNote m_filesNote;

    QRadioButton* m_bookButton = nullptr;
    QRadioButton* m_newBookButton = nullptr;
    QRadioButton* m_otherFilesButton = nullptr;
    QLabel* m_noBookNote = nullptr;
    QStackedWidget* m_targetPages = nullptr;
    ElementPlacePicker* m_picker = nullptr;
    QWidget* m_newBookPage = nullptr;
    QLineEdit* m_titleField = nullptr;
    QLineEdit* m_authorField = nullptr;
    QComboBox* m_languageBox = nullptr;
    QComboBox* m_typeBox = nullptr;
    QLineEdit* m_folderField = nullptr;
    QString m_newBookFolder;
    FieldNote m_newBookNote;
    QLabel* m_otherFilesNote = nullptr;

    QRadioButton* m_headingsButton = nullptr;
    QRadioButton* m_pagesButton = nullptr;
    QRadioButton* m_wholeButton = nullptr;
    QWidget* m_levels = nullptr;
    QComboBox* m_partLevelBox = nullptr;
    QComboBox* m_textLevelBox = nullptr;
    QWidget* m_patterns = nullptr;
    QComboBox* m_titlePatternBox = nullptr;
    QLineEdit* m_prefixField = nullptr;
    QComboBox* m_partPatternBox = nullptr;
    QWidget* m_emptyLines = nullptr;
    QSpinBox* m_emptyLinesBox = nullptr;
    QCheckBox* m_wholeFilesBox = nullptr;
    QCheckBox* m_headingAsTitleBox = nullptr;
    QLabel* m_pluginNote = nullptr;
    RowList* m_textList = nullptr;
    QComboBox* m_kindBox = nullptr;
    QPushButton* m_renameButton = nullptr;
    QLabel* m_textsNote = nullptr;
    FieldNote m_fullNote;

    QWidget* m_fileStyles = nullptr;
    QComboBox* m_formattingBox = nullptr;
    RowList* m_styleList = nullptr;
    QGridLayout* m_stylesLayout = nullptr;  ///< The grid of the tab Styles
    bool m_grown = false;                   ///< Whether the window got EXTRA_HEIGHT
    QHash<QString, QComboBox*> m_styleChoices;  ///< By the name of the file's style
    QWidget* m_textParagraphs = nullptr;
    QRadioButton* m_linesButton = nullptr;
    QRadioButton* m_emptyLineButton = nullptr;
    QLabel* m_paragraphsNote = nullptr;
    QComboBox* m_textStyleBox = nullptr;
    QCheckBox* m_dialogueBox = nullptr;

    QCheckBox* m_typographyBox = nullptr;
    QCheckBox* m_doubleSpacesBox = nullptr;
    QCheckBox* m_leadingSpacesBox = nullptr;
    QCheckBox* m_sceneMarkersBox = nullptr;
    QComboBox* m_emptyBox = nullptr;

    QWidget* m_stylesTab = nullptr;
    QWidget* m_cleanupTab = nullptr;
    QWidget* m_extrasTab = nullptr;
    /// The tabs of the options of the formats of the plugins, by their ids; nullptr: none. The
    /// tabs own them
    QHash<QString, QWidget*> m_pluginPages;
    int m_tab = 0;  ///< The tab in front, until another one comes in front
    QGroupBox* m_notesGroup = nullptr;
    QCheckBox* m_commentsBox = nullptr;
    QWidget* m_commentsOption = nullptr;
    QLabel* m_noCommentsNote = nullptr;
    QWidget* m_changes = nullptr;
    QLabel* m_changesLabel = nullptr;
    QRadioButton* m_acceptChangesButton = nullptr;
    QRadioButton* m_rejectButton = nullptr;
    QLabel* m_imagesLabel = nullptr;
    QCheckBox* m_webImagesBox = nullptr;
    QWidget* m_webImagesOption = nullptr;
    int m_webImages = 0;  ///< The images of Internet addresses of the files that are read

    QLabel* m_showInNavigatorLink = nullptr;
    QLabel* m_openTextLink = nullptr;
    QLabel* m_undoLink = nullptr;
};

}  // namespace dialogs
}  // namespace gui
}  // namespace kalahari
