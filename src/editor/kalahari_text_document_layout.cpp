/// @file kalahari_text_document_layout.cpp
/// @brief Custom QAbstractTextDocumentLayout implementation (OpenSpec #00043)

#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/image_store.h>
#include <kalahari/editor/kml_format_registry.h>
#include <kalahari/editor/text_tables.h>
#include "layout_notes_internal.h"
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QSvgRenderer>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextLayout>
#include <QTextLine>
#include <QTextOption>
#include <QTextTable>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <limits>

namespace kalahari::editor {

namespace {

/// Line width used when the document has no text width (no wrapping)
constexpr qreal UNWRAPPED_LINE_WIDTH = 10000.0;

/// Extent Qt itself uses for an "everything" update rect
constexpr qreal UNBOUNDED_EXTENT = 1000000000.0;

/// Accepted range of the line spacing multiplier
constexpr qreal MIN_LINE_SPACING = 0.5;
constexpr qreal MAX_LINE_SPACING = 4.0;

/// Accepted range of the spaces between two tab stops
constexpr int MIN_TAB_SPACES = 1;
constexpr int MAX_TAB_SPACES = 32;

/// An edit touching at most this many blocks lays them out at once. A larger one (a
/// load, a long paste or its undo) leaves them waiting for layout.
constexpr int IMMEDIATE_LAYOUT_LIMIT = 32;

/// Time one step of the background pass may take, so input stays responsive
constexpr qint64 BACKGROUND_STEP_MS = 4;

/// Share of the line width that wrapped text fills on average (estimates)
constexpr qreal AVERAGE_LINE_FILL = 0.95;

/// Page flow offset of a block whose lines follow its own top (not placed on a page)
constexpr qreal FLOW_UNPLACED = -1.0;

/// Page flow: the lines of a block (or a row of a table) were placed on a page with notes,
/// which may take another part of it the next time: they are placed again
constexpr qreal FLOW_PLACE_AGAIN = -2.0;

/// Tolerance of page flow comparisons (its lengths are whole pixels)
constexpr qreal FLOW_EPSILON = 0.01;

/// Page flow flags of a block: kept on the page of the next block; a page break; starts on a
/// new page
constexpr char FLOW_KEEP_WITH_NEXT = 1;
constexpr char FLOW_PAGE_BREAK = 2;
constexpr char FLOW_PAGE_BREAK_BEFORE = 4;

/// Page flow flags of a block of a table, and of the first block of a row of a table (the
/// row is placed as a whole)
constexpr char FLOW_TABLE = 8;
constexpr char FLOW_ROW_START = 16;

/// Page flow flag of the first block of a row of a table that may be split between pages
constexpr char FLOW_ROW_SPLIT = 32;

/// Page flow flag of a block with notes: placed again whenever the blocks are (the notes
/// take the bottom of the pages)
constexpr char FLOW_NOTES = 64;

/// The narrowest a table gets: so many paddings of its cells for each column (pixels)
constexpr qreal MIN_COLUMN_PADDINGS = 3.0;

/// Space between the edges of a cell and its text, in lines of the document font
constexpr qreal CELL_PADDING_LINES = 0.25;

/// The least space between the edges of a cell and its text (pixels)
constexpr qreal MIN_CELL_PADDING = 2.0;

/// Rounds of moving lines of a paragraph to the next page to keep its first or last lines
/// together (a three-line paragraph takes two: its last two lines, then all three; a long
/// one at most three: its last two lines, all of it, its last two lines again)
constexpr int KEEP_LINES_ROUNDS = 4;

constexpr qreal POINTS_PER_INCH = 72.0;

/// Space above and below a scene break, in lines of Normal
constexpr qreal SCENE_BREAK_PADDING = 0.5;

/// Space above and below a page break, in lines of Normal
constexpr qreal PAGE_BREAK_PADDING = 0.25;

/// Size of the label of a page break, as a share of the document font
constexpr qreal PAGE_BREAK_LABEL_SCALE = 0.75;

/// Opacity of the lines and labels of the objects, over the color of the text
constexpr qreal OBJECT_LINE_OPACITY = 0.6;

/// Lines of the objects are this many times thinner than a line of Normal is high
constexpr qreal OBJECT_LINE_THINNESS = 20.0;

/// The points of a centimeter
constexpr qreal POINTS_PER_CENTIMETER = 72.0 / 2.54;

/// The width of the frame of a missing image of the file's own width, in centimeters
constexpr qreal MISSING_IMAGE_CENTIMETERS = 6.0;

/// The least height of the frame of a missing image, in lines of the document font, and
/// its height as a share of its width when that is more
constexpr qreal MISSING_IMAGE_LINES = 3.5;
constexpr qreal MISSING_IMAGE_RATIO = 0.25;

/// The size of the text of the frame of a missing image, as a share of the document font
constexpr qreal MISSING_IMAGE_LABEL_SCALE = 0.8;

/// The opacity of the background of the frame of a missing image, over the text's color
constexpr qreal MISSING_IMAGE_FILL_OPACITY = 0.04;

/// How far an image's shadow falls to the right and down (points)
constexpr qreal IMAGE_SHADOW_POINTS = 3.0;

/// How opaque an image's shadow is, in the color of its border
constexpr qreal IMAGE_SHADOW_OPACITY = 0.35;

/// A double border line: two lines and the gap between them, each of the border's width
constexpr qreal DOUBLE_BORDER_LINES = 3.0;

/// The picture of a broken image on the frame of a missing one (24 units square), in the
/// color of the text
constexpr const char* BROKEN_IMAGE_SVG =
    "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\"><path fill=\"%1\" "
    "d=\"M21 5v6.59l-3-3.01-4 4.01-4-4-4 4-3-3.01V5c0-1.1.9-2 2-2h14c1.1 0 2 .9 2 2zm-3 "
    "6.42l3 3.01V19c0 1.1-.9 2-2 2H5c-1.1 0-2-.9-2-2v-6.58l3 2.99 4-4 4 4 4-3.99z\"/></svg>";

/// The object types the layout sizes and draws
constexpr int LAYOUT_OBJECTS[] = {HORIZONTAL_RULE_OBJECT, SCENE_BREAK_OBJECT,  PAGE_BREAK_OBJECT,
                                  IMAGE_OBJECT,           FIGURE_LIST_OBJECT,  CAPTION_NUMBER_OBJECT,
                                  CROSS_REFERENCE_OBJECT, FOOTNOTE_OBJECT};

/// What the signatures of the fields put between two fields of a block, and after the text
/// of a cross-reference to a caption the book does not have
constexpr QChar FIELD_SEPARATOR(0x1f);
constexpr QChar BROKEN_FIELD_MARK(0x01);

/// A line of text in a font, unwrapped, with the font's design metrics (as the text's)
void layOutUnwrapped(QTextLayout& text) {
    QTextOption option;
    option.setWrapMode(QTextOption::NoWrap);
    option.setUseDesignMetrics(true);
    text.setTextOption(option);
    text.beginLayout();
    QTextLine line = text.createLine();
    if (line.isValid()) {
        line.setLineWidth(UNWRAPPED_LINE_WIDTH);
    }
    text.endLayout();
}

/// Whether a format is of a character whose number the layout counts: a field, a note's number
bool isNumberedFormat(const QTextFormat& format) {
    return isFieldFormat(format) || isFootnoteFormat(format);
}

/// Whether a block holds a field, a note's number (unless @p withNotes is false) or a list of
/// figures
bool hasFieldsOrList(const QTextBlock& block, bool withNotes = true) {
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment fragment = it.fragment();
        if (!fragment.isValid()) {
            continue;
        }
        const QTextCharFormat format = fragment.charFormat();
        if (isFieldFormat(format) || textObjectOf(format) == TextObject::FigureList ||
            (withNotes && isFootnoteFormat(format))) {
            return true;
        }
    }
    return false;
}

/// What the rows of a list of figures show and where they go, in one text
QString figureListSignature(const std::vector<FigureListRow>& rows) {
    QString signature;
    for (const FigureListRow& row : rows) {
        signature += row.id + FIELD_SEPARATOR + row.elementId + FIELD_SEPARATOR + row.text +
                     QLatin1Char('\n');
    }
    return signature;
}

/// A number of a style's properties, or @p fallback without it
qreal numberOf(const QVariantMap& properties, const char* name, qreal fallback) {
    const QVariant value = properties.value(QLatin1String(name));
    bool ok = false;
    const qreal number = value.toDouble(&ok);
    return value.isValid() && ok ? number : fallback;
}

/// A flag of a style's properties, or @p fallback without it
bool flagOf(const QVariantMap& properties, const char* name, bool fallback) {
    const QVariant value = properties.value(QLatin1String(name));
    return value.isValid() ? value.toBool() : fallback;
}

/// Prose whose width per character is the average character width of the estimates
const QString& estimateSample() {
    static const QString sample = QStringLiteral(
        "The quick brown fox jumps over the lazy dog. Zażółć gęślą jaźń, mówiła.");
    return sample;
}

/// Extra space a line gets from the line spacing; whole pixels, so that lines stay on
/// the pixel grid (Qt rounds line heights up to whole pixels too)
qreal extraLeading(qreal lineHeight, qreal lineSpacing) {
    return std::round(lineHeight * (lineSpacing - 1.0));
}

/// Part of the extra leading that goes above the line (the rest goes below it)
qreal leadingAbove(qreal extra) {
    return std::floor(extra / 2.0);
}

/// The row of the cell of a table holding a block
int rowOfBlock(const QTextTable& table, const QTextBlock& block) {
    return table.cellAt(block.position()).row();
}

/// Rows [firstRow, lastRow] of a table with the rows their cells span: no cell spans into
/// them from the other rows
std::pair<int, int> spannedRows(const QTextTable& table, int firstRow, int lastRow) {
    const int rows = table.rows();
    for (bool grown = true; grown;) {
        grown = false;
        for (int column = 0; column < table.columns(); ++column) {
            const QTextTableCell top = table.cellAt(firstRow, column);
            if (top.isValid() && top.row() < firstRow) {
                firstRow = top.row();
                grown = true;
            }
            const QTextTableCell bottom = table.cellAt(lastRow, column);
            const int end = bottom.isValid() ? std::min(bottom.row() + bottom.rowSpan(), rows) - 1 : lastRow;
            if (end > lastRow) {
                lastRow = end;
                grown = true;
            }
        }
    }
    return {firstRow, lastRow};
}

/// The last row of the head of a table: its header rows with the first row below them,
/// and the rows their cells span; -1 for a table without header rows
int headEndOf(const QTextTable& table) {
    const int rows = table.rows();
    const int headerRows = std::clamp(table.format().headerRowCount(), 0, rows);
    return headerRows > 0 ? spannedRows(table, 0, std::min(headerRows, rows - 1)).second : -1;
}

/// The rows of a table laid out together with rows [firstRow, lastRow]. A row is as high
/// as its highest cell, so it goes with the rows its cells span. The heights of the head
/// (headEndOf()) give the space the header rows keep below them and the height of the
/// header repeated on each page: the head goes with every row touching it, and with the
/// row below it, whose page flow entry gives the others that height (tableHeader()).
std::pair<int, int> tableBand(const QTextTable& table, int firstRow, int lastRow) {
    const int rows = table.rows();
    if (rows <= 0) {
        return {0, -1};
    }
    firstRow = std::clamp(firstRow, 0, rows - 1);
    lastRow = std::clamp(lastRow, firstRow, rows - 1);
    auto band = spannedRows(table, firstRow, lastRow);
    if (const int headEnd = headEndOf(table); headEnd >= 0 && band.first <= headEnd + 1) {
        band = {0, std::max(band.second, headEnd)};
    }
    return band;
}

/// The first and the last block of rows [firstRow, lastRow] of a table, no cell spanning
/// into them from the other rows
std::pair<QTextBlock, QTextBlock> bandBlocks(const QTextTable& table, int firstRow, int lastRow) {
    const QTextDocument* doc = table.document();
    const QTextBlock first = doc->findBlock(table.cellAt(firstRow, 0).firstPosition());
    const QTextBlock last = lastRow + 1 < table.rows()
                                ? doc->findBlock(table.cellAt(lastRow + 1, 0).firstPosition()).previous()
                                : doc->findBlock(table.lastPosition());
    return {first, last};
}

}  // anonymous namespace

/// The rows of a list of figures as its object places them
struct KalahariTextDocumentLayout::FigureListPlacement {
    // What they were placed for: the rows, the width, the font, the line and the spacing
    // between the rows, the pages
    QString signature;
    qreal width = -1.0;
    QFont font;
    qreal lineHeight = 0.0;
    qreal spacing = 0.0;

    // Each row laid out on the width, at its top in the object without pages; without
    // captions, one row that says so (noCaptions); the height of all of them without pages
    struct Row {
        FigureListRow row;
        std::unique_ptr<QTextLayout> text;
        qreal top = 0.0;
        qreal height = 0.0;
    };
    std::vector<Row> rows;
    bool noCaptions = false;
    qreal height = 0.0;
};

// =============================================================================
// Constructor
// =============================================================================

KalahariTextDocumentLayout::KalahariTextDocumentLayout(QTextDocument* doc)
    : QAbstractTextDocumentLayout(doc)
    , m_layoutWidth(doc ? doc->textWidth() : -1)
    , m_layoutFont(doc ? doc->defaultFont() : QFont())
    , m_backgroundTimer(new QTimer(this)) {
    if (doc) {
        updateEstimateMetrics();

        // Qt announces a change of the text (contentsChange) before it has the changed
        // blocks laid out again (documentChanged()), and an edited block keeps its old lines
        // until then. Whoever reads the lines in between (the find bar giving the keys to
        // the text, which places the cursor) measured the new text by the old lines: past
        // its end after a shorter replacement, where Qt's debug build stops (QList::at). The
        // changed blocks lose their lines first, before the document's other listeners, as
        // the layout is made before they connect.
        connect(doc, &QTextDocument::contentsChange, this,
                [this](int from, int, int charsAdded) {
                    const auto [first, last] = changedBlocks(from, charsAdded);
                    clearLines(first, last);
                });
    }
    // Qt takes U+FFFC for an object only while the layout has a handler
    for (const int objectType : LAYOUT_OBJECTS) {
        registerHandler(objectType, this);
    }
    // Runs whenever the event loop has nothing else to do, while blocks wait for layout
    m_backgroundTimer->setInterval(0);
    connect(m_backgroundTimer, &QTimer::timeout, this, &KalahariTextDocumentLayout::layoutStep);
}

KalahariTextDocumentLayout::~KalahariTextDocumentLayout() {
    // A handler's destroyed() goes to the layout; while the layout itself is destroyed, a
    // debug Qt stops there, as the layout's part of the object is gone already
    for (const int objectType : LAYOUT_OBJECTS) {
        unregisterHandler(objectType, this);
    }
}

// =============================================================================
// Configuration
// =============================================================================

void KalahariTextDocumentLayout::setTextWidth(qreal width) {
    // QTextDocument::setTextWidth() re-lays out the whole document through
    // documentChanged() even when the width does not change.
    if (document()->textWidth() != width) {
        document()->setTextWidth(width);
    }
}

qreal KalahariTextDocumentLayout::textWidth() const {
    return document()->textWidth();
}

void KalahariTextDocumentLayout::setFont(const QFont& font) {
    // Same as setTextWidth(): setDefaultFont() always re-lays out the whole document.
    if (document()->defaultFont() != font) {
        document()->setDefaultFont(font);
    }
}

QFont KalahariTextDocumentLayout::font() const {
    return document()->defaultFont();
}

void KalahariTextDocumentLayout::setPageFlow(const PageFlow& flow) {
    // Whole pixels keep the lines on the pixel grid; a page's text area is at least a pixel
    // high and fits within the pitch
    PageFlow normalized;
    normalized.textHeight = std::max<qreal>(0.0, std::round(flow.textHeight));
    if (flow.enabled) {
        normalized.enabled = true;
        normalized.textHeight = std::max<qreal>(1.0, normalized.textHeight);
        normalized.pitch = std::max(normalized.textHeight, std::round(flow.pitch));
    }
    if (normalized == m_pageFlow) {
        return;
    }
    const bool placementChanged = normalized.enabled || m_pageFlow.enabled;
    // An image is never higher than a page's text: the images get their sizes again
    const bool imagesChanged = m_hasImages && normalized.textHeight != m_pageFlow.textHeight;
    m_pageFlow = normalized;
    if (imagesChanged || placementChanged) {
        // New sizes of the images; or the line breaks stay (design metrics in every view),
        // but the lines of each block are placed anew: on the pages, or one under another
        invalidateAll();
    } else {
        // Only the height the endless text area takes at least: the lines stay
        reportDocumentSize();
    }
}

void KalahariTextDocumentLayout::setImageStore(std::shared_ptr<ImageStore> store) {
    if (store == m_imageStore) {
        return;
    }
    m_imageStore = std::move(store);
    if (m_hasImages) {
        invalidateAll();  // the sizes of their files
    }
}

void KalahariTextDocumentLayout::refreshImages() {
    if (m_hasImages) {
        invalidateAll();
    }
}

void KalahariTextDocumentLayout::setMissingImageColor(const QColor& color) {
    if (color == m_missingImageColor) {
        return;
    }
    m_missingImageColor = color;
    if (m_hasImages) {
        emit update(QRectF(0, 0, UNBOUNDED_EXTENT, UNBOUNDED_EXTENT));
    }
}

void KalahariTextDocumentLayout::setTypography(const LayoutTypography& typography) {
    LayoutTypography normalized = typography;
    normalized.lineSpacing = std::clamp(typography.lineSpacing, MIN_LINE_SPACING, MAX_LINE_SPACING);
    normalized.paragraphSpacing = std::max<qreal>(0, typography.paragraphSpacing);
    normalized.firstLineIndent = std::max<qreal>(0, typography.firstLineIndent);
    normalized.referencePointSize = std::max<qreal>(0, typography.referencePointSize);
    normalized.tabIndent = std::max<qreal>(0, typography.tabIndent);
    normalized.tabSpaces = std::clamp(typography.tabSpaces, MIN_TAB_SPACES, MAX_TAB_SPACES);
    if (normalized == m_typography) {
        return;
    }
    m_typography = normalized;
    clearMetrics();
    invalidateAll();
}

qreal KalahariTextDocumentLayout::typographyScale() const {
    if (m_typography.referencePointSize <= 0) {
        return 1.0;
    }
    const qreal pointSize = document()->defaultFont().pointSizeF();
    return pointSize > 0 ? pointSize / m_typography.referencePointSize : 1.0;
}

qreal KalahariTextDocumentLayout::paragraphSpacing() const {
    // Whole pixels keep the blocks below on the pixel grid
    return std::round(m_typography.paragraphSpacing * typographyScale());
}

qreal KalahariTextDocumentLayout::firstLineIndent() const {
    return m_typography.firstLineIndent * typographyScale();
}

void KalahariTextDocumentLayout::layoutAllBlocks() {
    invalidateAll();
    layoutPendingBlocks();
}

// =============================================================================
// The numbers on the margin
// =============================================================================

MarginCounts KalahariTextDocumentLayout::marginCountsBefore(int blockNumber) const {
    // Counted again only when asked after a change (the numbers are shown): typing, a block
    // laid out. The blocks before the first changed one keep their counts
    const QTextDocument* doc = document();
    const int blockCount = doc->blockCount();
    if (m_marginCountsFrom < blockCount ||
        m_marginCounts.size() != static_cast<size_t>(blockCount) + 1) {
        const int from = std::clamp(m_marginCountsFrom, 0,
                                    std::max(static_cast<int>(m_marginCounts.size()) - 1, 0));
        m_marginCounts.resize(static_cast<size_t>(from) + 1);
        m_marginCounts.reserve(static_cast<size_t>(blockCount) + 1);
        auto index = static_cast<size_t>(from);
        for (QTextBlock block = doc->findBlockByNumber(from); block.isValid();
             block = block.next(), ++index) {
            // Every block shows a line at least (an edited one has none for a moment); the
            // lines of the cells of a table are not counted, as in Word: side by side, their
            // numbers would be drawn over each other
            const bool laidOut = index < m_blockPending.size() && m_blockPending[index] == 0;
            const qreal extent =
                index < m_blockExtents.size() ? m_blockExtents[index] : block.length();
            int lines = 0;
            if (tableOf(block) == nullptr) {
                lines = laidOut && block.layout() != nullptr
                            ? std::max(1, block.layout()->lineCount())
                            : static_cast<int>(estimatedLines(extent, blockMetrics(block)));
            }
            MarginCounts next = m_marginCounts.back();
            next.lines += lines;
            next.paragraphs += isNumberedParagraph(block) ? 1 : 0;
            m_marginCounts.push_back(next);
        }
        m_marginCountsFrom = std::numeric_limits<int>::max();
    }
    const auto last = static_cast<int>(m_marginCounts.size()) - 1;
    return m_marginCounts[static_cast<size_t>(std::clamp(blockNumber, 0, last))];
}

