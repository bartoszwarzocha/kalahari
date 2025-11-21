/// @file menu_builder.h
/// @brief MenuBuilder class - dynamically builds QMenuBar from CommandRegistry (Qt6)
///
/// @author Claude (AI Assistant)
/// @date 2025-11-21
/// @task #00047 - Command Registry Qt Migration

#pragma once

#include "command_registry.h"
#include <QMenuBar>
#include <QMenu>
#include <QMainWindow>
#include <string>
#include <vector>

namespace kalahari {
namespace gui {

/// @brief Dynamically builds QMenuBar from CommandRegistry
///
/// MenuBuilder provides abstraction layer between CommandRegistry (single source of truth
/// for commands) and Qt menu system. Instead of hardcoding menu creation,
/// MenuBuilder queries CommandRegistry and generates menus dynamically.
///
/// Key benefits:
/// - Single source of truth (CommandRegistry)
/// - Automatic menu structure updates
/// - Plugin-friendly (plugins register commands → menus update automatically)
/// - Maintainable (change command → menus update automatically)
///
/// Example usage:
/// @code
/// MenuBuilder builder;
/// CommandRegistry& registry = CommandRegistry::getInstance();
/// builder.buildMenuBar(registry, this);
/// @endcode
///
/// @note MenuBuilder does NOT own created menus. QMainWindow manages lifetime.
class MenuBuilder {
public:
    /// @brief Default constructor
    MenuBuilder() = default;

    /// @brief Destructor
    ~MenuBuilder() = default;

    // Disable copy and move (stateless class, not needed)
    MenuBuilder(const MenuBuilder&) = delete;
    MenuBuilder& operator=(const MenuBuilder&) = delete;
    MenuBuilder(MenuBuilder&&) = delete;
    MenuBuilder& operator=(MenuBuilder&&) = delete;

    /// @brief Build complete menu bar from CommandRegistry
    ///
    /// Creates QMenuBar with menus: File, Edit, View, Help
    /// Each menu is populated with commands from corresponding category.
    /// Menu structure:
    /// - File: New, Open, Save, Save As, Settings, Exit
    /// - Edit: Undo, Redo, Cut, Copy, Paste, Select All
    /// - View: Panels visibility toggles
    /// - Help: About Kalahari, About Qt
    ///
    /// QAction signals are automatically connected to CommandRegistry::executeCommand().
    ///
    /// @param registry CommandRegistry to read commands from
    /// @param parent Parent window (typically MainWindow)
    void buildMenuBar(CommandRegistry& registry, QMainWindow* parent);

    /// @brief Build single menu from category
    ///
    /// Creates QMenu with title and populates with commands from given category
    /// where showInMenu=true.
    ///
    /// @param title Menu title (e.g., "&File", "&Edit")
    /// @param category Command category (e.g., "File", "Edit")
    /// @param registry CommandRegistry to query
    /// @param parent Parent QObject for menu ownership
    /// @return Populated QMenu (parent owns)
    QMenu* buildMenu(const QString& title,
                     const std::string& category,
                     CommandRegistry& registry,
                     QObject* parent);

private:
    /// @brief Add commands from category to menu
    ///
    /// Queries CommandRegistry for commands in category where showInMenu=true
    /// and creates QActions for each. Automatically handles separators between
    /// logical groups.
    ///
    /// @param menu Target menu (must not be null)
    /// @param commands List of Command values from CommandRegistry
    /// @param registry CommandRegistry reference for signal connections
    void addCommandsToMenu(QMenu* menu,
                           const std::vector<Command>& commands,
                           CommandRegistry& registry);

    /// @brief Create menu action from Command descriptor
    ///
    /// Converts Command struct to QAction with:
    /// - Label (from command.label)
    /// - Icon (from command.icons via toQIcon())
    /// - Shortcut (from command.shortcut)
    /// - Tooltip (from command.tooltip)
    /// - Signal connected to CommandRegistry::executeCommand()
    ///
    /// @param menu Target menu
    /// @param command Command descriptor from CommandRegistry
    /// @param registry CommandRegistry reference for signal connection
    void createMenuAction(QMenu* menu,
                          const Command& command,
                          CommandRegistry& registry);
};

} // namespace gui
} // namespace kalahari
