/// @file kml_document_model.cpp
/// @brief KML reader: the paragraphs of a chapter with their text and formatting
///
/// Reads KML in a single streaming pass, by the format rules of KmlFormatRegistry.

#include "kalahari/editor/kml_document_model.h"
#include "kalahari/editor/kml_format_registry.h"
#include "kalahari/core/logger.h"

#include <QXmlStreamReader>

namespace kalahari {
namespace editor {

// Static member initialization
const std::vector<FormatRun> KmlDocumentModel::s_emptyFormats;

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
        return true;
    }

    // Wrap KML in root element if needed
    QXmlStreamReader reader(KmlFormatRegistry::withRootElement(kml));

    // Skip to first start element (root)
    while (!reader.atEnd() && !reader.isStartElement()) {
        reader.readNext();
    }

    if (reader.atEnd()) {
        logger.warn("KmlDocumentModel::loadKml: No root element found");
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
                // Parse the paragraph straight from the document reader (consumes </p>)
                Paragraph para;
                parseParagraphElement(reader, para);
                m_paragraphs.push_back(std::move(para));
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

    logger.debug("KmlDocumentModel::loadKml: Loaded {} paragraphs", m_paragraphs.size());
    return true;
}

void KmlDocumentModel::clear()
{
    m_paragraphs.clear();
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

Qt::Alignment KmlDocumentModel::paragraphAlignment(size_t index) const
{
    if (index >= m_paragraphs.size()) {
        return {};
    }
    return m_paragraphs[index].alignment;
}

// =============================================================================
// Private Methods
// =============================================================================

void KmlDocumentModel::parseParagraphElement(QXmlStreamReader& reader, Paragraph& para)
{
    const QString tag = reader.name().toString();

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

    // Parse inline content, up to and including the closing tag
    QString text;
    std::vector<FormatRun> formats;
    size_t pos = 0;

    parseInlineContent(reader, text, formats, QTextCharFormat(), pos, tag);

    para.text = text;
    para.formats = std::move(formats);
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

                // Add a format run for any formatting or metadata (the base format of
                // a paragraph is empty, so any property marks a non-default run)
                if (currentFormat.propertyCount() > 0) {
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
                // Read inline style attributes (font, size, color, bg) off the current
                // start element. Must run BEFORE reader.readNext() while the reader is
                // still positioned on this tag, otherwise reader.attributes() no longer
                // refers to it. Covers injected-attribute carriers (b/i/u/s/sub/sup) and
                // the <span> carrier (span is a formatting tag whose applyTagFormat is a
                // no-op, so only the attributes take effect).
                KmlFormatRegistry::applyInlineStyleAttributes(reader.attributes(), newFormat);
                reader.readNext();
                parseInlineContent(reader, text, formats, newFormat, currentPos, tag);
            } else if (const MetadataTagDef* def = KmlFormatRegistry::getMetadataTagDef(tag)) {
                // Apply metadata with all of its attributes
                QTextCharFormat newFormat = currentFormat;
                newFormat.setProperty(def->propertyId,
                                      KmlFormatRegistry::readMetadataAttributes(reader.attributes()));

                reader.readNext();
                parseInlineContent(reader, text, formats, newFormat, currentPos, tag);
            } else if (tag == QStringLiteral("t") || tag == QStringLiteral("text")) {
                // Text run - just parse content
                reader.readNext();
                parseInlineContent(reader, text, formats, currentFormat, currentPos, tag);
            } else {
                // Unknown element - skip it, together with its end tag (where
                // skipCurrentElement() stops), so the text after it is still parsed
                reader.skipCurrentElement();
                reader.readNext();
            }
        } else {
            reader.readNext();
        }
    }
}

} // namespace editor
} // namespace kalahari
