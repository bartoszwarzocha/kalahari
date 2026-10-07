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
#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/view_modes.h>
#include <QFont>
#include <QColor>
#include <QMargins>
#include <QSizeF>

namespace kalahari::editor {

/// @brief Default screen DPI (standard 96 DPI display)
constexpr double DEFAULT_DPI = 96.0;

/// @brief Typographic points per inch (page sizes are given in points)
constexpr double POINTS_PER_INCH = 72.0;

/// @brief Millimetres per inch (page margins are given in millimetres)
constexpr double MM_PER_INCH = 25.4;

/// @brief Narrowest wrap width (pixels), used when the viewport is narrower than its margins
constexpr double MIN_TEXT_WIDTH = 100.0;

/// @brief Smallest share of a page's width and of its height left for the text; margins
/// that would take more are scaled down
constexpr double MIN_PAGE_TEXT_SHARE = 0.2;

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

    /// @brief Search highlight colors (translucent: drawn under the text, they suit light and
    /// dark backgrounds alike)
    QColor searchHighlight{255, 214, 0, 96};   ///< Search match background
    QColor currentMatch{255, 140, 0, 140};     ///< Current search match background

    /// @brief Spell/grammar check colors
    QColor spellError{255, 0, 0};              ///< Spelling error underline
    QColor grammarWarning{0, 100, 255};        ///< Grammar warning underline

    /// @brief Word being read aloud (background)
    QColor spokenWord{0, 190, 170, 96};

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
               spellError == other.spellError &&
               grammarWarning == other.grammarWarning &&
               spokenWord == other.spokenWord;
    }

    bool operator!=(const RenderColors& other) const {
        return !(*this == other);
    }
};

/// @brief Cursor rendering configuration
struct CursorConfig {
    double width = 2.0;                        ///< Cursor width in pixels
    bool visible = true;                       ///< Whether cursor is visible
    bool blinkState = true;                    ///< Current blink state (for rendering)
};

/// @brief Page mode configuration
struct PageModeConfig {
    QSizeF pageSize{595.0, 842.0};            ///< Page size (A4 default, in points)
    QMarginsF marginsMm{25.4, 25.4, 25.4, 25.4};  ///< Page margins (millimetres)
    double pageSpacing = 20.0;                 ///< Gap between and around pages (pixels at 100%)
    QColor pageShadow{0, 0, 0, 50};           ///< Page shadow color
    bool showPageBreaks = true;                ///< Draw page shadows
    bool showPageNumbers = true;               ///< Page numbers at the bottom centre
};

/// @brief Typewriter scrolling configuration
///
/// The line with the cursor stays at a fixed height of the view. Extra room above the
/// first line and below the last one lets those lines reach it too.
struct TypewriterConfig {
    bool enabled = false;                      ///< Whether typewriter scrolling is on
    double focusPosition = 0.5;                ///< Height of the cursor line (0 = top, 1 = bottom)
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

    // -------------------------------------------------------------------------
    // DPI Scaling (for WYSIWYG rendering)
    // -------------------------------------------------------------------------

    /// Logical screen DPI (QScreen::logicalDotsPerInch()) - the DPI Qt converts font points
    /// to pixels with, so page sizes in points and margins in mm match the text
    double screenDpi = DEFAULT_DPI;

    /// Page mode: widget pixels per layout pixel at zoom 100%. The screen's physical DPI
    /// over its logical DPI shows the pages at their size on paper; 1 gives the size of the
    /// system's display scaling.
    double paperScale = 1.0;

    // -------------------------------------------------------------------------
    // Typography
    // -------------------------------------------------------------------------

    QFont font{"Segoe UI", 11};               ///< Base font for text

    /// Line spacing, paragraph spacing and first-line indent; lengths in pixels at 100% zoom
    LayoutTypography typography;

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
    PageModeConfig pageMode;                   ///< Page mode config
    TypewriterConfig typewriter;               ///< Typewriter scrolling config

