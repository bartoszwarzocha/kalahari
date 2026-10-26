/// @file kalahari_text_document_layout.h
/// @brief Custom QAbstractTextDocumentLayout without Qt leading gaps (OpenSpec #00043)

#pragma once

#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/paragraph_styles.h>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QFont>
#include <functional>
#include <map>
#include <utility>
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
/// - Line spacing, paragraph spacing, first-line indent and tab stops from
///   LayoutTypography; the extra line spacing is split evenly above and below each line
/// - Paragraph styles (ParagraphStyleSet): Normal is shown as the typography and the
///   document font say, every other style in the proportions of its package (see
///   BlockMetrics); the fonts of a style go over the text as QTextLayout formats, so the
///   document's own formats stay as they are
/// - Objects of the text (a horizontal line, the scene and page breaks) are sized and
///   drawn here; after a page break the page flow goes on at the next page, and a heading
///   is moved to the page of the paragraph after it
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
/// - The objects of the text (a horizontal line, the breaks): the layout is their handler.
///   Without a handler registered, Qt (6.5 and later) shapes U+FFFC as a plain character
///   and never asks the layout to size or draw an object.
class KalahariTextDocumentLayout : public QAbstractTextDocumentLayout,
                                   public QTextObjectInterface {
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)

public:
    explicit KalahariTextDocumentLayout(QTextDocument* doc);
    ~KalahariTextDocumentLayout() override;

    /// @brief Size of an object of the text (QTextObjectInterface): as wide as the text, one
    ///        line high, as resizeInlineObject() gives it
    QSizeF intrinsicSize(QTextDocument* doc, int posInDocument,
                         const QTextFormat& format) override;

    /// @brief Draw an object of the text (QTextObjectInterface), as drawInlineObject() does
    void drawObject(QPainter* painter, const QRectF& rect, QTextDocument* doc,
                    int posInDocument, const QTextFormat& format) override;

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
    /// Line breaks follow the font's design metrics in every view, with page flow or
    /// without it, so that they stay the same when a scaled painter zooms the page and
    /// when the view switches between the pages and the endless page.
    void setPageFlow(const PageFlow& flow);
    const PageFlow& pageFlow() const { return m_pageFlow; }

    /// @brief Paragraph spacing at the current document font (whole pixels)
    qreal paragraphSpacing() const;

    /// @brief First-line indent at the current document font
    qreal firstLineIndent() const;

    // ==========================================================================
    // Paragraph styles and objects
    // ==========================================================================

    /// @brief How the view shows a paragraph of a style
    ///
    /// Normal follows the typography (line and paragraph spacing, first-line indent) and the
    /// document font. Another style keeps the proportions of its package: its font is as
    /// many times larger than the document font as its package makes it larger than Normal,
    /// a length it sets itself is scaled by the same factor, and a length it takes from
    /// Normal follows the typography. Its line spacing is the typography's in the proportion
    /// of the two styles' line heights.
    struct BlockMetrics {
        qreal lineSpacing = 1.0;      ///< Multiplier of each line's natural height
        qreal spaceBefore = 0.0;      ///< Above the first line (whole pixels)
        qreal spaceAfter = 0.0;       ///< Below the last line (whole pixels)
        qreal firstLineIndent = 0.0;  ///< Of the first line (left-aligned and justified)
        qreal leftMargin = 0.0;       ///< Of every line (whole pixels)
        qreal rightMargin = 0.0;      ///< Of every line (whole pixels)
        Qt::Alignment alignment = DEFAULT_PARAGRAPH_ALIGNMENT;  ///< Without one of its own
        bool keepWithNext = false;    ///< Kept on the page of the paragraph after it
        bool pageBreak = false;       ///< A page break: the text after it starts on a new page
        QTextCharFormat charFormat;   ///< The style's font over the document font (empty
                                      ///< for Normal); a fragment's own format wins
        qreal emptyAscent = 0.0;      ///< Ascent and height of the line of an empty
        qreal emptyLineHeight = 0.0;  ///< paragraph in the style's font
        qreal estimatedLineHeight = 1.0;  ///< Line box height the estimates use
        qreal tabStopDistance = 0.0;  ///< Between two tab stops (0: Qt's default)
    };

    /// @brief Set the styles of the book; every paragraph is shown in its style again
    ///
    /// Call it outside an edit block of the document (the paragraphs are announced as
    /// changed, without a change of the text).
    void setParagraphStyles(const ParagraphStyleSet& styles);
    const ParagraphStyleSet& paragraphStyles() const { return m_styles; }

    /// @brief How a block is shown: its style, or the metrics of its object
    const BlockMetrics& blockMetrics(const QTextBlock& block) const;

    /// @brief Line spacing multiplier of a block (of its style)
    qreal lineSpacingOf(const QTextBlock& block) const;

    /// @brief Line spacing of a block as the layout of its document gives it; 1.0 for a
    ///        document with another layout
    static qreal blockLineSpacing(const QTextBlock& block);

    /// @brief The alignment a block is shown with: its own, else the one of its style
    Qt::Alignment shownAlignment(const QTextBlock& block) const;

    /// @brief How the scene breaks look (a view setting: the text keeps the breaks)
    void setSceneBreakLook(SceneBreakLook look);
    SceneBreakLook sceneBreakLook() const { return m_sceneBreakLook; }

    /// @brief Lay out every block now, also the ones already laid out
    void layoutAllBlocks();

    // ==========================================================================
    // The numbers on the margin
    // ==========================================================================

    /// @brief What the numbers on the margin count before a block: the lines of the blocks
    ///        before it (estimated for the ones waiting for layout) and the paragraphs with
    ///        text among them (isNumberedParagraph())
    /// @param blockNumber A block number; the block count: the whole document
    MarginCounts marginCountsBefore(int blockNumber) const;

    /// @brief Whether a block is numbered as a paragraph: a paragraph with text, not a
    ///        heading and not an object (a horizontal line, a scene or page break)
    static bool isNumberedParagraph(const QTextBlock& block);

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

    /// @brief Box of a laid out line of a block, as this layout stacks it: with the line
    ///        spacing of the block's style, and for an empty paragraph as high as a line of
    ///        its style's font
    QRectF lineBoxOf(const QTextBlock& block, const QTextLine& line) const;

    /// @brief lineBoxOf() for a block of any document; the line alone (no extra spacing)
    ///        for a document with another layout
    static QRectF blockLineBox(const QTextBlock& block, const QTextLine& line);

    /// @brief Index of the line whose box covers @p localY (block coordinates)
    ///
    /// Points above the first box or below the last one give the first or last line.
    /// @return -1 when the layout has no lines
    static int lineIndexAt(const QTextLayout& layout, qreal localY, qreal lineSpacing);

    /// @brief lineIndexAt() with the boxes of a block's lines (blockLineBox())
    static int lineIndexAt(const QTextBlock& block, const QTextLayout& layout, qreal localY);

    /// @brief lineIndexAt() with the boxes a function gives
    static int lineIndexAt(const QTextLayout& layout, qreal localY,
                           const std::function<QRectF(const QTextLine&)>& boxOf);

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

    /// @brief Size of an object of the text: as wide as the text, one line high
    void resizeInlineObject(QTextInlineObject item, int posInDocument,
                            const QTextFormat& format) override;

    /// @brief Draw an object of the text in the color of the text
    void drawInlineObject(QPainter* painter, const QRectF& rect, QTextInlineObject object,
                          int posInDocument, const QTextFormat& format) override;

