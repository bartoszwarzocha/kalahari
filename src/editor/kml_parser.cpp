/// @file kml_parser.cpp
/// @brief Implementation of KML Parser (OpenSpec #00042 Phase 1.10/1.12/7.8)

#include <kalahari/editor/kml_parser.h>
#include <kalahari/editor/kml_text_run.h>
#include <kalahari/editor/kml_inline_elements.h>
#include <kalahari/editor/kml_table.h>
#include <kalahari/editor/kml_comment.h>
#include <kalahari/core/logger.h>
#include <QXmlStreamReader>
#include <QDomDocument>

namespace kalahari::editor {

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

ParseResult<KmlDocument> KmlParser::parseDocument(const QString& kml)
{
    clearError();

    if (kml.isEmpty()) {
        return ParseResult<KmlDocument>::ok(std::make_unique<KmlDocument>());
    }

    // Check if input starts with <kml>, <doc> or <document> wrapper
    QString trimmed = kml.trimmed();
    bool needsWrapper = !trimmed.startsWith(QStringLiteral("<kml>")) &&
                        !trimmed.startsWith(QStringLiteral("<kml ")) &&
                        !trimmed.startsWith(QStringLiteral("<doc>")) &&
                        !trimmed.startsWith(QStringLiteral("<doc ")) &&
                        !trimmed.startsWith(QStringLiteral("<document>")) &&
                        !trimmed.startsWith(QStringLiteral("<document "));

    // Wrap in temporary root if needed (allows multiple root elements like <p>...<p>)
    QString wrappedKml = needsWrapper
        ? (QStringLiteral("<_root>") + kml + QStringLiteral("</_root>"))
        : kml;

    QXmlStreamReader reader(wrappedKml);
    auto doc = parseDocumentContent(reader);

    if (!doc) {
        return ParseResult<KmlDocument>::error(m_lastError, m_lastErrorLine, m_lastErrorColumn);
    }

    return ParseResult<KmlDocument>::ok(std::move(doc));
}

ParseResult<KmlParagraph> KmlParser::parseParagraph(const QString& kml)
{
    clearError();

    if (kml.isEmpty()) {
        setError("Empty input");
        return ParseResult<KmlParagraph>::error(m_lastError, m_lastErrorLine, m_lastErrorColumn);
    }

    QXmlStreamReader reader(kml);

    // Skip to first element
    while (!reader.atEnd() && !reader.isStartElement()) {
        reader.readNext();
    }

    if (reader.atEnd() || reader.hasError()) {
        setError(reader);
        return ParseResult<KmlParagraph>::error(m_lastError, m_lastErrorLine, m_lastErrorColumn);
    }

    if (reader.name() != QStringLiteral("p")) {
        setError(QString("Expected <p> element, found <%1>").arg(reader.name().toString()),
                 static_cast<int>(reader.lineNumber()),
                 static_cast<int>(reader.columnNumber()));
        return ParseResult<KmlParagraph>::error(m_lastError, m_lastErrorLine, m_lastErrorColumn);
    }

    auto para = parseParagraphElement(reader);
    if (!para) {
        return ParseResult<KmlParagraph>::error(m_lastError, m_lastErrorLine, m_lastErrorColumn);
    }

    return ParseResult<KmlParagraph>::ok(std::move(para));
}

ParseResult<KmlElement> KmlParser::parseElement(const QString& kml)
{
    clearError();

    if (kml.isEmpty()) {
        setError("Empty input");
        return ParseResult<KmlElement>::error(m_lastError, m_lastErrorLine, m_lastErrorColumn);
    }

    QXmlStreamReader reader(kml);

    // Skip to first element
    while (!reader.atEnd() && !reader.isStartElement()) {
        reader.readNext();
    }

    if (reader.atEnd() || reader.hasError()) {
        setError(reader);
        return ParseResult<KmlElement>::error(m_lastError, m_lastErrorLine, m_lastErrorColumn);
    }

    auto element = parseInlineElement(reader);
    if (!element) {
        return ParseResult<KmlElement>::error(m_lastError, m_lastErrorLine, m_lastErrorColumn);
    }

    return ParseResult<KmlElement>::ok(std::move(element));
}

ParseResult<KmlTable> KmlParser::parseTable(const QString& kml)
{
    clearError();

    if (kml.isEmpty()) {
        setError("Empty input");
        return ParseResult<KmlTable>::error(m_lastError, m_lastErrorLine, m_lastErrorColumn);
    }

    QXmlStreamReader reader(kml);

    // Skip to first element
    while (!reader.atEnd() && !reader.isStartElement()) {
        reader.readNext();
    }

    if (reader.atEnd() || reader.hasError()) {
        setError(reader);
        return ParseResult<KmlTable>::error(m_lastError, m_lastErrorLine, m_lastErrorColumn);
    }

    if (reader.name() != QStringLiteral("table")) {
        setError(QString("Expected <table> element, found <%1>").arg(reader.name().toString()),
                 static_cast<int>(reader.lineNumber()),
                 static_cast<int>(reader.columnNumber()));
        return ParseResult<KmlTable>::error(m_lastError, m_lastErrorLine, m_lastErrorColumn);
    }

    auto table = parseTableElement(reader);
    if (!table) {
        return ParseResult<KmlTable>::error(m_lastError, m_lastErrorLine, m_lastErrorColumn);
    }

    return ParseResult<KmlTable>::ok(std::move(table));
}

// =============================================================================
// Error Information
// =============================================================================

const QString& KmlParser::lastError() const
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

std::unique_ptr<KmlDocument> KmlParser::parseDocumentContent(QXmlStreamReader& reader)
{
    auto& logger = core::Logger::getInstance();
    auto doc = std::make_unique<KmlDocument>();

    // Skip to first element
    while (!reader.atEnd() && !reader.isStartElement()) {
        reader.readNext();
    }

    if (reader.atEnd()) {
        // Empty document is valid
        logger.debug("KmlParser: Empty document (atEnd after skip)");
        return doc;
    }

    if (reader.hasError()) {
        setError(reader);
        return nullptr;
    }

    logger.debug("KmlParser: First element is '{}'", reader.name().toString().toStdString());

    // Accept <kml>, <doc>, <document>, and <_root> (internal wrapper) as valid document wrappers
    bool hasDocWrapper = (reader.name() == QStringLiteral("kml") ||
                          reader.name() == QStringLiteral("doc") ||
                          reader.name() == QStringLiteral("document") ||
                          reader.name() == QStringLiteral("_root"));
    QString docEndTag = hasDocWrapper ? reader.name().toString() : QString();

    logger.debug("KmlParser: hasDocWrapper={}, docEndTag='{}'", hasDocWrapper, docEndTag.toStdString());

    if (hasDocWrapper) {
        // Move past <doc> or <document> start element
        reader.readNext();
        logger.debug("KmlParser: After readNext, tokenType={}, name='{}'",
                     (int)reader.tokenType(), reader.name().toString().toStdString());
    }

    // Parse paragraphs
    int iterations = 0;
    while (!reader.atEnd()) {
        iterations++;
        if (iterations > 1000) {
            logger.error("KmlParser: Too many iterations, breaking");
            break;
        }

        if (reader.isEndElement()) {
            logger.debug("KmlParser: EndElement '{}', docEndTag='{}'",
                        reader.name().toString().toStdString(), docEndTag.toStdString());
            if (hasDocWrapper && reader.name() == docEndTag) {
                logger.debug("KmlParser: Breaking on end of doc wrapper");
                break;  // End of document wrapper
            }
            reader.readNext();
            continue;
        }

        if (reader.isStartElement()) {
            logger.debug("KmlParser: StartElement '{}'", reader.name().toString().toStdString());
            if (reader.name() == QStringLiteral("p")) {
                auto para = parseParagraphElement(reader);
                if (!para) {
                    return nullptr;  // Error already set
                }
                doc->addParagraph(std::move(para));
                logger.debug("KmlParser: Added paragraph, count now {}", doc->paragraphCount());
            } else if (reader.name() == QStringLiteral("kml") ||
                       reader.name() == QStringLiteral("doc") ||
                       reader.name() == QStringLiteral("document") ||
                       reader.name() == QStringLiteral("_root")) {
                // Nested kml/doc/document/_root - skip it
                reader.skipCurrentElement();
            } else {
                // Unknown top-level element - skip it
                logger.debug("KmlParser: Skipping unknown element '{}'", reader.name().toString().toStdString());
                reader.skipCurrentElement();
            }
        } else {
            reader.readNext();
        }
    }

    logger.debug("KmlParser: Finished parsing, paragraphCount={}", doc->paragraphCount());

    if (reader.hasError() && reader.error() != QXmlStreamReader::PrematureEndOfDocumentError) {
        setError(reader);
        return nullptr;
    }

    return doc;
}

std::unique_ptr<KmlParagraph> KmlParser::parseParagraphElement(QXmlStreamReader& reader)
{
    // Reader should be positioned at <p> start element
    if (!reader.isStartElement() || reader.name() != QStringLiteral("p")) {
        setError("Expected <p> element",
                 static_cast<int>(reader.lineNumber()),
                 static_cast<int>(reader.columnNumber()));
        return nullptr;
    }

    auto para = std::make_unique<KmlParagraph>();

    // Read attributes
    QXmlStreamAttributes attrs = reader.attributes();

    // Style attribute
    if (attrs.hasAttribute(QStringLiteral("style"))) {
        para->setStyleId(attrs.value(QStringLiteral("style")).toString());
    }

    // Alignment attribute
    if (attrs.hasAttribute(QStringLiteral("align"))) {
        QString alignStr = attrs.value(QStringLiteral("align")).toString().toLower();
        if (alignStr == QStringLiteral("left")) {
            para->setAlignment(Qt::AlignLeft);
        } else if (alignStr == QStringLiteral("center")) {
            para->setAlignment(Qt::AlignHCenter);
        } else if (alignStr == QStringLiteral("right")) {
            para->setAlignment(Qt::AlignRight);
        } else if (alignStr == QStringLiteral("justify")) {
            para->setAlignment(Qt::AlignJustify);
        }
    }

    // Move past <p> start element
    reader.readNext();

    // Parse content until </p>
    while (!reader.atEnd()) {
        if (reader.isEndElement() && reader.name() == QStringLiteral("p")) {
            reader.readNext();  // Move past </p>
            break;
        }

        if (reader.isCharacters()) {
            // Direct text content (not wrapped in <t>)
            QString text = reader.text().toString();
            if (!text.isEmpty()) {
                para->addElement(std::make_unique<KmlTextRun>(text));
            }
            reader.readNext();
        } else if (reader.isStartElement()) {
            // Check for comments element (Phase 7.8)
            if (reader.name() == QStringLiteral("comments")) {
                parseCommentsElement(reader, para.get());
                // parseCommentsElement moves past </comments>
            } else {
                auto element = parseInlineElement(reader);
                if (element) {
                    para->addElement(std::move(element));
                }
                // Note: parseInlineElement moves the reader past the element
            }
        } else {
            reader.readNext();
        }
    }

    if (reader.hasError() && reader.error() != QXmlStreamReader::PrematureEndOfDocumentError) {
        setError(reader);
        return nullptr;
    }

    return para;
}

std::unique_ptr<KmlElement> KmlParser::parseInlineElement(QXmlStreamReader& reader)
{
    if (!reader.isStartElement()) {
        setError("Expected start element",
                 static_cast<int>(reader.lineNumber()),
                 static_cast<int>(reader.columnNumber()));
        return nullptr;
    }

    QString tagName = reader.name().toString();

    // Text run <t>
    if (tagName == QStringLiteral("t")) {
        return parseTextRun(reader);
    }

    // Inline container elements
    auto element = createInlineElement(tagName);
    if (!element) {
        // Unknown element - try to parse as text run fallback
        // Or skip the element entirely
        reader.skipCurrentElement();
        return nullptr;
    }

    // Cast to container to add children
    auto* container = dynamic_cast<KmlInlineContainer*>(element.get());
    if (!container) {
        setError(QString("Internal error: Element <%1> is not a container").arg(tagName),
                 static_cast<int>(reader.lineNumber()),
                 static_cast<int>(reader.columnNumber()));
        return nullptr;
    }

    // Move past start element
    reader.readNext();

    if (!parseInlineChildren(reader, container, tagName)) {
        return nullptr;  // Error already set
    }

    return element;
}

std::unique_ptr<KmlElement> KmlParser::parseTextRun(QXmlStreamReader& reader)
{
    // Reader should be at <t> start element
    if (!reader.isStartElement() || reader.name() != QStringLiteral("t")) {
        setError("Expected <t> element",
                 static_cast<int>(reader.lineNumber()),
                 static_cast<int>(reader.columnNumber()));
        return nullptr;
    }

    // Read style attribute if present
    QString styleId;
    QXmlStreamAttributes attrs = reader.attributes();
    if (attrs.hasAttribute(QStringLiteral("style"))) {
        styleId = attrs.value(QStringLiteral("style")).toString();
    }

    // Read text content
    QString text;
    reader.readNext();

    while (!reader.atEnd()) {
        if (reader.isCharacters()) {
            text += reader.text().toString();
        } else if (reader.isEndElement() && reader.name() == QStringLiteral("t")) {
            reader.readNext();  // Move past </t>
            break;
        }
        reader.readNext();
    }

    if (reader.hasError() && reader.error() != QXmlStreamReader::PrematureEndOfDocumentError) {
        setError(reader);
        return nullptr;
    }

    return std::make_unique<KmlTextRun>(text, styleId);
}

bool KmlParser::parseInlineChildren(QXmlStreamReader& reader,
                                    KmlInlineContainer* container,
                                    const QString& endTagName)
{
    while (!reader.atEnd()) {
        if (reader.isEndElement() && reader.name() == endTagName) {
            reader.readNext();  // Move past end element
            return true;
        }

        if (reader.isCharacters()) {
            // Direct text content
            QString text = reader.text().toString();
            if (!text.isEmpty()) {
                container->appendChild(std::make_unique<KmlTextRun>(text));
            }
            reader.readNext();
        } else if (reader.isStartElement()) {
            auto element = parseInlineElement(reader);
            if (element) {
                container->appendChild(std::move(element));
            }
            // parseInlineElement moves the reader
        } else {
            reader.readNext();
        }
    }

    if (reader.hasError() && reader.error() != QXmlStreamReader::PrematureEndOfDocumentError) {
        setError(reader);
        return false;
    }

    // Reached end without finding closing tag
    setError(QString("Missing closing tag </%1>").arg(endTagName), -1, -1);
    return false;
}

std::unique_ptr<KmlElement> KmlParser::createInlineElement(const QString& tagName)
{
    // Bold - accept both <b> (canonical) and <bold> (legacy)
    if (tagName == QStringLiteral("b") || tagName == QStringLiteral("bold")) {
        return std::make_unique<KmlBold>();
    }
    // Italic - accept both <i> (canonical) and <italic> (legacy)
    if (tagName == QStringLiteral("i") || tagName == QStringLiteral("italic")) {
        return std::make_unique<KmlItalic>();
    }
    // Underline - accept both <u> (canonical) and <underline> (legacy)
    if (tagName == QStringLiteral("u") || tagName == QStringLiteral("underline")) {
        return std::make_unique<KmlUnderline>();
    }
    // Strikethrough - accept both <s> (canonical) and <strikethrough>/<strike> (legacy)
    if (tagName == QStringLiteral("s") || tagName == QStringLiteral("strikethrough") ||
        tagName == QStringLiteral("strike")) {
        return std::make_unique<KmlStrikethrough>();
    }
    if (tagName == QStringLiteral("sub")) {
        return std::make_unique<KmlSubscript>();
    }
    if (tagName == QStringLiteral("sup")) {
        return std::make_unique<KmlSuperscript>();
    }

    // Unknown element type
    return nullptr;
}

// =============================================================================
// Table Parsing Methods (Phase 1.12)
// =============================================================================

std::unique_ptr<KmlTable> KmlParser::parseTableElement(QXmlStreamReader& reader)
{
    // Reader should be positioned at <table> start element
    if (!reader.isStartElement() || reader.name() != QStringLiteral("table")) {
        setError("Expected <table> element",
                 static_cast<int>(reader.lineNumber()),
                 static_cast<int>(reader.columnNumber()));
        return nullptr;
    }

    auto table = std::make_unique<KmlTable>();

    // Read style attribute if present
    QXmlStreamAttributes attrs = reader.attributes();
    if (attrs.hasAttribute(QStringLiteral("style"))) {
        table->setStyleId(attrs.value(QStringLiteral("style")).toString());
    }

    // Move past <table> start element
    reader.readNext();

    // Parse rows until </table>
    while (!reader.atEnd()) {
        if (reader.isEndElement() && reader.name() == QStringLiteral("table")) {
            reader.readNext();  // Move past </table>
            break;
        }

        if (reader.isStartElement()) {
            if (reader.name() == QStringLiteral("tr")) {
                auto row = parseTableRowElement(reader);
                if (row) {
                    table->addRow(std::move(row));
                }
                // parseTableRowElement moves the reader past </tr>
            } else {
                // Unknown element inside table - skip it
                reader.skipCurrentElement();
            }
        } else {
            reader.readNext();
        }
    }

    if (reader.hasError() && reader.error() != QXmlStreamReader::PrematureEndOfDocumentError) {
        setError(reader);
        return nullptr;
    }

    return table;
}

std::unique_ptr<KmlTableRow> KmlParser::parseTableRowElement(QXmlStreamReader& reader)
{
    // Reader should be positioned at <tr> start element
    if (!reader.isStartElement() || reader.name() != QStringLiteral("tr")) {
        setError("Expected <tr> element",
                 static_cast<int>(reader.lineNumber()),
                 static_cast<int>(reader.columnNumber()));
        return nullptr;
    }

    auto row = std::make_unique<KmlTableRow>();

    // Move past <tr> start element
    reader.readNext();

    // Parse cells until </tr>
    while (!reader.atEnd()) {
        if (reader.isEndElement() && reader.name() == QStringLiteral("tr")) {
            reader.readNext();  // Move past </tr>
            break;
        }

        if (reader.isStartElement()) {
            if (reader.name() == QStringLiteral("td")) {
                auto cell = parseTableCellElement(reader, false);
                if (cell) {
                    row->addCell(std::move(cell));
                }
            } else if (reader.name() == QStringLiteral("th")) {
                auto cell = parseTableCellElement(reader, true);
                if (cell) {
                    row->addCell(std::move(cell));
                }
            } else {
                // Unknown element inside row - skip it
                reader.skipCurrentElement();
            }
        } else {
            reader.readNext();
        }
    }

    if (reader.hasError() && reader.error() != QXmlStreamReader::PrematureEndOfDocumentError) {
        setError(reader);
        return nullptr;
    }

    return row;
}

std::unique_ptr<KmlTableCell> KmlParser::parseTableCellElement(QXmlStreamReader& reader, bool isHeader)
{
    // Reader should be positioned at <td> or <th> start element
    QString expectedTag = isHeader ? QStringLiteral("th") : QStringLiteral("td");
    if (!reader.isStartElement() || reader.name() != expectedTag) {
        setError(QString("Expected <%1> element").arg(expectedTag),
                 static_cast<int>(reader.lineNumber()),
                 static_cast<int>(reader.columnNumber()));
        return nullptr;
    }

    auto cell = std::make_unique<KmlTableCell>();
    cell->setHeader(isHeader);

    // Read colspan and rowspan attributes
    QXmlStreamAttributes attrs = reader.attributes();
    if (attrs.hasAttribute(QStringLiteral("colspan"))) {
        bool ok = false;
        int colspan = attrs.value(QStringLiteral("colspan")).toInt(&ok);
        if (ok && colspan >= 1) {
            cell->setColspan(colspan);
        }
    }
    if (attrs.hasAttribute(QStringLiteral("rowspan"))) {
        bool ok = false;
        int rowspan = attrs.value(QStringLiteral("rowspan")).toInt(&ok);
        if (ok && rowspan >= 1) {
            cell->setRowspan(rowspan);
        }
    }

    // Move past start element
    reader.readNext();

    // Parse cell content into the paragraph
    if (!parseCellContent(reader, &cell->content(), expectedTag)) {
        return nullptr;  // Error already set
    }

    return cell;
}

bool KmlParser::parseCellContent(QXmlStreamReader& reader,
                                  KmlParagraph* paragraph,
                                  const QString& endTagName)
{
    while (!reader.atEnd()) {
        if (reader.isEndElement() && reader.name() == endTagName) {
            reader.readNext();  // Move past end element
            return true;
        }

        if (reader.isCharacters()) {
            // Direct text content
            QString text = reader.text().toString();
            if (!text.isEmpty()) {
                paragraph->addElement(std::make_unique<KmlTextRun>(text));
            }
            reader.readNext();
        } else if (reader.isStartElement()) {
            auto element = parseInlineElement(reader);
            if (element) {
                paragraph->addElement(std::move(element));
            }
            // parseInlineElement moves the reader
        } else {
            reader.readNext();
        }
    }

    if (reader.hasError() && reader.error() != QXmlStreamReader::PrematureEndOfDocumentError) {
        setError(reader);
        return false;
    }

    // Reached end without finding closing tag
    setError(QString("Missing closing tag </%1>").arg(endTagName), -1, -1);
    return false;
}

// =============================================================================
// Comment Parsing Methods (Phase 7.8)
// =============================================================================

bool KmlParser::parseCommentsElement(QXmlStreamReader& reader, KmlParagraph* paragraph)
{
    // Reader should be at <comments> start element
    if (!reader.isStartElement() || reader.name() != QStringLiteral("comments")) {
        return false;
    }

    // Move past <comments> start element
    reader.readNext();

    // Parse comment elements until </comments>
    while (!reader.atEnd()) {
        if (reader.isEndElement() && reader.name() == QStringLiteral("comments")) {
            reader.readNext();  // Move past </comments>
            return true;
        }

        if (reader.isStartElement()) {
            if (reader.name() == QStringLiteral("comment")) {
                KmlComment comment = parseCommentElement(reader);
                paragraph->addComment(comment);
                // parseCommentElement moves past </comment>
            } else {
                // Unknown element inside comments - skip it
                reader.skipCurrentElement();
            }
        } else {
            reader.readNext();
        }
    }

    return !reader.hasError();
}

KmlComment KmlParser::parseCommentElement(QXmlStreamReader& reader)
{
    KmlComment comment;

    // Reader should be at <comment> start element
    if (!reader.isStartElement() || reader.name() != QStringLiteral("comment")) {
        return comment;
    }

    // Read attributes
    QXmlStreamAttributes attrs = reader.attributes();

    if (attrs.hasAttribute(QStringLiteral("id"))) {
        comment.setId(attrs.value(QStringLiteral("id")).toString());
    }

    if (attrs.hasAttribute(QStringLiteral("start"))) {
        bool ok = false;
        int start = attrs.value(QStringLiteral("start")).toInt(&ok);
        if (ok) {
            comment.setStartPos(start);
        }
    }

    if (attrs.hasAttribute(QStringLiteral("end"))) {
        bool ok = false;
        int end = attrs.value(QStringLiteral("end")).toInt(&ok);
        if (ok) {
            comment.setEndPos(end);
        }
    }

    if (attrs.hasAttribute(QStringLiteral("author"))) {
        comment.setAuthor(attrs.value(QStringLiteral("author")).toString());
    }

    if (attrs.hasAttribute(QStringLiteral("created"))) {
        QString dateStr = attrs.value(QStringLiteral("created")).toString();
        QDateTime dt = QDateTime::fromString(dateStr, Qt::ISODate);
        if (dt.isValid()) {
            comment.setCreatedAt(dt);
        }
    }

    if (attrs.hasAttribute(QStringLiteral("resolved"))) {
        QString resolvedStr = attrs.value(QStringLiteral("resolved")).toString().toLower();
        comment.setResolved(resolvedStr == QStringLiteral("true") ||
                           resolvedStr == QStringLiteral("1"));
    }

    // Read text content
    QString text;
    reader.readNext();

    while (!reader.atEnd()) {
        if (reader.isEndElement() && reader.name() == QStringLiteral("comment")) {
            reader.readNext();  // Move past </comment>
            break;
        }

        if (reader.isCharacters()) {
            text += reader.text().toString();
        }

        reader.readNext();
    }

    comment.setText(text);

    return comment;
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

}  // namespace kalahari::editor
