/// @file art_provider.h
/// @brief Central Visual Resource Manager for Kalahari application
///
/// ArtProvider is the single source of truth for all visual resources:
/// - Icons (via IconRegistry delegation)
/// - Theme colors (via ThemeManager delegation)
/// - Icon sizes for different contexts
/// - Self-updating QAction factory
///
/// Components use ArtProvider instead of directly accessing IconRegistry/ThemeManager.
/// This provides clean separation and automatic icon refresh on theme/size changes.
///
/// OpenSpec #00026: Centralized Icon Management System

#pragma once

#include <QAction>
#include <QColor>
#include <QIcon>
#include <QObject>
#include <QPixmap>
#include <QString>
#include <QSet>

namespace kalahari {
namespace core {

// ============================================================================
// IconContext - Context for icon sizing
// ============================================================================

/// @brief Icon context determines size based on UI location
enum class IconContext {
    Toolbar,    ///< 24px default - toolbar buttons
    Menu,       ///< 16px default - menu items
    TreeView,   ///< 20px default - Navigator, file trees, outlines
    TabBar,     ///< 16px default - notebook tabs, central tab bar
    StatusBar,  ///< 16px default - status bar indicators
    Button,     ///< 20px default - QPushButton with icon
    Panel,      ///< 20px default - panel captions (Log, Search, etc.)
    Dialog,     ///< 32px default - dialog icons (About, Error, etc.)
    ComboBox    ///< 16px default - ComboBox item icons
};

// ============================================================================
// ArtProvider - Central Visual Resource Manager (Singleton)
// ============================================================================

/// @brief Central point for all visual resources in Kalahari
///
/// Provides unified API for icons, colors, and sizes. Components create actions
/// via createAction() which auto-update when theme/size changes.
///
/// Example usage:
/// @code
/// // In ToolbarManager - ZERO refresh logic needed
/// auto& art = ArtProvider::getInstance();
/// m_toolbar->addAction(art.createAction("file.new", tr("New"), this));
/// m_toolbar->addAction(art.createAction("file.save", tr("Save"), this));
/// // Actions auto-update on theme/size change!
///
/// // Direct icon access (for special cases)
/// QIcon icon = art.getIcon("file.save", IconContext::Toolbar);
/// @endcode
class ArtProvider : public QObject {
    Q_OBJECT

public:
    /// @brief Get singleton instance
    static ArtProvider& getInstance();

    /// @brief Initialize ArtProvider (call once at startup)
    /// @note Must be called after IconRegistry::initialize() and ThemeManager setup
    void initialize();

    // ========================================================================
    // Action Factory (self-updating QAction)
    // ========================================================================

    /// @brief Create self-updating QAction
    /// @param cmdId Command ID (e.g., "file.new", "edit.copy")
    /// @param text Action text (label)
    /// @param parent Parent QObject (usually QMenu or QToolBar)
    /// @param context Icon context for sizing (default: Toolbar)
    /// @return QAction with icon, connected to resourcesChanged() for auto-update
    ///
    /// The returned action automatically updates its icon when:
    /// - Theme changes (colors update)
    /// - Icon theme changes (twotone/filled/outlined/rounded)
    /// - Icon size changes for the context
    QAction* createAction(const QString& cmdId,
                          const QString& text,
                          QObject* parent,
                          IconContext context = IconContext::Toolbar);

    // ========================================================================
    // Direct Icon Access
    // ========================================================================

    /// @brief Get icon for command ID with context-appropriate size
    /// @param cmdId Command ID (e.g., "file.save")
    /// @param context Icon context for sizing
    /// @return QIcon with current theme colors applied
    QIcon getIcon(const QString& cmdId, IconContext context = IconContext::Toolbar);

    /// @brief Get pixmap for command ID with specific size
    /// @param cmdId Command ID
    /// @param size Icon size in pixels
    /// @return QPixmap with current theme colors applied
    QPixmap getPixmap(const QString& cmdId, int size);

