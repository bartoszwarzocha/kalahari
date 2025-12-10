/// @file toolbar_manager.h
/// @brief Centralized toolbar management system (Task #00019)
///
/// Manages multiple toolbars with icons, configuration, and state persistence.
///
/// OpenSpec #00026: Refactored to use ArtProvider::createAction() for
/// self-updating icons. Icons now refresh automatically when theme changes.

#pragma once

#include <QMainWindow>
#include <QToolBar>
#include <QAction>
#include <QMenu>
#include <QMap>
#include <QStringList>
#include <string>
#include <unordered_map>
#include <vector>

namespace kalahari {
namespace gui {

// Forward declaration
class CommandRegistry;

/// @brief Centralized toolbar management system
///
/// ToolbarManager creates and manages multiple toolbars, provides visibility
/// control, and persists toolbar state (positions and visibility) across sessions.
///
/// Features:
/// - Multiple toolbars (File, Edit, Book, View, Tools)
/// - Icon-based toolbar buttons (24x24 icons)
/// - View menu integration (toggle visibility)
/// - State persistence via QSettings
/// - Toolbar movability and floatability
///
/// Example usage:
/// @code
/// ToolbarManager* manager = new ToolbarManager(mainWindow);
/// manager->createToolbars(registry);
/// manager->createViewMenuActions(viewMenu);
/// manager->restoreState();  // Restore from previous session
/// @endcode
class ToolbarManager {
public:
    /// @brief Toolbar configuration descriptor
    struct ToolbarConfig {
        std::string id;                  ///< Unique toolbar ID ("file", "edit")
        std::string label;               ///< Display label ("File Toolbar")
        Qt::ToolBarArea defaultArea;     ///< Default dock area (Qt::TopToolBarArea)
        bool defaultVisible;             ///< Default visibility (true)
        std::vector<std::string> commandIds; ///< Command IDs to show in toolbar
    };

    /// @brief Constructor
    /// @param mainWindow Parent QMainWindow (toolbar owner)
    explicit ToolbarManager(QMainWindow* mainWindow);

    /// @brief Destructor
    ///
    /// ToolbarManager does NOT own toolbars - they are owned by QMainWindow.
    /// Qt parent-child ownership handles cleanup.
    ~ToolbarManager() = default;

    // Disable copy and move (manager class, single instance per MainWindow)
    ToolbarManager(const ToolbarManager&) = delete;
    ToolbarManager& operator=(const ToolbarManager&) = delete;
    ToolbarManager(ToolbarManager&&) = delete;
    ToolbarManager& operator=(ToolbarManager&&) = delete;

    /// @brief Create all toolbars from configuration
    ///
    /// Creates 6 toolbars (File, Edit, Book, View, Tools) and adds them to MainWindow.
    /// Each toolbar is configured with commands from CommandRegistry.
    ///
    /// Toolbar structure:
    /// - File: New, Open, Save, SaveAs, Close
    /// - Edit: Undo, Redo, [SEP], Cut, Copy, Paste, SelectAll
    /// - Book: NewChapter, NewCharacter, NewLocation, BookProperties
    /// - View: Navigator, Properties, Search, Assistant, Log (panel toggles)
    /// - Tools: Spellcheck, WordCount, FocusMode
    ///
    /// @param registry CommandRegistry to retrieve commands from
    /// @note Must be called after CommandRegistry::registerCommand()
    /// @note Toolbars are movable, floatable, and dockable by default
    void createToolbars(CommandRegistry& registry);

    /// @brief Get toolbar by ID
    ///
    /// @param id Toolbar ID ("file", "edit", "book", "view", "tools")
    /// @return QToolBar pointer or nullptr if not found
    QToolBar* getToolbar(const std::string& id);

    /// @brief Show or hide toolbar
    ///
    /// @param id Toolbar ID
    /// @param visible true to show, false to hide
    /// @note Updates corresponding View menu action if it exists
    void showToolbar(const std::string& id, bool visible);

    /// @brief Check if toolbar is visible
    ///
    /// @param id Toolbar ID
    /// @return true if toolbar is visible, false otherwise
    bool isToolbarVisible(const std::string& id);

    /// @brief Save toolbar state to QSettings
    ///
    /// Saves toolbar visibility for each toolbar.
    /// Qt automatically saves toolbar positions via QMainWindow::saveState().
    ///
    /// @note Called from MainWindow::closeEvent()
    void saveState();

    /// @brief Restore toolbar state from QSettings
    ///
    /// Restores toolbar visibility from previous session.
    /// Qt automatically restores toolbar positions via QMainWindow::restoreState().
    ///
    /// @note Called from MainWindow::showEvent() on first show
    void restoreState();

    /// @brief Create View menu actions for toolbar toggles
    ///
    /// Adds "Toolbars" submenu to View menu with checkable actions
    /// for each toolbar. Actions are synced with toolbar visibility.
    ///
    /// @param viewMenu View menu to add actions to
    /// @note Creates separator before "Toolbars" section
    void createViewMenuActions(QMenu* viewMenu);

