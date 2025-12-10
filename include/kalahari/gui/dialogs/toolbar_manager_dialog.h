/// @file toolbar_manager_dialog.h
/// @brief Visual Studio-style toolbar customization dialog
///
/// ToolbarManagerDialog provides a 3-column interface for managing toolbars:
/// - Left: List of toolbars (built-in, user-defined, plugin)
/// - Center: Available commands from CommandRegistry
/// - Right: Commands in selected toolbar with reordering
///
/// OpenSpec #00031: Toolbar System

#pragma once

#include <QDialog>
#include <QListWidget>
#include <QTreeWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QMap>
#include <QString>
#include <QStringList>

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief Constants for toolbar management
namespace ToolbarConstants {
    /// @brief Separator marker in command lists
    static const QString SEPARATOR_MARKER = "_SEPARATOR_";

    /// @brief Prefix for user-created toolbars
    static const QString USER_TOOLBAR_PREFIX = "user_";

    /// @brief Prefix for plugin-created toolbars
    static const QString PLUGIN_TOOLBAR_PREFIX = "plugin_";
}

/// @brief Visual Studio-style dialog for managing toolbar customization
///
/// Features:
/// - View and select from built-in, user, and plugin toolbars
/// - Browse available commands by category
/// - Add/remove commands from toolbars
/// - Reorder commands with drag & drop or buttons
/// - Create new user-defined toolbars
/// - Reset toolbars to defaults
///
/// The dialog uses a 3-column layout:
/// 1. Left: Toolbar list with sections (Built-in, User, Plugin)
/// 2. Center: Available commands tree with category filter and search
/// 3. Right: Current toolbar commands with ordering controls
///
/// @see ToolbarManager
/// @see CommandRegistry
class ToolbarManagerDialog : public QDialog {
    Q_OBJECT

public:
    /// @brief Constructs the toolbar manager dialog
    /// @param parent Parent widget (typically MainWindow)
    explicit ToolbarManagerDialog(QWidget* parent = nullptr);

    /// @brief Destructor
    ~ToolbarManagerDialog() override = default;

private slots:
    // ========================================================================
    // Toolbar Selection
    // ========================================================================

    /// @brief Handle toolbar selection in left panel
    /// @param current Currently selected item
    /// @param previous Previously selected item
    void onToolbarSelected(QListWidgetItem* current, QListWidgetItem* previous);

    // ========================================================================
    // Command Operations
    // ========================================================================

    /// @brief Handle double-click on available command to add it
    /// @param item Double-clicked tree item
    /// @param column Column index (ignored)
    void onCommandDoubleClicked(QTreeWidgetItem* item, int column);

    /// @brief Add selected command to current toolbar
    void onAddCommand();

    /// @brief Remove selected command from current toolbar
    void onRemoveCommand();

    /// @brief Move selected command up in toolbar
    void onMoveUp();

    /// @brief Move selected command down in toolbar
    void onMoveDown();

    /// @brief Add separator to current toolbar
    void onAddSeparator();

    // ========================================================================
    // Filtering
    // ========================================================================

    /// @brief Handle category filter change
    /// @param index New combo box index
    void onCategoryChanged(int index);

    /// @brief Handle search text change
    /// @param text New search text
    void onSearchTextChanged(const QString& text);

    // ========================================================================
    // Toolbar Management
    // ========================================================================

    /// @brief Create new user-defined toolbar
    void onNewToolbar();

    /// @brief Delete selected user toolbar
    void onDeleteToolbar();

    /// @brief Rename selected user toolbar
    void onRenameToolbar();

    // ========================================================================
    // Dialog Actions
    // ========================================================================

    /// @brief Apply current changes without closing
    void onApply();

    /// @brief Reset all toolbars to default configuration
    void onReset();

    /// @brief Handle OK button (apply and close)
    void onAccept();

    // ========================================================================
    // State Updates
    // ========================================================================

    /// @brief Update button enable states based on selection
    void updateButtonStates();

    /// @brief Handle current toolbar item selection change
    void onCurrentToolbarSelectionChanged();

private:
    // ========================================================================
    // UI Setup
    // ========================================================================