    // -------------------------------------------------------------------------
    // Text Frame Border
    // -------------------------------------------------------------------------

    bool showTextFrameBorder = false;          ///< Show border around text area
    QColor textFrameBorderColor{180, 180, 180}; ///< Border color
    int textFrameBorderWidth = 1;              ///< Border width in pixels

    // -------------------------------------------------------------------------
    // Scroll State
    // -------------------------------------------------------------------------

    double scrollY = 0.0;                      ///< Vertical scroll offset (document units)
    double scrollX = 0.0;                      ///< Horizontal scroll offset (pixels; page mode,
                                               ///< when the zoomed page is wider than the view)
    int currentPageNumber = 1;                 ///< Current page number (1-based, for mirror margins)

    // -------------------------------------------------------------------------
    // Viewport Info
    // -------------------------------------------------------------------------

    QSizeF viewportSize;                       ///< Viewport dimensions
    double scrollBarWidth = 0.0;               ///< Width of the vertical scroll bar over the
                                               ///< view's right edge (pages keep clear of it)

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

        double mmToPixels = DEFAULT_DPI / MM_PER_INCH;  ///< Conversion factor (dpi / 25.4)

        // ---------------------------------------------------------------------
        // Effective font (after zoom in FontScaling mode)
        // ---------------------------------------------------------------------

        QFont effectiveFont;                    ///< Font with zoom applied (FontScaling mode)

        /// Typography handed to the layout: the input with the base font size as the
        /// reference size of its lengths
        LayoutTypography typography;

        // ---------------------------------------------------------------------
        // Effective margins (document units: pixels at 100% zoom)
        // ---------------------------------------------------------------------

        double marginLeft = 50.0;               ///< Left margin (view margin, or page margin)
        double marginTop = 30.0;                ///< Top margin
        double marginRight = 50.0;              ///< Right margin
        double marginBottom = 30.0;             ///< Bottom margin

        // ---------------------------------------------------------------------
        // Effective text width in pixels
        // ---------------------------------------------------------------------

        double textWidth = 700.0;               ///< Available width for text content

        // ---------------------------------------------------------------------
        // Page Mode specific (document units unless noted)
        // ---------------------------------------------------------------------

        double pageWidthPixels = 0.0;           ///< Page width
        double pageHeightPixels = 0.0;          ///< Page height
        QMarginsF pageMargins;                  ///< Page margins (fitted to the page)
        double textAreaHeight = 0.0;            ///< Height of the text area of a page
        double pagePitch = 0.0;                 ///< Page height plus the gap between pages
        double pageCenterOffset = 0.0;          ///< Widget x of the left edge of the pages
        double contentWidth = 0.0;              ///< Zoomed width of the pages with the gaps
                                                ///< around them (widget pixels)

        // ---------------------------------------------------------------------
        // View mapping: widget = origin + (document - (0, scrollY)) * viewScale
        // ---------------------------------------------------------------------

        double originX = 50.0;                  ///< Widget x of document x = 0
        double originY = 30.0;                  ///< Widget y of document y = scrollY
        double scrollPaddingTop = 30.0;         ///< Scroll room above the text (document units)
        double scrollPaddingBottom = 30.0;      ///< Scroll room below the text (document units)

        // ---------------------------------------------------------------------
        // Unified scale factor for rendering
        // ---------------------------------------------------------------------

        double viewScale = 1.0;                 ///< Painter scale (zoom in page mode, else 1)
        double totalScale = 1.0;                ///< Zoom scale (DPI needs none: see screenDpi)

        // ---------------------------------------------------------------------
        // Visible range
        // ---------------------------------------------------------------------

        size_t firstVisibleParagraph = 0;       ///< First visible paragraph index
        size_t lastVisibleParagraph = 0;        ///< Last visible paragraph index

    } computed;
};

}  // namespace kalahari::editor
