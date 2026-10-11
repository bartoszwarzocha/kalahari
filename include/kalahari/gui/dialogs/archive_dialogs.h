/// @file archive_dialogs.h
/// @brief Export Project Archive and Import Project Archive: each in one window, from the
/// choice of the archive to the result of the work

#pragma once

#include "kalahari/core/book_project.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

class QCheckBox;
class QEvent;
class QGridLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QTreeWidget;
class QVBoxLayout;
class QWidget;

namespace kalahari {
namespace core {
class BookTypeRegistry;
} // namespace core

namespace gui {
namespace dialogs {

class RowList;

/// @brief The base of the windows of the archives: the fields, a hint under them, and in the
/// place of the hint the progress of the work, its result or what went wrong
///
/// The accept button does the work. While it lasts, the fields and the buttons wait, and the
/// window cannot be closed. A success shows the result with Show in Folder, and the accept
/// button becomes Close. A failure shows what went wrong, with Copy, and the fields can be
/// changed for another try.
class ArchiveDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief Reports the progress of the work, 0-100
    using Progress = std::function<void(int)>;

    /// @brief The bar of the progress of the work; hidden before the work
    QProgressBar* progressBar() const { return m_progressBar; }

    /// @brief The sentence of the result, or of what went wrong; empty before the work
    QString resultText() const;

    /// @brief What went wrong, one problem a line; empty when nothing did
    QString detailsText() const;

    /// @brief Copy, shown when something went wrong
    QPushButton* copyButton() const { return m_copyButton; }

    /// @brief What Copy puts on the clipboard: the title of the window, the sentence of what
    /// went wrong and the problems
    QString clipboardText() const;

    /// @brief Whether the work is done: the window shows its result, and Close
    bool isDone() const { return m_state == State::Done; }

    /// @brief Show the result in the file manager of the system, as Show in Folder does
    void showResult();

    /// @brief Do the work of the window; when it is done, close the window
    void accept() override;

    /// @brief Close the window without the work, but not while the work lasts
    void reject() override;

protected:
    /// @brief A line under a field: an icon and a sentence, or a remark in grey without an icon
    struct FieldNote {
        QWidget* widget = nullptr;  ///< The line
        QLabel* icon = nullptr;     ///< Its icon
        QLabel* text = nullptr;     ///< Its sentence
    };

    /// @brief Create the window with an empty heading and no fields
    explicit ArchiveDialog(QWidget* parent = nullptr);

    /// @brief The layout of the fields, which wait while the work lasts and stay after it
    QVBoxLayout* fieldsLayout() const { return m_fieldsLayout; }

    /// @brief A group of rows with a title, at the end of the fields: a label at the left of
    /// each field, the labels of all the groups as wide as the widest
    ///
    /// The rows are on a grid: a form layout of more rows of wrapped sentences may give a row
    /// the height its sentence has at another width, which cuts the sentence.
    QGridLayout* addGroup(const QString& title);

    /// @brief Add a row to @p group: label @p label, with a colon, at the left of @p field
    /// @return The label of the row
    QLabel* addRow(QGridLayout* group, const QString& label, QWidget* field);

    /// @brief Add a line under the last row of @p group, under its field; hidden until
    /// showNote() gives it a sentence
    FieldNote addNote(QGridLayout* group);

    /// @brief Show or hide the row of @p field: the field and its label
    void setRowVisible(QWidget* field, bool visible);

    /// @brief Show @p text in line @p note, with the icon @p iconId; without an icon, the line
    /// is a remark in grey; an empty text hides the line
    static void showNote(const FieldNote& note, const QString& text,
                         const QString& iconId = QString());

    /// @brief The sentence of line @p note; empty while it is hidden
    QString noteText(const FieldNote& note) const;

    /// @brief A field of a path that cannot be typed, with Browse... beside it
    /// @param field Gets the field of the path
    /// @param browse Gets Browse..., which the window connects
    /// @return The field and the button, for addRow()
    QWidget* newPathField(QLineEdit** field, QPushButton** browse);

    /// @brief Show @p path in @p field, with its whole path in the tooltip
    static void showPath(QLineEdit* field, const QString& path);

    /// @brief Set the hint under the fields, shown until the work starts
    void setHint(const QString& text);

    /// @brief Do the work of the window; the accept button calls it while it is enabled
    virtual void run() = 0;

    /// @brief The work starts: the fields and the buttons wait, and the bar shows its progress
    void startWork();

    /// @brief The progress of the work, for the function that does it: it moves the bar
    Progress progress();

    /// @brief The work is done: the result, Show in Folder for @p path, and Close
    void finishWork(const QString& text, const QString& path);

