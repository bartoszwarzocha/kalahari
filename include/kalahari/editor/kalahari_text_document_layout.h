/// @file kalahari_text_document_layout.h
/// @brief Custom QAbstractTextDocumentLayout without Qt leading gaps (OpenSpec #00043)

#pragma once

#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/paragraph_format.h>
#include <kalahari/editor/paragraph_styles.h>
#include <kalahari/editor/text_captions.h>
#include <kalahari/editor/text_footnotes.h>
#include <kalahari/editor/text_highlight.h>
#include <kalahari/editor/text_images.h>
#include <kalahari/editor/text_lists.h>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QColor>
#include <QFont>
#include <QHash>
#include <QMarginsF>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

class QTextLayout;
class QTextLine;
class QTextTable;
class QTimer;

namespace kalahari::editor {

class ImageStore;

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
/// - A paragraph's own formatting (paragraph_format.h) goes over its style: its indents,
///   spacing and line spacing in the proportions of the style's lengths
/// - Lists (text_lists.h): an item is indented as its level says, with its number or bullet
///   in the indent of its first line (drawListMarker()); the numbers are counted again after
///   every change, and an item whose number changed is laid out again
/// - Objects of the text (a horizontal line, the scene and page breaks) are sized and
///   drawn here; after a page break the page flow goes on at the next page, and a heading
///   is moved to the page of the paragraph after it
/// - Fields (text_captions.h): a caption's number and a cross-reference are characters
///   sized and drawn as the text they show; after a change of a field the numbers are
///   counted again, and a field or a list of figures that shows something else is laid
///   out again. A list of figures higher than a page's text starts a page and goes on to
///   the next ones, its rows never split.
/// - Notes (text_footnotes.h): a note's number is a character sized and drawn as its number,
///   raised; the numbers are counted with the fields. A note's text is laid out in the style
///   Footnote: in the continuous views under its paragraph (the room under the block, above
///   the cards of its annotations), the endnotes under the last paragraph; with page flow the
///   footnotes at the bottom of the page of their numbers (the line with a number goes to the
///   next page when the first line of its note does not fit with it; the rest of a long note
///   goes on at the bottom of the next page) and the endnotes after the last line.
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

    /// @brief Size of an object of the text (QTextObjectInterface), as resizeInlineObject()
    ///        gives it: a line or a break as wide as the text and one line high, an image
    ///        as imageSize() gives it
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

    // ==========================================================================
    // Images
    // ==========================================================================

    /// @brief Set the files of the images of the text (the book's); without them every
    ///        image is shown as a missing file. The blocks with images wait for layout again.
    void setImageStore(std::shared_ptr<ImageStore> store);
    const std::shared_ptr<ImageStore>& imageStore() const { return m_imageStore; }

    /// @brief The files of the images changed (the store got another folder): the blocks
    ///        with images wait for layout again
    void refreshImages();

    /// @brief The size of an image's box in a block (whole pixels): its picture's width (the
    ///        file's own as cropped and turned, a percentage of the width of the block's
    ///        lines, or centimeters shown as the style's points are), its height in
    ///        proportion or its own, and its frame around (imageFrameMargins()); never wider
    ///        than the lines nor higher than a page's text, the picture made smaller in its
    ///        shape; a missing file a frame of the width with room for its name
    QSizeF imageSize(const QTextBlock& block, const ImageProperties& image) const;

    /// @brief The image of a block whose box is made @p width wide (the layout's pixels, as
    ///        a handle dragged gives it), in its own unit: centimeters stay centimeters,
    ///        otherwise a whole percent of the width of the block's lines; an own height
    ///        changes in proportion
    ImageProperties imageOfWidth(const QTextBlock& block, const ImageProperties& image,
                                 qreal width) const;

    /// @brief The width of a block's lines an image's percent is of (the area less the
    ///        paragraph's indents)
    qreal imageLineWidth(const QTextBlock& block) const;

    /// @brief The room an image's frame takes around its picture (the layout's pixels): its
    ///        border and the room inside it on each side, its shadow to the right and down
    QMarginsF imageFrameMargins(const ImageProperties& image) const;

    /// @brief The widest an image of a block is shown (the layout's pixels): its lines'
    ///        width, less the first line's indent
    qreal widestImage(const QTextBlock& block) const;

