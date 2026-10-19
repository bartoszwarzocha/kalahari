/// @file kalahari_text_document_layout.cpp
/// @brief Custom QAbstractTextDocumentLayout implementation (OpenSpec #00043)

#include <kalahari/editor/kalahari_text_document_layout.h>
#include <QElapsedTimer>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QPainter>
#include <QPen>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextLayout>
#include <QTextLine>
#include <QTextOption>
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace kalahari::editor {

namespace {

/// Line width used when the document has no text width (no wrapping)
constexpr qreal UNWRAPPED_LINE_WIDTH = 10000.0;

/// Extent Qt itself uses for an "everything" update rect
constexpr qreal UNBOUNDED_EXTENT = 1000000000.0;

/// Accepted range of the line spacing multiplier
constexpr qreal MIN_LINE_SPACING = 0.5;
constexpr qreal MAX_LINE_SPACING = 4.0;

/// An edit touching at most this many blocks lays them out at once. A larger one (a
/// load, a long paste or its undo) leaves them waiting for layout.
constexpr int IMMEDIATE_LAYOUT_LIMIT = 32;

/// Time one step of the background pass may take, so input stays responsive
constexpr qint64 BACKGROUND_STEP_MS = 4;

/// Share of the line width that wrapped text fills on average (estimates)
constexpr qreal AVERAGE_LINE_FILL = 0.95;

/// Page flow offset of a block whose lines follow its own top (not placed on a page)
constexpr qreal FLOW_UNPLACED = -1.0;

/// Tolerance of page flow comparisons (its lengths are whole pixels)
constexpr qreal FLOW_EPSILON = 0.01;

/// Page flow flags of a block: kept on the page of the next block; a page break
constexpr char FLOW_KEEP_WITH_NEXT = 1;
constexpr char FLOW_PAGE_BREAK = 2;

constexpr qreal POINTS_PER_INCH = 72.0;

/// Font size of Normal when the package gives none (the size StyleResolver assumes)
constexpr qreal DEFAULT_NORMAL_POINT_SIZE = 12.0;

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

/// The alignment a style's property names; none for another name
Qt::Alignment alignmentNamed(const QString& name) {
    if (name == QLatin1String("left")) {
        return Qt::AlignLeft;
    }
    if (name == QLatin1String("center")) {
        return Qt::AlignHCenter;
    }
    if (name == QLatin1String("right")) {
        return Qt::AlignRight;
    }
    if (name == QLatin1String("justify")) {
        return Qt::AlignJustify;
    }
    return {};
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

}  // anonymous namespace

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
    // Runs whenever the event loop has nothing else to do, while blocks wait for layout
    m_backgroundTimer->setInterval(0);
    connect(m_backgroundTimer, &QTimer::timeout, this, &KalahariTextDocumentLayout::layoutStep);
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
    m_pageFlow = normalized;
    if (placementChanged) {
        // The line breaks stay (design metrics in every view), but the lines of each block
        // are placed anew: on the pages, or one under another
        invalidateAll();
    } else {
        // Only the height the endless text area takes at least: the lines stay
        reportDocumentSize();
    }
}

void KalahariTextDocumentLayout::setTypography(const LayoutTypography& typography) {
    LayoutTypography normalized = typography;
    normalized.lineSpacing = std::clamp(typography.lineSpacing, MIN_LINE_SPACING, MAX_LINE_SPACING);
    normalized.paragraphSpacing = std::max<qreal>(0, typography.paragraphSpacing);
    normalized.firstLineIndent = std::max<qreal>(0, typography.firstLineIndent);
    normalized.referencePointSize = std::max<qreal>(0, typography.referencePointSize);
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
    const ParagraphStyle& style = m_styles.shownStyle(paragraphStyleOf(block.blockFormat()));
    auto it = m_styleMetrics.find(style.id);
    if (it == m_styleMetrics.end()) {
        it = m_styleMetrics.emplace(style.id, styleMetrics(style)).first;
    }
    return it->second;
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
    const qreal pixels = ratio * QFontMetricsF(font).fontDpi() / POINTS_PER_INCH;
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
    if (const Qt::Alignment alignment = alignmentNamed(own.value(QStringLiteral("alignment")).toString());
        alignment) {
        metrics.alignment = alignment;
    }
    metrics.keepWithNext = flagOf(own, "keepWithNext", false);

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

void KalahariTextDocumentLayout::setSceneBreakLook(SceneBreakLook look) {
    if (look == m_sceneBreakLook) {
        return;
    }
    m_sceneBreakLook = look;
    emit update(QRectF(0, 0, UNBOUNDED_EXTENT, UNBOUNDED_EXTENT));
}

void KalahariTextDocumentLayout::setFormattingMarksShown(bool shown) {
    if (shown == m_formattingMarksShown) {
        return;
    }
    m_formattingMarksShown = shown;
    emit update(QRectF(0, 0, UNBOUNDED_EXTENT, UNBOUNDED_EXTENT));
}

void KalahariTextDocumentLayout::applyStyleFormats(const QTextBlock& block) const {
    QTextLayout* layout = block.layout();
    if (layout == nullptr) {
        return;
    }

    // The style's font goes over each fragment, but not over what the fragment sets itself
    // (its own size, weight, family...): the document's formats stay as they are
    QList<QTextLayout::FormatRange> ranges;
    const QTextCharFormat& styleFormat = blockMetrics(block).charFormat;
    if (styleFormat.propertyCount() > 0) {
        const QMap<int, QVariant> styleProperties = styleFormat.properties();
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid()) {
                continue;
            }
            const QTextCharFormat own = fragment.charFormat();
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
            if (format.propertyCount() > 0) {
                QTextLayout::FormatRange range;
                range.start = fragment.position() - block.position();
                range.length = fragment.length();
                range.format = format;
                ranges.append(range);
            }
        }
    }
    // Setting formats announces a change of the block: only when they change
    if (ranges != layout->formats()) {
        layout->setFormats(ranges);
    }
}

