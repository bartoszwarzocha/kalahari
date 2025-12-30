/// @file kml_converter.cpp
/// @brief KML Converter implementation (OpenSpec #00043 Phase 7)

#include <kalahari/editor/kml_converter.h>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QUuid>
#include <algorithm>
#include <limits>
#include <stack>

namespace kalahari::editor {

// =============================================================================
// MetadataLayer Implementation
// =============================================================================

void MetadataLayer::addComment(const TextComment& comment) {
    m_comments.push_back(comment);
}

void MetadataLayer::removeComment(const QString& id) {
    m_comments.erase(
        std::remove_if(m_comments.begin(), m_comments.end(),
                       [&id](const TextComment& c) { return c.id == id; }),
        m_comments.end());
}

std::vector<TextComment> MetadataLayer::getCommentsAt(size_t position) const {
    std::vector<TextComment> result;
    for (const auto& c : m_comments) {
        if (position >= c.anchorStart && position < c.anchorEnd) {
            result.push_back(c);
        }
    }
    return result;
}

std::vector<TextComment> MetadataLayer::getCommentsInRange(size_t start, size_t end) const {
    std::vector<TextComment> result;
    for (const auto& c : m_comments) {
        if (c.anchorStart < end && c.anchorEnd > start) {
            result.push_back(c);
        }
    }
    return result;
}

void MetadataLayer::addTodo(const TextTodo& todo) {
    m_todos.push_back(todo);
}

void MetadataLayer::removeTodo(size_t index) {
    if (index < m_todos.size()) {
        m_todos.erase(m_todos.begin() + static_cast<ptrdiff_t>(index));
    }
}

void MetadataLayer::removeTodo(const QString& id) {
    m_todos.erase(
        std::remove_if(m_todos.begin(), m_todos.end(),
                       [&id](const TextTodo& t) { return t.id == id; }),
        m_todos.end());
}

std::vector<TextTodo> MetadataLayer::getTodosAt(size_t position) const {
    std::vector<TextTodo> result;
    for (const auto& t : m_todos) {
        if (t.position == position) {
            result.push_back(t);
        }
    }
    return result;
}

std::vector<TextTodo> MetadataLayer::getTodosInRange(size_t start, size_t end) const {
    std::vector<TextTodo> result;
    for (const auto& t : m_todos) {
        if (t.position >= start && t.position < end) {
            result.push_back(t);
        }
    }
    return result;
}

std::vector<TextTodo> MetadataLayer::getMarkersByType(MarkerType type) const {
    std::vector<TextTodo> result;
    for (const auto& todo : m_todos) {
        if (todo.type == type) {
            result.push_back(todo);
        }
    }
    return result;
}

std::optional<TextTodo> MetadataLayer::getMarkerById(const QString& id) const {
    for (const auto& todo : m_todos) {
        if (todo.id == id) {
            return todo;
        }
    }
    return std::nullopt;
}

std::optional<TextTodo> MetadataLayer::findNextMarker(size_t fromPosition,
    std::optional<MarkerType> typeFilter) const {
    std::optional<TextTodo> result;
    size_t minDist = std::numeric_limits<size_t>::max();

    for (const auto& todo : m_todos) {
        if (todo.position > fromPosition) {
            if (!typeFilter || todo.type == *typeFilter) {
                size_t dist = todo.position - fromPosition;
                if (dist < minDist) {
                    minDist = dist;
                    result = todo;
                }
            }
        }
    }
    return result;
}

std::optional<TextTodo> MetadataLayer::findPreviousMarker(size_t fromPosition,
    std::optional<MarkerType> typeFilter) const {
    std::optional<TextTodo> result;
    size_t maxPos = 0;

    for (const auto& todo : m_todos) {
        if (todo.position < fromPosition) {
            if (!typeFilter || todo.type == *typeFilter) {
                if (todo.position >= maxPos) {
                    maxPos = todo.position;
                    result = todo;
                }
            }
        }
    }
    return result;
}

void MetadataLayer::updateTodo(const QString& id, const TextTodo& updated) {
    for (auto& todo : m_todos) {
        if (todo.id == id) {
            todo = updated;
            return;
        }
    }
}

void MetadataLayer::toggleTodoCompleted(const QString& id) {
    for (auto& todo : m_todos) {
        if (todo.id == id) {
            todo.completed = !todo.completed;
            return;
        }
    }
}

QString MetadataLayer::generateMarkerId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void MetadataLayer::onTextInserted(size_t position, size_t length) {
    // Shift comments
    for (auto& c : m_comments) {
        if (c.anchorStart >= position) {
            c.anchorStart += length;
            c.anchorEnd += length;
        } else if (c.anchorEnd > position) {
            c.anchorEnd += length;
        }
    }
    // Shift TODOs
    for (auto& t : m_todos) {
        if (t.position >= position) {
            t.position += length;
        }
    }
}

void MetadataLayer::onTextDeleted(size_t position, size_t length) {
    size_t end = position + length;

    // Adjust comments
    for (auto& c : m_comments) {
        if (c.anchorStart >= end) {
            c.anchorStart -= length;
            c.anchorEnd -= length;
        } else if (c.anchorStart >= position) {
            c.anchorStart = position;
            if (c.anchorEnd > end) {
                c.anchorEnd -= length;
            } else {
                c.anchorEnd = position;
            }
        } else if (c.anchorEnd > end) {
            c.anchorEnd -= length;
        } else if (c.anchorEnd > position) {
            c.anchorEnd = position;
        }
    }

    // Remove empty comments
    m_comments.erase(
        std::remove_if(m_comments.begin(), m_comments.end(),
                       [](const TextComment& c) { return c.anchorStart >= c.anchorEnd; }),
        m_comments.end());

    // Adjust TODOs
    for (auto& t : m_todos) {
        if (t.position >= end) {
            t.position -= length;
        } else if (t.position > position) {
            t.position = position;
        }
    }
}

void MetadataLayer::clear() {
    m_comments.clear();
    m_todos.clear();
}

// =============================================================================
// KmlConverter - Parsing
// =============================================================================

KmlConversionResult KmlConverter::parseKml(const QString& kml) {
    auto buffer = std::make_unique<TextBuffer>();
    auto formatLayer = std::make_unique<FormatLayer>();
    auto metadataLayer = std::make_unique<MetadataLayer>();

    if (!parseKmlInto(kml, *buffer, *formatLayer, metadataLayer.get())) {
        return KmlConversionResult::error(m_lastError, m_lastErrorLine, m_lastErrorColumn);
    }

    return KmlConversionResult::ok(
        std::move(buffer),
        std::move(formatLayer),
        std::move(metadataLayer));
}

bool KmlConverter::parseKmlInto(const QString& kml,
                                 TextBuffer& buffer,
                                 FormatLayer& formatLayer,
                                 MetadataLayer* metadataLayer) {
    if (kml.isEmpty()) {
        return true;  // Empty is valid
    }

    // Wrap in root if needed (bare paragraphs)
    QString wrappedKml = kml.trimmed();
    if (!wrappedKml.startsWith("<kml") &&
        !wrappedKml.startsWith("<doc") &&
        !wrappedKml.startsWith("<document")) {
        wrappedKml = "<kml>" + wrappedKml + "</kml>";
    }

    QXmlStreamReader reader(wrappedKml);

    // Skip to root element
    while (!reader.atEnd() && !reader.isStartElement()) {
        reader.readNext();
    }

    if (reader.hasError()) {
        setError(reader);
        return false;
    }

    // Parse document content
    return parseDocumentContent(reader, buffer, formatLayer, metadataLayer);
}

bool KmlConverter::parseDocumentContent(QXmlStreamReader& reader,
                                         TextBuffer& buffer,
                                         FormatLayer& formatLayer,
                                         MetadataLayer* metadataLayer) {
    QString rootTag = reader.name().toString();
    size_t offset = 0;

    // Get current paragraph count for appending
    size_t startParagraph = buffer.paragraphCount();
    if (startParagraph > 0) {
        // Calculate offset from existing content
        // Skip empty first paragraph (fresh buffer default state)
        bool hasRealContent = !(startParagraph == 1 && buffer.paragraphText(0).isEmpty());
        if (hasRealContent) {
            for (size_t i = 0; i < startParagraph; ++i) {
                offset += buffer.paragraphText(i).length() + 1;  // +1 for newline
            }
        }
    }

    reader.readNext();

    // Begin batch insert mode to avoid O(n^2) Fenwick tree rebuilds
    // This defers rebuilding until all paragraphs are inserted
    buffer.beginBatchInsert();

    while (!reader.atEnd()) {
        if (reader.isEndElement() && reader.name().toString() == rootTag) {
            break;
        }

        if (reader.isStartElement()) {
            QString tag = reader.name().toString();

            if (tag == "p" || tag == "paragraph") {
                offset = parseParagraph(reader, buffer, formatLayer, offset);
            } else if (tag == "comments" && metadataLayer) {
                parseComments(reader, *metadataLayer);
            } else if (tag == "markers" && metadataLayer) {
                parseMarkers(reader, *metadataLayer);
            } else {
                // Skip unknown elements
                reader.skipCurrentElement();
            }
        } else {
            reader.readNext();
        }
    }

    // End batch mode - rebuilds Fenwick tree once
    buffer.endBatchInsert();

    if (reader.hasError()) {
        setError(reader);
        return false;
    }

    return true;
}

size_t KmlConverter::parseParagraph(QXmlStreamReader& reader,
                                     TextBuffer& buffer,
                                     FormatLayer& formatLayer,
                                     size_t baseOffset) {
    QString paragraphText;

    // Parse inline content
    size_t length = parseInlineContent(reader, paragraphText, formatLayer,
                                        baseOffset, FormatType::None, "p");

    // Add paragraph to buffer
    if (buffer.paragraphCount() == 0 || !buffer.paragraphText(0).isEmpty() ||
        buffer.paragraphCount() > 1) {
        // Append new paragraph
        size_t newIndex = buffer.paragraphCount();
        buffer.insertParagraph(newIndex, paragraphText);
    } else {
        // First paragraph, set text
        buffer.setParagraphText(0, paragraphText);
    }

    // Return new offset (text length + 1 for paragraph separator)
    return baseOffset + length + 1;
}

size_t KmlConverter::parseInlineContent(QXmlStreamReader& reader,
                                         QString& text,
                                         FormatLayer& formatLayer,
                                         size_t baseOffset,
                                         FormatType activeFormats,
                                         const QString& endTag) {
    size_t startLength = text.length();

    reader.readNext();

    while (!reader.atEnd()) {
        if (reader.isEndElement()) {
            QString tag = reader.name().toString();
            if (tag == endTag || tag == "paragraph") {
                break;
            }
            // End of inline element - handled by recursion
            break;
        }

        if (reader.isCharacters()) {
            QString chars = reader.text().toString();
            size_t textStart = baseOffset + text.length();
            text += chars;

            // Apply active formats to this text
            if (activeFormats != FormatType::None && !chars.isEmpty()) {
                TextFormat fmt;
                fmt.flags = activeFormats;
                formatLayer.addFormat(textStart, textStart + chars.length(), fmt);
            }
        } else if (reader.isStartElement()) {
            QString tag = reader.name().toString();
            FormatType formatType = tagToFormatType(tag);

            if (formatType != FormatType::None) {
                // Formatting element - recurse with updated formats
                FormatType newFormats = activeFormats | formatType;
                parseInlineContent(reader, text, formatLayer, baseOffset, newFormats, tag);
            } else if (tag == "t" || tag == "text") {
                // Text run element - just parse content
                parseInlineContent(reader, text, formatLayer, baseOffset, activeFormats, tag);
            } else {
                // Unknown element - skip
                reader.skipCurrentElement();
            }
        }

        reader.readNext();
    }

    return text.length() - startLength;
}

bool KmlConverter::parseComments(QXmlStreamReader& reader, MetadataLayer& metadata) {
    reader.readNext();

    while (!reader.atEnd()) {
        if (reader.isEndElement() && reader.name().toString() == "comments") {
            break;
        }

        if (reader.isStartElement() && reader.name().toString() == "comment") {
            TextComment comment;

            // Read attributes
            auto attrs = reader.attributes();
            if (attrs.hasAttribute("start")) {
                comment.anchorStart = attrs.value("start").toULongLong();
            }
            if (attrs.hasAttribute("end")) {
                comment.anchorEnd = attrs.value("end").toULongLong();
            }
            if (attrs.hasAttribute("author")) {
                comment.author = attrs.value("author").toString();
            }
            if (attrs.hasAttribute("timestamp")) {
                comment.timestamp = attrs.value("timestamp").toString();
            }
            if (attrs.hasAttribute("id")) {
                comment.id = attrs.value("id").toString();
            }

            // Read comment text
            comment.text = reader.readElementText();

            metadata.addComment(comment);
        } else {
            reader.readNext();
        }
    }

    return true;
}

bool KmlConverter::parseMarkers(QXmlStreamReader& reader, MetadataLayer& metadata) {
    reader.readNext();

    while (!reader.atEnd()) {
        if (reader.isEndElement() && reader.name().toString() == "markers") {
            break;
        }

        if (reader.isStartElement()) {
            QString tagName = reader.name().toString();
            if (tagName == "todo" || tagName == "note") {
                TextTodo marker;
                marker.type = (tagName == "todo") ? MarkerType::Todo : MarkerType::Note;

                // Read attributes
                QXmlStreamAttributes attrs = reader.attributes();
                if (attrs.hasAttribute("position")) {
                    marker.position = attrs.value("position").toULongLong();
                }
                if (attrs.hasAttribute("id")) {
                    marker.id = attrs.value("id").toString();
                }
                if (attrs.hasAttribute("completed")) {
                    marker.completed = (attrs.value("completed").toString() == "true");
                }
                if (attrs.hasAttribute("priority")) {
                    marker.priority = attrs.value("priority").toString();
                }
                if (attrs.hasAttribute("timestamp")) {
                    marker.timestamp = attrs.value("timestamp").toString();
                }

                // Read marker text content
                marker.text = reader.readElementText();

                // Generate ID if missing
                if (marker.id.isEmpty()) {
                    marker.id = MetadataLayer::generateMarkerId();
                }

                metadata.addTodo(marker);
            } else {
                reader.readNext();
            }
        } else {
            reader.readNext();
        }
    }

    return true;
}

FormatType KmlConverter::tagToFormatType(const QString& tag) const {
    if (tag == "b" || tag == "bold") return FormatType::Bold;
    if (tag == "i" || tag == "italic") return FormatType::Italic;
    if (tag == "u" || tag == "underline") return FormatType::Underline;
    if (tag == "s" || tag == "strike" || tag == "strikethrough") return FormatType::Strikethrough;
    if (tag == "sub" || tag == "subscript") return FormatType::Subscript;
    if (tag == "sup" || tag == "superscript") return FormatType::Superscript;
    return FormatType::None;
}

void KmlConverter::setError(QXmlStreamReader& reader) {
    m_lastError = reader.errorString();
    m_lastErrorLine = static_cast<int>(reader.lineNumber());
    m_lastErrorColumn = static_cast<int>(reader.columnNumber());
}

void KmlConverter::setError(const QString& message, int line, int col) {
    m_lastError = message;
    m_lastErrorLine = line;
    m_lastErrorColumn = col;
}

// =============================================================================
// KmlConverter - Serialization
// =============================================================================

QString KmlConverter::toKml(const TextBuffer& buffer,
                            const FormatLayer& formatLayer,
                            const MetadataLayer* metadataLayer) const {
    QString result;
    QXmlStreamWriter writer(&result);
    writer.setAutoFormatting(false);

    writer.writeStartElement("kml");

    size_t offset = 0;
    for (size_t i = 0; i < buffer.paragraphCount(); ++i) {
        QString paraText = buffer.paragraphText(i);
        size_t paraStart = offset;
        size_t paraEnd = offset + paraText.length();

        writer.writeStartElement("p");

        // Get format events for this paragraph
        auto events = buildFormatEvents(formatLayer, paraStart, paraEnd);

        // Write formatted text
        writeFormattedText(writer, paraText, paraStart, events);

        writer.writeEndElement();  // </p>

        offset = paraEnd + 1;  // +1 for paragraph separator
    }

    // Write comments if present
    if (metadataLayer && !metadataLayer->allComments().empty()) {
        writer.writeStartElement("comments");
        for (const auto& comment : metadataLayer->allComments()) {
            writer.writeStartElement("comment");
            writer.writeAttribute("start", QString::number(comment.anchorStart));
            writer.writeAttribute("end", QString::number(comment.anchorEnd));
            if (!comment.author.isEmpty()) {
                writer.writeAttribute("author", comment.author);
            }
            if (!comment.timestamp.isEmpty()) {
                writer.writeAttribute("timestamp", comment.timestamp);
            }
            if (!comment.id.isEmpty()) {
                writer.writeAttribute("id", comment.id);
            }
            writer.writeCharacters(comment.text);
            writer.writeEndElement();  // </comment>
        }
        writer.writeEndElement();  // </comments>
    }

    // Write TODO/Note markers if present (Task 9.13)
    if (metadataLayer && !metadataLayer->allTodos().empty()) {
        writer.writeStartElement("markers");
        for (const auto& marker : metadataLayer->allTodos()) {
            QString tagName = (marker.type == MarkerType::Todo) ? "todo" : "note";
            writer.writeStartElement(tagName);
            writer.writeAttribute("position", QString::number(marker.position));
            writer.writeAttribute("id", marker.id);
            if (marker.type == MarkerType::Todo) {
                writer.writeAttribute("completed", marker.completed ? "true" : "false");
            }
            if (!marker.priority.isEmpty()) {
                writer.writeAttribute("priority", marker.priority);
            }
            if (!marker.timestamp.isEmpty()) {
                writer.writeAttribute("timestamp", marker.timestamp);
            }
            if (!marker.text.isEmpty()) {
                writer.writeCharacters(marker.text);
            }
            writer.writeEndElement();  // </todo> or </note>
        }
        writer.writeEndElement();  // </markers>
    }

    writer.writeEndElement();  // </kml>

    return result;
}

QString KmlConverter::paragraphToKml(const TextBuffer& buffer,
                                      const FormatLayer& formatLayer,
                                      size_t paragraphIndex) const {
    if (paragraphIndex >= buffer.paragraphCount()) {
        return QString();
    }

    // Calculate paragraph offset
    size_t offset = 0;
    for (size_t i = 0; i < paragraphIndex; ++i) {
        offset += buffer.paragraphText(i).length() + 1;
    }

    QString paraText = buffer.paragraphText(paragraphIndex);
    size_t paraStart = offset;
    size_t paraEnd = offset + paraText.length();

    QString result;
    QXmlStreamWriter writer(&result);
    writer.setAutoFormatting(false);

    // Get format events for this paragraph
    auto events = buildFormatEvents(formatLayer, paraStart, paraEnd);

    // Write formatted text
    writeFormattedText(writer, paraText, paraStart, events);

    return result;
}

std::vector<KmlConverter::FormatEvent> KmlConverter::buildFormatEvents(
    const FormatLayer& formatLayer,
    size_t start,
    size_t end) const {

    std::vector<FormatEvent> events;
    auto ranges = formatLayer.getFormatsInRange(start, end);

    for (const auto& range : ranges) {
        // Clamp to paragraph bounds
        size_t rangeStart = std::max(range.start, start);
        size_t rangeEnd = std::min(range.end, end);

        if (rangeStart >= rangeEnd) continue;

        // Add events for each format type
        auto addEvents = [&](FormatType type) {
            if (range.format.hasFlag(type)) {
                events.push_back({rangeStart, type, true});
                events.push_back({rangeEnd, type, false});
            }
        };

        addEvents(FormatType::Bold);
        addEvents(FormatType::Italic);
        addEvents(FormatType::Underline);
        addEvents(FormatType::Strikethrough);
        addEvents(FormatType::Subscript);
        addEvents(FormatType::Superscript);
    }

    // Sort events
    std::sort(events.begin(), events.end());

    return events;
}

void KmlConverter::writeFormattedText(QXmlStreamWriter& writer,
                                       const QString& text,
                                       size_t textStart,
                                       const std::vector<FormatEvent>& events) const {
    if (events.empty()) {
        // No formatting - just write text
        if (!text.isEmpty()) {
            writer.writeCharacters(text);
        }
        return;
    }

    // Track open tags (stack)
    std::stack<FormatType> openTags;
    size_t lastPos = 0;

    for (const auto& event : events) {
        size_t relativePos = event.position - textStart;

        // Write text before this event
        if (relativePos > lastPos) {
            QString segment = text.mid(static_cast<int>(lastPos),
                                        static_cast<int>(relativePos - lastPos));
            writer.writeCharacters(segment);
            lastPos = relativePos;
        }

        if (event.isStart) {
            // Open tag
            writer.writeStartElement(formatTypeToTag(event.type));
            openTags.push(event.type);
        } else {
            // Close tag - need to close in reverse order for proper nesting
            // For simplicity, we close the current tag
            if (!openTags.empty() && openTags.top() == event.type) {
                writer.writeEndElement();
                openTags.pop();
            } else {
                // Tags are not properly nested - close anyway
                writer.writeEndElement();
            }
        }
    }

    // Write remaining text
    if (lastPos < static_cast<size_t>(text.length())) {
        writer.writeCharacters(text.mid(static_cast<int>(lastPos)));
    }

    // Close any remaining open tags
    while (!openTags.empty()) {
        writer.writeEndElement();
        openTags.pop();
    }
}

QString KmlConverter::formatTypeToTag(FormatType type) const {
    switch (type) {
        case FormatType::Bold: return "b";
        case FormatType::Italic: return "i";
        case FormatType::Underline: return "u";
        case FormatType::Strikethrough: return "s";
        case FormatType::Subscript: return "sub";
        case FormatType::Superscript: return "sup";
        default: return QString();
    }
}

}  // namespace kalahari::editor
