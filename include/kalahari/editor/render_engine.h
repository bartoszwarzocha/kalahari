/// @file render_engine.h
/// @brief RenderEngine for viewport-only paragraph rendering (OpenSpec #00043 Phase 11.4)
///
/// RenderEngine is responsible for rendering only visible paragraphs to the screen.
/// It uses QTextDocument directly and integrates with ViewportManager for visibility.

#pragma once

#include <kalahari/editor/editor_types.h>
#include <QObject>
#include <QPainter>
#include <QRegion>
#include <QColor>
#include <QFont>
#include <QTimer>
#include <QTextDocument>
#include <QTextBlock>
#include <memory>

namespace kalahari::editor {

class ViewportManager;
class SearchEngine;

// Use SelectionRange from editor_types.h for selections
// Use CursorPosition from editor_types.h for cursor

/// @brief RenderEngine handles efficient viewport-only rendering
///
/// Key features:
/// - Dirty region tracking with QRegion for minimal repaints
/// - Viewport-only paragraph rendering
/// - Selection and cursor rendering
/// - Uses QTextDocument directly with QTextBlock layouts
class RenderEngine : public QObject {
    Q_OBJECT

public:
    explicit RenderEngine(QObject* parent = nullptr);
    ~RenderEngine() override;

    // =========================================================================
    // Component Integration
    // =========================================================================

    /// @brief Set the text document for content access
    void setDocument(QTextDocument* doc);
    QTextDocument* document() const { return m_document; }

    /// @brief Set the viewport manager for visibility calculations
    void setViewportManager(ViewportManager* viewport);
    ViewportManager* viewportManager() const { return m_viewportManager; }

    /// @brief Set the search engine for match highlighting
    void setSearchEngine(SearchEngine* engine);
    SearchEngine* searchEngine() const { return m_searchEngine; }

    /// @brief Set search match highlight color
    void setSearchHighlightColor(const QColor& color);
    QColor searchHighlightColor() const { return m_searchHighlightColor; }

    /// @brief Set current match highlight color
    void setCurrentMatchColor(const QColor& color);
    QColor currentMatchColor() const { return m_currentMatchColor; }

    /// @brief Set comment highlight background color
    void setCommentHighlightColor(const QColor& color);
    QColor commentHighlightColor() const { return m_commentHighlightColor; }

    /// @brief Set comment border/underline color
    void setCommentBorderColor(const QColor& color);
    QColor commentBorderColor() const { return m_commentBorderColor; }

    /// @brief Set TODO marker highlight color
    void setTodoHighlightColor(const QColor& color);
    QColor todoHighlightColor() const { return m_todoHighlightColor; }

    /// @brief Set NOTE marker highlight color
    void setNoteHighlightColor(const QColor& color);
    QColor noteHighlightColor() const { return m_noteHighlightColor; }

    /// @brief Set completed TODO color (dimmed)
    void setCompletedTodoColor(const QColor& color);
    QColor completedTodoColor() const { return m_completedTodoColor; }

    // =========================================================================
    // Appearance Configuration
    // =========================================================================

    /// @brief Set the base font for rendering
    void setFont(const QFont& font);
    QFont font() const { return m_font; }

    /// @brief Set the background color
    void setBackgroundColor(const QColor& color);
    QColor backgroundColor() const { return m_backgroundColor; }

    /// @brief Set the text color
    void setTextColor(const QColor& color);
    QColor textColor() const { return m_textColor; }

    /// @brief Set the selection background color
    void setSelectionColor(const QColor& color);
    QColor selectionColor() const { return m_selectionColor; }

    /// @brief Set the selection text color
    void setSelectionTextColor(const QColor& color);
    QColor selectionTextColor() const { return m_selectionTextColor; }

    /// @brief Set the cursor color
    void setCursorColor(const QColor& color);
    QColor cursorColor() const { return m_cursorColor; }

    /// @brief Set left margin in pixels
    void setLeftMargin(double margin);
    double leftMargin() const { return m_leftMargin; }

    /// @brief Set top margin in pixels
    void setTopMargin(double margin);
    double topMargin() const { return m_topMargin; }

    /// @brief Set right margin in pixels
    void setRightMargin(double margin);
    double rightMargin() const { return m_rightMargin; }

    /// @brief Set line spacing multiplier (1.0 = single, 1.5 = 1.5x, 2.0 = double)
    void setLineSpacing(double spacing);
    double lineSpacing() const { return m_lineSpacing; }

    // =========================================================================
    // Dirty Region Tracking
    // =========================================================================

    /// @brief Mark a rectangular region as needing repaint
    void markDirty(const QRect& region);

    /// @brief Mark an entire paragraph as needing repaint
    void markParagraphDirty(size_t paragraphIndex);

    /// @brief Mark the entire viewport as needing repaint
    void markAllDirty();

    /// @brief Get the current dirty region
    QRegion dirtyRegion() const { return m_dirtyRegion; }

    /// @brief Check if any region is dirty
    bool isDirty() const { return !m_dirtyRegion.isEmpty(); }

    /// @brief Clear all dirty regions (call after paint)
    void clearDirtyRegion();

    // =========================================================================
    // Selection
    // =========================================================================

    /// @brief Set the current text selection
    void setSelection(const SelectionRange& selection);

    /// @brief Get the current selection
    SelectionRange selection() const { return m_selection; }

    /// @brief Clear the selection
    void clearSelection();

    /// @brief Check if there is an active selection
    bool hasSelection() const { return !m_selection.isEmpty(); }