private:
    /// @brief The first and the last block a change of the text touched (new coordinates)
    std::pair<QTextBlock, QTextBlock> changedBlocks(int from, int charsAdded) const;

    /// @brief Blocks [first, last] lose their lines (they wait for documentChanged())
    static void clearLines(const QTextBlock& first, const QTextBlock& last);

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
    bool layOutBlock(int number, const QTextBlock& block);

    /// @brief Break a single block into lines
    /// @param extent Receives the text length in average characters (for estimates)
    /// @param keepGlyphs Keep the shaped glyphs with the lines (blocks being edited)
    /// @param lead Receives the space above the block and the height of its first line box
    /// @return Height of the block, including its spacing above and below
    qreal layoutBlock(const QTextBlock& block, qreal* extent, bool keepGlyphs,
                      qreal* lead = nullptr) const;

    /// @brief Height a block of the given text extent probably gets at the current settings
    qreal estimatedHeight(qreal extent, const BlockMetrics& metrics) const;

    /// @brief Number of lines a block of the given text extent probably gets
    qreal estimatedLines(qreal extent, const BlockMetrics& metrics) const;

    /// @brief The numbers on the margin count the blocks from @p blockNumber on again (the
    ///        blocks before it are as they were)
    void recountMarginsFrom(int blockNumber);

    /// @brief Height of a line's box: the line, or for the line of an empty paragraph
    ///        (@p empty) the line of its style's font, with its share of the line spacing
    static qreal lineBoxHeight(const QTextLine& line, const BlockMetrics& metrics, bool empty);

    /// @brief Top of a line in its box (from the top of the box)
    static qreal lineTopInBox(const QTextLine& line, const BlockMetrics& metrics, bool empty);

    /// @brief Metrics of a style at the current font and typography (uncached)
    BlockMetrics styleMetrics(const ParagraphStyle& style) const;

    /// @brief Metrics of an object's block at the current font and typography (uncached)
    BlockMetrics objectMetrics(TextObject object) const;

    /// @brief Height of an object's line box
    qreal objectHeight(TextObject object) const;

    /// @brief Give a block's lines the fonts of its style, as formats of its layout
    void applyStyleFormats(const QTextBlock& block) const;

    /// @brief The page flow cache entry of a block (FlowEntry), with @p lead
    void storeFlowEntries(int number, const QTextBlock& block, qreal lead);

    /// @brief Forget the metrics of the styles and objects (font, typography or styles
    ///        changed)
    void clearMetrics();

    /// @brief Show every paragraph in its style anew (new styles or a new document font);
    ///        every block waits for layout
    void restyle();

    /// @brief Character width and line height the estimates use (current font)
    void updateEstimateMetrics();

    /// @brief Announce the document size if it is not the one announced last
    void reportDocumentSize();

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
    /// @return Height of the block in the flow, including its spacing above and below
    qreal paginateBlock(QTextLayout* layout, qreal top, const BlockMetrics& metrics) const;

    /// @brief Page flow, per block and kept in step with the document: what the flow needs
    ///        of the block's style, so a long text is placed without asking blockMetrics()
    struct FlowEntry {
        char flags = 0;                  ///< FLOW_KEEP_WITH_NEXT, FLOW_PAGE_BREAK
        qreal lead = 0;                  ///< The space above with the first line box (estimated while pending)
        qreal spaceBefore = 0;           ///< The style's space above
        qreal spaceAfter = 0;            ///< The style's space below
        qreal estimatedLineHeight = 1;   ///< A line box of the style, for the estimates
    };

    /// @brief The page flow entry of a block in a style, with @p lead
    static FlowEntry flowEntry(const BlockMetrics& metrics, qreal lead);

    /// @brief Height a block waiting for layout probably takes in the page flow
    /// @param height Its estimated height without page breaks
    qreal flowEstimate(qreal height, qreal top, const FlowEntry& entry) const;

    /// @brief Top of the text area after the one holding @p y
    qreal nextPageTop(qreal y) const;

    /// @brief Where the page flow puts a block kept with the next one: @p top, or the top of
    ///        the next text area when the block, the blocks kept with it and the first line
    ///        of the paragraph after them do not fit in the rest of its own
    qreal keptBlockTop(size_t index, qreal top) const;

    /// @brief Height the page flow gives the document: down to the end of the text area
    ///        of the page holding the last line
    qreal flowDocumentHeight() const;

    /// @brief Width reported for the document and its blocks
    qreal documentWidth() const;

    /// @brief Number of a laid out block covering @p y (lays out blocks as needed)
    int laidOutBlockAtY(qreal y) const;

    // View typography used for every block
    LayoutTypography m_typography;

    // The styles of the book, and the metrics of each style and object at the current font
    // and typography (filled in as blocks ask for them)
    // (maps: blockMetrics() hands out references, which a map keeps valid while it grows)
    ParagraphStyleSet m_styles;
    mutable std::map<QString, BlockMetrics> m_styleMetrics;
    mutable std::map<TextObject, BlockMetrics> m_objectMetrics;

    // True while restyle() announces the whole document as changed
    bool m_restyling = false;

    // How scene breaks look
    SceneBreakLook m_sceneBreakLook = SceneBreakLook::Asterisks;

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

    // What the numbers on the margin count before each block (marginCountsBefore()), one
    // more entry for the whole document; from block m_marginCountsFrom on counted again
    // after a change (recountMarginsFrom())
    mutable std::vector<MarginCounts> m_marginCounts;
    mutable int m_marginCountsFrom = 0;

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

    // Page flow, per block and kept in step with the document (FlowEntry)
    std::vector<FlowEntry> m_blockFlowEntries;

    // Page flow: bottom of the last line box (document coordinates)
    mutable qreal m_contentBottom = 0;

    // Last size announced through documentSizeChanged()
    QSizeF m_lastReportedSize;
};

}  // namespace kalahari::editor
