/// @file editor_appearance.h
/// @brief Visual appearance configuration for BookEditor (OpenSpec #00042 Phase 5)
///
/// EditorAppearance provides a centralized, configurable system for all visual
/// aspects of the text editor. Designed for easy customization and future
/// extensibility (themes, presets, user preferences).
///
/// Philosophy: "Pisarz, szklanka whisky, zanurzenie w procesie twórczym"
/// The visual environment should support deep focus and creative flow.

#pragma once

#include <QColor>
#include <QFont>
#include <QMarginsF>
#include <QString>
#include <QJsonObject>
#include <memory>

namespace kalahari::editor {

// =============================================================================
// Color Palette
// =============================================================================

/// @brief Complete color configuration for the editor
///
/// All colors are configurable. The structure is designed to support:
/// - Light/dark themes
/// - Custom user themes
/// - Per-mode color overrides (e.g., sepia for focus mode)
struct EditorColors {
    // Background colors
    QColor editorBackground{35, 35, 40};      ///< Main editor area background
    QColor pageBackground{255, 255, 255};      ///< Page/paper color
    QColor pageShadow{0, 0, 0, 60};            ///< Page drop shadow
    QColor marginArea{245, 245, 245};          ///< Margin/gutter area

    // Text colors
    QColor text{30, 30, 30};                   ///< Default text color
    QColor textSecondary{100, 100, 100};       ///< Secondary text (line numbers, etc.)
    QColor textDimmed{150, 150, 150};          ///< Dimmed text (focus mode inactive)

    // Selection & cursor
    QColor selection{66, 133, 244, 80};        ///< Selection highlight
    QColor selectionBorder{66, 133, 244};      ///< Selection border (optional)
    QColor cursor{30, 30, 30};                 ///< Cursor/caret color
    QColor cursorLine{0, 0, 0, 15};            ///< Current line highlight

    // UI elements
    QColor ruler{200, 200, 200};               ///< Ruler/guide lines
    QColor rulerMarker{150, 150, 150};         ///< Ruler markers (tabs, margins)
    QColor scrollbar{180, 180, 180};           ///< Scrollbar color
    QColor scrollbarHover{140, 140, 140};      ///< Scrollbar on hover

    // Accents
    QColor accent{66, 133, 244};               ///< Primary accent color
    QColor accentSecondary{52, 168, 83};       ///< Secondary accent
    QColor warning{251, 188, 4};               ///< Warning/attention color
    QColor error{234, 67, 53};                 ///< Error color

    // Focus mode specific
    QColor focusHighlight{255, 250, 230};      ///< Focused paragraph background
    QColor focusDimOverlay{255, 255, 255, 180}; ///< Overlay for dimmed paragraphs

    /// @brief Create default light theme colors
    static EditorColors lightTheme();

    /// @brief Create default dark theme colors
    static EditorColors darkTheme();

    /// @brief Create sepia/warm theme colors (good for focus)
    static EditorColors sepiaTheme();

    /// @brief Load colors from JSON
    static EditorColors fromJson(const QJsonObject& json);

    /// @brief Save colors to JSON
    QJsonObject toJson() const;
};

// =============================================================================
// Visual Elements Configuration
// =============================================================================

/// @brief Configuration for visual elements (rulers, margins, guides)
struct VisualElements {
    // Rulers
    bool showHorizontalRuler{false};           ///< Show ruler at top
    bool showVerticalRuler{false};             ///< Show ruler on left (line numbers area)
    int rulerHeight{24};                       ///< Horizontal ruler height in pixels
    int rulerWidth{48};                        ///< Vertical ruler width in pixels

    // Line numbers
    bool showLineNumbers{false};               ///< Show line numbers in gutter
    bool relativeLineNumbers{false};           ///< Relative line numbers from cursor

    // Guides
    bool showMarginGuide{false};               ///< Show margin guide line
    int marginGuideColumn{80};                 ///< Column position for margin guide
    bool showIndentGuides{false};              ///< Show vertical indent guides

