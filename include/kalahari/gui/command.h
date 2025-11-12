/// @file command.h
/// @brief Command Registry data structures
///
/// Defines core structures for unified command execution system:
/// - IconSet: Pre-rendered icons in 3 sizes (16/24/32px)
/// - KeyboardShortcut: Keyboard binding representation
/// - Command: Complete command descriptor with execution callbacks

#pragma once

#include <wx/wx.h>
#include <wx/bitmap.h>
#include <functional>
#include <string>

namespace kalahari {
namespace gui {

// ============================================================================
// IconSet - Pre-rendered Icons for Menu/Toolbar
// ============================================================================

/// @brief Pre-rendered icon set for command in multiple sizes
///
/// Stores wxBitmap instances for different UI contexts:
/// - 16x16: Menu items
/// - 24x24: Standard toolbar (default)
/// - 32x32: Large toolbar
///
/// Note: This is different from IconRegistry (which stores SVG).
/// IconSet stores final rendered bitmaps ready for immediate use.
struct IconSet {
    wxBitmap icon16;  ///< 16x16 bitmap for menus
    wxBitmap icon24;  ///< 24x24 bitmap for default toolbar
    wxBitmap icon32;  ///< 32x32 bitmap for large toolbar

    /// @brief Default constructor (empty icons)
    IconSet() = default;

    /// @brief Load icon from file path and scale to 3 sizes
    /// @param path Path to image file (PNG, BMP, or SVG eventually)
    /// @note For now, loads bitmap and scales. SVG support in Phase 2+
    explicit IconSet(const wxString& path);

    /// @brief Check if icon set is empty (all bitmaps invalid)
    bool isEmpty() const {
        return !icon16.IsOk() && !icon24.IsOk() && !icon32.IsOk();
    }
};

// ============================================================================
// KeyboardShortcut - Keyboard Binding Representation
// ============================================================================

/// @brief Keyboard shortcut descriptor
///
/// Represents a keyboard combination for command execution.
/// Format: Modifier(s) + Key (e.g., Ctrl+S, Ctrl+Shift+N)
struct KeyboardShortcut {
    int keyCode = 0;           ///< wxKeyCode (e.g., 'S', WXK_F1, WXK_RETURN)
    bool ctrl = false;         ///< Ctrl/Cmd modifier
    bool alt = false;          ///< Alt modifier
    bool shift = false;        ///< Shift modifier

    /// @brief Default constructor (no shortcut)
    KeyboardShortcut() = default;

    /// @brief Construct from key code and modifiers
    KeyboardShortcut(int key, bool ctrlMod = false, bool altMod = false, bool shiftMod = false)
        : keyCode(key), ctrl(ctrlMod), alt(altMod), shift(shiftMod) {}

    /// @brief Convert shortcut to human-readable string
    /// @return String like "Ctrl+S", "Ctrl+Shift+N", "F1"
    wxString toString() const;

    /// @brief Parse shortcut from string
    /// @param str String like "Ctrl+S", "ctrl+shift+a" (case-insensitive)
    /// @return Parsed shortcut or empty shortcut if parsing fails
    static KeyboardShortcut fromString(const wxString& str);

    /// @brief Check if shortcut is empty (no key defined)
    bool isEmpty() const { return keyCode == 0; }

    /// @brief Equality comparison
    bool operator==(const KeyboardShortcut& other) const {
        return keyCode == other.keyCode &&
               ctrl == other.ctrl &&
               alt == other.alt &&
               shift == other.shift;
    }

    /// @brief Inequality comparison
    bool operator!=(const KeyboardShortcut& other) const {
        return !(*this == other);
    }

    /// @brief Less-than comparison (for std::map ordering)
    /// @note Needed for using KeyboardShortcut as map key
    bool operator<(const KeyboardShortcut& other) const {
        if (keyCode != other.keyCode) return keyCode < other.keyCode;
        if (ctrl != other.ctrl) return ctrl < other.ctrl;
        if (alt != other.alt) return alt < other.alt;
        return shift < other.shift;
    }
};

// ============================================================================
// Command - Complete Command Descriptor
// ============================================================================

/// @brief Complete command descriptor with execution logic
///
/// Central data structure for Command Registry system.
/// Represents a single executable action (menu item, toolbar button, keyboard shortcut).
///
/// Example usage:
/// @code
/// Command cmd;
/// cmd.id = "file.save";
/// cmd.label = "Save";
/// cmd.tooltip = "Save current document";
/// cmd.category = "File";
/// cmd.icons = IconSet("icons/save.png");
/// cmd.shortcut = KeyboardShortcut('S', true);  // Ctrl+S
/// cmd.execute = []() { /* save logic */ };
/// cmd.isEnabled = []() { return documentModified; };
/// @endcode
struct Command {
    // ========================================================================
    // Identification
    // ========================================================================
    std::string id;              ///< Unique command ID ("file.save", "edit.undo")
    std::string label;           ///< Display label ("Save", "Undo")
    std::string tooltip;         ///< Tooltip text ("Save current document")
    std::string category;        ///< Category for grouping ("File", "Edit", "View")

    // ========================================================================
    // Visual Representation
    // ========================================================================
    IconSet icons;               ///< Icon set (16/24/32px)
    bool showInMenu = true;      ///< Show in menu bar
    bool showInToolbar = false;  ///< Show in toolbar

    // ========================================================================
    // Keyboard Binding
    // ========================================================================
    KeyboardShortcut shortcut;   ///< Keyboard shortcut
    bool isShortcutCustomizable = true; ///< Allow user to change shortcut

    // ========================================================================
    // Execution Logic
    // ========================================================================
    std::function<void()> execute;      ///< Command execution callback
    std::function<bool()> isEnabled;    ///< Enable/disable state callback
    std::function<bool()> isChecked;    ///< Check state for toggle commands

    // ========================================================================
    // Plugin Integration (Phase 2+)
    // ========================================================================
    bool isPluginCommand = false;       ///< True if registered by plugin
    std::string pluginId;               ///< Plugin ID that registered command
    int apiVersion = 1;                 ///< Command API version

    // ========================================================================
    // Helpers
    // ========================================================================

    /// @brief Check if command has valid execution callback
    bool canExecute() const { return execute != nullptr; }

    /// @brief Check if command should be enabled
    /// @return true if enabled callback returns true or if no callback set
    bool checkEnabled() const {
        return isEnabled ? isEnabled() : true;
    }

    /// @brief Check if command should be checked
    /// @return true if checked callback returns true, false otherwise
    bool checkChecked() const {
        return isChecked ? isChecked() : false;
    }
};

} // namespace gui
} // namespace kalahari
