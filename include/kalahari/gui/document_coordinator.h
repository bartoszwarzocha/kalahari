/// @file document_coordinator.h
/// @brief Document lifecycle and file operations coordination for MainWindow
///
/// OpenSpec #00038 - Phase 7: Extract Document Operations from MainWindow
/// This class manages document lifecycle, project operations, and archive import/export.

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>
#include <optional>
#include <filesystem>
#include "kalahari/core/document.h"

class QMainWindow;
class QTabWidget;
class QStatusBar;

namespace kalahari {
namespace gui {

class NavigatorPanel;
class PropertiesPanel;
class DashboardPanel;
class NavigatorCoordinator;
class StandaloneInfoBar;
class EditorPanel;

/// @brief Coordinates document lifecycle and file operations
///
/// Manages:
/// - New document/project creation
/// - Open/Save/SaveAs operations
/// - Recent files handling
/// - Standalone file operations
/// - Archive import/export
/// - Project open/close lifecycle
///
/// Example usage:
/// @code
/// auto coordinator = new DocumentCoordinator(
///     mainWindow, centralTabs, navigatorPanel, propertiesPanel,
///     dashboardPanel, navigatorCoordinator, standaloneInfoBar,
///     statusBar,
///     [this]() { return m_isDirty; },
///     [this](bool d) { setDirty(d); },
///     [this]() { updateWindowTitle(); },
///     this
/// );
/// @endcode
class DocumentCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Callback type for checking dirty state
    using DirtyStateGetter = std::function<bool()>;

    /// @brief Callback type for setting dirty state
    using DirtySetter = std::function<void(bool)>;

    /// @brief Callback type for updating window title
    using WindowTitleUpdater = std::function<void()>;

    /// @brief Constructor
    /// @param mainWindow Parent QMainWindow for dialogs
    /// @param centralTabs Central tab widget for editor tabs
    /// @param navigatorPanel Navigator panel for document structure
    /// @param propertiesPanel Properties panel (unused, kept for future)
    /// @param dashboardPanel Dashboard panel for refresh after project changes
    /// @param navigatorCoordinator NavigatorCoordinator for dirty chapter tracking
    /// @param standaloneInfoBar Info bar for standalone files
    /// @param statusBar Status bar for feedback messages
    /// @param isDirty Callback to check if document is dirty
    /// @param setDirty Callback to set dirty state
    /// @param updateTitle Callback to update window title
    /// @param parent Parent QObject
    explicit DocumentCoordinator(QMainWindow* mainWindow,
                                  QTabWidget* centralTabs,
                                  NavigatorPanel* navigatorPanel,
                                  PropertiesPanel* propertiesPanel,
                                  DashboardPanel* dashboardPanel,
                                  NavigatorCoordinator* navigatorCoordinator,
                                  StandaloneInfoBar* standaloneInfoBar,
                                  QStatusBar* statusBar,
                                  DirtyStateGetter isDirty,
                                  DirtySetter setDirty,
                                  WindowTitleUpdater updateTitle,
                                  QObject* parent = nullptr);

    /// @brief Destructor
    ~DocumentCoordinator() override = default;

    // =========================================================================
    // Document state accessors
    // =========================================================================

    /// @brief Get current document (if loaded)
    [[nodiscard]] std::optional<core::Document>& currentDocument() { return m_currentDocument; }
    [[nodiscard]] const std::optional<core::Document>& currentDocument() const { return m_currentDocument; }

    /// @brief Get current file path
    [[nodiscard]] const std::filesystem::path& currentFilePath() const { return m_currentFilePath; }

    /// @brief Set current file path
    void setCurrentFilePath(const std::filesystem::path& path) { m_currentFilePath = path; }

    /// @brief Get list of open standalone file paths
    [[nodiscard]] const QStringList& standaloneFilePaths() const { return m_standaloneFilePaths; }

public slots:
    // =========================================================================
    // Document operations
    // =========================================================================

    /// @brief Create new document
    void onNewDocument();

    /// @brief Create new project
    void onNewProject();

    /// @brief Open document via file dialog
    void onOpenDocument();

    /// @brief Open a recent file
    /// @param filePath Path to the file to open
    void onOpenRecentFile(const QString& filePath);

    /// @brief Save current document
    void onSaveDocument();

    /// @brief Save document with new name
    void onSaveAsDocument();

    /// @brief Save all modified files
    void onSaveAll();

    /// @brief Close current document
    void onCloseDocument();

    // =========================================================================
    // Standalone file operations
    // =========================================================================

    /// @brief Open standalone file via file dialog
    void onOpenStandaloneFile();

    /// @brief Open a specific standalone file
    /// @param path Absolute path to the file
    void openStandaloneFile(const QString& path);

    /// @brief Add current standalone file to project
    void onAddToProject();

    // =========================================================================
    // Archive operations
    // =========================================================================

    /// @brief Export current project to archive
    void onExportArchive();

    /// @brief Import project from archive
    void onImportArchive();

    // =========================================================================
    // Project lifecycle
    // =========================================================================

    /// @brief Handle project opened event
    /// @param projectPath Path to the opened project
    void onProjectOpened(const QString& projectPath);

    /// @brief Handle project closed event
    void onProjectClosed();

signals:
    /// @brief Emitted when a document is opened
    void documentOpened();

    /// @brief Emitted when a document is closed
    void documentClosed();

    /// @brief Emitted when document is modified
    void documentModified();

    /// @brief Emitted when recent files list is updated
    void recentFilesUpdated();

    /// @brief Emitted when window title should be updated
    /// @param title New window title
    void windowTitleChanged(const QString& title);

private:
    /// @brief Ask user to save if document is dirty
    /// @return true if operation should continue, false if cancelled
    bool maybeSave();

    /// @brief Get currently active EditorPanel tab
    /// @return Active EditorPanel or nullptr if not an editor tab
    EditorPanel* getCurrentEditor() const;

    /// @brief Get text from first chapter metadata (Phase 0 temporary hack)
    /// @param doc Document to extract text from
    /// @return Editor text content, or empty string if no content
    QString getPhase0Content(const core::Document& doc) const;

    /// @brief Set text in first chapter metadata (Phase 0 temporary hack)
    /// @param doc Document to update
    /// @param text Editor text content
    void setPhase0Content(core::Document& doc, const QString& text);

    QMainWindow* m_mainWindow;
    QTabWidget* m_centralTabs;
    NavigatorPanel* m_navigatorPanel;
    PropertiesPanel* m_propertiesPanel;
    DashboardPanel* m_dashboardPanel;
    NavigatorCoordinator* m_navigatorCoordinator;
    StandaloneInfoBar* m_standaloneInfoBar;
    QStatusBar* m_statusBar;

    DirtyStateGetter m_isDirty;
    DirtySetter m_setDirty;
    WindowTitleUpdater m_updateWindowTitle;

    /// @brief Current loaded document
    std::optional<core::Document> m_currentDocument;

    /// @brief Current .klh file path
    std::filesystem::path m_currentFilePath;

    /// @brief List of open standalone file paths
    QStringList m_standaloneFilePaths;
};

} // namespace gui
} // namespace kalahari
