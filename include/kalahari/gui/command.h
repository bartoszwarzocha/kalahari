/// @file command.h
/// @brief Command Registry data structures
///
/// Defines core structures for unified command execution system:
/// - IconSet: Pre-rendered icons in 3 sizes (16/24/32px)
/// - KeyboardShortcut: Keyboard binding representation
/// - Command: Complete command descriptor with execution callbacks

#pragma once

#include <QPixmap>
#include <QIcon>
#include <QString>
#include <QKeySequence>
#include <QAction>
#include <QStyle>
#include <QColor>
#include <functional>
#include <string>

namespace kalahari {
namespace gui {

// ============================================================================
// IconSet - Pre-rendered Icons for Menu/Toolbar
// ============================================================================

/// @brief Pre-rendered icon set for command in multiple sizes
///
/// Stores QPixmap instances for different UI contexts:
/// - 16x16: Menu items
/// - 24x24: Standard toolbar (default)
/// - 32x32: Large toolbar
///
/// Note: This is different from IconRegistry (which stores SVG).
/// IconSet stores final rendered bitmaps ready for immediate use.
struct IconSet {
    QPixmap icon16;  ///< 16x16 pixmap for menus
    QPixmap icon24;  ///< 24x24 pixmap for default toolbar
    QPixmap icon32;  ///< 32x32 pixmap for large toolbar

    /// @brief Default constructor (empty icons)
    IconSet() = default;

    /// @brief Load icon from file path and scale to 3 sizes
    /// @param path Path to image file (PNG, BMP, or SVG eventually)
    /// @note For now, loads pixmap and scales. SVG support in Phase 2+
    explicit IconSet(const QString& path);

    /// @brief Create IconSet from Qt standard icon (Task #00019)
    /// @param icon Qt Standard Pixmap enum value (e.g., QStyle::SP_FileIcon)
    /// @return IconSet with 3 sizes (16/24/32px)
    /// @note Uses QApplication::style()->standardIcon()
    static IconSet fromStandardIcon(QStyle::StandardPixmap icon);

    /// @brief Create placeholder icon with colored background and letter (Task #00019)
    /// @param letter Letter to display (e.g., "B" for Bold, "I" for Italic)
    /// @param color Background color
    /// @return IconSet with 3 sizes (16/24/32px)
    /// @note Letter is white, bold, centered, 60% of pixmap height
    static IconSet createPlaceholder(const QString& letter, const QColor& color);

    /// @brief Check if icon set is empty (all pixmaps invalid)
    bool isEmpty() const {
        return icon16.isNull() && icon24.isNull() && icon32.isNull();
    }

    /// @brief Convert to QIcon for Qt integration
    /// @return QIcon with all three sizes (16/24/32px)
    QIcon toQIcon() const;
};

// ============================================================================
// KeyboardShortcut - Keyboard Binding Representation
// ============================================================================

/// @brief Keyboard shortcut descriptor
///
/// Represents a keyboard combination for command execution.
/// Format: Modifier(s) + Key (e.g., Ctrl+S, Ctrl+Shift+N)
struct KeyboardShortcut {
    int keyCode = 0;           ///< Qt::Key (e.g., Qt::Key_S, Qt::Key_F1)
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;  ///< Ctrl/Alt/Shift flags

    /// @brief Default constructor (no shortcut)
    KeyboardShortcut() = default;

    /// @brief Construct from key code and modifiers
    KeyboardShortcut(Qt::Key key, Qt::KeyboardModifiers mods = Qt::NoModifier)
        : keyCode(key), modifiers(mods) {}

    /// @brief Construct from int key code and modifiers
    KeyboardShortcut(int key, Qt::KeyboardModifiers mods = Qt::NoModifier)
        : keyCode(key), modifiers(mods) {}

    /// @brief Convert shortcut to human-readable string
    /// @return String like "Ctrl+S", "Ctrl+Shift+N", "F1"
    QString toString() const;

    /// @brief Convert to QKeySequence for Qt integration
    /// @return QKeySequence for use in QAction::setShortcut()
    QKeySequence toQKeySequence() const;

    /// @brief Parse shortcut from QKeySequence
    /// @param seq QKeySequence to convert
    /// @return Parsed shortcut
    static KeyboardShortcut fromQKeySequence(const QKeySequence& seq);

    /// @brief Parse shortcut from string
    /// @param str String like "Ctrl+S", "ctrl+shift+a" (case-insensitive)
    /// @return Parsed shortcut or empty shortcut if parsing fails
    static KeyboardShortcut fromString(const QString& str);

    /// @brief Check if shortcut is empty (no key defined)
    bool isEmpty() const { return keyCode == 0; }

    /// @brief Equality comparison
    bool operator==(const KeyboardShortcut& other) const {
        return keyCode == other.keyCode && modifiers == other.modifiers;
    }

    /// @brief Inequality comparison
    bool operator!=(const KeyboardShortcut& other) const {
        return !(*this == other);
    }

    /// @brief Less-than comparison (for std::map ordering)
    /// @note Needed for using KeyboardShortcut as map key
    bool operator<(const KeyboardShortcut& other) const {
        if (keyCode != other.keyCode) return keyCode < other.keyCode;
        return modifiers < other.modifiers;
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
    // Menu Hierarchy (Task #00016)
    // ========================================================================
    std::string menuPath;        ///< Hierarchical menu path ("FILE/Import/DOCX Document...")
    int menuOrder = 0;           ///< Order within menu (10, 20, 30... allows insertion)
    bool addSeparatorAfter = false; ///< Add separator after this item
    int phase = 0;               ///< Implementation phase (0=now, 1=Phase 1, etc.)

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

    /// @brief Convert to QAction for Qt integration
    /// @param parent Parent QObject for memory management
    /// @return QAction with label, icon, shortcut, tooltip configured
    /// @note Execute callback NOT connected - connect externally via triggered signal
    QAction* toQAction(QObject* parent) const;
};

} // namespace gui
} // namespace kalahari