bool KalahariTextDocumentLayout::isNumberedParagraph(const QTextBlock& block) {
    return blockObject(block) == TextObject::None &&
           headingLevelOf(paragraphStyleOf(block.blockFormat())) == 0 &&
           !block.text().trimmed().isEmpty() && tableOf(block) == nullptr;
}

// =============================================================================
// Paragraph styles and objects
// =============================================================================

void KalahariTextDocumentLayout::setParagraphStyles(const ParagraphStyleSet& styles) {
    if (styles == m_styles) {
        return;
    }
    m_styles = styles;
    clearMetrics();
    restyle();
}

void KalahariTextDocumentLayout::restyle() {
    // A change of the whole document without a change of its text: documentChanged() gives
    // each block the formats of its style, and every block waits for layout. Formats set
    // outside such a change would be announced with the next edit, as a change of every
    // block between the two.
    m_restyling = true;
    document()->markContentsDirty(0, document()->characterCount());
    m_restyling = false;
}

void KalahariTextDocumentLayout::clearMetrics() {
    m_styleMetrics.clear();
    m_objectMetrics.clear();
    m_directMetrics.clear();
    m_noteBoxes.clear();  // laid out in the style Footnote
}

const KalahariTextDocumentLayout::BlockMetrics& KalahariTextDocumentLayout::blockMetrics(
    const QTextBlock& block) const {
    if (const TextObject object = blockObject(block); object != TextObject::None) {
        auto it = m_objectMetrics.find(object);
        if (it == m_objectMetrics.end()) {
            it = m_objectMetrics.emplace(object, objectMetrics(object)).first;
        }
        return it->second;
    }
    const QTextBlockFormat format = block.blockFormat();
    const ParagraphStyle& style = m_styles.shownStyle(paragraphStyleOf(format));
    auto it = m_styleMetrics.find(style.id);
    if (it == m_styleMetrics.end()) {
        it = m_styleMetrics.emplace(style.id, styleMetrics(style)).first;
    }

    // The paragraph's own formatting and its list over the style
    const QString own = paragraphFormatKey(format);
    const bool listItem = isListItem(format);
    if (own.isEmpty() && !listItem) {
        return it->second;
    }
    QString key = style.id + QLatin1Char('|') + own;
    if (listItem) {
        key += QStringLiteral("|%1|").arg(format.intProperty(LIST_LEVEL_PROPERTY)) +
               format.stringProperty(LIST_DEFINITION_PROPERTY);
    }
    auto direct = m_directMetrics.find(key);
    if (direct == m_directMetrics.end()) {
        direct = m_directMetrics
                     .emplace(key, directMetrics(it->second, format,
                                                 listItem ? listItemOf(format) : std::nullopt))
                     .first;
    }
    return direct->second;
}

qreal KalahariTextDocumentLayout::packagePixels() const {
    const QFont font = document()->defaultFont();
    const qreal fontPoints = font.pointSizeF() > 0 ? font.pointSizeF() : QFontInfo(font).pointSizeF();
    const qreal normalPoints =
        numberOf(m_styles.normal().properties, "fontSize", DEFAULT_NORMAL_POINT_SIZE);
    const qreal ratio = normalPoints > 0 ? fontPoints / normalPoints : 1.0;
    return ratio * QFontMetricsF(font).fontDpi() / POINTS_PER_INCH;
}

KalahariTextDocumentLayout::BlockMetrics KalahariTextDocumentLayout::directMetrics(
    const BlockMetrics& style, const QTextBlockFormat& format,
    const std::optional<ListItem>& item) const {
    // Points of the paragraph's own formatting are shown as the style's points are
    BlockMetrics metrics = style;
    const qreal pixels = packagePixels();
    const auto length = [pixels](qreal points) { return std::round(points * pixels); };
    const ParagraphFormat own = paragraphFormatOf(format);
    if (own.leftIndent) {
        metrics.leftMargin = std::max<qreal>(0.0, length(*own.leftIndent));
    }
    if (own.rightIndent) {
        metrics.rightMargin = std::max<qreal>(0.0, length(*own.rightIndent));
    }
    if (own.firstLineIndent) {
        metrics.firstLineIndent = *own.firstLineIndent * pixels;
    }
    if (own.spaceBefore) {
        metrics.spaceBefore = std::max<qreal>(0.0, length(*own.spaceBefore));
    }
    if (own.spaceAfter) {
        metrics.spaceAfter = std::max<qreal>(0.0, length(*own.spaceAfter));
    }
    if (own.lineSpacing) {
        // A multiple as the style's line height is: in the proportion of Normal's, over the
        // typography's line spacing; exactly so many points; at least so many, else as the
        // style spaces its lines
        metrics.lineSpacingRule = own.lineSpacing->rule;
        if (own.lineSpacing->rule == LineSpacingRule::Multiple) {
            const qreal normalLineHeight = numberOf(m_styles.normal().properties, "lineHeight", 1.0);
            metrics.lineSpacing = std::clamp(
                m_typography.lineSpacing * own.lineSpacing->value /
                    (normalLineHeight > 0 ? normalLineHeight : 1.0),
                MIN_LINE_SPACING, MAX_LINE_SPACING);
            metrics.lineHeight = 0.0;
        } else {
            metrics.lineHeight = std::max<qreal>(1.0, length(own.lineSpacing->value));
        }
    }
    metrics.keepWithNext = own.keepWithNext.value_or(metrics.keepWithNext);
    metrics.keepTogether = own.keepTogether.value_or(metrics.keepTogether);
    metrics.pageBreakBefore = own.pageBreakBefore.value_or(metrics.pageBreakBefore);
    metrics.widowControl = own.widowControl.value_or(metrics.widowControl);

    // A list item: its lines at the text of its level, its first line after the number
    if (item) {
        const ListLevel& level = item->levelFormat();
        metrics.listItem = true;
        metrics.leftMargin = length(level.textIndent);
        metrics.firstLineIndent = 0.0;
        metrics.markerIndent = length(level.markerIndent);
        metrics.markerAlignment = level.alignment;
    }
    metrics.estimatedLineHeight = boxHeightOf(metrics.emptyLineHeight, metrics);
    return metrics;
}

KalahariTextDocumentLayout::BlockMetrics KalahariTextDocumentLayout::styleMetrics(
    const ParagraphStyle& style) const {
    const QVariantMap& own = style.properties;
    const QVariantMap& normal = m_styles.normal().properties;
    const auto fromNormal = [&own, &normal](const char* name) {
        return own.value(QLatin1String(name)) == normal.value(QLatin1String(name));
    };

    // The package's points are as many times larger in the view as the document font is
    // larger than the package's Normal
    const QFont font = document()->defaultFont();
    const qreal fontPoints = font.pointSizeF() > 0 ? font.pointSizeF() : QFontInfo(font).pointSizeF();
    const qreal normalPoints = numberOf(normal, "fontSize", DEFAULT_NORMAL_POINT_SIZE);
    const qreal ratio = normalPoints > 0 ? fontPoints / normalPoints : 1.0;
    const qreal pixels = packagePixels();
    const auto length = [&own, pixels](const char* name) {
        return std::round(numberOf(own, name, 0.0) * pixels);
    };

    BlockMetrics metrics;
    const qreal normalLineHeight = numberOf(normal, "lineHeight", 1.0);
    metrics.lineSpacing =
        fromNormal("lineHeight") || normalLineHeight <= 0
            ? m_typography.lineSpacing
            : std::clamp(m_typography.lineSpacing * numberOf(own, "lineHeight", 1.0) / normalLineHeight,
                         MIN_LINE_SPACING, MAX_LINE_SPACING);
    metrics.spaceBefore = length("spaceBefore");
    metrics.spaceAfter = fromNormal("spaceAfter") ? paragraphSpacing() : length("spaceAfter");
    const qreal normalIndent = numberOf(normal, "firstLineIndent", 0.0);
    if (fromNormal("firstLineIndent")) {
        metrics.firstLineIndent = firstLineIndent();
    } else if (normalIndent != 0.0) {
        metrics.firstLineIndent = numberOf(own, "firstLineIndent", 0.0) * firstLineIndent() / normalIndent;
    } else {
        metrics.firstLineIndent = numberOf(own, "firstLineIndent", 0.0) * pixels;
    }
    metrics.leftMargin = length("leftMargin");
    metrics.rightMargin = length("rightMargin");
    if (const Qt::Alignment alignment = styleAlignmentOf(own.value(QStringLiteral("alignment")).toString());
        alignment) {
        metrics.alignment = alignment;
    }
    metrics.keepWithNext = flagOf(own, "keepWithNext", false);
    metrics.keepTogether = flagOf(own, "keepTogether", false);
    metrics.pageBreakBefore = flagOf(own, "pageBreakBefore", false);
    metrics.widowControl = flagOf(own, "widowControl", true);

    // The font of the style where it differs from Normal's (Normal is the document font)
    QTextCharFormat& format = metrics.charFormat;
    if (!fromNormal("fontSize") && numberOf(own, "fontSize", 0.0) > 0) {
        format.setFontPointSize(numberOf(own, "fontSize", 0.0) * ratio);
    }
    if (const bool bold = flagOf(own, "bold", false); bold != flagOf(normal, "bold", false)) {
        format.setFontWeight(bold ? QFont::Bold : QFont::Normal);
    }
    if (const bool italic = flagOf(own, "italic", false); italic != flagOf(normal, "italic", false)) {
        format.setFontItalic(italic);
    }
    if (const bool underline = flagOf(own, "underline", false);
        underline != flagOf(normal, "underline", false)) {
        format.setFontUnderline(underline);
    }
    if (const QString family = own.value(QStringLiteral("fontFamily")).toString();
        !family.isEmpty() && !fromNormal("fontFamily")) {
        format.setFontFamilies(QStringList{family});
    }

    // The line of an empty paragraph has the height of the style's font (Qt gives it the
    // height of the document font: the formats go over the text, and it has none)
    QTextCharFormat styled;
    styled.setFont(font);
    styled.merge(format);
    const QFontMetricsF styledMetrics(styled.font());
    metrics.emptyAscent = styledMetrics.ascent();
    metrics.emptyLineHeight = std::ceil(styledMetrics.ascent() + styledMetrics.descent());
    metrics.estimatedLineHeight =
        metrics.emptyLineHeight + extraLeading(metrics.emptyLineHeight, metrics.lineSpacing);

    // The tab stops: every first-line indent of the style (a style without one, as headings
    // have, takes the indent of the settings), or every few spaces of its font
    if (m_typography.tabStops == TabStops::Spaces) {
        metrics.tabStopDistance =
            m_typography.tabSpaces * styledMetrics.horizontalAdvance(QLatin1Char(' '));
    } else {
        metrics.tabStopDistance = metrics.firstLineIndent > 0
                                      ? metrics.firstLineIndent
                                      : m_typography.tabIndent * typographyScale();
    }
    return metrics;
}

KalahariTextDocumentLayout::BlockMetrics KalahariTextDocumentLayout::objectMetrics(
    TextObject object) const {
    // One line of Normal high, with no line spacing of its own: the object's height is the
    // box of a line of Normal; a break has some space around it
    BlockMetrics metrics;
    metrics.alignment = Qt::AlignLeft;
    metrics.spaceAfter = paragraphSpacing();
    const qreal line = objectHeight(object);
    const qreal padding = object == TextObject::SceneBreak  ? SCENE_BREAK_PADDING
                          : object == TextObject::PageBreak ? PAGE_BREAK_PADDING
                                                            : 0.0;
    metrics.spaceBefore = std::round(line * padding);
    metrics.spaceAfter += std::round(line * padding);
    metrics.estimatedLineHeight = line;
    metrics.pageBreak = object == TextObject::PageBreak;
    return metrics;
}

qreal KalahariTextDocumentLayout::objectHeight(TextObject /*object*/) const {
    return m_estimatedLineHeight;
}

qreal KalahariTextDocumentLayout::lineSpacingOf(const QTextBlock& block) const {
    return blockMetrics(block).lineSpacing;
}

qreal KalahariTextDocumentLayout::blockLineSpacing(const QTextBlock& block) {
    if (!block.isValid()) {
        return 1.0;
    }
    const auto* layout =
        qobject_cast<const KalahariTextDocumentLayout*>(block.document()->documentLayout());
    return layout != nullptr ? layout->lineSpacingOf(block) : 1.0;
}

Qt::Alignment KalahariTextDocumentLayout::shownAlignment(const QTextBlock& block) const {
    const Qt::Alignment own = ownAlignment(block.blockFormat());
    return own ? own : blockMetrics(block).alignment;
}

ParagraphFormat KalahariTextDocumentLayout::styleParagraphFormat(const QTextBlock& block) const {
    const ParagraphStyle& style = m_styles.shownStyle(paragraphStyleOf(block.blockFormat()));
    auto it = m_styleMetrics.find(style.id);
    if (it == m_styleMetrics.end()) {
        it = m_styleMetrics.emplace(style.id, styleMetrics(style)).first;
    }
    const BlockMetrics& metrics = it->second;

    // The pixels of the view back in points, to two decimals as KML writes them
    const qreal pixels = packagePixels();
    const auto points = [pixels](qreal length) {
        return pixels > 0 ? std::round(length / pixels * 100.0) / 100.0 : 0.0;
    };
    ParagraphFormat format;
    format.leftIndent = points(metrics.leftMargin);
    format.rightIndent = points(metrics.rightMargin);
    format.firstLineIndent = points(metrics.firstLineIndent);
    format.spaceBefore = points(metrics.spaceBefore);
    format.spaceAfter = points(metrics.spaceAfter);
    // A multiple as a paragraph's own one is read (directMetrics())
    const qreal normalLineHeight = numberOf(m_styles.normal().properties, "lineHeight", 1.0);
    const qreal multiple = m_typography.lineSpacing > 0
                               ? metrics.lineSpacing * (normalLineHeight > 0 ? normalLineHeight : 1.0) /
                                     m_typography.lineSpacing
                               : 1.0;
    format.lineSpacing = LineSpacing{LineSpacingRule::Multiple, std::round(multiple * 100.0) / 100.0};
    format.keepWithNext = metrics.keepWithNext;
    format.keepTogether = metrics.keepTogether;
    format.pageBreakBefore = metrics.pageBreakBefore;
    format.widowControl = metrics.widowControl;
    return format;
}

ParagraphFormat KalahariTextDocumentLayout::shownParagraphFormat(const QTextBlock& block) const {
    ParagraphFormat format = styleParagraphFormat(block);
    const QTextBlockFormat blockFormat = block.blockFormat();
    const ParagraphFormat own = paragraphFormatOf(blockFormat);
    const auto over = [](auto& shown, const auto& value) {
        if (value) {
            shown = value;
        }
    };
    over(format.leftIndent, own.leftIndent);
    over(format.rightIndent, own.rightIndent);
    over(format.firstLineIndent, own.firstLineIndent);
    over(format.spaceBefore, own.spaceBefore);
    over(format.spaceAfter, own.spaceAfter);
    over(format.lineSpacing, own.lineSpacing);
    over(format.keepWithNext, own.keepWithNext);
    over(format.keepTogether, own.keepTogether);
    over(format.pageBreakBefore, own.pageBreakBefore);
    over(format.widowControl, own.widowControl);
    // A list item's lines start at the text of its level, its first line at the number
    if (const std::optional<ListItem> item = listItemOf(blockFormat)) {
        const ListLevel& level = item->levelFormat();
        format.leftIndent = level.textIndent;
        format.firstLineIndent = level.markerIndent - level.textIndent;
    }
    return format;
}

void KalahariTextDocumentLayout::setSceneBreakLook(SceneBreakLook look) {
    if (look == m_sceneBreakLook) {
        return;
    }
    m_sceneBreakLook = look;
    emit update(QRectF(0, 0, UNBOUNDED_EXTENT, UNBOUNDED_EXTENT));
}

void KalahariTextDocumentLayout::applyStyleFormats(const QTextBlock& block) const {
    QTextLayout* layout = block.layout();
    if (layout == nullptr) {
        return;
    }

    // The style's font goes over each fragment, but not over what the fragment sets itself
    // (its own size, weight, family...): the document's formats stay as they are. The letter
    // spacing of the text, in points as in KML, goes over it in pixels, as its size does.
    QList<QTextLayout::FormatRange> ranges;
    QTextCharFormat styleFormat = blockMetrics(block).charFormat;
    if (const QTextTable* table = tableOf(block);
        table && table->cellAt(block.position()).row() < table->format().headerRowCount()) {
        styleFormat.setFontWeight(QFont::Bold);  // the header of a table
    }
    const QMap<int, QVariant> styleProperties = styleFormat.properties();
    const qreal pixelsPerPoint = QFontMetricsF(document()->defaultFont()).fontDpi() / POINTS_PER_INCH;
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment fragment = it.fragment();
        if (!fragment.isValid()) {
            continue;
        }
        const QTextCharFormat own = fragment.charFormat();
        const qreal spacing = own.doubleProperty(KmlPropLetterSpacing);
        if (styleProperties.isEmpty() && spacing == 0.0) {
            continue;
        }
        QTextCharFormat format = styleFormat;
        for (auto property = styleProperties.cbegin(); property != styleProperties.cend();
             ++property) {
            if (own.hasProperty(property.key())) {
                format.clearProperty(property.key());
            }
        }
        if (own.hasProperty(QTextFormat::FontFamilies) || own.hasProperty(QTextFormat::FontFamily)) {
            format.clearProperty(QTextFormat::FontFamilies);
            format.clearProperty(QTextFormat::FontFamily);
        }
        if (spacing != 0.0) {
            format.setFontLetterSpacingType(QFont::AbsoluteSpacing);
            format.setFontLetterSpacing(spacing * pixelsPerPoint);
        }
        if (format.propertyCount() > 0) {
            QTextLayout::FormatRange range;
            range.start = fragment.position() - block.position();
            range.length = fragment.length();
            range.format = format;
            ranges.append(range);
        }
    }
    // Setting formats announces a change of the block: only when they change
    if (ranges != layout->formats()) {
        layout->setFormats(ranges);
    }
}

KalahariTextDocumentLayout::FlowEntry KalahariTextDocumentLayout::flowEntry(
    const BlockMetrics& metrics, qreal lead) {
    FlowEntry entry;
    if (metrics.keepWithNext) {
        entry.flags |= FLOW_KEEP_WITH_NEXT;
    }
    if (metrics.pageBreak) {
        entry.flags |= FLOW_PAGE_BREAK;
    }
    if (metrics.pageBreakBefore) {
        entry.flags |= FLOW_PAGE_BREAK_BEFORE;
    }
    entry.lead = lead;
    entry.spaceBefore = metrics.spaceBefore;
    entry.spaceAfter = metrics.spaceAfter;
    entry.estimatedLineHeight = std::max<qreal>(1.0, metrics.estimatedLineHeight);
    return entry;
}

void KalahariTextDocumentLayout::storeFlowEntries(int number, const QTextBlock& block, qreal lead) {
    FlowEntry& entry = m_blockFlowEntries[static_cast<size_t>(number)];
    entry = flowEntry(blockMetrics(block), lead);
    if (hasNotes(block)) {
        entry.flags |= FLOW_NOTES;
    }
}

// =============================================================================
// Layout on demand
// =============================================================================

bool KalahariTextDocumentLayout::isLaidOut(int blockNumber) const {
    return blockNumber >= 0 && static_cast<size_t>(blockNumber) < m_blockPending.size() &&
           m_blockPending[static_cast<size_t>(blockNumber)] == 0;
}

bool KalahariTextDocumentLayout::ensureLaidOut(int first, int last) {
    const int pendingBefore = m_pendingCount;
    if (pendingBefore > 0) {
        layOutPending(first, last, -1);
    }
    return m_pendingCount != pendingBefore;
}

void KalahariTextDocumentLayout::layoutPendingBlocks() {
    layOutPending(0, static_cast<int>(m_blockHeights.size()) - 1, -1);
}

void KalahariTextDocumentLayout::ensureLaidOutTo(qreal y) {
    // Laying blocks out changes their heights, and so which block is at y
    while (ensureLaidOut(0, blockNumberAtY(y))) {
    }
}

QTextLayout* KalahariTextDocumentLayout::blockLayout(const QTextBlock& block) {
    if (!block.isValid()) {
        return nullptr;
    }
    if (auto* layout =
            qobject_cast<KalahariTextDocumentLayout*>(block.document()->documentLayout())) {
        layout->ensureLaidOut(block.blockNumber(), block.blockNumber());
    }
    return block.layout();
}

