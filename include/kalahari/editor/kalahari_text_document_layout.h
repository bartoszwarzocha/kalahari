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
class QTimer;

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
///   defaultFont())
/// - Layout on demand: after a width, font or typography change, a load or a large edit,
///   blocks wait for layout with an estimated height. The view lays out what it shows
///   (ensureLaidOut()), a background pass the rest, in steps of a few milliseconds.
///   Small edits lay out the blocks they touched at once.
/// - Glyphs are cached only for blocks laid out after an edit: the others keep their
///   line breaks, so memory does not grow with the length of the document
/// - Block positions answered from cached heights (binary search for a y position)
/// - Page flow (page mode): lines are placed only within the text areas of a stack of
///   pages; a line that would cross the end of a text area moves to the next one. After
///   an edit, only blocks whose place on their page changed are placed again.
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

    /// @brief Hit test - convert point to document position (lays out the block hit)
    ///
    /// Qt::FuzzyHit returns the nearest position between characters. Qt::ExactHit returns
    /// the character under the point, or -1 when the point is not on the text of a line.
    int hitTest(const QPointF& point, Qt::HitTestAccuracy accuracy) const override;

    /// @brief Number of pages: 1 without page flow, else the pages the text fills
    ///        (estimated while blocks wait for layout)
    int pageCount() const override;

    /// @brief Total document size; with page flow its height ends at the bottom of the
    ///        last page's text area
    QSizeF documentSize() const override;

    /// @brief Bounding rect of a text frame
    QRectF frameBoundingRect(QTextFrame* frame) const override;

    /// @brief Bounding rect of a text block, including its paragraph spacing (from the
    ///        height cache: estimated while the block waits for layout)
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

    /// @brief Set the view typography; every block waits for layout again when it changes
    void setTypography(const LayoutTypography& typography);
    const LayoutTypography& typography() const { return m_typography; }

    /// @brief Set the page flow; every block waits for layout again when it changes
    ///
    /// With page flow, line breaks follow the font's design metrics instead of its
    /// screen metrics, so that they stay the same when a scaled painter zooms the pages.
    void setPageFlow(const PageFlow& flow);
    const PageFlow& pageFlow() const { return m_pageFlow; }

    /// @brief Paragraph spacing at the current document font (whole pixels)
    qreal paragraphSpacing() const;

    /// @brief First-line indent at the current document font
    qreal firstLineIndent() const;

    /// @brief Lay out every block now, also the ones already laid out
    void layoutAllBlocks();

    // ==========================================================================
    // Layout on demand
    // ==========================================================================

    /// @brief True when the block's lines follow the current width, font and typography
    bool isLaidOut(int blockNumber) const;

    /// @brief Number of blocks waiting for layout (their heights are estimates)
    int pendingBlockCount() const { return m_pendingCount; }

    /// @brief Lay out the waiting blocks among [first, last] (block numbers)
    /// @return true when a block was laid out - block heights may have changed
    bool ensureLaidOut(int first, int last);

    /// @brief Lay out every waiting block now, as the background pass would
    void layoutPendingBlocks();

    /// @brief Lay out the waiting blocks from the start of the document down to y
    ///
    /// Positions down to y are then exact: with page flow, the pages above y hold their
    /// final lines (estimated heights above a page shift its text).
    void ensureLaidOutTo(qreal y);

    /// @brief The block's lines, laid out first when the block waits for layout
    ///
    /// Use it instead of QTextBlock::layout() wherever the lines are read. A block of a
    /// document with another layout is returned as it is.
    static QTextLayout* blockLayout(const QTextBlock& block);

    // ==========================================================================
    // Geometry
    // ==========================================================================

    /// @brief Top of a block in document coordinates (document height past the end)
    qreal blockY(int blockNumber) const;

    /// @brief Height of a block, including its paragraph spacing (estimated while the
    ///        block waits for layout)
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

    /// @brief Emitted when the heights or lines of blocks from @p firstBlock on may have
    ///        changed while the content stayed the same: a new width, font or typography,
    ///        or waiting blocks laid out (content changes are announced by the document)
    void blockGeometryChanged(int firstBlock);

protected:
    /// @brief Called by Qt when document content changes
    void documentChanged(int from, int charsRemoved, int charsAdded) override;