    // =========================================================================
    // Cursor
    // =========================================================================

    /// @brief Set the cursor position
    void setCursorPosition(const CursorPosition& position);

    /// @brief Get the cursor position
    CursorPosition cursorPosition() const { return m_cursorPosition; }

    /// @brief Set cursor visibility (for focus state)
    void setCursorVisible(bool visible);
    bool isCursorVisible() const { return m_cursorVisible; }

    /// @brief Set cursor blink interval in milliseconds (0 = no blink)
    void setCursorBlinkInterval(int ms);
    int cursorBlinkInterval() const { return m_cursorBlinkInterval; }

    /// @brief Start cursor blinking
    void startCursorBlink();

    /// @brief Stop cursor blinking
    void stopCursorBlink();

    /// @brief Get the cursor rectangle in widget coordinates
    QRectF cursorRect() const;

    /// @brief Set cursor width in pixels
    void setCursorWidth(double width);
    double cursorWidth() const { return m_cursorWidth; }

    // =========================================================================
    // Paint
    // =========================================================================

    /// @brief Paint the visible content to the given painter
    /// @param painter The QPainter to draw with
    /// @param clipRect The clip rectangle (from paint event)
    /// @param viewportSize The size of the viewport widget
    void paint(QPainter* painter, const QRect& clipRect, const QSize& viewportSize);

    /// @brief Paint only the dirty regions
    /// @param painter The QPainter to draw with
    /// @param viewportSize The size of the viewport widget
    void paintDirty(QPainter* painter, const QSize& viewportSize);

    // =========================================================================
    // Geometry Queries
    // =========================================================================

    /// @brief Get the Y position of a paragraph in document coordinates
    double paragraphY(size_t index) const;

    /// @brief Get the bounding rect of a paragraph in widget coordinates
    QRectF paragraphRect(size_t index) const;

    /// @brief Convert document Y coordinate to widget Y coordinate
    double documentToWidgetY(double docY) const;

    /// @brief Convert widget Y coordinate to document Y coordinate
    double widgetToDocumentY(double widgetY) const;

signals:
    /// @brief Emitted when a repaint is needed
    void repaintRequested(const QRegion& region);

    /// @brief Emitted when cursor blink state changes
    void cursorBlinkChanged(bool visible);

private slots:
    /// @brief Handle cursor blink timer
    void onCursorBlinkTimer();

private:
    // =========================================================================
    // Paint Helpers
    // =========================================================================

    /// @brief Paint the background
    void paintBackground(QPainter* painter, const QRect& clipRect);

    /// @brief Paint a single paragraph
    void paintParagraph(QPainter* painter, const QTextBlock& block, double y);

    /// @brief Paint the selection highlight
    void paintSelection(QPainter* painter);

    /// @brief Paint the cursor
    void paintCursor(QPainter* painter);

    /// @brief Paint search match highlights
    void paintSearchHighlights(QPainter* painter, const QRect& clipRect);

    /// @brief Paint comment highlights
    void paintCommentHighlights(QPainter* painter, const QRect& clipRect);

    /// @brief Paint TODO/NOTE marker highlights
    void paintMarkerHighlights(QPainter* painter, const QRect& clipRect);

    /// @brief Get rectangle for text range in a paragraph
    /// @param paraIndex Paragraph index
    /// @param offset Character offset within paragraph
    /// @param length Text length in characters
    /// @return Rectangle in widget coordinates, or empty if not visible
    QRectF getTextRect(size_t paraIndex, int offset, int length) const;

    /// @brief Calculate the rectangle for selection in a paragraph
    QRectF selectionRectForParagraph(size_t paraIndex, size_t startOffset,
                                      size_t endOffset, double paraY) const;

    // =========================================================================
    // Members
    // =========================================================================

    // Component references
    QTextDocument* m_document = nullptr;
    ViewportManager* m_viewportManager = nullptr;
    SearchEngine* m_searchEngine = nullptr;

    // Appearance
    QFont m_font;
    QColor m_backgroundColor{255, 255, 255};
    QColor m_textColor{0, 0, 0};
    QColor m_selectionColor{51, 153, 255, 128};  // Semi-transparent blue
    QColor m_selectionTextColor{255, 255, 255};
    QColor m_cursorColor{0, 0, 0};
    QColor m_searchHighlightColor{255, 255, 0, 128};   // Yellow semi-transparent
    QColor m_currentMatchColor{255, 165, 0, 180};      // Orange more opaque
    QColor m_commentHighlightColor{255, 255, 150, 100};  // Light yellow, semi-transparent
    QColor m_commentBorderColor{255, 200, 0};            // Orange border
    QColor m_todoHighlightColor{255, 200, 200, 100};     // Light red, semi-transparent
    QColor m_noteHighlightColor{200, 220, 255, 100};     // Light blue, semi-transparent
    QColor m_completedTodoColor{180, 180, 180, 80};      // Gray, dimmed
    double m_leftMargin = 10.0;
    double m_topMargin = 10.0;
    double m_rightMargin = 10.0;
    double m_lineSpacing = 1.0;
    double m_cursorWidth = 2.0;

    // Dirty tracking
    QRegion m_dirtyRegion;

    // Selection
    SelectionRange m_selection;

    // Cursor
    CursorPosition m_cursorPosition;
    bool m_cursorVisible = true;
    bool m_cursorBlinkState = true;  // Current blink state (on/off)
    int m_cursorBlinkInterval = 500;
    QTimer m_cursorBlinkTimer;
};

}  // namespace kalahari::editor
