/// @file viewport_manager.h
/// @brief Viewport manager for Word/Writer-style scrolling (OpenSpec #00043 Phase 11.8)
///
/// ViewportManager coordinates viewport state with QTextDocument.
/// It determines which paragraphs (blocks) are visible, manages scrolling, and calculates
/// scrollbar position from document layout heights.
///
/// Key features:
/// - Viewport size and scroll position management
/// - Visible block range calculation
/// - Buffer zone management for smooth scrolling
/// - Scrollbar position from layout heights
/// - Qt signals for viewport changes

#pragma once

#include <QObject>
#include <QSize>
#include <QRectF>
#include <QTextDocument>
#include <QTextBlock>
#include <memory>

namespace kalahari::editor {

/// @brief Default buffer size (paragraphs to pre-layout above/below viewport)
constexpr size_t DEFAULT_BUFFER_SIZE = 50;

/// @brief Viewport manager for coordinated scrolling
///
/// ViewportManager provides a high-level interface for viewport management
/// in the Word/Writer architecture. It works directly with QTextDocument
/// and uses QAbstractTextDocumentLayout for efficient scrolling.
///
/// Usage:
/// @code
/// QTextDocument document;
/// document.setPlainText(largeDocument);
///
/// ViewportManager viewport;
/// viewport.setDocument(&document);
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
class ViewportManager : public QObject {
    Q_OBJECT

public:
    /// @brief Construct viewport manager
    explicit ViewportManager(QObject* parent = nullptr);

    /// @brief Destructor
    ~ViewportManager() override;

    // =========================================================================
    // Component Integration
    // =========================================================================

    /// @brief Set the text document
    /// @param document QTextDocument to track (must outlive manager)
    void setDocument(QTextDocument* document);

    /// @brief Get the text document
    QTextDocument* document() const { return m_document; }

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

private slots:
    /// @brief Handle document content changes
    void onDocumentChanged();

private:
    /// @brief Update visible range from current scroll position
    void updateVisibleRange();

    /// @brief Emit signals for range change
    void notifyRangeChanged();

    /// @brief Get height of a text block
    /// @param block The text block
    /// @return Block height in pixels
    double blockHeight(const QTextBlock& block) const;

    QTextDocument* m_document = nullptr;

    QSize m_viewportSize{0, 0};
    double m_scrollY = 0.0;
    size_t m_bufferSize = DEFAULT_BUFFER_SIZE;

    size_t m_firstVisible = 0;
    size_t m_lastVisible = 0;

    /// @brief Estimated line height for blocks without layout
    double m_estimatedLineHeight = 20.0;

    // Cached values
    mutable double m_cachedTotalHeight = 0.0;
    mutable bool m_totalHeightDirty = true;
};

}  // namespace kalahari::editor
