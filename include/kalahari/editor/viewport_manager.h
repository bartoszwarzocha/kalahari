/// @file viewport_manager.h
/// @brief Viewport manager for Word/Writer-style scrolling (OpenSpec #00043 Phase 4)
///
/// ViewportManager coordinates viewport state with TextBuffer and LazyLayoutManager.
/// It determines which paragraphs are visible, manages scrolling, and calculates
/// scrollbar position from mixed real/estimated heights.
///
/// Key features:
/// - Viewport size and scroll position management
/// - Visible paragraph range calculation
/// - Buffer zone management for smooth scrolling
/// - Scrollbar position from mixed heights
/// - Qt signals for viewport changes

#pragma once

#include <kalahari/editor/text_buffer.h>
#include <kalahari/editor/lazy_layout_manager.h>
#include <QObject>
#include <QSize>
#include <QRectF>
#include <memory>

namespace kalahari::editor {

/// @brief Default buffer size (paragraphs to pre-layout above/below viewport)
constexpr size_t DEFAULT_BUFFER_SIZE = 50;

/// @brief Viewport manager for coordinated scrolling
///
/// ViewportManager provides a high-level interface for viewport management
/// in the Word/Writer architecture. It works with TextBuffer (for heights)
/// and LazyLayoutManager (for layouts) to provide efficient scrolling.
///
/// Usage:
/// @code
/// TextBuffer buffer;
/// buffer.setPlainText(largeDocument);
///
/// LazyLayoutManager layoutManager(&buffer);
///
/// ViewportManager viewport;
/// viewport.setBuffer(&buffer);
/// viewport.setLayoutManager(&layoutManager);
/// viewport.setViewportSize(QSize(800, 600));
///
/// // Connect to signals
/// connect(&viewport, &ViewportManager::viewportChanged, this, &MyWidget::onViewportChanged);
///
/// // Scroll to position
/// viewport.setScrollPosition(1000.0);
///
/// // Get visible range and render
/// auto [first, last] = viewport.visibleRange();
/// @endcode
///
/// Thread safety: Not thread-safe. Use from GUI thread only.
class ViewportManager : public QObject, public ITextBufferObserver {
    Q_OBJECT

public:
    /// @brief Construct viewport manager
    explicit ViewportManager(QObject* parent = nullptr);

    /// @brief Destructor
    ~ViewportManager() override;

    // =========================================================================
    // Component Integration
    // =========================================================================

    /// @brief Set the text buffer
    /// @param buffer TextBuffer to track (must outlive manager)
    void setBuffer(TextBuffer* buffer);

    /// @brief Get the text buffer
    TextBuffer* buffer() const { return m_buffer; }

    /// @brief Set the layout manager
    /// @param manager LazyLayoutManager (must outlive viewport manager)
    void setLayoutManager(LazyLayoutManager* manager);

    /// @brief Get the layout manager
    LazyLayoutManager* layoutManager() const { return m_layoutManager; }

    // =========================================================================
    // Viewport Configuration
    // =========================================================================

    /// @brief Set the viewport size
    /// @param size Viewport dimensions (width x height)
    void setViewportSize(const QSize& size);

    /// @brief Get the viewport size
    QSize viewportSize() const { return m_viewportSize; }

    /// @brief Get the viewport width
    int viewportWidth() const { return m_viewportSize.width(); }

    /// @brief Get the viewport height
    int viewportHeight() const { return m_viewportSize.height(); }

    /// @brief Set the buffer size (paragraphs to pre-layout)
    /// @param paragraphs Number of paragraphs above/below viewport
    void setBufferSize(size_t paragraphs);

    /// @brief Get the buffer size
    size_t bufferSize() const { return m_bufferSize; }

    // =========================================================================
    // Scroll Position
    // =========================================================================

    /// @brief Set the scroll position (Y coordinate)
    /// @param y Scroll offset in pixels
    void setScrollPosition(double y);

    /// @brief Get the scroll position
    double scrollPosition() const { return m_scrollY; }

    /// @brief Scroll by delta amount
    /// @param delta Pixels to scroll (+ve = down, -ve = up)
    void scrollBy(double delta);

    /// @brief Scroll to make paragraph visible
    /// @param index Paragraph index
    /// @return New scroll position
    double scrollToMakeParagraphVisible(size_t index);