int KalahariTextDocumentLayout::layOutPending(int first, int last, qint64 budgetMs) {
    first = std::max(first, 0);
    last = std::min(last, static_cast<int>(m_blockHeights.size()) - 1);
    if (m_pendingCount == 0 || first > last) {
        return last;
    }

    QElapsedTimer timer;
    timer.start();

    // Runs of consecutive blocks laid out, announced once the caches are consistent
    std::vector<std::pair<int, int>> runs;
    bool heightChanged = false;
    int number = first;
    QTextBlock block = document()->findBlockByNumber(first);
    for (; number <= last && block.isValid(); ++number, block = block.next()) {
        if (m_blockPending[static_cast<size_t>(number)] == 0) {
            continue;
        }
        if (const QTextTable* table = tableOf(block)) {
            // The row, with the rows laid out with it (tableBand()), and on after them
            const int row = rowOfBlock(*table, block);
            const auto [bandFirst, bandLast] = layOutTableRows(*table, row, row, &heightChanged);
            if (!runs.empty() && runs.back().first + runs.back().second == bandFirst) {
                runs.back().second += bandLast - bandFirst + 1;
            } else {
                runs.emplace_back(bandFirst, bandLast - bandFirst + 1);
            }
            number = std::max(number, bandLast);
            block = document()->findBlockByNumber(number);
        } else {
            heightChanged |= layOutBlock(number, block);
            if (!runs.empty() && runs.back().first + runs.back().second == number) {
                ++runs.back().second;
            } else {
                runs.emplace_back(number, 1);
            }
        }
        if (m_pendingCount == 0 || (budgetMs >= 0 && timer.elapsed() >= budgetMs)) {
            ++number;
            break;
        }
    }
    if (runs.empty()) {
        return number - 1;
    }

    // With page flow, the new lines are placed on the pages with the positions; whether
    // that moves the blocks below is known only then
    heightChanged |= m_pageFlow.enabled;
    if (heightChanged) {
        m_positionsDirty = true;
    }
    if (m_pendingCount == 0) {
        m_backgroundTimer->stop();
    }
    for (const auto& [runFirst, runCount] : runs) {
        emit blocksLaidOut(runFirst, runCount);
    }
    const int firstLaidOut = runs.front().first;
    const int lastLaidOut = runs.back().first + runs.back().second - 1;
    // Also for unchanged heights: the lines are new (a block's estimate can be exact while
    // its text wraps differently from before)
    emit blockGeometryChanged(firstLaidOut);
    notifyGeometryChanged(firstLaidOut,
                          heightChanged ? UNBOUNDED_EXTENT
                                        : blockY(lastLaidOut) + blockHeight(lastLaidOut));
    return number - 1;
}

void KalahariTextDocumentLayout::layoutStep() {
    const auto count = static_cast<int>(m_blockPending.size());
    if (m_pendingCount == 0 || count == 0) {
        m_backgroundTimer->stop();
        return;
    }

    // The next waiting block from where the last step stopped, wrapping around
    if (m_backgroundNext >= count) {
        m_backgroundNext = 0;
    }
    auto it = std::find(m_blockPending.begin() + m_backgroundNext, m_blockPending.end(), 1);
    if (it == m_blockPending.end()) {
        it = std::find(m_blockPending.begin(), m_blockPending.end(), 1);
    }
    if (it == m_blockPending.end()) {
        m_pendingCount = 0;  // nothing waits after all
        m_backgroundTimer->stop();
        return;
    }
    const auto first = static_cast<int>(std::distance(m_blockPending.begin(), it));
    m_backgroundNext = layOutPending(first, count - 1, BACKGROUND_STEP_MS) + 1;
}

void KalahariTextDocumentLayout::invalidateAll() {
    const QTextDocument* doc = document();
    updateEstimateMetrics();
    recountMarginsFrom(0);

    const auto count = static_cast<size_t>(doc->blockCount());
    if (m_blockHeights.size() != count || m_blockExtents.size() != count) {
        // Out of step with the document (first layout, or a change the cache missed):
        // the text length is the only extent known
        m_blockExtents.clear();
        m_blockExtents.reserve(count);
        for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
            m_blockExtents.push_back(block.length());
        }
        m_blockExtents.resize(count, 1);
        m_blockHeights.resize(count);
    }
    m_blockFlowEntries.assign(count, FlowEntry());
    m_blockSpaceBelow.assign(count, 0.0);
    m_blockNotesBelow.assign(count, 0.0);
    m_noteBoxes.clear();

    QTextBlock block = doc->begin();
    size_t i = 0;
    while (i < count && block.isValid()) {
        if (const QTextTable* table = tableOf(block)) {
            // A table's rows from the estimated heights of its cells
            for (const TableBlockLayout& entry : layoutTable(*table, true, false)) {
                if (i < count) {
                    m_blockHeights[i] = entry.height;
                    m_blockFlowEntries[i] = entry.flow;
                }
                ++i;
            }
            // The notes of its cells under its last block
            const QTextBlock tableLast = doc->findBlock(table->lastPosition());
            if (tableLast.isValid() && static_cast<size_t>(tableLast.blockNumber()) < count) {
                const auto last = static_cast<size_t>(tableLast.blockNumber());
                m_blockSpaceBelow[last] = askSpaceBelow(tableLast, &m_blockNotesBelow[last]);
            }
            block = doc->findBlockByNumber(static_cast<int>(i));
            continue;
        }
        const BlockMetrics& metrics = blockMetrics(block);
        m_blockHeights[i] = estimatedHeight(m_blockExtents[i], metrics);
        m_blockSpaceBelow[i] = askSpaceBelow(block, &m_blockNotesBelow[i]);
        storeFlowEntries(static_cast<int>(i), block,
                         metrics.spaceBefore + metrics.estimatedLineHeight);
        ++i;
        block = block.next();
    }
    m_blockPending.assign(count, 1);
    m_blockFlowOffsets.assign(count, FLOW_UNPLACED);
    m_blockFlowHeights.assign(count, 0.0);
    m_pendingCount = static_cast<int>(count);
    m_positionsDirty = true;
    m_backgroundNext = 0;
    if (m_pendingCount > 0) {
        m_backgroundTimer->start();
    }
    emit blockGeometryChanged(0);
    notifyGeometryChanged(0, UNBOUNDED_EXTENT);
}

void KalahariTextDocumentLayout::updateEstimateMetrics() {
    const QFontMetricsF metrics(document()->defaultFont());
    const QString& sample = estimateSample();
    m_averageCharWidth =
        std::max<qreal>(1.0, metrics.horizontalAdvance(sample) / static_cast<qreal>(sample.size()));
    // As Qt sizes a line: ascent + descent, rounded up to whole pixels
    const qreal lineHeight = std::ceil(metrics.ascent() + metrics.descent());
    m_estimatedLineHeight = lineHeight + extraLeading(lineHeight, m_typography.lineSpacing);
}

qreal KalahariTextDocumentLayout::estimatedHeight(qreal extent, const BlockMetrics& metrics,
                                                  qreal ownWidth) const {
    return estimatedLines(extent, metrics, ownWidth) * metrics.estimatedLineHeight +
           metrics.spaceBefore + metrics.spaceAfter;
}

qreal KalahariTextDocumentLayout::estimatedLines(qreal extent, const BlockMetrics& metrics,
                                                 qreal ownWidth) const {
    const qreal width = ownWidth >= 0 ? ownWidth : document()->textWidth();
    const qreal lineWidth =
        std::max<qreal>(1.0, (width > 0 ? width : UNWRAPPED_LINE_WIDTH) - metrics.leftMargin -
                                 metrics.rightMargin) *
        AVERAGE_LINE_FILL;
    return std::max<qreal>(1.0, std::ceil(extent * m_averageCharWidth / lineWidth));
}

void KalahariTextDocumentLayout::recountMarginsFrom(int blockNumber) {
    m_marginCountsFrom = std::min(m_marginCountsFrom, std::max(blockNumber, 0));
}

qreal KalahariTextDocumentLayout::askSpaceBelow(const QTextBlock& block, qreal* notes) const {
    // The notes first, the cards of the annotations under them
    const qreal notesRoom = askNotesBelow(block);
    if (notes != nullptr) {
        *notes = notesRoom;
    }
    // The cells of a row side by side are as high as the row: no space under one of them
    if (!m_spaceBelowProvider || m_pageFlow.enabled || tableOf(block) != nullptr) {
        return notesRoom;
    }
    return notesRoom + std::ceil(std::max<qreal>(0.0, m_spaceBelowProvider(block)));
}

void KalahariTextDocumentLayout::setSpaceBelowProvider(SpaceBelowProvider provider) {
    if (!provider && !m_spaceBelowProvider) {
        return;
    }
    m_spaceBelowProvider = std::move(provider);
    updateSpaceBelow(0);
}

void KalahariTextDocumentLayout::updateSpaceBelow(int first, int last) {
    // Asked in the middle of a change, the cache is out of step with the blocks: the change
    // asks every block it touched by itself
    const auto count = static_cast<int>(m_blockSpaceBelow.size());
    if (document()->blockCount() != count || m_blockNotesBelow.size() != m_blockSpaceBelow.size()) {
        return;
    }
    first = std::max(first, 0);
    last = last < 0 ? count - 1 : std::min(last, count - 1);
    bool changed = false;
    QTextBlock block = document()->findBlockByNumber(first);
    for (int number = first; number <= last && block.isValid(); ++number, block = block.next()) {
        qreal notes = 0.0;
        const qreal space = askSpaceBelow(block, &notes);
        qreal& stored = m_blockSpaceBelow[static_cast<size_t>(number)];
        m_blockNotesBelow[static_cast<size_t>(number)] = notes;
        if (space != stored) {
            stored = space;
            changed = true;
        }
    }
    if (!changed) {
        return;
    }
    // The blocks below move; their lines stay
    m_positionsDirty = true;
    emit blockGeometryChanged(first);
    notifyGeometryChanged(first, UNBOUNDED_EXTENT);
}

qreal KalahariTextDocumentLayout::spaceBelow(int blockNumber) const {
    const auto index = static_cast<size_t>(blockNumber);
    return blockNumber >= 0 && index < m_blockSpaceBelow.size() ? m_blockSpaceBelow[index] : 0.0;
}

void KalahariTextDocumentLayout::reportDocumentSize() {
    const QSizeF size = documentSize();
    if (size != m_lastReportedSize) {
        m_lastReportedSize = size;
        emit documentSizeChanged(size);
    }
}

void KalahariTextDocumentLayout::notifyGeometryChanged(int first, qreal bottom) {
    reportDocumentSize();

    const qreal top = blockY(first);
    emit update(QRectF(0, top, UNBOUNDED_EXTENT, bottom - top));
}

int KalahariTextDocumentLayout::laidOutBlockAtY(qreal y) const {
    // Laying out a block changes its height, and so which block covers y. The layout is
    // a cache of the document: filling it in leaves the query const for its callers.
    auto* self = const_cast<KalahariTextDocumentLayout*>(this);
    int number = blockNumberAtY(y);
    while (number >= 0 && self->ensureLaidOut(number, number)) {
        number = blockNumberAtY(y);
    }
    return number;
}

// =============================================================================
// Document Changed - Core Layout Logic
// =============================================================================

void KalahariTextDocumentLayout::documentChanged(int from, int charsRemoved, int charsAdded) {
    const QTextDocument* doc = document();

    // A new wrap width or default font (QTextDocument reports either as a change of the
    // whole document): every block waits for layout. The view lays out what it shows
    // before the next paint, the background pass the rest.
    if (doc->textWidth() != m_layoutWidth || doc->defaultFont() != m_layoutFont) {
        const bool fontChanged = doc->defaultFont() != m_layoutFont;
        m_layoutWidth = doc->textWidth();
        m_layoutFont = doc->defaultFont();
        clearMetrics();
        if (fontChanged) {
            // The fonts of the styles are sizes relative to the document font
            restyle();
        } else {
            invalidateAll();
        }
        return;
    }

    // Every paragraph is shown in its style anew (restyle()): the text stays, so the blocks
    // keep their extents for the estimates
    if (m_restyling) {
        for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
            applyStyleFormats(block);
        }
        invalidateAll();
        return;
    }

    // Every block touching the change is laid out again. Blocks outside it are untouched.
    const auto [first, last] = changedBlocks(from, charsAdded);
    recountMarginsFrom(first.blockNumber());  // the text, and so the lines, changed

    // The changed blocks lose their lines, as QTextDocumentLayout clears them (when the
    // document announced the change, they have lost them already): Qt keeps the lines of an
    // edited block, and a QTextCursor placed in one still waiting for layout measured the
    // new text by them (past its end after a shorter replacement)
    clearLines(first, last);

    // ... and get the fonts of their styles (a split block's new half has none, an edited
    // fragment may have its own now)
    const QTextBlock afterLast = last.next();
    for (QTextBlock block = first; block.isValid() && block != afterLast; block = block.next()) {
        applyStyleFormats(block);
    }

    // Untouched blocks keep their cache entries, so the old entries of the changed range
    // end where the new range ends, shifted by the change in block count.
    const int lastNumber = last.blockNumber();
    const int delta = doc->blockCount() - static_cast<int>(m_blockHeights.size());
    const int oldLast = lastNumber - delta;

    // The numbers of the list items are counted again (the changed items are laid out with
    // the new ones), and the items whose numbers changed are laid out again
    const bool hadLists = m_hasLists;
    const bool numbersKnown = !m_listMarkersDirty;
    std::vector<QString> oldMarkers;
    if (numbersKnown) {
        oldMarkers = std::move(m_listMarkers);
    }
    m_listMarkersDirty = true;

    // A change touching a field (its paragraph, a field removed or brought) may change what
    // the fields outside it show: they are counted again (the changed ones are laid out with
    // the new numbers); any other change moves them
    const bool fieldsTouched = touchesFields(from, charsRemoved, charsAdded, first, last);
    std::vector<std::pair<int, QString>> oldSignatures;
    if (fieldsTouched) {
        oldSignatures = std::move(m_fieldSignatures);
        m_captionNumbersDirty = true;
    } else {
        shiftFields(from, charsRemoved, charsAdded, first.blockNumber(), oldLast, delta);
    }
    replaceRange(first.blockNumber(), lastNumber, oldLast);
    renumberLists(first.blockNumber(), lastNumber, delta, oldMarkers, numbersKnown, hadLists);
    if (fieldsTouched) {
        refreshFields(first.blockNumber(), lastNumber, delta, oldSignatures);
        // The notes of a table are under its last block, which the change may have left
        for (const QTextBlock& end : {first, last}) {
            if (const QTextTable* table = tableOf(end)) {
                const int tableLast = doc->findBlock(table->lastPosition()).blockNumber();
                updateSpaceBelow(tableLast, tableLast);
            }
        }
    }
}

std::pair<QTextBlock, QTextBlock> KalahariTextDocumentLayout::changedBlocks(int from,
                                                                          int charsAdded) const {
    // Qt reports a change in new-document coordinates: [from, from + charsAdded) holds
    // everything inserted or reformatted. The blocks touching that range changed, including
    // the block holding the first character after it: it may have been split off from, or
    // merged with, the changed text.
    const QTextDocument* doc = document();
    QTextBlock first = doc->findBlock(from);
    if (!first.isValid()) {
        first = doc->firstBlock();
    }
    QTextBlock last = doc->findBlock(from + charsAdded);
    if (!last.isValid()) {
        last = doc->lastBlock();
    }

    // The rows of a table are laid out with the rows that go with them (tableBand()); the
    // other rows keep their lines, and only move
    const QTextTable* firstTable = tableOf(first);
    const QTextTable* lastTable = tableOf(last);
    if (firstTable) {
        const int lastRow = firstTable == lastTable ? rowOfBlock(*firstTable, last) : firstTable->rows() - 1;
        const auto [firstRow, bandLast] = tableBand(*firstTable, rowOfBlock(*firstTable, first), lastRow);
        const auto [bandFirstBlock, bandLastBlock] = bandBlocks(*firstTable, firstRow, bandLast);
        first = bandFirstBlock;
        if (firstTable == lastTable) {
            last = bandLastBlock;
        }
    }
    if (lastTable && lastTable != firstTable) {
        const auto [firstRow, lastRow] = tableBand(*lastTable, 0, rowOfBlock(*lastTable, last));
        last = bandBlocks(*lastTable, firstRow, lastRow).second;
    }
    return {first, last};
}

void KalahariTextDocumentLayout::clearLines(const QTextBlock& first, const QTextBlock& last) {
    const QTextBlock afterLast = last.next();
    for (QTextBlock block = first; block.isValid() && block != afterLast; block = block.next()) {
        block.clearLayout();
    }
}

void KalahariTextDocumentLayout::replaceRange(int first, int last, int oldLast) {
    const QTextDocument* doc = document();
    const int oldCount = static_cast<int>(m_blockHeights.size());

    if (oldLast < first - 1 || oldLast >= oldCount) {
        // The cache is out of step with the document: start it over
        invalidateAll();
        return;
    }

    // A small range is laid out now; a large one waits, with estimated heights
    const int rangeSize = std::max(last - first + 1, 0);
    const bool layOutNow = rangeSize <= IMMEDIATE_LAYOUT_LIMIT;
    std::vector<qreal> heights;
    std::vector<qreal> extents;
    std::vector<qreal> spaces;
    std::vector<qreal> notes;
    std::vector<FlowEntry> flowEntries;
    heights.reserve(static_cast<size_t>(rangeSize));
    extents.reserve(static_cast<size_t>(rangeSize));
    spaces.reserve(static_cast<size_t>(rangeSize));
    notes.reserve(static_cast<size_t>(rangeSize));
    flowEntries.reserve(static_cast<size_t>(rangeSize));
    // The tables whose head is in the range: the height of their header for the rows below
    // the range (tableHeader())
    struct HeaderOfRows {
        int first = 0;  // the blocks of the rows
        int last = 0;
        qreal header = 0;
    };
    std::vector<HeaderOfRows> headers;
    QTextBlock block = doc->findBlockByNumber(first);
    for (int number = first; number <= last && block.isValid(); ++number) {
        if (const QTextTable* table = tableOf(block)) {
            // Rows of a table: changedBlocks() gives the rows laid out together (tableBand())
            const int tableLast = doc->findBlock(table->lastPosition()).blockNumber();
            const QTextBlock end = doc->findBlockByNumber(std::min(last, tableLast));
            const auto [firstRow, lastRow] =
                tableBand(*table, rowOfBlock(*table, block), rowOfBlock(*table, end));
            qreal header = 0;
            const std::vector<TableBlockLayout> entries =
                layoutTable(*table, !layOutNow, true, firstRow, lastRow, &header);
            if (entries.empty()) {
                // No rows or columns: laid out as a paragraph
                const BlockMetrics& metrics = blockMetrics(block);
                qreal extent = block.length();
                heights.push_back(estimatedHeight(extent, metrics));
                extents.push_back(extent);
                qreal blockNotes = 0.0;
                spaces.push_back(askSpaceBelow(block, &blockNotes));
                notes.push_back(blockNotes);
                flowEntries.push_back(flowEntry(metrics, metrics.spaceBefore + metrics.estimatedLineHeight));
                block = block.next();
                continue;
            }
            const int bandFirst = bandBlocks(*table, firstRow, lastRow).first.blockNumber();
            for (size_t i = static_cast<size_t>(std::max(0, number - bandFirst)); i < entries.size(); ++i) {
                heights.push_back(entries[i].height);
                extents.push_back(entries[i].extent);
                // No cards in the cells: no space under them, but the notes of the table under
                // its last block
                qreal blockNotes = 0.0;
                const int entryNumber = bandFirst + static_cast<int>(i);
                spaces.push_back(
                    entryNumber == tableLast
                        ? askSpaceBelow(doc->findBlockByNumber(entryNumber), &blockNotes)
                        : 0.0);
                notes.push_back(blockNotes);
                flowEntries.push_back(entries[i].flow);
            }
            number = bandFirst + static_cast<int>(entries.size()) - 1;
            if (firstRow == 0 && number < tableLast) {
                headers.push_back({number + 1, tableLast, header});
            }
            block = doc->findBlockByNumber(number).next();
            continue;
        }
        const BlockMetrics& metrics = blockMetrics(block);
        qreal extent = block.length();
        qreal lead = metrics.spaceBefore + metrics.estimatedLineHeight;
        heights.push_back(layOutNow ? layoutBlock(block, &extent, true, &lead)
                                    : estimatedHeight(extent, metrics));
        extents.push_back(extent);
        qreal blockNotes = 0.0;
        spaces.push_back(askSpaceBelow(block, &blockNotes));
        notes.push_back(blockNotes);
        flowEntries.push_back(flowEntry(metrics, lead));
        if (hasNotes(block)) {
            flowEntries.back().flags |= FLOW_NOTES;
        }
        block = block.next();
    }
    const std::vector<char> pending(heights.size(), layOutNow ? 0 : 1);
    const std::vector<qreal> unplaced(heights.size(), FLOW_UNPLACED);
    const std::vector<qreal> noFlowHeights(heights.size(), 0.0);

    // Replace the range's cache entries. Positions only move when a height changed or
    // blocks were added/removed - typing within a line leaves them valid.
    const auto begin = static_cast<std::ptrdiff_t>(first);
    // oldLast + 1: oldLast is -1 when the cache is empty or blocks were inserted at the
    // very start, and begin() - 1 is not a valid iterator
    const auto oldEnd = static_cast<std::ptrdiff_t>(oldLast) + 1;
    m_pendingCount -= static_cast<int>(
        std::count(m_blockPending.begin() + begin, m_blockPending.begin() + oldEnd, 1));
    m_pendingCount += layOutNow ? 0 : static_cast<int>(pending.size());
    bool geometryChanged = true;
    if (static_cast<std::ptrdiff_t>(heights.size()) == oldEnd - begin) {
        geometryChanged =
            !std::equal(heights.begin(), heights.end(), m_blockHeights.begin() + begin) ||
            !std::equal(spaces.begin(), spaces.end(), m_blockSpaceBelow.begin() + begin);
        std::copy(heights.begin(), heights.end(), m_blockHeights.begin() + begin);
        std::copy(extents.begin(), extents.end(), m_blockExtents.begin() + begin);
        std::copy(spaces.begin(), spaces.end(), m_blockSpaceBelow.begin() + begin);
        std::copy(notes.begin(), notes.end(), m_blockNotesBelow.begin() + begin);
        std::copy(pending.begin(), pending.end(), m_blockPending.begin() + begin);
        std::copy(unplaced.begin(), unplaced.end(), m_blockFlowOffsets.begin() + begin);
        std::copy(noFlowHeights.begin(), noFlowHeights.end(), m_blockFlowHeights.begin() + begin);
        std::copy(flowEntries.begin(), flowEntries.end(), m_blockFlowEntries.begin() + begin);
    } else {
        const auto replace = [begin, oldEnd](auto& cache, const auto& entries) {
            cache.erase(cache.begin() + begin, cache.begin() + oldEnd);
            cache.insert(cache.begin() + begin, entries.begin(), entries.end());
        };
        replace(m_blockHeights, heights);
        replace(m_blockExtents, extents);
        replace(m_blockSpaceBelow, spaces);
        replace(m_blockNotesBelow, notes);
        replace(m_blockPending, pending);
        replace(m_blockFlowOffsets, unplaced);
        replace(m_blockFlowHeights, noFlowHeights);
        replace(m_blockFlowEntries, flowEntries);
    }
    for (const HeaderOfRows& rows : headers) {
        setTableHeader(rows.first, rows.last, rows.header);
    }
    // With page flow, the range's new lines are placed on the pages with the positions;
    // whether that moves the blocks below is known only then
    geometryChanged |= m_pageFlow.enabled;
    if (geometryChanged) {
        m_positionsDirty = true;
    }

    if (layOutNow && !heights.empty()) {
        emit blocksLaidOut(first, static_cast<int>(heights.size()));
    }
    if (m_pendingCount > 0) {
        m_backgroundTimer->start();
    } else {
        m_backgroundTimer->stop();
    }

    // Repaint the changed blocks, and everything below them if they moved it
    notifyGeometryChanged(first, (geometryChanged || heights.empty())
                                     ? UNBOUNDED_EXTENT
                                     : blockY(last) + blockHeight(last));
}

