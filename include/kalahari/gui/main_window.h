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

namespace kalahari {
namespace gui {

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

    // Menus
    QMenu* m_fileMenu;
    QMenu* m_editMenu;

    // Toolbars
    QToolBar* m_fileToolbar;
};

} // namespace gui
} // namespace kalahari
