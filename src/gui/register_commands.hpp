/// @file register_commands.hpp
/// @brief Helper macros for consistent command registration (Task #00016)
///
/// Macros ensure uniform registration of ~200 menu items.
/// CRITICAL: All strings use tr() for Qt i18n support (EN+PL in MVP, +4 in Phase 2)

#pragma once

// Standard menu command (no toolbar, no shortcut)
// label_tr: Translatable string (will be wrapped in tr())
#define REG_CMD(id_, label_tr_, path_, order_, sep_, phase_) \
    do { \
        Command cmd; \
        cmd.id = id_; \
        cmd.label = tr(label_tr_).toStdString(); \
        cmd.tooltip = tr(label_tr_).toStdString(); \
        cmd.category = std::string(path_).substr(0, std::string(path_).find('/')); \
        cmd.menuPath = path_; \
        cmd.menuOrder = order_; \
        cmd.addSeparatorAfter = sep_; \
        cmd.phase = phase_; \
        cmd.showInMenu = true; \
        cmd.showInToolbar = false; \
        cmd.execute = []() {}; \
        registry.registerCommand(cmd); \
        count++; \
    } while(0)

// Menu command with callback
#define REG_CMD_CB(id_, label_tr_, path_, order_, sep_, phase_, callback_) \
    do { \
        Command cmd; \
        cmd.id = id_; \
        cmd.label = tr(label_tr_).toStdString(); \
        cmd.tooltip = tr(label_tr_).toStdString(); \
        cmd.category = std::string(path_).substr(0, std::string(path_).find('/')); \
        cmd.menuPath = path_; \
        cmd.menuOrder = order_; \
        cmd.addSeparatorAfter = sep_; \
        cmd.phase = phase_; \
        cmd.showInMenu = true; \
        cmd.showInToolbar = false; \
        cmd.execute = callback_; \
        registry.registerCommand(cmd); \
        count++; \
    } while(0)

// Menu command with toolbar and shortcut
#define REG_CMD_TOOL(id_, label_tr_, path_, order_, sep_, phase_, shortcut_, callback_) \
    do { \
        Command cmd; \
        cmd.id = id_; \
        cmd.label = tr(label_tr_).toStdString(); \
        cmd.tooltip = tr(label_tr_).toStdString(); \
        cmd.category = std::string(path_).substr(0, std::string(path_).find('/')); \
        cmd.menuPath = path_; \
        cmd.menuOrder = order_; \
        cmd.addSeparatorAfter = sep_; \
        cmd.phase = phase_; \
        cmd.showInMenu = true; \
        cmd.showInToolbar = true; \
        cmd.shortcut = shortcut_; \
        cmd.execute = callback_; \
        registry.registerCommand(cmd); \
        count++; \
    } while(0)

// Menu command with toolbar, shortcut, and icon (Task #00019)
#define REG_CMD_TOOL_ICON(id_, label_tr_, path_, order_, sep_, phase_, shortcut_, icon_, callback_) \
    do { \
        Command cmd; \
        cmd.id = id_; \
        cmd.label = tr(label_tr_).toStdString(); \
        cmd.tooltip = tr(label_tr_).toStdString(); \
        cmd.category = std::string(path_).substr(0, std::string(path_).find('/')); \
        cmd.menuPath = path_; \
        cmd.menuOrder = order_; \
        cmd.addSeparatorAfter = sep_; \
        cmd.phase = phase_; \
        cmd.showInMenu = true; \
        cmd.showInToolbar = true; \
        cmd.shortcut = shortcut_; \
        cmd.icons = icon_; \
        cmd.execute = callback_; \
        registry.registerCommand(cmd); \
        count++; \
    } while(0)