void KalahariTextDocumentLayout::storeFlowEntries(int number, const QTextBlock& block, qreal lead) {
    const auto index = static_cast<size_t>(number);
    const BlockMetrics& metrics = blockMetrics(block);
    char flags = 0;
    if (metrics.keepWithNext) {
        flags |= FLOW_KEEP_WITH_NEXT;
    }
    if (metrics.pageBreak) {
        flags |= FLOW_PAGE_BREAK;
    }
    m_blockFlowFlags[index] = flags;
    m_blockLeads[index] = lead;
    m_blockSpacesAfter[index] = metrics.spaceAfter;
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
        heightChanged |= layOutBlock(number, block);
        if (!runs.empty() && runs.back().first + runs.back().second == number) {
            ++runs.back().second;
        } else {
            runs.emplace_back(number, 1);
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
    m_blockFlowFlags.assign(count, 0);
    m_blockLeads.assign(count, 0.0);
    m_blockSpacesAfter.assign(count, 0.0);

    QTextBlock block = doc->begin();
    for (size_t i = 0; i < count && block.isValid(); ++i, block = block.next()) {
        const BlockMetrics& metrics = blockMetrics(block);
        m_blockHeights[i] = estimatedHeight(m_blockExtents[i], metrics);
        storeFlowEntries(static_cast<int>(i), block,
                         metrics.spaceBefore + metrics.estimatedLineHeight);
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

qreal KalahariTextDocumentLayout::estimatedHeight(qreal extent, const BlockMetrics& metrics) const {
    const qreal width = document()->textWidth();
    const qreal lineWidth =
        std::max<qreal>(1.0, (width > 0 ? width : UNWRAPPED_LINE_WIDTH) - metrics.leftMargin -
                                 metrics.rightMargin) *
        AVERAGE_LINE_FILL;
    const qreal lines = std::max<qreal>(1.0, std::ceil(extent * m_averageCharWidth / lineWidth));
    return lines * metrics.estimatedLineHeight + metrics.spaceBefore + metrics.spaceAfter;
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
    Q_UNUSED(charsRemoved);
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
    const int oldLast =
        lastNumber - (doc->blockCount() - static_cast<int>(m_blockHeights.size()));
    replaceRange(first.blockNumber(), lastNumber, oldLast);
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
    std::vector<char> flags;
    std::vector<qreal> leads;
    std::vector<qreal> spacesAfter;
    heights.reserve(static_cast<size_t>(rangeSize));
    extents.reserve(static_cast<size_t>(rangeSize));
    QTextBlock block = doc->findBlockByNumber(first);
    for (int number = first; number <= last && block.isValid(); ++number) {
        const BlockMetrics& metrics = blockMetrics(block);
        qreal extent = block.length();
        qreal lead = metrics.spaceBefore + metrics.estimatedLineHeight;
        heights.push_back(layOutNow ? layoutBlock(block, &extent, true, &lead)
                                    : estimatedHeight(extent, metrics));
        extents.push_back(extent);
        flags.push_back(static_cast<char>((metrics.keepWithNext ? FLOW_KEEP_WITH_NEXT : 0) |
                                          (metrics.pageBreak ? FLOW_PAGE_BREAK : 0)));
        leads.push_back(lead);
        spacesAfter.push_back(metrics.spaceAfter);
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
        geometryChanged = !std::equal(heights.begin(), heights.end(), m_blockHeights.begin() + begin);
        std::copy(heights.begin(), heights.end(), m_blockHeights.begin() + begin);
        std::copy(extents.begin(), extents.end(), m_blockExtents.begin() + begin);
        std::copy(pending.begin(), pending.end(), m_blockPending.begin() + begin);
        std::copy(unplaced.begin(), unplaced.end(), m_blockFlowOffsets.begin() + begin);
        std::copy(noFlowHeights.begin(), noFlowHeights.end(), m_blockFlowHeights.begin() + begin);
        std::copy(flags.begin(), flags.end(), m_blockFlowFlags.begin() + begin);
        std::copy(leads.begin(), leads.end(), m_blockLeads.begin() + begin);
        std::copy(spacesAfter.begin(), spacesAfter.end(), m_blockSpacesAfter.begin() + begin);
    } else {
        const auto replace = [begin, oldEnd](auto& cache, const auto& entries) {
            cache.erase(cache.begin() + begin, cache.begin() + oldEnd);
            cache.insert(cache.begin() + begin, entries.begin(), entries.end());
        };
        replace(m_blockHeights, heights);
        replace(m_blockExtents, extents);
        replace(m_blockPending, pending);
        replace(m_blockFlowOffsets, unplaced);
        replace(m_blockFlowHeights, noFlowHeights);
        replace(m_blockFlowFlags, flags);
        replace(m_blockLeads, leads);
        replace(m_blockSpacesAfter, spacesAfter);
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
    const auto index = static_cast<size_t>(number);
    qreal extent = block.length();
    qreal lead = 0.0;
    const qreal height = layoutBlock(block, &extent, false, &lead);
    const bool changed = height != m_blockHeights[index];
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

qreal KalahariTextDocumentLayout::lineBoxHeight(const QTextLine& line, const BlockMetrics& metrics,
                                                bool empty) {
    const qreal height = empty ? std::max(line.height(), metrics.emptyLineHeight) : line.height();
    return height + extraLeading(height, metrics.lineSpacing);
}

qreal KalahariTextDocumentLayout::lineTopInBox(const QTextLine& line, const BlockMetrics& metrics,
                                               bool empty) {
    const qreal height = empty ? std::max(line.height(), metrics.emptyLineHeight) : line.height();
    // The line of an empty paragraph sits on the baseline of a line in the style's font
    const qreal baseline = empty ? std::max<qreal>(0.0, std::round(metrics.emptyAscent - line.ascent()))
                                 : 0.0;
    return leadingAbove(extraLeading(height, metrics.lineSpacing)) + baseline;
}

qreal KalahariTextDocumentLayout::layoutBlock(const QTextBlock& block, qreal* extent,
                                              bool keepGlyphs, qreal* lead) const {
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
    layout->setTextOption(textOption);

    // Blocks laid out on demand keep only the line breaks: their glyphs are shaped again
    // when a line is drawn (only the visible ones are). Cached glyphs of a whole chapter
    // took most of the editor's memory. A block laid out after an edit keeps its glyphs:
    // the cursor works there, and each cursor position query would shape the whole
    // paragraph again. Justified lines come out the same either way (tested).
    layout->setCacheEnabled(keepGlyphs);

    // The lines lie between the margins of the style
    const qreal width = document()->textWidth();
    const qreal lineWidth = std::max<qreal>(
        1.0, (width > 0 ? width : UNWRAPPED_LINE_WIDTH) - metrics.leftMargin - metrics.rightMargin);

    // The first-line indent belongs to paragraphs whose lines start at their leading
    // edge (left-aligned and justified ones), never more than half of the line (a negative
    // one, a hanging indent, never out of the left margin). Right-to-left lines keep x at
    // the margin: the shorter line already leaves the indent on their leading (right) side.
    const bool leadingAligned = !(alignment & (Qt::AlignRight | Qt::AlignHCenter));
    const qreal firstIndent =
        leadingAligned ? std::clamp(metrics.firstLineIndent, -metrics.leftMargin, lineWidth / 2.0)
                       : 0.0;
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
        line.setPosition(QPointF(metrics.leftMargin + (rightToLeft ? 0.0 : lineIndent),
                                 y + lineTopInBox(line, metrics, empty)));
        const qreal box = lineBoxHeight(line, metrics, empty);
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
    return QRectF(line.x(), line.y() - lineTopInBox(line, metrics, empty), line.width(),
                  lineBoxHeight(line, metrics, empty));
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
            y += m_blockHeights[i];
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
    qreal lastSpacing = 0;
    QTextBlock block = document()->begin();
    for (size_t i = 0; i < count; ++i, block = block.next()) {
        if ((m_blockFlowFlags[i] & FLOW_KEEP_WITH_NEXT) != 0) {
            y = keptBlockTop(i, y);
        }
        m_blockYPositions[i] = y;
        const qreal offset = std::fmod(y, m_pageFlow.pitch);
        qreal height = 0;
        if (m_blockPending[i] != 0) {
            height = (m_blockFlowFlags[i] & FLOW_PAGE_BREAK) != 0
                         ? nextPageTop(y) - y
                         : flowEstimate(m_blockHeights[i], y,
                                        inStep && block.isValid() ? blockMetrics(block) : BlockMetrics());
        } else if (std::abs(m_blockFlowOffsets[i] - offset) < FLOW_EPSILON) {
            height = m_blockFlowHeights[i];
        } else if (!inStep || !block.isValid() || !block.layout()) {
            height = m_blockFlowOffsets[i] == FLOW_UNPLACED ? m_blockHeights[i]
                                                             : m_blockFlowHeights[i];
        } else {
            height = paginateBlock(block.layout(), y, blockMetrics(block));
            m_blockFlowOffsets[i] = offset;
            m_blockFlowHeights[i] = height;
        }
        y += height;
        // A page break ending the text leaves the next page in the document, empty
        lastSpacing = (m_blockFlowFlags[i] & FLOW_PAGE_BREAK) != 0 ? -2.0 * FLOW_EPSILON
                                                                   : m_blockSpacesAfter[i];
    }
    m_cachedDocumentHeight = y;
    m_contentBottom = y - lastSpacing;
    m_positionsDirty = !inStep;
}

qreal KalahariTextDocumentLayout::flowLineTop(qreal top, qreal height) const {
    const qreal pitch = m_pageFlow.pitch;
    const qreal areaHeight = m_pageFlow.textHeight;
    const qreal page = std::floor((top + FLOW_EPSILON) / pitch);
    const qreal offset = top - page * pitch;
    const bool betweenAreas = offset >= areaHeight - FLOW_EPSILON;
    // A line at the top of its area stays even when it is taller than the area: moving
    // it on would not make it fit
    const bool crossesEnd = offset > FLOW_EPSILON && offset + height > areaHeight + FLOW_EPSILON;
    return (betweenAreas || crossesEnd) ? (page + 1.0) * pitch : top;
}

qreal KalahariTextDocumentLayout::paginateBlock(QTextLayout* layout, qreal top,
                                                const BlockMetrics& metrics) const {
    // Line boxes are stacked as layoutBlock() stacks them; a page break moves a line and
    // all lines after it down to the next text area. A page break's own line stays where it
    // is (it is drawn only with the formatting marks): it may end in the page's margin.
    const bool empty = layout->text().isEmpty();
    qreal natural = metrics.spaceBefore;  // box top within the block without page breaks
    qreal shift = 0;                      // how far page breaks moved the lines so far
    for (int i = 0; i < layout->lineCount(); ++i) {
        QTextLine line = layout->lineAt(i);
        const qreal boxHeight = lineBoxHeight(line, metrics, empty);
        const qreal boxTop = top + natural + shift;
        if (!metrics.pageBreak) {
            shift += flowLineTop(boxTop, boxHeight) - boxTop;
        }
        line.setPosition(QPointF(line.x(), natural + shift + lineTopInBox(line, metrics, empty)));
        natural += boxHeight;
    }
    if (layout->lineCount() == 0) {
        // No lines (not laid out by this layout): the block keeps its default height
        const qreal height = QFontMetricsF(document()->defaultFont()).height();
        shift = flowLineTop(top + natural, height) - (top + natural);
        natural += height;
    }
    if (metrics.pageBreak) {
        return nextPageTop(top) - top;
    }
    return natural + shift + metrics.spaceAfter;
}

qreal KalahariTextDocumentLayout::flowEstimate(qreal height, qreal top,
                                               const BlockMetrics& metrics) const {
    // The estimate is a whole number of estimated lines; they are placed as lines are
    const qreal lineHeight = std::max<qreal>(1.0, metrics.estimatedLineHeight);
    const int lines = std::max(
        1, static_cast<int>(std::lround((height - metrics.spaceBefore - metrics.spaceAfter) / lineHeight)));
    qreal y = top + metrics.spaceBefore;
    for (int i = 0; i < lines; ++i) {
        y = flowLineTop(y, lineHeight) + lineHeight;
    }
    return y - top + metrics.spaceAfter;
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
    for (; next < count && (m_blockFlowFlags[next] & FLOW_KEEP_WITH_NEXT) != 0; ++next) {
        needed += m_blockHeights[next];
    }
    if (next >= count || (m_blockFlowFlags[next] & FLOW_PAGE_BREAK) != 0) {
        return top;  // nothing to keep them with: they stay where the text puts them
    }
    needed += m_blockLeads[next];

    // They move only from below the top of a text area, and only when they fit in the next
    // one (a longer run would not fit there either)
    const qreal pitch = m_pageFlow.pitch;
    const qreal areaHeight = m_pageFlow.textHeight;
    const qreal offset = top - std::floor((top + FLOW_EPSILON) / pitch) * pitch;
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
        return m_blockHeights[index];
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

    int number = clip.isEmpty() ? 0 : laidOutBlockAtY(clip.top());
    for (QTextBlock block = document()->findBlockByNumber(number); block.isValid();
         block = block.next(), ++number) {
        ensureLaidOut(number, number);
        const qreal y = blockY(number);
        if (!clip.isEmpty() && y > clip.bottom()) {
            break;
        }
        if (QTextLayout* layout = block.layout()) {
            layout->draw(painter, QPointF(0, y));
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

    // A fuzzy hit above or below the document snaps to the first or last block
    const int number = laidOutBlockAtY(y);
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
    const qreal height = static_cast<size_t>(number) < m_blockHeights.size()
        ? blockHeight(number)
        : QFontMetricsF(document()->defaultFont()).height();
    return QRectF(0, blockY(number), documentWidth(), height);
}

// =============================================================================
// Objects of the text
// =============================================================================

void KalahariTextDocumentLayout::resizeInlineObject(QTextInlineObject item, int /*posInDocument*/,
                                                    const QTextFormat& format) {
    const TextObject object = textObjectOf(format);
    if (object == TextObject::None) {
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

void KalahariTextDocumentLayout::drawInlineObject(QPainter* painter, const QRectF& rect,
                                                  QTextInlineObject /*object*/,
                                                  int /*posInDocument*/, const QTextFormat& format) {
    const TextObject object = textObjectOf(format);
    if (object == TextObject::None || painter == nullptr) {
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
            // The pages show the break themselves; the formatting marks show it there too
            if (m_pageFlow.enabled && !m_formattingMarksShown) {
                break;
            }
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
        case TextObject::None:
            break;
    }
    painter->restore();
}

}  // namespace kalahari::editor
