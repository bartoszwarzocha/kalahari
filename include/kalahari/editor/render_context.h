/// @file render_context.h
/// @brief RenderContext - Pure data structure for rendering attributes
///
/// RenderContext is a pure data container with no logic.
/// All computation is done in Pipeline::configure().
///
/// Contains:
/// - INPUT PARAMETERS: Set by BookEditor based on user settings
/// - COMPUTED VALUES: Calculated by EditorRenderPipeline

#pragma once

#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/view_modes.h>
#include <QFont>
#include <QColor>
#include <QMargins>
#include <QSizeF>

namespace kalahari::editor {

/// @brief Default screen DPI (standard 96 DPI display)
constexpr double DEFAULT_DPI = 96.0;

/// @brief Margin configuration for rendering
///
/// Defines the margins around the text content area.
/// All values are in pixels.
struct RenderMargins {
    double left = 50.0;     ///< Left margin (pixels)
    double top = 30.0;      ///< Top margin (pixels)
    double right = 50.0;    ///< Right margin (pixels)
    double bottom = 30.0;   ///< Bottom margin (pixels)

    /// @brief Check if margins are equal
    bool operator==(const RenderMargins& other) const {
        return left == other.left && top == other.top &&
               right == other.right && bottom == other.bottom;
    }

    bool operator!=(const RenderMargins& other) const {
        return !(*this == other);
    }

    /// @brief Convert to QMarginsF
    QMarginsF toQMarginsF() const {
        return QMarginsF(left, top, right, bottom);
    }

    /// @brief Create from QMarginsF
    static RenderMargins fromQMarginsF(const QMarginsF& m) {
        return RenderMargins{m.left(), m.top(), m.right(), m.bottom()};
    }
};

/// @brief Color scheme for text rendering
///
/// Defines all colors used in text rendering.
/// Colors are applied in the render stage of the pipeline.
struct RenderColors {
    QColor text{30, 30, 30};                   ///< Default text color
    QColor background{255, 255, 255};          ///< Background color
    QColor cursor{30, 30, 30};                 ///< Cursor color
    QColor selection{51, 153, 255, 127};       ///< Selection highlight color
    QColor selectionText{255, 255, 255};       ///< Selected text color
    QColor inactiveText{150, 150, 150};        ///< Dimmed text (focus mode)
    QColor lineHighlight{245, 245, 245};       ///< Current line highlight

    /// @brief Search highlight colors
    QColor searchHighlight{255, 255, 100};     ///< Search match background
    QColor currentMatch{255, 180, 50};         ///< Current search match background

    /// @brief Marker colors
    QColor commentHighlight{255, 255, 200};    ///< Comment annotation background
    QColor commentBorder{200, 180, 100};       ///< Comment annotation border
    QColor todoHighlight{255, 220, 100};       ///< TODO marker background
    QColor noteHighlight{100, 200, 255};       ///< NOTE marker background
    QColor completedTodo{200, 255, 200};       ///< Completed TODO background

    /// @brief Spell/grammar check colors
    QColor spellError{255, 0, 0};              ///< Spelling error underline
    QColor grammarWarning{0, 100, 255};        ///< Grammar warning underline

    /// @brief Check if colors are equal
    bool operator==(const RenderColors& other) const {
        return text == other.text &&
               background == other.background &&
               cursor == other.cursor &&
               selection == other.selection &&
               selectionText == other.selectionText &&
               inactiveText == other.inactiveText &&
               lineHighlight == other.lineHighlight &&
               searchHighlight == other.searchHighlight &&
               currentMatch == other.currentMatch &&
               commentHighlight == other.commentHighlight &&
               commentBorder == other.commentBorder &&
               todoHighlight == other.todoHighlight &&
               noteHighlight == other.noteHighlight &&
               completedTodo == other.completedTodo &&
               spellError == other.spellError &&
               grammarWarning == other.grammarWarning;
    }

    bool operator!=(const RenderColors& other) const {
        return !(*this == other);
    }
};

/// @brief Cursor rendering configuration
struct CursorConfig {
    double width = 2.0;                        ///< Cursor width in pixels
    int blinkInterval = 530;                   ///< Blink interval in ms (0 = no blink)
    bool visible = true;                       ///< Whether cursor is visible
    bool blinkState = true;                    ///< Current blink state (for rendering)
};

/// @brief Focus mode configuration
struct FocusModeConfig {
    bool enabled = false;                      ///< Whether focus mode is active
    int focusedParagraph = -1;                 ///< Currently focused paragraph (-1 = none)
    double dimOpacity = 0.4;                   ///< Opacity for non-focused text (0.0-1.0)
};

/// @brief Page mode configuration
struct PageModeConfig {
    QSizeF pageSize{595.0, 842.0};            ///< Page size (A4 default, in points)
    double pageSpacing = 20.0;                 ///< Gap between pages (pixels)
    QColor pageShadow{0, 0, 0, 50};           ///< Page shadow color
    bool showPageBreaks = true;                ///< Show page break lines
};

/// @brief Complete rendering context - pure data structure
///
/// Contains:
/// - INPUT PARAMETERS: Set by BookEditor based on user settings
/// - COMPUTED VALUES: Calculated by EditorRenderPipeline
///
/// Usage:
/// @code
/// RenderContext ctx;
/// ctx.margins.left = 60.0;
/// ctx.colors.text = Qt::black;
/// ctx.zoomFactor = 1.25;  // 125% zoom
/// ctx.zoomMode = ZoomMode::FontScaling;
///
/// pipeline.configure(ctx);  // Fills ctx.computed
/// pipeline.render(painter, clipRect);
/// @endcode
struct RenderContext {
    // =========================================================================
    // INPUT PARAMETERS (set by BookEditor)
    // =========================================================================

