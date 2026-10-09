/// @file editor_appearance.h
/// @brief Visual appearance configuration for BookEditor (OpenSpec #00042 Phase 5)
///
/// EditorAppearance holds what the editor shows the way the settings say: colors, the
/// text's font and typography, the page, the cursor and the writing modes. EditorPanel
/// fills it from the editor settings; the defaults here are those of an editor without
/// settings and match the settings schema.
///
/// Philosophy: "Pisarz, szklanka whisky, zanurzenie w procesie twórczym"
/// The visual environment should support deep focus and creative flow.

#pragma once

#include <QColor>
#include <QFont>
#include <QSizeF>
#include <QString>

namespace kalahari::editor {

// =============================================================================
// Editor Color Mode
// =============================================================================

/// @brief Editor color mode (independent from application theme)
///
/// The editor can have its own light/dark mode that user can toggle
/// independently from the application theme. This allows:
/// - Dark app theme with light editor (for distraction-free writing)
/// - Light app theme with dark editor (for eye comfort at night)
enum class EditorColorMode {
    Light,  ///< Light background, dark text
    Dark    ///< Dark background, light text
};

// =============================================================================
// Color Palette
// =============================================================================

/// @brief Colors of the editor, for the light and the dark paper
struct EditorColors {
    QColor selection{66, 133, 244, 80};        ///< Selection highlight
    QColor pageShadow{0, 0, 0, 60};            ///< Shade of the view's sides in Distraction-Free

    /// @brief The paper and its text
    struct ContinuousColors {
        // Light mode
        QColor backgroundLight{255, 255, 255};   ///< Background - light mode
        QColor textLight{30, 30, 30};            ///< Text - light mode
        // Dark mode
        QColor backgroundDark{35, 35, 40};       ///< Background - dark mode
        QColor textDark{224, 224, 224};          ///< Text - dark mode
    } continuous;

    /// @brief The paragraphs Focus dims
    struct FocusColors {
        // Inactive paragraph colors - must be between background and text for visibility
        // Light mode: text is dark (30), inactive should be lighter gray
        // Dark mode: text is light (224), inactive should be darker but still visible
        QColor inactiveLight{170, 170, 170};     ///< Inactive text - light mode
        QColor inactiveDark{120, 120, 125};      ///< Inactive text - dark mode (visible on dark bg)
    } focus;

    /// @brief Get background color for current mode
    QColor background(EditorColorMode mode) const {
        return mode == EditorColorMode::Light
            ? continuous.backgroundLight
            : continuous.backgroundDark;
    }

    /// @brief Get text color for current mode
    QColor textColor(EditorColorMode mode) const {
        return mode == EditorColorMode::Light
            ? continuous.textLight
            : continuous.textDark;
    }

    /// @brief Get inactive text color for Focus mode
    QColor focusInactiveColor(EditorColorMode mode) const {
        return mode == EditorColorMode::Light
            ? focus.inactiveLight
            : focus.inactiveDark;
    }

    /// @brief The marks of the annotations' kinds on one paper (the theme's colors; not
    /// valid: the text's color)
    struct AnnotationColors {
        QColor comment;  ///< Comments
        QColor todo;     ///< To-dos
        QColor note;     ///< Notes
    };
    AnnotationColors annotationsLight;  ///< On the light paper
    AnnotationColors annotationsDark;   ///< On the dark paper

    /// @brief The marks' colors on the paper of a mode
    const AnnotationColors& annotations(EditorColorMode mode) const {
        return mode == EditorColorMode::Light ? annotationsLight : annotationsDark;
    }
};

// =============================================================================
// Typography Configuration
// =============================================================================

/// @brief Canonical default editor text font. Single source of truth: the editor,
/// the settings dialog (SettingsCoordinator) and the toolbar font combo all read
/// their fallback from here. When these drifted apart (dialog defaulted to Consolas
/// 12 while the editor used Georgia 14), saving settings silently changed the
/// editor font, because the dialog persisted its own default on every save.
inline constexpr const char* DEFAULT_TEXT_FONT_FAMILY = "Georgia";
inline constexpr int DEFAULT_TEXT_FONT_SIZE = 14;

/// @brief Typography settings for the editor
struct EditorTypography {
    // Main text
    QFont textFont{DEFAULT_TEXT_FONT_FAMILY, DEFAULT_TEXT_FONT_SIZE};  ///< Main text font
    qreal lineHeight{1.6};                     ///< Line height multiplier
    qreal paragraphSpacing{12.0};              ///< Space between paragraphs

    // First line indent
    bool firstLineIndent{true};                ///< Indent first line of paragraphs
    qreal indentSize{24.0};                    ///< First line indent in pixels

    QFont uiFont{"Segoe UI", 10};              ///< Font of the Distraction-Free texts (word count)
};

// =============================================================================
// Page Layout Configuration
// =============================================================================

/// @brief The page: its format, the gap around it and its numbers
struct PageLayout {
    // Page size
    enum class PageSize {
        A4,
        A5,
        Letter,
        Legal,
        Custom,
        B5,
        Trade6x9                               ///< 6 x 9 inch, a common book format
    };