bool KalahariTextDocumentLayout::layOutBlock(int number, const QTextBlock& block) {
    if (const QTextTable* table = tableOf(block)) {
        bool changed = false;
        const int row = rowOfBlock(*table, block);
        layOutTableRows(*table, row, row, &changed);
        return changed;
    }
    const auto index = static_cast<size_t>(number);
    qreal extent = block.length();
    qreal lead = 0.0;
    const qreal height = layoutBlock(block, &extent, false, &lead);
    const bool changed = height != m_blockHeights[index];
    recountMarginsFrom(number);  // its lines, no longer estimated
    m_blockHeights[index] = height;
    m_blockExtents[index] = extent;
    m_blockFlowOffsets[index] = FLOW_UNPLACED;  // the new lines follow the block's top
    storeFlowEntries(number, block, lead);
    if (m_blockPending[index] != 0) {
        m_blockPending[index] = 0;
        --m_pendingCount;
    }
    return changed;
}

qreal KalahariTextDocumentLayout::boxHeightOf(qreal height, const BlockMetrics& metrics) {
    switch (metrics.lineSpacingRule) {
        case LineSpacingRule::Exactly:
            return metrics.lineHeight;
        case LineSpacingRule::AtLeast:
            return std::max(height + extraLeading(height, metrics.lineSpacing), metrics.lineHeight);
        case LineSpacingRule::Multiple:
            break;
    }
    return height + extraLeading(height, metrics.lineSpacing);
}

qreal KalahariTextDocumentLayout::lineBoxHeight(const QTextLine& line, const BlockMetrics& metrics,
                                                bool empty, qreal excess) {
    // An image's height above the text's line is added to the box, without line spacing:
    // an image is never cut by an exact line height, nor gets a share of a multiple
    const qreal height = empty ? std::max(line.height(), metrics.emptyLineHeight) : line.height();
    return boxHeightOf(height - excess, metrics) + excess;
}

qreal KalahariTextDocumentLayout::lineTopInBox(const QTextLine& line, const BlockMetrics& metrics,
                                               bool empty, qreal excess) {
    const qreal height = empty ? std::max(line.height(), metrics.emptyLineHeight) : line.height();
    // The line of an empty paragraph sits on the baseline of a line in the style's font
    const qreal baseline = empty ? std::max<qreal>(0.0, std::round(metrics.emptyAscent - line.ascent()))
                                 : 0.0;
    const qreal text = height - excess;
    return leadingAbove(boxHeightOf(text, metrics) - text) + baseline;
}

qreal KalahariTextDocumentLayout::imageExcess(const QTextBlock& block, const QTextLine& line,
                                              const BlockMetrics& metrics) const {
    if (!m_hasImages || !line.isValid()) {
        return 0.0;  // Qt sizes each image before the lines are placed: none was sized
    }
    const int start = block.position() + line.textStart();
    const int end = start + line.textLength();
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment fragment = it.fragment();
        if (!fragment.isValid() || fragment.position() + fragment.length() <= start) {
            continue;
        }
        if (fragment.position() >= end) {
            break;
        }
        if (isImageFormat(fragment.charFormat())) {
            return std::max<qreal>(0.0, line.ascent() - metrics.emptyAscent);
        }
    }
    return 0.0;
}

qreal KalahariTextDocumentLayout::areaWidth(const QTextBlock& block) const {
    const QTextTable* table = tableOf(block);
    if (table == nullptr) {
        const qreal documentWidth = document()->textWidth();
        return documentWidth > 0 ? documentWidth : UNWRAPPED_LINE_WIDTH;
    }
    // The width of the cell's text, as layoutTable() places it
    const std::vector<qreal> edges = tableColumnEdges(*table);
    const int columns = static_cast<int>(edges.size()) - 1;
    const QTextTableCell cell = table->cellAt(block.position());
    const int column = std::clamp(cell.column(), 0, std::max(0, columns - 1));
    const int right = std::clamp(column + cell.columnSpan(), column + 1, columns);
    return std::max<qreal>(1.0, edges[static_cast<size_t>(right)] -
                                    edges[static_cast<size_t>(column)] - 2.0 * cellPadding());
}

qreal KalahariTextDocumentLayout::layoutBlock(const QTextBlock& block, qreal* extent,
                                              bool keepGlyphs, qreal* lead,
                                              const CellPlacement* cell) const {
    QTextLayout* layout = block.layout();
    if (!layout) {
        return QFontMetricsF(document()->defaultFont()).height() + paragraphSpacing();
    }
    const BlockMetrics& metrics = blockMetrics(block);

    // Glyph fonts come from the document's character formats (resolved against the
    // document's default font) with the formats of the paragraph's style on top
    // (applyStyleFormats()), so the QTextLayout's own font is irrelevant here.

    // A paragraph without its own alignment has the alignment of its style
    const Qt::Alignment own = ownAlignment(block.blockFormat());
    const Qt::Alignment alignment = own ? own : metrics.alignment;

    QTextOption textOption;
    textOption.setAlignment(alignment);
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    // Every view is zoomed by a scaled painter: with the font's design metrics the glyph
    // advances scale linearly, so lines break the same way and letters stay evenly
    // spaced at every zoom, in the pages and in the endless page alike
    textOption.setUseDesignMetrics(true);
    if (metrics.tabStopDistance > 0) {
        textOption.setTabStopDistance(metrics.tabStopDistance);
    }
    layout->setTextOption(textOption);

    // Blocks laid out on demand keep only the line breaks: their glyphs are shaped again
    // when a line is drawn (only the visible ones are). Cached glyphs of a whole chapter
    // took most of the editor's memory. A block laid out after an edit keeps its glyphs:
    // the cursor works there, and each cursor position query would shape the whole
    // paragraph again. Justified lines come out the same either way (tested).
    layout->setCacheEnabled(keepGlyphs);

    // The lines lie between the margins of the style, in a cell between those of its text
    const qreal documentWidth = document()->textWidth();
    const qreal width = cell != nullptr ? cell->width
                                        : (documentWidth > 0 ? documentWidth : UNWRAPPED_LINE_WIDTH);
    const qreal lineWidth = std::max<qreal>(1.0, width - metrics.leftMargin - metrics.rightMargin);
    const QPointF cellOffset = cell != nullptr ? QPointF(cell->left, cell->top) : QPointF();

    // The first-line indent belongs to paragraphs whose lines start at their leading
    // edge (left-aligned and justified ones), never more than half of the line (a negative
    // one, a hanging indent, never out of the left margin). Right-to-left lines keep x at
    // the margin: the shorter line already leaves the indent on their leading (right) side.
    // A list item's first line starts after its number. A paragraph of a table has no
    // first-line indent: the cell's text starts at its edge.
    const bool leadingAligned = !(alignment & (Qt::AlignRight | Qt::AlignHCenter));
    const qreal ownIndent = metrics.listItem ? markerOverflow(block, metrics)
                            : cell != nullptr ? 0.0
                                              : metrics.firstLineIndent;
    const qreal firstIndent =
        leadingAligned ? std::clamp(ownIndent, -metrics.leftMargin, lineWidth / 2.0) : 0.0;
    const bool rightToLeft = block.textDirection() == Qt::RightToLeft;
    const bool empty = block.length() <= 1;

    // Lines are stacked from the space above the paragraph, each with its extra leading
    // split above and below it. Horizontal alignment is Qt's (QTextOption) - the only manual
    // x offsets are the margin and the indent, which also shortens the line (Qt aligns
    // within the shorter width).
    layout->beginLayout();
    qreal y = metrics.spaceBefore;
    qreal textLength = 0;
    qreal firstBox = 0;
    for (QTextLine line = layout->createLine(); line.isValid(); line = layout->createLine()) {
        const qreal lineIndent = line.lineNumber() == 0 ? firstIndent : 0.0;
        line.setLineWidth(lineWidth - lineIndent);
        const qreal excess = imageExcess(block, line, metrics);
        line.setPosition(cellOffset + QPointF(metrics.leftMargin + (rightToLeft ? 0.0 : lineIndent),
                                              y + lineTopInBox(line, metrics, empty, excess)));
        const qreal box = lineBoxHeight(line, metrics, empty, excess);
        if (line.lineNumber() == 0) {
            firstBox = box;
        }
        y += box;
        textLength += line.naturalTextWidth();
    }
    layout->endLayout();
    if (extent) {
        *extent = textLength / m_averageCharWidth;
    }
    if (lead) {
        // A heading is kept with the first row of a list of figures, as with the first line
        // of a paragraph: the list goes on to the next pages
        if (blockObject(block) == TextObject::FigureList) {
            const FigureListPlacement& placement = figureListPlacement();
            if (!placement.rows.empty()) {
                firstBox = placement.rows.front().height;
            }
        }
        *lead = metrics.spaceBefore + firstBox;
    }
    return y + metrics.spaceAfter;
}

QRectF KalahariTextDocumentLayout::lineBox(const QTextLine& line, qreal lineSpacing) {
    const qreal extra = extraLeading(line.height(), lineSpacing);
    return QRectF(line.x(), line.y() - leadingAbove(extra), line.width(), line.height() + extra);
}

QRectF KalahariTextDocumentLayout::lineBoxOf(const QTextBlock& block, const QTextLine& line) const {
    const BlockMetrics& metrics = blockMetrics(block);
    const bool empty = block.length() <= 1;
    const qreal excess = imageExcess(block, line, metrics);
    return QRectF(line.x(), line.y() - lineTopInBox(line, metrics, empty, excess), line.width(),
                  lineBoxHeight(line, metrics, empty, excess));
}

QRectF KalahariTextDocumentLayout::blockLineBox(const QTextBlock& block, const QTextLine& line) {
    const auto* layout = block.isValid() ? qobject_cast<const KalahariTextDocumentLayout*>(
                                               block.document()->documentLayout())
                                         : nullptr;
    return layout != nullptr ? layout->lineBoxOf(block, line) : lineBox(line, 1.0);
}

int KalahariTextDocumentLayout::lineIndexAt(const QTextLayout& layout, qreal localY,
                                            qreal lineSpacing) {
    return lineIndexAt(layout, localY,
                       [lineSpacing](const QTextLine& line) { return lineBox(line, lineSpacing); });
}

int KalahariTextDocumentLayout::lineIndexAt(const QTextBlock& block, const QTextLayout& layout,
                                            qreal localY) {
    return lineIndexAt(layout, localY,
                       [&block](const QTextLine& line) { return blockLineBox(block, line); });
}

int KalahariTextDocumentLayout::lineIndexAt(const QTextLayout& layout, qreal localY,
                                            const std::function<QRectF(const QTextLine&)>& boxOf) {
    const int count = layout.lineCount();
    if (count == 0) {
        return -1;
    }
    // First line whose box ends below localY (boxes are stacked in line order)
    int low = 0;
    int high = count - 1;
    while (low < high) {
        const int middle = low + (high - low) / 2;
        if (localY < boxOf(layout.lineAt(middle)).bottom()) {
            high = middle;
        } else {
            low = middle + 1;
        }
    }
    return low;
}

// =============================================================================
// Block Position Management
// =============================================================================

void KalahariTextDocumentLayout::updateBlockPositions() const {
    if (!m_positionsDirty) return;

    const size_t count = m_blockHeights.size();
    m_blockYPositions.resize(count);
    qreal y = 0;
    if (!m_pageFlow.enabled) {
        for (size_t i = 0; i < count; ++i) {
            m_blockYPositions[i] = y;
            y += m_blockHeights[i] + m_blockSpaceBelow[i];
        }
        m_cachedDocumentHeight = y;
        m_contentBottom = y;
        m_positionsDirty = false;
        return;
    }

    // Page flow: blocks are placed in order, each where the one above ends. A laid out
    // block whose place on its page is the same as when its lines were placed keeps
    // them (a block moved by whole pages does too); the others are placed again.
    //
    // Queried in the middle of a document change (Qt asks for block rects then), the
    // cache is out of step with the blocks: positions then come from the cached heights,
    // without touching any lines, and are computed again on the next query.
    //
    // A block kept with the next one (a heading) starts on the next page when it would end
    // its own, the space it leaves belonging to the block above. The text after a page
    // break starts on the next page, the rest of the break's page belonging to the break.
    const bool inStep = document()->blockCount() == static_cast<int>(count);
    m_headerRepeats.clear();
    m_pageNotes.clear();
    m_notesLastPage = -1;
    m_endnoteLines.clear();
    m_endnotesTop = -1.0;
    const bool withNotes = inStep && (!footnoteNumbers().notes().empty() ||
                                      !m_footnoteContext.otherEndnotes.empty());
    qreal lastSpacing = 0;
    QTextBlock block = document()->begin();
    for (size_t i = 0; i < count; ++i, block = block.next()) {
        const FlowEntry& entry = m_blockFlowEntries[i];
        if ((entry.flags & FLOW_TABLE) != 0) {
            // A row of a table goes on the next page as a whole when it does not fit in the
            // rest of its own, a row higher than a page (or one that may be) split between
            // pages; its blocks have its top
            if ((entry.flags & FLOW_ROW_START) == 0) {
                m_blockYPositions[i] = y;
                y += m_blockHeights[i];
                lastSpacing = 0;
                continue;
            }
            size_t last = i;
            while (last + 1 < count && (m_blockFlowEntries[last + 1].flags & FLOW_TABLE) != 0 &&
                   (m_blockFlowEntries[last + 1].flags & FLOW_ROW_START) == 0) {
                ++last;
            }
            int tableFirst = -1;
            if (entry.header > 0 && inStep && block.isValid()) {
                if (const QTextTable* table = tableOf(block)) {
                    tableFirst = document()->findBlock(table->firstPosition()).blockNumber();
                }
            }
            const auto [top, height] = placeRow(i, last, y, inStep, tableFirst);
            for (size_t k = i; k <= last; ++k) {
                m_blockYPositions[k] = top;
            }
            y = top + height;
            lastSpacing = 0;
            // The footnotes of its cells under it, on the page where it ends
            const QTextBlock rowFirst = block;
            for (; i < last; ++i) {
                block = block.next();
            }
            if (withNotes && block.isValid()) {
                const std::vector<size_t> notes = notesBetween(
                    rowFirst.position(), block.position() + block.length(), NoteKind::Footnote);
                if (!notes.empty()) {
                    reserveNotes(y, notes);
                }
            }
            continue;
        }
        if ((entry.flags & FLOW_PAGE_BREAK_BEFORE) != 0 &&
            y - std::floor((y + FLOW_EPSILON) / m_pageFlow.pitch) * m_pageFlow.pitch > FLOW_EPSILON) {
            y = nextPageTop(y);
        }
        if ((entry.flags & FLOW_KEEP_WITH_NEXT) != 0) {
            y = keptBlockTop(i, y);
        }
        m_blockYPositions[i] = y;
        const qreal offset = std::fmod(y, m_pageFlow.pitch);
        // A block with notes, or on a page with notes, is placed again: the notes may take
        // another part of the page now
        const bool withPageNotes =
            withNotes && ((entry.flags & FLOW_NOTES) != 0 ||
                          std::floor((y + FLOW_EPSILON) / m_pageFlow.pitch) <= m_notesLastPage);
        qreal height = 0;
        if (m_blockPending[i] != 0) {
            height = (entry.flags & FLOW_PAGE_BREAK) != 0 ? nextPageTop(y) - y
                                                          : flowEstimate(m_blockHeights[i], y, entry);
        } else if (!withPageNotes && std::abs(m_blockFlowOffsets[i] - offset) < FLOW_EPSILON) {
            height = m_blockFlowHeights[i];
        } else if (!inStep || !block.isValid() || !block.layout()) {
            height = m_blockFlowOffsets[i] == FLOW_UNPLACED ? m_blockHeights[i]
                                                             : m_blockFlowHeights[i];
        } else {
            height = paginateBlock(block, y, blockMetrics(block));
            m_blockFlowOffsets[i] = withPageNotes ? FLOW_PLACE_AGAIN : offset;
            m_blockFlowHeights[i] = height;
        }
        y += height;
        // A page break ending the text leaves the next page in the document, empty
        lastSpacing = (entry.flags & FLOW_PAGE_BREAK) != 0 ? -2.0 * FLOW_EPSILON : entry.spaceAfter;
    }
    m_cachedDocumentHeight = y;
    m_contentBottom = y - lastSpacing;
    if (withNotes) {
        // The endnotes after the last line; the pages end with the last of the notes
        m_contentBottom = placeEndnotes(m_contentBottom);
        if (m_notesLastPage >= 0) {
            m_contentBottom = std::max(m_contentBottom, m_notesLastPage * m_pageFlow.pitch + 1.0);
        }
    }
    m_positionsDirty = !inStep;
}

qreal KalahariTextDocumentLayout::flowLineTop(qreal top, qreal height, qreal notes) const {
    const qreal pitch = m_pageFlow.pitch;
    qreal placed = top;
    for (;;) {
        const qreal page = std::floor((placed + FLOW_EPSILON) / pitch);
        const qreal areaHeight = flowAreaHeight(page);
        const bool reduced = areaHeight < m_pageFlow.textHeight - FLOW_EPSILON;  // by notes
        const qreal offset = placed - page * pitch;
        // The notes of the line need the line above them on a page without notes yet
        const qreal needed = notes > 0.0 ? notes + (reduced ? 0.0 : noteSeparatorRoom()) : 0.0;
        const bool betweenAreas = offset >= areaHeight - FLOW_EPSILON;
        // A line at the top of a whole area stays even when it is taller than the area:
        // moving it on would not make it fit; an area made smaller by notes it leaves
        const bool crossesEnd = (offset > FLOW_EPSILON || reduced) &&
                                offset + height + needed > areaHeight + FLOW_EPSILON;
        if (!betweenAreas && !crossesEnd) {
            return placed;
        }
        placed = (page + 1.0) * pitch;
    }
}

