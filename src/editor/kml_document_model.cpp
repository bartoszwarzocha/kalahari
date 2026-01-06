/// @file kml_document_model.cpp
/// @brief Lightweight document model with lazy rendering (OpenSpec #00043)
///
/// Implementation of KmlDocumentModel for fast document loading and
/// viewport-only rendering using QTextLayout.

#include "kalahari/editor/kml_document_model.h"
#include "kalahari/editor/kml_format_registry.h"
#include "kalahari/core/logger.h"

#include <QXmlStreamReader>
#include <QFontMetricsF>
#include <QTextFormat>
#include <QTextLine>
#include <QTextOption>

#include <algorithm>
#include <cmath>

namespace kalahari {
namespace editor {

// Static member initialization
const std::vector<FormatRun> KmlDocumentModel::s_emptyFormats;

// =============================================================================
// Constructor / Destructor
// =============================================================================

KmlDocumentModel::KmlDocumentModel(QObject* parent)
    : QObject(parent)
    , m_paragraphs()
    , m_heightTree()
    , m_font()
    , m_lineWidth(800.0)
    , m_estimatedLineHeight(20.0)
    , m_charsPerLine(80.0)
{
    // Set default font
    m_font = QFont(QStringLiteral("Segoe UI"), 11);

    // Calculate chars per line from font metrics
    QFontMetricsF fm(m_font);
    double avgCharWidth = fm.averageCharWidth();
    if (avgCharWidth > 0) {
        m_charsPerLine = m_lineWidth / avgCharWidth;
    }
    m_estimatedLineHeight = fm.height();
}

KmlDocumentModel::~KmlDocumentModel() = default;

// =============================================================================
// Document Loading
// =============================================================================

bool KmlDocumentModel::loadKml(const QString& kml)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("KmlDocumentModel::loadKml: Loading {} chars", kml.size());

    // Clear existing content
    clear();

    if (kml.isEmpty()) {
        emit documentLoaded();
        return true;
    }

    // Wrap KML in root element if needed
    QString wrappedKml = kml.trimmed();
    if (!wrappedKml.startsWith(QStringLiteral("<kml")) &&
        !wrappedKml.startsWith(QStringLiteral("<document")) &&
        !wrappedKml.startsWith(QStringLiteral("<doc"))) {
        wrappedKml = QStringLiteral("<kml>") + wrappedKml + QStringLiteral("</kml>");
    }

    QXmlStreamReader reader(wrappedKml);

    // Skip to first start element (root)
    while (!reader.atEnd() && !reader.isStartElement()) {
        reader.readNext();
    }

    if (reader.atEnd()) {
        logger.warn("KmlDocumentModel::loadKml: No root element found");
        emit documentLoaded();
        return true;
    }

    // Skip root element
    QString rootTag = reader.name().toString();
    reader.readNext();

    // Parse paragraphs
    while (!reader.atEnd()) {
        if (reader.isEndElement()) {
            QString tag = reader.name().toString();
            if (tag == rootTag) {
                break;
            }
            reader.readNext();
            continue;
        }

        if (reader.isStartElement()) {
            QString tag = reader.name().toString();

            if (tag == QStringLiteral("p") || tag == QStringLiteral("paragraph")) {
                // Read entire paragraph element as string
                QString paraKml;
                int depth = 1;
                QString startTagWithAttrs;

                // Capture start tag with attributes
                QXmlStreamAttributes attrs = reader.attributes();
                startTagWithAttrs = QStringLiteral("<") + tag;
                for (const auto& attr : attrs) {
                    startTagWithAttrs += QStringLiteral(" ") +
                                        attr.name().toString() +
                                        QStringLiteral("=\"") +
                                        attr.value().toString() +
                                        QStringLiteral("\"");
                }
                startTagWithAttrs += QStringLiteral(">");

                // Read until closing tag
                reader.readNext();
                while (!reader.atEnd() && depth > 0) {
                    if (reader.isStartElement()) {
                        QString elemTag = reader.name().toString();
                        QXmlStreamAttributes elemAttrs = reader.attributes();
                        paraKml += QStringLiteral("<") + elemTag;
                        for (const auto& attr : elemAttrs) {
                            paraKml += QStringLiteral(" ") +
                                      attr.name().toString() +
                                      QStringLiteral("=\"") +
                                      attr.value().toString() +
                                      QStringLiteral("\"");
                        }
                        paraKml += QStringLiteral(">");
                        depth++;
                    } else if (reader.isEndElement()) {
                        QString elemTag = reader.name().toString();
                        depth--;
                        if (depth > 0) {
                            paraKml += QStringLiteral("</") + elemTag + QStringLiteral(">");
                        }
                    } else if (reader.isCharacters()) {
                        QString text = reader.text().toString();
                        // Escape XML special characters
                        paraKml += KmlFormatRegistry::escapeXml(text);
                    }
                    reader.readNext();
                }

                // Parse paragraph into structure
                Paragraph para;
                QString fullParaKml = startTagWithAttrs + paraKml + QStringLiteral("</") + tag + QStringLiteral(">");
                if (parseParagraph(fullParaKml, para)) {
                    m_paragraphs.push_back(std::move(para));
                }
            } else {
                // Skip unknown elements
                reader.skipCurrentElement();
            }
        } else {
            reader.readNext();
        }
    }

