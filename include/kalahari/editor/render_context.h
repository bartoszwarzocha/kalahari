/// @file render_context.h
/// @brief RenderContext - All rendering attributes in one struct (OpenSpec #00043 Phase 12.1)
///
/// RenderContext centralizes all rendering configuration in one place.
/// This replaces scattered state across BookEditor, RenderEngine, and EditorAppearance,
/// providing a single source of truth for how content should be rendered.

#pragma once

#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/view_modes.h>
#include <QFont>
#include <QColor>
#include <QMargins>
#include <QSizeF>

namespace kalahari::editor {

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
               inactiveText == other.inactiveText;
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

/// @brief Complete rendering context
///
/// RenderContext contains ALL rendering configuration needed by the pipeline.
/// This is the ONLY place where rendering state is stored.
///
/// Pipeline order when applying context:
/// 1. TEXT - Get content from ITextSource
/// 2. ATTRIBUTES - Apply this context (font, colors, margins)
/// 3. LAYOUT - Calculate block positions (using margins, scale, text width)
/// 4. RENDER - Draw to painter (using colors, cursor config)
///
/// Usage:
/// @code
/// RenderContext ctx;
/// ctx.margins.left = 60.0;
/// ctx.colors.text = Qt::black;
/// ctx.scaleFactor = 1.25;  // 125% zoom
///
/// pipeline.setContext(ctx);
/// pipeline.render(painter, clipRect);
/// @endcode
struct RenderContext {
    // =========================================================================
    // Core Layout Parameters
    // =========================================================================

    RenderMargins margins;                     ///< Margins around content
    double scaleFactor = 1.0;                  ///< Zoom/scale factor (1.0 = 100%)
    double textWidth = 800.0;                  ///< Available width for text (pixels)
    double lineSpacing = 1.0;                  ///< Line spacing multiplier

    // =========================================================================
    // Typography
    // =========================================================================

    QFont font{"Segoe UI", 11};               ///< Base font for text

    // =========================================================================
    // Colors
    // =========================================================================

    RenderColors colors;                       ///< All rendering colors

    // =========================================================================
    // View Mode
    // =========================================================================

    ViewMode viewMode = ViewMode::Continuous;  ///< Current view mode

    // =========================================================================
    // Mode-specific Configuration
    // =========================================================================

    CursorConfig cursor;                       ///< Cursor rendering config
    FocusModeConfig focusMode;                 ///< Focus mode config
    PageModeConfig pageMode;                   ///< Page mode config

    // =========================================================================
    // Text Frame Border
    // =========================================================================

    bool showTextFrameBorder = false;          ///< Show border around text area
    QColor textFrameBorderColor{180, 180, 180}; ///< Border color
    int textFrameBorderWidth = 1;              ///< Border width in pixels

    // =========================================================================
    // Scroll State (for coordinate transforms)
    // =========================================================================

    double scrollY = 0.0;                      ///< Current vertical scroll offset
    int currentPageNumber = 1;                 ///< Current page number (1-based, for mirror margins)

    // =========================================================================
    // Viewport Info (set by render call)
    // =========================================================================

    QSizeF viewportSize;                       ///< Viewport dimensions
    size_t firstVisibleParagraph = 0;          ///< First visible paragraph index
    size_t lastVisibleParagraph = 0;           ///< Last visible paragraph index

    // =========================================================================
    // Computed Properties
    // =========================================================================

    /// @brief Get effective text width after margins and scale
    /// @return Available width for text content
    double effectiveTextWidth() const {
        return (textWidth - margins.left - margins.right) / scaleFactor;
    }

    /// @brief Get left edge of content area
    /// @return X coordinate where content starts
    double contentLeft() const {
        return margins.left;
    }

    /// @brief Get top edge of content area
    /// @return Y coordinate where content starts (before scroll)
    double contentTop() const {
        return margins.top;
    }

    /// @brief Convert document Y to widget Y
    /// @param docY Y coordinate in document space
    /// @return Y coordinate in widget space
    double documentToWidgetY(double docY) const {
        return margins.top + (docY - scrollY) * scaleFactor;
    }

    /// @brief Convert widget Y to document Y
    /// @param widgetY Y coordinate in widget space
    /// @return Y coordinate in document space
    double widgetToDocumentY(double widgetY) const {
        return (widgetY - margins.top) / scaleFactor + scrollY;
    }

    /// @brief Convert document X to widget X
    /// @param docX X coordinate in document space
    /// @return X coordinate in widget space
    double documentToWidgetX(double docX) const {
        return margins.left + docX * scaleFactor;
    }

    /// @brief Convert widget X to document X
    /// @param widgetX X coordinate in widget space
    /// @return X coordinate in document space
    double widgetToDocumentX(double widgetX) const {
        return (widgetX - margins.left) / scaleFactor;
    }

    // =========================================================================
    // Margin Calculation
    // =========================================================================

    /// @brief Calculate effective margins based on view mode and configuration
    /// @param pageMargins Page margin config (for Page/Typewriter)
    /// @param viewMargins View margin config (for Continuous/Focus/DistractionFree)
    /// @param mode Current view mode
    /// @param pageNumber Current page number (for mirror margins)
    /// @param dpi Screen DPI for mm to pixel conversion (default 96)
    /// @return RenderMargins in pixels
    static RenderMargins calculateMargins(
        const PageMarginsConfig& pageMargins,
        const ViewMarginsConfig& viewMargins,
        ViewMode mode,
        int pageNumber = 1,
        double dpi = 96.0)
    {
        // Convert mm to pixels: pixels = mm * dpi / 25.4
        constexpr double MM_TO_INCH = 25.4;
        auto mmToPixels = [dpi](double mm) { return mm * dpi / MM_TO_INCH; };

        switch (mode) {
            case ViewMode::Page:
            case ViewMode::Typewriter:
                // Use page margins (in mm, convert to pixels)
                return RenderMargins{
                    mmToPixels(pageMargins.effectiveLeft(pageNumber)),
                    mmToPixels(pageMargins.top),
                    mmToPixels(pageMargins.effectiveRight(pageNumber)),
                    mmToPixels(pageMargins.bottom)
                };

            case ViewMode::Continuous:
            case ViewMode::Focus:
            case ViewMode::DistractionFree:
            default:
                // Use view margins (already in pixels)
                return RenderMargins{
                    viewMargins.horizontal,
                    viewMargins.vertical,
                    viewMargins.horizontal,
                    viewMargins.vertical
                };
        }
    }

    // =========================================================================
    // Equality
    // =========================================================================

    /// @brief Check if context equals another (for change detection)
    bool operator==(const RenderContext& other) const {
        return margins == other.margins &&
               scaleFactor == other.scaleFactor &&
               textWidth == other.textWidth &&
               lineSpacing == other.lineSpacing &&
               font == other.font &&
               colors == other.colors &&
               viewMode == other.viewMode;
    }

    bool operator!=(const RenderContext& other) const {
        return !(*this == other);
    }
};

}  // namespace kalahari::editor