qreal KalahariTextDocumentLayout::paginateBlock(const QTextBlock& block, qreal top,
                                                const BlockMetrics& metrics) const {
    QTextLayout* layout = block.layout();
    // Line boxes are stacked as layoutBlock() stacks them; a page break moves a line and
    // all lines after it down to the next text area. A page break's own line stays where it
    // is (a thin line that says why the page ends there): it may end in the page's margin.
    const bool empty = layout->text().isEmpty();
    const int count = layout->lineCount();
    if (count == 0) {
        // No lines (not laid out by this layout): the block keeps its default height
        const qreal height = QFontMetricsF(document()->defaultFont()).height();
        const qreal natural = metrics.spaceBefore;
        const qreal shift = flowLineTop(top + natural, height) - (top + natural);
        return metrics.pageBreak ? nextPageTop(top) - top
                                 : natural + height + shift + metrics.spaceAfter;
    }

    // A list of figures goes on to the next pages as the lines of a paragraph do, row by row:
    // its line at the top of its first row, the block as high as its rows on the pages
    if (count == 1 && blockObject(block) == TextObject::FigureList) {
        QTextLine line = layout->lineAt(0);
        const FigureListPlacement& placement = figureListPlacement();
        const qreal boxTop = top + metrics.spaceBefore;
        const std::vector<qreal> tops = figureListTops(boxTop);
        if (!tops.empty() && tops.size() == placement.rows.size()) {
            line.setPosition(QPointF(line.x(), metrics.spaceBefore + tops.front()));
            return metrics.spaceBefore + tops.back() + placement.rows.back().height +
                   metrics.spaceAfter;
        }
    }

    // The footnotes of each line (with page flow they take the bottom of its page); placed
    // again with the lines, the notes of the pages from the block's on as they were before it
    std::vector<std::vector<size_t>> lineNotes;
    const std::vector<size_t> notes =
        notesBetween(block.position(), block.position() + block.length(), NoteKind::Footnote);
    if (!notes.empty()) {
        lineNotes.resize(static_cast<size_t>(count));
        const std::vector<NotePlace>& places = footnoteNumbers().notes();
        for (const size_t index : notes) {
            const QTextLine line = layout->lineForTextPosition(places[index].position -
                                                               block.position());
            if (line.isValid()) {
                lineNotes[static_cast<size_t>(line.lineNumber())].push_back(index);
            }
        }
    }
    const auto firstPage = static_cast<size_t>(
        std::max<qreal>(0.0, std::floor((top + FLOW_EPSILON) / m_pageFlow.pitch)));
    const std::vector<PageNotes> notesBefore =
        lineNotes.empty() || firstPage >= m_pageNotes.size()
            ? std::vector<PageNotes>{}
            : std::vector<PageNotes>(m_pageNotes.begin() + static_cast<std::ptrdiff_t>(firstPage),
                                     m_pageNotes.end());
    const int notesLastBefore = m_notesLastPage;

    // A line marked here starts the next text area, though it would fit in its own: to keep
    // the first or the last two lines of the paragraph together, or all of them
    const qreal pitch = m_pageFlow.pitch;
    std::vector<char> breakBefore(static_cast<size_t>(count), 0);
    std::vector<qreal> pages(static_cast<size_t>(count), 0.0);
    qreal natural = 0;  // box top within the block without page breaks
    qreal shift = 0;    // how far page breaks moved the lines so far
    const auto place = [&]() {
        natural = metrics.spaceBefore;
        shift = 0;
        if (!lineNotes.empty()) {
            m_pageNotes.resize(std::min(m_pageNotes.size(), firstPage));
            m_pageNotes.insert(m_pageNotes.end(), notesBefore.begin(), notesBefore.end());
            m_notesLastPage = notesLastBefore;
        }
        for (int i = 0; i < count; ++i) {
            QTextLine line = layout->lineAt(i);
            const qreal excess = imageExcess(block, line, metrics);
            const qreal boxHeight = lineBoxHeight(line, metrics, empty, excess);
            const qreal boxTop = top + natural + shift;
            const std::vector<size_t>* ownNotes =
                lineNotes.empty() || lineNotes[static_cast<size_t>(i)].empty()
                    ? nullptr
                    : &lineNotes[static_cast<size_t>(i)];
            qreal placed = metrics.pageBreak
                               ? boxTop
                               : flowLineTop(boxTop, boxHeight,
                                             ownNotes != nullptr ? notesNeeded(*ownNotes) : 0.0);
            const qreal offset = placed - std::floor((placed + FLOW_EPSILON) / pitch) * pitch;
            if (breakBefore[static_cast<size_t>(i)] != 0 && offset > FLOW_EPSILON) {
                placed = nextPageTop(placed);
            }
            if (ownNotes != nullptr) {
                reserveNotes(placed + boxHeight, *ownNotes);
            }
            shift += placed - boxTop;
            pages[static_cast<size_t>(i)] = std::floor((placed + FLOW_EPSILON) / pitch);
            line.setPosition(
                QPointF(line.x(), natural + shift + lineTopInBox(line, metrics, empty, excess)));
            natural += boxHeight;
        }
    };
    place();
    if (metrics.pageBreak) {
        return nextPageTop(top) - top;
    }
    for (int round = 0; round < KEEP_LINES_ROUNDS && pages.front() != pages.back(); ++round) {
        // The whole paragraph on the next page: the breaks marked before are not needed
        const auto startOnNextPage = [&]() {
            if (breakBefore[0] != 0) {
                return false;
            }
            std::fill(breakBefore.begin(), breakBefore.end(), 0);
            breakBefore[0] = 1;
            return true;
        };
        bool moved = false;
        const qreal linesHeight = natural - metrics.spaceBefore;
        if (metrics.keepTogether && linesHeight <= m_pageFlow.textHeight + FLOW_EPSILON) {
            moved = startOnNextPage();
        } else if (metrics.widowControl && count >= 2) {
            // The first line alone at the end of a page goes on with the second; the last
            // line alone at the top of a page takes the line before it
            const auto last = static_cast<size_t>(count - 1);
            if (pages[0] != pages[1]) {
                moved = startOnNextPage();
            } else if (pages[last] != pages[last - 1] && breakBefore[last - 1] == 0) {
                breakBefore[last - 1] = 1;
                moved = true;
            }
        }
        if (!moved) {
            break;
        }
        place();
    }
    return natural + shift + metrics.spaceAfter;
}

qreal KalahariTextDocumentLayout::flowEstimate(qreal height, qreal top,
                                               const FlowEntry& entry) const {
    // The estimate is a whole number of estimated lines; they are placed as lines are
    const qreal lineHeight = entry.estimatedLineHeight;
    const int lines = std::max(
        1, static_cast<int>(std::lround((height - entry.spaceBefore - entry.spaceAfter) / lineHeight)));
    qreal y = top + entry.spaceBefore;
    for (int i = 0; i < lines; ++i) {
        y = flowLineTop(y, lineHeight) + lineHeight;
    }
    return y - top + entry.spaceAfter;
}

qreal KalahariTextDocumentLayout::nextPageTop(qreal y) const {
    const qreal page = std::floor((y + FLOW_EPSILON) / m_pageFlow.pitch);
    return (page + 1.0) * m_pageFlow.pitch;
}

qreal KalahariTextDocumentLayout::keptBlockTop(size_t index, qreal top) const {
    // The blocks kept with the next one from this one on, and the paragraph after them
    const size_t count = m_blockHeights.size();
    qreal needed = 0;  // from the top down to the end of that paragraph's first line
    size_t next = index;
    for (; next < count && (m_blockFlowEntries[next].flags & FLOW_KEEP_WITH_NEXT) != 0; ++next) {
        needed += m_blockHeights[next];
    }
    if (next >= count ||
        (m_blockFlowEntries[next].flags & (FLOW_PAGE_BREAK | FLOW_PAGE_BREAK_BEFORE)) != 0) {
        return top;  // nothing to keep them with: they stay where the text puts them
    }
    needed += m_blockFlowEntries[next].lead;

    // They move only from below the top of a text area, and only when they fit in the next
    // one (a longer run would not fit there either)
    const qreal pitch = m_pageFlow.pitch;
    const qreal page = std::floor((top + FLOW_EPSILON) / pitch);
    const qreal areaHeight = flowAreaHeight(page);
    const qreal offset = top - page * pitch;
    const bool fits = offset + needed <= areaHeight + FLOW_EPSILON;
    if (offset <= FLOW_EPSILON || fits || needed > areaHeight + FLOW_EPSILON) {
        return top;
    }
    return nextPageTop(top);
}

qreal KalahariTextDocumentLayout::flowDocumentHeight() const {
    // The page holding the bottom of the last line ends the document
    const qreal bottom = std::max<qreal>(0.0, m_contentBottom - FLOW_EPSILON);
    const qreal lastPage = std::floor(bottom / m_pageFlow.pitch);
    return lastPage * m_pageFlow.pitch + m_pageFlow.textHeight;
}

qreal KalahariTextDocumentLayout::blockY(int blockNumber) const {
    updateBlockPositions();

    if (blockNumber < 0) {
        return 0;
    }
    if (static_cast<size_t>(blockNumber) >= m_blockYPositions.size()) {
        return m_cachedDocumentHeight;
    }
    return m_blockYPositions[static_cast<size_t>(blockNumber)];
}

qreal KalahariTextDocumentLayout::blockHeight(int blockNumber) const {
    const auto index = static_cast<size_t>(blockNumber);
    if (blockNumber < 0 || index >= m_blockHeights.size()) {
        return 0;
    }
    if (!m_pageFlow.enabled) {
        return m_blockHeights[index] + m_blockSpaceBelow[index];
    }
    // Page flow: from the block's top to the next block's (page breaks included)
    updateBlockPositions();
    const qreal bottom =
        index + 1 < m_blockYPositions.size() ? m_blockYPositions[index + 1] : m_cachedDocumentHeight;
    return bottom - m_blockYPositions[index];
}

int KalahariTextDocumentLayout::blockNumberAtY(qreal y) const {
    updateBlockPositions();

    if (m_blockYPositions.empty()) {
        return -1;
    }
    // Last block starting at or above y
    const auto it = std::upper_bound(m_blockYPositions.begin(), m_blockYPositions.end(), y);
    if (it == m_blockYPositions.begin()) {
        return 0;
    }
    return static_cast<int>(std::distance(m_blockYPositions.begin(), it)) - 1;
}

int KalahariTextDocumentLayout::firstBlockOfRow(int blockNumber) const {
    const auto count = static_cast<int>(m_blockFlowEntries.size());
    int number = blockNumber;
    while (number > 0 && number < count &&
           (m_blockFlowEntries[static_cast<size_t>(number)].flags & FLOW_TABLE) != 0 &&
           (m_blockFlowEntries[static_cast<size_t>(number)].flags & FLOW_ROW_START) == 0) {
        --number;
    }
    return number;
}

int KalahariTextDocumentLayout::lastBlockOfRow(int blockNumber) const {
    const auto count = static_cast<int>(m_blockFlowEntries.size());
    if (blockNumber < 0 || blockNumber >= count ||
        (m_blockFlowEntries[static_cast<size_t>(blockNumber)].flags & FLOW_TABLE) == 0) {
        return blockNumber;
    }
    int number = blockNumber;
    while (number + 1 < count &&
           (m_blockFlowEntries[static_cast<size_t>(number) + 1].flags & FLOW_TABLE) != 0 &&
           (m_blockFlowEntries[static_cast<size_t>(number) + 1].flags & FLOW_ROW_START) == 0) {
        ++number;
    }
    return number;
}

qreal KalahariTextDocumentLayout::rowHeight(size_t first) const {
    qreal height = m_blockHeights[first];
    for (size_t i = first + 1; i < m_blockHeights.size() &&
                               (m_blockFlowEntries[i].flags & FLOW_TABLE) != 0 &&
                               (m_blockFlowEntries[i].flags & FLOW_ROW_START) == 0;
         ++i) {
        height += m_blockHeights[i];
    }
    return height;
}

int KalahariTextDocumentLayout::blockNumberAt(const QPointF& point) const {
    const int number = laidOutBlockAtY(point.y());
    const QTextBlock block = document()->findBlockByNumber(number);
    const QTextTable* table = tableOf(block);
    if (!table) {
        return number;
    }

    // The cell of the row in the column under the point, and in it the last paragraph
    // starting above the point (a cell spanning rows starts in a row above)
    const std::vector<qreal> edges = tableColumnEdges(*table);
    const auto after = std::upper_bound(edges.begin() + 1, edges.end() - 1, point.x());
    const int column = static_cast<int>(std::distance(edges.begin() + 1, after));
    const int row = table->cellAt(document()->findBlockByNumber(firstBlockOfRow(number)).position()).row();
    const QTextTableCell cell = table->cellAt(row, column);
    if (!cell.isValid()) {
        return number;
    }
    QTextBlock found = document()->findBlock(cell.firstPosition());
    const QTextBlock end = document()->findBlock(cell.lastPosition()).next();
    const qreal localY = point.y() - blockY(found.blockNumber());
    for (QTextBlock candidate = found.next(); candidate.isValid() && candidate != end;
         candidate = candidate.next()) {
        const QTextLayout* lines = candidate.layout();
        if (!lines || lines->lineCount() == 0 ||
            lineBoxOf(candidate, lines->lineAt(0)).top() > localY) {
            break;
        }
        found = candidate;
    }
    return found.blockNumber();
}

qreal KalahariTextDocumentLayout::cellPadding() const {
    const qreal line = QFontMetricsF(document()->defaultFont()).height();
    return std::max(MIN_CELL_PADDING, std::round(line * CELL_PADDING_LINES));
}

qreal KalahariTextDocumentLayout::tableBorderWidth(const QTextTable& table) const {
    return std::max<qreal>(0.0, tablePropertiesOf(table.format()).borders.width * packagePixels());
}

std::vector<qreal> KalahariTextDocumentLayout::tableColumnEdges(const QTextTable& table) const {
    const int columns = std::max(1, table.columns());
    const qreal documentWidth = document()->textWidth();
    const qreal textWidth = documentWidth > 0 ? documentWidth : UNWRAPPED_LINE_WIDTH;

    // The table's width and place: never wider than the text, its points shown as the
    // style's points are
    const TableProperties properties = tablePropertiesOf(table.format());
    const qreal pixels = packagePixels();
    const qreal narrowest = std::min(textWidth, MIN_COLUMN_PADDINGS * cellPadding() * columns);
    const qreal indent = properties.alignment == TableAlignment::Left
                             ? std::clamp(std::round(properties.indent * pixels), 0.0, textWidth - narrowest)
                             : 0.0;
    const qreal width = std::clamp(properties.width ? std::round(*properties.width * pixels)
                                                    : textWidth - indent,
                                   narrowest, textWidth - indent);
    const qreal left = properties.alignment == TableAlignment::Center ? std::round((textWidth - width) / 2.0)
                       : properties.alignment == TableAlignment::Right ? textWidth - width
                                                                       : indent;

    // The widths of the format in percent, else equal columns
    std::vector<qreal> shares(static_cast<size_t>(columns), 1.0);
    const QList<QTextLength> widths = table.format().columnWidthConstraints();
    if (widths.size() == columns) {
        std::vector<qreal> percent;
        for (const QTextLength& length : widths) {
            if (length.type() != QTextLength::PercentageLength || length.rawValue() <= 0) {
                break;
            }
            percent.push_back(length.rawValue());
        }
        if (percent.size() == shares.size()) {
            shares = percent;
        }
    }
    qreal total = 0;
    for (const qreal share : shares) {
        total += share;
    }
    std::vector<qreal> edges(shares.size() + 1, left);
    qreal sum = 0;
    for (size_t i = 0; i < shares.size(); ++i) {
        sum += shares[i];
        edges[i + 1] = left + std::round(width * sum / total);
    }
    edges.back() = left + std::round(width);
    return edges;
}

std::vector<KalahariTextDocumentLayout::TableRow> KalahariTextDocumentLayout::tableRows(
    const QTextTable& table) const {
    // Each row from its first block: its top, and the height of its blocks together (in
    // the page flow, as the flow places them)
    updateBlockPositions();
    std::vector<TableRow> rows(static_cast<size_t>(std::max(0, table.rows())));
    qreal bottom = 0;
    qreal header = 0;
    for (int row = 0; row < table.rows(); ++row) {
        TableRow& place = rows[static_cast<size_t>(row)];
        place.top = bottom;
        for (int column = 0; column < table.columns(); ++column) {
            const QTextTableCell cell = table.cellAt(row, column);
            if (cell.row() != row || cell.column() != column) {
                continue;
            }
            const int first = document()->findBlock(cell.firstPosition()).blockNumber();
            if (first < 0 || static_cast<size_t>(first) >= m_blockHeights.size()) {
                break;
            }
            place.top = blockY(first);
            place.height = rowFlowHeight(static_cast<size_t>(first));
            header = m_blockFlowEntries[static_cast<size_t>(first)].header;
            break;
        }
        bottom = place.top + place.height;

        // Its parts on the pages: a part goes to the end of its page's text area, the next
        // one starts below the header repeated on the next page
        qreal top = place.top;
        while (m_pageFlow.enabled && m_pageFlow.pitch > 0) {
            const qreal pageTop = pageTopOf(top);
            const qreal areaEnd = pageTop +
                                  flowAreaHeight(std::floor(pageTop / m_pageFlow.pitch + 0.5));
            if (bottom <= areaEnd + FLOW_EPSILON) {
                break;
            }
            place.parts.emplace_back(top, areaEnd);
            top = nextPageTop(top) + header;
        }
        place.parts.emplace_back(top, std::max(top, bottom));
    }
    return rows;
}

std::vector<qreal> KalahariTextDocumentLayout::tableRowHeights(const QTextTable& table) const {
    std::vector<qreal> heights(static_cast<size_t>(std::max(0, table.rows())), 0.0);
    for (int row = 0; row < table.rows(); ++row) {
        for (int column = 0; column < table.columns(); ++column) {
            const QTextTableCell cell = table.cellAt(row, column);
            if (cell.row() != row || cell.column() != column) {
                continue;
            }
            const int first = document()->findBlock(cell.firstPosition()).blockNumber();
            if (first >= 0 && static_cast<size_t>(first) < m_blockHeights.size()) {
                heights[static_cast<size_t>(row)] = rowHeight(static_cast<size_t>(first));
            }
            break;
        }
    }
    return heights;
}

std::vector<qreal> KalahariTextDocumentLayout::headerRepeats(const QTextTable& table) const {
    updateBlockPositions();
    std::vector<qreal> tops;
    const int first = document()->findBlock(table.firstPosition()).blockNumber();
    for (const HeaderRepeat& repeat : m_headerRepeats) {
        if (repeat.tableFirstBlock == first) {
            tops.push_back(repeat.top);
        }
    }
    return tops;
}

QPointF KalahariTextDocumentLayout::fromRepeatedHeader(const QPointF& point) const {
    updateBlockPositions();
    const auto after = std::upper_bound(
        m_headerRepeats.begin(), m_headerRepeats.end(), point.y(),
        [](qreal y, const HeaderRepeat& repeat) { return y < repeat.top; });
    if (after == m_headerRepeats.begin()) {
        return point;
    }
    const HeaderRepeat& repeat = *std::prev(after);
    if (point.y() >= repeat.top + repeat.height) {
        return point;
    }
    return QPointF(point.x(), point.y() - repeat.top + blockY(repeat.tableFirstBlock));
}

qreal KalahariTextDocumentLayout::rowFlowHeight(size_t first) const {
    if (m_pageFlow.enabled && first < m_blockFlowHeights.size() &&
        m_blockFlowHeights[first] > 0) {
        return m_blockFlowHeights[first];
    }
    return rowHeight(first);
}

qreal KalahariTextDocumentLayout::pageTopOf(qreal y) const {
    return std::floor((y + FLOW_EPSILON) / m_pageFlow.pitch) * m_pageFlow.pitch;
}

qreal KalahariTextDocumentLayout::paginateRow(size_t first, size_t last, qreal top,
                                              qreal header, bool natural) const {
    // Each cell's lines stacked as layoutTable() stacks them; a line that does not fit in
    // the rest of its page goes on to the next one, below the repeated header, with the
    // cell's lines after it
    const qreal padding = cellPadding();
    qreal height = rowHeight(first);
    qreal shift = 0;
    int cell = -1;
    QTextBlock block = document()->findBlockByNumber(static_cast<int>(first));
    for (size_t index = first; index <= last && block.isValid(); ++index, block = block.next()) {
        const QTextTable* table = tableOf(block);
        const int own = table ? table->cellAt(block.position()).firstPosition() : -1;
        if (own != cell) {
            cell = own;
            shift = 0;
        }
        QTextLayout* layout = block.layout();
        if (!layout) {
            continue;
        }
        const BlockMetrics& metrics = blockMetrics(block);
        const bool empty = block.length() <= 1;
        qreal y = m_blockFlowEntries[index].cellTop + metrics.spaceBefore;
        for (int i = 0; i < layout->lineCount(); ++i) {
            QTextLine line = layout->lineAt(i);
            const qreal excess = imageExcess(block, line, metrics);
            const qreal box = lineBoxHeight(line, metrics, empty, excess);
            if (!natural) {
                const qreal boxTop = top + y + shift;
                const qreal moved = flowLineTop(boxTop, box);
                if (moved > boxTop + FLOW_EPSILON) {
                    shift += moved + header - boxTop;
                }
            }
            line.setPosition(QPointF(line.x(), y + shift + lineTopInBox(line, metrics, empty, excess)));
            y += box;
        }
        height = std::max(height, y + shift + padding);
    }
    return height;
}