    /// @brief Get icon with custom colors (or theme defaults if not specified)
    /// @param cmdId Action ID for the icon
    /// @param primary Primary color (optional, uses theme primary if invalid/not set)
    /// @param secondary Secondary color (optional, uses theme secondary if invalid/not set)
    /// @return QIcon that renders at any size from SVG
    QIcon getThemedIcon(const QString& cmdId,
                        const QColor& primary = QColor(),
                        const QColor& secondary = QColor());

    /// @brief Get HiDPI-aware pixmap for preview (Settings Dialog)
    /// @param cmdId Command ID
    /// @param logicalSize Logical size (display size)
    /// @param devicePixelRatio Device pixel ratio for HiDPI
    /// @param iconThemeOverride Optional icon theme to preview (default: current)
    /// @return QPixmap rendered at physical size with correct devicePixelRatio
    QPixmap getPreviewPixmap(const QString& cmdId,
                             int logicalSize,
                             qreal devicePixelRatio,
                             const QString& iconThemeOverride = QString());

    // ========================================================================
    // Theme Information
    // ========================================================================

    /// @brief Get current icon theme (twotone/filled/outlined/rounded)
    QString getIconTheme() const;

    /// @brief Get current primary icon color
    QColor getPrimaryColor() const;

    /// @brief Get current secondary icon color
    QColor getSecondaryColor() const;

    /// @brief Get current theme name (Light/Dark/Custom)
    QString getThemeName() const;

    // ========================================================================
    // Size Configuration
    // ========================================================================

    /// @brief Get icon size for context
    int getIconSize(IconContext context) const;

    /// @brief Set icon size for context
    /// @note Emits resourcesChanged() signal
    void setIconSize(IconContext context, int size);

    // ========================================================================
    // Theme Configuration
    // ========================================================================

    /// @brief Set icon theme (twotone/filled/outlined/rounded)
    /// @note Emits resourcesChanged() signal
    void setIconTheme(const QString& theme);

    /// @brief Set primary icon color
    /// @note Emits resourcesChanged() signal
    void setPrimaryColor(const QColor& color);

    /// @brief Set secondary icon color
    /// @note Emits resourcesChanged() signal
    void setSecondaryColor(const QColor& color);

    // ========================================================================
    // Batch Mode (for SettingsDialog - prevents multiple refreshes)
    // ========================================================================

    /// @brief Begin batch update mode
    ///
    /// While in batch mode, resourcesChanged() is deferred until endBatchUpdate().
    /// Use this in SettingsDialog when applying multiple changes at once.
    void beginBatchUpdate();

    /// @brief End batch update mode and emit resourcesChanged() if any changes occurred
    void endBatchUpdate();

    /// @brief Check if batch mode is active
    bool isBatchMode() const { return m_batchMode; }

signals:
    /// @brief Emitted when any visual resource changes
    ///
    /// Connected to all managed QActions for automatic icon refresh.
    /// Also emitted when:
    /// - Theme changes (via ThemeManager)
    /// - Icon theme changes (twotone/filled/etc.)
    /// - Icon sizes change
    void resourcesChanged();

public slots:
    /// @brief Handle theme change from ThemeManager
    void onThemeChanged();

private:
    ArtProvider();
    ~ArtProvider() override = default;
    ArtProvider(const ArtProvider&) = delete;
    ArtProvider& operator=(const ArtProvider&) = delete;

    /// @brief Refresh icon for a managed action
    void refreshAction(QAction* action);

    /// @brief Current icon theme (twotone/filled/outlined/rounded)
    QString m_iconTheme;

    /// @brief Set of managed actions (for cleanup when destroyed)
    QSet<QAction*> m_managedActions;

    /// @brief Batch mode flag - when true, resourcesChanged() is deferred
    bool m_batchMode = false;

    /// @brief Flag indicating changes occurred during batch mode
    bool m_pendingChanges = false;

    /// @brief Emit resourcesChanged() unless in batch mode
    void emitResourcesChanged();
};

} // namespace core
} // namespace kalahari