    /// @brief Get maximum scroll position
    double maxScrollPosition() const;

    /// @brief Clamp scroll position to valid range
    double clampScrollPosition(double y) const;

    // =========================================================================
    // Visible Range
    // =========================================================================

    /// @brief Get first visible paragraph (in viewport)
    size_t firstVisibleParagraph() const { return m_firstVisible; }

    /// @brief Get last visible paragraph (in viewport)
    size_t lastVisibleParagraph() const { return m_lastVisible; }

    /// @brief Get visible range as pair
    std::pair<size_t, size_t> visibleRange() const {
        return {m_firstVisible, m_lastVisible};
    }

    /// @brief Get buffer start (first paragraph to pre-layout)
    size_t bufferStart() const;

    /// @brief Get buffer end (last paragraph to pre-layout)
    size_t bufferEnd() const;

    /// @brief Get full range including buffer
    std::pair<size_t, size_t> bufferedRange() const {
        return {bufferStart(), bufferEnd()};
    }

    /// @brief Check if paragraph is in visible range
    bool isParagraphVisible(size_t index) const;

    /// @brief Check if paragraph is in buffered range
    bool isParagraphInBuffer(size_t index) const;

    // =========================================================================
    // Scrollbar
    // =========================================================================

    /// @brief Get scrollbar position (0.0 to 1.0)
    /// @return Normalized scroll position
    double scrollbarPosition() const;

    /// @brief Get scrollbar thumb size (0.0 to 1.0)
    /// @return Proportion of content visible
    double scrollbarThumbSize() const;

    /// @brief Set scroll position from scrollbar (0.0 to 1.0)
    /// @param position Normalized position
    void setScrollbarPosition(double position);

    /// @brief Check if scrollbar is needed
    bool isScrollbarNeeded() const;

    // =========================================================================
    // Geometry Queries
    // =========================================================================

    /// @brief Get viewport rectangle in document coordinates
    QRectF viewportRect() const;

    /// @brief Get total document height
    double totalDocumentHeight() const;

    /// @brief Find paragraph at Y coordinate
    size_t paragraphAtY(double y) const;

    /// @brief Get Y position of paragraph
    double paragraphY(size_t index) const;

    /// @brief Get height of paragraph
    double paragraphHeight(size_t index) const;

    // =========================================================================
    // Layout Coordination
    // =========================================================================

    /// @brief Request layout for visible + buffer paragraphs
    ///
    /// Calls layoutManager->layoutVisibleParagraphs() and updates state.
    void requestLayout();

    /// @brief Update layout manager viewport
    void syncLayoutManagerViewport();

    // =========================================================================
    // ITextBufferObserver Implementation
    // =========================================================================

    void onTextChanged() override;
    void onParagraphInserted(size_t index) override;
    void onParagraphRemoved(size_t index) override;
    void onParagraphChanged(size_t index) override;
    void onHeightChanged(size_t index, double oldHeight, double newHeight) override;

signals:
    /// @brief Emitted when viewport position or size changes
    void viewportChanged();

    /// @brief Emitted when visible paragraph range changes
    /// @param first First visible paragraph
    /// @param last Last visible paragraph
    void visibleRangeChanged(size_t first, size_t last);

    /// @brief Emitted when scroll position changes
    /// @param position New scroll position
    void scrollPositionChanged(double position);

    /// @brief Emitted when layout is needed for paragraphs
    /// @param first First paragraph to layout
    /// @param last Last paragraph to layout
    void layoutRequested(size_t first, size_t last);

    /// @brief Emitted when total document height changes
    /// @param newHeight New total height
    void documentHeightChanged(double newHeight);

private:
    /// @brief Update visible range from current scroll position
    void updateVisibleRange();

    /// @brief Emit signals for range change
    void notifyRangeChanged();

    TextBuffer* m_buffer = nullptr;
    LazyLayoutManager* m_layoutManager = nullptr;

    QSize m_viewportSize{0, 0};
    double m_scrollY = 0.0;
    size_t m_bufferSize = DEFAULT_BUFFER_SIZE;

    size_t m_firstVisible = 0;
    size_t m_lastVisible = 0;

    // Cached values
    mutable double m_cachedTotalHeight = 0.0;
    mutable bool m_totalHeightDirty = true;
};

}  // namespace kalahari::editor