    /// @brief Whether the image's file is missing (or is not an image the editor reads)
    bool isImageMissing(const ImageProperties& image) const;

    /// @brief The rectangle of the image at a position of a laid out block, in the block's
    ///        coordinates (as its lines are placed); std::nullopt: no image there, or its
    ///        block is not laid out
    std::optional<QRectF> imageRect(int position) const;

    /// @brief Set the color of the frame of an image whose file is missing (its dashed line
    ///        and its picture of a broken image); not valid: the color of the text
    void setMissingImageColor(const QColor& color);
    const QColor& missingImageColor() const { return m_missingImageColor; }

    // ==========================================================================
    // Captions, cross-references and the list of figures
    // ==========================================================================

    /// @brief Set what the numbers of the fields depend on outside the text: the book around
    ///        it (text_captions.h); the laid out fields and lists of figures that show
    ///        something else then are laid out again
    void setCaptionContext(const CaptionContext& context);
    const CaptionContext& captionContext() const { return m_captionContext; }

    /// @brief The numbers of the document's fields in the book around it, counted again when
    ///        they are asked for after a change of a field or of the context
    const CaptionNumbers& captionNumbers() const;

    /// @brief Whether the document has a field (a caption's number, a cross-reference) or a
    ///        list of figures
    bool hasFields() const;

    /// @brief The row of a list of figures under a point; std::nullopt: no row there, or the
    ///        list's block is not laid out
    /// @param position The list's character
    /// @param point In the coordinates of the list's block (as its lines are placed)
    std::optional<FigureListRow> figureListRowAt(int position, const QPointF& point) const;

    /// @brief The rectangle of the list of figures at a position of a laid out block, in the
    ///        block's coordinates; std::nullopt: no list there, or its block is not laid out
    std::optional<QRectF> figureListRect(int position) const;

    // ==========================================================================
    // Notes
    // ==========================================================================

    /// @brief Set what the numbers of the notes depend on outside the text (text_footnotes.h);
    ///        the laid out blocks whose notes show other numbers then are laid out again
    void setFootnoteContext(const FootnoteContext& context);
    const FootnoteContext& footnoteContext() const { return m_footnoteContext; }

    /// @brief The numbers of the document's notes, counted again when they are asked for
    ///        after a change of a note or of the context
    const FootnoteNumbers& footnoteNumbers() const;

    /// @brief Ranges of the texts of the notes drawn marked, by the notes' ids (positions of
    ///        noteText()): matches of the search, misspelled words, grammar issues
    using NoteMarks = QHash<QString, std::vector<TextHighlight>>;

    /// @brief The colors the text of the notes is drawn in, and what is marked in it
    struct NoteColors {
        QColor text;  ///< Of their text and numbers
        QColor line;  ///< Of the line above them
        QColor link;  ///< Of a link in them
        QColor searchMatch;   ///< Under a match of the search
        QColor currentMatch;  ///< Under the current match
        QColor selection;     ///< Under the text selected
        QColor spelling;      ///< Of the wave under a misspelled word
        QColor grammar;       ///< Of the wave under a grammar issue
        const NoteMarks* marks = nullptr;  ///< Of this text's notes; nullptr: none
        /// The text of a note shown selected (its number is selected in the text)
        std::optional<NoteRange> selected;
    };

    /// @brief Draw the notes under a block of the continuous views (its footnotes, under the
    ///        last block the endnotes too), in the block's coordinates (as its lines are placed)
    void drawNotesBelow(QPainter* painter, const QTextBlock& block, const NoteColors& colors) const;

    /// @brief The room the notes take under a block of the continuous views, the top of the
    ///        room under it (spaceBelow()); 0: none, or with page flow
    qreal notesBelow(int blockNumber) const;

    /// @brief Draw the notes of a page (page flow): its footnotes at the bottom of its text
    ///        area, the endnotes placed on it; the painter's origin at the top left of the
    ///        page's text area
    void drawPageNotes(QPainter* painter, int page, const NoteColors& colors) const;

    /// @brief The text of a note as shown: its id and its rectangle (document coordinates)
    struct NoteArea {
        QString id;
        QRectF rect;
        QString elementId;  ///< The text of an endnote of another text shown here; empty: this
                            ///< one's note
        size_t firstLine = 0;  ///< The line of the note's text at its top (the page view
                               ///< shows a part of a note on a page)
    };

