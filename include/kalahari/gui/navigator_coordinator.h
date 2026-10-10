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
#include <functional>
#include <optional>

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_registry.h"

class QTabWidget;
class QStatusBar;

namespace kalahari {
namespace core {
    struct ProjectElement;
}

namespace editor {
    class StatisticsCollector;
}

namespace gui {

namespace dialogs {
    enum class NewElementKind;
}

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
    /// Does NOT persist any content. Used by the tab-close discard path.
    void discardChapterChanges(const QString& elementId);

    /// @brief Clear current element ID (on project close)
    void clearCurrentElement() { m_currentElementId.clear(); }

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

public slots:
    /// @brief Handle element selection in navigator
    /// @param elementId Element ID of selected item
    /// @param elementTitle Display title of selected item
    void onElementSelected(const QString& elementId, const QString& elementTitle);

    /// @brief Handle rename request from navigator
    /// @param elementId Element ID to rename
    /// @param currentTitle Current title (for edit dialog)
    void onRequestRename(const QString& elementId, const QString& currentTitle);

    /// @brief Handle delete request from navigator
    /// @param elementId Element ID to delete
    void onRequestDelete(const QString& elementId);

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

    /// @brief Handle an element dragged to another place of its list in the navigator
    /// @param elementId Element ID
    /// @param index Its new index in its list
    void onElementMoved(const QString& elementId, int index);

    /// @brief Handle add chapter request from navigator context menu
    /// @param groupId Group (part) to add the chapter to; empty: the body of the book
    void onRequestAddChapter(const QString& groupId);

    /// @brief Handle add part request from navigator context menu
    void onRequestAddPart();

    /// @brief Handle add item request from navigator context menu (front/back section)
    /// @param sectionType Section type ("front_matter" or "back_matter")
    void onRequestAddItem(const QString& sectionType);

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

    /// @brief Ask for a new element and add it at the end of @p place of the book, or of
    /// group @p groupId
    /// @param dialogKind What the element is, for the dialog
    /// @param kinds Kinds it can have, in the order the dialog offers them
    /// @param current The kind chosen at the start; the first one when it is not one of them
    /// @param groupTitle Title of group @p groupId, for the dialog
    void addElement(dialogs::NewElementKind dialogKind, const QList<core::KindRef>& kinds,
                    const core::KindRef& current, core::BookPlace place,
                    const QString& groupId, const QString& groupTitle);

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
};

} // namespace gui
} // namespace kalahari