private:
    /// @brief Replace the cache entries of blocks [first, last] after an edit
    ///
    /// A small range is laid out at once; a large one waits for layout.
    /// @param oldLast Number of the last block of the changed range before the change
    void replaceRange(int first, int last, int oldLast);

    /// @brief Every block waits for layout, with an estimated height
    void invalidateAll();

    /// @brief Lay out the waiting blocks among [first, last] while time is left
    /// @param budgetMs Time limit in milliseconds (negative: no limit)
    /// @return Number of the last block looked at
    int layOutPending(int first, int last, qint64 budgetMs);

    /// @brief One step of the background pass
    void layoutStep();

    /// @brief Lay out the block and store its height and text extent
    /// @return true when its height changed
    bool layOutBlock(int number, const QTextBlock& block, qreal spacing, qreal indent);

    /// @brief Break a single block into lines
    /// @param spacing Paragraph spacing, @param indent first-line indent (current font)
    /// @param extent Receives the text length in average characters (for estimates)
    /// @param keepGlyphs Keep the shaped glyphs with the lines (blocks being edited)
    /// @return Height of the block, including its paragraph spacing
    qreal layoutBlock(const QTextBlock& block, qreal spacing, qreal indent, qreal* extent,
                      bool keepGlyphs) const;

    /// @brief Height a block of the given text extent probably gets at the current settings
    qreal estimatedHeight(qreal extent, qreal spacing) const;

    /// @brief Character width and line height the estimates use (current font)
    void updateEstimateMetrics();

    /// @brief Announce a new document size and the area to repaint
    /// @param first First block that changed, @param bottom lowest y the change can affect
    void notifyGeometryChanged(int first, qreal bottom);

    /// @brief Factor from the typography's reference font size to the document font
    qreal typographyScale() const;

    /// @brief Recalculate cumulative block positions from the height cache
    ///
    /// With page flow, also places the lines of every laid out block whose place on its
    /// page changed (or that was laid out again).
    void updateBlockPositions() const;

    /// @brief Top of a line box of the given height in the page flow
    /// @param top Top the box would have without page breaks (document coordinates)
    /// @return @p top, or the top of the next text area when the box starts between two
    ///         areas, or would cross the end of its area below another line
    qreal flowLineTop(qreal top, qreal height) const;

    /// @brief Place a laid out block's lines in the page flow
    /// @param top Top of the block (document coordinates)
    /// @return Height of the block in the flow, including its paragraph spacing
    qreal paginateBlock(QTextLayout* layout, qreal top, qreal spacing) const;

    /// @brief Height a block waiting for layout probably takes in the page flow
    /// @param height Its estimated height without page breaks
    qreal flowEstimate(qreal height, qreal top, qreal spacing) const;

    /// @brief Height the page flow gives the document: down to the end of the text area
    ///        of the page holding the last line
    qreal flowDocumentHeight() const;

    /// @brief Width reported for the document and its blocks
    qreal documentWidth() const;

    /// @brief Number of a laid out block covering @p y (lays out blocks as needed)
    int laidOutBlockAtY(qreal y) const;

    // View typography used for every block
    LayoutTypography m_typography;

    // Page flow (page mode); its text areas in whole pixels
    PageFlow m_pageFlow;

    // Per block, indexed by block number and kept in step with the document: height
    // (estimated while waiting for layout), text length in average characters (for
    // the estimates) and whether it waits for layout
    std::vector<qreal> m_blockHeights;
    std::vector<qreal> m_blockExtents;
    std::vector<char> m_blockPending;
    int m_pendingCount = 0;

    // Width and font the laid out blocks follow; a change makes every block wait
    qreal m_layoutWidth = -1;
    QFont m_layoutFont;

    // Metrics of the current font for the estimates
    qreal m_averageCharWidth = 1;
    qreal m_estimatedLineHeight = 1;

    // Background pass: next block to look at, and the timer driving it
    int m_backgroundNext = 0;
    QTimer* m_backgroundTimer = nullptr;

    // Cached block Y positions (cumulative heights)
    mutable std::vector<qreal> m_blockYPositions;
    mutable bool m_positionsDirty = true;
    mutable qreal m_cachedDocumentHeight = 0;

    // Page flow, per block: where on its page (offset from the top of the page) the
    // block's lines were last placed - FLOW_UNPLACED when they follow its own top - and
    // the height it takes in the flow. Kept in step with the document.
    mutable std::vector<qreal> m_blockFlowOffsets;
    mutable std::vector<qreal> m_blockFlowHeights;

    // Page flow: bottom of the last line box (document coordinates)
    mutable qreal m_contentBottom = 0;

    // Last size announced through documentSizeChanged()
    QSizeF m_lastReportedSize;
};

}  // namespace kalahari::editor