    // Current line
    bool highlightCurrentLine{true};           ///< Subtle highlight on cursor line
    bool highlightCurrentParagraph{false};     ///< Highlight entire paragraph

    // Page elements (Page Mode)
    bool showPageShadows{true};                ///< Drop shadows under pages
    bool showPageBorders{false};               ///< Thin border around pages
    bool showPageNumbers{true};                ///< Page numbers in Page Mode

    // Scrollbar
    bool showScrollbar{true};                  ///< Show scrollbar
    bool autoHideScrollbar{true};              ///< Auto-hide when not scrolling
    int scrollbarWidth{12};                    ///< Scrollbar width in pixels

    // Minimap (future)
    bool showMinimap{false};                   ///< Show document minimap
    int minimapWidth{100};                     ///< Minimap width in pixels

    /// @brief Load from JSON
    static VisualElements fromJson(const QJsonObject& json);

    /// @brief Save to JSON
    QJsonObject toJson() const;
};

// =============================================================================
// Typography Configuration
// =============================================================================

/// @brief Typography settings for the editor
struct EditorTypography {
    // Main text
    QFont textFont{"Georgia", 14};             ///< Main text font
    qreal lineHeight{1.6};                     ///< Line height multiplier
    qreal paragraphSpacing{12.0};              ///< Space between paragraphs

    // First line indent
    bool firstLineIndent{true};                ///< Indent first line of paragraphs
    qreal indentSize{24.0};                    ///< First line indent in pixels

    // UI fonts
    QFont uiFont{"Segoe UI", 10};              ///< Font for UI elements
    QFont monospaceFont{"Consolas", 12};       ///< Monospace font (code, etc.)

    /// @brief Load from JSON
    static EditorTypography fromJson(const QJsonObject& json);

    /// @brief Save to JSON
    QJsonObject toJson() const;
};

// =============================================================================
// Page Layout Configuration
// =============================================================================

/// @brief Page layout settings for Page Mode
struct PageLayout {
    // Page size
    enum class PageSize {
        A4,
        A5,
        Letter,
        Legal,
        Custom
    };

    PageSize pageSize{PageSize::A4};
    qreal customWidth{210.0};                  ///< Custom width in mm
    qreal customHeight{297.0};                 ///< Custom height in mm

    // Margins (in mm)
    QMarginsF margins{25.4, 25.4, 25.4, 25.4}; ///< Page margins (1 inch default)

    // Display
    qreal zoomLevel{1.0};                      ///< Zoom level (1.0 = 100%)
    qreal pageGap{20.0};                       ///< Gap between pages in pixels
    bool centerPages{true};                    ///< Center pages horizontally

    /// @brief Get page dimensions in pixels at given DPI
    QSizeF pageSizePixels(qreal dpi = 96.0) const;

    /// @brief Get text area dimensions (page minus margins)
    QSizeF textAreaPixels(qreal dpi = 96.0) const;

    /// @brief Load from JSON
    static PageLayout fromJson(const QJsonObject& json);

    /// @brief Save to JSON
    QJsonObject toJson() const;
};

// =============================================================================
// Mode-Specific Settings
// =============================================================================

/// @brief Settings specific to Typewriter Mode
struct TypewriterSettings {
    bool enabled{false};                       ///< Typewriter mode active
    qreal focusPosition{0.4};                  ///< Vertical position (0-1, 0.4 = 40% from top)
    bool smoothScroll{true};                   ///< Smooth scrolling animation
    int scrollDuration{150};                   ///< Scroll animation duration in ms
};

/// @brief Settings specific to Focus Mode
struct FocusModeSettings {
    bool enabled{false};                       ///< Focus mode active

    enum class FocusScope {
        Paragraph,                             ///< Focus on current paragraph
        Sentence,                              ///< Focus on current sentence
        Line                                   ///< Focus on current line
    };

