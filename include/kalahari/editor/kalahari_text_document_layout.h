/// @file kalahari_text_document_layout.h
/// @brief Custom QAbstractTextDocumentLayout without Qt leading gaps (OpenSpec #00043)

#pragma once

#include <kalahari/editor/editor_types.h>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QFont>
#include <vector>

class QTextLayout;
class QTextLine;

namespace kalahari::editor {

/// @brief Custom document layout that positions text lines without Qt's internal leading
///
/// Qt's default QTextDocumentLayout adds font.leading() between lines and document
/// margins around the text. This layout stacks blocks directly below each other and
/// spaces lines only as the view typography asks for.
///
/// Key features:
/// - Line spacing, paragraph spacing and first-line indent from LayoutTypography; the
///   extra line spacing is split evenly above and below each line
/// - Wrap width and default font are owned by the QTextDocument (textWidth(),
///   defaultFont()); changing either there re-lays out the document exactly once
/// - Incremental updates: an edit re-lays out only the blocks it touched
/// - Block positions answered from cached heights (binary search for a y position)
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

    /// @brief Bounding rect of a text block, including its paragraph spacing
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

    /// @brief Set the view typography; re-lays out every block when it changes
    void setTypography(const LayoutTypography& typography);
    const LayoutTypography& typography() const { return m_typography; }

    /// @brief Paragraph spacing at the current document font (whole pixels)
    qreal paragraphSpacing() const;

    /// @brief First-line indent at the current document font
    qreal firstLineIndent() const;

    /// @brief Force layout of all blocks
    void layoutAllBlocks();

    // ==========================================================================
    // Geometry
    // ==========================================================================

    /// @brief Top of a block in document coordinates (document height past the end)
    qreal blockY(int blockNumber) const;

    /// @brief Height of a block, including its paragraph spacing
    qreal blockHeight(int blockNumber) const;

    /// @brief Number of the block covering @p y (clamped to the first/last block)
    /// @return -1 for an empty height cache
    int blockNumberAtY(qreal y) const;

    /// @brief Box of a laid out line: the line plus its share of the line spacing
    ///
    /// Consecutive boxes of a block touch, so every y within the block's lines belongs
    /// to exactly one line. Block coordinates.
    static QRectF lineBox(const QTextLine& line, qreal lineSpacing);

    /// @brief Index of the line whose box covers @p localY (block coordinates)
    ///
    /// Points above the first box or below the last one give the first or last line.
    /// @return -1 when the layout has no lines
    static int lineIndexAt(const QTextLayout& layout, qreal localY, qreal lineSpacing);

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

    /// @brief Break a single block into lines
    /// @param spacing Paragraph spacing, @param indent first-line indent (current font)
    /// @return Height of the block, including its paragraph spacing
    qreal layoutBlock(const QTextBlock& block, qreal spacing, qreal indent) const;

    /// @brief Factor from the typography's reference font size to the document font
    qreal typographyScale() const;

    /// @brief Recalculate cumulative block positions from the height cache
    void updateBlockPositions() const;

    /// @brief Width reported for the document and its blocks
    qreal documentWidth() const;

    // View typography used for every block
    LayoutTypography m_typography;

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
