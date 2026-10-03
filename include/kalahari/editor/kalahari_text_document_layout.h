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
/// - Wrap width and default font are owned by the QTextDocument (textWidth(),
///   defaultFont()); changing either there re-lays out the document exactly once
/// - Incremental updates: an edit re-lays out only the blocks it touched
/// - Full QTextCursor and undo/redo compatibility
class KalahariTextDocumentLayout : public QAbstractTextDocumentLayout {
    Q_OBJECT

public:
    explicit KalahariTextDocumentLayout(QTextDocument* doc);
    ~KalahariTextDocumentLayout() override = default;

    // ==========================================================================
    // QAbstractTextDocumentLayout required overrides
    // ==========================================================================

    /// @brief Draw the blocks intersecting the paint context's clip rect
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

    /// @brief Set the wrap width - forwards to QTextDocument::setTextWidth()
    ///
    /// The document's text width is the single source of truth, so setting it on the
    /// document directly has the same effect. A width <= 0 disables wrapping.
    /// Does nothing when the width is unchanged.
    void setTextWidth(qreal width);
    qreal textWidth() const;

    /// @brief Set the default font - forwards to QTextDocument::setDefaultFont()
    ///
    /// Does nothing when the font is unchanged.
    void setFont(const QFont& font);
    QFont font() const;

    /// @brief Force layout of all blocks
    void layoutAllBlocks();

signals:
    /// @brief Emitted after blocks have been laid out
    /// @param firstBlock Number of the first block laid out
    /// @param blockCount Number of consecutive blocks laid out
    void blocksLaidOut(int firstBlock, int blockCount);

protected:
    /// @brief Called by Qt when document content changes
    void documentChanged(int from, int charsRemoved, int charsAdded) override;

private:
    /// @brief Lay out blocks [first, last] (by number) and update the height cache
    /// @param oldLast Number of the last block of the changed range before the change
    void relayoutRange(int first, int last, int oldLast);

    /// @brief Break a single block into lines, starting at y=0
    void layoutBlock(const QTextBlock& block) const;

    /// @brief Height of a laid out block, measured from its QTextLayout
    qreal measuredHeight(const QTextBlock& block) const;

    /// @brief Recalculate cumulative block positions from the height cache
    void updateBlockPositions() const;

    /// @brief Get Y position of a block
    qreal blockY(int blockNumber) const;

    /// @brief Number of the block covering @p y (clamped to the first/last block)
    int blockNumberAtY(qreal y) const;

    /// @brief Width reported for the document and its blocks
    qreal documentWidth() const;

    // Height of every block, indexed by block number; kept in step with the document
    std::vector<qreal> m_blockHeights;

    // Cached block Y positions (cumulative heights)
    mutable std::vector<qreal> m_blockYPositions;
    mutable bool m_positionsDirty = true;
    mutable qreal m_cachedDocumentHeight = 0;

    // Last size announced through documentSizeChanged()
    QSizeF m_lastReportedSize;
};

}  // namespace kalahari::editor