    /// @brief The work failed: what went wrong, the problems, Copy, and the fields for another
    /// try
    void failWork(const QString& text, const QStringList& problems);

    /// @brief A change of what the window shows makes it as high as it needs
    bool event(QEvent* event) override;

private:
    /// @brief Make the window on the screen as high as what it shows needs at its width, within
    /// the screen; the window does not get lower
    void fitHeight();

    /// @brief The state of the work
    enum class State {
        Ready,    ///< The fields can be changed; the accept button does the work
        Working,  ///< The work lasts
        Done      ///< The result shows; the accept button closes the window
    };

    State m_state = State::Ready;
    QWidget* m_fields = nullptr;
    QVBoxLayout* m_fieldsLayout = nullptr;
    QList<QLabel*> m_rowLabels;  ///< The labels of the rows of all the groups
    FieldNote m_hint;
    QProgressBar* m_progressBar = nullptr;
    QWidget* m_result = nullptr;
    QLabel* m_resultIcon = nullptr;
    QLabel* m_resultText = nullptr;
    QLabel* m_showLink = nullptr;
    QPlainTextEdit* m_details = nullptr;
    QPushButton* m_copyButton = nullptr;
    QString m_resultPath;
};

/// @brief Export Project Archive: the folder and the name of the archive, the files that are
/// not ready, and the progress and the result of the export in one window
///
/// The window says under the name when the export replaces an archive, and lists the text
/// elements that are not final, which go to the archive as they are. The backups of the
/// database go only when the writer checks it. The changes of the book that are not saved are
/// saved before the export.
class ExportArchiveDialog : public ArchiveDialog {
    Q_OBJECT

public:
    /// @brief A text element of the book that is not final
    struct UnreadyFile {
        QString title;   ///< Title of the element
        QString status;  ///< Its status: "draft", "revision"
        QString iconId;  ///< ArtProvider id of the icon of its kind
    };

    /// @brief What the window says about the book
    struct Book {
        QString title;                ///< Title of the book
        QList<UnreadyFile> unready;   ///< Text elements that are not final, in reading order
        bool unsavedChanges = false;  ///< Whether the book has changes that are not saved
        int backups = 0;              ///< Number of the backups of the database of the book
    };

    /// @brief Writes the archive to @p path, with the backups of the database when
    /// @p withBackups is true; false, with the problems, when it cannot
    using Exporter = std::function<bool(const QString& path, bool withBackups,
                                        const Progress& progress, QStringList* problems)>;

    /// @brief What the window says about the book open in the program, without its changes
    /// that are not saved, which the caller knows
    static Book openBook();

    /// @brief Create the window
    /// @param book What the window says about the book
    /// @param folder Folder of the archive at the start
    /// @param exporter Writes the archive
    /// @param parent Parent widget
    ExportArchiveDialog(const Book& book, const QString& folder, Exporter exporter,
                        QWidget* parent = nullptr);

    /// @brief The folder of the archive
    QString folder() const { return m_folder; }

    /// @brief Set the folder of the archive, as Browse... does
    void setFolder(const QString& folder);

    /// @brief The field of the name of the archive
    QLineEdit* nameField() const { return m_nameField; }

    /// @brief The path of the archive: the folder and the name, which gets ".klh.zip" when it
    /// does not end with it; empty while the archive cannot be written there
    QString archivePath() const;

    /// @brief The line under the folder: why the program cannot use it, or that it creates it
    QString folderNote() const { return noteText(m_folderNote); }

    /// @brief The line under the name: why it cannot be the name of the archive, the name the
    /// archive gets, or that the export replaces an archive
    QString nameNote() const { return noteText(m_nameNote); }

    /// @brief The check box that takes the backups of the database to the archive
    QCheckBox* backupsBox() const { return m_backupsBox; }

    /// @brief The line under the check box of the backups: what they give, or that the book
    /// has none yet
    QString backupsNote() const { return noteText(m_backupsNote); }

    /// @brief The group of the files that are not ready; hidden when there are none
    QGroupBox* unreadyGroup() const { return m_unreadyGroup; }

    /// @brief The list of the files that are not ready
    QTreeWidget* unreadyList() const;

protected:
    /// @brief Export the book
    void run() override;

private:
    /// @brief Choose the folder in the system window
    void browse();

    /// @brief The lines under the folder and the name, and the accept button, for the fields
    void updateFields();

    /// @brief The file name of the archive: the name, with ".klh.zip" when it does not end
    /// with it; empty when the name is empty
    QString fileName() const;

    /// @brief Why the name cannot be the name of a file; empty when it can
    QString nameProblem() const;

