/// @file menu_builder.h
/// @brief MenuBuilder class - dynamically builds QMenuBar from CommandRegistry (Qt6)
///
/// Task #00047 - Command Registry Qt Migration
///
/// OpenSpec #00026: Refactored to use ArtProvider::createAction() for
/// self-updating icons. Icons now refresh automatically when theme changes.

#pragma once

#include "command_registry.h"
#include <QMenuBar>
#include <QMenu>
#include <QMainWindow>
#include <string>
#include <vector>

namespace kalahari {
namespace gui {

/// @brief Dynamically builds QMenuBar from CommandRegistry with hierarchical menu support
///
/// MenuBuilder provides abstraction layer between CommandRegistry and Qt menu system.
/// Supports hierarchical menus via menuPath (e.g., "FILE/Import/DOCX Document..."),
/// automatic separator insertion, and dynamic submenu providers.
///
/// Key features:
/// - **Hierarchical menus:** FILE > Import > DOCX
/// - **Automatic ordering:** menuOrder (10, 20, 30...) allows insertion
/// - **Separators:** addSeparatorAfter for visual grouping
/// - **Dynamic submenu:** Recent Books, Perspectives, Panels
/// - **Phase markers:** QMessageBox "Coming in Phase X" for unimplemented features
///
/// Example usage:
/// @code
/// MenuBuilder builder;
/// CommandRegistry& registry = CommandRegistry::getInstance();
/// builder.buildMenuBar(registry, this);
///
/// // Register dynamic submenu
/// builder.registerDynamicMenu("FILE/Recent Books", [this]() {
///     return getRecentBooksActions();
/// });
/// @endcode
class MenuBuilder {
public:
    /// @brief Dynamic submenu provider function
    ///
    /// Returns list of QActions to insert dynamically at runtime.
    /// Provider is called each time menu is shown (for fresh data).
    using DynamicMenuProvider = std::function<std::vector<QAction*>()>;

    /// @brief Default constructor
    MenuBuilder() = default;

    /// @brief Destructor
    ~MenuBuilder() = default;

    // Disable copy and move
    MenuBuilder(const MenuBuilder&) = delete;
    MenuBuilder& operator=(const MenuBuilder&) = delete;
    MenuBuilder(MenuBuilder&&) = delete;
    MenuBuilder& operator=(MenuBuilder&&) = delete;

    /// @brief Build complete menu bar from CommandRegistry
    ///
    /// Parses all commands' menuPath fields and builds hierarchical menu structure.
    /// Example:
    /// - menuPath="FILE/Import/DOCX Document..." → FILE menu → Import submenu → DOCX action
    /// - menuPath="VIEW/Panels/Navigator" → VIEW menu → Panels submenu → Navigator action
    ///
    /// Commands sorted by menuOrder within each submenu. Separators inserted where
    /// addSeparatorAfter=true.
    ///
    /// @param registry CommandRegistry to read commands from
    /// @param parent Parent window (typically MainWindow)
    void buildMenuBar(CommandRegistry& registry, QMainWindow* parent);

    /// @brief Register dynamic submenu provider
    ///
    /// Registers function to provide dynamic menu items at specified path.
    /// Provider called when menu shown (for up-to-date data).
    ///
    /// Example:
    /// @code
    /// builder.registerDynamicMenu("FILE/Recent Books", [this]() {
    ///     std::vector<QAction*> actions;
    ///     for (const auto& book : m_recentBooks) {
    ///         QAction* action = new QAction(QString::fromStdString(book.name));
    ///         connect(action, &QAction::triggered, [book]() { openBook(book.path); });
    ///         actions.push_back(action);
    ///     }
    ///     return actions;
    /// });
    /// @endcode
    ///
    /// @param menuPath Full path to dynamic submenu (e.g., "FILE/Recent Books")
    /// @param provider Function returning vector of QActions
    void registerDynamicMenu(const std::string& menuPath, DynamicMenuProvider provider);

    /// @brief Update all dynamic submenus
    ///
    /// Calls all registered providers and refreshes dynamic menu content.
    /// Useful after data changes (e.g., Recent Books list updated).
    void updateDynamicMenus();

    /// @brief Get menu by technical name (i18n-safe)
    ///
    /// Returns cached menu by technical name (e.g., "VIEW", "FILE").
    /// Use this instead of searching by translated text!
    ///
    /// @param technicalName Technical menu name (e.g., "VIEW", "FILE", "EDIT")
    /// @return QMenu pointer or nullptr if not found
    QMenu* getMenu(const std::string& technicalName) const;

    // OpenSpec #00026: refreshIcons() method REMOVED
    // Icon refresh is now automatic via ArtProvider::createAction()
    // which connects each action to ArtProvider::resourcesChanged() signal.

private:
    /// @brief Parse menuPath and build hierarchical structure
    ///
    /// Recursively creates QMenu/submenu hierarchy from menuPath strings.
    /// Example: "FILE/Import/DOCX" → FILE menu → Import submenu
    ///
    /// @param menuBar Target menu bar
    /// @param commands All commands to process
    /// @param registry CommandRegistry reference
    void buildMenuHierarchy(QMenuBar* menuBar,
                            const std::vector<Command>& commands,
                            CommandRegistry& registry);

    /// @brief Get or create submenu at path
    ///
    /// Returns existing submenu or creates new one if needed.
    /// Example: getOrCreateMenu(fileMenu, "Import") → Import submenu
    ///
    /// @param parent Parent menu
    /// @param title Submenu title
    /// @return Existing or new submenu
    QMenu* getOrCreateSubmenu(QMenu* parent, const QString& title);

    /// @brief Create menu action from Command
    ///
    /// Handles phase markers: if phase > 0, shows QMessageBox "Coming in Phase X".
    ///
    /// @param menu Target menu
    /// @param command Command descriptor
    /// @param registry CommandRegistry reference
    void createMenuAction(QMenu* menu,
                          const Command& command,
                          CommandRegistry& registry);

    // Dynamic submenu providers
    std::map<std::string, DynamicMenuProvider> m_dynamicProviders;

    // Menu cache (path → QMenu*)
    std::map<std::string, QMenu*> m_menuCache;
};

} // namespace gui
} // namespace kalahari
