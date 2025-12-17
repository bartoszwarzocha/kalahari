/// @file icon_registrar.h
/// @brief Application icon registration with IconRegistry
///
/// This module is extracted from MainWindow to reduce its size (OpenSpec #00038).
/// Contains all icon registrations for the application.

#ifndef KALAHARI_GUI_ICON_REGISTRAR_H
#define KALAHARI_GUI_ICON_REGISTRAR_H

namespace kalahari {
namespace gui {

/// @brief Register all application icons with IconRegistry.
///
/// This function registers all SVG icons used throughout the application
/// with the central IconRegistry. Icons are organized by category:
/// - File menu icons
/// - Edit menu icons
/// - Book menu icons
/// - Insert menu icons
/// - Format menu icons
/// - Tools menu icons
/// - Assistant menu icons
/// - View menu icons
/// - Help menu icons
/// - Log panel icons
/// - Common/utility icons
/// - New item dialog icons
/// - Structure icons
///
/// Call once during application startup, before creating any UI elements
/// that require icons.
///
/// @note This was extracted from MainWindow::registerCommands() in OpenSpec #00038.
void registerAllIcons();

} // namespace gui
} // namespace kalahari

#endif // KALAHARI_GUI_ICON_REGISTRAR_H