    /// @brief The note whose text is shown under a point (document coordinates)
    std::optional<NoteArea> noteAreaAt(const QPointF& point) const;

    /// @brief The place in the text of a note of this text nearest to a point (document
    ///        coordinates): the note's id and the position in its noteText(), length 0;
    ///        std::nullopt: no text of one of its notes there
    std::optional<NoteRange> notePositionAt(const QPointF& point) const;

    /// @brief Where the start of a range of a note's text is shown: the rectangle of the range
    ///        in its first line (document coordinates); std::nullopt: not laid out
    std::optional<QRectF> noteRangeRect(const NoteRange& range) const;

    /// @brief Where the text of a note is shown, its first part (document coordinates);
    ///        std::nullopt: nowhere (a note waiting for the layout of its paragraph)
    std::optional<QRectF> noteRect(const QString& id) const;

    /// @brief The continuous views give a note at least @p height of room (a frame over it
    ///        that the text below makes room for); 0: its own height
    void setNoteRoom(const QString& id, qreal height);

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
        qreal tabStopDistance = 0.0;  ///< Between two tab stops (0: Qt's default)
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
    // The numbers on the margin
    // ==========================================================================

    /// @brief What the numbers on the margin count before a block: the lines of the blocks
    ///        before it (estimated for the ones waiting for layout; none of the cells of a
    ///        table) and the paragraphs with text among them (isNumberedParagraph())
    /// @param blockNumber A block number; the block count: the whole document
    MarginCounts marginCountsBefore(int blockNumber) const;

    /// @brief Whether a block is numbered as a paragraph: a paragraph with text, not a
    ///        heading, not an object (a horizontal line, a scene or page break) and not in a
    ///        cell of a table
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
    // Room under the blocks
    // ==========================================================================

    /// @brief Gives the room the view needs under a block's text (the cards of its
    ///        annotations), in document units; 0: none
    using SpaceBelowProvider = std::function<qreal(const QTextBlock& block)>;

    /// @brief Set what gives the room under the blocks (none: no block has any); every
    ///        block is asked again
    ///
    /// Without page flow only: the pages keep their lines as they are printed. Not in the
    /// cells of a table: they are side by side, as high as their row. The room is part of
    /// the block's height (blockHeight(), blockBoundingRect()), under its spacing; its lines
    /// stay where they are. Whole pixels.
    void setSpaceBelowProvider(SpaceBelowProvider provider);

    /// @brief Ask the blocks [first, last] for their room again: what it depends on changed
    ///        while their text stayed (an edited text asks by itself)
    /// @param last -1: to the end of the document
    void updateSpaceBelow(int first, int last = -1);

    /// @brief The room under a block's text (0 with page flow)
    qreal spaceBelow(int blockNumber) const;

    // ==========================================================================
    // Geometry
    // ==========================================================================

    /// @brief Top of a block in document coordinates (document height past the end)
    qreal blockY(int blockNumber) const;

