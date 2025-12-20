/// @file paragraph_layout.cpp
/// @brief Implementation of ParagraphLayout (OpenSpec #00042 Phase 2.1/2.2/2.5)

#include <kalahari/editor/paragraph_layout.h>
#include <QFontMetricsF>
#include <QTextLine>
#include <QPainterPath>
#include <cmath>

namespace {
// Default selection colors - blue highlight with white text
// These are typical defaults that work well on most backgrounds
const QColor DEFAULT_SELECTION_BG(0x30, 0x8C, 0xC6);  // Blue similar to Qt::blue but lighter
const QColor DEFAULT_SELECTION_FG(Qt::white);
}

namespace kalahari::editor {

// =============================================================================
// Constructors / Destructor
// =============================================================================

ParagraphLayout::ParagraphLayout()
    : m_layout(std::make_unique<QTextLayout>())
    , m_text()
    , m_font()
    , m_formats()
    , m_width(0.0)
    , m_height(0.0)
    , m_dirty(true)
    , m_selectionStart(-1)
    , m_selectionEnd(-1)
    , m_selectionBg(DEFAULT_SELECTION_BG)
    , m_selectionFg(DEFAULT_SELECTION_FG)
    , m_spellErrors()
{
    m_layout->setCacheEnabled(true);
}

ParagraphLayout::ParagraphLayout(const QString& text)
    : m_layout(std::make_unique<QTextLayout>(text))
    , m_text(text)
    , m_font()
    , m_formats()
    , m_width(0.0)
    , m_height(0.0)
    , m_dirty(true)
    , m_selectionStart(-1)
    , m_selectionEnd(-1)
    , m_selectionBg(DEFAULT_SELECTION_BG)
    , m_selectionFg(DEFAULT_SELECTION_FG)
    , m_spellErrors()
{
    m_layout->setCacheEnabled(true);
}

ParagraphLayout::ParagraphLayout(const QString& text, const QFont& font)
    : m_layout(std::make_unique<QTextLayout>(text, font))
    , m_text(text)
    , m_font(font)
    , m_formats()
    , m_width(0.0)
    , m_height(0.0)
    , m_dirty(true)
    , m_selectionStart(-1)
    , m_selectionEnd(-1)
    , m_selectionBg(DEFAULT_SELECTION_BG)
    , m_selectionFg(DEFAULT_SELECTION_FG)
    , m_spellErrors()
{
    m_layout->setCacheEnabled(true);
}

ParagraphLayout::~ParagraphLayout() = default;

ParagraphLayout::ParagraphLayout(const ParagraphLayout& other)
    : m_layout(std::make_unique<QTextLayout>(other.m_text, other.m_font))
    , m_text(other.m_text)
    , m_font(other.m_font)
    , m_formats(other.m_formats)
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_dirty(true)  // Force re-layout after copy
    , m_selectionStart(other.m_selectionStart)
    , m_selectionEnd(other.m_selectionEnd)
    , m_selectionBg(other.m_selectionBg)
    , m_selectionFg(other.m_selectionFg)
    , m_spellErrors(other.m_spellErrors)
{
    m_layout->setCacheEnabled(true);
}

ParagraphLayout::ParagraphLayout(ParagraphLayout&& other) noexcept
    : m_layout(std::move(other.m_layout))
    , m_text(std::move(other.m_text))
    , m_font(std::move(other.m_font))
    , m_formats(std::move(other.m_formats))
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_dirty(other.m_dirty)
    , m_selectionStart(other.m_selectionStart)
    , m_selectionEnd(other.m_selectionEnd)
    , m_selectionBg(std::move(other.m_selectionBg))
    , m_selectionFg(std::move(other.m_selectionFg))
    , m_spellErrors(std::move(other.m_spellErrors))
{
    // Reset other to valid empty state
    other.m_layout = std::make_unique<QTextLayout>();
    other.m_text.clear();  // QString after std::move is unspecified, explicitly clear
    other.m_font = QFont();
    other.m_formats.clear();
    other.m_width = 0.0;
    other.m_height = 0.0;
    other.m_dirty = true;
    other.m_selectionStart = -1;
    other.m_selectionEnd = -1;
    other.m_selectionBg = DEFAULT_SELECTION_BG;
    other.m_selectionFg = DEFAULT_SELECTION_FG;
    other.m_spellErrors.clear();
}

ParagraphLayout& ParagraphLayout::operator=(const ParagraphLayout& other)
{
    if (this != &other) {
        m_text = other.m_text;
        m_font = other.m_font;
        m_formats = other.m_formats;
        m_width = other.m_width;
        m_height = other.m_height;
        m_dirty = true;  // Force re-layout after copy
        m_selectionStart = other.m_selectionStart;
        m_selectionEnd = other.m_selectionEnd;
        m_selectionBg = other.m_selectionBg;
        m_selectionFg = other.m_selectionFg;
        m_spellErrors = other.m_spellErrors;

        m_layout = std::make_unique<QTextLayout>(m_text, m_font);
        m_layout->setCacheEnabled(true);
    }
    return *this;
}

ParagraphLayout& ParagraphLayout::operator=(ParagraphLayout&& other) noexcept
{
    if (this != &other) {
        m_layout = std::move(other.m_layout);
        m_text = std::move(other.m_text);
        m_font = std::move(other.m_font);
        m_formats = std::move(other.m_formats);
        m_width = other.m_width;
        m_height = other.m_height;
        m_dirty = other.m_dirty;
        m_selectionStart = other.m_selectionStart;
        m_selectionEnd = other.m_selectionEnd;
        m_selectionBg = std::move(other.m_selectionBg);
        m_selectionFg = std::move(other.m_selectionFg);
        m_spellErrors = std::move(other.m_spellErrors);

        // Reset other to valid empty state
        other.m_layout = std::make_unique<QTextLayout>();
        other.m_text.clear();  // QString after std::move is unspecified, explicitly clear
        other.m_font = QFont();
        other.m_formats.clear();
        other.m_width = 0.0;
        other.m_height = 0.0;
        other.m_dirty = true;
        other.m_selectionStart = -1;
        other.m_selectionEnd = -1;
        other.m_selectionBg = DEFAULT_SELECTION_BG;
        other.m_selectionFg = DEFAULT_SELECTION_FG;
        other.m_spellErrors.clear();
    }
    return *this;
}

// =============================================================================
// Text and Font
// =============================================================================

QString ParagraphLayout::text() const
{
    return m_text;
}

void ParagraphLayout::setText(const QString& text)
{
    if (m_text != text) {
        m_text = text;
        m_layout->setText(text);
        invalidate();
    }
}

QFont ParagraphLayout::font() const
{
    return m_font;
}

void ParagraphLayout::setFont(const QFont& font)
{
    if (m_font != font) {
        m_font = font;
        m_layout->setFont(font);
        invalidate();
    }
}

// =============================================================================
// Formatting (Phase 2.2)
// =============================================================================

void ParagraphLayout::setFormats(const QList<QTextLayout::FormatRange>& formats)
{
    m_formats = formats;
    m_layout->setFormats(formats);
    invalidate();
}

QList<QTextLayout::FormatRange> ParagraphLayout::formats() const
{
    return m_formats;
}

void ParagraphLayout::clearFormats()
{
    if (!m_formats.isEmpty()) {
        m_formats.clear();
        m_layout->clearFormats();
        invalidate();
    }
}

bool ParagraphLayout::hasFormats() const
{
    return !m_formats.isEmpty();
}

// =============================================================================
// Layout Operations
// =============================================================================

qreal ParagraphLayout::doLayout(qreal width)
{
    // Skip if already laid out at this width and not dirty
    if (!m_dirty && qFuzzyCompare(m_width, width)) {
        return m_height;
    }

    performLayout(width);
    return m_height;
}

qreal ParagraphLayout::layoutWidth() const
{
    return m_width;
}

bool ParagraphLayout::isDirty() const
{
    return m_dirty;
}

void ParagraphLayout::invalidate()
{
    m_dirty = true;
}

void ParagraphLayout::clear()
{
    m_text.clear();
    m_font = QFont();
    m_formats.clear();
    m_width = 0.0;
    m_height = 0.0;
    m_dirty = true;
    m_selectionStart = -1;
    m_selectionEnd = -1;
    m_spellErrors.clear();

    m_layout = std::make_unique<QTextLayout>();
    m_layout->setCacheEnabled(true);
}

// =============================================================================
// Geometry
// =============================================================================

qreal ParagraphLayout::height() const
{
    return m_height;
}

int ParagraphLayout::lineCount() const
{
    if (m_dirty) {
        return 0;
    }
    return m_layout->lineCount();
}

QRectF ParagraphLayout::boundingRect() const
{
    if (m_dirty) {
        return QRectF();
    }
    return m_layout->boundingRect();
}

QRectF ParagraphLayout::lineRect(int lineIndex) const
{
    if (m_dirty || lineIndex < 0 || lineIndex >= m_layout->lineCount()) {
        return QRectF();
    }

    QTextLine line = m_layout->lineAt(lineIndex);
    if (!line.isValid()) {
        return QRectF();
    }

    return line.rect();
}

// =============================================================================
// Hit Testing (Phase 2.4)
// =============================================================================

int ParagraphLayout::positionAt(const QPointF& point) const
{
    if (m_dirty || m_layout->lineCount() == 0) {
        return -1;
    }

    // Find the line containing the y-coordinate
    int lineIndex = -1;
    for (int i = 0; i < m_layout->lineCount(); ++i) {
        QTextLine line = m_layout->lineAt(i);
        if (!line.isValid()) {
            continue;
        }

        // Check if point.y() is within this line's vertical bounds
        qreal lineTop = line.y();
        qreal lineBottom = lineTop + line.height();

        if (point.y() >= lineTop && point.y() < lineBottom) {
            lineIndex = i;
            break;
        }
    }

    // If point is above all lines, use first line
    if (lineIndex == -1 && point.y() < 0) {
        lineIndex = 0;
    }

    // If point is below all lines, use last line
    if (lineIndex == -1) {
        lineIndex = m_layout->lineCount() - 1;
    }

    QTextLine line = m_layout->lineAt(lineIndex);
    if (!line.isValid()) {
        return -1;
    }

    // Use xToCursor to find the character position
    // CursorBetweenCharacters places cursor between characters (for clicking)
    return line.xToCursor(point.x(), QTextLine::CursorBetweenCharacters);
}

QRectF ParagraphLayout::cursorRect(int position) const
{
    if (m_dirty || m_layout->lineCount() == 0) {
        return QRectF();
    }

    // Clamp position to valid range
    if (position < 0) {
        position = 0;
    }
    if (position > m_text.length()) {
        position = m_text.length();
    }

    // Find the line containing this position
    int lineIndex = lineForPosition(position);
    if (lineIndex < 0) {
        return QRectF();
    }

    QTextLine line = m_layout->lineAt(lineIndex);
    if (!line.isValid()) {
        return QRectF();
    }

    // Get the x-coordinate for the cursor position
    // Use Leading edge for cursor positioning
    qreal x = line.cursorToX(position, QTextLine::Leading);

    // Return a thin rectangle at the cursor position
    // Width of 1 pixel for cursor line
    return QRectF(x, line.y(), 1.0, line.height());
}

int ParagraphLayout::lineForPosition(int position) const
{
    if (m_dirty || m_layout->lineCount() == 0) {
        return -1;
    }

    // Clamp position to valid range
    if (position < 0) {
        position = 0;
    }
    if (position > m_text.length()) {
        position = m_text.length();
    }

    // Search for the line containing this position
    for (int i = 0; i < m_layout->lineCount(); ++i) {
        QTextLine line = m_layout->lineAt(i);
        if (!line.isValid()) {
            continue;
        }

        int lineStart = line.textStart();
        int lineEnd = lineStart + line.textLength();

        // Position is within this line
        if (position >= lineStart && position <= lineEnd) {
            return i;
        }
    }

    // If position is at end of text and no line matched,
    // return the last line
    if (position == m_text.length() && m_layout->lineCount() > 0) {
        return m_layout->lineCount() - 1;
    }

    return -1;
}

// =============================================================================
// Drawing (Phase 2.5)
// =============================================================================

void ParagraphLayout::draw(QPainter* painter, const QPointF& position)
{
    if (!painter || m_dirty) {
        return;
    }

    painter->save();

    // Draw selection highlighting first (behind text)
    if (hasSelection()) {
        drawSelection(painter, position);
    }

    // Draw the text using QTextLayout::draw()
    // QTextLayout::draw uses selections passed as parameter, but we handle selection
    // manually for more control. Pass empty selections list.
    QList<QTextLayout::FormatRange> selections;

    // If we have selection, create a format range for it to change text color
    if (hasSelection()) {
        int selStart = std::max(0, std::min(m_selectionStart, m_selectionEnd));
        int selEnd = std::min(static_cast<int>(m_text.length()), std::max(m_selectionStart, m_selectionEnd));
        if (selEnd > selStart) {
            QTextLayout::FormatRange selFormat;
            selFormat.start = selStart;
            selFormat.length = selEnd - selStart;
            selFormat.format.setForeground(m_selectionFg);
            selections.append(selFormat);
        }
    }

    m_layout->draw(painter, position, selections);

    // Draw spell error underlines on top
    if (hasSpellErrors()) {
        drawSpellErrors(painter, position);
    }

    painter->restore();
}

void ParagraphLayout::setSelection(int start, int end)
{
    m_selectionStart = start;
    m_selectionEnd = end;
}

void ParagraphLayout::clearSelection()
{
    m_selectionStart = -1;
    m_selectionEnd = -1;
}

bool ParagraphLayout::hasSelection() const
{
    return m_selectionStart >= 0 && m_selectionEnd >= 0 && m_selectionStart != m_selectionEnd;
}

int ParagraphLayout::selectionStart() const
{
    return m_selectionStart;
}

int ParagraphLayout::selectionEnd() const
{
    return m_selectionEnd;
}

void ParagraphLayout::setSelectionColors(const QColor& background, const QColor& foreground)
{
    m_selectionBg = background;
    m_selectionFg = foreground;
}

QColor ParagraphLayout::selectionBackgroundColor() const
{
    return m_selectionBg;
}

QColor ParagraphLayout::selectionForegroundColor() const
{
    return m_selectionFg;
}

void ParagraphLayout::addSpellError(int start, int length)
{
    if (start >= 0 && length > 0) {
        m_spellErrors.emplace_back(start, length);
    }
}

void ParagraphLayout::clearSpellErrors()
{
    m_spellErrors.clear();
}

std::vector<SpellError> ParagraphLayout::spellErrors() const
{
    return m_spellErrors;
}

bool ParagraphLayout::hasSpellErrors() const
{
    return !m_spellErrors.empty();
}

// =============================================================================
// Advanced Access
// =============================================================================

const QTextLayout& ParagraphLayout::textLayout() const
{
    return *m_layout;
}

QTextLayout& ParagraphLayout::textLayout()
{
    return *m_layout;
}

// =============================================================================
// Private Methods
// =============================================================================

void ParagraphLayout::performLayout(qreal width)
{
    // Get font metrics for leading calculation
    QFontMetricsF fontMetrics(m_font);
    qreal leading = fontMetrics.leading();

    // Handle negative leading (common with some fonts)
    if (leading < 0) {
        leading = 0;
    }

    qreal currentHeight = 0.0;

    m_layout->beginLayout();

    while (true) {
        QTextLine line = m_layout->createLine();
        if (!line.isValid()) {
            break;
        }

        line.setLineWidth(width);
        currentHeight += leading;
        line.setPosition(QPointF(0, currentHeight));
        currentHeight += line.height();
    }

    m_layout->endLayout();

    m_width = width;
    m_height = currentHeight;
    m_dirty = false;
}

void ParagraphLayout::drawSelection(QPainter* painter, const QPointF& position)
{
    if (!hasSelection() || m_layout->lineCount() == 0) {
        return;
    }

    // Normalize selection range
    int selStart = std::max(0, std::min(m_selectionStart, m_selectionEnd));
    int selEnd = std::min(static_cast<int>(m_text.length()), std::max(m_selectionStart, m_selectionEnd));

    if (selEnd <= selStart) {
        return;
    }

    painter->save();
    painter->setBrush(m_selectionBg);
    painter->setPen(Qt::NoPen);

    // Iterate through lines and draw selection rectangles
    for (int i = 0; i < m_layout->lineCount(); ++i) {
        QTextLine line = m_layout->lineAt(i);
        if (!line.isValid()) {
            continue;
        }

        int lineStart = line.textStart();
        int lineEnd = lineStart + line.textLength();

        // Check if this line overlaps with selection
        if (selEnd <= lineStart || selStart >= lineEnd) {
            continue;  // No overlap
        }

        // Calculate the portion of selection on this line
        int rangeStart = std::max(selStart, lineStart);
        int rangeEnd = std::min(selEnd, lineEnd);

        // Get x coordinates for the selection range
        qreal x1 = line.cursorToX(rangeStart, QTextLine::Leading);
        qreal x2 = line.cursorToX(rangeEnd, QTextLine::Leading);

        // Ensure x1 < x2
        if (x1 > x2) {
            std::swap(x1, x2);
        }

        // Draw selection rectangle for this line
        QRectF selRect(
            position.x() + x1,
            position.y() + line.y(),
            x2 - x1,
            line.height()
        );

        painter->drawRect(selRect);
    }

    painter->restore();
}

void ParagraphLayout::drawSpellErrors(QPainter* painter, const QPointF& position)
{
    if (m_spellErrors.empty() || m_layout->lineCount() == 0) {
        return;
    }

    // Spell error color - traditional red wavy underline
    QColor errorColor(Qt::red);

    for (const auto& error : m_spellErrors) {
        drawWavyUnderline(painter, error.start, error.length, position, errorColor);
    }
}

void ParagraphLayout::drawWavyUnderline(QPainter* painter, int startPos, int length,
                                         const QPointF& offset, const QColor& color)
{
    if (length <= 0 || m_layout->lineCount() == 0) {
        return;
    }

    // Clamp to valid range
    int endPos = std::min(startPos + length, static_cast<int>(m_text.length()));
    startPos = std::max(0, startPos);

    if (endPos <= startPos) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    QPen wavyPen(color);
    wavyPen.setWidthF(1.0);
    painter->setPen(wavyPen);

    // Wave parameters
    const qreal waveHeight = 2.0;  // Height of the wave
    const qreal waveLength = 4.0;  // Width of one full wave cycle

    // Iterate through lines to find where to draw
    for (int i = 0; i < m_layout->lineCount(); ++i) {
        QTextLine line = m_layout->lineAt(i);
        if (!line.isValid()) {
            continue;
        }

        int lineStart = line.textStart();
        int lineEnd = lineStart + line.textLength();

        // Check if this line overlaps with the error range
        if (endPos <= lineStart || startPos >= lineEnd) {
            continue;  // No overlap
        }

        // Calculate the portion of error on this line
        int rangeStart = std::max(startPos, lineStart);
        int rangeEnd = std::min(endPos, lineEnd);

        // Get x coordinates
        qreal x1 = line.cursorToX(rangeStart, QTextLine::Leading);
        qreal x2 = line.cursorToX(rangeEnd, QTextLine::Leading);

        if (x1 > x2) {
            std::swap(x1, x2);
        }

        // Position the underline at the baseline
        // Use descent to position below the text baseline
        QFontMetricsF fm(m_font);
        qreal baselineY = offset.y() + line.y() + line.ascent() + fm.underlinePos();

        // Build the wavy path
        QPainterPath wavePath;
        qreal currentX = offset.x() + x1;
        qreal endX = offset.x() + x2;

        wavePath.moveTo(currentX, baselineY);

        bool up = true;
        while (currentX < endX) {
            qreal nextX = std::min(currentX + waveLength / 2.0, endX);
            qreal nextY = baselineY + (up ? -waveHeight : waveHeight);

            // Use quadratic bezier for smooth wave
            qreal controlX = (currentX + nextX) / 2.0;
            wavePath.quadTo(controlX, nextY, nextX, baselineY);

            currentX = nextX;
            up = !up;
        }

        painter->drawPath(wavePath);
    }

    painter->restore();
}

}  // namespace kalahari::editor