    if (reader.hasError() &&
        reader.error() != QXmlStreamReader::PrematureEndOfDocumentError) {
        logger.error("KmlDocumentModel::loadKml: XML error: {}",
                    reader.errorString().toStdString());
        return false;
    }

    // Initialize height tree with estimated heights and calculate statistics
    size_t count = m_paragraphs.size();
    m_cachedCharCount = 0;
    m_cachedWordCount = 0;
    m_cachedCharCountNoSpaces = 0;

    if (count > 0) {
        m_heightTree.resize(count, m_estimatedLineHeight);

        // Update with better estimates based on text length and calculate stats
        for (size_t i = 0; i < count; ++i) {
            const QString& text = m_paragraphs[i].text;

            // Height estimation
            double height = estimateHeight(text);
            m_heightTree.setHeight(i, height);

            // Character count
            m_cachedCharCount += static_cast<size_t>(text.length());

            // Word count and character count without spaces
            if (!text.isEmpty()) {
                int wordCount = 0;
                int nonSpaceCount = 0;
                bool inWord = false;
                for (const QChar& c : text) {
                    if (c.isSpace()) {
                        inWord = false;
                    } else {
                        ++nonSpaceCount;
                        if (!inWord) {
                            inWord = true;
                            ++wordCount;
                        }
                    }
                }
                m_cachedWordCount += static_cast<size_t>(wordCount);
                m_cachedCharCountNoSpaces += static_cast<size_t>(nonSpaceCount);
            }
        }
    }

    logger.debug("KmlDocumentModel::loadKml: Loaded {} paragraphs, {} chars, {} words, total height: {}",
                count, m_cachedCharCount, m_cachedWordCount, m_heightTree.totalHeight());

    emit documentLoaded();
    emit totalHeightChanged(m_heightTree.totalHeight());