    /// @brief Create and configure all UI elements
    void setupUI();

    /// @brief Create connections between signals and slots
    void createConnections();

    /// @brief Create the left panel (toolbar list)
    /// @return Widget containing toolbar list
    QWidget* createToolbarListPanel();

    /// @brief Create the center panel (available commands)
    /// @return Widget containing command tree
    QWidget* createAvailableCommandsPanel();

    /// @brief Create the right panel (current toolbar)
    /// @return Widget containing toolbar commands
    QWidget* createCurrentToolbarPanel();

    // ========================================================================
    // Data Loading
    // ========================================================================

    /// @brief Load toolbar configurations from ToolbarManager
    void loadToolbarConfigs();

    /// @brief Save toolbar configurations to ToolbarManager
    void saveToolbarConfigs();

    /// @brief Populate toolbar list (left panel)
    void populateToolbarList();

    /// @brief Populate available commands tree (center panel)
    void populateAvailableCommands();

    /// @brief Populate current toolbar list (right panel)
    /// @param toolbarId ID of toolbar to display
    void populateCurrentToolbar(const QString& toolbarId);

    /// @brief Filter available commands based on category and search
    void filterAvailableCommands();

    // ========================================================================
    // Helper Methods
    // ========================================================================

    /// @brief Get sanitized toolbar ID from display name
    /// @param name User-entered toolbar name
    /// @return Sanitized ID suitable for use as key
    QString sanitizeToolbarName(const QString& name) const;

    /// @brief Check if toolbar is a built-in toolbar
    /// @param toolbarId Toolbar ID to check
    /// @return true if built-in, false otherwise
    bool isBuiltInToolbar(const QString& toolbarId) const;

    /// @brief Check if toolbar is a plugin toolbar
    /// @param toolbarId Toolbar ID to check
    /// @return true if plugin toolbar, false otherwise
    bool isPluginToolbar(const QString& toolbarId) const;

    /// @brief Check if toolbar is a user-defined toolbar
    /// @param toolbarId Toolbar ID to check
    /// @return true if user toolbar, false otherwise
    bool isUserToolbar(const QString& toolbarId) const;

    /// @brief Mark dialog as modified
    void setModified(bool modified);

    // ========================================================================
    // UI Widgets
    // ========================================================================

    /// @brief Left panel - list of toolbars
    QListWidget* m_toolbarList;

    /// @brief Center panel - category filter dropdown
    QComboBox* m_categoryCombo;

    /// @brief Center panel - search filter input
    QLineEdit* m_searchFilter;

    /// @brief Center panel - available commands tree
    QTreeWidget* m_availableCommands;

    /// @brief Right panel - current toolbar commands
    QListWidget* m_currentToolbar;

    /// @brief Move command up button
    QPushButton* m_moveUpBtn;

    /// @brief Move command down button
    QPushButton* m_moveDownBtn;

    /// @brief Remove command button
    QPushButton* m_removeBtn;

    /// @brief Add separator button
    QPushButton* m_separatorBtn;

    /// @brief Add command button (center panel)
    QPushButton* m_addCommandBtn;

    /// @brief New toolbar button
    QPushButton* m_newToolbarBtn;

    /// @brief Delete toolbar button
    QPushButton* m_deleteToolbarBtn;

    /// @brief Rename toolbar button
    QPushButton* m_renameToolbarBtn;

    /// @brief Reset to defaults button
    QPushButton* m_resetBtn;

    /// @brief Standard dialog buttons (Apply, OK, Cancel)
    QDialogButtonBox* m_buttonBox;

    // ========================================================================
    // State
    // ========================================================================

    /// @brief Currently selected toolbar ID
    QString m_selectedToolbarId;

    /// @brief Pending changes (toolbar ID -> command list)
    QMap<QString, QStringList> m_pendingChanges;

    /// @brief Original toolbar configurations (for reset)
    QMap<QString, QStringList> m_originalConfigs;

    /// @brief Toolbar display names (toolbar ID -> display name)
    QMap<QString, QString> m_toolbarNames;

    /// @brief Track modification state
    bool m_modified;

    /// @brief Built-in toolbar IDs
    QStringList m_builtInToolbarIds;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