    /// @brief Height of a block, including its paragraph spacing and the room under it
    ///        (estimated while the block waits for layout)
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
        /// Its parts on the pages, top and bottom of each: one, but for a row split between
        /// pages (a part on a page the table goes on to starts below the repeated header)
        std::vector<std::pair<qreal, qreal>> parts;
    };

    /// @brief The rows of a table, each at its place (in the page flow, a row may start
    ///        the next page); a row covered whole by cells spanning from above has no
    ///        height of its own: it goes with the row above
    std::vector<TableRow> tableRows(const QTextTable& table) const;

    /// @brief The tops of the copies of a table's header rows at the top of each page the
    ///        table goes on to (the page flow), in order
    std::vector<qreal> headerRepeats(const QTextTable& table) const;

    /// @brief A point in a copy of a header (headerRepeats()) moved to the same place of the
    ///        header itself: a click there puts the cursor in the header; another point as
    ///        it is
    QPointF fromRepeatedHeader(const QPointF& point) const;

    /// @brief The heights of a table's rows as laid out, without the pages: a row covered
    ///        whole by cells spanning from above has none (0), its height goes with the row
    ///        above
    std::vector<qreal> tableRowHeights(const QTextTable& table) const;

    /// @brief Pixels of a point of the text's lengths: the widths, heights and indents of
    ///        tables are shown at it, as the style's points are
    qreal pixelsPerPoint() const { return packagePixels(); }

    /// @brief Space between the edges of a cell and its text (whole pixels)
    qreal cellPadding() const;

    /// @brief The width of the lines of a table's borders, its points shown as the style's
    ///        points are (pixels)
    qreal tableBorderWidth(const QTextTable& table) const;

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
    /// @brief The first and the last block a change of the text touched (new coordinates),
    ///        in a table with the rows laid out with the changed ones
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

    /// @brief Number of lines a block of the given text extent probably gets
    /// @param width The width its lines have; negative: the width of the text
    qreal estimatedLines(qreal extent, const BlockMetrics& metrics, qreal width = -1.0) const;

    /// @brief The numbers on the margin count the blocks from @p blockNumber on again (the
    ///        blocks before it are as they were)
    void recountMarginsFrom(int blockNumber);

    /// @brief The room under a block: its notes' in the continuous views (askNotesBelow()),
    ///        and the provider's (0 without a provider, with page flow or in a cell of a table)
    /// @param notes Receives the part of the notes
    qreal askSpaceBelow(const QTextBlock& block, qreal* notes = nullptr) const;

    /// @brief Height of a line's box: the line, or for the line of an empty paragraph
    ///        (@p empty) the line of its style's font, with its share of the line spacing
    ///        @p excess: the part of the line above the line of the style's font that an
    ///        image takes, which gets no share of the line spacing
    static qreal lineBoxHeight(const QTextLine& line, const BlockMetrics& metrics, bool empty,
                               qreal excess = 0.0);

    /// @brief Height of the box of a line @p height high, as the line spacing makes it
    static qreal boxHeightOf(qreal height, const BlockMetrics& metrics);

    /// @brief Top of a line in its box (from the top of the box)
    static qreal lineTopInBox(const QTextLine& line, const BlockMetrics& metrics, bool empty,
                              qreal excess = 0.0);

    /// @brief How much higher an image makes a line of a block than a line of the style's
    ///        font (0: no image in the line, or not higher)
    qreal imageExcess(const QTextBlock& block, const QTextLine& line,
                      const BlockMetrics& metrics) const;

    /// @brief The width a block's lines lie in, with its margins: the text's, in a table
    ///        the cell's text
    qreal areaWidth(const QTextBlock& block) const;


    /// @brief Draw an image in its box: its frame, and inside it its picture or the frame of
    ///        a missing file
    void drawImage(QPainter* painter, const QRectF& box, const ImageProperties& image) const;

    /// @brief Draw an image's border and shadow around its picture
    void drawImageFrame(QPainter* painter, const QRectF& box, const ImageProperties& image) const;

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

    /// @brief The font of a field at a position: the document font, the style's and the
    ///        field's own over it
    QFont fieldFont(int position, const QTextFormat& format) const;

    /// @brief The rows of the list of figures as its object places them (FigureListPlacement):
    ///        placed again when the captions, the width, the font or the pages changed
    struct FigureListPlacement;
    const FigureListPlacement& figureListPlacement() const;

    /// @brief The tops of the rows of the list of figures, from the top of its line
    /// @param start The top of its line in the document, on the pages; < 0: without pages
    std::vector<qreal> figureListTops(qreal start) const;

    /// @brief The top of the line of the list of figures at a position, in the document, as
    ///        the pages placed it; -1 without pages (or not placed yet)
    qreal figureListStart(int position) const;

    /// @brief Draw the list of figures at a position in its object's rectangle (on the pages
    ///        its rows go on below it)
    void drawFigureList(QPainter* painter, const QRectF& rect, int position) const;

    /// @brief Count the fields and their numbers again, when a change or the context asks
    void countFields() const;

    /// @brief What each block with fields shows, by block number in order: the texts of its
    ///        fields, the captions of a list of figures
    std::vector<std::pair<int, QString>> fieldSignatures() const;

    /// @brief Whether a change of the text touched a field: one removed ([from, from +
    ///        charsRemoved) of the old text), one in the changed blocks, or a note's number in
    ///        them when it brought an object ([from, from + charsAdded) of the new text)
    ///
    /// Text typed next to a note's number changes no number: the notes only move.
    bool touchesFields(int from, int charsRemoved, int charsAdded, const QTextBlock& first,
                       const QTextBlock& last) const;

    /// @brief After a change of the text that touched no field: the fields after it move
    /// @param blockDelta Change in the number of blocks
    void shiftFields(int from, int charsRemoved, int charsAdded, int firstBlock, int oldLastBlock,
                     int blockDelta);

    /// @brief After a change of blocks [first, last] that touched a field, or of the context
    ///        (first > last): the laid out blocks outside it whose fields show something else
    ///        are laid out again
    /// @param delta Change in the number of blocks
    /// @param old What the blocks with fields showed before, by the old block numbers
    void refreshFields(int first, int last, int delta,
                       const std::vector<std::pair<int, QString>>& old);

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
    /// @param notes The room the footnotes of the line need with it on its page (the first
    ///        line of its last note), without the line above the notes
    /// @return @p top, or the top of the next text area when the box starts between two
    ///         areas, or would cross the end of its area (less the notes at its bottom) below
    ///         another line; a text area made smaller by notes is left when the box does not
    ///         fit in it
    qreal flowLineTop(qreal top, qreal height, qreal notes = 0.0) const;

    /// @brief The height of a page's text area the text takes: less its footnotes
    qreal flowAreaHeight(qreal page) const;

    /// @brief Place a laid out block's lines in the page flow
    /// @param top Top of the block (document coordinates)
    /// @return Height of the block in the flow, including its spacing above and below
    qreal paginateBlock(const QTextBlock& block, qreal top, const BlockMetrics& metrics) const;

    /// @brief Page flow, per block and kept in step with the document: what the flow needs
    ///        of the block's style, so a long text is placed without asking blockMetrics()
    struct FlowEntry {
        char flags = 0;                  ///< FLOW_KEEP_WITH_NEXT, FLOW_PAGE_BREAK,
                                         ///< FLOW_PAGE_BREAK_BEFORE
        qreal lead = 0;                  ///< The space above with the first line box (estimated while pending)
        qreal spaceBefore = 0;           ///< The style's space above
        qreal spaceAfter = 0;            ///< The style's space below
        qreal estimatedLineHeight = 1;   ///< A line box of the style, for the estimates; of
                                         ///< a row's first block: the least space its first
                                         ///< lines need
        qreal cellTop = 0;               ///< A block of a table: the top of its lines below
                                         ///< the top of its row, without page breaks
        qreal header = 0;                ///< A row's first block: the height of the table's
                                         ///< header rows repeated above it on a new page
    };

    /// @brief The page flow entry of a block in a style, with @p lead
    static FlowEntry flowEntry(const BlockMetrics& metrics, qreal lead);

    /// @brief What laying out a table gives each of its blocks
    struct TableBlockLayout {
        qreal height = 0.0;  ///< Its height in the vertical flow (0 but for a row's last)
        qreal extent = 1.0;  ///< Its text length in average characters
        FlowEntry flow;      ///< FLOW_TABLE, FLOW_ROW_START for a row's first block
    };

    /// @brief Lay out the blocks of rows of a table in their cells, or estimate their heights
    /// @param estimate Estimate the heights from the text lengths, without laying out
    /// @param keepGlyphs Keep the shaped glyphs with the lines (a table being edited)
    /// @param firstRow, lastRow The rows, laid out together (tableBand() in the source);
    ///        a negative @p lastRow: to the last row
    /// @param headerHeight Receives the height of the header repeated on each page
    /// @return For each block of the rows, in order from their first one
    std::vector<TableBlockLayout> layoutTable(const QTextTable& table, bool estimate,
                                              bool keepGlyphs, int firstRow = 0,
                                              int lastRow = -1,
                                              qreal* headerHeight = nullptr) const;

    /// @brief The height of the header of a table repeated on each page, as the rows below
    ///        its head have it (its cache entries laid out before)
    qreal tableHeader(const QTextTable& table) const;

    /// @brief The rows of a table from block @p first to @p last get the height of the
    ///        header repeated on each page
    void setTableHeader(int first, int last, qreal header);

    /// @brief Lay out rows of a table, with the rows that go with them, and store the
    ///        cache entries of their blocks
    /// @param heightChanged Set when a block's height changed
    /// @return The numbers of the first and last block laid out
    std::pair<int, int> layOutTableRows(const QTextTable& table, int firstRow, int lastRow,
                                        bool* heightChanged);

    /// @brief Height of the row of a table whose first block is @p first (the heights of
    ///        the row's blocks together)
    qreal rowHeight(size_t first) const;

    /// @brief Height of the row of a table whose first block is @p first as the page flow
    ///        places it: split between pages, higher than rowHeight()
    qreal rowFlowHeight(size_t first) const;

    /// @brief Place the lines of the row of a table from block @p first to @p last at
    ///        @p top of the page flow: a line that does not fit in the rest of its page
    ///        goes on to the next one, below the repeated header of @p header (each cell
    ///        splits at the same place); @p natural places them without page breaks
    /// @return The height the row takes in the flow
    qreal paginateRow(size_t first, size_t last, qreal top, qreal header, bool natural) const;

    /// @brief Where the page flow puts the row of a table from block @p i to @p last: its
    ///        top and its height, split between pages when needed; the copies of the header
    ///        of the table starting at block @p tableFirst it repeats go to m_headerRepeats
    std::pair<qreal, qreal> placeRow(size_t i, size_t last, qreal y, bool inStep,
                                     int tableFirst) const;

    /// @brief Top of the text area holding @p y
    qreal pageTopOf(qreal y) const;

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

    // The notes (kalahari_text_document_layout_notes.cpp)

    /// @brief A note's text laid out in the style Footnote, its number in front
    struct NoteBox;

    /// @brief The text of the note @p index of footnoteNumbers() laid out (cached by its id,
    ///        laid out again when its text, number or the width changed); an index past them
    ///        is of the book's other endnotes (FootnoteContext::otherEndnotes)
    const NoteBox& noteBox(size_t index) const;

    /// @brief The endnotes shown after the last paragraph (indices as noteBox() takes them):
    ///        the text's own unless they are at the end of the book, among the book's other
    ///        ones in reading order
    std::vector<size_t> shownEndnotes() const;

    /// @brief The id of the note @p index of noteBox() and its text (empty: this one's)
    std::pair<QString, QString> noteKey(size_t index) const;

    /// @brief The room a note's text takes under its block (the room its frame asks for, when
    ///        it is more)
    qreal noteRoom(size_t index) const;

    /// @brief The metrics of the style the notes are shown in (Footnote)
    const BlockMetrics& noteMetrics() const;

    /// @brief A note's number in its line: as wide as the number in a smaller font, as high
    ///        as the text around it (@p ascent and @p descent receive its parts)
    qreal noteNumberWidth(int position, const QTextFormat& format, qreal* ascent,
                          qreal* descent) const;

    /// @brief Draw a note's number raised in its object's rectangle (@p ascent: of its line's
    ///        text, < 0: of its font)
    void drawNoteNumber(QPainter* painter, const QRectF& rect, qreal ascent, int position,
                        const QTextFormat& format) const;

    /// @brief The room of the line above the notes, and of the gap under them
    qreal noteSeparatorRoom() const;
    qreal noteBottomGap() const;

    /// @brief The notes of footnoteNumbers() under a block in the continuous views (indices):
    ///        its footnotes, a table's under its last block
    std::vector<size_t> footnotesUnder(const QTextBlock& block) const;

    /// @brief The notes of footnoteNumbers() of a kind among the characters [from, to)
    std::vector<size_t> notesBetween(int from, int to, NoteKind kind) const;

    /// @brief The room the notes take under a block in the continuous views
    qreal askNotesBelow(const QTextBlock& block) const;

    /// @brief The index of a note of this text in footnoteNumbers() (std::nullopt: none)
    std::optional<size_t> noteIndexOf(const QString& id) const;

    /// @brief The top of line @p line of the note @p index of noteBox() where it is shown
    ///        (document coordinates); std::nullopt: not shown
    std::optional<qreal> noteLineTop(size_t index, size_t line) const;

    /// @brief Draw the marks of a note's text (NoteColors::marks, selected) in its lines from
    ///        @p first to @p last - 1, @p shift down: the backgrounds under the text, or the
    ///        waves over it
    void drawNoteMarks(QPainter* painter, size_t note, size_t first, size_t last, qreal shift,
                       const NoteColors& colors, bool waves) const;

    /// @brief Draw the lines [first, last) of a note's text, the top of the first at @p top
    void drawNoteLines(QPainter* painter, size_t note, size_t first, size_t last, qreal top,
                       const NoteColors& colors) const;

    /// @brief Draw the line above notes at @p top: short, or as wide as the text above a
    ///        note going on from the page before
    void drawNoteSeparator(QPainter* painter, qreal top, bool continued,
                           const NoteColors& colors) const;

    /// @brief The page flow places the footnotes of a line whose box ends at @p bottom: on
    ///        its page what fits, the rest at the bottom of the pages after it
    void reserveNotes(qreal bottom, const std::vector<size_t>& notes) const;

    /// @brief The room the footnotes of a line need on its page with it: those before its last
    ///        whole, the first line of its last
    qreal notesNeeded(const std::vector<size_t>& notes) const;

    /// @brief The page flow places the endnotes after the last line, from @p top on
    /// @return The bottom of their last line
    qreal placeEndnotes(qreal top) const;

    /// @brief The part of a note on a page (page flow)
    struct PageNote {
        size_t note = 0;     ///< Of footnoteNumbers()
        size_t first = 0;    ///< Its lines [first, last)
        size_t last = 0;
        bool continued = false;  ///< It goes on from the page before
    };

    /// @brief The footnotes at the bottom of a page: the room they take (with the line above
    ///        them) and their parts in order
    struct PageNotes {
        qreal reserved = 0.0;
        std::vector<PageNote> parts;
    };

    /// @brief A line of an endnote on the pages, its top in the document
    struct EndnoteLine {
        size_t note = 0;
        size_t line = 0;
        qreal top = 0.0;
    };

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

    // The files of the images; whether the document has shown an image (laid out again
    // when the page's height changes); the color of the frame of a missing one
    std::shared_ptr<ImageStore> m_imageStore;
    mutable bool m_hasImages = false;
    QColor m_missingImageColor;

    // The fields: what their numbers depend on outside the text, their numbers (counted
    // again when asked for after a change of a field), whether the document has a field or
    // a list of figures, each of their characters in order and what each block with fields
    // shows (by block number); the characters and blocks are kept in step with the text
    struct FieldChar {
        int position = 0;
        int block = 0;
        QTextCharFormat format;
    };
    CaptionContext m_captionContext;
    mutable CaptionNumbers m_captionNumbers;
    mutable bool m_captionNumbersDirty = true;
    mutable bool m_hasFields = false;
    mutable std::vector<FieldChar> m_fieldChars;
    std::vector<std::pair<int, QString>> m_fieldSignatures;

    // The list of figures as last placed (figureListPlacement())
    mutable std::unique_ptr<FigureListPlacement> m_figureList;

    // The notes: what their numbers depend on outside the text, their numbers (counted with
    // the fields), their texts laid out (by id), the room a frame over one of them takes
    FootnoteContext m_footnoteContext;
    mutable FootnoteNumbers m_footnoteNumbers;
    mutable std::unordered_map<QString, std::unique_ptr<NoteBox>> m_noteBoxes;
    std::map<QString, qreal> m_noteRooms;

    // Page flow: the footnotes at the bottom of each page (by page, to the last page with
    // any), the last page with any (-1: none), the endnotes' lines and the line above them
    // (its top; < 0: none); placed again with the positions
    mutable std::vector<PageNotes> m_pageNotes;
    mutable int m_notesLastPage = -1;
    mutable std::vector<EndnoteLine> m_endnoteLines;
    mutable qreal m_endnotesTop = -1.0;

    // Per block, indexed by block number and kept in step with the document: height
    // (estimated while waiting for layout), text length in average characters (for
    // the estimates) and whether it waits for layout
    std::vector<qreal> m_blockHeights;
    std::vector<qreal> m_blockExtents;
    std::vector<char> m_blockPending;
    int m_pendingCount = 0;

    // The room under each block's text (setSpaceBelowProvider()), kept in step with the
    // document; not part of m_blockHeights, so a new room places no line again
    SpaceBelowProvider m_spaceBelowProvider;
    std::vector<qreal> m_blockSpaceBelow;

    // The part of each block's room that its notes take (the continuous views), kept in step
    // with m_blockSpaceBelow
    std::vector<qreal> m_blockNotesBelow;

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

    // Page flow: the copies of the header rows of tables at the top of the pages they go on
    // to, in order (document coordinates)
    struct HeaderRepeat {
        qreal top = 0;
        qreal height = 0;
        int tableFirstBlock = 0;  ///< The number of the table's first block
    };
    mutable std::vector<HeaderRepeat> m_headerRepeats;

    // Last size announced through documentSizeChanged()
    QSizeF m_lastReportedSize;
};

}  // namespace kalahari::editor