    // OpenSpec #00026: refreshIcons() method REMOVED
    // Icon refresh is now automatic via ArtProvider::createAction()
    // which connects each action to ArtProvider::resourcesChanged() signal.

    /// @brief Update toolbar icon sizes from ArtProvider
    ///
    /// Called when icon sizes change in Settings. Updates all toolbars
    /// to use the new icon size from ArtProvider::getIconSize().
    ///
    /// @note Call this from MainWindow when ArtProvider::resourcesChanged() is emitted
    void updateIconSizes();

    // ========================================================================
    // Customization API (OpenSpec #00031)
    // ========================================================================

    /// @brief Get custom toolbar configuration
    /// @param toolbarId Toolbar ID (e.g., "main", "edit", "user_xxx")
    /// @return List of command IDs in toolbar
    QStringList getToolbarCommands(const QString& toolbarId) const;

    /// @brief Set custom toolbar configuration
    /// @param toolbarId Toolbar ID
    /// @param commands List of command IDs (use "_SEPARATOR_" for separators)
    void setToolbarCommands(const QString& toolbarId, const QStringList& commands);

    /// @brief Get list of all toolbar IDs
    /// @return List of toolbar IDs (built-in + user-defined)
    QStringList getToolbarIds() const;

    /// @brief Get toolbar display name
    /// @param toolbarId Toolbar ID
    /// @return User-visible toolbar name
    QString getToolbarName(const QString& toolbarId) const;

    /// @brief Check if toolbar is user-defined
    /// @param toolbarId Toolbar ID
    /// @return true if user-created toolbar
    bool isUserToolbar(const QString& toolbarId) const;

    /// @brief Create new user-defined toolbar
    /// @param name Display name for toolbar
    /// @param commands Initial command IDs
    /// @return Generated toolbar ID
    QString createUserToolbar(const QString& name, const QStringList& commands = {});

    /// @brief Delete user-defined toolbar
    /// @param toolbarId Toolbar ID (must be user toolbar)
    /// @return true if deleted, false if not found or not user toolbar
    bool deleteUserToolbar(const QString& toolbarId);

    /// @brief Rename user-defined toolbar
    /// @param toolbarId Toolbar ID
    /// @param newName New display name
    /// @return true if renamed
    bool renameUserToolbar(const QString& toolbarId, const QString& newName);

    /// @brief Rebuild toolbar from current configuration
    /// @param toolbarId Toolbar ID to rebuild
    void rebuildToolbar(const QString& toolbarId);

    /// @brief Reset all toolbars to default configuration
    void resetToDefaults();

    /// @brief Load toolbar configurations from SettingsManager
    void loadConfigurations();

    // ========================================================================
    // Context Menu & Locking API (OpenSpec #00031 - Phase E)
    // ========================================================================

    /// @brief Show context menu at position
    /// @param globalPos Global screen position to show menu at
    void showContextMenu(const QPoint& globalPos);

    /// @brief Set toolbar positions locked/unlocked
    /// @param locked true to lock (prevent moving), false to unlock
    void setToolbarsLocked(bool locked);

    /// @brief Check if toolbars are locked
    /// @return true if toolbars are locked (cannot be moved)
    bool isToolbarsLocked() const;

    /// @brief Save toolbar configurations to SettingsManager
    void saveConfigurations();

private:
    /// @brief Initialize toolbar configurations
    ///
    /// Defines 6 toolbar configurations with command lists.
    void initializeConfigs();

    /// @brief Create single toolbar from configuration
    ///
    /// @param config Toolbar configuration
    /// @param registry CommandRegistry to retrieve commands
    /// @return Created QToolBar
    QToolBar* createToolbar(const ToolbarConfig& config, CommandRegistry& registry);

    /// @brief Slot called when View menu toolbar action is toggled
    ///
    /// @param checked New checked state
    /// @note This is connected to View menu actions
    void onViewActionToggled(bool checked);

    QMainWindow* m_mainWindow;  ///< Parent main window
    std::unordered_map<std::string, QToolBar*> m_toolbars;       ///< Toolbars by ID
    std::unordered_map<std::string, ToolbarConfig> m_configs;    ///< Toolbar configs
    std::unordered_map<std::string, QAction*> m_viewActions;     ///< View menu actions

    // OpenSpec #00031: Customization support
    QMap<QString, QStringList> m_toolbarCommands;    ///< Toolbar configurations (toolbar ID -> command list)
    QMap<QString, QString> m_userToolbarNames;       ///< User toolbar names (toolbar ID -> display name)
    QMap<QString, QStringList> m_defaultConfigs;     ///< Default configurations for reset

    // OpenSpec #00031 - Phase E: Toolbar locking
    bool m_toolbarsLocked = false;                   ///< Whether toolbar positions are locked
};

} // namespace gui
} // namespace kalahari
