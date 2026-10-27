/// @file kalahari_text_document_layout.h
/// @brief Custom QAbstractTextDocumentLayout without Qt leading gaps (OpenSpec #00043)

#pragma once

#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/paragraph_format.h>
#include <kalahari/editor/paragraph_styles.h>
#include <kalahari/editor/text_lists.h>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QFont>
#include <functional>
#include <map>
#include <optional>
#include <utility>
#include <vector>

class QTextLayout;
class QTextLine;
class QTextTable;
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
/// - Paragraph styles (ParagraphStyleSet): Normal is shown as the typography and the
///   document font say, every other style in the proportions of its package (see
///   BlockMetrics); the fonts of a style go over the text as QTextLayout formats, so the
///   document's own formats stay as they are
/// - A paragraph's own formatting (paragraph_format.h) goes over its style: its indents,
///   spacing and line spacing in the proportions of the style's lengths
/// - Lists (text_lists.h): an item is indented as its level says, with its number or bullet
///   in the indent of its first line (drawListMarker()); the numbers are counted again after
///   every change, and an item whose number changed is laid out again
/// - Objects of the text (a horizontal line, the scene and page breaks) are sized and
///   drawn here; after a page break the page flow goes on at the next page, and a heading
///   is moved to the page of the paragraph after it
/// - Page flow keeps the breaks of a paragraph: no single first or last line alone on a page
///   (widow control, on unless the paragraph turns it off), all its lines on one page, a
///   new page before it
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
/// - Tables (text_tables.h): a table is as wide as the text, its columns as wide as its
///   format says; the paragraphs of a cell are laid out on the cell's width and their lines
///   placed in the cell. A row of a table is one unit of the vertical flow: every block of
///   the row has the row's top as its top, the last one has the row's height and the others
///   none, and in the page flow a row that does not fit in the rest of a page starts the
///   next one. A table is laid out as a whole whenever one of its blocks is.
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
        LineSpacingRule lineSpacingRule = LineSpacingRule::Multiple;  ///< At least or
        qreal lineHeight = 0.0;       ///< exactly so high a line box (whole pixels)
        qreal spaceBefore = 0.0;      ///< Above the first line (whole pixels)
        qreal spaceAfter = 0.0;       ///< Below the last line (whole pixels)
        qreal firstLineIndent = 0.0;  ///< Of the first line (left-aligned and justified)
        qreal leftMargin = 0.0;       ///< Of every line (whole pixels)
        qreal rightMargin = 0.0;      ///< Of every line (whole pixels)
        Qt::Alignment alignment = DEFAULT_PARAGRAPH_ALIGNMENT;  ///< Without one of its own
        bool keepWithNext = false;    ///< Kept on the page of the paragraph after it
        bool keepTogether = false;    ///< All its lines on one page
        bool pageBreakBefore = false; ///< Starts on a new page
        bool widowControl = true;     ///< No single first or last line alone on a page
        bool pageBreak = false;       ///< A page break: the text after it starts on a new page
        bool listItem = false;        ///< An item of a list: its first line starts after the
        qreal markerIndent = 0.0;     ///< number, which stands this far from the start of
                                      ///< the text (whole pixels)
        ListMarkerAlignment markerAlignment = ListMarkerAlignment::Left;
        QTextCharFormat charFormat;   ///< The style's font over the document font (empty
                                      ///< for Normal); a fragment's own format wins
        qreal emptyAscent = 0.0;      ///< Ascent and height of the line of an empty
        qreal emptyLineHeight = 0.0;  ///< paragraph in the style's font
        qreal estimatedLineHeight = 1.0;  ///< Line box height the estimates use
    };

    /// @brief Set the styles of the book; every paragraph is shown in its style again
    ///
    /// Call it outside an edit block of the document (the paragraphs are announced as
    /// changed, without a change of the text).
    void setParagraphStyles(const ParagraphStyleSet& styles);
    const ParagraphStyleSet& paragraphStyles() const { return m_styles; }

    /// @brief How a block is shown: its style with its own formatting and its list over it,
    ///        or the metrics of its object
    const BlockMetrics& blockMetrics(const QTextBlock& block) const;

    /// @brief The number or bullet of a list item, as drawListMarker() draws it
    struct ListMarker {
        QString text;     ///< The number with its suffix, or the bullet
        QFont font;       ///< Of the first character of the item, without its lines
        QColor color;     ///< Of the first character; invalid: the color of the text
        qreal x = 0.0;    ///< Left of the mark, from the start of the text (block coordinates)
        qreal width = 0.0;
    };

    /// @brief The number or bullet of a block: nothing outside a list and for an item without
    ///        a number
    std::optional<ListMarker> listMarker(const QTextBlock& block) const;

    /// @brief Draw the number or bullet of a laid out block on the baseline of its first line
    /// @param origin Top left of the block in the painter's coordinates
    /// @param textColor The color of the text, for a mark without a color of its own
    void drawListMarker(QPainter* painter, const QTextBlock& block, const QPointF& origin,
                        const QColor& textColor) const;

    /// @brief Line spacing multiplier of a block (of its style)
    qreal lineSpacingOf(const QTextBlock& block) const;

    /// @brief Line spacing of a block as the layout of its document gives it; 1.0 for a
    ///        document with another layout
    static qreal blockLineSpacing(const QTextBlock& block);

    /// @brief The alignment a block is shown with: its own, else the one of its style
    Qt::Alignment shownAlignment(const QTextBlock& block) const;

    /// @brief The formatting the style of a block gives it, in points, every part with a
    ///        value (Normal as the typography and the document font show it)
    ParagraphFormat styleParagraphFormat(const QTextBlock& block) const;

    /// @brief The formatting a block is shown with, in points: its own over its style's
    ///        (the indents of a list item are those of its level)
    ParagraphFormat shownParagraphFormat(const QTextBlock& block) const;

    /// @brief How the scene breaks look (a view setting: the text keeps the breaks)
    void setSceneBreakLook(SceneBreakLook look);
    SceneBreakLook sceneBreakLook() const { return m_sceneBreakLook; }

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

    /// @brief Number of the block covering @p y (clamped to the first/last block): in a
    ///        row of a table, the last block of the row (it has the row's height)
    /// @return -1 for an empty height cache
    int blockNumberAtY(qreal y) const;

    /// @brief Number of the block under a point (document coordinates): in a row of a
    ///        table, the paragraph of the cell under it (laid out first)
    /// @return -1 for an empty height cache
    int blockNumberAt(const QPointF& point) const;

    /// @brief Number of the first block of the row of a table holding the block; the block
    ///        itself outside a table
    int firstBlockOfRow(int blockNumber) const;

    /// @brief Number of the last block of the row of a table holding the block; the block
    ///        itself outside a table
    int lastBlockOfRow(int blockNumber) const;

    /// @brief The left edges of a table's columns and the right edge of the last one, at
    ///        the current width of the text (whole pixels)
    std::vector<qreal> tableColumnEdges(const QTextTable& table) const;

    /// @brief A row of a table as the layout places it (document coordinates)
    struct TableRow {
        qreal top = 0.0;
        qreal height = 0.0;
    };

    /// @brief The rows of a table, each at its place (in the page flow, a row may start
    ///        the next page); a row covered whole by cells spanning from above has no
    ///        height of its own: it goes with the row above
    std::vector<TableRow> tableRows(const QTextTable& table) const;

    /// @brief Space between the edges of a cell and its text (whole pixels)
    qreal cellPadding() const;

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

    /// @brief Where the lines of a block of a table go: in its cell, below the paragraphs
    ///        above it in the cell
    struct CellPlacement {
        qreal left = 0.0;   ///< Left of the cell's text (document coordinates)
        qreal top = 0.0;    ///< Top of the block below the top of its row
        qreal width = 0.0;  ///< Width of the cell's text
    };

    /// @brief Break a single block into lines
    /// @param extent Receives the text length in average characters (for estimates)
    /// @param keepGlyphs Keep the shaped glyphs with the lines (blocks being edited)
    /// @param lead Receives the space above the block and the height of its first line box
    /// @param cell The place of a block of a table in its cell: its lines are as wide as
    ///        the cell's text, without a first-line indent, and placed in the cell
    /// @return Height of the block, including its spacing above and below
    qreal layoutBlock(const QTextBlock& block, qreal* extent, bool keepGlyphs,
                      qreal* lead = nullptr, const CellPlacement* cell = nullptr) const;

    /// @brief Height a block of the given text extent probably gets at the current settings
    /// @param width The width its lines have; negative: the width of the text
    qreal estimatedHeight(qreal extent, const BlockMetrics& metrics, qreal width = -1.0) const;

    /// @brief Height of a line's box: the line, or for the line of an empty paragraph
    ///        (@p empty) the line of its style's font, with its share of the line spacing
    static qreal lineBoxHeight(const QTextLine& line, const BlockMetrics& metrics, bool empty);

    /// @brief Height of the box of a line @p height high, as the line spacing makes it
    static qreal boxHeightOf(qreal height, const BlockMetrics& metrics);

    /// @brief Top of a line in its box (from the top of the box)
    static qreal lineTopInBox(const QTextLine& line, const BlockMetrics& metrics, bool empty);

    /// @brief Metrics of a style at the current font and typography (uncached)
    BlockMetrics styleMetrics(const ParagraphStyle& style) const;

    /// @brief Metrics of an object's block at the current font and typography (uncached)
    BlockMetrics objectMetrics(TextObject object) const;

    /// @brief Pixels of a point of the style package at the current font (the package's
    ///        Normal shown in the document font)
    qreal packagePixels() const;

    /// @brief Metrics of a style with a paragraph's own formatting and its list item over
    ///        them (uncached)
    BlockMetrics directMetrics(const BlockMetrics& style, const QTextBlockFormat& format,
                               const std::optional<ListItem>& item) const;

    /// @brief The marks of the list items by block number, counted when the document changed
    const std::vector<QString>& listMarkers() const;

    /// @brief After a change of blocks [first, last]: the laid out list items outside them
    ///        whose numbers changed are laid out again
    /// @param delta Change in the number of blocks
    /// @param old The marks before the change, by the old block numbers
    /// @param oldKnown Whether @p old holds them (they had been counted)
    /// @param hadLists Whether the document had list items before the change
    void renumberLists(int first, int last, int delta, const std::vector<QString>& old,
                       bool oldKnown, bool hadLists);

    /// @brief Where the first line of a list item starts after its number: from the left
    ///        margin (0 when the number fits in front of the text indent)
    qreal markerOverflow(const QTextBlock& block, const BlockMetrics& metrics) const;

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
        char flags = 0;                  ///< FLOW_KEEP_WITH_NEXT, FLOW_PAGE_BREAK,
                                         ///< FLOW_PAGE_BREAK_BEFORE
        qreal lead = 0;                  ///< The space above with the first line box (estimated while pending)
        qreal spaceBefore = 0;           ///< The style's space above
        qreal spaceAfter = 0;            ///< The style's space below
        qreal estimatedLineHeight = 1;   ///< A line box of the style, for the estimates
    };

    /// @brief The page flow entry of a block in a style, with @p lead
    static FlowEntry flowEntry(const BlockMetrics& metrics, qreal lead);

    /// @brief What laying out a table gives each of its blocks
    struct TableBlockLayout {
        qreal height = 0.0;  ///< Its height in the vertical flow (0 but for a row's last)
        qreal extent = 1.0;  ///< Its text length in average characters
        FlowEntry flow;      ///< FLOW_TABLE, FLOW_ROW_START for a row's first block
    };

    /// @brief Lay out the blocks of a table in their cells, or estimate their heights
    /// @param estimate Estimate the heights from the text lengths, without laying out
    /// @param keepGlyphs Keep the shaped glyphs with the lines (a table being edited)
    /// @return For each block of the table, in order from its first one
    std::vector<TableBlockLayout> layoutTable(const QTextTable& table, bool estimate,
                                              bool keepGlyphs) const;

    /// @brief Lay out a table and store the cache entries of its blocks
    /// @param heightChanged Set when a block's height changed
    /// @return The numbers of its first and last block
    std::pair<int, int> layOutTable(const QTextTable& table, bool* heightChanged);

    /// @brief Height of the row of a table whose first block is @p first (the heights of
    ///        the row's blocks together)
    qreal rowHeight(size_t first) const;

    /// @brief Left of the text of the cell holding a block of a table
    qreal cellTextLeft(const QTextBlock& block) const;

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

    // The metrics of a style with a paragraph's own formatting or a list item over it, by
    // the style and the properties
    mutable std::map<QString, BlockMetrics> m_directMetrics;

    // The marks of the list items by block number (listMarkers()), whether the document has
    // list items, and whether the marks wait to be counted
    mutable std::vector<QString> m_listMarkers;
    mutable bool m_hasLists = false;
    mutable bool m_listMarkersDirty = true;

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