    FocusScope scope{FocusScope::Paragraph};
    qreal dimOpacity{0.3};                     ///< Opacity of dimmed content (0-1)
    bool highlightBackground{false};           ///< Highlight focused area background
    bool fadeTransition{true};                 ///< Smooth fade transition
    int transitionDuration{200};               ///< Transition duration in ms
};

/// @brief Settings for Distraction-Free Mode
struct DistractionFreeSettings {
    bool enabled{false};                       ///< Distraction-free mode active
    bool fullscreen{true};                     ///< Use fullscreen
    bool hideAllUI{true};                      ///< Hide all UI elements
    bool showWordCount{true};                  ///< Show word count at bottom
    bool showClock{false};                     ///< Show clock
    qreal textWidth{0.6};                      ///< Text width as fraction of screen (0-1)
    int uiFadeTimeout{2000};                   ///< UI fade timeout in ms
    bool fadeOnMouseMove{true};                ///< Show UI on mouse move to edges
};

// =============================================================================
// Editor Appearance (Main Class)
// =============================================================================

/// @brief Central configuration for all visual aspects of the editor
///
/// EditorAppearance aggregates all visual settings and provides:
/// - Easy serialization (load/save to JSON)
/// - Theme presets
/// - Per-mode overrides
/// - Future: live preview, undo/redo for settings
///
/// Usage:
/// @code
/// EditorAppearance appearance;
/// appearance.colors = EditorColors::darkTheme();
/// appearance.typography.textFont = QFont("Literata", 16);
/// editor->setAppearance(appearance);
/// @endcode
class EditorAppearance {
public:
    EditorAppearance() = default;
    ~EditorAppearance() = default;

    // =========================================================================
    // Configuration Sections
    // =========================================================================

    EditorColors colors;                       ///< Color palette
    VisualElements elements;                   ///< Visual elements configuration
    EditorTypography typography;               ///< Typography settings
    PageLayout pageLayout;                     ///< Page mode layout
    TypewriterSettings typewriter;             ///< Typewriter mode settings
    FocusModeSettings focusMode;               ///< Focus mode settings
    DistractionFreeSettings distractionFree;   ///< Distraction-free mode settings

    // =========================================================================
    // Presets
    // =========================================================================

    /// @brief Create default appearance (light theme)
    static EditorAppearance defaultAppearance();

    /// @brief Create dark theme appearance
    static EditorAppearance darkAppearance();

    /// @brief Create sepia/warm appearance (good for long writing sessions)
    static EditorAppearance sepiaAppearance();

    /// @brief Create minimal appearance (no visual clutter)
    static EditorAppearance minimalAppearance();

    /// @brief Create typewriter appearance (classic feel)
    static EditorAppearance typewriterAppearance();

    /// @brief Create high contrast appearance (accessibility)
    static EditorAppearance highContrastAppearance();

    /// @brief Check if system high-contrast mode is enabled
    static bool isSystemHighContrastEnabled();

    /// @brief Get appearance adjusted for system settings
    static EditorAppearance systemAwareAppearance();

    // =========================================================================
    // Serialization
    // =========================================================================

    /// @brief Load appearance from JSON
    static EditorAppearance fromJson(const QJsonObject& json);

    /// @brief Save appearance to JSON
    QJsonObject toJson() const;

    /// @brief Load from file
    static EditorAppearance loadFromFile(const QString& path);

    /// @brief Save to file
    bool saveToFile(const QString& path) const;

    // =========================================================================
    // Utilities
    // =========================================================================

    /// @brief Create a copy with modifications
    EditorAppearance with(std::function<void(EditorAppearance&)> modifier) const;

    /// @brief Interpolate between two appearances (for transitions)
    static EditorAppearance lerp(const EditorAppearance& a,
                                  const EditorAppearance& b,
                                  qreal t);
};

}  // namespace kalahari::editor