std::pair<qreal, qreal> KalahariTextDocumentLayout::placeRow(size_t i, size_t last, qreal y,
                                                             bool inStep, int tableFirst) const {
    const FlowEntry& entry = m_blockFlowEntries[i];
    const qreal natural = rowHeight(i);
    const qreal header = entry.header;
    const bool splits = (entry.flags & FLOW_ROW_SPLIT) != 0;

    // On this page when it fits (a header row with the first row after the header), else
    // on the next one; a row that may be split starts here when its first lines fit
    qreal top = flowLineTop(y, splits ? entry.estimatedLineHeight : std::max(natural, entry.lead));
    const qreal pageTop = pageTopOf(top);
    const bool moved = top > y + FLOW_EPSILON;
    const bool startsPage = top - pageTop < FLOW_EPSILON;
    if (header > 0 && startsPage) {
        // A row starting a page the table goes on to: the header rows above it again
        if (tableFirst >= 0) {
            m_headerRepeats.push_back({top, header, tableFirst});
        }
        top += header;
    }
    const qreal pageIndex = std::floor((pageTop + FLOW_EPSILON) / m_pageFlow.pitch);
    const bool fits = top + natural <= pageTop + flowAreaHeight(pageIndex) + FLOW_EPSILON;
    const bool split = !fits && (splits || moved || startsPage);

    qreal height = natural;
    const bool laidOut = inStep && m_blockPending[i] == 0;
    const qreal offset = top - pageTop;
    // Placed again on a page with notes: they may take another part of it now
    const bool underNotes = pageIndex <= m_notesLastPage;
    if (split && laidOut) {
        if (!underNotes && std::abs(m_blockFlowOffsets[i] - offset) < FLOW_EPSILON &&
            m_blockFlowHeights[i] > 0) {
            height = m_blockFlowHeights[i];  // placed there already
        } else {
            height = paginateRow(i, last, top, header, false);
            m_blockFlowOffsets[i] = underNotes ? FLOW_PLACE_AGAIN : offset;
        }
    } else if (split) {
        // Not laid out yet (or the document in the middle of a change): the height it
        // probably takes, a page break taking the rest of a page and the header
        qreal rest = natural;
        qreal partTop = top;
        height = 0;
        while (m_pageFlow.textHeight > header + FLOW_EPSILON &&
               rest > pageTopOf(partTop) + m_pageFlow.textHeight - partTop + FLOW_EPSILON) {
            rest -= pageTopOf(partTop) + m_pageFlow.textHeight - partTop;
            const qreal next = nextPageTop(partTop) + header;
            height += next - partTop;
            partTop = next;
        }
        height += rest;
    } else if (laidOut && m_blockFlowOffsets[i] != FLOW_UNPLACED) {
        paginateRow(i, last, top, header, true);  // its lines as they were
        m_blockFlowOffsets[i] = FLOW_UNPLACED;
    }
    m_blockFlowHeights[i] = height;

    // The pages a split row goes on to start with the header too
    if (header > 0 && split && tableFirst >= 0) {
        qreal page = nextPageTop(top);
        while (page < top + height - FLOW_EPSILON) {
            m_headerRepeats.push_back({page, header, tableFirst});
            page = nextPageTop(page);
        }
    }
    return {top, height};
}

qreal KalahariTextDocumentLayout::cellTextLeft(const QTextBlock& block) const {
    const QTextTable* table = tableOf(block);
    if (!table) {
        return 0.0;
    }
    const std::vector<qreal> edges = tableColumnEdges(*table);
    const int column = std::clamp(table->cellAt(block.position()).column(), 0,
                                  static_cast<int>(edges.size()) - 2);
    return edges[static_cast<size_t>(column)] + cellPadding();
}

std::vector<KalahariTextDocumentLayout::TableBlockLayout> KalahariTextDocumentLayout::layoutTable(
    const QTextTable& table, bool estimate, bool keepGlyphs, int firstRow, int lastRow,
    qreal* headerHeight) const {
    const QTextDocument* doc = document();
    const int rows = table.rows();
    if (lastRow < 0 || lastRow >= rows) {
        lastRow = rows - 1;
    }
    firstRow = std::clamp(firstRow, 0, std::max(0, lastRow));
    if (rows <= 0 || table.columns() <= 0) {
        return {};
    }
    const auto [firstBlock, lastBlock] = bandBlocks(table, firstRow, lastRow);
    const int firstNumber = firstBlock.blockNumber();
    const int lastNumber = lastBlock.blockNumber();
    std::vector<TableBlockLayout> blocks(static_cast<size_t>(std::max(0, lastNumber - firstNumber + 1)));
    for (TableBlockLayout& entry : blocks) {
        entry.flow.flags = FLOW_TABLE;
    }
    const std::vector<qreal> edges = tableColumnEdges(table);
    const qreal padding = cellPadding();
    const qreal pixels = packagePixels();
    const int columns = static_cast<int>(edges.size()) - 1;

    // Each cell's paragraphs one below another on the cell's width; a row as high as its
    // highest cell, or as its least height, a cell spanning rows makes the last of them
    // higher when they are too low
    std::vector<qreal> rowHeights(static_cast<size_t>(std::max(0, rows)), 0.0);
    std::vector<int> firstOfRow(rowHeights.size(), -1);  // index of the row's first block
    std::vector<int> lastOfRow(rowHeights.size(), -1);   // and of its last one
    std::vector<qreal> firstLines(rowHeights.size(), 0.0);  // the space its first lines need
    std::vector<TableRowProperties> rowProperties(rowHeights.size());
    struct Cell {
        int row = 0;
        int rows = 1;
        int firstIndex = 0;  // its blocks
        int lastIndex = -1;
        qreal height = 0;
        CellAlignment alignment = CellAlignment::Top;
    };
    std::vector<Cell> cells;
    for (int row = firstRow; row <= lastRow; ++row) {
        rowProperties[static_cast<size_t>(row)] = rowPropertiesOf(table, row);
        for (int column = 0; column < columns; ++column) {
            const QTextTableCell cell = table.cellAt(row, column);
            if (cell.row() != row || cell.column() != column) {
                continue;
            }
            const int right = std::min(column + cell.columnSpan(), columns);
            CellPlacement placement;
            placement.left = edges[static_cast<size_t>(column)] + padding;
            placement.top = padding;
            placement.width = std::max<qreal>(
                1.0, edges[static_cast<size_t>(right)] - edges[static_cast<size_t>(column)] - 2.0 * padding);
            // The cell's text starts and ends at its padding: without the space above its
            // first paragraph and below its last one
            QTextBlock block = doc->findBlock(cell.firstPosition());
            const QTextBlock end = doc->findBlock(cell.lastPosition()).next();
            const BlockMetrics& firstMetrics = blockMetrics(block);
            placement.top -= firstMetrics.spaceBefore;
            firstLines[static_cast<size_t>(row)] = std::max(
                firstLines[static_cast<size_t>(row)], 2.0 * padding + firstMetrics.estimatedLineHeight);
            Cell laid;
            laid.row = row;
            laid.rows = std::min(cell.rowSpan(), rows - row);
            laid.alignment = cellPropertiesOf(cell).alignment;
            laid.firstIndex = block.blockNumber() - firstNumber;
            qreal spaceAfter = 0;
            for (; block.isValid() && block != end; block = block.next()) {
                const int index = block.blockNumber() - firstNumber;
                if (index < 0 || static_cast<size_t>(index) >= blocks.size()) {
                    continue;
                }
                TableBlockLayout& entry = blocks[static_cast<size_t>(index)];
                const BlockMetrics& metrics = blockMetrics(block);
                entry.flow.cellTop = placement.top;
                qreal extent = block.length();
                const qreal height =
                    estimate ? estimatedHeight(extent, metrics, placement.width)
                             : layoutBlock(block, &extent, keepGlyphs, nullptr, &placement);
                entry.extent = extent;
                placement.top += height;
                spaceAfter = metrics.spaceAfter;
                if (firstOfRow[static_cast<size_t>(row)] < 0) {
                    firstOfRow[static_cast<size_t>(row)] = index;
                }
                lastOfRow[static_cast<size_t>(row)] = index;
                laid.lastIndex = index;
            }
            laid.height = placement.top - spaceAfter + padding;
            if (laid.rows <= 1) {
                rowHeights[static_cast<size_t>(row)] =
                    std::max(rowHeights[static_cast<size_t>(row)], laid.height);
            }
            cells.push_back(laid);
        }
        const qreal least = std::round(rowProperties[static_cast<size_t>(row)].minHeight * pixels);
        rowHeights[static_cast<size_t>(row)] = std::max(rowHeights[static_cast<size_t>(row)], least);
    }
    const auto spannedHeight = [&rowHeights](const Cell& cell) {
        qreal height = 0;
        for (int row = cell.row; row < cell.row + cell.rows; ++row) {
            height += rowHeights[static_cast<size_t>(row)];
        }
        return height;
    };
    for (const Cell& cell : cells) {
        if (cell.rows > 1) {
            const qreal height = spannedHeight(cell);
            if (height < cell.height) {
                rowHeights[static_cast<size_t>(cell.row + cell.rows - 1)] += cell.height - height;
            }
        }
    }

    // A cell's text in the middle or at the bottom of its rows: its lines lower
    for (const Cell& cell : cells) {
        if (cell.alignment == CellAlignment::Top || cell.lastIndex < cell.firstIndex) {
            continue;
        }
        const qreal room = spannedHeight(cell) - cell.height;
        const qreal shift = std::round(cell.alignment == CellAlignment::Middle ? room / 2.0 : room);
        if (shift <= 0) {
            continue;
        }
        QTextBlock block = doc->findBlockByNumber(firstNumber + cell.firstIndex);
        for (int index = cell.firstIndex; index <= cell.lastIndex && block.isValid();
             ++index, block = block.next()) {
            blocks[static_cast<size_t>(index)].flow.cellTop += shift;
            if (estimate) {
                continue;
            }
            if (QTextLayout* lines = block.layout()) {
                for (int i = 0; i < lines->lineCount(); ++i) {
                    QTextLine line = lines->lineAt(i);
                    line.setPosition(line.position() + QPointF(0.0, shift));
                }
            }
        }
    }

    // A row's height goes to its last block, and the rows after it without blocks of their
    // own (covered whole by cells spanning from above) with it
    int unit = -1;
    for (int row = firstRow; row <= lastRow; ++row) {
        if (firstOfRow[static_cast<size_t>(row)] >= 0) {
            unit = row;
            TableBlockLayout& first = blocks[static_cast<size_t>(firstOfRow[static_cast<size_t>(row)])];
            first.flow.flags |= FLOW_ROW_START;
            if (rowProperties[static_cast<size_t>(row)].allowSplit) {
                first.flow.flags |= FLOW_ROW_SPLIT;
            }
            first.flow.estimatedLineHeight = firstLines[static_cast<size_t>(row)];
        }
        if (unit >= 0) {
            blocks[static_cast<size_t>(lastOfRow[static_cast<size_t>(unit)])].height +=
                rowHeights[static_cast<size_t>(row)];
        }
    }

    // The header rows go with the first row after them, and are repeated above a row
    // starting a page the table goes on to. Rows below the head (tableBand()) have the
    // height of the header from the rows laid out before.
    const int headerRows = std::clamp(table.format().headerRowCount(), 0, rows);
    qreal header = 0;
    if (firstRow == 0) {
        for (int row = 0; row < headerRows && headerRows < rows; ++row) {
            header += rowHeights[static_cast<size_t>(row)];
        }
    } else {
        header = tableHeader(table);
    }
    if (headerHeight) {
        *headerHeight = header;
    }
    const qreal firstBody = headerRows < rows ? rowHeights[static_cast<size_t>(headerRows)] : 0.0;
    for (int row = firstRow; row <= lastRow; ++row) {
        const int index = firstOfRow[static_cast<size_t>(row)];
        if (index < 0) {
            continue;
        }
        FlowEntry& flow = blocks[static_cast<size_t>(index)].flow;
        if (row < headerRows) {
            // The space a row takes is what keeps a heading above the table with it
            qreal together = firstBody;
            for (int below = row; below < headerRows; ++below) {
                together += rowHeights[static_cast<size_t>(below)];
            }
            flow.lead = together;
        } else {
            flow.lead = blocks[static_cast<size_t>(lastOfRow[static_cast<size_t>(row)])].height;
            flow.header = header;
        }
    }
    return blocks;
}

qreal KalahariTextDocumentLayout::tableHeader(const QTextTable& table) const {
    // The first row below the head starts no later than the rows laid out
    const int headEnd = headEndOf(table);
    if (headEnd < 0 || headEnd + 1 >= table.rows()) {
        return 0.0;
    }
    const auto index = static_cast<size_t>(
        document()->findBlock(table.cellAt(headEnd + 1, 0).firstPosition()).blockNumber());
    return index < m_blockFlowEntries.size() && (m_blockFlowEntries[index].flags & FLOW_ROW_START) != 0
               ? m_blockFlowEntries[index].header
               : 0.0;
}

void KalahariTextDocumentLayout::setTableHeader(int first, int last, qreal header) {
    last = std::min(last, static_cast<int>(m_blockFlowEntries.size()) - 1);
    for (int i = std::max(first, 0); i <= last; ++i) {
        FlowEntry& entry = m_blockFlowEntries[static_cast<size_t>(i)];
        if ((entry.flags & FLOW_TABLE) == 0) {
            break;
        }
        if ((entry.flags & FLOW_ROW_START) != 0) {
            entry.header = header;
        }
    }
}

std::pair<int, int> KalahariTextDocumentLayout::layOutTableRows(const QTextTable& table, int firstRow,
                                                              int lastRow, bool* heightChanged) {
    const auto [bandFirst, bandLast] = tableBand(table, firstRow, lastRow);
    qreal header = 0;
    const std::vector<TableBlockLayout> entries =
        layoutTable(table, false, false, bandFirst, bandLast, &header);
    const int first = entries.empty() ? 0 : bandBlocks(table, bandFirst, bandLast).first.blockNumber();
    const int last = first + static_cast<int>(entries.size()) - 1;
    if (entries.empty() || first < 0 || static_cast<size_t>(last) >= m_blockHeights.size()) {
        return {first, first - 1};  // the cache is out of step with the document
    }
    for (size_t i = 0; i < entries.size(); ++i) {
        const size_t index = static_cast<size_t>(first) + i;
        if (entries[i].height != m_blockHeights[index] && heightChanged) {
            *heightChanged = true;
        }
        m_blockHeights[index] = entries[i].height;
        m_blockExtents[index] = entries[i].extent;
        m_blockFlowOffsets[index] = FLOW_UNPLACED;
        m_blockFlowEntries[index] = entries[i].flow;
        if (m_blockPending[index] != 0) {
            m_blockPending[index] = 0;
            --m_pendingCount;
        }
    }
    if (bandFirst == 0) {
        // The rows below have the height of the header too
        setTableHeader(last + 1, document()->findBlock(table.lastPosition()).blockNumber(), header);
    }
    return {first, last};
}

qreal KalahariTextDocumentLayout::documentWidth() const {
    return qMax<qreal>(0, document()->textWidth());
}

// =============================================================================
// QAbstractTextDocumentLayout Overrides
// =============================================================================

void KalahariTextDocumentLayout::draw(QPainter* painter, const PaintContext& context) {
    if (!painter) return;

    const QRectF& clip = context.clip;
    painter->save();
    if (!clip.isEmpty()) {
        painter->setClipRect(clip);
    }

    int number = clip.isEmpty() ? 0 : firstBlockOfRow(laidOutBlockAtY(clip.top()));
    for (QTextBlock block = document()->findBlockByNumber(number); block.isValid();
         block = block.next(), ++number) {
        ensureLaidOut(number, number);
        const qreal y = blockY(number);
        if (!clip.isEmpty() && y > clip.bottom()) {
            break;
        }
        if (QTextLayout* layout = block.layout()) {
            layout->draw(painter, QPointF(0, y));
            drawListMarker(painter, block, QPointF(0, y), context.palette.color(QPalette::Text));
        }
    }

    painter->restore();
}

int KalahariTextDocumentLayout::hitTest(const QPointF& point, Qt::HitTestAccuracy accuracy) const {
    const bool exact = (accuracy == Qt::ExactHit);
    const qreal y = point.y();
    if (exact && (y < 0 || y >= documentSize().height())) {
        return -1;
    }

    // A fuzzy hit above or below the document snaps to the first or last block; in a row of
    // a table, the paragraph of the cell under the point
    const int number = blockNumberAt(point);
    const QTextBlock block = document()->findBlockByNumber(number);
    QTextLayout* layout = block.isValid() ? block.layout() : nullptr;
    if (!layout || layout->lineCount() == 0) {
        return exact ? -1 : qMax(0, document()->characterCount() - 1);
    }

    // Line whose box covers the point (or the nearest line, for a fuzzy hit)
    const qreal localY = y - blockY(number);
    const QTextLine line = layout->lineAt(lineIndexAt(block, *layout, localY));
    if (exact) {
        // An exact hit is on the text of a line: not in the spacing after the paragraph,
        // nor before or after the text of the line
        const QRectF box = lineBoxOf(block, line);
        const qreal startX = line.cursorToX(line.textStart());
        const qreal endX = line.cursorToX(line.textStart() + line.textLength());
        if (localY < box.top() || localY >= box.bottom() || point.x() < qMin(startX, endX) ||
            point.x() > qMax(startX, endX)) {
            return -1;
        }
    }
    const int pos = line.xToCursor(point.x(), exact ? QTextLine::CursorOnCharacter
                                                    : QTextLine::CursorBetweenCharacters);
    return block.position() + pos;
}

int KalahariTextDocumentLayout::pageCount() const {
    if (!m_pageFlow.enabled) {
        return 1;  // Continuous layout, single page
    }
    updateBlockPositions();
    return static_cast<int>(std::lround((flowDocumentHeight() - m_pageFlow.textHeight) /
                                        m_pageFlow.pitch)) + 1;
}

QSizeF KalahariTextDocumentLayout::documentSize() const {
    updateBlockPositions();
    // Without the flow the endless text area is at least as high as a page's
    return QSizeF(documentWidth(), m_pageFlow.enabled
                                       ? flowDocumentHeight()
                                       : std::max(m_cachedDocumentHeight, m_pageFlow.textHeight));
}

QRectF KalahariTextDocumentLayout::frameBoundingRect(QTextFrame* frame) const {
    if (frame == document()->rootFrame()) {
        return QRectF(QPointF(0, 0), documentSize());
    }
    return QRectF();
}

QRectF KalahariTextDocumentLayout::blockBoundingRect(const QTextBlock& block) const {
    if (!block.isValid()) return QRectF();

    const int number = block.blockNumber();
    // A block the height cache does not know yet (queried while the document is in the
    // middle of a change) gets the default line height
    // A block of a table has the row's top and height: its lines are placed in the row
    const qreal height = static_cast<size_t>(number) < m_blockHeights.size()
        ? blockHeight(lastBlockOfRow(number))
        : QFontMetricsF(document()->defaultFont()).height();
    return QRectF(0, blockY(number), documentWidth(), height);
}

// =============================================================================
// Lists
// =============================================================================

const std::vector<QString>& KalahariTextDocumentLayout::listMarkers() const {
    if (m_listMarkersDirty) {
        m_listMarkers = editor::listMarkers(*document(), &m_hasLists);
        m_listMarkersDirty = false;
    }
    return m_listMarkers;
}

