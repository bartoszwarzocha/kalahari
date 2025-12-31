/// @file kml_parser.cpp
/// @brief KML Parser implementation (OpenSpec #00043 Phase 11.1)
///
/// Parses KML markup directly into QTextDocument with QTextCharFormat for
/// formatting and metadata. This replaces the old TextBuffer+FormatLayer approach.

#include <kalahari/editor/kml_parser.h>
#include <kalahari/editor/kml_format_registry.h>
#include <kalahari/core/logger.h>
#include <QXmlStreamReader>
#include <QTextCursor>
#include <QTextBlock>
#include <QFont>
#include <QVariantMap>

namespace kalahari {
namespace editor {

// =============================================================================
// Constructor / Destructor
// =============================================================================

KmlParser::KmlParser()
    : m_lastError()
    , m_lastErrorLine(-1)
    , m_lastErrorColumn(-1)
{
}

KmlParser::~KmlParser() = default;

// =============================================================================
// Public Parsing Methods
// =============================================================================

QTextDocument* KmlParser::parseKml(const QString& kml)
{
    clearError();

    // Create new document
    auto* document = new QTextDocument();

    if (!parseInto(kml, document)) {
        delete document;
        return nullptr;
    }

    return document;
}

bool KmlParser::parseInto(const QString& kml, QTextDocument* document)
{
    clearError();

    if (!document) {
        setError(QStringLiteral("Document pointer is null"));
        return false;
    }

    // Clear document
    document->clear();

    if (kml.isEmpty()) {
        return true;  // Empty KML is valid
    }

    // Wrap in root if needed (bare paragraphs without document wrapper)
    QString wrappedKml = kml.trimmed();
    bool needsWrapper = !wrappedKml.startsWith(QStringLiteral("<kml")) &&
                        !wrappedKml.startsWith(QStringLiteral("<doc")) &&
                        !wrappedKml.startsWith(QStringLiteral("<document"));

    if (needsWrapper) {
        wrappedKml = QStringLiteral("<kml>") + wrappedKml + QStringLiteral("</kml>");
    }

    QXmlStreamReader reader(wrappedKml);

    // Skip to first element
    while (!reader.atEnd() && !reader.isStartElement()) {
        reader.readNext();
    }

    if (reader.hasError()) {
        setError(reader);
        return false;
    }

    if (reader.atEnd()) {
        return true;  // Empty document is valid
    }

    // Get root tag name
    QString rootTag = reader.name().toString();

    // Create cursor at document start
    QTextCursor cursor(document);

    // Move past root element
    reader.readNext();

    // Parse document content
    if (!parseDocumentContent(reader, cursor, rootTag)) {
        return false;
    }

    return true;
}

// =============================================================================
// Error Information
// =============================================================================

QString KmlParser::lastError() const
{
    return m_lastError;
}

int KmlParser::lastErrorLine() const
{
    return m_lastErrorLine;
}

int KmlParser::lastErrorColumn() const
{
    return m_lastErrorColumn;
}

// =============================================================================
// Internal Parsing Methods
// =============================================================================

bool KmlParser::parseDocumentContent(QXmlStreamReader& reader,
                                      QTextCursor& cursor,
                                      const QString& rootTag)
{
    auto& logger = core::Logger::getInstance();
    bool firstParagraph = true;

    while (!reader.atEnd()) {
        if (reader.isEndElement()) {
            QString tag = reader.name().toString();
            if (tag == rootTag) {
                break;  // End of document
            }
            reader.readNext();
            continue;
        }

        if (reader.isStartElement()) {
            QString tag = reader.name().toString();

            if (tag == QStringLiteral("p") || tag == QStringLiteral("paragraph")) {
                // Insert paragraph break before non-first paragraphs
                if (!firstParagraph) {
                    cursor.insertBlock();
                }
                firstParagraph = false;

                parseParagraph(reader, cursor);
            } else {
                // Unknown top-level element - skip it
                logger.debug("KmlParser: Skipping unknown element '{}'",
                            tag.toStdString());
                reader.skipCurrentElement();
            }
        } else {
            reader.readNext();
        }
    }

    if (reader.hasError() &&
        reader.error() != QXmlStreamReader::PrematureEndOfDocumentError) {
        setError(reader);
        return false;
    }

    return true;
}

void KmlParser::parseParagraph(QXmlStreamReader& reader, QTextCursor& cursor)
{
    // Reader should be at <p> start element
    if (!reader.isStartElement()) {
        return;
    }

    // Read paragraph attributes (style, alignment)
    QXmlStreamAttributes attrs = reader.attributes();

    // Apply paragraph block format if needed
    QTextBlockFormat blockFormat;
    if (attrs.hasAttribute(QStringLiteral("align"))) {
        QString alignStr = attrs.value(QStringLiteral("align")).toString().toLower();
        if (alignStr == QStringLiteral("left")) {
            blockFormat.setAlignment(Qt::AlignLeft);
        } else if (alignStr == QStringLiteral("center")) {
            blockFormat.setAlignment(Qt::AlignHCenter);
        } else if (alignStr == QStringLiteral("right")) {
            blockFormat.setAlignment(Qt::AlignRight);
        } else if (alignStr == QStringLiteral("justify")) {
            blockFormat.setAlignment(Qt::AlignJustify);
        }
        cursor.setBlockFormat(blockFormat);
    }

    // Move past <p> start element
    reader.readNext();

    // Parse inline content with default format
    QTextCharFormat defaultFormat;
    parseInlineContent(reader, cursor, defaultFormat, QStringLiteral("p"));
}

void KmlParser::parseInlineContent(QXmlStreamReader& reader,
                                    QTextCursor& cursor,
                                    QTextCharFormat activeFormat,
                                    const QString& endTag)
{
    while (!reader.atEnd()) {
        if (reader.isEndElement()) {
            QString tag = reader.name().toString();
            if (tag == endTag || tag == QStringLiteral("paragraph")) {
                reader.readNext();  // Move past end element
                return;
            }
            // End of nested element - handled by caller
            return;
        }

        if (reader.isCharacters()) {
            // Insert text with active format
            QString text = reader.text().toString();
            if (!text.isEmpty()) {
                cursor.insertText(text, activeFormat);
            }
            reader.readNext();
        } else if (reader.isStartElement()) {
            QString tag = reader.name().toString();

            if (KmlFormatRegistry::isFormattingTag(tag)) {
                // Formatting element - recurse with updated format
                QTextCharFormat newFormat = KmlFormatRegistry::applyTagFormat(tag, activeFormat);
                reader.readNext();  // Move past start element
                parseInlineContent(reader, cursor, newFormat, tag);
            } else if (KmlFormatRegistry::isMetadataTag(tag)) {
                // Metadata element (comment, todo, footnote)
                // Apply metadata properties to format, then parse content recursively
                QTextCharFormat metaFormat = activeFormat;
                parseMetadataElement(reader, tag, metaFormat);
                reader.readNext();  // Move past start element
                parseInlineContent(reader, cursor, metaFormat, tag);
            } else if (tag == QStringLiteral("t") || tag == QStringLiteral("text")) {
                // Text run element - just parse content with same format
                reader.readNext();
                parseInlineContent(reader, cursor, activeFormat, tag);
            } else {
                // Unknown element - skip
                reader.skipCurrentElement();
            }
        } else {
            reader.readNext();
        }
    }
}

void KmlParser::parseMetadataElement(QXmlStreamReader& reader,
                                      const QString& tag,
                                      QTextCharFormat& format)
{
    // Note: This function updates the format with metadata properties
    // The calling code (parseInlineContent) should use this format to insert text
    // However, metadata elements contain their own text content, so we need
    // to return the content to be inserted

    // Read attributes
    QXmlStreamAttributes attrs = reader.attributes();
    QVariantMap metadata;

    // Common attributes for all metadata types
    if (attrs.hasAttribute(QStringLiteral("id"))) {
        metadata[QStringLiteral("id")] = attrs.value(QStringLiteral("id")).toString();
    }

    // Type-specific attributes
    if (tag == QStringLiteral("comment")) {
        if (attrs.hasAttribute(QStringLiteral("author"))) {
            metadata[QStringLiteral("author")] = attrs.value(QStringLiteral("author")).toString();
        }
        if (attrs.hasAttribute(QStringLiteral("created"))) {
            metadata[QStringLiteral("created")] = attrs.value(QStringLiteral("created")).toString();
        }
        if (attrs.hasAttribute(QStringLiteral("resolved"))) {
            QString resolved = attrs.value(QStringLiteral("resolved")).toString().toLower();
            metadata[QStringLiteral("resolved")] = (resolved == QStringLiteral("true") ||
                                                    resolved == QStringLiteral("1"));
        }
        format.setProperty(KmlPropComment, metadata);
    }
    else if (tag == QStringLiteral("todo")) {
        if (attrs.hasAttribute(QStringLiteral("completed"))) {
            QString completed = attrs.value(QStringLiteral("completed")).toString().toLower();
            metadata[QStringLiteral("completed")] = (completed == QStringLiteral("true") ||
                                                     completed == QStringLiteral("1"));
        }
        if (attrs.hasAttribute(QStringLiteral("priority"))) {
            metadata[QStringLiteral("priority")] = attrs.value(QStringLiteral("priority")).toString();
        }
        format.setProperty(KmlPropTodo, metadata);
    }
    else if (tag == QStringLiteral("footnote")) {
        if (attrs.hasAttribute(QStringLiteral("number"))) {
            metadata[QStringLiteral("number")] = attrs.value(QStringLiteral("number")).toInt();
        }
        format.setProperty(KmlPropFootnote, metadata);
    }
    else if (tag == QStringLiteral("charref")) {
        if (attrs.hasAttribute(QStringLiteral("target"))) {
            metadata[QStringLiteral("target")] = attrs.value(QStringLiteral("target")).toString();
        }
        format.setProperty(KmlPropCharRef, metadata);
    }
    else if (tag == QStringLiteral("locref")) {
        if (attrs.hasAttribute(QStringLiteral("target"))) {
            metadata[QStringLiteral("target")] = attrs.value(QStringLiteral("target")).toString();
        }
        format.setProperty(KmlPropLocRef, metadata);
    }

    // Note: The actual text insertion happens in parseInlineContent
    // after this function returns with the format updated
}

// =============================================================================
// Error Handling
// =============================================================================

void KmlParser::setError(QXmlStreamReader& reader)
{
    m_lastError = reader.errorString();
    m_lastErrorLine = static_cast<int>(reader.lineNumber());
    m_lastErrorColumn = static_cast<int>(reader.columnNumber());

    if (m_lastError.isEmpty()) {
        m_lastError = QStringLiteral("Unknown XML parsing error");
    }
}

void KmlParser::setError(const QString& message, int line, int col)
{
    m_lastError = message;
    m_lastErrorLine = line;
    m_lastErrorColumn = col;
}

void KmlParser::clearError()
{
    m_lastError.clear();
    m_lastErrorLine = -1;
    m_lastErrorColumn = -1;
}

} // namespace editor
} // namespace kalahari
