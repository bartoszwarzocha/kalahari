/// @file navigator_coordinator.h
/// @brief Navigator panel interaction coordination for MainWindow
///
/// OpenSpec #00038 - Phase 6: Extract Navigator Handlers from MainWindow
/// This class manages navigator panel signals and coordinates UI responses.

#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QList>
#include <QSet>
#include <functional>
#include <optional>

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/gui/add_target.h"

class QTabWidget;
class QStatusBar;
class QTimer;

namespace kalahari {
namespace core {
    struct ProjectElement;
    struct UnassignedFile;
}

namespace editor {
    class StatisticsCollector;
}

namespace gui {

class NavigatorPanel;
class PropertiesPanel;
class EditorPanel;

/// @brief Coordinates navigator panel interactions
///
/// Manages:
/// - Element selection (opening chapters in editor tabs)
/// - Rename, delete, move operations
/// - Properties display (element, section, part)
/// - Drag & drop reordering
/// - Per-chapter dirty state tracking
///
/// Example usage:
/// @code
/// auto coordinator = new NavigatorCoordinator(
///     navigatorPanel, propertiesPanel, centralTabs, statusBar, this);
/// connect(coordinator, &NavigatorCoordinator::documentModified,
///         this, &MainWindow::onDocumentModified);
/// @endcode
class NavigatorCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param navigatorPanel Navigator panel instance
    /// @param propertiesPanel Properties panel instance
    /// @param centralTabs Central tab widget for editor tabs
    /// @param statusBar Status bar for feedback messages
    /// @param parent Parent QObject
    explicit NavigatorCoordinator(NavigatorPanel* navigatorPanel,
                                   PropertiesPanel* propertiesPanel,
                                   QTabWidget* centralTabs,
                                   QStatusBar* statusBar,
                                   QObject* parent = nullptr);

    /// @brief Destructor
    ~NavigatorCoordinator() override = default;

    /// @brief Get the current element ID being edited
    [[nodiscard]] QString currentElementId() const { return m_currentElementId; }

    /// @brief Get dirty state for a chapter
    /// @param elementId Element ID to check
    /// @return true if chapter has unsaved changes
    [[nodiscard]] bool isChapterDirty(const QString& elementId) const;

    /// @brief Get all dirty chapter states
    [[nodiscard]] const QMap<QString, bool>& dirtyChapters() const { return m_dirtyChapters; }

    /// @brief Set dirty state for a chapter
    /// @param elementId Element ID
    /// @param dirty Dirty state
    void setChapterDirty(const QString& elementId, bool dirty);

    /// @brief Clear all dirty chapter states
    void clearDirtyChapters();

    /// @brief Discard unsaved changes for a single chapter
    /// @param elementId Element ID whose changes should be discarded
    ///
    /// Clears the chapter dirty state everywhere so nothing goes stale:
    /// - the unsaved text ProjectManager keeps (single source of truth)
    /// - the m_dirtyChapters display cache
    /// - the navigator "*" indicator (via chapterDirtyStateChanged)
    /// Does NOT persist any content. Used by the tab-close discard path, and when the text
    /// of the chapter's tab is the saved text again.
    void discardChapterChanges(const QString& elementId);

    /// @brief Clear current element ID (on project close)
    void clearCurrentElement() { m_currentElementId.clear(); }

    /// @brief Give ProjectManager the words of the chapters changed in their tabs since they
    /// were counted, for the counts of the book, and show them in the Properties panel
    ///
    /// A change in a chapter's editor does it a moment after the last change.
    void countWords();

    /// @brief Refresh navigator with the open project, keeping its expanded items
    void refreshNavigator();

    /// @brief Show the title of element @p elementId on its open tab, with the "*" of its
    /// unsaved changes
    void refreshTabTitle(const QString& elementId);

    // =========================================================================
    // Statistics Integration (OpenSpec #00042 Task 7.7)
    // =========================================================================

    /// @brief Set the statistics collector for new editor panels
    /// @param collector Pointer to shared StatisticsCollector (nullptr to disable)
    ///
    /// When set, new EditorPanel instances created for chapters will be connected
    /// to this collector for statistics tracking.
    void setStatisticsCollector(editor::StatisticsCollector* collector) { m_statisticsCollector = collector; }

    /// @brief Get the current statistics collector
    /// @return Pointer to StatisticsCollector, or nullptr if not set
    editor::StatisticsCollector* statisticsCollector() const { return m_statisticsCollector; }

    /// @brief What New File made with a book open
    struct NewFile {
        /// What it is
        enum class Made {
            Nothing,      ///< The writer cancelled, or it could not be made
            Element,      ///< An element of the book, open in the editor
            BookFile,     ///< An empty text file of the book outside its text, open in the editor
            OutsideFile,  ///< An empty text file outside the book, which the caller opens
        };
        Made made = Made::Nothing;
        QString title;  ///< Its title
    };

public slots:
    /// @brief Handle element selection in navigator
    /// @param elementId Element ID of selected item
    /// @param elementTitle Display title of selected item
    void onElementSelected(const QString& elementId, const QString& elementTitle);