void KalahariTextDocumentLayout::renumberLists(int first, int last, int delta,
                                               const std::vector<QString>& old, bool oldKnown,
                                               bool hadLists) {
    // A document without lists stays without numbers, unless the change brought an item
    if (!hadLists) {
        bool itemChanged = false;
        const QTextBlock afterLast = document()->findBlockByNumber(last).next();
        for (QTextBlock block = document()->findBlockByNumber(first);
             block.isValid() && block != afterLast; block = block.next()) {
            if (isListItem(block.blockFormat())) {
                itemChanged = true;
                break;
            }
        }
        if (!itemChanged) {
            return;  // the numbers wait to be counted when an item asks for its own
        }
    }

    // The laid out items outside the change whose numbers changed: their first lines may
    // start elsewhere, and their numbers are drawn anew
    const std::vector<QString>& markers = listMarkers();
    const auto count = std::min(markers.size(), m_blockPending.size());
    int firstMoved = -1;
    QTextBlock block = document()->begin();
    for (size_t number = 0; number < count && block.isValid(); ++number, block = block.next()) {
        const int index = static_cast<int>(number);
        if (index >= first && index <= last) {
            continue;
        }
        const int oldNumber = index < first ? index : index - delta;
        const QString before = oldKnown && oldNumber >= 0 && oldNumber < static_cast<int>(old.size())
                                   ? old[static_cast<size_t>(oldNumber)]
                                   : QString();
        if (markers[number] == before || m_blockPending[number] != 0) {
            continue;
        }
        if (layOutBlock(index, block)) {
            m_positionsDirty = true;
            if (firstMoved < 0) {
                firstMoved = index;
            }
        }
        emit blocksLaidOut(index, 1);
        emit update(QRectF(0, blockY(index), UNBOUNDED_EXTENT, blockHeight(index)));
    }
    if (firstMoved >= 0) {
        emit blockGeometryChanged(firstMoved);
        notifyGeometryChanged(firstMoved, UNBOUNDED_EXTENT);
    }
}

qreal KalahariTextDocumentLayout::markerOverflow(const QTextBlock& block,
                                                 const BlockMetrics& metrics) const {
    const std::optional<ListMarker> marker = listMarker(block);
    if (!marker) {
        return 0.0;
    }
    const qreal gap = QFontMetricsF(marker->font).horizontalAdvance(QLatin1Char(' '));
    return std::max<qreal>(0.0, std::ceil(marker->x + marker->width + gap) - metrics.leftMargin);
}

std::optional<KalahariTextDocumentLayout::ListMarker> KalahariTextDocumentLayout::listMarker(
    const QTextBlock& block) const {
    const std::vector<QString>& markers = listMarkers();
    const auto number = static_cast<size_t>(block.blockNumber());
    if (!block.isValid() || number >= markers.size() || markers[number].isEmpty()) {
        return std::nullopt;
    }
    const BlockMetrics& metrics = blockMetrics(block);
    if (!metrics.listItem) {
        return std::nullopt;
    }

    // The font of the first character of the item (of the paragraph when it is empty), with
    // its style's font under it, without its lines and background
    QTextCharFormat format;
    format.setFont(document()->defaultFont());
    format.merge(metrics.charFormat);
    const QTextBlock::iterator first = block.begin();
    format.merge(first.atEnd() ? block.charFormat() : first.fragment().charFormat());
    ListMarker marker;
    marker.text = markers[number];
    marker.font = format.font();
    marker.font.setUnderline(false);
    marker.font.setStrikeOut(false);
    marker.font.setOverline(false);
    if (format.verticalAlignment() == QTextCharFormat::AlignSuperScript ||
        format.verticalAlignment() == QTextCharFormat::AlignSubScript) {
        marker.font = metrics.charFormat.propertyCount() > 0
                          ? [&] {
                                QTextCharFormat styled;
                                styled.setFont(document()->defaultFont());
                                styled.merge(metrics.charFormat);
                                return styled.font();
                            }()
                          : document()->defaultFont();
    }
    if (format.foreground().style() != Qt::NoBrush) {
        marker.color = format.foreground().color();
    }

    // As wide as its glyphs are with the font's design metrics, as the text's
    QTextLayout text(marker.text, marker.font);
    QTextOption option;
    option.setWrapMode(QTextOption::NoWrap);
    option.setUseDesignMetrics(true);
    text.setTextOption(option);
    text.beginLayout();
    QTextLine line = text.createLine();
    if (line.isValid()) {
        line.setLineWidth(UNWRAPPED_LINE_WIDTH);
    }
    text.endLayout();
    marker.width = line.isValid() ? line.naturalTextWidth() : 0.0;
    switch (metrics.markerAlignment) {
        case ListMarkerAlignment::Left:
            marker.x = metrics.markerIndent;
            break;
        case ListMarkerAlignment::Center:
            marker.x = metrics.markerIndent - marker.width / 2.0;
            break;
        case ListMarkerAlignment::Right:
            marker.x = metrics.markerIndent - marker.width;
            break;
    }
    marker.x = std::max<qreal>(0.0, marker.x);
    return marker;
}

void KalahariTextDocumentLayout::drawListMarker(QPainter* painter, const QTextBlock& block,
                                                const QPointF& origin, const QColor& textColor) const {
    if (painter == nullptr) {
        return;
    }
    const QTextLayout* layout = block.layout();
    if (layout == nullptr || layout->lineCount() == 0) {
        return;
    }
    const std::optional<ListMarker> marker = listMarker(block);
    if (!marker) {
        return;
    }

    // On the baseline of the first line
    QTextLayout text(marker->text, marker->font);
    QTextOption option;
    option.setWrapMode(QTextOption::NoWrap);
    option.setUseDesignMetrics(true);
    text.setTextOption(option);
    text.beginLayout();
    QTextLine line = text.createLine();
    if (!line.isValid()) {
        text.endLayout();
        return;
    }
    line.setLineWidth(UNWRAPPED_LINE_WIDTH);
    text.endLayout();
    const QTextLine firstLine = layout->lineAt(0);
    const qreal baseline = firstLine.y() + firstLine.ascent();
    const qreal left = tableOf(block) != nullptr ? cellTextLeft(block) : 0.0;
    painter->save();
    painter->setPen(marker->color.isValid() ? marker->color : textColor);
    text.draw(painter, origin + QPointF(left + marker->x, baseline - line.ascent()));
    painter->restore();
}

// =============================================================================
// Captions, cross-references and the list of figures
// =============================================================================

void KalahariTextDocumentLayout::setCaptionContext(const CaptionContext& context) {
    if (context == m_captionContext) {
        return;
    }
    m_captionContext = context;
    m_captionNumbersDirty = true;
    if (m_fieldChars.empty()) {
        return;  // nothing in the text shows the numbers: they are counted when asked for
    }
    const std::vector<std::pair<int, QString>> old = m_fieldSignatures;
    refreshFields(0, -1, 0, old);
}

const CaptionNumbers& KalahariTextDocumentLayout::captionNumbers() const {
    if (m_captionNumbersDirty) {
        countFields();
    }
    return m_captionNumbers;
}

bool KalahariTextDocumentLayout::hasFields() const {
    captionNumbers();
    return m_hasFields;
}

void KalahariTextDocumentLayout::countFields() const {
    m_captionNumbersDirty = false;
    m_fieldChars.clear();
    const QTextDocument& doc = *document();
    int number = 0;
    for (QTextBlock block = doc.begin(); block.isValid(); block = block.next(), ++number) {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid()) {
                continue;
            }
            const QTextCharFormat format = fragment.charFormat();
            if (!isNumberedFormat(format) && textObjectOf(format) != TextObject::FigureList) {
                continue;
            }
            // Objects of one format next to each other are one fragment
            const QString text = fragment.text();
            for (int i = 0; i < text.size(); ++i) {
                if (text.at(i) == QChar::ObjectReplacementCharacter) {
                    m_fieldChars.push_back({fragment.position() + i, number, format});
                }
            }
        }
    }
    m_hasFields = std::any_of(m_fieldChars.cbegin(), m_fieldChars.cend(),
                              [](const FieldChar& field) {
                                  return !isFootnoteFormat(field.format);
                              });
    m_captionNumbers = CaptionNumbers(doc, m_captionContext);
    m_footnoteNumbers = FootnoteNumbers(doc, m_footnoteContext);
}

std::vector<std::pair<int, QString>> KalahariTextDocumentLayout::fieldSignatures() const {
    const CaptionNumbers& numbers = captionNumbers();
    std::vector<std::pair<int, QString>> signatures;
    QString list;
    bool listKnown = false;
    for (const FieldChar& field : m_fieldChars) {
        QString shown;
        if (isFootnoteFormat(field.format)) {
            shown = m_footnoteNumbers.numberAt(field.position);
        } else if (textObjectOf(field.format) == TextObject::FigureList) {
            if (!listKnown) {
                list = figureListSignature(numbers.figureList());
                listKnown = true;
            }
            shown = list;
        } else {
            const FieldText text = numbers.fieldText(field.position, field.format);
            shown = text.text;
            if (text.broken) {
                shown += BROKEN_FIELD_MARK;  // drawn on another background
            }
        }
        if (signatures.empty() || signatures.back().first != field.block) {
            signatures.emplace_back(field.block, shown);
        } else {
            signatures.back().second += FIELD_SEPARATOR + shown;
        }
    }
    return signatures;
}

bool KalahariTextDocumentLayout::touchesFields(int from, int charsRemoved, int charsAdded,
                                               const QTextBlock& first,
                                               const QTextBlock& last) const {
    const auto removed = std::lower_bound(
        m_fieldChars.begin(), m_fieldChars.end(), from,
        [](const FieldChar& field, int position) { return field.position < position; });
    if (removed != m_fieldChars.end() && removed->position < from + charsRemoved) {
        return true;
    }
    // A field's text may follow its paragraph (a caption); a note's number changes only with
    // a note brought (an object of the new text) or one removed
    bool objectAdded = false;
    const QTextDocument* doc = document();
    for (int position = from; position < from + charsAdded && !objectAdded; ++position) {
        objectAdded = doc->characterAt(position) == QChar::ObjectReplacementCharacter;
    }
    const QTextBlock afterLast = last.next();
    for (QTextBlock block = first; block.isValid() && block != afterLast; block = block.next()) {
        if (hasFieldsOrList(block, objectAdded)) {
            return true;
        }
    }
    return false;
}

void KalahariTextDocumentLayout::shiftFields(int from, int charsRemoved, int charsAdded,
                                             int firstBlock, int oldLastBlock, int blockDelta) {
    // The changed blocks hold no field (the change touched none), at most notes' numbers,
    // which stay as they are: the fields and the notes after the change move with it
    const int end = from + charsRemoved;
    const int delta = charsAdded - charsRemoved;
    for (FieldChar& field : m_fieldChars) {
        if (field.position >= end) {
            field.position += delta;
            field.block += blockDelta;
        }
    }
    if (!m_captionNumbersDirty) {
        m_captionNumbers.shift(end, delta);
        m_footnoteNumbers.shift(end, delta);
    }
    std::vector<std::pair<int, QString>> shifted;
    shifted.reserve(m_fieldSignatures.size());
    for (auto& [number, shown] : m_fieldSignatures) {
        if (number < firstBlock || (blockDelta == 0 && number <= oldLastBlock)) {
            shifted.emplace_back(number, std::move(shown));  // its notes show the same numbers
        } else if (number > oldLastBlock) {
            shifted.emplace_back(number + blockDelta, std::move(shown));
        }
    }
    m_fieldSignatures = std::move(shifted);
}

void KalahariTextDocumentLayout::refreshFields(int first, int last, int delta,
                                               const std::vector<std::pair<int, QString>>& old) {
    // What the blocks outside the change showed, by their new numbers
    QHash<int, QString> before;
    for (const auto& [number, shown] : old) {
        if (number >= first && number <= last - delta) {
            continue;  // a changed block: laid out anew
        }
        before.insert(number < first ? number : number + delta, shown);
    }
    m_fieldSignatures = fieldSignatures();

    // The laid out ones that show something else: their fields are as wide as their new
    // text, a list of figures as high as its new rows
    int firstMoved = -1;
    for (const auto& [number, shown] : m_fieldSignatures) {
        if (number >= first && number <= last) {
            continue;
        }
        const auto was = before.constFind(number);
        const auto index = static_cast<size_t>(number);
        if ((was != before.cend() && *was == shown) || index >= m_blockPending.size() ||
            m_blockPending[index] != 0) {
            continue;
        }
        const QTextBlock block = document()->findBlockByNumber(number);
        if (!block.isValid()) {
            continue;
        }
        if (layOutBlock(number, block)) {
            m_positionsDirty = true;
            if (firstMoved < 0) {
                firstMoved = number;
            }
        }
        // Its notes show their new numbers under it too (a table's under its last block)
        if (hasNotes(block)) {
            const QTextTable* table = tableOf(block);
            const int roomBlock =
                table ? document()->findBlock(table->lastPosition()).blockNumber() : number;
            updateSpaceBelow(roomBlock, roomBlock);
        }
        emit blocksLaidOut(number, 1);
        emit update(QRectF(0, blockY(number), UNBOUNDED_EXTENT, blockHeight(number)));
    }
    // The endnotes under the last block, as they are now; with page flow the notes at the
    // bottom of the pages are placed again with the positions
    if (!m_footnoteNumbers.notes().empty() || !m_footnoteContext.otherEndnotes.empty() ||
        (!m_blockNotesBelow.empty() && m_blockNotesBelow.back() > 0.0)) {
        updateSpaceBelow(document()->blockCount() - 1);
        if (m_pageFlow.enabled) {
            m_positionsDirty = true;
            if (firstMoved < 0) {
                firstMoved = 0;
            }
        }
    }
    if (firstMoved >= 0) {
        emit blockGeometryChanged(firstMoved);
        notifyGeometryChanged(firstMoved, UNBOUNDED_EXTENT);
    }
}

QFont KalahariTextDocumentLayout::fieldFont(int position, const QTextFormat& format) const {
    QTextCharFormat merged;
    merged.setFont(document()->defaultFont());
    if (const QTextBlock block = document()->findBlock(position); block.isValid()) {
        merged.merge(blockMetrics(block).charFormat);
    }
    merged.merge(format.toCharFormat());
    return merged.font();
}

const KalahariTextDocumentLayout::FigureListPlacement&
KalahariTextDocumentLayout::figureListPlacement() const {
    std::vector<FigureListRow> rows = captionNumbers().figureList();
    const QString signature = figureListSignature(rows);
    const qreal documentWidth = document()->textWidth();
    const qreal width = documentWidth > 0 ? documentWidth : UNWRAPPED_LINE_WIDTH;
    const QFont font = document()->defaultFont();
    const qreal lineHeight = m_estimatedLineHeight;
    const qreal spacing = paragraphSpacing();
    if (m_figureList && m_figureList->signature == signature && m_figureList->width == width &&
        m_figureList->font == font && m_figureList->lineHeight == lineHeight &&
        m_figureList->spacing == spacing) {
        return *m_figureList;
    }

    auto placement = std::make_unique<FigureListPlacement>();
    placement->signature = signature;
    placement->width = width;
    placement->font = font;
    placement->lineHeight = lineHeight;
    placement->spacing = spacing;
    if (rows.empty()) {
        //: The list of figures of a book without captions (Insert > List of Figures). In
        //: Polish: Książka nie ma jeszcze podpisów z numerami
        rows.push_back({QString(), tr("The book has no numbered captions yet"), 0, 0, QString()});
        placement->noCaptions = true;
    }

    // Each row wrapped on the width in the document font, its lines as far apart as the lines
    // of Normal; the rows as far apart as its paragraphs
    qreal y = 0.0;
    for (FigureListRow& row : rows) {
        FigureListPlacement::Row placed;
        placed.text = std::make_unique<QTextLayout>(row.text, font);
        QTextOption option;
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        option.setUseDesignMetrics(true);
        placed.text->setTextOption(option);
        placed.text->setCacheEnabled(true);
        placed.text->beginLayout();
        qreal lines = 0.0;
        for (QTextLine line = placed.text->createLine(); line.isValid();
             line = placed.text->createLine()) {
            line.setLineWidth(width);
            line.setPosition(QPointF(
                0.0, lines + leadingAbove(std::max<qreal>(0.0, lineHeight - line.height()))));
            lines += lineHeight;
        }
        placed.text->endLayout();
        placed.height = std::max(lineHeight, lines);
        placed.row = std::move(row);
        if (!placement->rows.empty()) {
            y += spacing;
        }
        placed.top = y;
        y += placed.height;
        placement->rows.push_back(std::move(placed));
    }
    placement->height = std::max(lineHeight, y);
    m_figureList = std::move(placement);
    return *m_figureList;
}

std::vector<qreal> KalahariTextDocumentLayout::figureListTops(qreal start) const {
    const FigureListPlacement& placement = figureListPlacement();
    std::vector<qreal> tops;
    tops.reserve(placement.rows.size());
    if (!m_pageFlow.enabled || start < 0) {
        for (const FigureListPlacement::Row& row : placement.rows) {
            tops.push_back(row.top);
        }
        return tops;
    }
    // On the pages its rows go on as the lines of a paragraph: one that would cross the end
    // of a page's text starts the next page, without the spacing above it
    qreal y = start;
    for (size_t i = 0; i < placement.rows.size(); ++i) {
        const qreal height = placement.rows[i].height;
        const qreal top = flowLineTop(i == 0 ? y : y + placement.spacing, height);
        tops.push_back(top - start);
        y = top + height;
    }
    return tops;
}

qreal KalahariTextDocumentLayout::figureListStart(int position) const {
    // The top of the list's line in the document, as the pages placed it last
    const QTextBlock block = document()->findBlock(position);
    const QTextLayout* layout = block.layout();
    const auto number = static_cast<size_t>(block.blockNumber());
    if (!m_pageFlow.enabled || m_positionsDirty || layout == nullptr || layout->lineCount() == 0 ||
        number >= m_blockYPositions.size()) {
        return -1.0;
    }
    return m_blockYPositions[number] + layout->lineAt(0).y();
}

void KalahariTextDocumentLayout::drawFigureList(QPainter* painter, const QRectF& rect,
                                                int position) const {
    const FigureListPlacement& placement = figureListPlacement();
    const std::vector<qreal> tops = figureListTops(figureListStart(position));
    // In the color of the text (the painter's pen); the note of a book without captions
    // fainter, as the labels of the objects
    QColor color = painter->pen().color();
    if (placement.noCaptions) {
        color.setAlphaF(static_cast<float>(color.alphaF() * OBJECT_LINE_OPACITY));
    }
    const QRectF clip = painter->hasClipping() ? painter->clipBoundingRect() : QRectF();
    painter->save();
    painter->setPen(color);
    for (size_t i = 0; i < placement.rows.size() && i < tops.size(); ++i) {
        const FigureListPlacement::Row& row = placement.rows[i];
        const QRectF rowRect(rect.left(), rect.top() + tops[i], rect.width(), row.height);
        if (!clip.isNull() && !clip.intersects(rowRect)) {
            continue;
        }
        row.text->draw(painter, rowRect.topLeft());
    }
    painter->restore();
}

std::optional<QRectF> KalahariTextDocumentLayout::figureListRect(int position) const {
    const QTextBlock block = document()->findBlock(position);
    const QTextLayout* layout = block.layout();
    if (!block.isValid() || layout == nullptr || layout->lineCount() == 0) {
        return std::nullopt;
    }
    const int offset = position - block.position();
    bool isList = false;
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment fragment = it.fragment();
        if (fragment.isValid() && fragment.position() <= position &&
            position < fragment.position() + fragment.length()) {
            isList = textObjectOf(fragment.charFormat()) == TextObject::FigureList &&
                     block.text().at(offset) == QChar::ObjectReplacementCharacter;
            break;
        }
    }
    if (!isList) {
        return std::nullopt;
    }
    const QTextLine line = layout->lineForTextPosition(offset);
    if (!line.isValid()) {
        return std::nullopt;
    }
    // Qt draws an object from its line's baseline up (QTextLine::draw())
    const qreal left = line.cursorToX(offset);
    const qreal right = line.cursorToX(offset + 1);
    const qreal height = figureListPlacement().height;
    const qreal baseline = line.y() + line.ascent();
    return QRectF(std::min(left, right), baseline - height, std::abs(right - left), height);
}

std::optional<FigureListRow> KalahariTextDocumentLayout::figureListRowAt(int position,
                                                                       const QPointF& point) const {
    const std::optional<QRectF> rect = figureListRect(position);
    if (!rect || point.x() < rect->left() || point.x() >= rect->right() || point.y() < rect->top()) {
        return std::nullopt;  // on the pages the rows go on below its line
    }
    const FigureListPlacement& placement = figureListPlacement();
    if (placement.noCaptions) {
        return std::nullopt;
    }
    const qreal y = point.y() - rect->top();
    const std::vector<qreal> tops = figureListTops(figureListStart(position));
    for (size_t i = 0; i < placement.rows.size() && i < tops.size(); ++i) {
        if (y >= tops[i] && y < tops[i] + placement.rows[i].height) {
            return placement.rows[i].row;
        }
    }
    return std::nullopt;
}

// =============================================================================
// Objects of the text
// =============================================================================

