/// @file kalahari_text_document_layout.h
/// @brief Custom QAbstractTextDocumentLayout without Qt leading gaps (OpenSpec #00043)

#pragma once

#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QFont>
#include <vector>

namespace kalahari::editor {

/// @brief Custom document layout that positions text lines without Qt's internal leading
///
/// Qt's default QTextDocumentLayout adds font.leading() between lines, causing gaps
/// between paragraphs. This class provides a layout where lines start at y=0 within
/// each block, eliminating the gaps while maintaining full Qt integration.
///
/// Key features:
/// - Lines positioned at y=0 within each block (no leading gaps)
/// - Proper text wrapping support via setTextWidth()
/// - Full QTextCursor and undo/redo compatibility
/// - Efficient incremental layout updates
class KalahariTextDocumentLayout : public QAbstractTextDocumentLayout {
    Q_OBJECT

public:
    explicit KalahariTextDocumentLayout(QTextDocument* doc);
    ~KalahariTextDocumentLayout() override = default;

    // ==========================================================================
    // QAbstractTextDocumentLayout required overrides
    // ==========================================================================

    /// @brief Draw the document
    void draw(QPainter* painter, const PaintContext& context) override;

    /// @brief Hit test - convert point to document position
    int hitTest(const QPointF& point, Qt::HitTestAccuracy accuracy) const override;

    /// @brief Number of pages (always 1 for continuous layout)
    int pageCount() const override;

    /// @brief Total document size
    QSizeF documentSize() const override;

    /// @brief Bounding rect of a text frame
    QRectF frameBoundingRect(QTextFrame* frame) const override;

    /// @brief Bounding rect of a text block
    QRectF blockBoundingRect(const QTextBlock& block) const override;

    // ==========================================================================
    // Configuration
    // ==========================================================================

    /// @brief Set the text width for wrapping
    void setTextWidth(qreal width);
    qreal textWidth() const { return m_textWidth; }

    /// @brief Set the font for layout
    void setFont(const QFont& font);
    QFont font() const { return m_font; }

protected:
    /// @brief Called by Qt when document content changes
    void documentChanged(int from, int charsRemoved, int charsAdded) override;

private:
    /// @brief Prepare layout for a single block with lines at y=0
    void layoutBlock(QTextBlock& block);

    /// @brief Recalculate all block positions
    void updateBlockPositions();

    /// @brief Get Y position of a block
    qreal blockY(int blockNumber) const;

    /// @brief Get height of a block
    qreal blockHeight(const QTextBlock& block) const;

    // Layout state
    qreal m_textWidth = 0;
    QFont m_font;

    // Cached block Y positions (cumulative)
    mutable std::vector<qreal> m_blockYPositions;
    mutable bool m_positionsDirty = true;
    mutable qreal m_cachedDocumentHeight = 0;
};

}  // namespace kalahari::editor
