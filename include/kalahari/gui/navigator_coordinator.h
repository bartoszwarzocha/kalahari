/// @file navigator_coordinator.h
/// @brief Navigator panel interaction coordination for MainWindow
///
/// OpenSpec #00038 - Phase 6: Extract Navigator Handlers from MainWindow
/// This class manages navigator panel signals and coordinates UI responses.

#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <functional>
#include <optional>

class QTabWidget;
class QStatusBar;
class QDockWidget;

namespace kalahari {
namespace core {
    class Document;
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
///     navigatorPanel, propertiesPanel, centralTabs,
///     propertiesDock, statusBar, this);
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
    /// @param propertiesDock Properties dock widget (for show/raise)
    /// @param statusBar Status bar for feedback messages
    /// @param parent Parent QObject
    explicit NavigatorCoordinator(NavigatorPanel* navigatorPanel,
                                   PropertiesPanel* propertiesPanel,
                                   QTabWidget* centralTabs,
                                   QDockWidget* propertiesDock,
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

    /// @brief Clear current element ID (on project close)
    void clearCurrentElement() { m_currentElementId.clear(); }

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
    /// @param elementType Type of element (for confirmation message)
    void onRequestDelete(const QString& elementId, const QString& elementType);

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
    /// @param partId Part ID
    void onRequestPartProperties(const QString& partId);

    /// @brief Handle chapter reorder from navigator drag & drop
    /// @param partId Part containing the chapter
    /// @param fromIndex Original index
    /// @param toIndex New index
    void onChapterReordered(const QString& partId, int fromIndex, int toIndex);

    /// @brief Handle part reorder from navigator drag & drop
    /// @param fromIndex Original index
    /// @param toIndex New index
    void onPartReordered(int fromIndex, int toIndex);

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

    /// @brief Refresh navigator with current document
    void refreshNavigator();

    /// @brief Save current chapter content to disk (OpenSpec #00042 Phase 7.5)
    /// @return true if saved successfully, false on error
    bool saveCurrentChapter();

    /// @brief Show save confirmation dialog for unsaved changes (OpenSpec #00042 Phase 7.5)
    /// @return true if user wants to proceed (saved or discarded), false if cancelled
    bool confirmSaveOrDiscard();

    NavigatorPanel* m_navigatorPanel;
    PropertiesPanel* m_propertiesPanel;
    QTabWidget* m_centralTabs;
    QDockWidget* m_propertiesDock;
    QStatusBar* m_statusBar;

    /// @brief Tracks dirty state per chapter elementId
    QMap<QString, bool> m_dirtyChapters;

    /// @brief Currently active element in editor
    QString m_currentElementId;
};

} // namespace gui
} // namespace kalahari