    // -------------------------------------------------------------------------
    // Core Layout Parameters
    // -------------------------------------------------------------------------

    RenderMargins margins;                     ///< Margins around content
    double zoomFactor = 1.0;                   ///< User's zoom level (1.0 = 100%)
    ZoomMode zoomMode = ZoomMode::FontScaling; ///< How zoom is applied
    double textWidth = 800.0;                  ///< Available width for text (pixels)
    double lineSpacing = 1.0;                  ///< Line spacing multiplier

    // -------------------------------------------------------------------------
    // DPI Scaling (for WYSIWYG rendering)
    // -------------------------------------------------------------------------

    double screenDpi = DEFAULT_DPI;            ///< Physical screen DPI from physicalDotsPerInch()

    // -------------------------------------------------------------------------
    // Typography
    // -------------------------------------------------------------------------

    QFont font{"Segoe UI", 11};               ///< Base font for text

    // -------------------------------------------------------------------------
    // Colors
    // -------------------------------------------------------------------------

    RenderColors colors;                       ///< All rendering colors

    // -------------------------------------------------------------------------
    // View Mode
    // -------------------------------------------------------------------------

    ViewMode viewMode = ViewMode::Continuous;  ///< Current view mode

    // -------------------------------------------------------------------------
    // Mode-specific Configuration
    // -------------------------------------------------------------------------

    CursorConfig cursor;                       ///< Cursor rendering config
    FocusModeConfig focusMode;                 ///< Focus mode config
    PageModeConfig pageMode;                   ///< Page mode config

    // -------------------------------------------------------------------------
    // Text Frame Border
    // -------------------------------------------------------------------------

    bool showTextFrameBorder = false;          ///< Show border around text area
    QColor textFrameBorderColor{180, 180, 180}; ///< Border color
    int textFrameBorderWidth = 1;              ///< Border width in pixels

    // -------------------------------------------------------------------------
    // Scroll State
    // -------------------------------------------------------------------------

    double scrollY = 0.0;                      ///< Current vertical scroll offset
    int currentPageNumber = 1;                 ///< Current page number (1-based, for mirror margins)

    // -------------------------------------------------------------------------
    // Viewport Info
    // -------------------------------------------------------------------------

    QSizeF viewportSize;                       ///< Viewport dimensions

    // =========================================================================
    // COMPUTED VALUES (set by Pipeline::configure())
    // =========================================================================

    /// @brief Pre-computed values for rendering
    ///
    /// These values are calculated by Pipeline::configure() based on input parameters.
    /// They should not be set directly - always call configure() after changing inputs.
    struct Computed {
        // ---------------------------------------------------------------------
        // DPI-derived values
        // ---------------------------------------------------------------------

        double dpiScale = 1.0;                  ///< screenDpi / DEFAULT_DPI
        double mmToPixels = DEFAULT_DPI / 25.4; ///< Conversion factor (dpi / 25.4)

        // ---------------------------------------------------------------------
        // Effective font (after zoom in FontScaling mode)
        // ---------------------------------------------------------------------

        QFont effectiveFont;                    ///< Font with zoom applied (FontScaling mode)

        // ---------------------------------------------------------------------
        // Effective margins in pixels (after DPI/mode calculation)
        // ---------------------------------------------------------------------

        double marginLeft = 50.0;               ///< Left margin (pixels)
        double marginTop = 30.0;                ///< Top margin (pixels)
        double marginRight = 50.0;              ///< Right margin (pixels)
        double marginBottom = 30.0;             ///< Bottom margin (pixels)

        // ---------------------------------------------------------------------
        // Effective text width in pixels
        // ---------------------------------------------------------------------

        double textWidth = 700.0;               ///< Available width for text content

        // ---------------------------------------------------------------------
        // Page Mode specific
        // ---------------------------------------------------------------------

        double pageWidthPixels = 0.0;           ///< Page width in pixels
        double pageHeightPixels = 0.0;          ///< Page height in pixels
        double textAreaHeight = 0.0;            ///< Height of text area on page
        double pageCenterOffset = 0.0;          ///< Offset to center page in viewport

        // ---------------------------------------------------------------------
        // Unified scale factor for rendering
        // ---------------------------------------------------------------------

        double viewScale = 1.0;                 ///< Scale for QPainter (PageScaling mode)
        double totalScale = 1.0;                ///< Combined DPI + zoom scale

        // ---------------------------------------------------------------------
        // Visible range
        // ---------------------------------------------------------------------

        size_t firstVisibleParagraph = 0;       ///< First visible paragraph index
        size_t lastVisibleParagraph = 0;        ///< Last visible paragraph index

    } computed;
};

}  // namespace kalahari::editor