    /// @brief Handle rename request from navigator
    /// @param elementId Element ID to rename
    /// @param currentTitle Current title (for edit dialog)
    void onRequestRename(const QString& elementId, const QString& currentTitle);

    /// @brief Delete an element from the book, with the elements inside it, after asking
    ///
    /// The window lists what goes and says where the files go: by default they stay in the
    /// book's folder, with the changes of their open tabs, and the Navigator lists them under
    /// "Unassigned Files"; the writer can delete them too, to the system's Trash, or for good
    /// from a folder without one.
    /// @param elementId Element ID to delete
    void onRequestDelete(const QString& elementId);

    /// @brief Show the text of a file of "Unassigned Files", read-only; from there the writer
    /// can add it to the book or delete it
    /// @param file The file, relative to the project folder
    void onRequestPreviewUnassigned(const QString& file);

    /// @brief Add a file of "Unassigned Files" to the book, as a new element of the kind, with
    /// the title and at the place the writer chooses; it takes the status and the notes of
    /// the file
    /// @param file The file, relative to the project folder
    void onRequestAddUnassigned(const QString& file);

    /// @brief Delete a file of "Unassigned Files" after asking: to the system's Trash, or for
    /// good from a folder without one
    /// @param file The file, relative to the project folder
    void onRequestDeleteUnassigned(const QString& file);

    /// @brief Delete all files of "Unassigned Files" after asking
    void onRequestDeleteAllUnassigned();

    /// @brief Handle move request from navigator
    /// @param elementId Element ID to move
    /// @param direction -1 for up, +1 for down
    void onRequestMove(const QString& elementId, int direction);

    /// @brief Handle properties request from navigator
    /// @param elementId Element ID (empty for document properties)
    void onRequestProperties(const QString& elementId);

    /// @brief Handle section properties request from navigator
    /// @param sectionType Section type ("section_frontmatter", "section_body", "section_backmatter")
    void onRequestSectionProperties(const QString& sectionType);

    /// @brief Handle part properties request from navigator
    /// @param partId Group (part) ID
    void onRequestPartProperties(const QString& partId);

    /// @brief Move an element to another place of its list: Move to Start, Move to End
    /// @param elementId Element ID
    /// @param index Its new index in its list
    void onElementMoved(const QString& elementId, int index);

    /// @brief Ask for a new place of an element in the book in the Move dialog, and move it
    /// there with the elements inside it; saves the .klh file
    /// @param elementId Element ID
    void onRequestMoveTo(const QString& elementId);

    /// @brief Move an element dragged in the Navigator to another place of the book, with the
    /// elements inside it; saves the .klh file
    /// @param elementId Element ID
    /// @param place Its new place, in the book without it
    void onElementMovedTo(const QString& elementId, const kalahari::core::ElementPlace& place);

    /// @brief Add an element in the Add Element window, with the kinds of @p target, the
    /// place of the Navigator or of the editor it was opened on; saves the .klh file
    ///
    /// The new element is selected in the Navigator; a text element opens in the editor with
    /// the cursor at the start of its text, and a material is a copy of the file the writer
    /// chooses.
    void onRequestAdd(const kalahari::gui::AddTarget& target);

    /// @brief Book > Add...: Add Element for what is in front (addTargetHere())
    void onRequestAddHere();

    /// @brief Book > New Chapter...: Add Element for what is in front with the main text kind
    /// of the book chosen; the whole book when the list of the element in front cannot have it
    void onRequestNewChapter();

    /// @brief Where Book > Add... adds: right after the element of the editor's tab in front,
    /// with its kind (in the Workshop, in the group of its kind); the whole book for any other
    /// tab and for the Dashboard
    AddTarget addTargetHere() const;

    /// @brief Ask for a new name of the node of the book or of the Workshop and give it; saves
    /// the .klh file
    ///
    /// The default name ("Book", "Workshop") is no name of the writer's own: the node keeps
    /// following the language of the program.
    /// @param itemType "book" or "workshop"
    void onRequestRenameNode(const QString& itemType);

    /// @brief Group the Workshop (Libraries and Resources) or not; saves the .klh file
    void onRequestWorkshopGrouped(bool grouped);

    /// @brief Open material @p elementId of the Workshop as the settings say
    /// (NavigatorPanel::OPEN_MATERIALS_SETTING): a picture on its preview tab or in the
    /// program the system gives its file; another file in the system program
    void onRequestOpenMaterial(const QString& elementId);

    /// @brief Show the preview of material @p elementId on a tab of its own beside the
    /// chapters, or bring its tab to the front
    void onRequestPreview(const QString& elementId);