    Book m_book;
    Exporter m_exporter;
    QString m_folder;
    QLineEdit* m_folderField = nullptr;
    QLineEdit* m_nameField = nullptr;
    FieldNote m_folderNote;
    FieldNote m_nameNote;
    QCheckBox* m_backupsBox = nullptr;
    FieldNote m_backupsNote;
    QGroupBox* m_unreadyGroup = nullptr;
    RowList* m_unreadyList = nullptr;
};

/// @brief Import Project Archive: the archive and its book, the folder of the imported book,
/// and the progress and the result of the import in one window
///
/// The window reads the title, the author, the type and the content of the book from the
/// archive without unpacking it, and the number of the backups of its database that the
/// archive has. The book goes to a new folder named after the archive. When a book is open,
/// the import first asks whether the imported book takes its place.
class ImportArchiveDialog : public ArchiveDialog {
    Q_OBJECT

public:
    /// @brief Whether the imported book opens: the answer to the question about the open book
    enum class Opening {
        Open,      ///< The imported book opens in the place of the open one
        DontOpen,  ///< The open book stays; the imported one goes to the Dashboard and to
                   ///< Recent Books
        Cancel     ///< Nothing is imported; the window stays as it was
    };

    /// @brief Asks about the open book before book @p title is imported
    using OpeningQuestion = std::function<Opening(const QString& title)>;

    /// @brief Unpacks archive @p archive to a new folder in @p folder and, when @p open is
    /// true, opens its book; false, with the problems, when it cannot
    using Importer = std::function<bool(const QString& archive, const QString& folder, bool open,
                                        const Progress& progress, QStringList* problems)>;

    /// @brief Create the window without an archive
    /// @param registry The book types, for the names of the type and the kinds
    /// @param folder Folder in which the folder of the book is made, at the start
    /// @param importer Unpacks the archive and opens the book
    /// @param parent Parent widget
    ImportArchiveDialog(const core::BookTypeRegistry& registry, const QString& folder,
                        Importer importer, QWidget* parent = nullptr);

    /// @brief Ask @p question about the open book before the import; without a question the
    /// imported book opens
    void setOpeningQuestion(OpeningQuestion question);

    /// @brief The archive; empty before it is chosen
    QString archive() const { return m_archive; }

    /// @brief Read archive @p path and show its book, as Browse... does
    void setArchive(const QString& path);

    /// @brief The folder in which the folder of the book is made
    QString folder() const { return m_folder; }

    /// @brief Set the folder in which the folder of the book is made, as Browse... does
    void setFolder(const QString& folder);

    /// @brief The folder of the imported book: the folder and the name of the archive
    QString bookFolder() const;

    /// @brief The line under the archive: why the program cannot read its book; empty when it
    /// can
    QString archiveNote() const { return noteText(m_archiveNote); }

    /// @brief The line under the folder: where the book goes, or why it cannot go there
    QString folderNote() const { return noteText(m_folderNote); }

    /// @brief The book of the archive as the window shows it, one "Label: value" a line
    QString bookText() const;

    /// @brief What @p elements are, as the window says it: "8 elements: 5 chapters, a
    /// prologue, an epilogue and a title page"
    /// @param registry The book types, for the names of the kinds
    /// @param elements The elements; the kinds of as many elements go in the order of their
    /// first elements
    static QString contentsOf(const core::BookTypeRegistry& registry,
                              const QList<const core::ProjectElement*>& elements);

protected:
    /// @brief Import the archive
    void run() override;

private:
    /// @brief Choose the archive in the system window
    void browseArchive();

    /// @brief Choose the folder in the system window
    void browseFolder();

    /// @brief The lines under the archive and the folder, and the accept button
    void updateFields();

    /// @brief Show the book of the archive in the rows under it
    void showBook();

    /// @brief The title of the book of the archive; the name of the archive when it has none
    QString bookTitle() const;

    /// @brief The name of the type of the book of the archive
    QString typeName() const;

    const core::BookTypeRegistry* m_registry;
    Importer m_importer;
    OpeningQuestion m_openingQuestion;
    QString m_archive;
    QString m_folder;
    std::optional<core::BookProject> m_project;
    QStringList m_archiveProblems;
    QLineEdit* m_archiveField = nullptr;
    QLineEdit* m_folderField = nullptr;
    FieldNote m_archiveNote;
    FieldNote m_folderNote;
    QList<QPair<QLabel*, QLabel*>> m_bookRows;  ///< Label and value of each row of the book
    QLabel* m_titleValue = nullptr;
    QLabel* m_authorValue = nullptr;
    QLabel* m_typeValue = nullptr;
    QLabel* m_contentsValue = nullptr;
    QLabel* m_workshopLabel = nullptr;
    QLabel* m_workshopValue = nullptr;
    QLabel* m_backupsValue = nullptr;
    int m_backups = 0;  ///< Number of the backups of the database in the archive
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
