/// @file command_registrar.h
/// @brief Application command registration with CommandRegistry
///
/// This module is extracted from MainWindow to reduce its size (OpenSpec #00038).
/// Contains all command registrations for the application.
///
/// Uses a callback struct pattern to allow commands to execute methods on MainWindow
/// without creating circular dependencies.

#ifndef KALAHARI_GUI_COMMAND_REGISTRAR_H
#define KALAHARI_GUI_COMMAND_REGISTRAR_H

#include <functional>

namespace kalahari {
namespace gui {

/// @brief Callbacks for MainWindow actions triggered by commands.
///
/// This struct provides a way for CommandRegistrar to call MainWindow methods
/// without creating a direct dependency. MainWindow creates this struct with
/// lambdas that capture `this` and call the appropriate methods.
///
/// Commands that don't need MainWindow interaction use empty callbacks.
struct CommandCallbacks {
    // =========================================================================
    // FILE COMMANDS
    // =========================================================================
    std::function<void()> onNewDocument;        ///< File > New
    std::function<void()> onNewProject;         ///< File > New Book...
    std::function<void()> onOpenDocument;       ///< File > Open Book...
    std::function<void()> onOpenStandaloneFile; ///< File > Open File...
    std::function<void()> onSaveDocument;       ///< File > Save
    std::function<void()> onSaveAsDocument;     ///< File > Save As...
    std::function<void()> onImportArchive;      ///< File > Import > Project Archive...
    std::function<void()> onExportArchive;      ///< File > Export > Project Archive...
    std::function<void()> onExit;               ///< File > Exit

    // =========================================================================
    // EDIT COMMANDS
    // =========================================================================
    std::function<void()> onUndo;               ///< Edit > Undo
    std::function<void()> onRedo;               ///< Edit > Redo
    std::function<void()> onCut;                ///< Edit > Cut
    std::function<void()> onCopy;               ///< Edit > Copy
    std::function<void()> onPaste;              ///< Edit > Paste
    std::function<void()> onSelectAll;          ///< Edit > Select All
    std::function<void()> onSettings;           ///< Edit > Preferences/Settings

    // =========================================================================
    // VIEW COMMANDS
    // =========================================================================
    std::function<void()> onDashboard;          ///< View > Dashboard
    std::function<void()> onResetLayout;        ///< View > Reset Layout

    // =========================================================================
    // TOOLS COMMANDS
    // =========================================================================
    std::function<void()> onToolbarManager;     ///< Tools > Customize Toolbars...

    // =========================================================================
    // HELP COMMANDS
    // =========================================================================
    std::function<void()> onAbout;              ///< Help > About Kalahari
};

/// @brief Register all application commands with CommandRegistry.
///
/// This function registers all menu and toolbar commands with the central
/// CommandRegistry. Commands are organized by menu category:
/// - File menu commands
/// - Edit menu commands
/// - Book menu commands
/// - Insert menu commands
/// - Format menu commands
/// - Tools menu commands
/// - Assistant menu commands
/// - View menu commands
/// - Help menu commands
///
/// Call once during application startup, before creating menus and toolbars.
///
/// @param callbacks Struct containing callbacks to MainWindow methods
/// @return Number of commands registered
///
/// @note This was extracted from MainWindow::registerCommands() in OpenSpec #00038.
/// @note Icon registration is handled separately by registerAllIcons().
int registerAllCommands(const CommandCallbacks& callbacks);

} // namespace gui
} // namespace kalahari

#endif // KALAHARI_GUI_COMMAND_REGISTRAR_H
