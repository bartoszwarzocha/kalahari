/// @file main_window.h
/// @brief Main application window (Qt6 QMainWindow subclass)
///
/// This file defines the MainWindow class, which is the primary GUI window
/// for Kalahari Writer's IDE. It manages menus, toolbars, status bar, and
/// dockable panels.

#pragma once

#include <QMainWindow>
#include <QAction>
#include <QMenu>
#include <QToolBar>
#include <optional>
#include <filesystem>
#include "kalahari/core/document.h"

class QDockWidget;
class QCloseEvent;
class QShowEvent;

namespace kalahari {

namespace gui {

// Forward declarations for panels
class EditorPanel;
class NavigatorPanel;
class PropertiesPanel;
class SearchPanel;
class AssistantPanel;
class LogPanel;

/// @brief Main application window
///
/// MainWindow is the top-level QMainWindow subclass for Kalahari.
/// It provides:
/// - File and Edit menus
/// - Toolbar with common actions
/// - Status bar
/// - Signal/slot connections for actions
///
/// Example usage:
/// @code
/// QApplication app(argc, argv);
/// MainWindow window;
/// window.show();
/// return app.exec();
/// @endcode
class MainWindow : public QMainWindow {
    Q_OBJECT  // Required for signals/slots!

public:
    /// @brief Constructor
    /// @param parent Parent widget (nullptr for top-level window)
    explicit MainWindow(QWidget* parent = nullptr);

    /// @brief Destructor
    ~MainWindow() override = default;

private:
    /// @brief Create all QAction objects
    ///
    /// Initializes actions for File and Edit menus with:
    /// - Display names
    /// - Keyboard shortcuts
    /// - Status tip text
    /// - Icons (if available)
    void createActions();

    /// @brief Create menu bar with File and Edit menus
    ///
    /// Adds actions to menus and sets up menu structure.
    void createMenus();

    /// @brief Create main toolbar
    ///
    /// Adds File actions (New, Open, Save) to toolbar with icons.
    void createToolbars();

    /// @brief Create status bar
    ///
    /// Shows "Ready" message on application start.
    void createStatusBar();

    /// @brief Create dockable panels
    ///
    /// Creates 6 dock widgets and sets up default layout.
    void createDocks();

    /// @brief Reset dock layout to default
    void resetLayout();

protected:
    /// @brief Save perspective on close
    /// @param event Close event
    void closeEvent(QCloseEvent* event) override;

    /// @brief Restore perspective on show
    /// @param event Show event
    void showEvent(QShowEvent* event) override;

private slots:
    /// @brief Slot for File > New action
    void onNewDocument();

    /// @brief Slot for File > Open action
    void onOpenDocument();

    /// @brief Slot for File > Save action
    void onSaveDocument();

    /// @brief Slot for File > Save As action
    void onSaveAsDocument();

    /// @brief Slot for File > Exit action
    void onExit();

    /// @brief Slot for Edit > Undo action
    void onUndo();

    /// @brief Slot for Edit > Redo action
    void onRedo();

    /// @brief Slot for Edit > Cut action
    void onCut();

    /// @brief Slot for Edit > Copy action
    void onCopy();

    /// @brief Slot for Edit > Paste action
    void onPaste();

    /// @brief Slot for Edit > Select All action
    void onSelectAll();

    /// @brief Slot for Edit > Settings action
    void onSettings();

private:
    // Actions
    QAction* m_newAction;
    QAction* m_openAction;
    QAction* m_saveAction;
    QAction* m_saveAsAction;
    QAction* m_exitAction;

    QAction* m_undoAction;
    QAction* m_redoAction;
    QAction* m_cutAction;
    QAction* m_copyAction;
    QAction* m_pasteAction;
    QAction* m_selectAllAction;
    QAction* m_settingsAction;

    // Menus
    QMenu* m_fileMenu;
    QMenu* m_editMenu;
    QMenu* m_viewMenu;

    // Toolbars
    QToolBar* m_fileToolbar;

    // View actions (panel toggles)
    QAction* m_viewNavigatorAction;
    QAction* m_viewPropertiesAction;
    QAction* m_viewLogAction;
    QAction* m_viewSearchAction;
    QAction* m_viewAssistantAction;
    QAction* m_resetLayoutAction;

    // Dock widgets
    QDockWidget* m_navigatorDock;
    QDockWidget* m_propertiesDock;
    QDockWidget* m_logDock;
    QDockWidget* m_searchDock;
    QDockWidget* m_assistantDock;

    // Panels (widgets inside docks)
    EditorPanel* m_editorPanel;
    NavigatorPanel* m_navigatorPanel;
    PropertiesPanel* m_propertiesPanel;
    SearchPanel* m_searchPanel;
    AssistantPanel* m_assistantPanel;
    LogPanel* m_logPanel;

    // First show flag (for geometry restore)
    bool m_firstShow;

    // Document management (Task #00008 - Phase 0)
    std::optional<core::Document> m_currentDocument;  ///< Current loaded document
    std::filesystem::path m_currentFilePath;          ///< Current .klh file path
    bool m_isDirty;                                   ///< Unsaved changes flag

    /// @brief Mark document as modified (add "*" to title)
    void setDirty(bool dirty);

    /// @brief Update window title with filename and dirty state
    void updateWindowTitle();

    /// @brief Get text from first chapter metadata (Phase 0 temporary hack)
    /// @param doc Document to extract text from
    /// @return Editor text content, or empty string if no content
    QString getPhase0Content(const core::Document& doc) const;

    /// @brief Set text in first chapter metadata (Phase 0 temporary hack)
    /// @param doc Document to update
    /// @param text Editor text content
    void setPhase0Content(core::Document& doc, const QString& text);
};

} // namespace gui
} // namespace kalahari