    /// @brief Open material @p elementId in the program the system gives its file; the writer
    /// is told when it cannot be opened
    void onRequestOpenWithSystem(const QString& elementId);

    /// @brief Open what link @p link of a material names: a text in the editor, a material as
    /// the settings say; a section or another element is shown in the Navigator
    void onRequestOpenLink(const QString& link);

    /// @brief New File with a book open: a new element of the kind, with the title and at the
    /// place the writer chooses in the New File window, or an empty text file of the book
    /// outside its text (in "Other Files"), which opens in the editor; saves the .klh file. An
    /// empty text file outside the book is the caller's to open in a tab of its own.
    /// @param untitled Title of an empty text file the window proposes
    NewFile onRequestNewFile(const QString& untitled);

    /// @brief Show the sections of the book in the Navigator, or hide them; saves the .klh
    /// file
    /// @param shown Whether the Navigator shows the sections
    void onRequestShowSections(bool shown);

    /// @brief Show the sections of the book in the Navigator or not, and name them; saves the
    /// .klh file, and refreshes the Navigator and the Properties panel
    /// @param sections Whether the Navigator shows the sections, and their names
    void onRequestSections(const kalahari::core::BookSections& sections);

    /// @brief Ask for a new name of a section of the book and give it; saves the .klh file
    ///
    /// The section gets the writer's own name, and the other two keep the names they have.
    /// @param sectionType "section_frontmatter", "section_body" or "section_backmatter"
    void onRequestRenameSection(const QString& sectionType);

signals:
    /// @brief Emitted when an element is selected/opened
    /// @param elementId Element ID that was opened
    void elementOpened(const QString& elementId);

    /// @brief Emitted when document is modified (needs save)
    void documentModified();

    /// @brief Emitted when navigator should be refreshed
    void refreshNavigatorRequested();

    /// @brief Emitted when chapter dirty state changes (OpenSpec #00042 Phase 7.5)
    /// @param elementId Element ID of the chapter
    /// @param isDirty True if chapter has unsaved changes
    void chapterDirtyStateChanged(const QString& elementId, bool isDirty);

private:
    /// @brief Get currently active EditorPanel tab
    /// @return Active EditorPanel or nullptr if not an editor tab
    EditorPanel* getCurrentEditor() const;

    /// @brief Close the editor tabs of @p element and of the elements inside it
    void closeTabsOf(const core::ProjectElement& element);

    /// @brief The tab @p editor of element @p elementId and the Navigator show whether its
    /// text differs from the saved text; back to the saved text, it has no changes to keep
    void showChapterModified(const QString& elementId, EditorPanel* editor, bool modified);

    /// @brief Write the unsaved text of @p element and of the elements inside it to their
    /// chapter files, with the changes of their open tabs; a tab whose text is written no
    /// longer shows changes. The files get the titles, the statuses and the notes of the
    /// elements, for the files that stay when the elements leave the book.
    /// @return The elements whose text could not be written: @p element or elements inside it
    QList<const core::ProjectElement*> saveTextsOf(const core::ProjectElement& element);

    /// @brief Delete @p files: to the system's Trash when @p trash, else for good
    ///
    /// When the Trash does not take a file, the writer chooses whether it goes for good; the
    /// writer is told about the files that could not be deleted.
    /// @param forGood Set to whether a file was deleted for good; nullptr: not wanted
    /// @return The files that are still on the disk
    QStringList deleteFiles(const QStringList& files, bool trash, bool* forGood = nullptr);

    /// @brief Ask whether to delete @p files of "Unassigned Files", and delete them
    void deleteUnassigned(const QList<core::UnassignedFile>& files);

    /// @brief Move element @p elementId to @p place of the book without it, and show it there;
    /// the writer is told when the book cannot be saved
    void moveElementTo(const QString& elementId, const core::ElementPlace& place);

    /// @brief The new element @p elementId with @p title is shown in the Navigator; a text
    /// element opens in the editor with the cursor at the start of its text
    void showAdded(const QString& elementId, const QString& title);

    NavigatorPanel* m_navigatorPanel;
    PropertiesPanel* m_propertiesPanel;
    QTabWidget* m_centralTabs;
    QStatusBar* m_statusBar;

    /// @brief Tracks dirty state per chapter elementId
    QMap<QString, bool> m_dirtyChapters;

    /// @brief Currently active element in editor
    QString m_currentElementId;

    /// @brief Statistics collector for editor panels (OpenSpec #00042 Task 7.7)
    editor::StatisticsCollector* m_statisticsCollector{nullptr};

    /// @brief Counts the words of the changed chapters a moment after the last change
    QTimer* m_wordCountTimer{nullptr};

    /// @brief Chapters changed since their words were counted
    QSet<QString> m_uncountedChapters;
};

} // namespace gui
} // namespace kalahari