void KalahariTextDocumentLayout::resizeInlineObject(QTextInlineObject item, int posInDocument,
                                                    const QTextFormat& format) {
    if (const std::optional<ImageProperties> image = imageOf(format)) {
        // On the baseline, as a character: all of it above
        m_hasImages = true;
        const QSizeF size = imageSize(document()->findBlock(posInDocument), *image);
        item.setWidth(size.width());
        item.setAscent(size.height());
        item.setDescent(0.0);
        return;
    }
    if (isFootnoteFormat(format)) {
        // As wide as its number in a smaller font, as high as the text around it
        qreal ascent = 0.0;
        qreal descent = 0.0;
        item.setWidth(noteNumberWidth(posInDocument, format, &ascent, &descent));
        item.setAscent(ascent);
        item.setDescent(descent);
        return;
    }
    if (isFieldFormat(format)) {
        // As wide and as high as the text it shows, in the font of the text around it
        const QFont font = fieldFont(posInDocument, format);
        QTextLayout text(captionNumbers().fieldText(posInDocument, format).text, font);
        layOutUnwrapped(text);
        const QFontMetricsF metrics(font);
        item.setWidth(text.lineCount() > 0 ? text.lineAt(0).naturalTextWidth() : 0.0);
        item.setAscent(metrics.ascent());
        item.setDescent(metrics.descent());
        return;
    }
    const TextObject object = textObjectOf(format);
    if (object == TextObject::None) {
        return;
    }
    if (object == TextObject::FigureList) {
        // As wide as the line, as high as its rows; all of it above the baseline
        const qreal width = document()->textWidth();
        item.setWidth(width > 0 ? width : UNWRAPPED_LINE_WIDTH);
        item.setAscent(figureListPlacement().height);
        item.setDescent(0.0);
        return;
    }
    // As wide as the line (objects have no margins), as high as a line box of Normal; the
    // baseline where a line of the document font has it in such a box
    const qreal width = document()->textWidth();
    const qreal height = objectHeight(object);
    const QFontMetricsF metrics(document()->defaultFont());
    const qreal ascent =
        std::clamp(std::round((height + metrics.ascent() - metrics.descent()) / 2.0), 0.0, height);
    item.setWidth(width > 0 ? width : UNWRAPPED_LINE_WIDTH);
    item.setAscent(ascent);
    item.setDescent(height - ascent);
}

QSizeF KalahariTextDocumentLayout::intrinsicSize(QTextDocument* /*doc*/, int posInDocument,
                                                 const QTextFormat& format) {
    if (const std::optional<ImageProperties> image = imageOf(format)) {
        return imageSize(document()->findBlock(posInDocument), *image);
    }
    if (isFootnoteFormat(format)) {
        qreal ascent = 0.0;
        qreal descent = 0.0;
        const qreal width = noteNumberWidth(posInDocument, format, &ascent, &descent);
        return QSizeF(width, ascent + descent);
    }
    if (isFieldFormat(format)) {
        const QFont font = fieldFont(posInDocument, format);
        QTextLayout text(captionNumbers().fieldText(posInDocument, format).text, font);
        layOutUnwrapped(text);
        return QSizeF(text.lineCount() > 0 ? text.lineAt(0).naturalTextWidth() : 0.0,
                      QFontMetricsF(font).height());
    }
    const TextObject object = textObjectOf(format);
    if (object == TextObject::None) {
        return QSizeF();
    }
    const qreal width = document()->textWidth();
    return QSizeF(width > 0 ? width : UNWRAPPED_LINE_WIDTH,
                  object == TextObject::FigureList ? figureListPlacement().height
                                                   : objectHeight(object));
}

void KalahariTextDocumentLayout::drawObject(QPainter* painter, const QRectF& rect,
                                            QTextDocument* /*doc*/, int posInDocument,
                                            const QTextFormat& format) {
    drawInlineObject(painter, rect, QTextInlineObject(), posInDocument, format);
}

void KalahariTextDocumentLayout::drawInlineObject(QPainter* painter, const QRectF& rect,
                                                  QTextInlineObject item, int posInDocument,
                                                  const QTextFormat& format) {
    if (painter == nullptr) {
        return;
    }
    if (const std::optional<ImageProperties> image = imageOf(format)) {
        drawImage(painter, rect, *image);
        return;
    }
    if (isFootnoteFormat(format)) {
        drawNoteNumber(painter, rect, item.isValid() ? item.ascent() : -1.0, posInDocument, format);
        return;
    }
    if (isFieldFormat(format)) {
        // The text it shows on the baseline of its line, in the color of the text (the
        // painter's pen); its background is the view's (the render pipeline's)
        const QFont font = fieldFont(posInDocument, format);
        QTextLayout text(captionNumbers().fieldText(posInDocument, format).text, font);
        layOutUnwrapped(text);
        if (text.lineCount() > 0) {
            const qreal ascent = item.isValid() ? item.ascent() : QFontMetricsF(font).ascent();
            text.draw(painter, QPointF(rect.left(), rect.top() + ascent - text.lineAt(0).ascent()));
        }
        return;
    }
    const TextObject object = textObjectOf(format);
    if (object == TextObject::None) {
        return;
    }
    if (object == TextObject::FigureList) {
        drawFigureList(painter, rect, posInDocument);
        return;
    }
    // In the color of the text (the painter's pen); lines as thin as the text's strokes
    const QColor textColor = painter->pen().color();
    const qreal thickness = std::max<qreal>(1.0, std::round(objectHeight(object) / OBJECT_LINE_THINNESS));
    const qreal middle = rect.top() + std::round(rect.height() / 2.0);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);
    switch (object) {
        case TextObject::HorizontalRule: {
            painter->setPen(QPen(textColor, thickness, Qt::SolidLine, Qt::FlatCap));
            painter->drawLine(QPointF(rect.left(), middle), QPointF(rect.right(), middle));
            break;
        }
        case TextObject::SceneBreak: {
            const QString text = sceneBreakText(m_sceneBreakLook);
            if (!text.isEmpty()) {
                painter->setFont(document()->defaultFont());
                painter->setPen(textColor);
                painter->drawText(rect, Qt::AlignCenter, text);
            } else if (m_sceneBreakLook == SceneBreakLook::Line) {
                const qreal length = std::round(rect.width() * SCENE_BREAK_LINE_SHARE);
                const qreal left = rect.left() + std::round((rect.width() - length) / 2.0);
                painter->setPen(QPen(textColor, thickness, Qt::SolidLine, Qt::FlatCap));
                painter->drawLine(QPointF(left, middle), QPointF(left + length, middle));
            }
            break;
        }
        case TextObject::PageBreak: {
            // In every view, with the formatting marks or without them: the pages start the
            // next one, the line says why the text ends there
            QColor markColor = textColor;
            markColor.setAlphaF(markColor.alphaF() * OBJECT_LINE_OPACITY);
            QFont labelFont = document()->defaultFont();
            if (labelFont.pointSizeF() > 0) {
                labelFont.setPointSizeF(labelFont.pointSizeF() * PAGE_BREAK_LABEL_SCALE);
            } else if (labelFont.pixelSize() > 0) {
                labelFont.setPixelSize(std::max(1, static_cast<int>(std::lround(
                                                       labelFont.pixelSize() * PAGE_BREAK_LABEL_SCALE))));
            }
            const QString label = tr("Page Break");
            const QFontMetricsF labelMetrics(labelFont);
            const qreal gap = std::round(labelMetrics.horizontalAdvance(label) +
                                         2.0 * labelMetrics.averageCharWidth());
            const qreal gapLeft = rect.left() + std::round((rect.width() - gap) / 2.0);
            QPen dashed(markColor, thickness, Qt::DashLine, Qt::FlatCap);
            painter->setPen(dashed);
            if (gapLeft > rect.left()) {
                painter->drawLine(QPointF(rect.left(), middle), QPointF(gapLeft, middle));
                painter->drawLine(QPointF(gapLeft + gap, middle), QPointF(rect.right(), middle));
            }
            painter->setFont(labelFont);
            painter->setPen(markColor);
            painter->drawText(rect, Qt::AlignCenter, label);
            break;
        }
        case TextObject::FigureList:  // drawn above
        case TextObject::None:
            break;
    }
    painter->restore();
}

// =============================================================================
// Images
// =============================================================================

qreal KalahariTextDocumentLayout::imageLineWidth(const QTextBlock& block) const {
    const BlockMetrics& metrics = blockMetrics(block);
    return std::max<qreal>(1.0, areaWidth(block) - metrics.leftMargin - metrics.rightMargin);
}

qreal KalahariTextDocumentLayout::widestImage(const QTextBlock& block) const {
    // Never wider than the line, the first one of the paragraph too (its indent, or the
    // number of a list item, as layoutBlock() places them)
    const BlockMetrics& metrics = blockMetrics(block);
    const qreal lineWidth = imageLineWidth(block);
    const Qt::Alignment own = ownAlignment(block.blockFormat());
    const Qt::Alignment alignment = own ? own : metrics.alignment;
    const bool leadingAligned = !(alignment & (Qt::AlignRight | Qt::AlignHCenter));
    const qreal ownIndent = metrics.listItem          ? markerOverflow(block, metrics)
                            : tableOf(block) != nullptr ? 0.0
                                                        : metrics.firstLineIndent;
    const qreal firstIndent =
        leadingAligned ? std::clamp(ownIndent, -metrics.leftMargin, lineWidth / 2.0) : 0.0;
    return std::max<qreal>(1.0, lineWidth - std::max<qreal>(0.0, firstIndent));
}

QMarginsF KalahariTextDocumentLayout::imageFrameMargins(const ImageProperties& image) const {
    if (!image.hasFrame()) {
        return QMarginsF();
    }
    const qreal pixels = packagePixels();
    const qreal line = image.borderLine == ImageBorderLine::None ? 0.0
                       : image.borderLine == ImageBorderLine::Double
                           ? image.borderWidth * DOUBLE_BORDER_LINES
                           : image.borderWidth;
    const qreal side = std::round((line + image.borderPadding) * pixels);
    const qreal shadow = image.shadow ? std::round(IMAGE_SHADOW_POINTS * pixels) : 0.0;
    return QMarginsF(side, side, side + shadow, side + shadow);
}

ImageProperties KalahariTextDocumentLayout::imageOfWidth(const QTextBlock& block,
                                                         const ImageProperties& image,
                                                         qreal width) const {
    // The width of the image's box: its picture is narrower by the frame
    const QMarginsF frame = imageFrameMargins(image);
    const qreal picture = std::max<qreal>(1.0, width - frame.left() - frame.right());
    ImageProperties resized = image;
    if (image.unit == ImageWidthUnit::Centimeters) {
        resized.width =
            std::round(picture / (POINTS_PER_CENTIMETER * packagePixels()) * 100.0) / 100.0;
    } else {
        resized.unit = ImageWidthUnit::Percent;
        resized.width = std::max(MIN_IMAGE_PERCENT,
                                 std::round(picture / imageLineWidth(block) * 100.0));
    }
    // An own height changes with the width: the image keeps its shape
    if (image.height > 0.0) {
        const QSizeF shown = imageSize(block, image);
        const qreal before = shown.width() - frame.left() - frame.right();
        if (before >= 1.0) {
            resized.height = std::round(image.height * picture / before * 100.0) / 100.0;
        }
    }
    return normalizedImage(resized);
}

QSizeF KalahariTextDocumentLayout::imageSize(const QTextBlock& block,
                                             const ImageProperties& image) const {
    const BlockMetrics& metrics = blockMetrics(block);
    const qreal lineWidth = imageLineWidth(block);
    const qreal widest = widestImage(block);

    // The file's pixels at 96 an inch (cropped and turned), centimeters shown as the style's
    // points are
    const qreal pixels = packagePixels();
    const std::optional<QSize> natural =
        m_imageStore ? m_imageStore->shownPixelSize(image) : std::nullopt;
    const QMarginsF frame = imageFrameMargins(image);
    const qreal frameWidth = frame.left() + frame.right();
    const qreal frameHeight = frame.top() + frame.bottom();
    qreal width = 0.0;
    switch (image.unit) {
        case ImageWidthUnit::Natural:
            width = natural ? natural->width() * POINTS_PER_INCH / IMAGE_PIXELS_PER_INCH * pixels
                            : MISSING_IMAGE_CENTIMETERS * POINTS_PER_CENTIMETER * pixels;
            break;
        case ImageWidthUnit::Percent:
            width = lineWidth * image.width / 100.0;
            break;
        case ImageWidthUnit::Centimeters:
            width = image.width * POINTS_PER_CENTIMETER * pixels;
            break;
    }
    width = std::max<qreal>(1.0, width);
    qreal height = 0.0;
    if (image.height > 0.0) {
        height = image.height * POINTS_PER_CENTIMETER * pixels;
    } else if (natural) {
        height = width * natural->height() / natural->width();
    } else {
        const qreal line = QFontMetricsF(document()->defaultFont()).height();
        height = std::max(line * MISSING_IMAGE_LINES, width * MISSING_IMAGE_RATIO);
    }
    // The box with its frame never wider than the line: the picture smaller, in its shape
    const qreal widestPicture = std::max<qreal>(1.0, widest - frameWidth);
    if (width > widestPicture) {
        height *= widestPicture / width;
        width = widestPicture;
    }

    // Never higher than a page's text, with the rest of its line's box and the paragraph's
    // spacing: an image higher than a page could never be placed on one
    if (m_pageFlow.textHeight > 0) {
        const qreal rest =
            std::max<qreal>(0.0, boxHeightOf(metrics.emptyLineHeight, metrics) - metrics.emptyAscent);
        const qreal tallest = m_pageFlow.textHeight - rest - metrics.spaceBefore -
                              metrics.spaceAfter - frameHeight;
        if (tallest >= 1.0 && height > tallest) {
            width *= tallest / height;
            height = tallest;
        }
    }
    return QSizeF(std::max(1.0, std::round(width)) + frameWidth,
                  std::max(1.0, std::round(height)) + frameHeight);
}

bool KalahariTextDocumentLayout::isImageMissing(const ImageProperties& image) const {
    return !m_imageStore || !m_imageStore->pixelSize(image.source);
}

std::optional<QRectF> KalahariTextDocumentLayout::imageRect(int position) const {
    const std::optional<ImageProperties> image = editor::imageAt(*document(), position);
    if (!image) {
        return std::nullopt;
    }
    const QTextBlock block = document()->findBlock(position);
    const QTextLayout* layout = block.layout();
    if (layout == nullptr || layout->lineCount() == 0) {
        return std::nullopt;
    }
    const int offset = position - block.position();
    const QTextLine line = layout->lineForTextPosition(offset);
    if (!line.isValid()) {
        return std::nullopt;
    }
    // Qt draws an object from its line's baseline up (QTextLine::draw())
    const qreal left = line.cursorToX(offset);
    const qreal right = line.cursorToX(offset + 1);
    const qreal height = imageSize(block, *image).height();
    const qreal baseline = line.y() + line.ascent();
    return QRectF(std::min(left, right), baseline - height, std::abs(right - left), height);
}

void KalahariTextDocumentLayout::drawImageFrame(QPainter* painter, const QRectF& box,
                                                const ImageProperties& image) const {
    const qreal pixels = packagePixels();
    const qreal shadow = image.shadow ? std::round(IMAGE_SHADOW_POINTS * pixels) : 0.0;
    // The frame's outer edge: the box less the room of the shadow
    const QRectF outer = box.adjusted(0.0, 0.0, -shadow, -shadow);
    const QColor color = image.borderColor.isValid() ? image.borderColor : painter->pen().color();
    painter->save();
    if (image.shadow) {
        QColor shade = color;
        shade.setAlphaF(shade.alphaF() * IMAGE_SHADOW_OPACITY);
        painter->fillRect(outer.translated(shadow, shadow), shade);
        // The paper under the picture, so the shadow shows only beside it (a transparent
        // picture, its padding)
        painter->fillRect(outer, painter->background());
    }
    if (image.borderLine != ImageBorderLine::None) {
        const qreal width = std::max<qreal>(1.0, image.borderWidth * pixels);
        Qt::PenStyle style = Qt::SolidLine;
        if (image.borderLine == ImageBorderLine::Dashed) {
            style = Qt::DashLine;
        } else if (image.borderLine == ImageBorderLine::Dotted) {
            style = Qt::DotLine;
        }
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(color, width, style, Qt::FlatCap, Qt::MiterJoin));
        painter->drawRect(outer.adjusted(width / 2.0, width / 2.0, -width / 2.0, -width / 2.0));
        if (image.borderLine == ImageBorderLine::Double) {
            const qreal inner = width * (DOUBLE_BORDER_LINES - 1.0);
            painter->drawRect(outer.adjusted(inner + width / 2.0, inner + width / 2.0,
                                             -inner - width / 2.0, -inner - width / 2.0));
        }
    }
    painter->restore();
}

void KalahariTextDocumentLayout::drawImage(QPainter* painter, const QRectF& box,
                                           const ImageProperties& image) const {
    if (box.isEmpty()) {
        return;
    }
    // The picture inside its frame: the border, the room around the picture, the shadow
    const QRectF rect = box.marginsRemoved(imageFrameMargins(image));
    if (image.hasFrame()) {
        drawImageFrame(painter, box, image);
    }
    if (rect.isEmpty()) {
        return;
    }
    if (m_imageStore) {
        // The picture in the pixels it takes on the screen: sharp at every zoom
        const QRectF device = painter->deviceTransform().mapRect(rect);
        const QSize pixels(std::max(1, qRound(device.width())), std::max(1, qRound(device.height())));
        const QImage picture = m_imageStore->shownImage(image, pixels);
        if (!picture.isNull()) {
            painter->save();
            painter->setRenderHint(QPainter::SmoothPixmapTransform);
            painter->drawImage(rect, picture);
            painter->restore();
            return;
        }
    }

    // A missing file: a dashed frame with the picture of a broken image, in the frame's color
    // (of the settings; else a shade of the text's), the file's name and what is wrong, in
    // the color of the text
    const QColor textColor = painter->pen().color();
    QColor lineColor = textColor;
    lineColor.setAlphaF(lineColor.alphaF() * OBJECT_LINE_OPACITY);
    if (m_missingImageColor.isValid()) {
        lineColor = m_missingImageColor;
    }
    QColor fillColor = lineColor;
    fillColor.setAlphaF(static_cast<float>(MISSING_IMAGE_FILL_OPACITY));
    const qreal thickness = std::max<qreal>(1.0, std::round(objectHeight(TextObject::HorizontalRule) /
                                                            OBJECT_LINE_THINNESS));
    painter->save();
    painter->setClipRect(rect, Qt::IntersectClip);
    painter->setRenderHint(QPainter::Antialiasing, false);
    const QRectF frame = rect.adjusted(thickness / 2.0, thickness / 2.0, -thickness / 2.0,
                                       -thickness / 2.0);
    painter->fillRect(frame, fillColor);
    painter->setPen(QPen(lineColor, thickness, Qt::DashLine, Qt::FlatCap));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(frame);

    QFont labelFont = document()->defaultFont();
    if (labelFont.pointSizeF() > 0) {
        labelFont.setPointSizeF(labelFont.pointSizeF() * MISSING_IMAGE_LABEL_SCALE);
    } else if (labelFont.pixelSize() > 0) {
        labelFont.setPixelSize(std::max(1, static_cast<int>(std::lround(
                                               labelFont.pixelSize() * MISSING_IMAGE_LABEL_SCALE))));
    }
    QFont nameFont = labelFont;
    nameFont.setWeight(QFont::DemiBold);
    const QFontMetricsF labelMetrics(labelFont);
    const QFontMetricsF nameMetrics(nameFont);
    const qreal gap = std::round(labelMetrics.averageCharWidth());
    const qreal glyph = std::round(labelMetrics.height() * 1.6);
    const qreal textWidth = std::max<qreal>(0.0, rect.width() - glyph - 4.0 * gap);
    const QString name = nameMetrics.elidedText(QFileInfo(image.source).fileName(),
                                                Qt::ElideMiddle, textWidth);
    //: The frame of an image whose file is not there, under the file's name. In Polish: Brak
    //: pliku obrazu
    const QString label = labelMetrics.elidedText(tr("Image file missing"), Qt::ElideRight, textWidth);
    const qreal labelWidth =
        std::max(nameMetrics.horizontalAdvance(name), labelMetrics.horizontalAdvance(label));
    const qreal contentWidth = glyph + gap + labelWidth;
    const qreal left = rect.left() + std::max(gap, std::round((rect.width() - contentWidth) / 2.0));
    const qreal middle = rect.top() + rect.height() / 2.0;

    QSvgRenderer broken(QString::fromLatin1(BROKEN_IMAGE_SVG).arg(lineColor.name()).toUtf8());
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->save();
    painter->setOpacity(painter->opacity() * lineColor.alphaF());
    broken.render(painter, QRectF(left, std::round(middle - glyph / 2.0), glyph, glyph));
    painter->restore();
    painter->setPen(textColor);
    const qreal textLeft = left + glyph + gap;
    const qreal textTop = middle - (nameMetrics.height() + labelMetrics.height()) / 2.0;
    painter->setFont(nameFont);
    painter->drawText(QPointF(textLeft, textTop + nameMetrics.ascent()), name);
    painter->setFont(labelFont);
    painter->drawText(QPointF(textLeft, textTop + nameMetrics.height() + labelMetrics.ascent()), label);
    painter->restore();
}

}  // namespace kalahari::editor