    PageSize pageSize{PageSize::A4};
    qreal customWidth{210.0};                  ///< Custom width in mm
    qreal customHeight{297.0};                 ///< Custom height in mm

    // Display
    qreal zoomLevel{1.0};                      ///< Zoom of a newly opened document (1.0 = 100%)
    qreal pageGap{20.0};                       ///< Gap between pages in pixels
    bool showPageNumbers{true};                ///< Page numbers at the bottom of the pages

    /// @brief Page dimensions in millimetres
    QSizeF pageSizeMm() const;

    /// @brief Page size from its id ("A4", "A5", "B5", "6x9", "Letter", "Legal", "Custom");
    ///        A4 for an unknown id
    static PageSize pageSizeFromId(const QString& id);

    /// @brief Id of a page size, as saved in the settings
    static QString pageSizeId(PageSize size);
};

// =============================================================================
// Mode-Specific Settings
// =============================================================================

/// @brief Typewriter scrolling (a toggle in every view mode)
struct TypewriterSettings {
    bool enabled{false};                       ///< Typewriter scrolling on
    qreal focusPosition{0.5};                  ///< Height of the cursor line (0-1, 0.5 = middle)
    bool smoothScroll{true};                   ///< Smooth scrolling animation
    int scrollDuration{150};                   ///< Scroll animation duration in ms
};

/// @brief Focus: every paragraph but the cursor's is dimmed (a toggle in every view mode)
struct FocusModeSettings {
    bool enabled{false};                       ///< Focus mode active
};

/// @brief What Distraction-Free writing shows at the edges of the view
struct DistractionFreeSettings {
    bool showWordCount{true};                  ///< Show word count at bottom
    bool showClock{false};                     ///< Show clock
    qreal textWidth{0.6};                      ///< Width of the undarkened middle (0-1 of the view)
    int uiFadeTimeout{2000};                   ///< UI fade timeout in ms
    bool fadeOnMouseMove{true};                ///< Show UI on mouse move to edges
};

/// @brief Text frame border settings
///
/// A frame around the text area of the pages, to show where the margins are.
struct TextFrameBorder {
    bool show = false;              ///< Show border around text area
    QColor color{180, 180, 180};    ///< Border color
    int width = 1;                  ///< Border width in pixels
};

/// @brief Cursor style enumeration
enum class CursorStyle {
    Line,       ///< Vertical line cursor (|)
    Block,      ///< Block cursor covering current character (█)
    Underline   ///< Underline cursor under current character (_)
};

/// @brief Settings for cursor appearance
struct CursorSettings {
    CursorStyle style{CursorStyle::Line};      ///< Cursor shape
    bool useCustomColor{false};                ///< Use custom color instead of text color
    QColor customColor{255, 255, 255};         ///< Custom cursor color (if useCustomColor)
    bool blinking{true};                       ///< Enable cursor blinking
    int blinkInterval{500};                    ///< Blink interval in milliseconds
    int lineWidth{2};                          ///< Width for Line cursor in pixels
};

// =============================================================================
// Margin Configuration
// =============================================================================

/// @brief Page margins configuration (every view: the continuous views are an endless page)
struct PageMarginsConfig {
    double top = 25.4;           ///< Top margin in mm (default 1 inch)
    double bottom = 25.4;        ///< Bottom margin in mm
    double left = 25.4;          ///< Left margin in mm
    double right = 25.4;         ///< Right margin in mm
};

// =============================================================================
// Editor Appearance (Main Class)
// =============================================================================

/// @brief Everything the editor shows the way the settings say
///
/// Usage:
/// @code
/// EditorAppearance appearance = editor->appearance();
/// appearance.typography.textFont = QFont("Literata", 16);
/// editor->setAppearance(appearance);
/// @endcode
class EditorAppearance {
public:
    /// @brief Current editor color mode (light/dark toggle)
    ///
    /// This is independent from the application theme. User can have:
    /// - Dark app theme with light editor
    /// - Light app theme with dark editor
    EditorColorMode colorMode{EditorColorMode::Dark};

    EditorColors colors;                       ///< Color palette
    EditorTypography typography;               ///< Typography settings
    PageLayout pageLayout;                     ///< Page mode layout
    TypewriterSettings typewriter;             ///< Typewriter mode settings
    FocusModeSettings focusMode;               ///< Focus mode settings
    DistractionFreeSettings distractionFree;   ///< Distraction-free mode settings
    CursorSettings cursor;                     ///< Cursor appearance settings
    TextFrameBorder textFrameBorder;           ///< Text frame border settings
    PageMarginsConfig pageMargins;             ///< The page's margins, in every view
    double annotationMarkScale{1.0};           ///< Size of the annotations' marks (1 = 100%)
};

}  // namespace kalahari::editor