    return true;
}

void KmlDocumentModel::clear()
{
    m_paragraphs.clear();
    m_heightTree.clear();
    m_cachedCharCount = 0;
    m_cachedWordCount = 0;
    m_cachedCharCountNoSpaces = 0;
}

bool KmlDocumentModel::isEmpty() const
{
    return m_paragraphs.empty();
}

// =============================================================================
// Paragraph Access
// =============================================================================

size_t KmlDocumentModel::paragraphCount() const
{
    return m_paragraphs.size();
}

QString KmlDocumentModel::paragraphText(size_t index) const
{
    if (index >= m_paragraphs.size()) {
        return QString();
    }
    return m_paragraphs[index].text;
}

const std::vector<FormatRun>& KmlDocumentModel::paragraphFormats(size_t index) const
{
    if (index >= m_paragraphs.size()) {
        return s_emptyFormats;
    }
    return m_paragraphs[index].formats;
}

QString KmlDocumentModel::plainText() const
{
    QString result;
    for (size_t i = 0; i < m_paragraphs.size(); ++i) {
        if (i > 0) {
            result += QLatin1Char('\n');
        }
        result += m_paragraphs[i].text;
    }
    return result;
}

size_t KmlDocumentModel::paragraphLength(size_t index) const
{
    if (index >= m_paragraphs.size()) {
        return 0;
    }
    return static_cast<size_t>(m_paragraphs[index].text.length());
}

size_t KmlDocumentModel::characterCount() const
{
    return m_cachedCharCount;
}

size_t KmlDocumentModel::wordCount() const
{
    return m_cachedWordCount;
}

size_t KmlDocumentModel::characterCountNoSpaces() const
{
    return m_cachedCharCountNoSpaces;
}

// =============================================================================
// Height Queries
// =============================================================================

double KmlDocumentModel::paragraphY(size_t index) const
{
    return m_heightTree.prefixSum(index);
}

double KmlDocumentModel::paragraphHeight(size_t index) const
{
    if (index >= m_heightTree.size()) {
        return 0.0;
    }
    return m_heightTree.height(index);
}

double KmlDocumentModel::totalHeight() const
{
    return m_heightTree.totalHeight();
}

size_t KmlDocumentModel::paragraphAtY(double y) const
{
    return m_heightTree.findIndexForY(y);
}

// =============================================================================
// Lazy Layout
// =============================================================================

void KmlDocumentModel::ensureLayouted(size_t first, size_t last)
{
    if (m_paragraphs.empty()) {
        return;
    }

    // Clamp range
    first = std::min(first, m_paragraphs.size() - 1);
    last = std::min(last, m_paragraphs.size() - 1);

    for (size_t i = first; i <= last; ++i) {
        if (!m_paragraphs[i].layoutValid) {
            createLayout(i);
        }
    }
}

QTextLayout* KmlDocumentModel::layout(size_t index) const
{
    if (index >= m_paragraphs.size()) {
        return nullptr;
    }
    return m_paragraphs[index].layout.get();
}

bool KmlDocumentModel::isLayouted(size_t index) const
{
    if (index >= m_paragraphs.size()) {
        return false;
    }
    return m_paragraphs[index].layoutValid;
}

void KmlDocumentModel::invalidateLayout(size_t index)
{
    if (index >= m_paragraphs.size()) {
        return;
    }

    m_paragraphs[index].layout.reset();
    m_paragraphs[index].layoutValid = false;

    // Update height tree with estimate
    double height = estimateHeight(m_paragraphs[index].text);
    double oldHeight = m_heightTree.height(index);
    if (std::abs(height - oldHeight) > 0.01) {
        m_heightTree.setHeight(index, height);
        emit paragraphHeightChanged(index, height);
        emit totalHeightChanged(m_heightTree.totalHeight());
    }
}

void KmlDocumentModel::invalidateAllLayouts()
{
    for (size_t i = 0; i < m_paragraphs.size(); ++i) {
        m_paragraphs[i].layout.reset();
        m_paragraphs[i].layoutValid = false;
    }

    // Recalculate all heights
    for (size_t i = 0; i < m_paragraphs.size(); ++i) {
        double height = estimateHeight(m_paragraphs[i].text);
        m_heightTree.setHeight(i, height);
    }

    emit totalHeightChanged(m_heightTree.totalHeight());
}

void KmlDocumentModel::evictLayouts(size_t keepFirst, size_t keepLast)
{
    if (m_paragraphs.empty()) {
        return;
    }

    // Clamp range
    keepFirst = std::min(keepFirst, m_paragraphs.size() - 1);
    keepLast = std::min(keepLast, m_paragraphs.size() - 1);

    // Evict layouts before keepFirst
    for (size_t i = 0; i < keepFirst; ++i) {
        if (m_paragraphs[i].layout) {
            m_paragraphs[i].layout.reset();
            // Keep layoutValid true - height is accurate
        }
    }

    // Evict layouts after keepLast
    for (size_t i = keepLast + 1; i < m_paragraphs.size(); ++i) {
        if (m_paragraphs[i].layout) {
            m_paragraphs[i].layout.reset();
            // Keep layoutValid true - height is accurate
        }
    }
}

// =============================================================================
// Configuration
// =============================================================================

void KmlDocumentModel::setFont(const QFont& font)
{
    if (m_font != font) {
        m_font = font;

        // Update chars per line estimate
        QFontMetricsF fm(m_font);
        double avgCharWidth = fm.averageCharWidth();
        if (avgCharWidth > 0) {
            m_charsPerLine = m_lineWidth / avgCharWidth;
        }
        m_estimatedLineHeight = fm.height();

        // Invalidate all layouts
        invalidateAllLayouts();
    }
}

QFont KmlDocumentModel::font() const
{
    return m_font;
}

void KmlDocumentModel::setLineWidth(double width)
{
    if (std::abs(m_lineWidth - width) > 0.01) {
        m_lineWidth = width;

        // Update chars per line estimate
        QFontMetricsF fm(m_font);
        double avgCharWidth = fm.averageCharWidth();
        if (avgCharWidth > 0) {
            m_charsPerLine = m_lineWidth / avgCharWidth;
        }

        // Invalidate all layouts
        invalidateAllLayouts();
    }
}

double KmlDocumentModel::lineWidth() const
{
    return m_lineWidth;
}

void KmlDocumentModel::setEstimatedLineHeight(double height)
{
    m_estimatedLineHeight = height;
}

void KmlDocumentModel::setTextColor(const QColor& color)
{
    if (m_textColor != color) {
        m_textColor = color;
        // Invalidate all layouts so they get recreated with new color
        invalidateAllLayouts();
    }
}

QColor KmlDocumentModel::textColor() const
{
    return m_textColor;
}

// =============================================================================
// Private Methods
// =============================================================================

bool KmlDocumentModel::parseParagraph(const QString& paraKml, Paragraph& para)
{
    QXmlStreamReader reader(paraKml);

    // Skip to <p> element
    while (!reader.atEnd() && !reader.isStartElement()) {
        reader.readNext();
    }

    if (reader.atEnd()) {
        return false;
    }

    QString tag = reader.name().toString();
    if (tag != QStringLiteral("p") && tag != QStringLiteral("paragraph")) {
        return false;
    }

    // Parse alignment attribute
    QXmlStreamAttributes attrs = reader.attributes();
    if (attrs.hasAttribute(QStringLiteral("align"))) {
        QString alignStr = attrs.value(QStringLiteral("align")).toString().toLower();
        if (alignStr == QStringLiteral("left")) {
            para.alignment = Qt::AlignLeft;
        } else if (alignStr == QStringLiteral("center")) {
            para.alignment = Qt::AlignHCenter;
        } else if (alignStr == QStringLiteral("right")) {
            para.alignment = Qt::AlignRight;
        } else if (alignStr == QStringLiteral("justify")) {
            para.alignment = Qt::AlignJustify;
        }
    }

    // Move past start element
    reader.readNext();

    // Parse inline content
    QString text;
    std::vector<FormatRun> formats;
    QTextCharFormat defaultFormat;
    size_t pos = 0;

    parseInlineContent(reader, text, formats, defaultFormat, pos, tag);

    para.text = text;
    para.formats = std::move(formats);
    para.layout.reset();
    para.layoutValid = false;

    return true;
}

void KmlDocumentModel::parseInlineContent(QXmlStreamReader& reader,
                                          QString& text,
                                          std::vector<FormatRun>& formats,
                                          QTextCharFormat currentFormat,
                                          size_t& currentPos,
                                          const QString& endTag)
{
    while (!reader.atEnd()) {
        if (reader.isEndElement()) {
            QString tag = reader.name().toString();
            if (tag == endTag || tag == QStringLiteral("paragraph")) {
                reader.readNext();
                return;
            }
            return;
        }

        if (reader.isCharacters()) {
            QString chars = reader.text().toString();
            if (!chars.isEmpty()) {
                size_t start = currentPos;
                text += chars;
                currentPos += static_cast<size_t>(chars.length());

                // Add format run if non-default formatting
                if (currentFormat.fontWeight() != QFont::Normal ||
                    currentFormat.fontItalic() ||
                    currentFormat.fontUnderline() ||
                    currentFormat.fontStrikeOut() ||
                    currentFormat.verticalAlignment() != QTextCharFormat::AlignNormal ||
                    currentFormat.hasProperty(KmlPropComment) ||
                    currentFormat.hasProperty(KmlPropTodo) ||
                    currentFormat.hasProperty(KmlPropFootnote)) {
                    FormatRun run;
                    run.start = start;
                    run.end = currentPos;
                    run.format = currentFormat;
                    formats.push_back(run);
                }
            }
            reader.readNext();
        } else if (reader.isStartElement()) {
            QString tag = reader.name().toString();

            if (KmlFormatRegistry::isFormattingTag(tag)) {
                // Apply formatting tag
                QTextCharFormat newFormat = KmlFormatRegistry::applyTagFormat(tag, currentFormat);
                reader.readNext();
                parseInlineContent(reader, text, formats, newFormat, currentPos, tag);
            } else if (KmlFormatRegistry::isMetadataTag(tag)) {
                // Apply metadata
                QTextCharFormat newFormat = currentFormat;
                QXmlStreamAttributes attrs = reader.attributes();
                QVariantMap metadata;

                if (attrs.hasAttribute(QStringLiteral("id"))) {
                    metadata[QStringLiteral("id")] = attrs.value(QStringLiteral("id")).toString();
                }

                if (tag == QStringLiteral("comment")) {
                    newFormat.setProperty(KmlPropComment, metadata);
                } else if (tag == QStringLiteral("todo")) {
                    newFormat.setProperty(KmlPropTodo, metadata);
                } else if (tag == QStringLiteral("footnote")) {
                    newFormat.setProperty(KmlPropFootnote, metadata);
                }

                reader.readNext();
                parseInlineContent(reader, text, formats, newFormat, currentPos, tag);
            } else if (tag == QStringLiteral("t") || tag == QStringLiteral("text")) {
                // Text run - just parse content
                reader.readNext();
                parseInlineContent(reader, text, formats, currentFormat, currentPos, tag);
            } else {
                // Unknown element - skip
                reader.skipCurrentElement();
            }
        } else {
            reader.readNext();
        }
    }
}

void KmlDocumentModel::createLayout(size_t index)
{
    if (index >= m_paragraphs.size()) {
        return;
    }

    Paragraph& para = m_paragraphs[index];

    // Create QTextLayout
    para.layout = std::make_unique<QTextLayout>(para.text, m_font);

    // Apply formats with text color
    applyFormats(para.layout.get(), para.formats, para.text.length());

    // Set text option with alignment
    QTextOption textOption;
    textOption.setAlignment(para.alignment);
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    para.layout->setTextOption(textOption);

    // Perform layout
    para.layout->beginLayout();

    double y = 0;
    while (true) {
        QTextLine line = para.layout->createLine();
        if (!line.isValid()) {
            break;
        }

        line.setLineWidth(m_lineWidth);
        line.setPosition(QPointF(0, y));
        y += line.height();
    }

    para.layout->endLayout();
    para.layoutValid = true;

    // Update height tree with actual height
    double actualHeight = para.layout->boundingRect().height();
    if (actualHeight < 1.0) {
        actualHeight = m_estimatedLineHeight;  // Minimum height for empty paragraphs
    }

    double oldHeight = m_heightTree.height(index);
    if (std::abs(actualHeight - oldHeight) > 0.01) {
        m_heightTree.setHeight(index, actualHeight);
        emit paragraphHeightChanged(index, actualHeight);
        emit totalHeightChanged(m_heightTree.totalHeight());
    }
}

double KmlDocumentModel::estimateHeight(const QString& text) const
{
    if (text.isEmpty()) {
        return m_estimatedLineHeight;
    }

    // Estimate number of lines
    double numLines = std::ceil(static_cast<double>(text.length()) / m_charsPerLine);
    if (numLines < 1.0) {
        numLines = 1.0;
    }

    return numLines * m_estimatedLineHeight;
}

void KmlDocumentModel::applyFormats(QTextLayout* layout, const std::vector<FormatRun>& formats, int textLength) const
{
    if (!layout) {
        return;
    }

    QList<QTextLayout::FormatRange> ranges;

    // First, add a base format covering all text with the text color
    // This ensures unformatted text gets the correct color
    if (textLength > 0) {
        QTextLayout::FormatRange baseRange;
        baseRange.start = 0;
        baseRange.length = textLength;
        baseRange.format.setForeground(m_textColor);
        ranges.append(baseRange);
    }

    // Then add the specific format ranges
    ranges.reserve(static_cast<int>(formats.size()) + 1);

    for (const FormatRun& run : formats) {
        if (run.start >= run.end) {
            continue;
        }

        QTextLayout::FormatRange range;
        range.start = static_cast<int>(run.start);
        range.length = static_cast<int>(run.end - run.start);
        range.format = run.format;
        // Ensure text color is set if not already specified
        if (!range.format.hasProperty(QTextFormat::ForegroundBrush)) {
            range.format.setForeground(m_textColor);
        }
        ranges.append(range);
    }

    layout->setFormats(ranges);
}

} // namespace editor
} // namespace kalahari
